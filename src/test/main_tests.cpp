// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2023 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <validation.h>
#include <net.h>

#include <test/setup_common.h>

#include <boost/signals2/signal.hpp>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(main_tests, TestingSetup)

static void TestBlockSubsidyHalvings(const Consensus::Params& consensusParams)
{
    CAmount nInitialSubsidy = 200000 * COIN;

    BOOST_CHECK_EQUAL(GetBlockSubsidy(0, consensusParams), 0);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1, consensusParams), nInitialSubsidy);
    BOOST_CHECK_EQUAL(200000 * COIN, nInitialSubsidy);
    nInitialSubsidy /= 2;
    BOOST_CHECK_EQUAL(100000 * COIN, nInitialSubsidy);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(14001, consensusParams), nInitialSubsidy);
    nInitialSubsidy /= 2;
    BOOST_CHECK_EQUAL(50000 * COIN, nInitialSubsidy);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(28001, consensusParams), nInitialSubsidy);
    nInitialSubsidy /= 2;
    BOOST_CHECK_EQUAL(25000 * COIN, nInitialSubsidy);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(42001, consensusParams), nInitialSubsidy);
    nInitialSubsidy /= 2;
    BOOST_CHECK_EQUAL(12500 * COIN, nInitialSubsidy);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(210001, consensusParams), nInitialSubsidy);
    nInitialSubsidy /= 2;
    BOOST_CHECK_EQUAL(6250 * COIN, nInitialSubsidy);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(378001, consensusParams), nInitialSubsidy);
    nInitialSubsidy /= 2;
    BOOST_CHECK_EQUAL(3125 * COIN, nInitialSubsidy);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(546001, consensusParams), nInitialSubsidy);
    nInitialSubsidy /= 2;
    nInitialSubsidy -= 2.5 * COIN;
    BOOST_CHECK_EQUAL(1560 * COIN, nInitialSubsidy);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(714001, consensusParams), nInitialSubsidy);
    nInitialSubsidy /= 2;
    nInitialSubsidy -= 50 * COIN;
    BOOST_CHECK_EQUAL(730 * COIN, nInitialSubsidy);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(2124001, consensusParams), nInitialSubsidy);

    // Restart cycle with divided by two regression
    nInitialSubsidy = 400 * COIN;
    BOOST_CHECK_EQUAL(400 * COIN, nInitialSubsidy);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(3700001, consensusParams), nInitialSubsidy);
    nInitialSubsidy /= 2;
    BOOST_CHECK_EQUAL(200 * COIN, nInitialSubsidy);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(4200001, consensusParams), nInitialSubsidy);
    nInitialSubsidy /= 2;
    BOOST_CHECK_EQUAL(100 * COIN, nInitialSubsidy);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(4700001, consensusParams), nInitialSubsidy);
    nInitialSubsidy /= 2;
    BOOST_CHECK_EQUAL(50 * COIN, nInitialSubsidy);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(5200001, consensusParams), nInitialSubsidy);
    nInitialSubsidy /= 2;
    BOOST_CHECK_EQUAL(25 * COIN, nInitialSubsidy);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(5700001, consensusParams), nInitialSubsidy);
    nInitialSubsidy /= 2;
    BOOST_CHECK_EQUAL(12.5 * COIN, nInitialSubsidy);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(6200001, consensusParams), nInitialSubsidy);
    nInitialSubsidy /= 2;
    BOOST_CHECK_EQUAL(6.25 * COIN, nInitialSubsidy);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(6700001, consensusParams), nInitialSubsidy);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(7200000, consensusParams), nInitialSubsidy);

    // extend halvings over here 
    BOOST_CHECK_EQUAL(GetBlockSubsidy(7200001, consensusParams), 0);
}

BOOST_AUTO_TEST_CASE(block_subsidy_test)
{
    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    TestBlockSubsidyHalvings(chainParams->GetConsensus()); // As in main
}

BOOST_AUTO_TEST_CASE(subsidy_limit_test)
{
    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    CAmount nSum = 0;
    for (int nHeight = 0; nHeight < 14000000; nHeight += 1000) {
        CAmount nSubsidy = GetBlockSubsidy(nHeight, chainParams->GetConsensus());
        BOOST_CHECK(nSubsidy <= 200000 * COIN);
        nSum += nSubsidy * 1000;
        BOOST_REQUIRE(MoneyRange(nSum));
    }
    BOOST_CHECK_EQUAL(nSum, CAmount{16521955000 * COIN});
    BOOST_CHECK_MESSAGE(nSum > CAmount{16500000000 * COIN}, "generates too less coins");
    BOOST_CHECK_MESSAGE(MoneyRange(nSum), "Subsidy is exceeding the total supply");
}

static bool ReturnFalse() { return false; }
static bool ReturnTrue() { return true; }

BOOST_AUTO_TEST_CASE(test_combiner_all)
{
    boost::signals2::signal<bool (), CombinerAll> Test;
    BOOST_CHECK(Test());
    Test.connect(&ReturnFalse);
    BOOST_CHECK(!Test());
    Test.connect(&ReturnTrue);
    BOOST_CHECK(!Test());
    Test.disconnect(&ReturnFalse);
    BOOST_CHECK(Test());
    Test.disconnect(&ReturnTrue);
    BOOST_CHECK(Test());
}
BOOST_AUTO_TEST_SUITE_END()
