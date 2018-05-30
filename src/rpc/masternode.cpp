// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2018 FXTC developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <activemasternode.h>
#include <init.h>
#include <netbase.h>
#include <key_io.h>
#include <validation.h>
#include <masternode-payments.h>
#include <masternode-sync.h>
#include <masternodeconfig.h>
#include <masternodeman.h>
#ifdef ENABLE_WALLET
#include <privatesend-client.h>
#endif // ENABLE_WALLET
#include <privatesend-server.h>
#include <rpc/server.h>
// FXTC BEGIN
#include <rpc/util.h>
// FXTC END
#include <util/system.h>
#include <util/moneystr.h>

#include <fstream>
#include <iomanip>
#include <univalue.h>

// FXTC BEGIN
#include <wallet/rpcwallet.h>
// FXTC END

UniValue masternodelist(const JSONRPCRequest& request);

#ifdef ENABLE_WALLET
void EnsureWalletIsUnlocked();

UniValue privatesend(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            RPCHelpMan{"privatesend",
                "\nStart, stop or reset privatesend mixing.\n",
                {
                    {"command", RPCArg::Type::STR, RPCArg::Optional::NO, "The command to execute, must be one of:\n"
            "       \"start\" - Start mixing\n"
            "       \"stop\"  - Stop mixing\n"
            "       \"reset\" - Reset mixing"},
                },
                RPCResult{
            "Not documented by Dash developers.\n"
                },
                RPCExamples{
                    HelpExampleCli("privatesend", "start")
            + HelpExampleCli("privatesend", "stop")
            + HelpExampleCli("privatesend", "reset")
            + HelpExampleRpc("privatesend", "\"start\"")
            + HelpExampleRpc("privatesend", "\"stop\"")
            + HelpExampleRpc("privatesend", "\"reset\"")
                },
            }.ToString());

    if(request.params[0].get_str() == "start") {
        {
            LOCK(pwallet->cs_wallet);
            EnsureWalletIsUnlocked(pwallet);
        }

        if(fMasterNode)
            return "Mixing is not supported from masternodes";

        privateSendClient.fEnablePrivateSend = true;
        bool result = privateSendClient.DoAutomaticDenominating(*g_connman);
        return "Mixing " + (result ? "started successfully" : ("start failed: " + privateSendClient.GetStatus() + ", will retry"));
    }

    if(request.params[0].get_str() == "stop") {
        privateSendClient.fEnablePrivateSend = false;
        return "Mixing was stopped";
    }

    if(request.params[0].get_str() == "reset") {
        privateSendClient.ResetPool();
        return "Mixing was reset";
    }

    return "Unknown command, please see \"help privatesend\"";
}
#endif // ENABLE_WALLET

UniValue getpoolinfo(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();
#endif // ENABLE_WALLET
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            RPCHelpMan{"getpoolinfo",
                "\nReturns an object containing mixing pool related information.\n",
                {},
                RPCResult{
            "Not documented by Dash developers.\n"
                },
                RPCExamples{
                    HelpExampleCli("getpoolinfo", "")
            + HelpExampleRpc("getpoolinfo", "")
                },
            }.ToString());

#ifdef ENABLE_WALLET
    CPrivateSendBase* pprivateSendBase = fMasterNode ? (CPrivateSendBase*)&privateSendServer : (CPrivateSendBase*)&privateSendClient;

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("state",             pprivateSendBase->GetStateString());
    obj.pushKV("mixing_mode",       (!fMasterNode && privateSendClient.fPrivateSendMultiSession) ? "multi-session" : "normal");
    obj.pushKV("queue",             pprivateSendBase->GetQueueSize());
    obj.pushKV("entries",           pprivateSendBase->GetEntriesCount());
    obj.pushKV("status",            privateSendClient.GetStatus());

    masternode_info_t mnInfo;
    if (privateSendClient.GetMixingMasternodeInfo(mnInfo)) {
        obj.pushKV("outpoint",      mnInfo.vin.prevout.ToStringShort());
        obj.pushKV("addr",          mnInfo.addr.ToString());
    }

    if (pwallet) {
        obj.pushKV("keys_left",     pwallet->nKeysLeftSinceAutoBackup);
        obj.pushKV("warnings",      pwallet->nKeysLeftSinceAutoBackup < PRIVATESEND_KEYS_THRESHOLD_WARNING
                                                ? "WARNING: keypool is almost depleted!" : "");
    }
#else // ENABLE_WALLET
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("state",             privateSendServer.GetStateString());
    obj.pushKV("queue",             privateSendServer.GetQueueSize());
    obj.pushKV("entries",           privateSendServer.GetEntriesCount());
#endif // ENABLE_WALLET

    return obj;
}


