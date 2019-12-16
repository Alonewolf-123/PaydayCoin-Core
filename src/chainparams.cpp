// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The PaydayCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>

#include <chainparamsseeds.h>
#include <consensus/merkle.h>
#include <tinyformat.h>
#include <util/system.h>
#include <util/strencodings.h>
#include <versionbitsinfo.h>

#include <assert.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
// PAYDAYCOIN includes for gen block
#include <uint256.h>
#include <arith_uint256.h>
#include <hash.h>
#include <streams.h>
#include <time.h>
static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}
// This will figure out a valid hash and Nonce if you're
// creating a different genesis block:
static void GenerateGenesisBlock(CBlockHeader &genesisBlock, uint256 &phash)
{
    arith_uint256 bnTarget;
    bnTarget.SetCompact(genesisBlock.nBits);
    uint32_t nOnce = 0;
    while (true) {
        genesisBlock.nNonce = nOnce;
        uint256 hash = genesisBlock.GetHash();
        if (UintToArith256(hash) <= bnTarget) {
            phash = hash;
            break;
        }
        nOnce++;
    }
    printf("genesis.nTime = %u \n", genesisBlock.nTime);
    printf("genesis.nNonce = %u \n", genesisBlock.nNonce);
    printf("Generate hash = %s\n", phash.ToString().c_str());
    printf("genesis.hashMerkleRoot = %s\n", genesisBlock.hashMerkleRoot.ToString().c_str());
}   
/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893, vtx=1)
 *   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
 *   vMerkleTree: 4a5e1e
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "The Times 12/01/2019 @ 8:10pm (UTC) worldpaycoin genesis block";
    const CScript genesisOutputScript = CScript() << ParseHex("0467c99a9ea089e2d37e657c10dd126cca11dd6b7f12e3fff06cddaed2404b29e4b976db40e2e714f8cb7518dd9352f0c608fee0eadb6545d9b15ae7949841e906") << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

 void CChainParams::TurnOffSegwitForUnitTests ()
{
  consensus.BIP16Height = 1000000;
}
/**
 * Main network
 */
