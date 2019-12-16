﻿// Copyright (c) 2017 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAYDAYCOIN_ETHEREUM_ETHEREUM_H
#define PAYDAYCOIN_ETHEREUM_ETHEREUM_H

#include <vector>
#include <ethereum/commondata.h>
#include <ethereum/rlp.h>
#include <amount.h>
class CWitnessAddress;

bool VerifyProof(dev::bytesConstRef path, const dev::RLP& value, const dev::RLP& parentNodes, const dev::RLP& root); 
bool parseEthMethodInputData(const std::vector<unsigned char>& vchInputExpectedMethodHash, const std::vector<unsigned char>& vchInputData, CAmount& outputAmount, uint32_t& nAsset, CWitnessAddress& witnessAddress);
#endif // PAYDAYCOIN_ETHEREUM_ETHEREUM_H