UniValue masternode(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    std::string strCommand;
    if (request.params.size() >= 1) {
        strCommand = request.params[0].get_str();
    }

#ifdef ENABLE_WALLET
    if (strCommand == "start-many")
        throw JSONRPCError(RPC_INVALID_PARAMETER, "DEPRECATED, please use start-all instead");
#endif // ENABLE_WALLET

    if (request.fHelp  ||
        (
#ifdef ENABLE_WALLET
            strCommand != "start-alias" && strCommand != "start-all" && strCommand != "start-missing" &&
         strCommand != "start-disabled" && strCommand != "outputs" &&
#endif // ENABLE_WALLET
         strCommand != "list" && strCommand != "list-conf" && strCommand != "count" &&
         strCommand != "debug" && strCommand != "current" && strCommand != "winner" && strCommand != "winners" && strCommand != "genkey" &&
         // FXTC BEGIN
         //strCommand != "connect" && strCommand != "status"))
         strCommand != "connect" && strCommand != "status" && strCommand != "collateral"))
         // FXTC END
            throw std::runtime_error(
                RPCHelpMan{"masternode",
                    "\nSet of commands to execute masternode related actions\n",
                    {
                        {"command", RPCArg::Type::STR, RPCArg::Optional::NO, "The command to execute, must be one of:\n"
                "       \"count\"        - Print number of all known masternodes (optional: \"ps\", \"enabled\", \"all\", \"qualify\")\n"
                "       \"current\"      - Print info on current masternode winner to be paid the next block (calculated locally)\n"
                "       \"genkey\"       - Generate new masternodeprivkey\n"
#ifdef ENABLE_WALLET
                "       \"outputs\"      - Print masternode compatible outputs\n"
                "       \"start-alias\"  - Start single remote masternode by assigned alias configured in masternode.conf\n"
                "       \"start-<mode>\" - Start remote masternodes configured in masternode.conf (<mode>: \"all\", \"missing\", \"disabled\")\n"
#endif // ENABLE_WALLET
                "       \"status\"       - Print masternode status information\n"
                "       \"list\"         - Print list of all known masternodes (see masternodelist for more info)\n"
                "       \"list-conf\"    - Print masternode.conf in JSON format\n"
                "       \"winner\"       - Print info on next masternode winner to vote for\n"
                "       \"winners\"      - Print list of masternode winners\n"
                // FXTC BEGIN
                "       \"collateral\"   - Print actual masternode collateral value"
                // FXTC END
                        },
                    },
                    RPCResult{
                "Not documented by Dash developers.\n"
                    },
                    RPCExamples{
                        HelpExampleCli("masternode", "collateral")
                + HelpExampleRpc("masternode", "\"collateral\"")
                    },
                }.ToString());

    if (strCommand == "list")
    {
        JSONRPCRequest newRequest;
        newRequest.fHelp = request.fHelp;
        // forward params but skip "list"
        for (unsigned int i = 1; i < request.params.size(); i++) {
            newRequest.params.push_back(request.params[i]);
        }
        return masternodelist(newRequest);
    }

    if(strCommand == "connect")
    {
        if (request.params.size() < 2)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Masternode address required");

        std::string strAddress = request.params[1].get_str();

        CService addr;
        if (!Lookup(strAddress.c_str(), addr, 0, false))
            throw JSONRPCError(RPC_INTERNAL_ERROR, strprintf("Incorrect masternode address %s", strAddress));

        // TODO: Pass CConnman instance somehow and don't use global variable.
        // FXTC BEGIN
        //CNode *pnode = g_connman->ConnectNode(CAddress(addr, NODE_NETWORK), NULL, false);
        CNode *pnode = g_connman->OpenNetworkConnection(CAddress(addr, NODE_NETWORK), false, nullptr, NULL, false, false, false, true);
        //
        if(!pnode)
            throw JSONRPCError(RPC_INTERNAL_ERROR, strprintf("Couldn't connect to masternode %s", strAddress));

        return "successfully connected";
    }

    if (strCommand == "count")
    {
        if (request.params.size() > 2)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Too many parameters");

        if (request.params.size() == 1)
            return mnodeman.size();

        std::string strMode = request.params[1].get_str();

        if (strMode == "ps")
            return mnodeman.CountEnabled(MIN_PRIVATESEND_PEER_PROTO_VERSION);

        if (strMode == "enabled")
            return mnodeman.CountEnabled();

        int nCount;
        masternode_info_t mnInfo;
        mnodeman.GetNextMasternodeInQueueForPayment(true, nCount, mnInfo);

        if (strMode == "qualify")
            return nCount;

        if (strMode == "all")
            return strprintf("Total: %d (PS Compatible: %d / Enabled: %d / Qualify: %d)",
                mnodeman.size(), mnodeman.CountEnabled(MIN_PRIVATESEND_PEER_PROTO_VERSION),
                mnodeman.CountEnabled(), nCount);
    }

    if (strCommand == "current" || strCommand == "winner")
    {
        int nCount;
        int nHeight;
        masternode_info_t mnInfo;
        CBlockIndex* pindex = NULL;
        {
            LOCK(cs_main);
            pindex = chainActive.Tip();
        }
        nHeight = pindex->nHeight + (strCommand == "current" ? 1 : 10);
        mnodeman.UpdateLastPaid(pindex);

        if(!mnodeman.GetNextMasternodeInQueueForPayment(nHeight, true, nCount, mnInfo))
            return "unknown";

        UniValue obj(UniValue::VOBJ);

        obj.pushKV("height",        nHeight);
        obj.pushKV("IP:port",       mnInfo.addr.ToString());
        obj.pushKV("protocol",      (int64_t)mnInfo.nProtocolVersion);
        obj.pushKV("outpoint",      mnInfo.vin.prevout.ToStringShort());
        obj.pushKV("payee",         EncodeDestination(mnInfo.pubKeyCollateralAddress.GetID()));
        obj.pushKV("lastseen",      mnInfo.nTimeLastPing);
        obj.pushKV("activeseconds", mnInfo.nTimeLastPing - mnInfo.sigTime);
        return obj;
    }

#ifdef ENABLE_WALLET
    if (strCommand == "start-alias")
    {
        if (request.params.size() < 2)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Please specify an alias");

        {
            LOCK(pwallet->cs_wallet);
            EnsureWalletIsUnlocked(pwallet);
        }

        std::string strAlias = request.params[1].get_str();

        bool fFound = false;

        UniValue statusObj(UniValue::VOBJ);
        statusObj.pushKV("alias", strAlias);

        for (CMasternodeConfig::CMasternodeEntry mne : masternodeConfig.getEntries()) {
            if(mne.getAlias() == strAlias) {
                fFound = true;
                std::string strError;
                CMasternodeBroadcast mnb;

                bool fResult = CMasternodeBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), strError, mnb);

                statusObj.pushKV("result", fResult ? "successful" : "failed");
                if(fResult) {
                    mnodeman.UpdateMasternodeList(mnb, *g_connman);
                    mnb.Relay(*g_connman);
                } else {
                    statusObj.pushKV("errorMessage", strError);
                }
                mnodeman.NotifyMasternodeUpdates(*g_connman);
                break;
            }
        }

        if(!fFound) {
            statusObj.pushKV("result", "failed");
            statusObj.pushKV("errorMessage", "Could not find alias in config. Verify with list-conf.");
        }

        return statusObj;

    }

    if (strCommand == "start-all" || strCommand == "start-missing" || strCommand == "start-disabled")
    {
        {
            LOCK(pwallet->cs_wallet);
            EnsureWalletIsUnlocked(pwallet);
        }

        if((strCommand == "start-missing" || strCommand == "start-disabled") && !masternodeSync.IsMasternodeListSynced()) {
            throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "You can't use this command until masternode list is synced");
        }

        int nSuccessful = 0;
        int nFailed = 0;

        UniValue resultsObj(UniValue::VOBJ);

        for (CMasternodeConfig::CMasternodeEntry mne : masternodeConfig.getEntries()) {
            std::string strError;

            COutPoint outpoint = COutPoint(uint256S(mne.getTxHash()), uint32_t(atoi(mne.getOutputIndex().c_str())));
            CMasternode mn;
            bool fFound = mnodeman.Get(outpoint, mn);
            CMasternodeBroadcast mnb;

            if(strCommand == "start-missing" && fFound) continue;
            if(strCommand == "start-disabled" && fFound && mn.IsEnabled()) continue;

            bool fResult = CMasternodeBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), strError, mnb);

            UniValue statusObj(UniValue::VOBJ);
            statusObj.pushKV("alias", mne.getAlias());
            statusObj.pushKV("result", fResult ? "successful" : "failed");

            if (fResult) {
                nSuccessful++;
                mnodeman.UpdateMasternodeList(mnb, *g_connman);
                mnb.Relay(*g_connman);
            } else {
                nFailed++;
                statusObj.pushKV("errorMessage", strError);
            }

            resultsObj.pushKV("status", statusObj);
        }
        mnodeman.NotifyMasternodeUpdates(*g_connman);

        UniValue returnObj(UniValue::VOBJ);
        returnObj.pushKV("overall", strprintf("Successfully started %d masternodes, failed to start %d, total %d", nSuccessful, nFailed, nSuccessful + nFailed));
        returnObj.pushKV("detail", resultsObj);

        return returnObj;
    }
