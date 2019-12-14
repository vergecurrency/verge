// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <consensus/merkle.h>

#include <tinyformat.h>
#include <util/system.h>
#include <util/strencodings.h>

#include <assert.h>

#include <chainparamsseeds.h>
#include <arith_uint256.h>

static CBlock CreateGenesisBlock(const char* pszTimestamp, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.nTime = nTime;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].SetEmpty();

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

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, bool isTestnet = false)
{
    const char* pszTimestamp = isTestnet ? "VERGE TESTNET" : "Name: Dogecoin Dark";
    return CreateGenesisBlock(pszTimestamp, nTime, nNonce, nBits, nVersion);
}

void CChainParams::UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    consensus.vDeployments[d].nStartTime = nStartTime;
    consensus.vDeployments[d].nTimeout = nTimeout;
}

/**
 * Main network
 */
/**
 * What makes a good checkpoint block?
 * + Is surrounded by blocks with reasonable timestamps
 *   (no blocks before with a timestamp after, none after with
 *    timestamp before)
 * + Contains no strange transactions
 */

class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = "main";
        consensus.ForkHeight = 2800000;
        consensus.MULTI_ALGO_SWITCH_BLOCK = 340000;
        consensus.STEALTH_TX_SWITCH_BLOCK = 1824150;
        consensus.FlexibleMiningAlgorithms = 2042000;
        consensus.CLOCK_DRIFT_FORK = 2218500;
        consensus.nSubsidyHalvingInterval = 500000;
		
        consensus.BIP34Height = consensus.ForkHeight;
        consensus.BIP65Height = consensus.ForkHeight;
        consensus.BIP66Height = consensus.ForkHeight; 


        consensus.powLimit = uint256S("00000fffff000000000000000000000000000000000000000000000000000000"); //ArithToUint256(~arith_uint256(0) >> 20);
        consensus.nPowTargetTimespan = 30; // diff readjusting time
        consensus.nPowTargetSpacing = 30; //
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 100; // 100 out of 200 blocks
        consensus.nMinerConfirmationWindow = 200;
        
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1529247969; // May 1st, 2016
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1559347200; // May 1st, 2017

        // Deployment of SegWit (BIP141, BIP143, and BIP147)
        // It is all safe :thumbsup:
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 1529247969; // November 15th, 2016.
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 1559347200; // November 15th, 2017.

        // The best chain should have at least this much work. 
        // KeyNote: (Kind of like a checkpoint)
        consensus.nMinimumChainWork = uint256S("0x000000000000000000000000000000000000000000000000000000222e000001");

        // By default assume that the signatures in ancestors of this block are valid.
        // KeyNote: Seems like speedup, similar to a checkpoint
        consensus.defaultAssumeValid = uint256S("0xc766387a2e0cd6af995ea432518614824fe313e988598ea8b26f58efb99ebcdc"); // block 1,100,000

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0xf7; 
        pchMessageStart[1] = 0xa7;
        pchMessageStart[2] = 0x7e;
        pchMessageStart[3] = 0xff;
        nDefaultPort = 21102;
        nPruneAfterHeight = 100000;

        genesis = CreateGenesisBlock(1412878964, 1473191, 0x1e0fffff, 1);
        consensus.hashGenesisBlock = genesis.GetHash();
        
        assert(genesis.hashMerkleRoot == uint256S("0x1c83275d9151711eec3aec37d829837cc3c2730b2bdfd00ec5e8e5dce675fd00"));
        assert(consensus.hashGenesisBlock == uint256S("0x00000fc63692467faeb20cdb3b53200dc601d75bdfa1001463304cc790d77278"));

        // Note that of those which support the service bits prefix, most only support a subset of
        // possible options.
        // This is fine at runtime as we'll fall back to using them as a oneshot if they don't support the
        // service bits we want, but we should get them updated to support all service bits wanted by any
        // release ASAP to avoid it where possible.

        // Adding some nodes in case it works

        if (!gArgs.IsArgSet("-without-tor")) {
            vSeeds.emplace_back("aizckfmksnk54gqcg5fur22cz22vzut2ig7cexz45pfm7witswzawcqd.onion"); //v6 SUNEROK
            vSeeds.emplace_back("jdaootwqo25qcfq3qjcantgfwlilzzc72e6ormt6t3gnafeacjzgd3qd.onion"); //v6 SUNEROK
            vSeeds.emplace_back("ry7yyqv4oeu2kjygco5xvcvi2sgr5bxkynvh6yctftghwe2jy6w2wkqd.onion"); //v6 SUNEROK
            vSeeds.emplace_back("f3cy2jkmfl3qzqpk4srruhpefkunq3rj6wi7z57yzinyxpk3rmbluoad.onion"); //v6 SUNEROK
            vSeeds.emplace_back("25hj4mdmslzvu2lngo4jcsbrouwsdktv6ps3xifipju74oqyoply6qad.onion"); //v6 SUNEROK
            vSeeds.emplace_back("amafdqgkmtbkld45kaal5cwwfbrsgnimw77gawwltsaklgxxlubjvhid.onion"); //v6 SUNEROK
            vSeeds.emplace_back("n7rk4xqurrvedhhghkkvz2pmxalgmoviokgjiwjgcvpcxa6piym5m2ad.onion"); //v6 SUNEROK
            vSeeds.emplace_back("rje6q245yhiyn4setn5abjlcqwapxzgwbfksrscexyhv7ffjdasmvaqd.onion"); //v6 SUNEROK
	    vSeeds.emplace_back("wu6bebmvs6jw2t6lmjg3aako5synunxm4i3zb7bybvzrvxe277achfad.onion"); //v6 SUNEROK
	    vSeeds.emplace_back("jvnd4gjzhmuiaph5xft6ju4twt5gjmpbqdc2sis7gd2pnnqpwwpqz5id.onion"); //v6 SUNEROK
	    vSeeds.emplace_back("wuoki3rq3htqmtrmrtkqtnxzgfsgclwj7jrxk35abkwksni2asdepzyd.onion"); //v6 SUNEROK
	    vSeeds.emplace_back("3czrw4cxofudpyhl46whvcyzgmkciaai2uw7s6motxejb3fijdkgmuqd.onion"); //v6 SUNEROK
	    vSeeds.emplace_back("7wg3vv67i5m3uiectqw4taj5pm3qmxnlgyb7ks7e5oilo62g7mxn2zqd.onion"); //v6 SUNEROK
	    vSeeds.emplace_back("y2y2cr6h4dn3g5l4545mj7dw245i3fsuixx6he6r4w4aqh3j23pwqoid.onion"); //v6 SUNEROK
            vSeeds.emplace_back("jno3zpfsdgrtdaxlyuowipokbumoidyecmczrjru7tjfovfjkztl2pad.onion"); //v6 Swat
            vSeeds.emplace_back("763co2copdnav2ik2jlq33wzj2rogt4wfexjvukx5tg3tiepm2ahbrid.onion"); //v6 Swat
            vSeeds.emplace_back("yvvioyzj3w5k6z64urv55xuh65oftcy5wts4tgwpi6hxdeixamsn63qd.onion"); //v6 Marplez SGP
            vSeeds.emplace_back("zwvqhwne3mlefxd52q35cckzdo46uhvcuvl7qdzqlatgpfqe4r43suid.onion"); //v6 Marplez AMS
            vSeeds.emplace_back("ojmwnkopgmpotm2byx7vsm7xplqxhctutw3d3ve7k5mbybsuydfk46qd.onion"); //v6 Marplez NYC
        } else {
            vSeeds.emplace_back("seed.marpmedev.xyz"); // marples DNS seed (v4, v5)
            vSeeds.emplace_back("seed.verge.dev"); // additional DNS seed
            vSeeds.emplace_back("185.162.9.97");
            vSeeds.emplace_back("159.89.46.252"); // v6-new-york
            vSeeds.emplace_back("139.59.34.170"); // v6-india
            vSeeds.emplace_back("134.209.197.243"); // v6-NL
            vSeeds.emplace_back("104.131.144.82");
            vSeeds.emplace_back("192.241.187.222");
            vSeeds.emplace_back("105.228.198.44");
            vSeeds.emplace_back("46.127.57.167");
            vSeeds.emplace_back("98.5.123.15");
            vSeeds.emplace_back("81.147.68.236");
            vSeeds.emplace_back("77.67.46.100");
            vSeeds.emplace_back("95.46.99.96");
            vSeeds.emplace_back("138.201.91.159");
            vSeeds.emplace_back("159.89.202.56");
            vSeeds.emplace_back("163.158.20.118");
            vSeeds.emplace_back("99.45.88.147");
            vSeeds.emplace_back("114.145.237.35");
            vSeeds.emplace_back("73.247.117.99");
            vSeeds.emplace_back("145.239.0.122");
        }
    
    

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,30);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,33);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,158); //128 + PUBKEY_ADDRESS
        base58Prefixes[EXT_PUBLIC_KEY] = {0x02, 0x2D, 0x25, 0x33};
        base58Prefixes[EXT_SECRET_KEY] = {0x02, 0x21, 0x31, 0x2B};

        bech32_hrp = "vg";

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;

        checkpointData = {
            {
				{ 0, uint256S("0x00000fc63692467faeb20cdb3b53200dc601d75bdfa1001463304cc790d77278")},
                { 15000, uint256S("0x000000000265c5f4683b169a68cb3cac89287c8b5df94e17b09ef19ac718026b")},
                { 100000, uint256S("0x000000000400a93131a94ad193c63faafeb8dfcc0c7d0e6f1c9c2614cb2823eb")},
                { 244219, uint256S("0x000000000139613d26f7436ecc568feb566c22d9a664359e53f0d0a542d5bdba")},
                { 400000, uint256S("0x0000000001d45af6613024ad5668bfa4909ac63e2b29c28042013d77a216830d")},
                { 500000, uint256S("0x0000000003700a4e9d81a67036d7647361086527e985cdf764648c5e61d07303")},
                { 600000, uint256S("0x30fa1eab961c99f6222f9925a27136c34ea27182c92e4f8c48ea3a90c7c2eb25")},
                { 700000, uint256S("0x3e4f3319706870bb149d1a976202c2a5e973384d181a600e7be59cbab5b63132")},
                { 800000, uint256S("0xf6b5f222bcc2f4e2439ccf6050d4ea3e9517c3752c3247302f039822ac9cc870")},
                { 900000, uint256S("0xc4d8b4079da888985854eda0200fb57045c2c70b29f10e98543f7c4076129e91")},
                {1000000, uint256S("0x000000000049eaba3d6c29d9f45bc2a944b46eec005e2b038f1ee924f2f9c029")},
                {1100000, uint256S("0xc766387a2e0cd6af995ea432518614824fe313e988598ea8b26f58efb99ebcdc")},
            }
        };

        checkpointIndexData = {
            {
                {0, uint256S("0x00000fc63692467faeb20cdb3b53200dc601d75bdfa1001463304cc790d77278")},
                {100000, uint256S("0x000000000400a93131a94ad193c63faafeb8dfcc0c7d0e6f1c9c2614cb2823eb")},
                {200000, uint256S("0x000000000010dd127bfe9f6b8c36080c45bcd352151aa0d2a4c79c9eba1e05b6")},
                {300000, uint256S("0x00000001eeb1cbdf539452ad9ae38c3c7bb481a246dc7464d22bf6272be0bc85")},
                {400000, uint256S("0x0000000001d45af6613024ad5668bfa4909ac63e2b29c28042013d77a216830d")},
                {500000, uint256S("0x0000000003700a4e9d81a67036d7647361086527e985cdf764648c5e61d07303")},
                {600000, uint256S("0x30fa1eab961c99f6222f9925a27136c34ea27182c92e4f8c48ea3a90c7c2eb25")},
                {700000, uint256S("0x3e4f3319706870bb149d1a976202c2a5e973384d181a600e7be59cbab5b63132")},
                {800000, uint256S("0xf6b5f222bcc2f4e2439ccf6050d4ea3e9517c3752c3247302f039822ac9cc870")},
                {900000, uint256S("0xc4d8b4079da888985854eda0200fb57045c2c70b29f10e98543f7c4076129e91")},
                {1000000, uint256S("0x000000000049eaba3d6c29d9f45bc2a944b46eec005e2b038f1ee924f2f9c029")},
                {1100000, uint256S("0xc766387a2e0cd6af995ea432518614824fe313e988598ea8b26f58efb99ebcdc")},
                {1200000, uint256S("0x8f35735055fbc08699daee957ec2c454ddf32a11accb24381d1b2b777dab56f9")},
                {1300000, uint256S("0xc7ae4450513571d7d5c307a6bdda2ecab5bf9c4321548ab1ea352d445cab28c4")},
                {1400000, uint256S("0x0f683608995591e3f78b59d1ea7b006bf689a9ea879890d717991aea7c8333bb")},
                {1500000, uint256S("0xf56cdd06d6eb7b159a7348bcfb918982c1106906be1f13f67629032924105860")},
                {1600000, uint256S("0x860fadffd552bd1b1c015e7d82e3bf25d8a695ac2658acaf7b171d61592888cd")},
                {1700000, uint256S("0x000000000008634e0d7beb20188b807eeb7597a6038334ef1385aebe5c4daa33")},
                {1800000, uint256S("0xfcfc1cb1bb408f3744f9102510c09224065a8d4046ff92e407b059bd4e5502e1")},
                {1900000, uint256S("0x1033b868e71679e4e8f7e6db2b785707bbc063095b283bbc2c236fcf1c512acc")},
                {2000000, uint256S("0x8457cf90dd4717a2049e737fc3e518057b0c41d61eb8b2e2a67a1707a4cec5d9")},
                {2100000, uint256S("0x0000000000006034a0a84252a3de77aea8d955cd90eb7c9f090e38a21d038a42")},
                {2200000, uint256S("0x00000037f9d28dfe25a6bdf681ec8f51198b0503d084f654fba72ee70f75904a")},
                {2300000, uint256S("0xe4334379b84be2ac4b2118a15323cb414bedf26a1dfce2c591b33e6dca643b92")},
                {2400000, uint256S("0x000000000000b9914ebfc5e69fe731572c5f110a9f9679316cb77dd5cd9e247f")},
                {2500000, uint256S("0xff122f47bff95423f64d2167401192ee336a6c51b0c560afa8a9ea16ee81d886")},
                {2600000, uint256S("0x20af6c9020b36967944f7dcacf4b7a49ec56c068c681f6170614585abd3de6cf")},
                {2700000, uint256S("0x49fa4075fa2396bf797818e172b3b5702a39d8318606a0ee01c5d06555e65bf6")},
            }
        };

        // FIXME: need to know the amount of transactions
        chainTxData = ChainTxData{
            // Data as of block 0000000000000000002d6cca6761c99b3c2e936f9a0e304b7c7651a993f461de (height 506081).
            1412878964, // * UNIX timestamp of last known number of transactions
            1,  // * total number of transactions between genesis and that timestamp
                        //   (the tx=... number in the SetBestChain debug.log lines)
            1         // * estimated number of transactions per second after that timestamp
        };

        /* disable fallback fee on mainnet */
        m_fallback_fee_enabled = false;
    }
};

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        // KeyNote: we'll leave testnet as is for now
        strNetworkID = "test";
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.MULTI_ALGO_SWITCH_BLOCK = 340000;
        consensus.STEALTH_TX_SWITCH_BLOCK = 1824150;
        consensus.FlexibleMiningAlgorithms = 2042000;
        consensus.CLOCK_DRIFT_FORK = 2218500;
        
        consensus.BIP34Height = 0;
        consensus.BIP65Height = 0; // 00000000007f6655f22f98e72ed80d8b06dc761d5da09df0fa1dc4be4f861eb6
        consensus.BIP66Height = 0; // 000000002104c8c45e99a8853285a3b592602a3ccde2b832481da85e9e4ba182
        consensus.powLimit = uint256S("00000fffff000000000000000000000000000000000000000000000000000000"); // FIXME: change to actual genesis limit
        consensus.nPowTargetTimespan = 24 * 60 * 60; // 1 day
        consensus.nPowTargetSpacing = 45;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1456790400; // March 1st, 2016
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1493596800; // May 1st, 2017

        // Deployment of SegWit (BIP141, BIP143, and BIP147)
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 1462060800; // May 1st 2016
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 1493596800; // May 1st 2017

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x000000000000000000000000000000000000000000000000000000000000000f");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x65b4e101cacf3e1e4f3a9237e3a74ffd1186e595d8b78fa8ea22c21ef5bf9347"); //also genesis 

        pchMessageStart[0] = 0xcd;
        pchMessageStart[1] = 0xf2;
        pchMessageStart[2] = 0xc0;
        pchMessageStart[3] = 0xef;
        nDefaultPort = 21104;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1462058066, 2, 0x1e0fffff, 1, true);
        consensus.hashGenesisBlock = genesis.GetHash();

        assert(consensus.hashGenesisBlock == uint256S("0x65b4e101cacf3e1e4f3a9237e3a74ffd1186e595d8b78fa8ea22c21ef5bf9347"));
        assert(genesis.hashMerkleRoot == uint256S("0x768cc22f70bbcc4de26f83aca1b4ea2a7e25f0d100497ba47c7ff2d9b696414c"));

        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
        vSeeds.emplace_back("2l7hxpeyhmy4c2tnlmgf7rgcn6epsaaspv7473f3r5uncqzs6pnltqqd.onion");
        vSeeds.emplace_back("qwwqi7h6bkkcw6clp34ttg5mxmcwerkot2hepfhi6g4yclvlhsv2kxid.onion");
        vSeeds.emplace_back("2l7hxpeyhmy4c2tnlmgf7rgcn6epsaaspv7473f3r5uncqzs6pnltqqd.onion");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,115);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,198);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,243); // 128 + PUBKEY_ADDRESS_TEST
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "vt";

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;


        checkpointData = {
            {
                {0, uint256S("000000002a936ca763904c3c35fce2f3556c559c0214345d31b1bcebf76acb70")},
            }
        };

        chainTxData = ChainTxData{
            // Data as of block 000000000000033cfa3c975eb83ecf2bb4aaedf68e6d279f6ed2b427c64caff9 (height 1260526)
            1462058066,
            1,
            0.1
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
    CRegTestParams() {
        strNetworkID = "regtest";
        consensus.nSubsidyHalvingInterval = 150;
        consensus.BIP34Height = 100000000; // BIP34 has not activated on regtest (far in the future so block v1 are not rejected in tests)
        consensus.BIP65Height = 1351; // BIP65 activated on regtest (Used in rpc activation tests)
        consensus.BIP66Height = 1251; // BIP66 activated on regtest (Used in rpc activation tests)
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 7 * 24 * 60 * 60; // one week
        consensus.nPowTargetSpacing = 30;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");

        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xda;
        nDefaultPort = 8333;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1462058066, 2, 0x1e0fffff, 1);
        consensus.hashGenesisBlock = genesis.GetHash();
        //assert(consensus.hashGenesisBlock == uint256S("0x65b4e101cacf3e1e4f3a9237e3a74ffd1186e595d8b78fa8ea22c21ef5bf9347"));
        //assert(genesis.hashMerkleRoot == uint256S("0x768cc22f70bbcc4de26f83aca1b4ea2a7e25f0d100497ba47c7ff2d9b696414c"));

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;

        checkpointData = {
            {
                {0, uint256S("0x65b4e101cacf3e1e4f3a9237e3a74ffd1186e595d8b78fa8ea22c21ef5bf9347")},
            }
        };

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "vgrt";

        /* enable fallback fee on regtest */
        m_fallback_fee_enabled = true;
    }
};

static std::unique_ptr<CChainParams> globalChainParams;

const CChainParams &Params() {
    assert(globalChainParams);
    return *globalChainParams;
}

std::unique_ptr<CChainParams> CreateChainParams(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
        return std::unique_ptr<CChainParams>(new CMainParams());
    else if (chain == CBaseChainParams::TESTNET)
        return std::unique_ptr<CChainParams>(new CTestNetParams());
    else if (chain == CBaseChainParams::REGTEST)
        return std::unique_ptr<CChainParams>(new CRegTestParams());
    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(network);
}

void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    globalChainParams->UpdateVersionBitsParameters(d, nStartTime, nTimeout);
}
