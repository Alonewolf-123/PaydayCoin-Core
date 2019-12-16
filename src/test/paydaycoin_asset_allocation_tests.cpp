// Copyright (c) 2016-2018 The PaydayCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/test_paydaycoin_services.h>
#include <util/time.h>
#include <rpc/server.h>
#include <services/asset.h>
#include <base58.h>
#include <chainparams.h>
#include <boost/test/unit_test.hpp>
#include <iterator>
#include <key.h>
#include <rpc/util.h>
#include <core_io.h>
#include <services/rpc/assetrpc.h>
using namespace std;
BOOST_GLOBAL_FIXTURE( PaydayCoinTestingSetup );
BOOST_FIXTURE_TEST_SUITE(paydaycoin_asset_allocation_tests, BasicPaydayCoinTestingSetup)

BOOST_AUTO_TEST_CASE(generate_asset_allocation_address_sync)
{
	UniValue r;
	printf("Running generate_asset_allocation_address_sync...\n");
	GenerateBlocks(5);
	string newaddress = GetNewFundedAddress("node1");
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "getnewaddress"));
    string newaddressreceiver = r.get_str();
	string guid = AssetNew("node1", newaddress, "data", "''", "8", "10000", "1000000");

	AssetSend("node1", guid, "\"[{\\\"address\\\":\\\"" + newaddressreceiver + "\\\",\\\"amount\\\":5000}]\"");
	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationinfo " + guid + " " + newaddressreceiver ));
	UniValue balance = find_value(r.get_obj(), "balance");
	BOOST_CHECK_NO_THROW(r = CallRPC("node2", "assetallocationinfo " + guid + " " + newaddressreceiver ));
	balance = find_value(r.get_obj(), "balance");
	StopNode("node2");
	StartNode("node2");
	GenerateBlocks(5, "node2");
	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationinfo " + guid + " " + newaddressreceiver ));
	balance = find_value(r.get_obj(), "balance");
	BOOST_CHECK_NO_THROW(r = CallRPC("node2", "assetallocationinfo " + guid + " " + newaddressreceiver ));
	balance = find_value(r.get_obj(), "balance");

}
BOOST_AUTO_TEST_CASE(generate_asset_allocation_lock)
{
	UniValue r;
	printf("Running generate_asset_allocation_lock...\n");
	GenerateBlocks(5);
	string txid;
	string newaddress1 = GetNewFundedAddress("node2");
	string newaddress = GetNewFundedAddress("node1", txid);

	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "getrawtransaction " + txid + " true"));
	UniValue voutArray = find_value(r.get_obj(), "vout");
	int vout = 0;
	CAmount nAmount = 0;
	for (unsigned int i = 0; i<voutArray.size(); i++) {
		UniValue scriptObj = find_value(voutArray[i].get_obj(), "scriptPubKey").get_obj();
		UniValue addressesArray = find_value(scriptObj, "addresses");
		for (unsigned int j = 0; j<addressesArray.size(); j++) {
			if (addressesArray[j].get_str() == newaddress) {
				vout = i;
				nAmount = AmountFromValue(find_value(voutArray[i].get_obj(), "value"));
			}
		}
	}
	// for fees;
	nAmount -= 2000000;
	BOOST_CHECK(nAmount > 0);
	string strAmount = ValueFromAmount(nAmount).write();
	string voutstr = itostr(vout);
	// lock outpoint so other txs can't spend through wallet auto-selection 
	BOOST_CHECK_NO_THROW(CallRPC("node1", "lockunspent false \"[{\\\"txid\\\":\\\"" + txid + "\\\",\\\"vout\\\":" + voutstr + "}]\""));


	string guid = AssetNew("node1", newaddress, "data", "''", "8", "1", "100000");

	AssetSend("node1", guid, "\"[{\\\"address\\\":\\\"" + newaddress + "\\\",\\\"amount\\\":1}]\"");
	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationinfo " + guid + " " + newaddress));
	UniValue balance = find_value(r.get_obj(), "balance");
	BOOST_CHECK_EQUAL(AssetAmountFromValue(balance, 8), 1 * COIN);

	LockAssetAllocation("node1", guid, newaddress, txid, voutstr);

    // unlock now to test spending
    BOOST_CHECK_NO_THROW(CallRPC("node1", "lockunspent true \"[{\\\"txid\\\":\\\"" + txid + "\\\",\\\"vout\\\":" + voutstr + "}]\""));
    // cannot spend as normal through sendrawtransaction
    BOOST_CHECK_NO_THROW(r = CallRPC("node1", "createrawtransaction \"[{\\\"txid\\\":\\\"" + txid + "\\\",\\\"vout\\\":" + voutstr + "}]\" \"[{\\\"" + newaddress1 + "\\\":" + strAmount + "}]\"", true, false));

    string rawTx = r.get_str();
    rawTx.erase(std::remove(rawTx.begin(), rawTx.end(), '\n'), rawTx.end());
    BOOST_CHECK_NO_THROW(r = CallRPC("node1", "signrawtransactionwithwallet " + rawTx));

    string hex_str = find_value(r.get_obj(), "hex").get_str();

    BOOST_CHECK_NO_THROW(r = CallRPC("node1", "sendrawtransaction " + hex_str, true, false));
    string res = r.get_str();
    res.erase(std::remove(res.begin(), res.end(), '\n'), res.end());
    BOOST_CHECK(res.empty());
    string txid0 = AssetAllocationTransfer(false, "node1", guid, newaddress, "\"[{\\\"address\\\":\\\"" + newaddress1 + "\\\",\\\"amount\\\":0.11}]\"");
    BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationinfo " + guid + " " + newaddress1));
	balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(AssetAmountFromValue(balance, 8), 0.11 * COIN);
    BOOST_CHECK_NO_THROW(r = CallRPC("node1", "getrawtransaction " + txid0 + " true"));
	UniValue vinArray = find_value(r.get_obj(), "vin");
	bool found = false;
	for (unsigned int i = 0; i<vinArray.size(); i++) {
		if (find_value(vinArray[i].get_obj(), "txid").get_str() == txid && find_value(vinArray[i].get_obj(), "vout").get_int() == vout) {
			found = true;
			break;
		}
	}
	BOOST_CHECK(found);

}
BOOST_AUTO_TEST_CASE(generate_asset_allocation_send_address)
{
	UniValue r;
	printf("Running generate_asset_allocation_send_address...\n");
	GenerateBlocks(5);
    GenerateBlocks(5, "node2");
	string newaddress1 = GetNewFundedAddress("node1");
    CallRPC("node2", "sendtoaddress " + newaddress1 + " 1", true, false);
    CallRPC("node2", "sendtoaddress " + newaddress1 + " 1", true, false);
	GenerateBlocks(5);
	GenerateBlocks(5, "node2");
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "getnewaddress"));
    string newaddress2 = r.get_str();
        
	string guid = AssetNew("node1", newaddress1, "data","''", "8", "1", "100000");

	AssetSend("node1", guid, "\"[{\\\"address\\\":\\\"" + newaddress1 + "\\\",\\\"amount\\\":1}]\"");
	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationinfo " + guid + " " + newaddress1 ));
	UniValue balance = find_value(r.get_obj(), "balance");
	BOOST_CHECK_EQUAL(AssetAmountFromValue(balance, 8), 1 * COIN);

	string txid0 = AssetAllocationTransfer(false, "node1", guid, newaddress1, "\"[{\\\"address\\\":\\\"" + newaddress2 + "\\\",\\\"amount\\\":0.11}]\"");
	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationinfo " + guid + " " + newaddress2 ));
	balance = find_value(r.get_obj(), "balance");
	BOOST_CHECK_EQUAL(AssetAmountFromValue(balance, 8), 0.11 * COIN);

	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationsenderstatus " + guid + " " + newaddress1 + " " + txid0));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);

	// send using zdag
	string txid1 = AssetAllocationTransfer(true, "node1", guid, newaddress1, "\"[{\\\"address\\\":\\\"" + newaddress2 + "\\\",\\\"amount\\\":0.12}]\"");
	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationinfo " + guid + " " + newaddress2));
	balance = find_value(r.get_obj(), "balance_zdag");
	BOOST_CHECK_EQUAL(AssetAmountFromValue(balance, 8), 0.23 * COIN);

	// non zdag cannot be found since it was already mined, but ends up briefly in conflict state because sender is conflicted
	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationsenderstatus " + guid + " " + newaddress1 + " " + txid0));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_MINOR_CONFLICT);

	// first tx should have to wait 1 sec for good status
	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationsenderstatus " + guid + " " + newaddress1 + " " + txid1));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_MINOR_CONFLICT);

	// check just sender
	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationsenderstatus " + guid + " " + newaddress1 + " ''"));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_MINOR_CONFLICT);

	// wait for 1 second as required by unit test
	MilliSleep(1000);
	// second send
	string txid2 = AssetAllocationTransfer(true, "node1", guid, newaddress1, "\"[{\\\"address\\\":\\\"" + newaddress2 + "\\\",\\\"amount\\\":0.13}]\"");
	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationinfo " + guid + " " + newaddress2 ));
	balance = find_value(r.get_obj(), "balance_zdag");
	BOOST_CHECK_EQUAL(AssetAmountFromValue(balance, 8), 0.36 * COIN);

	// sender is conflicted so txid0 is conflicted by extension even if its not found
	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationsenderstatus " + guid + " " + newaddress1 + " " + txid0));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_MINOR_CONFLICT);

	// first ones now OK because it was found explicitly
	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationsenderstatus " + guid + " " + newaddress1 + " " + txid1));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_STATUS_OK);

	// second one hasn't waited enough time yet
	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationsenderstatus " + guid + " " + newaddress1 + " " + txid2));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_MINOR_CONFLICT);

	// check just sender
	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationsenderstatus " + guid + " " + newaddress1 + " ''"));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_MINOR_CONFLICT);

	// wait for 1.5 second to clear minor warning status
	MilliSleep(1500);
	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationsenderstatus " + guid + " " + newaddress1 + " " + txid0));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);

	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationsenderstatus " + guid + " " + newaddress1 + " " + txid1));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_STATUS_OK);


	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationsenderstatus " + guid + " " + newaddress1 + " " + txid2));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_STATUS_OK);


	// check just sender as well
	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationsenderstatus " + guid + " " + newaddress1 + " ''"));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_STATUS_OK);


	// after pow, should get not found on all of them
	GenerateBlocks(1);

	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationsenderstatus " + guid + " " + newaddress1 + " " + txid0));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);

	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationsenderstatus " + guid + " " + newaddress1 + " " + txid1));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);


	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationsenderstatus " + guid + " " + newaddress1 + " " + txid2));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);

	// check just sender as well
	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "assetallocationsenderstatus " + guid + " " + newaddress1 + " ''"));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);
    
}
BOOST_AUTO_TEST_CASE(generate_asset_consistency_check)
{
	UniValue assetInvalidatedResults, assetNowResults, assetValidatedResults;
	UniValue assetAllocationsInvalidatedResults, assetAllocationsNowResults, assetAllocationsValidatedResults;
	UniValue r;
	printf("Running generate_asset_consistency_check...\n");
	GenerateBlocks(5);

	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "getblockcount", true, false));
	string strBeforeBlockCount = r.write();
    // remove new line and string terminator
    strBeforeBlockCount = strBeforeBlockCount.substr(1, strBeforeBlockCount.size() - 4);
    
	// first check around disconnect/connect by invalidating and revalidating an early block
	BOOST_CHECK_NO_THROW(assetNowResults = CallRPC("node1", "listassets " + itostr(INT_MAX) + " 0"));
	BOOST_CHECK_NO_THROW(assetAllocationsNowResults = CallRPC("node1", "listassetallocations " + itostr(INT_MAX) + " 0"));

	// disconnect back to block 10 where no assets would have existed
	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "getblockhash 10", true, false));
	string blockHashInvalidated = r.get_str();
	BOOST_CHECK_NO_THROW(CallRPC("node1", "invalidateblock " + blockHashInvalidated, true, false));
	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "getblockcount", true, false));
    string strAfterBlockCount = r.write();
    strAfterBlockCount = strAfterBlockCount.substr(1, strAfterBlockCount.size() - 4);
	BOOST_CHECK_EQUAL(strAfterBlockCount, "9");

	UniValue emptyResult(UniValue::VARR);
	BOOST_CHECK_NO_THROW(assetInvalidatedResults = CallRPC("node1", "listassets " + itostr(INT_MAX) + " 0"));
	BOOST_CHECK_EQUAL(assetInvalidatedResults.write(), emptyResult.write());
	BOOST_CHECK_NO_THROW(assetAllocationsInvalidatedResults = CallRPC("node1", "listassetallocations " + itostr(INT_MAX) + " 0"));
	BOOST_CHECK_EQUAL(assetAllocationsInvalidatedResults.write(), emptyResult.write());

	// reconnect to tip and ensure block count matches
	BOOST_CHECK_NO_THROW(CallRPC("node1", "reconsiderblock " + blockHashInvalidated, true, false));
	BOOST_CHECK_NO_THROW(r = CallRPC("node1", "getblockcount", true, false));
    string strRevalidatedBlockCount = r.write();
    strRevalidatedBlockCount = strRevalidatedBlockCount.substr(1, strRevalidatedBlockCount.size() - 4);
	BOOST_CHECK_EQUAL(strRevalidatedBlockCount, strBeforeBlockCount);

	BOOST_CHECK_NO_THROW(assetValidatedResults = CallRPC("node1", "listassets " + itostr(INT_MAX) + " 0"));
	BOOST_CHECK_EQUAL(assetValidatedResults.write(), assetNowResults.write());
	BOOST_CHECK_NO_THROW(assetAllocationsValidatedResults = CallRPC("node1", "listassetallocations " + itostr(INT_MAX) + " 0"));
	BOOST_CHECK_EQUAL(assetAllocationsValidatedResults.write(), assetAllocationsNowResults.write());	
	// try to check after reindex
	StopNode("node1");
	StartNode("node1", true, "", true);

	BOOST_CHECK_NO_THROW(assetValidatedResults = CallRPC("node1", "listassets " + itostr(INT_MAX) + " 0"));
	BOOST_CHECK_EQUAL(assetValidatedResults.write(), assetNowResults.write());
	BOOST_CHECK_NO_THROW(assetAllocationsValidatedResults = CallRPC("node1", "listassetallocations " + itostr(INT_MAX) + " 0"));
	BOOST_CHECK_EQUAL(assetAllocationsValidatedResults.write(), assetAllocationsNowResults.write());	
	ECC_Stop();
}
BOOST_AUTO_TEST_SUITE_END ()
