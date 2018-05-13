// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/block.h>

#include <hash.h>
#include <tinyformat.h>
#include <utilstrencodings.h>
#include <crypto/common.h>
#include <crypto/scrypt.h>
#include <crypto/nist5.h>
#include <crypto/Lyra2Z.h>

uint256 CBlockHeader::GetHash() const
{
    return SerializeHash(*this);
}

uint256 CBlockHeader::GetPoWHash() const
{
    int32_t nAlgo = nVersion & ALGO_VERSION_MASK;

    uint256 powHash = uint256S("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

    switch (nAlgo)
    {
        case ALGO_SHA256D: powHash = GetHash(); break;
        case ALGO_SCRYPT:  scrypt_1024_1_1_256(BEGIN(nVersion), BEGIN(powHash)); break;
        case ALGO_NIST5:   powHash = NIST5(BEGIN(nVersion), END(nNonce)); break;
        case ALGO_LYRA2Z:  lyra2z_hash(BEGIN(nVersion), BEGIN(powHash)); break;
        default:           break; // FXTC TODO: we should not be here
    }

    return powHash;
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%u)\n",
        GetHash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce,
        vtx.size());
    for (const auto& tx : vtx) {
        s << "  " << tx->ToString() << "\n";
    }
    return s.str();
}
