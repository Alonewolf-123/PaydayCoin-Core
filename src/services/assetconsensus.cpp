﻿// Copyright (c) 2013-2019 The PaydayCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <services/assetconsensus.h>
#include <validation.h>
#include <chainparams.h>
#include <consensus/validation.h>
#include <ethereum/ethereum.h>
#include <ethereum/address.h>
#include <ethereum/common.h>
#include <ethereum/commondata.h>
#include <boost/thread.hpp>
#include <services/rpc/assetrpc.h>
extern AssetBalanceMap mempoolMapAssetBalances;
extern ArrivalTimesMapImpl arrivalTimesMap;
std::unique_ptr<CBlockIndexDB> pblockindexdb;
std::unique_ptr<CLockedOutpointsDB> plockedoutpointsdb;
std::unique_ptr<CEthereumTxRootsDB> pethereumtxrootsdb;
std::unique_ptr<CEthereumMintedTxDB> pethereumtxmintdb;
extern std::unordered_set<std::string> assetAllocationConflicts;
extern CCriticalSection cs_assetallocation;
extern CCriticalSection cs_assetallocationarrival;
using namespace std;
bool DisconnectPaydayCoinTransaction(const CTransaction& tx, const CBlockIndex* pindex, CCoinsViewCache& view, AssetMap &mapAssets, AssetAllocationMap &mapAssetAllocations, EthereumMintTxVec &vecMintKeys)
{
    if(tx.IsCoinBase())
        return true;

    if (!IsPaydayCoinTx(tx.nVersion))
        return true;
 
    if(tx.nVersion == PAYDAYCOIN_TX_VERSION_ASSET_ALLOCATION_MINT){
        if(!DisconnectMintAsset(tx, mapAssetAllocations, vecMintKeys))
            return false;       
    }
    else if(tx.nVersion == PAYDAYCOIN_TX_VERSION_MINT){
        if(!DisconnectMint(tx, vecMintKeys))
            return false;       
    }    
    else{
        if (IsAssetAllocationTx(tx.nVersion))
        {
            if(!DisconnectAssetAllocation(tx, mapAssetAllocations))
                return false;       
        }
        else if (IsAssetTx(tx.nVersion))
        {
            if (tx.nVersion == PAYDAYCOIN_TX_VERSION_ASSET_SEND) {
                if(!DisconnectAssetSend(tx, mapAssets, mapAssetAllocations))
                    return false;
            } else if (tx.nVersion == PAYDAYCOIN_TX_VERSION_ASSET_UPDATE) {  
                if(!DisconnectAssetUpdate(tx, mapAssets))
                    return false;
            }
            else if(tx.nVersion == PAYDAYCOIN_TX_VERSION_ASSET_TRANSFER) {  
                 if(!DisconnectAssetTransfer(tx, mapAssets))
                    return false;
            }
            else if (tx.nVersion == PAYDAYCOIN_TX_VERSION_ASSET_ACTIVATE) {
                if(!DisconnectAssetActivate(tx, mapAssets))
                    return false;
            }     
        }
    }   
    return true;       
}

