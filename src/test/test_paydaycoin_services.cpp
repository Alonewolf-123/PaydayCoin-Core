// Copyright (c) 2016-2018 The PaydayCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/test_paydaycoin_services.h>
#include <util/time.h>
#include <util/system.h>
#include <amount.h>
#include <rpc/server.h>
#include <services/asset.h>
#include <services/assetallocation.h>
#include <wallet/crypter.h>
#include <random.h>
#include <base58.h>
#include <chainparams.h>
#include <core_io.h>
#include <key_io.h>
#include <memory>
#include <string>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>
#include <bech32.h>
#include <rpc/util.h>
#include <services/rpc/assetrpc.h>
static int node1LastBlock = 0;
static int node2LastBlock = 0;
static int node3LastBlock = 0;
static bool node1Online = false;
static bool node2Online = false;
static bool node3Online = false;
std::map<string, string> mapNodes;
// create a map between node alias names and URLs to be used in testing for example CallRPC("mynode", "getblockchaininfo") would call getblockchaininfo on the node alias mynode which would be pushed as a URL here.
// it is assumed RPC ports are open and u:p is the authentication
void InitNodeURLMap() {
	mapNodes.clear();
	mapNodes["node1"] = "http://127.0.0.1:28379";
	mapNodes["node2"] = "http://127.0.0.1:38379";
	mapNodes["node3"] = "http://127.0.0.1:48379";

}
// lookup the URL based on node passed in
string LookupURL(const string& node) {
	if (mapNodes.find(node) != mapNodes.end())
		return mapNodes[node];
	return "";
}
// PAYDAYCOIN testing setup
void StartNodes()
{
	printf("Stopping any test nodes that are running...\n");
	InitNodeURLMap();
	StopNodes();
    if (boost::filesystem::exists(boost::filesystem::system_complete("node1/regtest")))
        boost::filesystem::remove_all(boost::filesystem::system_complete("node1/regtest")); 
    if (boost::filesystem::exists(boost::filesystem::system_complete("node2/regtest")))
        boost::filesystem::remove_all(boost::filesystem::system_complete("node2/regtest"));  
    if (boost::filesystem::exists(boost::filesystem::system_complete("node3/regtest")))
        boost::filesystem::remove_all(boost::filesystem::system_complete("node3/regtest"));                     
	node1LastBlock = 0;
	node2LastBlock = 0;
	node3LastBlock = 0;
	//StopMainNetNodes();
	printf("Starting 3 nodes in a regtest setup...\n");
	StartNode("node1");
    BOOST_CHECK_NO_THROW(CallExtRPC("node1", "generate", "5"));
	StartNode("node2");
	StartNode("node3");

}
void StartMainNetNodes()
{
	StopMainNetNodes();
	printf("Starting 2 nodes in mainnet setup...\n");
	StartNode("mainnet1", false);
	SelectParams(CBaseChainParams::MAIN);

}
void StopMainNetNodes()
{
	printf("Stopping mainnet1..\n");
	StopNode("mainnet1", false);
	printf("Done!\n");
}
void StopNodes()
{
	StopNode("node1");
	StopNode("node2");
	StopNode("node3");
	printf("Done!\n");
}
void StartNode(const string &dataDir, bool regTest, const string& extraArgs, bool reindex)
{
	boost::filesystem::path fpath = boost::filesystem::system_complete("../paydaycoind");
	string nodePath = fpath.string() + string(" -unittest -tpstest -assetindex -daemon -server -debug=0 -concurrentprocessing=1 -datadir=") + dataDir;
	if (regTest)
		nodePath += string(" -regtest");
	if (reindex)
		nodePath += string(" -reindex");		
	if (!extraArgs.empty())
		nodePath += string(" ") + extraArgs;

	boost::thread t(runCommand, nodePath);
	printf("Launching %s, waiting 1 second before trying to ping...\n", nodePath.c_str());
	MilliSleep(1000);
	UniValue r;
	while (1)
	{
		try {
			printf("Calling getblockchaininfo!\n");
			r = CallRPC(dataDir, "getblockchaininfo", regTest);
			if (dataDir == "node1")
			{
				if (node1LastBlock > find_value(r.get_obj(), "blocks").get_int())
				{
					printf("Waiting for %s to catch up, current block number %d vs total blocks %d...\n", dataDir.c_str(), find_value(r.get_obj(), "blocks").get_int(), node1LastBlock);
					MilliSleep(500);
					continue;
				}
				node1Online = true;
				node1LastBlock = 0;
			}
			else if (dataDir == "node2")
			{
				if (node2LastBlock > find_value(r.get_obj(), "blocks").get_int())
				{
					printf("Waiting for %s to catch up, current block number %d vs total blocks %d...\n", dataDir.c_str(), find_value(r.get_obj(), "blocks").get_int(), node2LastBlock);
					MilliSleep(500);
					continue;
				}
				node2Online = true;
				node2LastBlock = 0;
			}
			else if (dataDir == "node3")
			{
				if (node3LastBlock > find_value(r.get_obj(), "blocks").get_int())
				{
					printf("Waiting for %s to catch up, current block number %d vs total blocks %d...\n", dataDir.c_str(), find_value(r.get_obj(), "blocks").get_int(), node3LastBlock);
					MilliSleep(500);
					continue;
				}
				node3Online = true;
				node3LastBlock = 0;
			}
			MilliSleep(500);
		}
		catch (const runtime_error& error)
		{
			printf("Waiting for %s to come online, trying again in 1 second...\n", dataDir.c_str());
			MilliSleep(1000);
			continue;
		}
		break;
	}
	printf("Done!\n");
}