#endif // ENABLE_WALLET

    if (strCommand == "genkey")
    {
        CKey secret;
        secret.MakeNewKey(false);

        return EncodeSecret(secret);
    }

    if (strCommand == "list-conf")
    {
        UniValue resultObj(UniValue::VOBJ);

        for (CMasternodeConfig::CMasternodeEntry mne : masternodeConfig.getEntries()) {
            COutPoint outpoint = COutPoint(uint256S(mne.getTxHash()), uint32_t(atoi(mne.getOutputIndex().c_str())));
            CMasternode mn;
            bool fFound = mnodeman.Get(outpoint, mn);

            std::string strStatus = fFound ? mn.GetStatus() : "MISSING";

            UniValue mnObj(UniValue::VOBJ);
            mnObj.pushKV("alias", mne.getAlias());
            mnObj.pushKV("address", mne.getIp());
            mnObj.pushKV("privateKey", mne.getPrivKey());
            mnObj.pushKV("txHash", mne.getTxHash());
            mnObj.pushKV("outputIndex", mne.getOutputIndex());
            mnObj.pushKV("status", strStatus);
            resultObj.pushKV("masternode", mnObj);
        }

        return resultObj;
    }

#ifdef ENABLE_WALLET
    if (strCommand == "outputs") {
        // Find possible candidates
        std::vector<COutput> vPossibleCoins;
        // FXTC BEGIN
        //pwallet->AvailableCoins(vPossibleCoins, true, NULL, false, ONLY_MASTERNODE_COLLATERAL);
        auto locked_chain = pwallet->chain().lock();
        LOCK(pwallet->cs_wallet);
        pwallet->AvailableCoins(*locked_chain, vPossibleCoins, true, NULL, false, ONLY_MASTERNODE_COLLATERAL);
        // FXTC END

        UniValue obj(UniValue::VOBJ);
        for (COutput& out : vPossibleCoins) {
            obj.pushKV(out.tx->GetHash().ToString(), strprintf("%d", out.i));
        }

        return obj;
    }
