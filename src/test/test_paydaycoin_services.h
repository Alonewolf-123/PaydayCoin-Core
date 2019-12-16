// Copyright (c) 2016-2018 The PaydayCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAYDAYCOIN_TEST_TEST_PAYDAYCOIN_SERVICES_H
#define PAYDAYCOIN_TEST_TEST_PAYDAYCOIN_SERVICES_H

#include <stdio.h>
#include <univalue.h>
#include <map>
using namespace std;
/** Testing paydaycoin services setup that configures a complete environment with 3 nodes.
 */
UniValue CallRPC(const string &dataDir, const string& commandWithArgs, bool regTest = true, bool readJson = true);
UniValue CallExtRPC(const string &node, const string& command, const string& args="", bool readJson = true);
void StartNode(const string &dataDir, bool regTest = true, const string& extraArgs="", bool reindex = false);
void StopNode(const string &dataDir="node1", bool regtest = true);
void StartNodes();
void StartMainNetNodes();
void StopMainNetNodes();
void StopNodes();
void GenerateBlocks(int nBlocks, const string& node="node1", bool bRegtest = true);
void GenerateSpendableCoins();
string GetNewFundedAddress(const string &node, bool bRegtest=true);
string GetNewFundedAddress(const string &node, string& txid);
void GenerateMainNetBlocks(int nBlocks, const string& node);
string CallExternal(string &cmd);
void SetSysMocktime(const int64_t& expiryTime);
void SleepFor(const int& seconds, bool actualSleep=false);
void GetOtherNodes(const string& node, string& otherNode1, string& otherNode2);
string PaydayCoinMint(const string& node, const string& address, const string& amount, int height, const string& txroot_hex, const string& tx_hex, const string& txmerkleproof_hex, const string& txmerkleproofpath_hex, const string& receipt_hex, const string& receiptroot_hex, const string& receiptmerkleproof_hex, const string& witness="''");
string AssetNew(const string& node, const string& address, string pubdata = "''", string contract="''", const string& precision="8", const string& supply = "1", const string& maxsupply = "10", const string& updateflags = "31", const string& witness = "''", const string& symbol = "SYM",  bool bRegtest = true);
void AssetUpdate(const string& node, const string& guid, const string& pubdata = "''", const string& supply = "''",  const string& updateflags = "31", const string& witness = "''");
void AssetTransfer(const string& node, const string &tonode, const string& guid, const string& toaddress, const string& witness = "''", bool bRegtest = true);
void BurnAssetAllocation(const string& node, const string &guid, const string &address,const string &amount, bool confirm=true);
void LockAssetAllocation(const string& node, const string &guid, const string &address,const string &txid,const string &index);
string AssetSend(const string& node, const string& name, const string& inputs, const string& witness = "''", bool completetx=true, bool bRegtest = true);
string AssetAllocationTransfer(const bool usezdag, const string& node, const string& name, const string& fromaddress, const string& inputs, const string& witness = "''");

// PAYDAYCOIN testing setup
struct PaydayCoinTestingSetup {
    PaydayCoinTestingSetup(); 
    ~PaydayCoinTestingSetup();
};
struct BasicPaydayCoinTestingSetup {
    BasicPaydayCoinTestingSetup();
    ~BasicPaydayCoinTestingSetup();
};
struct PaydayCoinMainNetSetup {
	PaydayCoinMainNetSetup();
	~PaydayCoinMainNetSetup();
};
#endif
