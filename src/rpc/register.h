// Copyright (c) 2009-2018 The PaydayCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAYDAYCOIN_RPC_REGISTER_H
#define PAYDAYCOIN_RPC_REGISTER_H

/** These are in one header file to avoid creating tons of single-function
 * headers for everything under src/rpc/ */
class CRPCTable;

/** Register block chain RPC commands */
void RegisterBlockchainRPCCommands(CRPCTable &tableRPC);
/** Register P2P networking RPC commands */
void RegisterNetRPCCommands(CRPCTable &tableRPC);
/** Register miscellaneous RPC commands */
void RegisterMiscRPCCommands(CRPCTable &tableRPC);
/** Register mining RPC commands */
void RegisterMiningRPCCommands(CRPCTable &tableRPC);
/** Register raw transaction RPC commands */
void RegisterRawTransactionRPCCommands(CRPCTable &tableRPC);
// PAYDAYCOIN
/** Register PaydayCoin Asset RPC commands */
void RegisterAssetRPCCommands(CRPCTable &tableRPC);
/** Register governance RPC commands */
void RegisterGovernanceRPCCommands(CRPCTable &tableRPC);
static inline void RegisterAllCoreRPCCommands(CRPCTable &t)
{
    RegisterBlockchainRPCCommands(t);
    RegisterNetRPCCommands(t);
    RegisterMiscRPCCommands(t);
    RegisterMiningRPCCommands(t);
    RegisterRawTransactionRPCCommands(t);
    // PAYDAYCOIN
    RegisterAssetRPCCommands(t);
    RegisterGovernanceRPCCommands(t);
}

#endif // PAYDAYCOIN_RPC_REGISTER_H
