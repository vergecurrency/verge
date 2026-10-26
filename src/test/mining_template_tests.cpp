// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <consensus/merkle.h>
#include <consensus/validation.h>
#include <core_io.h>
#include <miner.h>
#include <net.h>
#include <net_processing.h>
#include <pow.h>
#include <rpc/server.h>
#include <script/standard.h>
#include <streams.h>
#include <test/setup_common.h>
#include <util/strencodings.h>
#include <validation.h>
#include <validationinterface.h>

#include <boost/test/unit_test.hpp>

namespace {
struct MiningTemplateSetup : RegTestingSetup {
    const int original_activation{Params().GetConsensus().FlexibleMiningAlgorithms};
    CKey key;
    CBasicKeyStore keystore;

    MiningTemplateSetup()
    {
        const_cast<Consensus::Params&>(Params().GetConsensus()).FlexibleMiningAlgorithms = 0;
        key.MakeNewKey(true);
        keystore.AddKey(key);
        CAddress address(CService(), NODE_NETWORK);
        auto* node = new CNode(0, NODE_NETWORK, 0, INVALID_SOCKET, address, 0, 0, CAddress(), "", false);
        node->SetSendVersion(PROTOCOL_VERSION);
        peerLogic->InitializeNode(node);
        node->nVersion = 1;
        node->fSuccessfullyConnected = true;
        CConnmanTest::AddNode(*node);
    }

    ~MiningTemplateSetup()
    {
        SyncWithValidationInterfaceQueue();
        bool update_connection_time = false;
        peerLogic->FinalizeNode(0, update_connection_time);
        CConnmanTest::ClearNodes();
        const_cast<Consensus::Params&>(Params().GetConsensus()).FlexibleMiningAlgorithms = original_activation;
    }

    void Finalize(CBlock& block)
    {
        block.hashMerkleRoot = BlockMerkleRoot(block);
        while (!CheckProofOfWork(block.GetPoWHash(block.GetAlgo()), block.nBits, Params().GetConsensus())) {
            ++block.nNonce;
        }
        BOOST_REQUIRE(SignBlock(block, keystore));
    }

    void Mine(int algorithm)
    {
        auto block_template = BlockAssembler(Params()).CreateNewBlock(GetScriptForRawPubKey(key.GetPubKey()), algorithm, false);
        CBlock block = block_template->block;
        Finalize(block);
        BOOST_REQUIRE(ProcessNewBlock(Params(), std::make_shared<const CBlock>(block), true, nullptr));
        BOOST_REQUIRE(chainActive.Tip()->GetBlockHash() == block.GetHash());
    }

    UniValue GetTemplate(int algorithm)
    {
        JSONRPCRequest request;
        request.strMethod = "getblocktemplate";
        request.params = UniValue(UniValue::VARR);
        request.params.push_back(UniValue(UniValue::VOBJ));
        request.params.push_back(GetAlgoName(algorithm));
        return tableRPC["getblocktemplate"]->actor(request);
    }

    void CheckAlgorithm(int algorithm)
    {
        const int other = (algorithm + 1) % NUM_ALGOS;
        Mine(other);
        for (int i = 0; i < 10; ++i) {
            Mine(i % 2 == 0 || i == 9 ? algorithm : other);
        }
        const uint256 tip = chainActive.Tip()->GetBlockHash();
        BOOST_CHECK(GetTemplate(algorithm)["mining_disabled"].get_bool());
        BOOST_CHECK(GetTemplate(algorithm)["mining_disabled"].get_bool());
        BOOST_CHECK(!GetTemplate(other)["mining_disabled"].get_bool());
        BOOST_CHECK(GetTemplate(algorithm)["mining_disabled"].get_bool());

        const CScript payout = GetScriptForRawPubKey(key.GetPubKey());
        BOOST_CHECK_THROW(BlockAssembler(Params()).CreateNewBlock(payout, algorithm, false), std::runtime_error);
        auto paused = BlockAssembler(Params()).CreateNewBlock(payout, algorithm, false, true);
        CBlock block = paused->block;
        Finalize(block);
        {
            LOCK(cs_main);
            CValidationState state;
            BOOST_CHECK(!TestBlockValidity(state, Params(), block, chainActive.Tip(), false, true, false));
            BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-blk-algorithm");

            CValidationState template_state;
            BOOST_CHECK(TestBlockValidity(template_state, Params(), block, chainActive.Tip(), false, true, false, false));

            CBlock overpaid = block;
            CMutableTransaction coinbase(*overpaid.vtx[0]);
            ++coinbase.vout[0].nValue;
            overpaid.vtx[0] = MakeTransactionRef(std::move(coinbase));
            overpaid.hashMerkleRoot = BlockMerkleRoot(overpaid);
            CValidationState invalid_state;
            BOOST_CHECK(!TestBlockValidity(invalid_state, Params(), overpaid, chainActive.Tip(), false, true, false, false));
            BOOST_CHECK_EQUAL(invalid_state.GetRejectReason(), "bad-cb-amount");
        }

        JSONRPCRequest proposal;
        proposal.strMethod = "getblocktemplate";
        proposal.params = UniValue(UniValue::VARR);
        UniValue options(UniValue::VOBJ);
        options.pushKV("mode", "proposal");
        CDataStream serialized_block(SER_NETWORK, PROTOCOL_VERSION);
        serialized_block << block;
        options.pushKV("data", HexStr(serialized_block));
        proposal.params.push_back(options);
        BOOST_CHECK_EQUAL(tableRPC["getblocktemplate"]->actor(proposal).get_str(), "bad-blk-algorithm");

        CValidationState header_state;
        BOOST_CHECK(!ProcessNewBlockHeaders({block.GetBlockHeader()}, header_state, Params()));
        BOOST_CHECK_EQUAL(header_state.GetRejectReason(), "bad-blk-algorithm");
        BOOST_CHECK(!ProcessNewBlock(Params(), std::make_shared<const CBlock>(block), true, nullptr));
        BOOST_CHECK(chainActive.Tip()->GetBlockHash() == tip);

        Mine(other);
        BOOST_CHECK(!GetTemplate(algorithm)["mining_disabled"].get_bool());
        Mine(algorithm);
    }
};
}

BOOST_FIXTURE_TEST_SUITE(mining_template_tests, MiningTemplateSetup)

BOOST_AUTO_TEST_CASE(scrypt_pause_and_recovery) { CheckAlgorithm(ALGO_SCRYPT); }
BOOST_AUTO_TEST_CASE(groestl_pause_and_recovery) { CheckAlgorithm(ALGO_GROESTL); }
BOOST_AUTO_TEST_CASE(x17_pause_and_recovery) { CheckAlgorithm(ALGO_X17); }
BOOST_AUTO_TEST_CASE(blake_pause_and_recovery) { CheckAlgorithm(ALGO_BLAKE); }
BOOST_AUTO_TEST_CASE(lyra2re_pause_and_recovery) { CheckAlgorithm(ALGO_LYRA2RE); }

BOOST_AUTO_TEST_SUITE_END()