bool CheckPaydayCoinMint(const bool ibd, const CTransaction& tx, std::string& errorMessage, const bool &fJustCheck, const bool& bSanity, const bool& bMiner, const int& nHeight, const uint256& blockhash, AssetMap& mapAssets, AssetAllocationMap &mapAssetAllocations, EthereumMintTxVec &vecMintKeys, bool &bTxRootError)
{
    static bool bGethTestnet = gArgs.GetBoolArg("-gethtestnet", false);
    // unserialize mint object from txn, check for valid
    CMintPaydayCoin mintPaydayCoin(tx);
    CAsset dbAsset;
    if(mintPaydayCoin.IsNull())
    {
        errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Cannot unserialize data inside of this transaction relating to an paydaycoinmint");
        return false;
    }
    if(tx.nVersion == PAYDAYCOIN_TX_VERSION_MINT && !mintPaydayCoin.assetAllocationTuple.IsNull())
    {
        errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Tried to mint PaydayCoin but asset information was present");
        return false;
    }  
    if(tx.nVersion == PAYDAYCOIN_TX_VERSION_ASSET_ALLOCATION_MINT && mintPaydayCoin.assetAllocationTuple.IsNull())
    {
        errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Tried to mint asset but asset information was not present");
        return false;
    } 
    if(tx.nVersion == PAYDAYCOIN_TX_VERSION_ASSET_ALLOCATION_MINT && !GetAsset(mintPaydayCoin.assetAllocationTuple.nAsset, dbAsset)) 
    {
        errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Failed to read from asset DB");
        return false;
    }
    
   
    
    // do this check only when not in IBD (initial block download) or litemode
    // if we are starting up and verifying the db also skip this check as fLoaded will be false until startup sequence is complete
    EthereumTxRoot txRootDB;
   
    uint32_t cutoffHeight;
    const bool &ethTxRootShouldExist = !ibd && !fLiteMode && fLoaded && fGethSynced;
    // validate that the block passed is committed to by the tx root he also passes in, then validate the spv proof to the tx root below  
    // the cutoff to keep txroots is 120k blocks and the cutoff to get approved is 40k blocks. If we are syncing after being offline for a while it should still validate up to 120k worth of txroots
    if(!pethereumtxrootsdb || !pethereumtxrootsdb->ReadTxRoots(mintPaydayCoin.nBlockNumber, txRootDB)){
        if(ethTxRootShouldExist){
            errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Missing transaction root for SPV proof at Ethereum block: ") + itostr(mintPaydayCoin.nBlockNumber);
            bTxRootError = true;
            return false;
        }
    }  
    if(ethTxRootShouldExist){
        LOCK(cs_ethsyncheight);
        // cutoff is ~1 week of blocks is about 40K blocks
        cutoffHeight = fGethSyncHeight - MAX_ETHEREUM_TX_ROOTS;
        if(fGethSyncHeight >= MAX_ETHEREUM_TX_ROOTS && mintPaydayCoin.nBlockNumber <= (uint32_t)cutoffHeight) {
            errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("The block height is too old, your SPV proof is invalid. SPV Proof must be done within 40000 blocks of the burn transaction on Ethereum blockchain");
            bTxRootError = true;
            return false;
        } 
        
        // ensure that we wait at least ETHEREUM_CONFIRMS_REQUIRED blocks (~1 hour) before we are allowed process this mint transaction  
        // also ensure sanity test that the current height that our node thinks Eth is on isn't less than the requested block for spv proof
        if(fGethCurrentHeight <  mintPaydayCoin.nBlockNumber || fGethSyncHeight <= 0 || (fGethSyncHeight - mintPaydayCoin.nBlockNumber < (bGethTestnet? 10: ETHEREUM_CONFIRMS_REQUIRED))){
            errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Not enough confirmations on Ethereum to process this mint transaction. Blocks required: ") + itostr(ETHEREUM_CONFIRMS_REQUIRED - (fGethSyncHeight - mintPaydayCoin.nBlockNumber));
            bTxRootError = true;
            return false;
        } 
    }
    
     // check transaction receipt validity

    const std::vector<unsigned char> &vchReceiptParentNodes = mintPaydayCoin.vchReceiptParentNodes;
    dev::RLP rlpReceiptParentNodes(&vchReceiptParentNodes);

    const std::vector<unsigned char> &vchReceiptValue = mintPaydayCoin.vchReceiptValue;
    dev::RLP rlpReceiptValue(&vchReceiptValue);
    
    if (!rlpReceiptValue.isList()){
        errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Transaction Receipt RLP must be a list");
        return false;
    }
    if (rlpReceiptValue.itemCount() != 4){
        errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Transaction Receipt RLP invalid item count");
        return false;
    } 
    const uint64_t &nStatus = rlpReceiptValue[0].toInt<uint64_t>(dev::RLP::VeryStrict);
    if (nStatus != 1){
        errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Transaction Receipt showing invalid status, make sure transaction was accepted by Ethereum EVM");
        return false;
    } 

     
    // check transaction spv proofs
    dev::RLP rlpTxRoot(&mintPaydayCoin.vchTxRoot);
    dev::RLP rlpReceiptRoot(&mintPaydayCoin.vchReceiptRoot);

    if(!txRootDB.vchTxRoot.empty() && rlpTxRoot.toBytes(dev::RLP::VeryStrict) != txRootDB.vchTxRoot){
        errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Mismatching Tx Roots");
        bTxRootError = true; // roots can be wrong because geth may not give us affected headers post re-org
        return false;
    }

    if(!txRootDB.vchReceiptRoot.empty() && rlpReceiptRoot.toBytes(dev::RLP::VeryStrict) != txRootDB.vchReceiptRoot){
        errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Mismatching Receipt Roots");
        bTxRootError = true; // roots can be wrong because geth may not give us affected headers post re-org
        return false;
    } 
    
    
    const std::vector<unsigned char> &vchTxParentNodes = mintPaydayCoin.vchTxParentNodes;
    dev::RLP rlpTxParentNodes(&vchTxParentNodes);
    const std::vector<unsigned char> &vchTxValue = mintPaydayCoin.vchTxValue;
    dev::RLP rlpTxValue(&vchTxValue);
    const std::vector<unsigned char> &vchTxPath = mintPaydayCoin.vchTxPath;
    dev::RLP rlpTxPath(&vchTxPath);
    const uint32_t &nPath = rlpTxPath.toInt<uint32_t>(dev::RLP::VeryStrict);
    
    // ensure eth tx not already spent
    const std::pair<uint64_t, uint32_t> &ethKey = std::make_pair(mintPaydayCoin.nBlockNumber, nPath);
    if(pethereumtxmintdb->ExistsKey(ethKey)){
        errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("This block number+transaction index pair has already been used to mint");
        return false;
    } 
    // add the key to flush to db later
    vecMintKeys.emplace_back(ethKey);
    
    // verify receipt proof
    if(!VerifyProof(&vchTxPath, rlpReceiptValue, rlpReceiptParentNodes, rlpReceiptRoot)){
        errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Could not verify ethereum transaction receipt using SPV proof");
        return false;
    } 
    // verify transaction proof
    if(!VerifyProof(&vchTxPath, rlpTxValue, rlpTxParentNodes, rlpTxRoot)){
        errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Could not verify ethereum transaction using SPV proof");
        return false;
    } 
    if (!rlpTxValue.isList()){
        errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Transaction RLP must be a list");
        return false;
    }
    if (rlpTxValue.itemCount() < 6){
        errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Transaction RLP invalid item count");
        return false;
    }        
    if (!rlpTxValue[5].isData()){
        errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Transaction data RLP must be an array");
        return false;
    }        
    if (rlpTxValue[3].isEmpty()){
        errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Invalid transaction receiver");
        return false;
    }                       
    const dev::Address &address160 = rlpTxValue[3].toHash<dev::Address>(dev::RLP::VeryStrict);
    if(tx.nVersion == PAYDAYCOIN_TX_VERSION_MINT && Params().GetConsensus().vchPDAYXContract != address160.asBytes()){
        errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Receiver not the expected PDAYX contract address");
        return false;
    }
    else if(tx.nVersion == PAYDAYCOIN_TX_VERSION_ASSET_ALLOCATION_MINT && dbAsset.vchContract != address160.asBytes()){
        errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Receiver not the expected PDAYX contract address");
        return false;
    }
    
    CAmount outputAmount;
    uint32_t nAsset = 0;
    const std::vector<unsigned char> &rlpBytes = rlpTxValue[5].toBytes(dev::RLP::VeryStrict);
    CWitnessAddress witnessAddress;
    if(!parseEthMethodInputData(Params().GetConsensus().vchPDAYXBurnMethodSignature, rlpBytes, outputAmount, nAsset, witnessAddress)){
        errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Could not parse and validate transaction data");
        return false;
    }
    if(tx.nVersion == PAYDAYCOIN_TX_VERSION_MINT) {
        int witnessversion;
        std::vector<unsigned char> witnessprogram;
        if (tx.vout[0].scriptPubKey.IsWitnessProgram(witnessversion, witnessprogram)){
            if(!fUnitTest && (witnessAddress.vchWitnessProgram != witnessprogram || witnessAddress.nVersion != (unsigned char)witnessversion)){
                errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Witness address does not match extracted witness address from burn transaction");
                return false;
            }
        }
        else{
            errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Witness program not detected in the first output of the mint transaction");
            return false;
        } 
       
    }
    else if(tx.nVersion == PAYDAYCOIN_TX_VERSION_ASSET_ALLOCATION_MINT)
    {
        if(witnessAddress != mintPaydayCoin.assetAllocationTuple.witnessAddress){
            errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Minting address does not match address passed into burn function");
            return false;
        }
    }    
    if(tx.nVersion == PAYDAYCOIN_TX_VERSION_MINT && nAsset != 0){
        errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Cannot mint an asset in a paydaycoin mint operation");
        return false;
    }
    else if(tx.nVersion == PAYDAYCOIN_TX_VERSION_ASSET_ALLOCATION_MINT && nAsset != dbAsset.nAsset){
        errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Invalid asset being minted, does not match asset GUID encoded in transaction data");
        return false;
    }
    if(tx.nVersion == PAYDAYCOIN_TX_VERSION_MINT && outputAmount != tx.vout[0].nValue){
        errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Burn amount must match mint amount");
        return false;
    }
    else if(tx.nVersion == PAYDAYCOIN_TX_VERSION_ASSET_ALLOCATION_MINT && outputAmount != mintPaydayCoin.nValueAsset){
        errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Burn amount must match asset mint amount");
        return false;
    }  
    if(tx.nVersion == PAYDAYCOIN_TX_VERSION_ASSET_ALLOCATION_MINT){
        if(outputAmount <= 0){
            errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Burn amount must be positive");
            return false;
        }  
    
        const std::string &receiverTupleStr = mintPaydayCoin.assetAllocationTuple.ToString();
        auto result1 = mapAssetAllocations.emplace(std::piecewise_construct,  std::forward_as_tuple(receiverTupleStr),  std::forward_as_tuple(std::move(emptyAllocation)));
        auto mapAssetAllocation = result1.first;
        const bool &mapAssetAllocationNotFound = result1.second;
        if(mapAssetAllocationNotFound){
            CAssetAllocation receiverAllocation;
            GetAssetAllocation(mintPaydayCoin.assetAllocationTuple, receiverAllocation);
            if (receiverAllocation.assetAllocationTuple.IsNull()) {           
                receiverAllocation.assetAllocationTuple.nAsset = std::move(mintPaydayCoin.assetAllocationTuple.nAsset);
                receiverAllocation.assetAllocationTuple.witnessAddress = std::move(mintPaydayCoin.assetAllocationTuple.witnessAddress);
            }
            mapAssetAllocation->second = std::move(receiverAllocation);             
        }
        CAssetAllocation& storedReceiverAllocationRef = mapAssetAllocation->second;
        if (!AssetRange(mintPaydayCoin.nValueAsset))
        {
            errorMessage = "PAYDAYCOIN_CONSENSUS_ERROR: ERRCODE: 2029 - " + _("Amount out of money range");
            return false;
        }

        // update balances  
        storedReceiverAllocationRef.nBalance += mintPaydayCoin.nValueAsset; 
        if(!fJustCheck && !bSanity && !bMiner)     
            passetallocationdb->WriteMintIndex(tx, mintPaydayCoin, nHeight, blockhash);         
                                       
    }
    return true;
}
bool CheckPaydayCoinInputs(const bool ibd, const CTransaction& tx, CValidationState& state, const CCoinsViewCache &inputs, bool fJustCheck, bool &bOverflow, int nHeight, const CBlock& block, const bool &bSanity, const bool &bMiner, std::vector<uint256> &txsToRemove)
{
    AssetAllocationMap mapAssetAllocations;
    AssetMap mapAssets;
    EthereumMintTxVec vecMintKeys;
	std::vector<COutPoint> vecLockedOutpoints;
    if (nHeight == 0)
        nHeight = ::ChainActive().Height()+1;
    std::string errorMessage;
    bool good = true;
    bool bTxRootError = false;
    bOverflow=false;
    if (block.vtx.empty()) {  
        if(tx.IsCoinBase())
            return true;
		if (!IsPaydayCoinTx(tx.nVersion))
			return true;
        if (IsAssetAllocationTx(tx.nVersion))
        {
            errorMessage.clear();
            good = CheckAssetAllocationInputs(tx, inputs, fJustCheck, nHeight, uint256(), mapAssetAllocations, vecLockedOutpoints, errorMessage, bOverflow, bSanity, bMiner);
        }
        else if (IsAssetTx(tx.nVersion))
        {
            errorMessage.clear();
            good = CheckAssetInputs(tx, inputs, fJustCheck, nHeight, uint256(), mapAssets, mapAssetAllocations, errorMessage, bSanity, bMiner);
        }
        else if(IsPaydayCoinMintTx(tx.nVersion)) 
        {
            if(nHeight <= Params().GetConsensus().nBridgeStartBlock){
                errorMessage = "Bridge is disabled until blockheight 51000";
                good = false;
            }
            else{
                errorMessage.clear();
                good = CheckPaydayCoinMint(ibd, tx, errorMessage, fJustCheck, bSanity, bMiner, nHeight, uint256(), mapAssets, mapAssetAllocations, vecMintKeys, bTxRootError);
            }
        }
  
        if (!good || !errorMessage.empty()){
            if(bTxRootError)
                return state.Error(errorMessage);
            else
                return state.Invalid(ValidationInvalidReason::TX_MEMPOOL_POLICY, false, REJECT_INVALID, errorMessage);
        }
      
        return true;
    }
    else if (!block.vtx.empty()) {
        const uint256& blockHash = block.GetHash();
        std::vector<std::pair<uint256, uint256> > blockIndex;
        for (unsigned int i = 0; i < block.vtx.size(); i++)
        {

            good = true;
            const CTransaction &tx = *(block.vtx[i]);    
            if(!bMiner && !fJustCheck && !bSanity){
                const uint256& txHash = tx.GetHash(); 
                blockIndex.push_back(std::make_pair(txHash, blockHash));
            }  
            if(tx.IsCoinBase()) 
                continue;

            if(!IsPaydayCoinTx(tx.nVersion))
                continue;      
            if (IsAssetAllocationTx(tx.nVersion))
            {
                errorMessage.clear();
                // fJustCheck inplace of bSanity to preserve global structures from being changed during test calls, fJustCheck is actually passed in as false because we want to check in PoW mode
                good = CheckAssetAllocationInputs(tx, inputs, false, nHeight, blockHash, mapAssetAllocations, vecLockedOutpoints, errorMessage, bOverflow, fJustCheck, bMiner);

            }
            else if (IsAssetTx(tx.nVersion))
            {
                errorMessage.clear();
                good = CheckAssetInputs(tx, inputs, false, nHeight, blockHash, mapAssets, mapAssetAllocations, errorMessage, fJustCheck, bMiner);
            } 
            else if(IsPaydayCoinMintTx(tx.nVersion))
            {
                if(nHeight <= Params().GetConsensus().nBridgeStartBlock){
                    errorMessage = "Bridge is disabled until blockheight 51000";
                    good = false;
                }
                else{
                    errorMessage.clear();
                    good = CheckPaydayCoinMint(ibd, tx, errorMessage, false, fJustCheck, bMiner, nHeight, blockHash, mapAssets, mapAssetAllocations, vecMintKeys, bTxRootError);
                }
            }
             
                        
            if (!good)
            {
                if (!errorMessage.empty()) {
                    // if validation fails we should not include this transaction in a block
                    if(bMiner){
                        good = true;
                        errorMessage.clear();
                        txsToRemove.push_back(tx.GetHash());
                        continue;
                    }
                }
                
            } 
        }
                        
        if(!bSanity && !fJustCheck){
            if(!bMiner && pblockindexdb){
                if(!pblockindexdb->FlushWrite(blockIndex) || !passetallocationdb->Flush(mapAssetAllocations) || !passetdb->Flush(mapAssets) || !plockedoutpointsdb->FlushWrite(vecLockedOutpoints) || !pethereumtxmintdb->FlushWrite(vecMintKeys)){
                    good = false;
                    errorMessage = "Error flushing to asset dbs";
                }
            }
        }        
        if (!good || !errorMessage.empty()){
            if(bTxRootError)
                return state.Error(errorMessage);
            else
                return state.Invalid(bOverflow? ValidationInvalidReason::TX_CONFLICT: ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, errorMessage);
        }
    }
    return true;
}