#endif // ENABLE_WALLET

    if (strCommand == "status")
    {
        if (!fMasterNode)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "This is not a masternode");

        UniValue mnObj(UniValue::VOBJ);

        mnObj.pushKV("outpoint", activeMasternode.outpoint.ToStringShort());
        mnObj.pushKV("service", activeMasternode.service.ToString());

        CMasternode mn;
        if(mnodeman.Get(activeMasternode.outpoint, mn)) {
            mnObj.pushKV("payee", EncodeDestination(mn.pubKeyCollateralAddress.GetID()));
        }

        mnObj.pushKV("status", activeMasternode.GetStatus());
        return mnObj;
    }

    if (strCommand == "winners")
    {
        int nHeight;
        {
            LOCK(cs_main);
            CBlockIndex* pindex = chainActive.Tip();
            if(!pindex) return NullUniValue;

            nHeight = pindex->nHeight;
        }

        int nLast = 10;
        std::string strFilter = "";

        if (request.params.size() >= 2) {
            nLast = atoi(request.params[1].get_str());
        }

        if (request.params.size() == 3) {
            strFilter = request.params[2].get_str();
        }

        if (request.params.size() > 3)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Correct usage is 'masternode winners ( \"count\" \"filter\" )'");

        UniValue obj(UniValue::VOBJ);

        for(int i = nHeight - nLast; i < nHeight + 20; i++) {
            std::string strPayment = GetRequiredPaymentsString(i);
            if (strFilter !="" && strPayment.find(strFilter) == std::string::npos) continue;
            obj.pushKV(strprintf("%d", i), strPayment);
        }

        return obj;
    }

    // FXTC BEGIN
    if (strCommand == "collateral")
    {
        LOCK(cs_main);

        CMasternode cm;
        CAmount nValue = cm.CollateralValue(chainActive.Height());

        if (nValue == 0)
            return "unknown";

        return ValueFromAmount(nValue);
    }
    // FXTC END

    return NullUniValue;
}

