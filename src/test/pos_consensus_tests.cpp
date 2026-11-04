// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/forkchoice.h>
#include <chainparams.h>
#include <pos/consensus.h>
#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(pos_consensus_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(slot_boundaries)
{
    Consensus::Params params = CreateChainParams(CBaseChainParams::REGTEST)->GetConsensus();
    params.nPoSActivationHeight = 1000;

    const uint32_t final_pow_time = 1462058066;
    const uint64_t activation_slot = final_pow_time / params.nPoSSlotSeconds + 1;
    const uint32_t first_time = static_cast<uint32_t>(activation_slot * params.nPoSSlotSeconds);

    pos::SlotInfo info;
    BOOST_CHECK(pos::GetSlotInfo(final_pow_time, first_time, params, info));
    BOOST_CHECK_EQUAL(info.relative_slot, 0U);
    BOOST_CHECK_EQUAL(info.epoch, 0U);
    BOOST_CHECK_EQUAL(info.slot_in_epoch, 0U);

    BOOST_CHECK(pos::GetSlotInfo(final_pow_time, first_time + 119 * 30, params, info));
    BOOST_CHECK_EQUAL(info.epoch, 0U);
    BOOST_CHECK_EQUAL(info.slot_in_epoch, 119U);

    BOOST_CHECK(pos::GetSlotInfo(final_pow_time, first_time + 120 * 30, params, info));
    BOOST_CHECK_EQUAL(info.epoch, 1U);
    BOOST_CHECK_EQUAL(info.slot_in_epoch, 0U);

    BOOST_CHECK(!pos::GetSlotInfo(final_pow_time, first_time + 1, params, info));
    BOOST_CHECK(!pos::GetSlotInfo(final_pow_time, first_time - 30, params, info));
}

BOOST_AUTO_TEST_CASE(initial_snapshot_height)
{
    Consensus::Params params = CreateChainParams(CBaseChainParams::REGTEST)->GetConsensus();
    params.nPoSActivationHeight = 1000;
    BOOST_CHECK_EQUAL(pos::GetInitialStakeSnapshotHeight(params), 760);

    params.nPoSActivationHeight = 239;
    BOOST_CHECK_EQUAL(pos::GetInitialStakeSnapshotHeight(params), -1);
}

BOOST_AUTO_TEST_CASE(epoch_seeds_commit_to_delayed_entropy)
{
    uint256 first;
    uint256 second;
    const uint256 pow_entropy = pos::MixPoWEntropy(
        uint256(), 3261, uint256S("04"));
    BOOST_REQUIRE(pos::ComputeInitialEpochSeed(
        2, 3500, uint256S("01"), pow_entropy, first));
    BOOST_REQUIRE(pos::ComputeInitialEpochSeed(
        2, 3500, uint256S("01"), pow_entropy, second));
    BOOST_CHECK(first == second);
    BOOST_CHECK(!pos::ComputeInitialEpochSeed(
        2, 3500, uint256S("01"), uint256(), second));

    const uint256 vrf_output = uint256S("06");
    const uint256 epoch_entropy = pos::MixEpochEntropy(
        uint256(), 0, 10, COutPoint(uint256S("05"), 1),
        vrf_output.begin());
    BOOST_CHECK(pos::ComputeNextEpochSeed(
                    first, 1, uint256S("02"), epoch_entropy) ==
                pos::ComputeNextEpochSeed(
                    second, 1, uint256S("02"), epoch_entropy));
    BOOST_CHECK(pos::ComputeNextEpochSeed(
                    first, 1, uint256S("02"), epoch_entropy) !=
                pos::ComputeNextEpochSeed(
                    first, 1, uint256S("03"), epoch_entropy));
    BOOST_CHECK(pos::ComputeNextEpochSeed(
                    first, 1, uint256S("02"), epoch_entropy) !=
                pos::ComputeNextEpochSeed(
                    first, 1, uint256S("02"),
                    pos::EmptyEpochEntropy(0)));
}

BOOST_AUTO_TEST_CASE(fork_weight_and_tie_breaking)
{
    const uint256 lower = uint256S("01");
    const uint256 higher = uint256S("02");

    BOOST_CHECK(pos::PreferFork(2, higher, 1, lower));
    BOOST_CHECK(!pos::PreferFork(1, lower, 2, higher));
    BOOST_CHECK(pos::PreferFork(1, lower, 1, higher));
    BOOST_CHECK(!pos::PreferFork(1, higher, 1, lower));
    BOOST_CHECK(!pos::PreferFork(1, lower, 1, lower));
}

BOOST_AUTO_TEST_CASE(fork_summary_preserves_order_and_cache_is_bounded)
{
    auto summary = std::make_shared<pos::ForkChoiceSummary>();
    summary->heads = {{10, MAX_MONEY - 5}, {12, 10}, {14, 3}, {9, 1}};
    for (int threshold = 0; threshold < 20; ++threshold) {
        CAmount reference = 0;
        for (const auto& head : summary->heads) {
            if (head.first >= threshold && MoneyRange(reference + head.second)) {
                reference += head.second;
            }
        }
        BOOST_CHECK_EQUAL(summary->WeightAbove(threshold), reference);
    }
    pos::ForkChoiceCache cache(2, 4096);
    cache.Put(uint256S("01"), summary);
    cache.Put(uint256S("02"), summary);
    BOOST_REQUIRE(cache.Get(uint256S("01")));
    cache.Put(uint256S("03"), summary);
    BOOST_CHECK(!cache.Get(uint256S("02")));
    BOOST_CHECK(cache.Get(uint256S("01")));
    BOOST_CHECK_EQUAL(cache.Size(), 2U);
    BOOST_CHECK_LE(cache.DynamicMemoryUsage(), 4096U);
    auto large = std::make_shared<pos::ForkChoiceSummary>();
    large->heads.resize(1000);
    cache.Put(uint256S("04"), large);
    BOOST_CHECK(!cache.Get(uint256S("04")));
    BOOST_CHECK_EQUAL(cache.Size(), 2U);
    pos::ForkChoiceCache byte_limited(128, cache.DynamicMemoryUsage() / 2);
    for (int i = 1; i <= 100; ++i) {
        uint256 hash;
        hash.begin()[0] = i;
        byte_limited.Put(hash, summary);
    }
    BOOST_CHECK_EQUAL(byte_limited.Size(), 1U);
    cache.Clear();
    BOOST_CHECK_EQUAL(cache.Size(), 0U);
    BOOST_CHECK_EQUAL(cache.DynamicMemoryUsage(), 0U);
}

BOOST_AUTO_TEST_SUITE_END()
