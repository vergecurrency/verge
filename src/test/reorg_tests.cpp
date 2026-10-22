// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <test/setup_common.h>
#include <validation.h>

#include <boost/test/unit_test.hpp>

#include <vector>

namespace {
std::vector<CBlockIndex> MakeBranch(CBlockIndex* parent, size_t length)
{
    std::vector<CBlockIndex> blocks(length);
    for (size_t i = 0; i < length; ++i) {
        blocks[i].pprev = i ? &blocks[i - 1] : parent;
        blocks[i].nHeight = blocks[i].pprev ? blocks[i].pprev->nHeight + 1 : 0;
        blocks[i].BuildSkip();
    }
    return blocks;
}
}

BOOST_FIXTURE_TEST_SUITE(reorg_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(automatic_reorg_depth_boundary)
{
    auto chain = MakeBranch(nullptr, 302);
    const CBlockIndex* tip = &chain[300];
    BOOST_CHECK_EQUAL(MAX_AUTOMATIC_REORG_DEPTH, 6);
    auto within_limit = MakeBranch(&chain[300 - MAX_AUTOMATIC_REORG_DEPTH], 120);
    auto beyond_limit = MakeBranch(&chain[299 - MAX_AUTOMATIC_REORG_DEPTH], 10000);
    BOOST_CHECK(IsAutomaticReorgAllowed(tip, tip));
    BOOST_CHECK(IsAutomaticReorgAllowed(tip, &chain[301]));
    BOOST_CHECK(IsAutomaticReorgAllowed(tip, &within_limit.back()));
    BOOST_CHECK(!IsAutomaticReorgAllowed(tip, &beyond_limit.back()));
    BOOST_CHECK(!IsAutomaticReorgAllowed(&chain[301], &within_limit.back()));
    BOOST_CHECK(!IsAutomaticReorgAllowed(tip, nullptr));

    // A large advertised height or work score cannot override the local anchor.
    beyond_limit.back().nChainWork = arith_uint256(1) << 200;
    BOOST_CHECK(!IsAutomaticReorgAllowed(tip, &beyond_limit.back()));
}

BOOST_AUTO_TEST_CASE(bootstrap_extensions_and_short_chains)
{
    auto chain = MakeBranch(nullptr, 1001);
    auto fork = MakeBranch(&chain[0], 200);
    BOOST_CHECK(IsAutomaticReorgAllowed(nullptr, &chain.back()));
    BOOST_CHECK(IsAutomaticReorgAllowed(&chain[0], &chain.back()));
    BOOST_CHECK(IsAutomaticReorgAllowed(&chain[MAX_AUTOMATIC_REORG_DEPTH], &fork.back()));
    BOOST_CHECK(!IsAutomaticReorgAllowed(&chain[MAX_AUTOMATIC_REORG_DEPTH + 1], &fork.back()));
    BOOST_CHECK(IsAutomaticReorgAllowed(&chain[300], &chain.back()));
}

BOOST_AUTO_TEST_SUITE_END()