void ResyncAssetAllocationStates(){ 
    int count = 0;
     {
        
        vector<string> vecToRemoveMempoolBalances;
        LOCK2(cs_main, mempool.cs);
        LOCK(cs_assetallocation);
        LOCK(cs_assetallocationarrival);
        for (auto&indexObj : mempoolMapAssetBalances) {
            vector<uint256> vecToRemoveArrivalTimes;
            const string& strSenderTuple = indexObj.first;
            // if no arrival time for this mempool balance, remove it
            auto arrivalTimes = arrivalTimesMap.find(strSenderTuple);
            if(arrivalTimes == arrivalTimesMap.end()){
                vecToRemoveMempoolBalances.push_back(strSenderTuple);
                continue;
            }
            for(auto& arrivalTime: arrivalTimes->second){
                const uint256& txHash = arrivalTime.first;
                const CTransactionRef txRef = mempool.get(txHash);
                // if mempool doesn't have txid then remove from both arrivalTime and mempool balances
                if (!txRef){
                    vecToRemoveArrivalTimes.push_back(txHash);
                }
                else if(!arrivalTime.first.IsNull() && ((::ChainActive().Tip()->GetMedianTimePast()*1000) - arrivalTime.second) > 1800000){
                    vecToRemoveArrivalTimes.push_back(txHash);
                }
            }
            // if we are removing everything from arrivalTime map then might as well remove it from parent altogether
            if(vecToRemoveArrivalTimes.size() >= arrivalTimes->second.size()){
                arrivalTimesMap.erase(strSenderTuple);
                vecToRemoveMempoolBalances.push_back(strSenderTuple);
            } 
            // otherwise remove the individual txids
            else{
                for(auto &removeTxHash: vecToRemoveArrivalTimes){
                    arrivalTimes->second.erase(removeTxHash);
                }
            }         
        }
        count+=vecToRemoveMempoolBalances.size();
        for(auto& senderTuple: vecToRemoveMempoolBalances){
            mempoolMapAssetBalances.erase(senderTuple);
            // also remove from assetAllocationConflicts
            unordered_set<string>::const_iterator it = assetAllocationConflicts.find(senderTuple);
            if (it != assetAllocationConflicts.end()) {
                assetAllocationConflicts.erase(it);
            }
        }       
    }   
    if(count > 0)
        LogPrint(BCLog::PDAY,"removeExpiredMempoolBalances removed %d expired asset allocation transactions from mempool balances\n", count);

}
bool ResetAssetAllocation(const string &senderStr, const uint256 &txHash, const bool &bMiner, const bool& bCheckExpiryOnly) {


    bool removeAllConflicts = true;
    if(!bMiner){
        {
            LOCK(cs_assetallocationarrival);
            // remove the conflict once we revert since it is assumed to be resolved on POW
            auto arrivalTimes = arrivalTimesMap.find(senderStr);
            
            if(arrivalTimes != arrivalTimesMap.end()){
                // remove only if all arrival times are either expired (30 mins) or no more zdag transactions left for this sender
                for(auto& arrivalTime: arrivalTimes->second){
                    // ensure mempool has the tx and its less than 30 mins old
                    if(bCheckExpiryOnly && !mempool.get(arrivalTime.first))
                        continue;
                    if(!arrivalTime.first.IsNull() && ((::ChainActive().Tip()->GetMedianTimePast()*1000) - arrivalTime.second) <= 1800000){
                        removeAllConflicts = false;
                        break;
                    }
                }
            }
            if(removeAllConflicts){
                if(arrivalTimes != arrivalTimesMap.end())
                    arrivalTimesMap.erase(arrivalTimes);
                unordered_set<string>::const_iterator it = assetAllocationConflicts.find(senderStr);
                if (it != assetAllocationConflicts.end()) {
                    assetAllocationConflicts.erase(it);
                }   
            }
            else if(!bCheckExpiryOnly){
                arrivalTimes->second.erase(txHash);
                if(arrivalTimes->second.size() <= 0)
                    removeAllConflicts = true;
            }
        }
        if(removeAllConflicts)
        {
            LOCK(cs_assetallocation);
            mempoolMapAssetBalances.erase(senderStr);
            if(!bCheckExpiryOnly){
                unordered_set<string>::const_iterator it = assetAllocationConflicts.find(senderStr);
                if (it != assetAllocationConflicts.end()) {
                    assetAllocationConflicts.erase(it);
                }  
            }
        }
    }
    

    return removeAllConflicts;
    
}
bool DisconnectMint(const CTransaction &tx,  EthereumMintTxVec &vecMintKeys){
    CMintPaydayCoin mintPaydayCoin(tx);
    if(mintPaydayCoin.IsNull())
    {
        LogPrint(BCLog::PDAY,"DisconnectMint: Cannot unserialize data inside of this transaction relating to an paydaycoinmint\n");
        return false;
    }   
    const std::vector<unsigned char> &vchTxPath = mintPaydayCoin.vchTxPath;
    dev::RLP rlpTxPath(&vchTxPath);
    const uint32_t &nPath = rlpTxPath.toInt<uint32_t>(dev::RLP::VeryStrict);
    // remove eth spend tx from our internal db
    const std::pair<uint64_t, uint32_t> &ethKey = std::make_pair(mintPaydayCoin.nBlockNumber, nPath);
    vecMintKeys.emplace_back(ethKey);  
    return true; 
}
bool DisconnectMintAsset(const CTransaction &tx, AssetAllocationMap &mapAssetAllocations, EthereumMintTxVec &vecMintKeys){
    CMintPaydayCoin mintPaydayCoin(tx);
    if(mintPaydayCoin.IsNull())
    {
        LogPrint(BCLog::PDAY,"DisconnectMintAsset: Cannot unserialize data inside of this transaction relating to an assetallocationmint\n");
        return false;
    }   
    const std::vector<unsigned char> &vchTxPath = mintPaydayCoin.vchTxPath;
    dev::RLP rlpTxPath(&vchTxPath);
    const uint32_t &nPath = rlpTxPath.toInt<uint32_t>(dev::RLP::VeryStrict);
    // remove eth spend tx from our internal db
    const std::pair<uint64_t, uint32_t> &ethKey = std::make_pair(mintPaydayCoin.nBlockNumber, nPath);
    vecMintKeys.emplace_back(ethKey);  
    // recver
    const std::string &receiverTupleStr = mintPaydayCoin.assetAllocationTuple.ToString();
    auto result1 = mapAssetAllocations.emplace(std::piecewise_construct,  std::forward_as_tuple(receiverTupleStr),  std::forward_as_tuple(std::move(emptyAllocation)));
    auto mapAssetAllocation = result1.first;
    const bool& mapAssetAllocationNotFound = result1.second;
    if(mapAssetAllocationNotFound){
        CAssetAllocation receiverAllocation;
        GetAssetAllocation(mintPaydayCoin.assetAllocationTuple, receiverAllocation);
        if (receiverAllocation.assetAllocationTuple.IsNull()) {
            receiverAllocation.assetAllocationTuple.nAsset = std::move(mintPaydayCoin.assetAllocationTuple.nAsset);
            receiverAllocation.assetAllocationTuple.witnessAddress = std::move(mintPaydayCoin.assetAllocationTuple.witnessAddress);
        } 
        mapAssetAllocation->second = std::move(receiverAllocation);                 
    }
    CAssetAllocation& storedReceiverAllocationRef = mapAssetAllocation->second;
    
    storedReceiverAllocationRef.nBalance -= mintPaydayCoin.nValueAsset;
    if(storedReceiverAllocationRef.nBalance < 0) {
        LogPrint(BCLog::PDAY,"DisconnectMintAsset: Receiver balance of %s is negative: %lld\n",mintPaydayCoin.assetAllocationTuple.ToString(), storedReceiverAllocationRef.nBalance);
        return false;
    }       
    else if(storedReceiverAllocationRef.nBalance == 0){
        storedReceiverAllocationRef.SetNull();
    }
    if(fAssetIndex){
        const uint256& txid = tx.GetHash();
        if(fAssetIndexGuids.empty() || std::find(fAssetIndexGuids.begin(), fAssetIndexGuids.end(), mintPaydayCoin.assetAllocationTuple.nAsset) != fAssetIndexGuids.end()){
            if(!passetindexdb->EraseIndexTXID(mintPaydayCoin.assetAllocationTuple, txid)){
                LogPrint(BCLog::PDAY,"DisconnectMintAsset: Could not erase mint asset allocation from asset allocation index\n");
            }
            if(!passetindexdb->EraseIndexTXID(mintPaydayCoin.assetAllocationTuple.nAsset, txid)){
                LogPrint(BCLog::PDAY,"DisconnectMintAsset: Could not erase mint asset allocation from asset index\n");
            } 
        }      
    }    
    return true; 
}
bool DisconnectAssetAllocation(const CTransaction &tx, AssetAllocationMap &mapAssetAllocations){
    const uint256& txid = tx.GetHash();
    CAssetAllocation theAssetAllocation(tx);

    const std::string &senderTupleStr = theAssetAllocation.assetAllocationTuple.ToString();
    if(theAssetAllocation.assetAllocationTuple.IsNull()){
        LogPrint(BCLog::PDAY,"DisconnectAssetAllocation: Could not decode asset allocation\n");
        return false;
    }
    auto result = mapAssetAllocations.emplace(std::piecewise_construct,  std::forward_as_tuple(senderTupleStr),  std::forward_as_tuple(std::move(emptyAllocation)));
    auto mapAssetAllocation = result.first;
    const bool & mapAssetAllocationNotFound = result.second;
    if(mapAssetAllocationNotFound){
        CAssetAllocation senderAllocation;
        GetAssetAllocation(theAssetAllocation.assetAllocationTuple, senderAllocation);
        if (senderAllocation.assetAllocationTuple.IsNull()) {
            senderAllocation.assetAllocationTuple.nAsset = std::move(theAssetAllocation.assetAllocationTuple.nAsset);
            senderAllocation.assetAllocationTuple.witnessAddress = std::move(theAssetAllocation.assetAllocationTuple.witnessAddress);       
        } 
        mapAssetAllocation->second = std::move(senderAllocation);               
    }
    CAssetAllocation& storedSenderAllocationRef = mapAssetAllocation->second;

    for(const auto& amountTuple:theAssetAllocation.listSendingAllocationAmounts){
        const CAssetAllocationTuple receiverAllocationTuple(theAssetAllocation.assetAllocationTuple.nAsset, amountTuple.first);
       
        const std::string &receiverTupleStr = receiverAllocationTuple.ToString();
        CAssetAllocation receiverAllocation;
        
        auto result1 = mapAssetAllocations.emplace(std::piecewise_construct,  std::forward_as_tuple(receiverTupleStr),  std::forward_as_tuple(std::move(emptyAllocation)));
        auto mapAssetAllocationReceiver = result1.first;
        const bool& mapAssetAllocationReceiverNotFound = result1.second;
        if(mapAssetAllocationReceiverNotFound){
            GetAssetAllocation(receiverAllocationTuple, receiverAllocation);
            if (receiverAllocation.assetAllocationTuple.IsNull()) {
                receiverAllocation.assetAllocationTuple.nAsset = std::move(receiverAllocationTuple.nAsset);
                receiverAllocation.assetAllocationTuple.witnessAddress = std::move(receiverAllocationTuple.witnessAddress);
            } 
            mapAssetAllocationReceiver->second = std::move(receiverAllocation);               
        }
        CAssetAllocation& storedReceiverAllocationRef = mapAssetAllocationReceiver->second;

        // reverse allocations
        storedReceiverAllocationRef.nBalance -= amountTuple.second;
        storedSenderAllocationRef.nBalance += amountTuple.second; 

        if(storedReceiverAllocationRef.nBalance < 0) {
            LogPrint(BCLog::PDAY,"DisconnectAssetAllocation: Receiver balance of %s is negative: %lld\n",receiverAllocationTuple.ToString(), storedReceiverAllocationRef.nBalance);
            return false;
        }
        else if(storedReceiverAllocationRef.nBalance == 0){
            storedReceiverAllocationRef.SetNull();  
        }
        if(fAssetIndex){
            if(fAssetIndexGuids.empty() || std::find(fAssetIndexGuids.begin(), fAssetIndexGuids.end(), receiverAllocationTuple.nAsset) != fAssetIndexGuids.end()){
                if(!passetindexdb->EraseIndexTXID(receiverAllocationTuple, txid)){
                    LogPrint(BCLog::PDAY,"DisconnectAssetAllocation: Could not erase receiver allocation from asset allocation index\n");
                }
                if(!passetindexdb->EraseIndexTXID(receiverAllocationTuple.nAsset, txid)){
                    LogPrint(BCLog::PDAY,"DisconnectAssetAllocation: Could not erase receiver allocation from asset index\n");
                }
            }
        }                                       
    }
    if(fAssetIndex){
        if(fAssetIndexGuids.empty() || std::find(fAssetIndexGuids.begin(), fAssetIndexGuids.end(), theAssetAllocation.assetAllocationTuple.nAsset) != fAssetIndexGuids.end()){
            if(!passetindexdb->EraseIndexTXID(theAssetAllocation.assetAllocationTuple, txid)){
                LogPrint(BCLog::PDAY,"DisconnectAssetAllocation: Could not erase sender allocation from asset allocation index\n");
            }
            if(!passetindexdb->EraseIndexTXID(theAssetAllocation.assetAllocationTuple.nAsset, txid)){
                LogPrint(BCLog::PDAY,"DisconnectAssetAllocation: Could not erase sender allocation from asset index\n");
            }
        }     
    }         
    return true; 
}
bool CheckAssetAllocationInputs(const CTransaction &tx, const CCoinsViewCache &inputs,
        bool fJustCheck, int nHeight, const uint256& blockhash, AssetAllocationMap &mapAssetAllocations, std::vector<COutPoint> &vecLockedOutpoints, string &errorMessage, bool& bOverflow, const bool &bSanityCheck, const bool &bMiner) {
    if (passetallocationdb == nullptr)
        return false;
    const uint256 & txHash = tx.GetHash();
    if (!bSanityCheck)
        LogPrint(BCLog::PDAY,"*** ASSET ALLOCATION %d %d %s %s\n", nHeight,
            ::ChainActive().Tip()->nHeight, txHash.ToString().c_str(),
            fJustCheck ? "JUSTCHECK" : "BLOCK");
            

    // unserialize assetallocation from txn, check for valid
    CAssetAllocation theAssetAllocation(tx);
    if(theAssetAllocation.assetAllocationTuple.IsNull())
    {
        errorMessage = "PAYDAYCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Cannot unserialize data inside of this transaction relating to an assetallocation");
        return error(errorMessage.c_str());
    }

    string retError = "";
    if(fJustCheck)
    {
        switch (tx.nVersion) {
        case PAYDAYCOIN_TX_VERSION_ASSET_ALLOCATION_SEND:
            if (theAssetAllocation.listSendingAllocationAmounts.empty())
            {
                errorMessage = "PAYDAYCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 1004 - " + _("Asset send must send an input or transfer balance");
                return error(errorMessage.c_str());
            }
            if (theAssetAllocation.listSendingAllocationAmounts.size() > 250)
            {
                errorMessage = "PAYDAYCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 1005 - " + _("Too many receivers in one allocation send, maximum of 250 is allowed at once");
                return error(errorMessage.c_str());
            }
			if (!theAssetAllocation.lockedOutpoint.IsNull())
			{
				errorMessage = "PAYDAYCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 1004 - " + _("Cannot include locked outpoint information for allocation send");
				return error(errorMessage.c_str());
			}
            break; 
        case PAYDAYCOIN_TX_VERSION_ASSET_ALLOCATION_BURN:
			if (!theAssetAllocation.lockedOutpoint.IsNull())
			{
				errorMessage = "PAYDAYCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 1004 - " + _("Cannot include locked outpoint information for allocation burn");
				return error(errorMessage.c_str());
			}
            break;  
		case PAYDAYCOIN_TX_VERSION_ASSET_ALLOCATION_LOCK:
			if (theAssetAllocation.lockedOutpoint.IsNull())
			{
				errorMessage = "PAYDAYCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 1004 - " + _("Asset allocation lock must include outpoint information");
				return error(errorMessage.c_str());
			}
			break;
        default:
            errorMessage = "PAYDAYCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 1009 - " + _("Asset transaction has unknown op");
            return error(errorMessage.c_str());
        }
    }

    const CWitnessAddress &user1 = theAssetAllocation.assetAllocationTuple.witnessAddress;
    const string & senderTupleStr = theAssetAllocation.assetAllocationTuple.ToString();

    CAssetAllocation dbAssetAllocation;
    AssetAllocationMap::iterator mapAssetAllocation;
    CAsset dbAsset;
    if(fJustCheck){
        if (!GetAssetAllocation(theAssetAllocation.assetAllocationTuple, dbAssetAllocation))
        {
            errorMessage = "PAYDAYCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 1010 - " + _("Cannot find sender asset allocation");
            return error(errorMessage.c_str());
        }     
    }
    else{
        auto result = mapAssetAllocations.emplace(std::piecewise_construct,  std::forward_as_tuple(senderTupleStr),  std::forward_as_tuple(std::move(emptyAllocation)));
        mapAssetAllocation = result.first;
        const bool& mapAssetAllocationNotFound = result.second;
        
        if(mapAssetAllocationNotFound){
            if (!GetAssetAllocation(theAssetAllocation.assetAllocationTuple, dbAssetAllocation))
            {
                errorMessage = "PAYDAYCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 1010 - " + _("Cannot find sender asset allocation");
                return error(errorMessage.c_str());
            }
            mapAssetAllocation->second = std::move(dbAssetAllocation);             
        }
    }
    CAssetAllocation& storedSenderAllocationRef = fJustCheck? dbAssetAllocation:mapAssetAllocation->second;
    
    if (!GetAsset(storedSenderAllocationRef.assetAllocationTuple.nAsset, dbAsset))
    {
        errorMessage = "PAYDAYCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 1011 - " + _("Failed to read from asset DB");
        return error(errorMessage.c_str());
    }
        
    AssetBalanceMap::iterator mapBalanceSender;
    CAmount mapBalanceSenderCopy;
    bool mapSenderMempoolBalanceNotFound = false;
    if(fJustCheck && !bSanityCheck){
        LOCK(cs_assetallocation); 
        auto result =  mempoolMapAssetBalances.emplace(std::piecewise_construct,  std::forward_as_tuple(senderTupleStr),  std::forward_as_tuple(std::move(storedSenderAllocationRef.nBalance))); 
        mapBalanceSender = result.first;
        mapSenderMempoolBalanceNotFound = result.second;
        mapBalanceSenderCopy = mapBalanceSender->second;
    }
    else
        mapBalanceSenderCopy = storedSenderAllocationRef.nBalance;     
           
    if (tx.nVersion == PAYDAYCOIN_TX_VERSION_ASSET_ALLOCATION_BURN)
    {       
        std::vector<unsigned char> vchEthAddress;
        uint32_t nAssetFromScript;
        CAmount nAmountFromScript;
        CWitnessAddress burnWitnessAddress;
        if(!GetPaydayCoinBurnData(tx, nAssetFromScript, burnWitnessAddress, nAmountFromScript, vchEthAddress)){
            errorMessage = "PAYDAYCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR ERRCODE: 1001 - " + _("Cannot unserialize data inside of this transaction relating to an assetallocationburn");
            return error(errorMessage.c_str());
        }   
        if(burnWitnessAddress != user1)
        {
            errorMessage = "PAYDAYCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 1010 - " + _("Mismatching deserailized witness address");
            return error(errorMessage.c_str());
        }
        if(storedSenderAllocationRef.assetAllocationTuple.nAsset != nAssetFromScript)
        {
            errorMessage = "PAYDAYCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 1010 - " + _("Invalid asset details entered in the script output");
            return error(errorMessage.c_str());
        }
        if (storedSenderAllocationRef.assetAllocationTuple != theAssetAllocation.assetAllocationTuple || !FindAssetOwnerInTx(inputs, tx, user1))
        {
            errorMessage = "PAYDAYCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 1015 - " + _("Cannot send this asset. Asset allocation owner must sign off on this change");
            return error(errorMessage.c_str());
        }
        if(dbAsset.vchContract.empty())
        {
            errorMessage = "PAYDAYCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 1010 - " + _("Cannot burn, no contract provided in asset by owner");
            return error(errorMessage.c_str());
        } 
        if (nAmountFromScript <= 0 || nAmountFromScript > dbAsset.nTotalSupply)
        {
            errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2029 - " + _("Amount out of money range");
            return error(errorMessage.c_str());
        }        
        mapBalanceSenderCopy -= nAmountFromScript;
        if (mapBalanceSenderCopy < 0) {
            if(fJustCheck && !bSanityCheck)
            {
                LOCK(cs_assetallocation); 
                if(mapSenderMempoolBalanceNotFound){
                    mempoolMapAssetBalances.erase(mapBalanceSender);
                }
            }
            bOverflow = true;
            errorMessage = "PAYDAYCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 1016 - " + _("Sender balance is insufficient");
            return error(errorMessage.c_str());
        }
        if (!fJustCheck) {   
            const CAssetAllocationTuple receiverAllocationTuple(nAssetFromScript,  CWitnessAddress(0, vchFromString("burn")));
            const string& receiverTupleStr = receiverAllocationTuple.ToString();  
            auto result = mapAssetAllocations.emplace(std::piecewise_construct,  std::forward_as_tuple(receiverTupleStr),  std::forward_as_tuple(std::move(emptyAllocation)));
            auto mapAssetAllocationReceiver = result.first;
            const bool& mapAssetAllocationReceiverNotFound = result.second;
            if(mapAssetAllocationReceiverNotFound){
                CAssetAllocation dbAssetAllocationReceiver;
                if (!GetAssetAllocation(receiverAllocationTuple, dbAssetAllocationReceiver)) {               
                    dbAssetAllocationReceiver.assetAllocationTuple.nAsset = std::move(receiverAllocationTuple.nAsset);
                    dbAssetAllocationReceiver.assetAllocationTuple.witnessAddress = std::move(receiverAllocationTuple.witnessAddress);              
                }
                mapAssetAllocationReceiver->second = std::move(dbAssetAllocationReceiver);                  
            } 
            mapAssetAllocationReceiver->second.nBalance += nAmountFromScript;                        
        }else if (!bSanityCheck && !bMiner) {
            LOCK(cs_assetallocationarrival);
            // add conflicting sender if using ZDAG
            assetAllocationConflicts.insert(senderTupleStr);
        }
    }
	else if (tx.nVersion == PAYDAYCOIN_TX_VERSION_ASSET_ALLOCATION_LOCK)
	{
		if (storedSenderAllocationRef.assetAllocationTuple != theAssetAllocation.assetAllocationTuple || !FindAssetOwnerInTx(inputs, tx, user1))
		{
			errorMessage = "PAYDAYCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 1015a - " + _("Cannot send this asset. Asset allocation owner must sign off on this change");
			return error(errorMessage.c_str());
		}
        if (!bSanityCheck && !fJustCheck){
    		storedSenderAllocationRef.lockedOutpoint = theAssetAllocation.lockedOutpoint;
    		// this will batch write the outpoint in the calling function, we save the outpoint so that we cannot spend this outpoint without creating an PAYDAYCOIN_TX_VERSION_ASSET_ALLOCATION_SEND transaction
    		vecLockedOutpoints.emplace_back(std::move(theAssetAllocation.lockedOutpoint));
        }
	}
    else if (tx.nVersion == PAYDAYCOIN_TX_VERSION_ASSET_ALLOCATION_SEND)
	{
        if (storedSenderAllocationRef.assetAllocationTuple != theAssetAllocation.assetAllocationTuple || !FindAssetOwnerInTx(inputs, tx, user1, storedSenderAllocationRef.lockedOutpoint))
        {
            errorMessage = "PAYDAYCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 1015a - " + _("Cannot send this asset. Asset allocation owner must sign off on this change");
            return error(errorMessage.c_str());
        }       
		// ensure lockedOutpoint is cleared on PoW if it was set once a send happens, it is useful only once typical for atomic scripts like CLTV based atomic swaps or hashlock type of usecases
		if (!bSanityCheck && !fJustCheck && !storedSenderAllocationRef.lockedOutpoint.IsNull()) {
			// this will flag the batch write function on plockedoutpointsdb to erase this outpoint
			vecLockedOutpoints.emplace_back(std::move(emptyOutPoint));
			storedSenderAllocationRef.lockedOutpoint.SetNull();
		}
        // check balance is sufficient on sender
        CAmount nTotal = 0;
        for (const auto& amountTuple : theAssetAllocation.listSendingAllocationAmounts) {
            nTotal += amountTuple.second;
            if (amountTuple.second <= 0)
            {
                errorMessage = "PAYDAYCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 1020 - " + _("Receiving amount must be positive");
                return error(errorMessage.c_str());
            }           
        }
        mapBalanceSenderCopy -= nTotal;
        if (mapBalanceSenderCopy < 0) {
            if(fJustCheck && !bSanityCheck && !bMiner)
            {
                LOCK(cs_assetallocationarrival);
                // add conflicting sender
                assetAllocationConflicts.insert(senderTupleStr);
                // If we already have this transaction in the arrival map we must have already accepted it, so don't set to overflow.
                // We return true so that the mempool doesn't remove this transaction erroneously
                ArrivalTimesMap &arrivalTimes = arrivalTimesMap[senderTupleStr];
                ArrivalTimesMap::iterator it = arrivalTimes.find(txHash);
                if (it != arrivalTimes.end()){
                    LogPrint(BCLog::PDAY, "PaydayCoin ZDAG transaction overflowed but already accepted in mempool, so this transaction acts as a no-op...\n");
                    return true;
                }
            }
            if(fJustCheck && !bSanityCheck)
            {
                LOCK(cs_assetallocation); 
                if(mapSenderMempoolBalanceNotFound){
                    mempoolMapAssetBalances.erase(mapBalanceSender);
                }
            }
            bOverflow = true;            
            errorMessage = "PAYDAYCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 1021 - " + _("Sender balance is insufficient");
            return error(errorMessage.c_str());
        }
               
        for (const auto& amountTuple : theAssetAllocation.listSendingAllocationAmounts) {
            if (amountTuple.first == theAssetAllocation.assetAllocationTuple.witnessAddress) {
                if(fJustCheck && !bSanityCheck)
                {
                    LOCK(cs_assetallocation); 
                    if(mapSenderMempoolBalanceNotFound){
                        mempoolMapAssetBalances.erase(mapBalanceSender);
                    }
                }           
                errorMessage = "PAYDAYCOIN_ASSET_ALLOCATION_CONSENSUS_ERROR: ERRCODE: 1022 - " + _("Cannot send an asset allocation to yourself");
                return error(errorMessage.c_str());
            }
        
            const CAssetAllocationTuple receiverAllocationTuple(theAssetAllocation.assetAllocationTuple.nAsset, amountTuple.first);
            const string &receiverTupleStr = receiverAllocationTuple.ToString();
            AssetBalanceMap::iterator mapBalanceReceiver;
            AssetAllocationMap::iterator mapBalanceReceiverBlock;            
            if(fJustCheck && !bSanityCheck){
                LOCK(cs_assetallocation);
                auto result = mempoolMapAssetBalances.emplace(std::piecewise_construct,  std::forward_as_tuple(receiverTupleStr),  std::forward_as_tuple(0));
                auto mapBalanceReceiver = result.first;
                const bool& mapAssetAllocationReceiverNotFound = result.second;
                if(mapAssetAllocationReceiverNotFound){
                    CAssetAllocation receiverAllocation;
                    GetAssetAllocation(receiverAllocationTuple, receiverAllocation);
                    mapBalanceReceiver->second = receiverAllocation.nBalance;
                }
                if(!bSanityCheck){
                    mapBalanceReceiver->second += amountTuple.second;
                }
            }  
            else{           
                auto result =  mapAssetAllocations.emplace(std::piecewise_construct,  std::forward_as_tuple(receiverTupleStr),  std::forward_as_tuple(std::move(emptyAllocation)));
                auto mapBalanceReceiverBlock = result.first;
                const bool& mapAssetAllocationReceiverBlockNotFound = result.second;
                if(mapAssetAllocationReceiverBlockNotFound){
                    CAssetAllocation receiverAllocation;
                    if (!GetAssetAllocation(receiverAllocationTuple, receiverAllocation)) {                   
                        receiverAllocation.assetAllocationTuple.nAsset = std::move(receiverAllocationTuple.nAsset);
                        receiverAllocation.assetAllocationTuple.witnessAddress = std::move(receiverAllocationTuple.witnessAddress);                       
                    }
                    mapBalanceReceiverBlock->second = std::move(receiverAllocation);  
                }
                mapBalanceReceiverBlock->second.nBalance += amountTuple.second;
                if(!fJustCheck){ 
                    // to remove mempool balances but need to check to ensure that all txid's from arrivalTimes are first gone before removing receiver mempool balance
                    // otherwise one can have a conflict as a sender and send himself an allocation and clear the mempool balance inadvertently
                    ResetAssetAllocation(receiverTupleStr, txHash, bMiner);      
                }     
            }

        }   
    }
    // write assetallocation  
    // asset sends are the only ones confirming without PoW
    if(!fJustCheck){
        if (!bSanityCheck && tx.nVersion != PAYDAYCOIN_TX_VERSION_ASSET_ALLOCATION_LOCK) {
            ResetAssetAllocation(senderTupleStr, txHash, bMiner);
           
        } 
        storedSenderAllocationRef.listSendingAllocationAmounts.clear();
        storedSenderAllocationRef.nBalance = std::move(mapBalanceSenderCopy);
        if(storedSenderAllocationRef.nBalance == 0)
            storedSenderAllocationRef.SetNull();    

        if(!bMiner) {   
            // send notification on pow, for zdag transactions this is the second notification meaning the zdag tx has been confirmed
            passetallocationdb->WriteAssetAllocationIndex(tx, dbAsset, nHeight, blockhash);  
            LogPrint(BCLog::PDAY,"CONNECTED ASSET ALLOCATION: op=%s assetallocation=%s hash=%s height=%d fJustCheck=%d\n",
                assetAllocationFromTx(tx.nVersion).c_str(),
                senderTupleStr.c_str(),
                txHash.ToString().c_str(),
                nHeight,
                fJustCheck ? 1 : 0);      
        }
                    
    }
    else if(!bSanityCheck){
		if(tx.nVersion != PAYDAYCOIN_TX_VERSION_ASSET_ALLOCATION_LOCK)
        {
            LOCK(cs_assetallocationarrival);
            ArrivalTimesMap &arrivalTimes = arrivalTimesMap[senderTupleStr];
            arrivalTimes[txHash] = GetTimeMillis();
        }

        // send a realtime notification on zdag, send another when pow happens (above)
        if(tx.nVersion == PAYDAYCOIN_TX_VERSION_ASSET_ALLOCATION_SEND)
            passetallocationdb->WriteAssetAllocationIndex(tx, dbAsset, nHeight, blockhash);
        {
            LOCK(cs_assetallocation);
            mapBalanceSender->second = std::move(mapBalanceSenderCopy);
        }
    }
     
    return true;
}