UniValue masternodelist(const JSONRPCRequest& request)
{
    std::string strMode = "status";
    std::string strFilter = "";

    if (request.params.size() >= 1) strMode = request.params[0].get_str();
    if (request.params.size() == 2) strFilter = request.params[1].get_str();

    if (request.fHelp || (
                strMode != "activeseconds" && strMode != "addr" && strMode != "full" && strMode != "info" &&
                strMode != "lastseen" && strMode != "lastpaidtime" && strMode != "lastpaidblock" &&
                strMode != "protocol" && strMode != "payee" && strMode != "pubkey" &&
                strMode != "rank" && strMode != "status"))
    {
        throw std::runtime_error(
            RPCHelpMan{"masternodelist",
                "\nGet a list of masternodes in different modes\n",
                {
                    {"mode", RPCArg::Type::STR, /* default */ "status", "The mode to run list in, must be one of:\n"
            "       \"activeseconds\"  - Print number of seconds masternode recognized by the network as enabled\n"
            "                            (since latest issued \"masternode start/start-many/start-alias\")\n"
            "       \"addr\"           - Print ip address associated with a masternode (can be additionally filtered, partial match)\n"
            "       \"full\"           - Print info in format 'status protocol payee lastseen activeseconds lastpaidtime lastpaidblock IP'\n"
            "                            (can be additionally filtered, partial match)\n"
            "       \"info\"           - Print info in format 'status protocol payee lastseen activeseconds sentinelversion sentinelstate IP'\n"
            "                            (can be additionally filtered, partial match)\n"
            "       \"lastpaidblock\"  - Print the last block height a node was paid on the network\n"
            "       \"lastpaidtime\"   - Print the last time a node was paid on the network\n"
            "       \"lastseen\"       - Print timestamp of when a masternode was last seen on the network\n"
            "       \"payee\"          - Print Dash address associated with a masternode (can be additionally filtered,\n"
            "                            partial match)\n"
            "       \"protocol\"       - Print protocol of a masternode (can be additionally filtered, exact match)\n"
            "       \"pubkey\"         - Print the masternode (not collateral) public key\n"
            "       \"rank\"           - Print rank of a masternode based on current block\n"
            "       \"status\"         - Print masternode status: PRE_ENABLED / ENABLED / EXPIRED / WATCHDOG_EXPIRED / NEW_START_REQUIRED /\n"
            "                            UPDATE_REQUIRED / POSE_BAN / OUTPOINT_SPENT (can be additionally filtered, partial match)"},
                    {"filter", RPCArg::Type::STR, /* default */ "", "Filter results. Partial match by outpoint by default in all modes,\n"
            "                            additional matches in some modes are also available"},
                },
                RPCResult{
            "Not documented by Dash developers.\n"
                },
                RPCExamples{
                    HelpExampleCli("masternodelist", "status")
            + HelpExampleRpc("masternodelist", "\"status\"")
                },
            }.ToString());
    }

    if (strMode == "full" || strMode == "lastpaidtime" || strMode == "lastpaidblock") {
        CBlockIndex* pindex = NULL;
        {
            LOCK(cs_main);
            pindex = chainActive.Tip();
        }
        mnodeman.UpdateLastPaid(pindex);
    }

    UniValue obj(UniValue::VOBJ);
    if (strMode == "rank") {
        CMasternodeMan::rank_pair_vec_t vMasternodeRanks;
        mnodeman.GetMasternodeRanks(vMasternodeRanks);
        for (std::pair<int, CMasternode>& s : vMasternodeRanks) {
            std::string strOutpoint = s.second.vin.prevout.ToStringShort();
            if (strFilter !="" && strOutpoint.find(strFilter) == std::string::npos) continue;
            obj.pushKV(strOutpoint, s.first);
        }
    } else {
        std::map<COutPoint, CMasternode> mapMasternodes = mnodeman.GetFullMasternodeMap();
        for (auto& mnpair : mapMasternodes) {
            CMasternode mn = mnpair.second;
            std::string strOutpoint = mnpair.first.ToStringShort();
            if (strMode == "activeseconds") {
                if (strFilter !="" && strOutpoint.find(strFilter) == std::string::npos) continue;
                obj.pushKV(strOutpoint, (int64_t)(mn.lastPing.sigTime - mn.sigTime));
            } else if (strMode == "addr") {
                std::string strAddress = mn.addr.ToString();
                if (strFilter !="" && strAddress.find(strFilter) == std::string::npos &&
                    strOutpoint.find(strFilter) == std::string::npos) continue;
                obj.pushKV(strOutpoint, strAddress);
            } else if (strMode == "full") {
                std::ostringstream streamFull;
                streamFull << std::setw(18) <<
                               mn.GetStatus() << " " <<
                               mn.nProtocolVersion << " " <<
                               EncodeDestination(mn.pubKeyCollateralAddress.GetID()) << " " <<
                               (int64_t)mn.lastPing.sigTime << " " << std::setw(8) <<
                               (int64_t)(mn.lastPing.sigTime - mn.sigTime) << " " << std::setw(10) <<
                               mn.GetLastPaidTime() << " "  << std::setw(6) <<
                               mn.GetLastPaidBlock() << " " <<
                               mn.addr.ToString();
                std::string strFull = streamFull.str();
                if (strFilter !="" && strFull.find(strFilter) == std::string::npos &&
                    strOutpoint.find(strFilter) == std::string::npos) continue;
                obj.pushKV(strOutpoint, strFull);
            } else if (strMode == "info") {
                std::ostringstream streamInfo;
                streamInfo << std::setw(18) <<
                               mn.GetStatus() << " " <<
                               mn.nProtocolVersion << " " <<
                               EncodeDestination(mn.pubKeyCollateralAddress.GetID()) << " " <<
                               (int64_t)mn.lastPing.sigTime << " " << std::setw(8) <<
                               (int64_t)(mn.lastPing.sigTime - mn.sigTime) << " " <<
                               SafeIntVersionToString(mn.lastPing.nSentinelVersion) << " "  <<
                               (mn.lastPing.fSentinelIsCurrent ? "current" : "expired") << " " <<
                               mn.addr.ToString();
                std::string strInfo = streamInfo.str();
                if (strFilter !="" && strInfo.find(strFilter) == std::string::npos &&
                    strOutpoint.find(strFilter) == std::string::npos) continue;
                obj.pushKV(strOutpoint, strInfo);
            } else if (strMode == "lastpaidblock") {
                if (strFilter !="" && strOutpoint.find(strFilter) == std::string::npos) continue;
                obj.pushKV(strOutpoint, mn.GetLastPaidBlock());
            } else if (strMode == "lastpaidtime") {
                if (strFilter !="" && strOutpoint.find(strFilter) == std::string::npos) continue;
                obj.pushKV(strOutpoint, mn.GetLastPaidTime());
            } else if (strMode == "lastseen") {
                if (strFilter !="" && strOutpoint.find(strFilter) == std::string::npos) continue;
                obj.pushKV(strOutpoint, (int64_t)mn.lastPing.sigTime);
            } else if (strMode == "payee") {
                std::string strPayee = EncodeDestination(mn.pubKeyCollateralAddress.GetID());
                if (strFilter !="" && strPayee.find(strFilter) == std::string::npos &&
                    strOutpoint.find(strFilter) == std::string::npos) continue;
                obj.pushKV(strOutpoint, strPayee);
            } else if (strMode == "protocol") {
                if (strFilter !="" && strFilter != strprintf("%d", mn.nProtocolVersion) &&
                    strOutpoint.find(strFilter) == std::string::npos) continue;
                obj.pushKV(strOutpoint, (int64_t)mn.nProtocolVersion);
            } else if (strMode == "pubkey") {
                if (strFilter !="" && strOutpoint.find(strFilter) == std::string::npos) continue;
                obj.pushKV(strOutpoint, HexStr(mn.pubKeyMasternode));
            } else if (strMode == "status") {
                std::string strStatus = mn.GetStatus();
                if (strFilter !="" && strStatus.find(strFilter) == std::string::npos &&
                    strOutpoint.find(strFilter) == std::string::npos) continue;
                obj.pushKV(strOutpoint, strStatus);
            }
        }
    }
    return obj;
}

