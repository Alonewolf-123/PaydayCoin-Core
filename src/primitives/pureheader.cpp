// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The PaydayCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/pureheader.h>

#include <hash.h>
#include <util/strencodings.h>

uint256 CPureBlockHeader::GetHash() const
{
    // if(nVersion < 4)
    if (nTime % 4 == 0) {
        return HashJKS(BEGIN(nVersion), END(nNonce));
    }
    if (nTime % 4 == 1) {
        return HashKSJ(BEGIN(nVersion), END(nNonce));
    }
    if (nTime % 4 == 2) {
        return HashSJK(BEGIN(nVersion), END(nNonce));
    }
    if (nTime % 4 == 3) {
        return HashQuark(BEGIN(nVersion), END(nNonce));
    }
    // return SerializeHash(*this);
}

void CPureBlockHeader::SetBaseVersion(int32_t nBaseVersion, int32_t nChainId)
{
    assert(nBaseVersion >= 1 && nBaseVersion < VERSION_AUXPOW);
    assert(!IsAuxpow());
    nVersion = nBaseVersion | (nChainId * VERSION_CHAIN_START);
}
