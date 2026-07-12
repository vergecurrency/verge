// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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

BOOST_AUTO_TEST_SUITE_END()