class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = "main";
        consensus.nSubsidyHalvingInterval = 525600; 
        // 35% increase after 1 year, 100% increase after 2.5 years
        consensus.nSeniorityHeight1 = 525600; 
        consensus.nSeniorityLevel1 = 0.35;
        consensus.nSeniorityHeight2 = 525600 * 2.5; 
        consensus.nSeniorityLevel2 = 1.0;        
        consensus.nSuperblockStartBlock = 1;
        consensus.nSuperblockCycle = 43800;
        consensus.nGovernanceMinQuorum = 10; 
        consensus.nGovernanceFilterElements = 20000;
        consensus.nMasternodeMinimumConfirmations = 15;
        
        consensus.BIP16Height = 1;
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1; 
        consensus.BIP66Height = 1;
        consensus.powLimit = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 20
        consensus.nPowTargetTimespan = 6 * 60 * 60; // 6h retarget
        consensus.nPowTargetSpacing = 1 * 60; // PaydayCoin: 1 minute
        consensus.nAuxpowChainId = 0x1000;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1916; // 95% of 2016
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // CSV (BIP68, BIP112 and BIP113) as well as segwit (BIP141, BIP143 and
        // BIP147) are deployed together with P2SH.

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x000000000000000000000000000000000000000000000000000000000000000");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0xe44257e8e027e8a67fd647c54e1bd6976988d75b416affabe3f82fd87a67f5ff");
        consensus.nAuxpowChainId = 0x1000;
        consensus.nAuxpowStartHeight = 1;
        consensus.fStrictChainId = true;
        consensus.nLegacyBlocksBefore = 1;
        consensus.vchPDAYXContract = ParseHex("197a2f58c94eff4b2c6ae0922fbc840080f839c2");
        consensus.vchPDAYXBurnMethodSignature = ParseHex("285c5bc6");
        consensus.nBridgeStartBlock = 75000;
        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0xce;
        pchMessageStart[1] = 0xe2;
        pchMessageStart[2] = 0xca;
        pchMessageStart[3] = 0xff;
        nDefaultPort = 8123;
        nPruneAfterHeight = 100000;
        m_assumed_blockchain_size = 30;
        m_assumed_chain_state_size = 2;

        genesis = CreateGenesisBlock(1575231000, 372953, 0x1e0fffff, 1, 0 * COIN);
        
        // uint256 hash;
        // CBlockHeader genesisHeader = genesis.GetBlockHeader();
        // GenerateGenesisBlock(genesisHeader, hash);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x00000c5ab58c628aa9f88a45d9565b58bfcbe933d6de26c4b29eba2f5ee978a0"));
        assert(genesis.hashMerkleRoot == uint256S("0xfdb53c0a5de58902d33a26502d1740734dee0544d1edded070aaa6faa341a4cf"));

        vSeeds.emplace_back("pday001.usernodes.org");
        vSeeds.emplace_back("pday002.usernodes.org");
        vSeeds.emplace_back("pday003.usernodes.org");
        vSeeds.emplace_back("pday004.usernodes.org");
        
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,55);  // 'P'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,5);   // '7 or 8'
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,120); // 'q'
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

        bech32_hrp = "pday";
        
        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
        strSporkAddress = "PUSCmw9tasz7C9UnTbUkGvY6ob8hmjdn9U";    
        nFulfilledRequestExpireTime = 60*60; // fulfilled requests expire in 1 hour
        checkpointData = {
            {
                { 100, uint256S("0x00000676e3c383de040193f1e4c76aa53ca38c8c7acd55a2948b128491067048")},
                // { 5000, uint256S("0xeef3554a3f467bcdc7570f799cecdb262058cecf34d555827c99b5719b1df4f6")},
                // { 10000, uint256S("0xe44257e8e027e8a67fd647c54e1bd6976988d75b416affabe3f82fd87a67f5ff")},
            }
        };

        nMNCollateral = 15000 * COIN;
        nMNCollateralLevel1 = 15000 * COIN;
        nMNCollateralLevel2 = 8000 * COIN;
        nMNCollateralLevel3 = 6000 * COIN;
        nMNCollateralLevel4 = 4000 * COIN;

        nMasternodeForkBlock = 5000;
        chainTxData = ChainTxData{
            // Data from rpc: getchaintxstats 4096 0000000000000000000f1c54590ee18d15ec70e68c8cd4cfbadb1b4f11697eee
            /* nTime    */ 0,
            /* nTxCount */ 0,
            /* dTxRate  */ 0
        };

        m_fallback_fee_enabled = true;
    }
};

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = "test";
        consensus.nSubsidyHalvingInterval = 525600;
        // 35% increase after 1 hr, 100% increase after 2.5 hr
        consensus.nSeniorityHeight1 = 60;
        consensus.nSeniorityLevel1 = 0.35;
        consensus.nSeniorityHeight2 = 60*2.5;
        consensus.nSeniorityLevel2 = 1.0;         
        consensus.nSuperblockStartBlock = 1;
        consensus.nSuperblockCycle = 60;
        consensus.nGovernanceMinQuorum = 1;
        consensus.nGovernanceFilterElements = 500;
        consensus.nMasternodeMinimumConfirmations = 1;
        consensus.BIP16Height = 1;
        consensus.BIP34Height = 500; // BIP34 activated on regtest (Used in functional tests)
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1351; // BIP65 activated on regtest (Used in functional tests)
        consensus.BIP66Height = 1251; // BIP66 activated on regtest (Used in functional tests)
        
        consensus.powLimit = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 20
        consensus.nPowTargetTimespan = 6 * 60 * 60; // 6h retarget
        consensus.nPowTargetSpacing = 1 * 60; // PaydayCoin: 1 minute
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 999999999999ULL; // December 31, 2008
        // CSV (BIP68, BIP112 and BIP113) as well as segwit (BIP141, BIP143 and
        // BIP147) are deployed together with P2SH.

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x000000000000000000000000000000000000000000000000000000000000000");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x000000000000000000000000000000000000000000000000000000000000000"); //1354312
        consensus.nAuxpowStartHeight = 1;
        consensus.nAuxpowChainId = 0x1000;
        consensus.fStrictChainId = false;
        consensus.nLegacyBlocksBefore = 1;
        consensus.vchPDAYXContract = ParseHex("46c6accc790ebd002c022057a14a7d247d5e5d37");
        consensus.vchPDAYXBurnMethodSignature = ParseHex("285c5bc6");
        consensus.nBridgeStartBlock = 100;
        pchMessageStart[0] = 0xce;
        pchMessageStart[1] = 0xe2;
        pchMessageStart[2] = 0xca;
        pchMessageStart[3] = 0xfe;
        nDefaultPort = 18123;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 30;
        m_assumed_chain_state_size = 2;

        genesis = CreateGenesisBlock(1575231010, 2018250, 0x1e0fffff, 1, 0 * COIN);
        
        // uint256 hash;
        // CBlockHeader genesisHeader = genesis.GetBlockHeader();
        // GenerateGenesisBlock(genesisHeader, hash);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x00000274a8333fc92e19a96d13de667325416702743ef5d31698dca1912957e0"));
        assert(genesis.hashMerkleRoot == uint256S("0xfdb53c0a5de58902d33a26502d1740734dee0544d1edded070aaa6faa341a4cf"));

        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
        /*vSeeds.emplace_back("testnet-seed.paydaycoin.jonasschnelli.ch");
        vSeeds.emplace_back("seed.tpday.petertodd.org");
        vSeeds.emplace_back("seed.testnet.paydaycoin.sprovoost.nl");
        vSeeds.emplace_back("testnet-seed.bluematt.me"); // Just a static list of stable node(s), only supports x9
*/
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,65);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "tpday";



        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;

        // privKey: cU52TqHDWJg6HoL3keZHBvrJgsCLsduRvDFkPyZ5EmeMwoEHshiT
        strSporkAddress = "TCGpumHyMXC5BmfkaAQXwB7Bf4kbkhM9BX";
        nFulfilledRequestExpireTime = 5*60; // fulfilled requests expire in 5 minutes
        /*checkpointData = {
            {
                {546, uint256S("000000002a936ca763904c3c35fce2f3556c559c0214345d31b1bcebf76acb70")},
            }
        };*/

        chainTxData = ChainTxData{
            // Data from rpc: getchaintxstats 4096 0000000000000037a8cd3e06cd5edbfe9dd1dbcc5dacab279376ef7cfc2b4c75
            /* nTime    */ 0,
            /* nTxCount */ 0,
            /* dTxRate  */ 0
        };

        /* enable fallback fee on testnet */
        m_fallback_fee_enabled = true;
    }
};

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
    explicit CRegTestParams(const ArgsManager& args) {
        strNetworkID = "regtest";
        consensus.nSubsidyHalvingInterval = 150;
        // 35% increase after 1 hr, 100% increase after 2.5 hr
        consensus.nSeniorityHeight1 = 60;
        consensus.nSeniorityLevel1 = 0.35;
        consensus.nSeniorityHeight2 = 60*2.5;
        consensus.nSeniorityLevel2 = 1.0;      
        consensus.nSuperblockStartBlock = 1;
        consensus.nSuperblockCycle = 10;
        consensus.nGovernanceMinQuorum = 1;
        consensus.nGovernanceFilterElements = 100;
        consensus.nMasternodeMinimumConfirmations = 1;
        consensus.BIP16Height = 1;
        consensus.BIP34Height = 1;
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 6 * 60 * 60; // PaydayCoin: 6 hour
        consensus.nPowTargetSpacing = 1 * 60; // PaydayCoin: 1 minute
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        // CSV (BIP68, BIP112 and BIP113) as well as segwit (BIP141, BIP143 and
        // BIP147) are deployed together with P2SH.

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");
        consensus.nAuxpowStartHeight = 0;
        consensus.nAuxpowChainId = 0x1000;
        consensus.fStrictChainId = true;
        consensus.nLegacyBlocksBefore = 0;
        consensus.vchPDAYXContract = ParseHex("5f6e74ba20bf26161612eac8f7e8b3b6c9baaadd");
        consensus.vchPDAYXBurnMethodSignature = ParseHex("285c5bc6");
        consensus.nBridgeStartBlock = 100;
        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xda;
        nDefaultPort = 18000;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 0;
        m_assumed_chain_state_size = 0;

        UpdateVersionBitsParametersFromArgs(args);

        genesis = CreateGenesisBlock(1575231020, 0, 0x207fffff, 1, 0 * COIN);
        
        // uint256 hash;
        // CBlockHeader genesisHeader = genesis.GetBlockHeader();
        // GenerateGenesisBlock(genesisHeader, hash);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x13358f48b8d6fda98402542b42b645ccecf71d78646da7f63db9992de8a0ab30"));
        assert(genesis.hashMerkleRoot == uint256S("0xfdb53c0a5de58902d33a26502d1740734dee0544d1edded070aaa6faa341a4cf"));

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;
        // privKey: cPPpaK9LCXjGGXVJUqcrtEMVQw5tALMuN3WsVuPCWFf9tswYYDvY
        strSporkAddress = "TCSJVL68KFq9FdbfxB2KhTcWp6rHD7vePs";
        nFulfilledRequestExpireTime = 5*60; // fulfilled requests expire in 5 minutes
       /* checkpointData = {
            {
                {0, uint256S("0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206")},
            }
        };*/

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,65);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "pdayrt";
        
        /* enable fallback fee on regtest */
        m_fallback_fee_enabled = true;
    }

    /**
     * Allows modifying the Version Bits regtest parameters.
     */
    void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
    {
        consensus.vDeployments[d].nStartTime = nStartTime;
        consensus.vDeployments[d].nTimeout = nTimeout;
    }
    void UpdateVersionBitsParametersFromArgs(const ArgsManager& args);
};