bool DecodeHexVecMnb(std::vector<CMasternodeBroadcast>& vecMnb, std::string strHexMnb) {

    if (!IsHex(strHexMnb))
        return false;

    std::vector<unsigned char> mnbData(ParseHex(strHexMnb));
    CDataStream ssData(mnbData, SER_NETWORK, PROTOCOL_VERSION);
    try {
        ssData >> vecMnb;
    }
    catch (const std::exception&) {
        return false;
    }

    return true;
}

UniValue masternodebroadcast(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    std::string strCommand;
    if (request.params.size() >= 1)
        strCommand = request.params[0].get_str();

    if (request.fHelp  ||
        (
#ifdef ENABLE_WALLET
            strCommand != "create-alias" && strCommand != "create-all" &&
#endif // ENABLE_WALLET
            strCommand != "decode" && strCommand != "relay"))
        throw std::runtime_error(
            RPCHelpMan{"masternodebroadcast",
                "\nSet of commands to create and relay masternode broadcast messages\n",
                {
                    {"command", RPCArg::Type::STR, RPCArg::Optional::NO, "The command to execute, must be one of:\n"
#ifdef ENABLE_WALLET
            "       \"create-alias\"  - Create single remote masternode broadcast message by assigned alias configured in masternode.conf\n"
            "       \"create-all\"    - Create remote masternode broadcast messages for all masternodes configured in masternode.conf\n"
#endif // ENABLE_WALLET
            "       \"decode\"        - Decode masternode broadcast message\n"
            "       \"relay\"         - Relay masternode broadcast message to the network"},
                },
                RPCResult{
            "Not documented by Dash developers.\n"
                },
                RPCExamples{
                    HelpExampleCli("masternodebroadcast", "create-all")
            + HelpExampleRpc("masternodebroadcast", "\"create-all\"")
                },
            }.ToString());

#ifdef ENABLE_WALLET
    if (strCommand == "create-alias")
    {
        // wait for reindex and/or import to finish
        if (fImporting || fReindex)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Wait for reindex and/or import to finish");

        if (request.params.size() < 2)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Please specify an alias");

        {
            LOCK(pwallet->cs_wallet);
            EnsureWalletIsUnlocked(pwallet);
        }

        bool fFound = false;
        std::string strAlias = request.params[1].get_str();

        UniValue statusObj(UniValue::VOBJ);
        std::vector<CMasternodeBroadcast> vecMnb;

        statusObj.pushKV("alias", strAlias);

        for (CMasternodeConfig::CMasternodeEntry mne : masternodeConfig.getEntries()) {
            if(mne.getAlias() == strAlias) {
                fFound = true;
                std::string strError;
                CMasternodeBroadcast mnb;

                bool fResult = CMasternodeBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), strError, mnb, true);

                statusObj.pushKV("result", fResult ? "successful" : "failed");
                if(fResult) {
                    vecMnb.push_back(mnb);
                    CDataStream ssVecMnb(SER_NETWORK, PROTOCOL_VERSION);
                    ssVecMnb << vecMnb;
                    statusObj.pushKV("hex", HexStr(ssVecMnb.begin(), ssVecMnb.end()));
                } else {
                    statusObj.pushKV("errorMessage", strError);
                }
                break;
            }
        }

        if(!fFound) {
            statusObj.pushKV("result", "not found");
            statusObj.pushKV("errorMessage", "Could not find alias in config. Verify with list-conf.");
        }

        return statusObj;

    }

    if (strCommand == "create-all")
    {
        // wait for reindex and/or import to finish
        if (fImporting || fReindex)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Wait for reindex and/or import to finish");

        {
            LOCK(pwallet->cs_wallet);
            EnsureWalletIsUnlocked(pwallet);
        }

        std::vector<CMasternodeConfig::CMasternodeEntry> mnEntries;
        mnEntries = masternodeConfig.getEntries();

        int nSuccessful = 0;
        int nFailed = 0;

        UniValue resultsObj(UniValue::VOBJ);
        std::vector<CMasternodeBroadcast> vecMnb;

        for (CMasternodeConfig::CMasternodeEntry mne : masternodeConfig.getEntries()) {
            std::string strError;
            CMasternodeBroadcast mnb;

            bool fResult = CMasternodeBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), strError, mnb, true);

            UniValue statusObj(UniValue::VOBJ);
            statusObj.pushKV("alias", mne.getAlias());
            statusObj.pushKV("result", fResult ? "successful" : "failed");

            if(fResult) {
                nSuccessful++;
                vecMnb.push_back(mnb);
            } else {
                nFailed++;
                statusObj.pushKV("errorMessage", strError);
            }

            resultsObj.pushKV("status", statusObj);
        }

        CDataStream ssVecMnb(SER_NETWORK, PROTOCOL_VERSION);
        ssVecMnb << vecMnb;
        UniValue returnObj(UniValue::VOBJ);
        returnObj.pushKV("overall", strprintf("Successfully created broadcast messages for %d masternodes, failed to create %d, total %d", nSuccessful, nFailed, nSuccessful + nFailed));
        returnObj.pushKV("detail", resultsObj);
        returnObj.pushKV("hex", HexStr(ssVecMnb.begin(), ssVecMnb.end()));

        return returnObj;
    }
