// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2018 FXTC developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow.h>
// FXTC BEGIN
#include <spork.h>
// FXTC END

#include <arith_uint256.h>
#include <chain.h>
#include <primitives/block.h>
#include <uint256.h>

unsigned int static DarkGravityWave(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params) {
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);

    const int64_t nPastAlgoFastBlocks = 5; // fast average for algo
    const int64_t nPastAlgoBlocks = nPastAlgoFastBlocks * ALGO_ACTIVE_COUNT; // average for algo

    const int64_t nPastFastBlocks = nPastAlgoFastBlocks * 2; //fast average for chain
    int64_t nPastBlocks = nPastFastBlocks * ALGO_ACTIVE_COUNT; // average for chain

    // stabilizing block spacing
    if ((pindexLast->nHeight + 1) >= 0)
        nPastBlocks *= 100;

    // make sure we have at least ALGO_ACTIVE_COUNT blocks, otherwise just return powLimit
    if (!pindexLast || pindexLast->nHeight < nPastBlocks) {
        if (pindexLast->nHeight < nPastAlgoBlocks)
            return bnPowLimit.GetCompact();
        else
            nPastBlocks = pindexLast->nHeight;
    }

    const CBlockIndex *pindex = pindexLast;
    const CBlockIndex *pindexFast = pindexLast;
    arith_uint256 bnPastTargetAvg(0);
    arith_uint256 bnPastTargetAvgFast(0);

    const CBlockIndex *pindexAlgo = nullptr;
    const CBlockIndex *pindexAlgoFast = nullptr;
    const CBlockIndex *pindexAlgoLast = nullptr;
    arith_uint256 bnPastAlgoTargetAvg(0);
    arith_uint256 bnPastAlgoTargetAvgFast(0);

    // count blocks mined by actual algo for secondary average
    int32_t nVersion = pblock->nVersion & ALGO_VERSION_MASK;

    unsigned int nCountBlocks = 0;
    unsigned int nCountFastBlocks = 0;
    unsigned int nCountAlgoBlocks = 0;
    unsigned int nCountAlgoFastBlocks = 0;

    while (nCountBlocks < nPastBlocks && nCountAlgoBlocks < nPastAlgoBlocks) {
        arith_uint256 bnTarget = arith_uint256().SetCompact(pindex->nBits) / pindex->GetBlockHeader().GetAlgoEfficiency(pindex->nHeight); // convert to normalized target by algo efficiency

        // calculate algo average
        if (nVersion == (pindex->nVersion & ALGO_VERSION_MASK))
        {
            nCountAlgoBlocks++;

            pindexAlgo = pindex;
            if (!pindexAlgoLast)
                pindexAlgoLast = pindex;

            // algo average
            bnPastAlgoTargetAvg = (bnPastAlgoTargetAvg * (nCountAlgoBlocks - 1) + bnTarget) / nCountAlgoBlocks;
            // fast algo average
            if (nCountAlgoBlocks <= nPastAlgoFastBlocks)
            {
                nCountAlgoFastBlocks++;
                pindexAlgoFast = pindex;
                bnPastAlgoTargetAvgFast = bnPastAlgoTargetAvg;
            }
        }

        nCountBlocks++;

        // average
        bnPastTargetAvg = (bnPastTargetAvg * (nCountBlocks - 1) + bnTarget) / nCountBlocks;
        // fast average
        if (nCountBlocks <= nPastFastBlocks)
        {
            nCountFastBlocks++;
            pindexFast = pindex;
            bnPastTargetAvgFast = bnPastTargetAvg;
        }

        // next block
        if(nCountBlocks != nPastBlocks) {
            assert(pindex->pprev); // should never fail
            pindex = pindex->pprev;
        }
    }

    // FXTC instamine protection for blockchain
    if (pindexLast->GetBlockTime() - pindexFast->GetBlockTime() < params.nPowTargetSpacing / 2)
    {
        nCountBlocks = nCountFastBlocks;
        pindex = pindexFast;
        bnPastTargetAvg = bnPastTargetAvgFast;
    }

    arith_uint256 bnNew(bnPastTargetAvg);

    if (pindexAlgo && pindexAlgoLast && nCountAlgoBlocks > 1)
    {
        // FXTC instamine protection for algo
        if (pindexLast->GetBlockTime() - pindexAlgoFast->GetBlockTime() < params.nPowTargetSpacing * ALGO_ACTIVE_COUNT / 2)
        {
            nCountAlgoBlocks = nCountAlgoFastBlocks;
            pindexAlgo = pindexAlgoFast;
            bnPastAlgoTargetAvg = bnPastAlgoTargetAvgFast;
        }

        bnNew = bnPastAlgoTargetAvg;

        // pindexLast instead of pindexAlgoLst on purpose
        int64_t nActualTimespan = pindexLast->GetBlockTime() - pindexAlgo->GetBlockTime();
        int64_t nTargetTimespan = nCountAlgoBlocks * params.nPowTargetSpacing * ALGO_ACTIVE_COUNT;

        // higher algo diff faster
        if (nActualTimespan < 1)
            nActualTimespan = 1;
        // lower algo diff slower
        if (nActualTimespan > nTargetTimespan*2)
            nActualTimespan = nTargetTimespan*2;

        // Retarget algo
        bnNew *= nActualTimespan;
        bnNew /= nTargetTimespan;
    } else {
        bnNew = bnPowLimit;
    }

    int64_t nActualTimespan = pindexLast->GetBlockTime() - pindex->GetBlockTime();
    int64_t nTargetTimespan = nCountBlocks * params.nPowTargetSpacing;

    // higher diff faster
    if (nActualTimespan < 1)
        nActualTimespan = 1;
    // lower diff slower
    if (nActualTimespan > nTargetTimespan*2)
        nActualTimespan = nTargetTimespan*2;

    // Retarget
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;

    // at least PoW limit
    if ((bnPowLimit / pblock->GetAlgoEfficiency(pindexLast->nHeight+1)) > bnNew)
        bnNew *= pblock->GetAlgoEfficiency(pindexLast->nHeight+1); // convert normalized target to actual algo target
    else
        bnNew = bnPowLimit;

    // mining handbrake via spork
    if ((bnPowLimit * GetHandbrakeForce(pblock->nVersion, pindexLast->nHeight+1)) < bnNew)
        bnNew = bnPowLimit;
    else
        bnNew /= GetHandbrakeForce(pblock->nVersion, pindexLast->nHeight+1);

    return bnNew.GetCompact();
}