void StopNode(const string &dataDir, bool regtest) {
	printf("Stopping %s..\n", dataDir.c_str());
	UniValue r;
	try {
		r = CallRPC(dataDir, "getblockchaininfo", regtest);
		if (r.isObject())
		{
			if (dataDir == "node1")
				node1LastBlock = find_value(r.get_obj(), "blocks").get_int();
			else if (dataDir == "node2")
				node2LastBlock = find_value(r.get_obj(), "blocks").get_int();
			else if (dataDir == "node3")
				node3LastBlock = find_value(r.get_obj(), "blocks").get_int();
		}
	}
	catch (const runtime_error& error)
	{
	}
	try {
		CallRPC(dataDir, "stop", regtest);
	}
	catch (const runtime_error& error)
	{
	}
	while (1)
	{
		try {
			MilliSleep(1000);
			CallRPC(dataDir, "getblockchaininfo", regtest);
		}
		catch (const runtime_error& error)
		{
			break;
		}
	}
	try {
		if (dataDir == "node1")
			node1Online = false;
		else if (dataDir == "node2")
			node2Online = false;
		else if (dataDir == "node3")
			node3Online = false;
	}
	catch (const runtime_error& error)
	{

	}
}
UniValue CallExtRPC(const string &node, const string& command, const string& args, bool readJson)
{
	string url = LookupURL(node);
	BOOST_CHECK(!url.empty());
	UniValue val;
	string curlcmd = "curl -s --user u:p --data-binary '{\"jsonrpc\":\"1.0\",\"id\":\"unittest\",\"method\":\"" + command + "\",\"params\":[" + args + "]}' -H 'content-type:text/plain;' " + url;

    string rawJson = CallExternal(curlcmd);

	val.read(rawJson);
  
    if(!readJson)
        return val;
	if (val.isNull())
		throw runtime_error("Could not parse rpc results");
 
	// try to get error message if exist
	UniValue errorValue = find_value(val.get_obj(), "error");
	if (errorValue.isObject()) {
		throw runtime_error(find_value(errorValue.get_obj(), "message").get_str());
	}
	return find_value(val.get_obj(), "result");
}
UniValue CallRPC(const string &dataDir, const string& commandWithArgs, bool regTest, bool readJson)
{
	UniValue val;
	boost::filesystem::path fpath = boost::filesystem::system_complete("../paydaycoin-cli");
	string path = fpath.string() + string(" -datadir=") + dataDir;
	if (regTest)
		path += string(" -regtest ");
	else
		path += " ";
	path += commandWithArgs;
	string rawJson = CallExternal(path);
	if (readJson)
	{
		val.read(rawJson);
		if (val.isNull())
			throw runtime_error("Could not parse rpc results");
	}
	else
		val.setStr(rawJson);
	return val;
}
int fsize(FILE *fp) {
	int prev = ftell(fp);
	fseek(fp, 0L, SEEK_END);
	int sz = ftell(fp);
	fseek(fp, prev, SEEK_SET); //go back to where we were
	return sz;
}
void safe_fclose(FILE* file)
{
	if (file)
		BOOST_VERIFY(0 == fclose(file));
	if (boost::filesystem::exists("cmdoutput.log"))
		boost::filesystem::remove("cmdoutput.log");
}
int runSysCommand(const std::string& strCommand)
{
	int nErr = ::system(strCommand.c_str());
	return nErr;
}
std::string CallExternal(std::string &cmd)
{
	cmd += " > cmdoutput.log || true";
	if (runSysCommand(cmd))
		return string("ERROR");
	boost::shared_ptr<FILE> pipe(fopen("cmdoutput.log", "r"), safe_fclose);
	if (!pipe) return "ERROR";
	char buffer[128];
	std::string result = "";
	if (fsize(pipe.get()) > 0)
	{
		while (!feof(pipe.get())) {
			if (fgets(buffer, 128, pipe.get()) != NULL)
				result += buffer;
		}
	}
	return result;
}
void GenerateMainNetBlocks(int nBlocks, const string& node)
{
	int height, targetHeight, newHeight, timeoutCounter;
	UniValue r;
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "getblockchaininfo", false));
	targetHeight = find_value(r.get_obj(), "blocks").get_int() + nBlocks;
	newHeight = 0;
	const string &sBlocks = strprintf("%d", nBlocks);
	string otherNode1, otherNode2;
	GetOtherNodes(node, otherNode1, otherNode2);
	while (newHeight < targetHeight)
	{
		BOOST_CHECK_NO_THROW(r = CallRPC(node, "generate " + sBlocks, false));
		BOOST_CHECK_NO_THROW(r = CallRPC(node, "getblockchaininfo", false));
		newHeight = find_value(r.get_obj(), "blocks").get_int();
		BOOST_CHECK_NO_THROW(r = CallRPC(node, "getwalletinfo", false));
		CAmount balance = AmountFromValue(find_value(r.get_obj(), "balance"));
		printf("Current block height %d, Target block height %d, balance %f\n", newHeight, targetHeight, ValueFromAmount(balance).get_real());
	}
	BOOST_CHECK(newHeight >= targetHeight);
	height = 0;
	timeoutCounter = 0;
	MilliSleep(10);
	while (!otherNode1.empty() && height < newHeight)
	{
		try
		{
			r = CallRPC(otherNode1, "getblockchaininfo", false);
		}
		catch (const runtime_error &e)
		{
			r = NullUniValue;
		}
		if (!r.isObject())
		{
			height = newHeight;
			break;
		}
		height = find_value(r.get_obj(), "blocks").get_int();
		timeoutCounter++;
		if (timeoutCounter > 100) {
			printf("Error: Timeout on getblockchaininfo for %s, height %d vs newHeight %d!\n", otherNode1.c_str(), height, newHeight);
			break;
		}
		MilliSleep(10);
	}
	if (!otherNode1.empty())
		BOOST_CHECK(height >= targetHeight);
}
// generate n Blocks, with up to 10 seconds relay time buffer for other nodes to get the blocks.
// may fail if your network is slow or you try to generate too many blocks such that can't relay within 10 seconds
void GenerateBlocks(int nBlocks, const string& node, bool bRegtest)
{
	if(!bRegtest){
		GenerateMainNetBlocks(nBlocks, node);
		return;
	}
	int height, newHeight, timeoutCounter;
	UniValue r;
	string otherNode1, otherNode2;
	GetOtherNodes(node, otherNode1, otherNode2);
	try
	{
		r = CallRPC(node, "getblockchaininfo", bRegtest);
	}
	catch (const runtime_error &e)
	{
		return;
	}
	newHeight = find_value(r.get_obj(), "blocks").get_int() + nBlocks;
	const string &sBlocks = strprintf("%d", nBlocks);
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "generate " + sBlocks, bRegtest));
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "getblockchaininfo", bRegtest));
	height = find_value(r.get_obj(), "blocks").get_int();
	BOOST_CHECK(height >= newHeight);
	height = 0;
	timeoutCounter = 0;
	MilliSleep(10);
	while (!otherNode1.empty() && height < newHeight)
	{
		try
		{
			r = CallRPC(otherNode1, "getblockchaininfo", bRegtest);
		}
		catch (const runtime_error &e)
		{
			r = NullUniValue;
		}
		if (!r.isObject())
		{
			height = newHeight;
			break;
		}
		height = find_value(r.get_obj(), "blocks").get_int();
		timeoutCounter++;
		if (timeoutCounter > 100) {
			printf("Error: Timeout on getblockchaininfo for %s, height %d vs newHeight %d!\n", otherNode1.c_str(), height, newHeight);
			break;
		}
		MilliSleep(10);
	}
	if (!otherNode1.empty())
		BOOST_CHECK(height >= newHeight);
	height = 0;
	timeoutCounter = 0;
	while (!otherNode2.empty() && height < newHeight)
	{
		try
		{
			r = CallRPC(otherNode2, "getblockchaininfo", bRegtest);
		}
		catch (const runtime_error &e)
		{
			r = NullUniValue;
		}
		if (!r.isObject())
		{
			height = newHeight;
			break;
		}
		height = find_value(r.get_obj(), "blocks").get_int();
		timeoutCounter++;
		if (timeoutCounter > 100) {
			printf("Error: Timeout on getblockchaininfo for %s, height %d vs newHeight %d!\n", otherNode2.c_str(), height, newHeight);
			break;
		}
		MilliSleep(10);
	}
	if (!otherNode2.empty())
		BOOST_CHECK(height >= newHeight);
	height = 0;
	timeoutCounter = 0;
}
void GenerateSpendableCoins() {
	UniValue r;

	const string &sBlocks = strprintf("%d", 101);
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "generate", sBlocks));
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "getnewaddress"));

	string newaddress = r.get_str();
	CallExtRPC("node1", "sendtoaddress", "\"" + newaddress + "\",\"100000\"", false);
	GenerateBlocks(10, "node1");
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "getnewaddress"));
	newaddress = r.get_str();
	CallExtRPC("node1", "sendtoaddress", "\"" + newaddress + "\",\"100000\"", false);
	GenerateBlocks(10, "node1");
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "getnewaddress"));
	newaddress = r.get_str();
	CallExtRPC("node1", "sendtoaddress", "\"" + newaddress + "\",\"100000\"", false);
	GenerateBlocks(10, "node1");
}
string GetNewFundedAddress(const string &node, bool bRegtest) {
	UniValue r;
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "getnewaddress", bRegtest, false));
	string newaddress = r.get_str();
	newaddress.erase(std::remove(newaddress.begin(), newaddress.end(), '\n'), newaddress.end());
	string sendnode = "";
	if (node == "node1")
		sendnode = "node2";
	else if (node == "node2")
		sendnode = "node3";
	else if (node == "node3")
		sendnode = "node2";
	else if(node == "mainnet1")
		sendnode = "mainnet1";
    CallRPC(bRegtest? sendnode: node, "sendtoaddress " + newaddress + " 10", bRegtest, false);
	if(bRegtest)
		GenerateBlocks(1, sendnode, bRegtest);
	GenerateBlocks(1, node, bRegtest);
    BOOST_CHECK_NO_THROW(r = CallRPC(node, "addressbalance " + newaddress, bRegtest));
    CAmount nAmount = AmountFromValue(find_value(r.get_obj(), "amount"));
    BOOST_CHECK_EQUAL(nAmount, 10*COIN);
	return newaddress;
}
string GetNewFundedAddress(const string &node, string& txid) {
    UniValue r;
    BOOST_CHECK_NO_THROW(r = CallExtRPC(node, "getnewaddress"));
    string newaddress = r.get_str();
    string sendnode = "";
    if (node == "node1")
        sendnode = "node2";
    else if (node == "node2")
        sendnode = "node3";
    else if (node == "node3")
        sendnode = "node2";
    r = CallExtRPC(sendnode, "sendtoaddress", "\"" + newaddress + "\",\"5\"");
    r = CallExtRPC(sendnode, "sendtoaddress", "\"" + newaddress + "\",\"5\"");
    r = CallExtRPC(sendnode, "sendtoaddress", "\"" + newaddress + "\",\"5\"");
    r = CallExtRPC(sendnode, "sendtoaddress", "\"" + newaddress + "\",\"5\"");
    r = CallExtRPC(sendnode, "sendtoaddress", "\"" + newaddress + "\",\"5\"");
    txid = r.get_str();
    GenerateBlocks(10, sendnode);
    GenerateBlocks(10, node);
    BOOST_CHECK_NO_THROW(r = CallRPC(node, "addressbalance " + newaddress));
    CAmount nAmount = AmountFromValue(find_value(r.get_obj(), "amount"));
    BOOST_CHECK_EQUAL(nAmount, 25*COIN);
    return newaddress;
}
void SleepFor(const int& milliseconds, bool actualSleep) {
	if (actualSleep)
		MilliSleep(milliseconds);
	float seconds = milliseconds / 1000;
	BOOST_CHECK(seconds > 0);
	UniValue r;
	try
	{
		r = CallExtRPC("node1", "getblockchaininfo");
		int64_t currentTime = find_value(r.get_obj(), "mediantime").get_int64();
		currentTime += seconds;
		SetSysMocktime(currentTime);
	}
	catch (const runtime_error &e)
	{
	}
	try
	{
		r = CallExtRPC("node2", "getblockchaininfo");
		int64_t currentTime = find_value(r.get_obj(), "mediantime").get_int64();
		currentTime += seconds;
		SetSysMocktime(currentTime);
	}
	catch (const runtime_error &e)
	{
	}
	try
	{
		r = CallExtRPC("node3", "getblockchaininfo");
		int64_t currentTime = find_value(r.get_obj(), "mediantime").get_int64();
		currentTime += seconds;
		SetSysMocktime(currentTime);
	}
	catch (const runtime_error &e)
	{
	}
}
void SetSysMocktime(const int64_t& expiryTime) {
	BOOST_CHECK(expiryTime > 0);
	string expiryStr = strprintf("%lld", expiryTime);
	try
	{
		CallExtRPC("node1", "getblockchaininfo");
		BOOST_CHECK_NO_THROW(CallExtRPC("node1", "setmocktime", "\"" + expiryStr + "\""));
		GenerateBlocks(5, "node1");
		GenerateBlocks(5, "node1");
		GenerateBlocks(5, "node1");
		UniValue r;
		BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "getblockchaininfo"));
		BOOST_CHECK(expiryTime <= find_value(r.get_obj(), "mediantime").get_int64());
	}
	catch (const runtime_error &e)
	{
	}
	try
	{
		CallExtRPC("node2", "getblockchaininfo");
		BOOST_CHECK_NO_THROW(CallExtRPC("node2", "setmocktime", "\"" + expiryStr + "\""));
		GenerateBlocks(5, "node2");
		GenerateBlocks(5, "node2");
		GenerateBlocks(5, "node2");
		UniValue r;
		BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "getblockchaininfo"));
		BOOST_CHECK(expiryTime <= find_value(r.get_obj(), "mediantime").get_int64());
	}
	catch (const runtime_error &e)
	{
	}
	try
	{
		CallExtRPC("node3", "getblockchaininfo");
		BOOST_CHECK_NO_THROW(CallExtRPC("node3", "setmocktime", "\"" + expiryStr + "\""));
		GenerateBlocks(5, "node3");
		GenerateBlocks(5, "node3");
		GenerateBlocks(5, "node3");
		UniValue r;
		BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "getblockchaininfo"));
		BOOST_CHECK(expiryTime <= find_value(r.get_obj(), "mediantime").get_int64());
	}
	catch (const runtime_error &e)
	{
	}
}
void GetOtherNodes(const string& node, string& otherNode1, string& otherNode2)
{
	otherNode1 = "";
	otherNode2 = "";
	if (node == "node1")
	{
		if (node2Online)
			otherNode1 = "node2";
		if (node3Online)
			otherNode2 = "node3";
	}
	else if (node == "node2")
	{
		if (node1Online)
			otherNode1 = "node1";
		if (node3Online)
			otherNode2 = "node3";
	}
	else if (node == "node3")
	{
		if (node1Online)
			otherNode1 = "node1";
		if (node2Online)
			otherNode2 = "node2";
	}
}
string PaydayCoinMint(const string& node, const string& address, const string& amount, int height, const string& tx_hex, const string& txroot_hex, const string& txmerkleproof_hex, const string& txmerkleproofpath_hex, const string& receipt_hex, const string& receiptroot_hex, const string& receiptmerkleproof_hex, const string& witness)
{
    string otherNode1, otherNode2;
    GetOtherNodes(node, otherNode1, otherNode2);
    UniValue r;
    BOOST_CHECK_NO_THROW(r = CallRPC(node, "addressbalance " + address));
    CAmount nAmountBefore = AmountFromValue(find_value(r.get_obj(), "amount"));
    // ensure that block number you claim the burn is at least 1 hour old
    int heightPlus240 = height+(ETHEREUM_CONFIRMS_REQUIRED*1.5);
    string headerStr = "\"[[" + itostr(height) + ", \\\"" + itostr(height) + "\\\", \\\"" + itostr(height-1) + "\\\", \\\"" + txroot_hex + "\\\", \\\"" + receiptroot_hex + "\\\"]]\"";
 
    
    BOOST_CHECK_NO_THROW(r = CallRPC(node, "paydaycoinsetethheaders " + headerStr));
    BOOST_CHECK_NO_THROW(r = CallRPC(node, "paydaycoinsetethstatus synced " + itostr(heightPlus240)));
    if (!otherNode1.empty())
    {
        
        BOOST_CHECK_NO_THROW(r = CallRPC(otherNode1, "paydaycoinsetethheaders " + headerStr));
        BOOST_CHECK_NO_THROW(r = CallRPC(otherNode1, "paydaycoinsetethstatus synced " + itostr(heightPlus240)));
    }
    if (!otherNode2.empty())
    {
        
        BOOST_CHECK_NO_THROW(r = CallRPC(otherNode2, "paydaycoinsetethheaders " + headerStr));
        BOOST_CHECK_NO_THROW(r = CallRPC(otherNode2, "paydaycoinsetethstatus synced " + itostr(heightPlus240)));
    }   
    // "paydaycoinmint [address] [amount] [blocknumber] [tx_hex] [txroot_hex] [txmerkleproof_hex] [txmerkleroofpath_hex] [receipt_hex] [receiptroot_hex] [receiptmerkleproof_hex] [witness]\n"
    BOOST_CHECK_NO_THROW(r = CallRPC(node, "paydaycoinmint " + address + " " + amount + " " + itostr(height) + " " + tx_hex + " a0" + txroot_hex + " " + txmerkleproof_hex + " " + txmerkleproofpath_hex + " " + receipt_hex + " a0" + receiptroot_hex + " " + receiptmerkleproof_hex +  " " + witness));
    
    BOOST_CHECK_NO_THROW(r = CallRPC(node, "signrawtransactionwithwallet " + find_value(r.get_obj(), "hex").get_str()));
    string hex_str = find_value(r.get_obj(), "hex").get_str();
   
    BOOST_CHECK_NO_THROW(r = CallRPC(node, "sendrawtransaction " + hex_str, true, false));  
    GenerateBlocks(5, node);
    BOOST_CHECK_NO_THROW(r = CallRPC(node, "addressbalance " + address));
    CAmount nAmountAfter = AmountFromValue(find_value(r.get_obj(), "amount"));
    // account for fees
    BOOST_CHECK(abs((nAmountBefore+AmountFromValue(amount)) -  nAmountAfter) < 0.1*COIN);
    return hex_str;
}
bool FindAssetGUIDFromAssetIndexResults(const UniValue& results, std::string guid){
	UniValue arrayVal = results.get_array();
	for(unsigned i = 0;i<arrayVal.size();i++){
		UniValue r = arrayVal[i];
		if(itostr(find_value(r.get_obj(), "asset_guid").get_int()) == guid)
			return true;
	}
	return false;
}
	
