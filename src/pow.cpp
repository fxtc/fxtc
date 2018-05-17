// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2018 FXTC developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow.h>

#include <arith_uint256.h>
#include <chain.h>
#include <primitives/block.h>
#include <uint256.h>

unsigned int static DarkGravityWave(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params) {
    const int64_t nAvgBlocks = 24;

    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    int64_t nPastBlocks = nAvgBlocks * ALGO_ACTIVE_COUNT;

    // make sure we have at least ALGO_ACTIVE_COUNT blocks, otherwise just return powLimit
    if (!pindexLast || pindexLast->nHeight < nPastBlocks) {
        if (pindexLast->nHeight < (int)ALGO_ACTIVE_COUNT)
            return bnPowLimit.GetCompact();
        else
            nPastBlocks = pindexLast->nHeight;
    }

    const CBlockIndex *pindex = pindexLast;
    arith_uint256 bnPastTargetAvg(0);

    const CBlockIndex *pindexAlgo = nullptr;
    const CBlockIndex *pindexAlgoLast = nullptr;
    arith_uint256 bnPastAlgoTargetAvg(0);

    // count blocks mined by actual algo
    int32_t nVersion = pblock->nVersion & ALGO_VERSION_MASK;
    unsigned int nPastAlgoBlocks = 0;

    for (unsigned int nCountBlocks = 1; nCountBlocks <= nPastBlocks; nCountBlocks++) {
        arith_uint256 bnTarget = arith_uint256().SetCompact(pindex->nBits) / pblock->GetAlgoEfficiency(); // convert to normalized target by algo efficiency

        if (nVersion == (pindex->nVersion & ALGO_VERSION_MASK))
        {
            nPastAlgoBlocks++;

            if (!pindexAlgoLast)
                pindexAlgoLast=pindex;
            else
                pindexAlgo=pindex;

            bnPastAlgoTargetAvg = (bnPastAlgoTargetAvg * (nPastAlgoBlocks - 1) + bnTarget) / nPastAlgoBlocks;
        }

        bnPastTargetAvg = (bnPastTargetAvg * (nCountBlocks - 1) + bnTarget) / nCountBlocks;

        if(nCountBlocks != nPastBlocks) {
            assert(pindex->pprev); // should never fail
            pindex = pindex->pprev;
        }

        if (nPastAlgoBlocks == nAvgBlocks) nCountBlocks=nPastBlocks;
    }

    arith_uint256 bnNew(bnPastTargetAvg);

    if (pindexAlgo && pindexAlgoLast && nPastAlgoBlocks > 1)
    {
        bnNew = bnPastAlgoTargetAvg;

        // retarget down if no new algo block is mined
        if (pindexLast->nHeight != pindexAlgoLast->nHeight) nPastAlgoBlocks++;

        int64_t nActualTimespan = pindexLast->GetBlockTime() - pindexAlgo->GetBlockTime();
        int64_t nTargetTimespan = (nPastAlgoBlocks - 1) * params.nPowTargetSpacing * ALGO_ACTIVE_COUNT;

        if (nActualTimespan < 1)
            nActualTimespan = 1;
        if (nActualTimespan > nTargetTimespan*ALGO_ACTIVE_COUNT)
            nActualTimespan = nTargetTimespan*ALGO_ACTIVE_COUNT;

        // Retarget algo
        bnNew *= nActualTimespan;
        bnNew /= nTargetTimespan;
    } else {
        bnNew = bnPowLimit;
    }

    int64_t nActualTimespan = pindexLast->GetBlockTime() - pindex->GetBlockTime();
    int64_t nTargetTimespan = nPastBlocks * params.nPowTargetSpacing;

    if (nActualTimespan < nTargetTimespan/ALGO_ACTIVE_COUNT)
        nActualTimespan = nTargetTimespan/ALGO_ACTIVE_COUNT;
    if (nActualTimespan > nTargetTimespan*ALGO_ACTIVE_COUNT)
        nActualTimespan = nTargetTimespan*ALGO_ACTIVE_COUNT;

    // Retarget
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;

    // at least PoW limit
    if ((bnPowLimit / pblock->GetAlgoEfficiency()) > bnNew)
        bnNew *= pblock->GetAlgoEfficiency(); // convert normalized target to actual algo target
    else
        bnNew = bnPowLimit;

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