unsigned int GetNextWorkRequiredBTC(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    // Only change once per difficulty adjustment interval
    if ((pindexLast->nHeight+1) % params.DifficultyAdjustmentInterval() != 0)
    {
        if (params.fPowAllowMinDifficultyBlocks)
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*2)
                return nProofOfWorkLimit;
            else
            {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // Go back by what we want to be 14 days worth of blocks
    int nHeightFirst = pindexLast->nHeight - (params.DifficultyAdjustmentInterval()-1);
    assert(nHeightFirst >= 0);
    const CBlockIndex* pindexFirst = pindexLast->GetAncestor(nHeightFirst);
    assert(pindexFirst);

    return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);
}

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    unsigned int nBits = DarkGravityWave(pindexLast, pblock, params);

    // Dead lock protection will halve work every block spacing when no block for 2 * number of active algos * block spacing (FxTC: every two minutes if no block for 10 minutes)
    int nHalvings = (pblock->GetBlockTime() - pindexLast->GetBlockTime()) / (params.nPowTargetSpacing * 2) - ALGO_ACTIVE_COUNT + 1;
    if (nHalvings > 0)
    {
        const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
        arith_uint256 bnBits;
        bnBits.SetCompact(nBits);

        // Special difficulty rule for testnet:
        // If the new block's timestamp is more than 2x block spacing
        // then allow mining of a min-difficulty block.
        // Also can not be less than PoW limit.
        if (params.fPowAllowMinDifficultyBlocks || (bnPowLimit >> nHalvings) < bnBits)
            bnBits = bnPowLimit;
        else
            bnBits <<= nHalvings;

        nBits = bnBits.GetCompact();
    }

    return nBits;
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nPowTargetTimespan/4)
        nActualTimespan = params.nPowTargetTimespan/4;
    if (nActualTimespan > params.nPowTargetTimespan*4)
        nActualTimespan = params.nPowTargetTimespan*4;

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}

// FXTC BEGIN
unsigned int GetHandbrakeForce(int32_t nVersion, int nHeight)
{
    int32_t nVersionAlgo = nVersion & ALGO_VERSION_MASK;

    // NIST5 braked and disabled
    if (nVersionAlgo == ALGO_NIST5)
    {
        if (nHeight >= 21000) return 4070908800;
        if (nHeight >= 19335) return 20;
    }

    if (nHeight >= sporkManager.GetSporkValue(SPORK_FXTC_01_HANDBRAKE_HEIGHT))
    {
        switch (nVersionAlgo)
        {
            case ALGO_SHA256D: return sporkManager.GetSporkValue(SPORK_FXTC_01_HANDBRAKE_FORCE_SHA256D);
            case ALGO_SCRYPT:  return sporkManager.GetSporkValue(SPORK_FXTC_01_HANDBRAKE_FORCE_SCRYPT);
            case ALGO_NIST5:   return sporkManager.GetSporkValue(SPORK_FXTC_01_HANDBRAKE_FORCE_NIST5);
            case ALGO_LYRA2Z:  return sporkManager.GetSporkValue(SPORK_FXTC_01_HANDBRAKE_FORCE_LYRA2Z);
            case ALGO_X11:     return sporkManager.GetSporkValue(SPORK_FXTC_01_HANDBRAKE_FORCE_X11);
            case ALGO_X16R:    return sporkManager.GetSporkValue(SPORK_FXTC_01_HANDBRAKE_FORCE_X16R);
            default:           return 1; // FXTC TODO: we should not be here
        }
    }

    return 1; // FXTC TODO: we should not be here
}
// FXTC END