string AssetNew(const string& node, const string& address, string pubdata, string contract, const string& precision, const string& supply, const string& maxsupply, const string& updateflags, const string& witness, const string& symbol, bool bRegtest)
{
	string otherNode1, otherNode2;
	GetOtherNodes(node, otherNode1, otherNode2);
	UniValue r;
	// "assetnew [address] [symbol] [public value] [contract] [precision=8] [supply] [max_supply] [update_flags] [witness]\n"
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "assetnew " + address + " " + symbol + " ""\"" + pubdata + "\" " + contract + " " + precision + " " + supply + " " + maxsupply + " " + updateflags + " " + witness, bRegtest));
	if(contract == "''")
		contract.clear();
	if(pubdata == "''")
		pubdata.clear();
	string guid = itostr(find_value(r.get_obj(), "asset_guid").get_int());
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "signrawtransactionwithwallet " + find_value(r.get_obj(), "hex").get_str(), bRegtest));
	string hex_str = find_value(r.get_obj(), "hex").get_str();
   
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "sendrawtransaction " + hex_str, bRegtest, false)); 
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "decoderawtransaction " + hex_str + " true", bRegtest));
	
	GenerateBlocks(1, node, bRegtest);
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "assetinfo " + guid, bRegtest ));
	int nprecision;
	ParseInt32(precision, &nprecision);
	BOOST_CHECK(itostr(find_value(r.get_obj(), "asset_guid").get_int()) == guid);
	BOOST_CHECK(find_value(r.get_obj(), "address").get_str() == address);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "public_value").get_str() , pubdata);
	UniValue balance = find_value(r.get_obj(), "balance");
	UniValue totalsupply = find_value(r.get_obj(), "total_supply");
	UniValue maxsupplyu = find_value(r.get_obj(), "max_supply");
	UniValue supplytmp(UniValue::VSTR);
	supplytmp.setStr(supply);
	UniValue maxsupplytmp(UniValue::VSTR);
	maxsupplytmp.setStr(maxsupply);
	BOOST_CHECK(AssetAmountFromValue(balance, nprecision) == AssetAmountFromValue(supplytmp, nprecision));
	BOOST_CHECK(AssetAmountFromValue(totalsupply, nprecision) == AssetAmountFromValue(supplytmp, nprecision));
	BOOST_CHECK_EQUAL(AssetAmountFromValue(maxsupplyu, nprecision), AssetAmountFromValue(maxsupplytmp, nprecision));
	int update_flags = find_value(r.get_obj(), "update_flags").get_int();
	int paramUpdateFlags;
	ParseInt32(updateflags, &paramUpdateFlags);

	BOOST_CHECK(update_flags == paramUpdateFlags);

	BOOST_CHECK_NO_THROW(r = CallRPC(node, "listassetindexassets " + address , bRegtest));
	BOOST_CHECK(FindAssetGUIDFromAssetIndexResults(r, guid));
	GenerateBlocks(1, node, bRegtest);
	if (!otherNode1.empty())
	{
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode1, "assetinfo " + guid , bRegtest));
		BOOST_CHECK(itostr(find_value(r.get_obj(), "asset_guid").get_int()) == guid);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "public_value").get_str() , pubdata);
		UniValue balance = find_value(r.get_obj(), "balance");
		UniValue totalsupply = find_value(r.get_obj(), "total_supply");
		UniValue maxsupplyu = find_value(r.get_obj(), "max_supply");
		BOOST_CHECK(AssetAmountFromValue(balance, nprecision) == AssetAmountFromValue(supplytmp, nprecision));
		BOOST_CHECK(AssetAmountFromValue(totalsupply, nprecision) == AssetAmountFromValue(supplytmp, nprecision));
		BOOST_CHECK_EQUAL(AssetAmountFromValue(maxsupplyu, nprecision), AssetAmountFromValue(maxsupplytmp, nprecision));
        int update_flags = find_value(r.get_obj(), "update_flags").get_int();
        int paramUpdateFlags;
		ParseInt32(updateflags, &paramUpdateFlags);
        BOOST_CHECK(update_flags == paramUpdateFlags);
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode1, "listassetindexassets " + address , bRegtest));
		BOOST_CHECK(FindAssetGUIDFromAssetIndexResults(r, guid));
	}
	if (!otherNode2.empty())
	{
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode2, "assetinfo " + guid, bRegtest));
		BOOST_CHECK(itostr(find_value(r.get_obj(), "asset_guid").get_int()) == guid);
		BOOST_CHECK(find_value(r.get_obj(), "public_value").get_str() == pubdata);
		UniValue balance = find_value(r.get_obj(), "balance");
		UniValue totalsupply = find_value(r.get_obj(), "total_supply");
		UniValue maxsupplyu = find_value(r.get_obj(), "max_supply");
		BOOST_CHECK(AssetAmountFromValue(balance, nprecision) == AssetAmountFromValue(supplytmp, nprecision));
		BOOST_CHECK(AssetAmountFromValue(totalsupply, nprecision) == AssetAmountFromValue(supplytmp, nprecision));
		BOOST_CHECK_EQUAL(AssetAmountFromValue(maxsupplyu, nprecision), AssetAmountFromValue(maxsupplytmp, nprecision));

        int update_flags = find_value(r.get_obj(), "update_flags").get_int();
        int paramUpdateFlags;
		ParseInt32(updateflags, &paramUpdateFlags);
        BOOST_CHECK(update_flags == paramUpdateFlags);
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode2, "listassetindexassets " + address , bRegtest));
		BOOST_CHECK(FindAssetGUIDFromAssetIndexResults(r, guid));
	}
	return guid;
}
void AssetUpdate(const string& node, const string& guid, const string& pubdata, const string& supply, const string& updateflags, const string& witness)
{
	string otherNode1, otherNode2;
	GetOtherNodes(node, otherNode1, otherNode2);
	UniValue r;
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "assetinfo " + guid));
	string oldaddress = find_value(r.get_obj(), "address").get_str();
	string oldpubdata = find_value(r.get_obj(), "public_value").get_str();
    UniValue totalsupply = find_value(r.get_obj(), "total_supply");
	int nprecision = find_value(r.get_obj(), "precision").get_int();
    CAmount oldsupplyamount = AssetAmountFromValue(totalsupply, nprecision);
	int oldflags = find_value(r.get_obj(), "update_flags").get_int();
    
	CAmount supplyamount = 0;
	if (supply != "''") {
		UniValue supplytmp(UniValue::VSTR);
		supplytmp.setStr(supply);
		supplyamount = AssetAmountFromValue(supplytmp, nprecision);
	}
	CAmount newamount = oldsupplyamount + supplyamount;
   
	string newpubdata = pubdata == "''" ? oldpubdata : pubdata;
	string newsupply = supply == "''" ? "0" : supply;
	int flagsi;
	ParseInt32(updateflags, &flagsi);
	int newflags = updateflags == "''" ? oldflags : flagsi;
	string newflagsstr = itostr(newflags);
	// "assetupdate [asset] [public] [contract] [supply] [update_flags] [witness]\n"
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "assetupdate " + guid + " " + newpubdata + " '' " +  newsupply + " " + newflagsstr + " " + witness));
    BOOST_CHECK_NO_THROW(r = CallRPC(node, "signrawtransactionwithwallet " + find_value(r.get_obj(), "hex").get_str()));
	string hex_str = find_value(r.get_obj(), "hex").get_str();
   
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "sendrawtransaction " + hex_str, true, false)); 
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "paydaycoindecoderawtransaction " + hex_str));
	UniValue flagValue = find_value(r.get_obj(), "update_flags");
	if (newflags != oldflags && newflags != 0) {
		BOOST_CHECK_EQUAL(flagValue.get_int(), newflags);
	}
	else {
		BOOST_CHECK(flagValue.isNull());
	}
	// ensure sender state not changed before generating blocks
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "assetinfo " + guid));
	BOOST_CHECK(itostr(find_value(r.get_obj(), "asset_guid").get_int()) == guid);
	BOOST_CHECK(find_value(r.get_obj(), "address").get_str() == oldaddress);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "public_value").get_str(), oldpubdata);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "update_flags").get_int(), oldflags);

	totalsupply = find_value(r.get_obj(), "total_supply");
	BOOST_CHECK(AssetAmountFromValue(totalsupply, nprecision) == oldsupplyamount);
	GenerateBlocks(5, node);
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "assetinfo " + guid));

	BOOST_CHECK(itostr(find_value(r.get_obj(), "asset_guid").get_int()) == guid);
	BOOST_CHECK(find_value(r.get_obj(), "address").get_str() == oldaddress);
	totalsupply = find_value(r.get_obj(), "total_supply");
	BOOST_CHECK(AssetAmountFromValue(totalsupply, nprecision) == newamount);

	BOOST_CHECK_NO_THROW(r = CallRPC(node, "listassetindexassets " + oldaddress ));
	BOOST_CHECK(FindAssetGUIDFromAssetIndexResults(r, guid));
	GenerateBlocks(6, node);
	if (!otherNode1.empty())
	{
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode1, "assetinfo " + guid ));
		BOOST_CHECK(itostr(find_value(r.get_obj(), "asset_guid").get_int()) == guid);
		BOOST_CHECK(find_value(r.get_obj(), "address").get_str() == oldaddress);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "public_value").get_str(), newpubdata);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "update_flags").get_int(), newflags);
	
		totalsupply = find_value(r.get_obj(), "total_supply");
		BOOST_CHECK(AssetAmountFromValue(totalsupply, nprecision) == newamount);
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode1, "listassetindexassets " + oldaddress ));
		BOOST_CHECK(FindAssetGUIDFromAssetIndexResults(r, guid));
	}
	if (!otherNode2.empty())
	{
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode2, "assetinfo " + guid));
		BOOST_CHECK(itostr(find_value(r.get_obj(), "asset_guid").get_int()) == guid);
		BOOST_CHECK(find_value(r.get_obj(), "address").get_str() == oldaddress);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "public_value").get_str(), newpubdata);
		totalsupply = find_value(r.get_obj(), "total_supply");
		BOOST_CHECK(AssetAmountFromValue(totalsupply, nprecision) == newamount);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "update_flags").get_int(), newflags);
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode2, "listassetindexassets " + oldaddress ));
		BOOST_CHECK(FindAssetGUIDFromAssetIndexResults(r, guid));
	}

}
void AssetTransfer(const string& node, const string &tonode, const string& guid, const string& toaddress, const string& witness, bool bRegtest)
{
	UniValue r;

	BOOST_CHECK_NO_THROW(r = CallRPC(node, "assetinfo " + guid , bRegtest));
	string oldaddress = find_value(r.get_obj(), "address").get_str();


	// "assettransfer [asset] [address] [witness]\n"
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "assettransfer " + guid + " " + toaddress + " " + witness, bRegtest));
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "signrawtransactionwithwallet " + find_value(r.get_obj(), "hex").get_str(), bRegtest));
	string hex_str = find_value(r.get_obj(), "hex").get_str();
    
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "sendrawtransaction " + hex_str, bRegtest, false));
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "decoderawtransaction " + hex_str + " true", bRegtest));

	GenerateBlocks(1, node, bRegtest);
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "assetinfo " + guid, bRegtest));


	BOOST_CHECK(itostr(find_value(r.get_obj(), "asset_guid").get_int()) == guid);
	if(bRegtest){
		BOOST_CHECK_NO_THROW(r = CallRPC(tonode, "assetinfo " + guid, bRegtest));
		BOOST_CHECK(find_value(r.get_obj(), "address").get_str() == toaddress);
	}

	BOOST_CHECK_NO_THROW(r = CallRPC(node, "listassetindexassets " + toaddress , bRegtest));
	BOOST_CHECK(FindAssetGUIDFromAssetIndexResults(r, guid));
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "listassetindexassets " + oldaddress , bRegtest));
	BOOST_CHECK(!FindAssetGUIDFromAssetIndexResults(r, guid));


}
string AssetAllocationTransfer(const bool usezdag, const string& node, const string& guid, const string& fromaddress, const string& inputs, const string& witness) {

	UniValue r;
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "assetinfo " + guid));
	int nprecision = find_value(r.get_obj(), "precision").get_int();

	CAssetAllocation theAssetAllocation;
	UniValue valueTo;
	string inputsTmp = inputs;
	boost::replace_all(inputsTmp, "\\\"", "\"");
	boost::replace_all(inputsTmp, "\"[", "[");
	boost::replace_all(inputsTmp, "]\"", "]");
	BOOST_CHECK(valueTo.read(inputsTmp));
	BOOST_CHECK(valueTo.isArray());
	UniValue receivers = valueTo.get_array();
	CAmount inputamount = 0;
	BOOST_CHECK(receivers.size() > 0);
	for (unsigned int idx = 0; idx < receivers.size(); idx++) {
		const UniValue& receiver = receivers[idx];
		BOOST_CHECK(receiver.isObject());
      
		UniValue receiverObj = receiver.get_obj();
        
        const CTxDestination &dest = DecodeDestination(find_value(receiverObj, "address").get_str());
        UniValue detail = DescribeAddress(dest);
        string witnessProgramHex = find_value(detail.get_obj(), "witness_program").get_str();
        unsigned char witnessVersion = (unsigned char)find_value(detail.get_obj(), "witness_version").get_int();                  
		UniValue amountObj = find_value(receiverObj, "amount");
		if (amountObj.isNum()) {
			const CAmount &amount = AssetAmountFromValue(amountObj, nprecision);
			inputamount += amount;
			BOOST_CHECK(amount > 0);
			theAssetAllocation.listSendingAllocationAmounts.push_back(make_pair(CWitnessAddress(witnessVersion, ParseHex(witnessProgramHex)), amount));
		}


	}

	string otherNode1, otherNode2;
	GetOtherNodes(node, otherNode1, otherNode2);

	BOOST_CHECK_NO_THROW(r = CallRPC(node, "assetallocationinfo " + guid + " " + fromaddress));
	UniValue balance;
    if(usezdag)
        balance = find_value(r.get_obj(), "balance_zdag");
    else
        balance = find_value(r.get_obj(), "balance");
	CAmount newfromamount;
    try{
        newfromamount  = AssetAmountFromValue(balance, nprecision) - inputamount;
    }
    catch(...){
        newfromamount = 0;
    }
	// "assetallocationsendmany [asset] [addressfrom] ( [{\"address\":\"address\",\"amount\":amount},...] [witness]\n"
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "assetallocationsendmany " + guid + " " + fromaddress + " " + inputs + " " + witness));
    BOOST_CHECK_NO_THROW(r = CallRPC(node, "signrawtransactionwithwallet " + find_value(r.get_obj(), "hex").get_str()));
	string hex_str = find_value(r.get_obj(), "hex").get_str();


	BOOST_CHECK_NO_THROW(r = CallRPC(node, "sendrawtransaction " + hex_str, true, false));
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "paydaycoindecoderawtransaction " + hex_str));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "txtype").get_str(), "assetallocationsend");
	if (!theAssetAllocation.listSendingAllocationAmounts.empty())
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "allocations").get_array().size(), theAssetAllocation.listSendingAllocationAmounts.size());

	BOOST_CHECK_NO_THROW(r = CallRPC(node, "decoderawtransaction " + hex_str + " true"));
	string txid = find_value(r.get_obj(), "txid").get_str();
	if (usezdag) {
		MilliSleep(100);
	}
    else
        GenerateBlocks(1, node);
    if (newfromamount > 0 || usezdag)
    {
    	BOOST_CHECK_NO_THROW(r = CallRPC(node, "assetallocationinfo " + guid + " " + fromaddress));
        if(usezdag)
    	    balance = find_value(r.get_obj(), "balance_zdag");
        else
            balance = find_value(r.get_obj(), "balance");
    	if(newfromamount <= 0)
            BOOST_CHECK_EQUAL(0, newfromamount);
        else
		    BOOST_CHECK_EQUAL(AssetAmountFromValue(balance, nprecision), newfromamount);
    }
    else if(!usezdag){
        BOOST_CHECK_THROW(r = CallRPC(node, "assetallocationinfo " + guid + " " + fromaddress), runtime_error);
	
		for (unsigned int idx = 0; idx < receivers.size(); idx++) {
			const UniValue& receiver = receivers[idx];
			BOOST_CHECK(receiver.isObject());
		
			UniValue receiverObj = receiver.get_obj();
			string strAddress = find_value(receiverObj, "address").get_str();
			
			BOOST_CHECK_NO_THROW(r = CallRPC(node, "listassetindexallocations " + strAddress ));
			BOOST_CHECK(FindAssetGUIDFromAssetIndexResults(r, guid));
		}
		if(newfromamount > 0){
			BOOST_CHECK_NO_THROW(r = CallRPC(node, "listassetindexallocations " + fromaddress ));
			BOOST_CHECK(FindAssetGUIDFromAssetIndexResults(r, guid));
		}	
	}

	return txid;
}
void BurnAssetAllocation(const string& node, const string &guid, const string &address,const string &amount, bool confirm){
    UniValue r;
    try{
        r = CallRPC(node, "assetallocationinfo " + guid + " burn");
    }
    catch(runtime_error&err){
    }
    CAmount beforeBalance = 0;
    if(r.isObject()){
        UniValue balanceB = find_value(r.get_obj(), "balance");
        beforeBalance = AssetAmountFromValue(balanceB, 8);
    }
    
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "assetallocationburn " + guid + " " + address + " " + amount + " 0x931D387731bBbC988B312206c74F77D004D6B84b"));
    BOOST_CHECK_NO_THROW(r = CallRPC(node, "signrawtransactionwithwallet " + find_value(r.get_obj(), "hex").get_str()));
	string hexStr = find_value(r.get_obj(), "hex").get_str();
 
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "sendrawtransaction " + hexStr, true, false));  
    if(confirm){
    	GenerateBlocks(5, "node1");
    	BOOST_CHECK_NO_THROW(r = CallRPC(node, "assetallocationinfo " + guid + " burn"));
    	UniValue balance = find_value(r.get_obj(), "balance");
    	BOOST_CHECK_EQUAL(AssetAmountFromValue(balance, 8), beforeBalance+(0.5 * COIN));
    }
}
void LockAssetAllocation(const string& node, const string &guid, const string &address,const string &txid, const string &index){
    UniValue r;
    string outpointStr = txid+"-"+index;
    BOOST_CHECK_NO_THROW(r = CallRPC(node, "assetallocationlock " + guid + " " + address + " " + txid + " " + index + " ''"));
    BOOST_CHECK_NO_THROW(r = CallRPC(node, "signrawtransactionwithwallet " + find_value(r.get_obj(), "hex").get_str()));
    string hexStr = find_value(r.get_obj(), "hex").get_str();
   
    BOOST_CHECK_NO_THROW(r = CallRPC(node, "sendrawtransaction " + hexStr, true, false));  
    BOOST_CHECK_NO_THROW(r = CallRPC(node, "paydaycoindecoderawtransaction " + hexStr));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "txtype").get_str(), "assetallocationlock");
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "locked_outpoint").get_str() , outpointStr);  

    GenerateBlocks(5, "node1");
    BOOST_CHECK_NO_THROW(r = CallRPC(node, "assetallocationinfo " + guid + " " + address));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "locked_outpoint").get_str() , outpointStr);  
}
string AssetSend(const string& node, const string& guid, const string& inputs, const string& witness, bool completetx, bool bRegtest)
{
	UniValue r;
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "assetinfo " + guid, bRegtest));
	int nprecision = find_value(r.get_obj(), "precision").get_int();
    string fromAddress = find_value(r.get_obj(), "address").get_str();
    
	CAssetAllocation theAssetAllocation;
	UniValue valueTo;
	string inputsTmp = inputs;
	boost::replace_all(inputsTmp, "\\\"", "\"");
	boost::replace_all(inputsTmp, "\"[", "[");
	boost::replace_all(inputsTmp, "]\"", "]");
	BOOST_CHECK(valueTo.read(inputsTmp));
	BOOST_CHECK(valueTo.isArray());
	UniValue receivers = valueTo.get_array();
	CAmount inputamount = 0;
	for (unsigned int idx = 0; idx < receivers.size(); idx++) {
		const UniValue& receiver = receivers[idx];
		BOOST_CHECK(receiver.isObject());


		UniValue receiverObj = receiver.get_obj();
        const CTxDestination &dest = DecodeDestination(find_value(receiverObj, "address").get_str());
        UniValue detail = DescribeAddress(dest);
        string witnessProgramHex = find_value(detail.get_obj(), "witness_program").get_str();
        unsigned char witnessVersion = (unsigned char)find_value(detail.get_obj(), "witness_version").get_int();
		UniValue amountObj = find_value(receiverObj, "amount");
        if (amountObj.isNum()) {
			const CAmount &amount = AssetAmountFromValue(amountObj, nprecision);
			inputamount += amount;
			BOOST_CHECK(amount > 0);
			theAssetAllocation.listSendingAllocationAmounts.push_back(make_pair(CWitnessAddress(witnessVersion, ParseHex(witnessProgramHex)), amount));
		}


	}

	string otherNode1, otherNode2;
	GetOtherNodes(node, otherNode1, otherNode2);
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "assetinfo " + guid, bRegtest ));
	
	UniValue balance = find_value(r.get_obj(), "balance");
	CAmount newfromamount = AssetAmountFromValue(balance, nprecision) - inputamount;
    UniValue totalsupply = find_value(r.get_obj(), "total_supply");
    CAmount fromsupply = AssetAmountFromValue(totalsupply, nprecision);
	// "assetsendmany [asset] ( [{\"address\":\"address\",\"amount\":amount},...] [witness]\n"
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "assetsendmany " + guid + " " + inputs + " " + witness, bRegtest));
    BOOST_CHECK_NO_THROW(r = CallRPC(node, "signrawtransactionwithwallet " + find_value(r.get_obj(), "hex").get_str(), bRegtest));
	string hex_str = find_value(r.get_obj(), "hex").get_str();
	if (completetx) {
    
		BOOST_CHECK_NO_THROW(r = CallRPC(node, "sendrawtransaction " + hex_str, bRegtest, false)); 
		BOOST_CHECK_NO_THROW(r = CallRPC(node, "decoderawtransaction " + hex_str + " true", bRegtest));

		GenerateBlocks(1, node, bRegtest);
		BOOST_CHECK_NO_THROW(r = CallRPC(node, "assetinfo " + guid , bRegtest));
        totalsupply = find_value(r.get_obj(), "total_supply");
		BOOST_CHECK_EQUAL(AssetAmountFromValue(totalsupply, nprecision), fromsupply);
		balance = find_value(r.get_obj(), "balance");
        
		if (newfromamount > 0)
			BOOST_CHECK_EQUAL(AssetAmountFromValue(balance, nprecision), newfromamount);

		if (!otherNode1.empty())
		{
			BOOST_CHECK_NO_THROW(r = CallRPC(otherNode1, "assetinfo " + guid, bRegtest ));
            totalsupply = find_value(r.get_obj(), "total_supply");
			BOOST_CHECK_EQUAL(AssetAmountFromValue(totalsupply, nprecision), fromsupply);
			balance = find_value(r.get_obj(), "balance");
			if (newfromamount > 0)
				BOOST_CHECK_EQUAL(AssetAmountFromValue(balance, nprecision), newfromamount);

		}
		if (!otherNode2.empty())
		{
			BOOST_CHECK_NO_THROW(r = CallRPC(otherNode2, "assetinfo " + guid, bRegtest ));
            totalsupply = find_value(r.get_obj(), "total_supply");
			BOOST_CHECK_EQUAL(AssetAmountFromValue(totalsupply, nprecision), fromsupply);
			balance = find_value(r.get_obj(), "balance");
			if (newfromamount > 0)
				BOOST_CHECK_EQUAL(AssetAmountFromValue(balance, nprecision), newfromamount);

		}
	}
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "paydaycoindecoderawtransaction " + hex_str, bRegtest));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "txtype").get_str(), "assetsend");
	if (!theAssetAllocation.listSendingAllocationAmounts.empty())
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "allocations").get_array().size(), theAssetAllocation.listSendingAllocationAmounts.size());
	
	for (unsigned int idx = 0; idx < receivers.size(); idx++) {
		const UniValue& receiver = receivers[idx];
		BOOST_CHECK(receiver.isObject());
      
		UniValue receiverObj = receiver.get_obj();
        string strAddress = find_value(receiverObj, "address").get_str();
        
		BOOST_CHECK_NO_THROW(r = CallRPC(node, "listassetindexallocations " + strAddress, bRegtest ));
		BOOST_CHECK(FindAssetGUIDFromAssetIndexResults(r, guid));


	}
	if(newfromamount > 0){
		BOOST_CHECK_NO_THROW(r = CallRPC(node, "listassetindexassets " + fromAddress, bRegtest ));
		BOOST_CHECK(FindAssetGUIDFromAssetIndexResults(r, guid));
	}		
	return hex_str;

}

BasicPaydayCoinTestingSetup::BasicPaydayCoinTestingSetup()
{
}
BasicPaydayCoinTestingSetup::~BasicPaydayCoinTestingSetup()
{
}
PaydayCoinTestingSetup::PaydayCoinTestingSetup()
{
}
PaydayCoinTestingSetup::~PaydayCoinTestingSetup()
{
}
PaydayCoinMainNetSetup::PaydayCoinMainNetSetup()
{
	//StartMainNetNodes();
}
PaydayCoinMainNetSetup::~PaydayCoinMainNetSetup()
{
	//StopMainNetNodes();
}