void CRegTestParams::UpdateVersionBitsParametersFromArgs(const ArgsManager& args)
{
    if (!args.IsArgSet("-vbparams")) return;

    for (const std::string& strDeployment : args.GetArgs("-vbparams")) {
        std::vector<std::string> vDeploymentParams;
        boost::split(vDeploymentParams, strDeployment, boost::is_any_of(":"));
        if (vDeploymentParams.size() != 3) {
            throw std::runtime_error("Version bits parameters malformed, expecting deployment:start:end");
        }
        int64_t nStartTime, nTimeout;
        if (!ParseInt64(vDeploymentParams[1], &nStartTime)) {
            throw std::runtime_error(strprintf("Invalid nStartTime (%s)", vDeploymentParams[1]));
        }
        if (!ParseInt64(vDeploymentParams[2], &nTimeout)) {
            throw std::runtime_error(strprintf("Invalid nTimeout (%s)", vDeploymentParams[2]));
        }
        bool found = false;
        for (int j=0; j < (int)Consensus::MAX_VERSION_BITS_DEPLOYMENTS; ++j) {
            if (vDeploymentParams[0] == VersionBitsDeploymentInfo[j].name) {
                UpdateVersionBitsParameters(Consensus::DeploymentPos(j), nStartTime, nTimeout);
                found = true;
                LogPrintf("Setting version bits activation parameters for %s to start=%ld, timeout=%ld\n", vDeploymentParams[0], nStartTime, nTimeout);
                break;
            }
        }
        if (!found) {
            throw std::runtime_error(strprintf("Invalid deployment (%s)", vDeploymentParams[0]));
        }
    }
}

static std::unique_ptr<const CChainParams> globalChainParams;

const CChainParams &Params() {
    assert(globalChainParams);
    return *globalChainParams;
}

std::unique_ptr<const CChainParams> CreateChainParams(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
        return std::unique_ptr<CChainParams>(new CMainParams());
    else if (chain == CBaseChainParams::TESTNET)
        return std::unique_ptr<CChainParams>(new CTestNetParams());
    else if (chain == CBaseChainParams::REGTEST)
        return std::unique_ptr<CChainParams>(new CRegTestParams(gArgs));
    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(network);
}
void TurnOffSegwitForUnitTests ()
{
  /* TODO: It is ugly that we need a const-cast here, but this is only for
     unit testing.  Upstream avoids this by turning off segwit through
     forcing command-line args in the tests.  For that to work in our case,
     we would have to have an explicit argument for BIP16.  */
  auto* params = const_cast<CChainParams*> (globalChainParams.get ());
  params->TurnOffSegwitForUnitTests ();
}