bool DisconnectAssetSend(const CTransaction &tx, AssetMap &mapAssets, AssetAllocationMap &mapAssetAllocations){
    const uint256 &txid = tx.GetHash();
    CAsset dbAsset;
    CAssetAllocation theAssetAllocation(tx);
    if(theAssetAllocation.assetAllocationTuple.IsNull()){
        LogPrint(BCLog::PDAY,"DisconnectAssetSend: Could not decode asset allocation in asset send\n");
        return false;
    } 
    auto result  = mapAssets.emplace(std::piecewise_construct,  std::forward_as_tuple(theAssetAllocation.assetAllocationTuple.nAsset),  std::forward_as_tuple(std::move(emptyAsset)));
    auto mapAsset = result.first;
    const bool& mapAssetNotFound = result.second;
    if(mapAssetNotFound){
        if (!GetAsset(theAssetAllocation.assetAllocationTuple.nAsset, dbAsset)) {
            LogPrint(BCLog::PDAY,"DisconnectAssetSend: Could not get asset %d\n",theAssetAllocation.assetAllocationTuple.nAsset);
            return false;               
        } 
        mapAsset->second = std::move(dbAsset);                        
    }
    CAsset& storedSenderRef = mapAsset->second;
               
               
    for(const auto& amountTuple:theAssetAllocation.listSendingAllocationAmounts){
        const CAssetAllocationTuple receiverAllocationTuple(theAssetAllocation.assetAllocationTuple.nAsset, amountTuple.first);
        const std::string &receiverTupleStr = receiverAllocationTuple.ToString();
        CAssetAllocation receiverAllocation;
        auto result = mapAssetAllocations.emplace(std::piecewise_construct,  std::forward_as_tuple(receiverTupleStr),  std::forward_as_tuple(std::move(emptyAllocation)));
        auto mapAssetAllocation = result.first;
        const bool &mapAssetAllocationNotFound = result.second;
        if(mapAssetAllocationNotFound){
            GetAssetAllocation(receiverAllocationTuple, receiverAllocation);
            if (receiverAllocation.assetAllocationTuple.IsNull()) {
                receiverAllocation.assetAllocationTuple.nAsset = std::move(receiverAllocationTuple.nAsset);
                receiverAllocation.assetAllocationTuple.witnessAddress = std::move(receiverAllocationTuple.witnessAddress);
            } 
            mapAssetAllocation->second = std::move(receiverAllocation);            
        }
        CAssetAllocation& storedReceiverAllocationRef = mapAssetAllocation->second;
                    
        // reverse allocation
        if(storedReceiverAllocationRef.nBalance >= amountTuple.second){
            storedReceiverAllocationRef.nBalance -= amountTuple.second;
            storedSenderRef.nBalance += amountTuple.second;
        } 

        if(storedReceiverAllocationRef.nBalance == 0){
            storedReceiverAllocationRef.SetNull();       
        }
        
        if(fAssetIndex){
            if(fAssetIndexGuids.empty() || std::find(fAssetIndexGuids.begin(), fAssetIndexGuids.end(), receiverAllocationTuple.nAsset) != fAssetIndexGuids.end()){
                if(!passetindexdb->EraseIndexTXID(receiverAllocationTuple, txid)){
                    LogPrint(BCLog::PDAY,"DisconnectAssetSend: Could not erase receiver allocation from asset allocation index\n");
                }
                if(!passetindexdb->EraseIndexTXID(receiverAllocationTuple.nAsset, txid)){
                    LogPrint(BCLog::PDAY,"DisconnectAssetSend: Could not erase receiver allocation from asset index\n");
                } 
            }
        }                                             
    }     
    if(fAssetIndex){
        if(fAssetIndexGuids.empty() || std::find(fAssetIndexGuids.begin(), fAssetIndexGuids.end(), theAssetAllocation.assetAllocationTuple.nAsset) != fAssetIndexGuids.end()){
            if(!passetindexdb->EraseIndexTXID(theAssetAllocation.assetAllocationTuple, txid)){
                LogPrint(BCLog::PDAY,"DisconnectAssetSend: Could not erase sender allocation from asset allocation index\n");
            }
            if(!passetindexdb->EraseIndexTXID(theAssetAllocation.assetAllocationTuple.nAsset, txid)){
                LogPrint(BCLog::PDAY,"DisconnectAssetSend: Could not erase sender allocation from asset index\n");
            }
        }     
    }          
    return true;  
}
bool DisconnectAssetUpdate(const CTransaction &tx, AssetMap &mapAssets){
    
    CAsset dbAsset;
    CAsset theAsset(tx);
    if(theAsset.IsNull()){
        LogPrint(BCLog::PDAY,"DisconnectAssetUpdate: Could not decode asset\n");
        return false;
    }
    auto result = mapAssets.emplace(std::piecewise_construct,  std::forward_as_tuple(theAsset.nAsset),  std::forward_as_tuple(std::move(emptyAsset)));
    auto mapAsset = result.first;
    const bool &mapAssetNotFound = result.second;
    if(mapAssetNotFound){
        if (!GetAsset(theAsset.nAsset, dbAsset)) {
            LogPrint(BCLog::PDAY,"DisconnectAssetUpdate: Could not get asset %d\n",theAsset.nAsset);
            return false;               
        } 
        mapAsset->second = std::move(dbAsset);                    
    }
    CAsset& storedSenderRef = mapAsset->second;   
           
    if(theAsset.nBalance > 0){
        // reverse asset minting by the issuer
        storedSenderRef.nBalance -= theAsset.nBalance;
        storedSenderRef.nTotalSupply -= theAsset.nBalance;
        if(storedSenderRef.nBalance < 0 || storedSenderRef.nTotalSupply < 0) {
            LogPrint(BCLog::PDAY,"DisconnectAssetUpdate: Asset cannot be negative: Balance %lld, Supply: %lld\n",storedSenderRef.nBalance, storedSenderRef.nTotalSupply);
            return false;
        }                                          
    } 
    if(fAssetIndex){
        const uint256 &txid = tx.GetHash();
        if(fAssetIndexGuids.empty() || std::find(fAssetIndexGuids.begin(), fAssetIndexGuids.end(), theAsset.nAsset) != fAssetIndexGuids.end()){
            if(!passetindexdb->EraseIndexTXID(theAsset.nAsset, txid)){
                LogPrint(BCLog::PDAY,"DisconnectAssetUpdate: Could not erase asset update from asset index\n");
            }
        }
    }         
    return true;  
}
bool DisconnectAssetTransfer(const CTransaction &tx, AssetMap &mapAssets){
    
    CAsset dbAsset;
    CAsset theAsset(tx);
    if(theAsset.IsNull()){
        LogPrint(BCLog::PDAY,"DisconnectAssetTransfer: Could not decode asset\n");
        return false;
    }
    auto result = mapAssets.emplace(std::piecewise_construct,  std::forward_as_tuple(theAsset.nAsset),  std::forward_as_tuple(std::move(emptyAsset)));
    auto mapAsset = result.first;
    const bool &mapAssetNotFound = result.second;
    if(mapAssetNotFound){
        if (!GetAsset(theAsset.nAsset, dbAsset)) {
            LogPrint(BCLog::PDAY,"DisconnectAssetTransfer: Could not get asset %d\n",theAsset.nAsset);
            return false;               
        } 
        mapAsset->second = std::move(dbAsset);                    
    }
    CAsset& storedSenderRef = mapAsset->second; 
    // theAsset.witnessAddress  is enforced to be the sender of the transfer which was the owner at the time of transfer
    // so set it back to reverse the transfer
    storedSenderRef.witnessAddress = theAsset.witnessAddress;   
    if(fAssetIndex){
        const uint256 &txid = tx.GetHash();
        if(fAssetIndexGuids.empty() || std::find(fAssetIndexGuids.begin(), fAssetIndexGuids.end(), theAsset.nAsset) != fAssetIndexGuids.end()){
            if(!passetindexdb->EraseIndexTXID(theAsset.nAsset, txid)){
                LogPrint(BCLog::PDAY,"DisconnectAssetTransfer: Could not erase asset update from asset index\n");
            }
        }
    }         
    return true;  
}
bool DisconnectAssetActivate(const CTransaction &tx, AssetMap &mapAssets){
    
    CAsset theAsset(tx);
    
    if(theAsset.IsNull()){
        LogPrint(BCLog::PDAY,"DisconnectAssetActivate: Could not decode asset in asset activate\n");
        return false;
    }
    auto result = mapAssets.emplace(std::piecewise_construct,  std::forward_as_tuple(theAsset.nAsset),  std::forward_as_tuple(std::move(emptyAsset)));
    auto mapAsset = result.first;
    const bool &mapAssetNotFound = result.second;
    if(mapAssetNotFound){
        CAsset dbAsset;
        if (!GetAsset(theAsset.nAsset, dbAsset)) {
            LogPrint(BCLog::PDAY,"DisconnectAssetActivate: Could not get asset %d\n",theAsset.nAsset);
            return false;               
        } 
        mapAsset->second = std::move(dbAsset);      
    }
    mapAsset->second.SetNull();  
    if(fAssetIndex){
        const uint256 &txid = tx.GetHash();
        if(fAssetIndexGuids.empty() || std::find(fAssetIndexGuids.begin(), fAssetIndexGuids.end(), theAsset.nAsset) != fAssetIndexGuids.end()){
            if(!passetindexdb->EraseIndexTXID(theAsset.nAsset, txid)){
                LogPrint(BCLog::PDAY,"DisconnectAssetActivate: Could not erase asset activate from asset index\n");
            }
        }    
    }       
    return true;  
}
bool CheckAssetInputs(const CTransaction &tx, const CCoinsViewCache &inputs,
        bool fJustCheck, int nHeight, const uint256& blockhash, AssetMap& mapAssets, AssetAllocationMap &mapAssetAllocations, string &errorMessage, const bool &bSanityCheck, const bool &bMiner) {
    if (passetdb == nullptr)
        return false;
    const uint256& txHash = tx.GetHash();
    if (!bSanityCheck)
        LogPrint(BCLog::PDAY, "*** ASSET %d %d %s %s\n", nHeight,
            ::ChainActive().Tip()->nHeight, txHash.ToString().c_str(),
            fJustCheck ? "JUSTCHECK" : "BLOCK");

    // unserialize asset from txn, check for valid
    CAsset theAsset;
    CAssetAllocation theAssetAllocation;
    vector<unsigned char> vchData;

    int nDataOut;
    if(!GetPaydayCoinData(tx, vchData, nDataOut) || (tx.nVersion != PAYDAYCOIN_TX_VERSION_ASSET_SEND && !theAsset.UnserializeFromData(vchData)) || (tx.nVersion == PAYDAYCOIN_TX_VERSION_ASSET_SEND && !theAssetAllocation.UnserializeFromData(vchData)))
    {
        errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR ERRCODE: 2000 - " + _("Cannot unserialize data inside of this transaction relating to an asset");
        return error(errorMessage.c_str());
    }
    

    if(fJustCheck)
    {
        if (tx.nVersion != PAYDAYCOIN_TX_VERSION_ASSET_SEND) {
            if (theAsset.vchPubData.size() > MAX_VALUE_LENGTH)
            {
                errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2004 - " + _("Asset public data too big");
                return error(errorMessage.c_str());
            }
        }
        switch (tx.nVersion) {
        case PAYDAYCOIN_TX_VERSION_ASSET_ACTIVATE:
            if (theAsset.nAsset <= PAYDAYCOIN_TX_VERSION_MINT)
            {
                errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2005 - " + _("asset guid invalid");
                return error(errorMessage.c_str());
            }
            if (!theAsset.vchContract.empty() && theAsset.vchContract.size() != MAX_GUID_LENGTH)
            {
                errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2005 - " + _("Contract address not proper size");
                return error(errorMessage.c_str());
            }  
            if (theAsset.nPrecision > 8)
            {
                errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2005 - " + _("Precision must be between 0 and 8");
                return error(errorMessage.c_str());
            }
            if (theAsset.strSymbol.size() > 8 || theAsset.strSymbol.size() < 1)
            {
                errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2005 - " + _("Symbol must be between 1 and 8");
                return error(errorMessage.c_str());
            }
            if (!AssetRange(theAsset.nMaxSupply, theAsset.nPrecision))
            {
                errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2014 - " + _("Max supply out of money range");
                return error(errorMessage.c_str());
            }
            if (theAsset.nBalance > theAsset.nMaxSupply)
            {
                errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2015 - " + _("Total supply cannot exceed maximum supply");
                return error(errorMessage.c_str());
            }
            if (!theAsset.witnessAddress.IsValid())
            {
                errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2015 - " + _("Address specified is invalid");
                return error(errorMessage.c_str());
            }
            if(theAsset.nUpdateFlags > ASSET_UPDATE_ALL){
                errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2026 - " + _("Invalid update flags");
                return error(errorMessage.c_str());
            } 
            if(!theAsset.witnessAddressTransfer.IsNull())   {
                errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2026 - " + _("Cannot include transfer address upon activation");
                return error(errorMessage.c_str());
            }      
            break;

        case PAYDAYCOIN_TX_VERSION_ASSET_UPDATE:
            if (theAsset.nBalance < 0){
                errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2017 - " + _("Balance must be greater than or equal to 0");
                return error(errorMessage.c_str());
            }
            if (!theAssetAllocation.assetAllocationTuple.IsNull())
            {
                errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2019 - " + _("Cannot update allocations");
                return error(errorMessage.c_str());
            }
            if (!theAsset.vchContract.empty() && theAsset.vchContract.size() != MAX_GUID_LENGTH)
            {
                errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2005 - " + _("Contract address not proper size");
                return error(errorMessage.c_str());
            }  
            if(theAsset.nUpdateFlags > ASSET_UPDATE_ALL){
                errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2026 - " + _("Invalid update flags");
                return error(errorMessage.c_str());
            }  
            if(!theAsset.witnessAddressTransfer.IsNull())   {
                errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2026 - " + _("Cannot include transfer address upon update");
                return error(errorMessage.c_str());
            }           
            break;
            
        case PAYDAYCOIN_TX_VERSION_ASSET_SEND:
            if (theAssetAllocation.listSendingAllocationAmounts.empty())
            {
                errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2020 - " + _("Asset send must send an input or transfer balance");
                return error(errorMessage.c_str());
            }
            if (theAssetAllocation.listSendingAllocationAmounts.size() > 250)
            {
                errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2021 - " + _("Too many receivers in one allocation send, maximum of 250 is allowed at once");
                return error(errorMessage.c_str());
            }
            if(!theAsset.witnessAddressTransfer.IsNull())   {
                errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2026 - " + _("Cannot include transfer address upon sending asset");
                return error(errorMessage.c_str());
            }  
            break;
        case PAYDAYCOIN_TX_VERSION_ASSET_TRANSFER:
            if(theAsset.witnessAddressTransfer.IsNull())   {
                errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2026 - " + _("Must include transfer address upon transferring asset");
                return error(errorMessage.c_str());
            } 
            break;
            errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2023 - " + _("Asset transaction has unknown op");
            return error(errorMessage.c_str());
        }
    }

    CAsset dbAsset;
    const uint32_t &nAsset = tx.nVersion == PAYDAYCOIN_TX_VERSION_ASSET_SEND ? theAssetAllocation.assetAllocationTuple.nAsset : theAsset.nAsset;
    auto result = mapAssets.emplace(std::piecewise_construct,  std::forward_as_tuple(nAsset),  std::forward_as_tuple(std::move(emptyAsset)));
    auto mapAsset = result.first;
    const bool & mapAssetNotFound = result.second; 
    if (mapAssetNotFound)
    {
        if (!GetAsset(nAsset, dbAsset)){
            if (tx.nVersion != PAYDAYCOIN_TX_VERSION_ASSET_ACTIVATE) {
                errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2024 - " + _("Failed to read from asset DB");
                return error(errorMessage.c_str());
            }
            else
                mapAsset->second = std::move(theAsset);      
        }
        else{
            if(tx.nVersion == PAYDAYCOIN_TX_VERSION_ASSET_ACTIVATE){
                errorMessage =  "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2041 - " + _("Asset already exists");
                return error(errorMessage.c_str());
            }
            mapAsset->second = std::move(dbAsset);      
        }
    }
    CAsset &storedSenderAssetRef = mapAsset->second;
    if (tx.nVersion == PAYDAYCOIN_TX_VERSION_ASSET_TRANSFER) {
        if (theAsset.nAsset != storedSenderAssetRef.nAsset || storedSenderAssetRef.witnessAddress != theAsset.witnessAddress || !FindAssetOwnerInTx(inputs, tx, storedSenderAssetRef.witnessAddress))
        {
            errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 1015 - " + _("Cannot transfer this asset. Asset owner must sign off on this change");
            return error(errorMessage.c_str());
        } 
		if(theAsset.nPrecision != storedSenderAssetRef.nPrecision)
		{
			errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 1015 - " + _("Cannot transfer this asset. Precision cannot be changed.");
			return error(errorMessage.c_str());
		}
        if(theAsset.strSymbol != storedSenderAssetRef.strSymbol)
        {
            errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 1015 - " + _("Cannot transfer this asset. Symbol cannot be changed.");
            return error(errorMessage.c_str());
        }        
        storedSenderAssetRef.witnessAddress = theAsset.witnessAddressTransfer;   
        // sanity to ensure transfer field is never set on the actual asset in db  
        storedSenderAssetRef.witnessAddressTransfer.SetNull();      
    }

    else if (tx.nVersion == PAYDAYCOIN_TX_VERSION_ASSET_UPDATE) {
        if (theAsset.nAsset != storedSenderAssetRef.nAsset || storedSenderAssetRef.witnessAddress != theAsset.witnessAddress || !FindAssetOwnerInTx(inputs, tx, storedSenderAssetRef.witnessAddress))
        {
            errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 1015 - " + _("Cannot update this asset. Asset owner must sign off on this change");
            return error(errorMessage.c_str());
        }
		if (theAsset.nPrecision != storedSenderAssetRef.nPrecision)
		{
			errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 1015 - " + _("Cannot update this asset. Precision cannot be changed.");
			return error(errorMessage.c_str());
		}
        if(theAsset.strSymbol != storedSenderAssetRef.strSymbol)
        {
            errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 1015 - " + _("Cannot update this asset. Symbol cannot be changed.");
            return error(errorMessage.c_str());
        }         
        if (theAsset.nBalance > 0 && !(storedSenderAssetRef.nUpdateFlags & ASSET_UPDATE_SUPPLY))
        {
            errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2026 - " + _("Insufficient privileges to update supply");
            return error(errorMessage.c_str());
        }          
        // increase total supply
        storedSenderAssetRef.nTotalSupply += theAsset.nBalance;
        storedSenderAssetRef.nBalance += theAsset.nBalance;

        if (!AssetRange(storedSenderAssetRef.nTotalSupply, storedSenderAssetRef.nPrecision))
        {
            errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2029 - " + _("Total supply out of money range");
            return error(errorMessage.c_str());
        }
        if (storedSenderAssetRef.nTotalSupply > storedSenderAssetRef.nMaxSupply)
        {
            errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2030 - " + _("Total supply cannot exceed maximum supply");
            return error(errorMessage.c_str());
        }
		if (!theAsset.vchPubData.empty()) {
			if (!(storedSenderAssetRef.nUpdateFlags & ASSET_UPDATE_DATA))
			{
				errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2026 - " + _("Insufficient privileges to update public data");
				return error(errorMessage.c_str());
			}
			storedSenderAssetRef.vchPubData = theAsset.vchPubData;
		}
                                    
		if (!theAsset.vchContract.empty() && tx.nVersion != PAYDAYCOIN_TX_VERSION_ASSET_TRANSFER) {
			if (!(storedSenderAssetRef.nUpdateFlags & ASSET_UPDATE_CONTRACT))
			{
				errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2026 - " + _("Insufficient privileges to update smart contract");
				return error(errorMessage.c_str());
			}
			storedSenderAssetRef.vchContract = theAsset.vchContract;
		}
 
        if (theAsset.nUpdateFlags > 0) {
			if (!(storedSenderAssetRef.nUpdateFlags & (ASSET_UPDATE_FLAGS | ASSET_UPDATE_ADMIN))) {
				errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2040 - " + _("Insufficient privileges to update flags");
				return error(errorMessage.c_str());
			}
			storedSenderAssetRef.nUpdateFlags = theAsset.nUpdateFlags;
        } 
    }      
    else if (tx.nVersion == PAYDAYCOIN_TX_VERSION_ASSET_SEND) {
        if (storedSenderAssetRef.nAsset != theAssetAllocation.assetAllocationTuple.nAsset || storedSenderAssetRef.witnessAddress != theAssetAllocation.assetAllocationTuple.witnessAddress || !FindAssetOwnerInTx(inputs, tx, storedSenderAssetRef.witnessAddress))
        {
            errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 1015 - " + _("Cannot send this asset. Asset owner must sign off on this change");
            return error(errorMessage.c_str());
        }

        // check balance is sufficient on sender
        CAmount nTotal = 0;
        for (const auto& amountTuple : theAssetAllocation.listSendingAllocationAmounts) {
            nTotal += amountTuple.second;
            if (amountTuple.second <= 0)
            {
                errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2032 - " + _("Receiving amount must be positive");
                return error(errorMessage.c_str());
            }
        }
        if (storedSenderAssetRef.nBalance < nTotal) {
            errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 2033 - " + _("Sender balance is insufficient");
            return error(errorMessage.c_str());
        }
        for (const auto& amountTuple : theAssetAllocation.listSendingAllocationAmounts) {
            if (!bSanityCheck) {
                CAssetAllocation receiverAllocation;
                const CAssetAllocationTuple receiverAllocationTuple(theAssetAllocation.assetAllocationTuple.nAsset, amountTuple.first);
                const string& receiverTupleStr = receiverAllocationTuple.ToString();
                auto result =  mapAssetAllocations.emplace(std::piecewise_construct,  std::forward_as_tuple(receiverTupleStr),  std::forward_as_tuple(std::move(emptyAllocation)));
                auto mapAssetAllocation = result.first;
                const bool& mapAssetAllocationNotFound = result.second;
               
                if(mapAssetAllocationNotFound){
                    GetAssetAllocation(receiverAllocationTuple, receiverAllocation);
                    if (receiverAllocation.assetAllocationTuple.IsNull()) {
                        receiverAllocation.assetAllocationTuple.nAsset = std::move(receiverAllocationTuple.nAsset);
                        receiverAllocation.assetAllocationTuple.witnessAddress = std::move(receiverAllocationTuple.witnessAddress);                       
                    } 
                    mapAssetAllocation->second = std::move(receiverAllocation);                   
                }
				// adjust receiver balance
                mapAssetAllocation->second.nBalance += amountTuple.second;
                                        
                // adjust sender balance
                storedSenderAssetRef.nBalance -= amountTuple.second;                              
            }
        }
        if (!bSanityCheck && !fJustCheck && !bMiner)
            passetallocationdb->WriteAssetAllocationIndex(tx, storedSenderAssetRef, nHeight, blockhash);  
    }
    else if (tx.nVersion == PAYDAYCOIN_TX_VERSION_ASSET_ACTIVATE)
    {
        if (!FindAssetOwnerInTx(inputs, tx, storedSenderAssetRef.witnessAddress))
        {
            errorMessage = "PAYDAYCOIN_ASSET_CONSENSUS_ERROR: ERRCODE: 1015 - " + _("Cannot create this asset. Asset owner must sign off on this change");
            return error(errorMessage.c_str());
        }          
        // starting supply is the supplied balance upon init
        storedSenderAssetRef.nTotalSupply = storedSenderAssetRef.nBalance;
    }
    // set the asset's txn-dependent values
    storedSenderAssetRef.nHeight = nHeight;
    storedSenderAssetRef.txHash = txHash;
    // write asset, if asset send, only write on pow since asset -> asset allocation is not 0-conf compatible
    if (!bSanityCheck && !fJustCheck && !bMiner) {
        passetdb->WriteAssetIndex(tx, storedSenderAssetRef, nHeight, blockhash);
        LogPrint(BCLog::PDAY,"CONNECTED ASSET: tx=%s symbol=%d hash=%s height=%d fJustCheck=%d\n",
                assetFromTx(tx.nVersion).c_str(),
                nAsset,
                txHash.ToString().c_str(),
                nHeight,
                fJustCheck ? 1 : 0);
    }
    return true;
}
bool CBlockIndexDB::FlushErase(const std::vector<uint256> &vecTXIDs){
    if(vecTXIDs.empty())
        return true;

    CDBBatch batch(*this);
    for (const uint256 &txid : vecTXIDs) {
        batch.Erase(txid);
    }
    LogPrint(BCLog::PDAY, "Flushing %d block index removals\n", vecTXIDs.size());
    return WriteBatch(batch);
}
bool CBlockIndexDB::FlushWrite(const std::vector<std::pair<uint256, uint256> > &blockIndex){
    if(blockIndex.empty())
        return true;
    CDBBatch batch(*this);
    for (const auto &pair : blockIndex) {
        batch.Write(pair.first, pair.second);
    }
    LogPrint(BCLog::PDAY, "Flush writing %d block indexes\n", blockIndex.size());
    return WriteBatch(batch);
}
bool CLockedOutpointsDB::FlushErase(const std::vector<COutPoint> &lockedOutpoints) {
	if (lockedOutpoints.empty())
		return true;

	CDBBatch batch(*this);
	for (const auto &outpoint : lockedOutpoints) {
		batch.Erase(outpoint);
	}
	LogPrint(BCLog::PDAY, "Flushing %d locked outpoints removals\n", lockedOutpoints.size());
	return WriteBatch(batch);
}
bool CLockedOutpointsDB::FlushWrite(const std::vector<COutPoint> &lockedOutpoints) {
	if (lockedOutpoints.empty())
		return true;
	CDBBatch batch(*this);
	int write = 0;
	int erase = 0;
	for (const auto &outpoint : lockedOutpoints) {
		if (outpoint.IsNull()) {
			erase++;
			batch.Erase(outpoint);
		}
		else {
			write++;
			batch.Write(outpoint, true);
		}
	}
	LogPrint(BCLog::PDAY, "Flushing %d locked outpoints (erased %d, written %d)\n", lockedOutpoints.size(), erase, write);
	return WriteBatch(batch);
}
bool CheckPaydayCoinLockedOutpoints(const CTransactionRef &tx, CValidationState& state) {
	// PAYDAYCOIN
	const CTransaction &myTx = (*tx);
	// if not an allocation send ensure the outpoint locked isn't being spent
	if (myTx.nVersion != PAYDAYCOIN_TX_VERSION_ASSET_ALLOCATION_SEND) {
		for (unsigned int i = 0; i < myTx.vin.size(); i++)
		{
			bool locked = false;
			// spending as non allocation send while using a locked outpoint should be invalid
			if (plockedoutpointsdb && plockedoutpointsdb->ReadOutpoint(myTx.vin[i].prevout, locked) && locked) {
                return state.Invalid(ValidationInvalidReason::TX_MISSING_INPUTS, false, REJECT_INVALID, "non-allocation-input");
			}
		}
	}
	// ensure that the locked outpoint is being spent
	else {

		CAssetAllocation theAssetAllocation(myTx);
		if (theAssetAllocation.assetAllocationTuple.IsNull()) {
            return state.Invalid(ValidationInvalidReason::TX_MISSING_INPUTS, false, REJECT_INVALID, "invalid-allocation");
		}
		CAssetAllocation assetAllocationDB;
		if (!GetAssetAllocation(theAssetAllocation.assetAllocationTuple, assetAllocationDB)) {
            return state.Invalid(ValidationInvalidReason::TX_MISSING_INPUTS, false, REJECT_INVALID, "non-existing-allocation");
		}
		bool found = assetAllocationDB.lockedOutpoint.IsNull();
        
		for (unsigned int i = 0; i < myTx.vin.size(); i++)
		{
			bool locked = false;
            if(!found)
                LogPrintf("found %d out match %d\n", found? 1: 0, assetAllocationDB.lockedOutpoint == myTx.vin[i].prevout? 1: 0);
            
			// spending as non allocation send while using a locked outpoint should be invalid
			if (!found && assetAllocationDB.lockedOutpoint == myTx.vin[i].prevout && plockedoutpointsdb && plockedoutpointsdb->ReadOutpoint(myTx.vin[i].prevout, locked) && locked) {
				found = true;
				break;
			}
		}
		if (!found) {
            return state.Invalid(ValidationInvalidReason::TX_MISSING_INPUTS, false, REJECT_INVALID, "missing-lockpoint");
		}
	}
	return true;
}
bool CEthereumTxRootsDB::PruneTxRoots(const uint32_t &fNewGethSyncHeight) {
    uint32_t fNewGethCurrentHeight = fGethCurrentHeight;
    boost::scoped_ptr<CDBIterator> pcursor(NewIterator());
    pcursor->SeekToFirst();
    vector<uint32_t> vecHeightKeys;
    uint32_t nKey = 0;
    uint32_t cutoffHeight = 0;
    if(fNewGethSyncHeight > 0)
    {
        const uint32_t &nCutoffHeight = MAX_ETHEREUM_TX_ROOTS*3;
        // cutoff to keep blocks is ~3 week of blocks is about 120k blocks
        cutoffHeight = fNewGethSyncHeight - nCutoffHeight;
        if(fNewGethSyncHeight < nCutoffHeight){
            LogPrint(BCLog::PDAY, "Nothing to prune fGethSyncHeight = %d\n", fNewGethSyncHeight);
            return true;
        }
    }
    std::vector<unsigned char> txPos;
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        try {
            if(pcursor->GetKey(nKey)){
                // if height is before cutoff height or after tip height passed in (re-org), remove the txroot from db
                if (fNewGethSyncHeight > 0 && (nKey < cutoffHeight || nKey > fNewGethSyncHeight)) {
                    vecHeightKeys.emplace_back(nKey);
                }
                else if(nKey > fNewGethCurrentHeight)
                    fNewGethCurrentHeight = nKey;
            }
            pcursor->Next();
        }
        catch (std::exception &e) {
            return error("%s() : deserialize error", __PRETTY_FUNCTION__);
        }
    }

    {
        LOCK(cs_ethsyncheight);
        fGethSyncHeight = fNewGethSyncHeight;
        fGethCurrentHeight = fNewGethCurrentHeight;
    }      
    return FlushErase(vecHeightKeys);
}
bool CEthereumTxRootsDB::Init(){
    return PruneTxRoots(0);
}
void CEthereumTxRootsDB::AuditTxRootDB(std::vector<std::pair<uint32_t, uint32_t> > &vecMissingBlockRanges){
    boost::scoped_ptr<CDBIterator> pcursor(NewIterator());
    pcursor->SeekToFirst();
    vector<uint32_t> vecHeightKeys;
    uint32_t nKey = 0;
    uint32_t nKeyIndex = 0;
    uint32_t nCurrentSyncHeight = 0;
    {
        LOCK(cs_ethsyncheight);
        nCurrentSyncHeight = fGethSyncHeight;
       
    }
    uint32_t nKeyCutoff = nCurrentSyncHeight - MAX_ETHEREUM_TX_ROOTS;
    if(nCurrentSyncHeight < MAX_ETHEREUM_TX_ROOTS)
        nKeyCutoff = 0;
    std::vector<unsigned char> txPos;
    std::map<uint32_t, EthereumTxRoot> mapTxRoots;
    
    // sort keys numerically
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        try {
            if(!pcursor->GetKey(nKey)){
                 pcursor->Next();
                 continue;
            }
            EthereumTxRoot txRoot;
            pcursor->GetValue(txRoot);
            mapTxRoots.emplace(nKey, txRoot);
            pcursor->Next();
        }
        catch (std::exception &e) {
            return;
        }
    } 
    while(mapTxRoots.size() < 2){
        vecMissingBlockRanges.emplace_back(make_pair(nKeyCutoff, nCurrentSyncHeight));
        return;
    }
    auto setIt = mapTxRoots.begin();
    nKeyIndex = setIt->first;
    setIt++;
    // we should have at least MAX_ETHEREUM_TX_ROOTS roots available from the tip for consensus checks
    if(nCurrentSyncHeight >= MAX_ETHEREUM_TX_ROOTS && nKeyIndex > nKeyCutoff){
        vecMissingBlockRanges.emplace_back(make_pair(nKeyCutoff, nKeyIndex-1));
    }
    std::vector<unsigned char> vchPrevHash;
    std::vector<uint32_t> vecRemoveKeys;
    // find sequence gaps in sorted key set 
    for (; setIt != mapTxRoots.end(); ++setIt){
            const uint32_t &key = setIt->first;
            const uint32_t &nNextKeyIndex = nKeyIndex+1;
            if (key != nNextKeyIndex && (key-1) >= nNextKeyIndex)
                vecMissingBlockRanges.emplace_back(make_pair(nNextKeyIndex, key-1));
            // if continious index we want to ensure hash chain is also continious
            else{
                // if prevhash of prev txroot != hash of this tx root then request inconsistent roots again
                const EthereumTxRoot &txRoot = setIt->second;
                auto prevRootPair = std::prev(setIt);
                const EthereumTxRoot &txRootPrev = prevRootPair->second;
                if(txRoot.vchPrevHash != txRootPrev.vchBlockHash){
                    // get a range of -50 to +50 around effected tx root to minimize chance that you will be requesting 1 root at a time in a long range fork
                    // this is fine because relayer fetches 100 headers at a time anyway
                    vecMissingBlockRanges.emplace_back(make_pair(std::max(0,(int32_t)key-50), std::min((int32_t)key+50, (int32_t)nCurrentSyncHeight)));
                    vecRemoveKeys.push_back(key);
                }
            }
            nKeyIndex = key;   
    } 
    if(!vecRemoveKeys.empty()){
        LogPrint(BCLog::PDAY, "Detected an %d inconsistent hash chains in Ethereum headers, removing...\n", vecRemoveKeys.size());
        pethereumtxrootsdb->FlushErase(vecRemoveKeys);
    }
}
bool CEthereumTxRootsDB::FlushErase(const std::vector<uint32_t> &vecHeightKeys){
    if(vecHeightKeys.empty())
        return true;
    const uint32_t &nFirst = vecHeightKeys.front();
    const uint32_t &nLast = vecHeightKeys.back();
    CDBBatch batch(*this);
    for (const auto &key : vecHeightKeys) {
        batch.Erase(key);
    }
    LogPrint(BCLog::PDAY, "Flushing, erasing %d ethereum tx roots, block range (%d-%d)\n", vecHeightKeys.size(), nFirst, nLast);
    return WriteBatch(batch);
}
bool CEthereumTxRootsDB::FlushWrite(const EthereumTxRootMap &mapTxRoots){
    if(mapTxRoots.empty())
        return true;
    const uint32_t &nFirst = mapTxRoots.begin()->first;
    uint32_t nLast = nFirst;
    CDBBatch batch(*this);
    for (const auto &key : mapTxRoots) {
        batch.Write(key.first, key.second);
        nLast = key.first;
    }
    LogPrint(BCLog::PDAY, "Flushing, writing %d ethereum tx roots, block range (%d-%d)\n", mapTxRoots.size(), nFirst, nLast);
    return WriteBatch(batch);
}
bool CEthereumMintedTxDB::FlushWrite(const EthereumMintTxVec &vecMintKeys){
    if(vecMintKeys.empty())
        return true;
    CDBBatch batch(*this);
    for (const auto &key : vecMintKeys) {
        batch.Write(key, true);
    }
    LogPrint(BCLog::PDAY, "Flushing, writing %d ethereum tx mints\n", vecMintKeys.size());
    return WriteBatch(batch);
}
bool CEthereumMintedTxDB::FlushErase(const EthereumMintTxVec &vecMintKeys){
    if(vecMintKeys.empty())
        return true;
    CDBBatch batch(*this);
    for (const auto &key : vecMintKeys) {
        batch.Erase(key);
    }
    LogPrint(BCLog::PDAY, "Flushing, erasing %d ethereum tx mints\n", vecMintKeys.size());
    return WriteBatch(batch);
}