#endif // ENABLE_WALLET

    if (strCommand == "decode")
    {
        if (request.params.size() != 2)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Correct usage is 'masternodebroadcast decode \"hexstring\"'");

        std::vector<CMasternodeBroadcast> vecMnb;

        if (!DecodeHexVecMnb(vecMnb, request.params[1].get_str()))
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Masternode broadcast message decode failed");

        int nSuccessful = 0;
        int nFailed = 0;
        int nDos = 0;
        UniValue returnObj(UniValue::VOBJ);

        for (CMasternodeBroadcast& mnb : vecMnb) {
            UniValue resultObj(UniValue::VOBJ);

            if(mnb.CheckSignature(nDos)) {
                nSuccessful++;
                resultObj.pushKV("outpoint", mnb.vin.prevout.ToStringShort());
                resultObj.pushKV("addr", mnb.addr.ToString());
                resultObj.pushKV("pubKeyCollateralAddress", EncodeDestination(mnb.pubKeyCollateralAddress.GetID()));
                resultObj.pushKV("pubKeyMasternode", EncodeDestination(mnb.pubKeyMasternode.GetID()));
                resultObj.pushKV("vchSig", EncodeBase64(&mnb.vchSig[0], mnb.vchSig.size()));
                resultObj.pushKV("sigTime", mnb.sigTime);
                resultObj.pushKV("protocolVersion", mnb.nProtocolVersion);
                resultObj.pushKV("nLastDsq", mnb.nLastDsq);

                UniValue lastPingObj(UniValue::VOBJ);
                lastPingObj.pushKV("outpoint", mnb.lastPing.vin.prevout.ToStringShort());
                lastPingObj.pushKV("blockHash", mnb.lastPing.blockHash.ToString());
                lastPingObj.pushKV("sigTime", mnb.lastPing.sigTime);
                lastPingObj.pushKV("vchSig", EncodeBase64(&mnb.lastPing.vchSig[0], mnb.lastPing.vchSig.size()));

                resultObj.pushKV("lastPing", lastPingObj);
            } else {
                nFailed++;
                resultObj.pushKV("errorMessage", "Masternode broadcast signature verification failed");
            }

            returnObj.pushKV(mnb.GetHash().ToString(), resultObj);
        }

        returnObj.pushKV("overall", strprintf("Successfully decoded broadcast messages for %d masternodes, failed to decode %d, total %d", nSuccessful, nFailed, nSuccessful + nFailed));

        return returnObj;
    }

    if (strCommand == "relay")
    {
        if (request.params.size() < 2 || request.params.size() > 3)
            throw JSONRPCError(RPC_INVALID_PARAMETER,   "masternodebroadcast relay \"hexstring\" ( fast )\n"
                                                        "\nArguments:\n"
                                                        "1. \"hex\"      (string, required) Broadcast messages hex string\n"
                                                        "2. fast       (string, optional) If none, using safe method\n");

        std::vector<CMasternodeBroadcast> vecMnb;

        if (!DecodeHexVecMnb(vecMnb, request.params[1].get_str()))
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Masternode broadcast message decode failed");

        int nSuccessful = 0;
        int nFailed = 0;
        bool fSafe = request.params.size() == 2;
        UniValue returnObj(UniValue::VOBJ);

        // verify all signatures first, bailout if any of them broken
        for (CMasternodeBroadcast& mnb : vecMnb) {
            UniValue resultObj(UniValue::VOBJ);

            resultObj.pushKV("outpoint", mnb.vin.prevout.ToStringShort());
            resultObj.pushKV("addr", mnb.addr.ToString());

            int nDos = 0;
            bool fResult;
            if (mnb.CheckSignature(nDos)) {
                if (fSafe) {
                    fResult = mnodeman.CheckMnbAndUpdateMasternodeList(NULL, mnb, nDos, *g_connman);
                } else {
                    mnodeman.UpdateMasternodeList(mnb, *g_connman);
                    mnb.Relay(*g_connman);
                    fResult = true;
                }
                mnodeman.NotifyMasternodeUpdates(*g_connman);
            } else fResult = false;

            if(fResult) {
                nSuccessful++;
                resultObj.pushKV(mnb.GetHash().ToString(), "successful");
            } else {
                nFailed++;
                resultObj.pushKV("errorMessage", "Masternode broadcast signature verification failed");
            }

            returnObj.pushKV(mnb.GetHash().ToString(), resultObj);
        }

        returnObj.pushKV("overall", strprintf("Successfully relayed broadcast messages for %d masternodes, failed to relay %d, total %d", nSuccessful, nFailed, nSuccessful + nFailed));

        return returnObj;
    }

    return NullUniValue;
}

UniValue sentinelping(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            RPCHelpMan{"sentinelping",
                "\nSentinel ping.\n",
                {
                    {"version", RPCArg::Type::STR, RPCArg::Optional::NO, "Sentinel version in the form \"x.x.x\""},
                },
                RPCResult{
            "\"state\"                      (boolean) Ping result.\n"
                },
                RPCExamples{
                    HelpExampleCli("sentinelping", "1.0.2")
            + HelpExampleRpc("sentinelping", "\"1.0.2\"")
                },
            }.ToString());
    }

    activeMasternode.UpdateSentinelPing(StringVersionToInt(request.params[0].get_str()));
    return true;
}

// Dash
static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         argNames
  //  --------------------- ------------------------  -----------------------  ----------
    { "dash",               "masternode",             &masternode,             {"command"}  },
    { "dash",               "masternodelist",         &masternodelist,         {"mode", "filter"}  },
    { "dash",               "masternodebroadcast",    &masternodebroadcast,    {"command"}  },
    { "dash",               "getpoolinfo",            &getpoolinfo,            {}  },
    { "dash",               "sentinelping",           &sentinelping,           {"version"}  },
#ifdef ENABLE_WALLET
// FXTC TODO:    { "dash",               "privatesend",            &privatesend,            {"command"}  },
#endif
};

void RegisterDashMasternodeRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
//
