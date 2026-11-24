// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <arith_uint256.h>
#include <chainparams.h>
#include <pos/consensus.h>
#include <pos/crypto.h>
#include <pos/validation.h>
#include <pos/vrf.h>
#include <primitives/block.h>
#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <array>
#include <limits>

namespace {
pos::StakeSnapshot Snapshot(const std::vector<CAmount>& weights)
{
    pos::StakeSnapshot snapshot;
    snapshot.source_epoch = 7;
    snapshot.source_height = 1000;
    unsigned char secret[32]{};
    secret[31] = 3;
    for (size_t i = 0; i < weights.size(); ++i) {
        pos::SnapshotEntry entry;
        entry.outpoint = COutPoint(ArithToUint256(arith_uint256(i + 1)), 0);
        entry.bond.value = weights[i];
        entry.bond.creation_height = 1;
        BOOST_REQUIRE(pos::GetSchnorrPublicKey(secret, entry.bond.data.signing_public_key));
        snapshot.total_value += weights[i];
        snapshot.entries.push_back(entry);
    }
    snapshot.root = pos::ComputeSnapshotRoot(snapshot.entries);
    return snapshot;
}
} // namespace

BOOST_FIXTURE_TEST_SUITE(pos_public_eligibility_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(network_selection_is_explicit)
{
    BOOST_CHECK(!CreateChainParams(CBaseChainParams::MAIN)->GetConsensus().fPoSPublicEligibility);
    BOOST_CHECK(CreateChainParams(CBaseChainParams::TESTNET)->GetConsensus().fPoSPublicEligibility);
    BOOST_CHECK(CreateChainParams(CBaseChainParams::REGTEST)->GetConsensus().fPoSPublicEligibility);
    BOOST_CHECK(!CreateChainParams(CBaseChainParams::MAIN)->GetConsensus().fPoSPostQuantum);
    BOOST_CHECK(CreateChainParams(CBaseChainParams::TESTNET)->GetConsensus().fPoSPostQuantum);
    BOOST_CHECK(CreateChainParams(CBaseChainParams::REGTEST)->GetConsensus().fPoSPostQuantum);
}

BOOST_AUTO_TEST_CASE(fixed_public_selection_vector)
{
    auto snapshot = Snapshot({COIN, 2 * COIN, 7 * COIN});
    // Treat this independently fixed root as an authenticated snapshot input.
    snapshot.root = uint256S("abcd");
    COutPoint winner;
    uint256 hash;
    BOOST_REQUIRE(pos::SelectPublicStake(snapshot, 2, uint256S("feed"), 500, winner, hash));
    BOOST_CHECK_EQUAL(hash.GetHex(), "bd951cf22db16bf3cdcce77477ced070b548e65bd738459a8492c721fc8e053b");
    BOOST_CHECK(winner == snapshot.entries[2].outpoint);
    COutPoint repeated;
    uint256 repeated_hash;
    BOOST_REQUIRE(pos::SelectPublicStake(snapshot, 2, uint256S("feed"), 500, repeated, repeated_hash));
    BOOST_CHECK(winner == repeated);
    BOOST_CHECK(hash == repeated_hash);
}

BOOST_AUTO_TEST_CASE(stake_weighting_and_split_balance)
{
    const auto snapshot = Snapshot({COIN, 2 * COIN, 7 * COIN});
    const auto split = Snapshot(std::vector<CAmount>(10, COIN));
    std::array<size_t, 3> won{};
    size_t split_owner_won = 0;
    for (uint64_t slot = 1; slot <= 10000; ++slot) {
        COutPoint winner;
        uint256 hash;
        BOOST_REQUIRE(pos::SelectPublicStake(snapshot, 2, uint256S("feed"), slot, winner, hash));
        const auto index = UintToArith256(winner.hash).GetLow64() - 1;
        BOOST_REQUIRE(index < won.size());
        ++won[index];
        BOOST_REQUIRE(pos::SelectPublicStake(split, 2, uint256S("feed"), slot, winner, hash));
        if (UintToArith256(winner.hash).GetLow64() > 3) ++split_owner_won;
    }
    BOOST_CHECK(won[0] > 800 && won[0] < 1200);
    BOOST_CHECK(won[1] > 1700 && won[1] < 2300);
    BOOST_CHECK(won[2] > 6600 && won[2] < 7400);
    BOOST_CHECK(split_owner_won > 6600 && split_owner_won < 7400);
}

BOOST_AUTO_TEST_CASE(invalid_snapshots_fail_closed_and_clear_output)
{
    const auto valid = Snapshot({COIN, COIN});
    COutPoint winner;
    uint256 hash;
    const auto rejects = [&](const pos::StakeSnapshot& snapshot,
                             uint32_t network = 2, uint64_t slot = 500,
                             const uint256& seed = uint256S("feed")) {
        winner = valid.entries[0].outpoint;
        hash = uint256S("01");
        BOOST_CHECK(!pos::SelectPublicStake(snapshot, network, seed, slot, winner, hash));
        BOOST_CHECK(winner.IsNull());
        BOOST_CHECK(hash.IsNull());
    };
    auto bad = valid;
    bad.entries.clear();
    rejects(bad);
    bad = valid;
    ++bad.total_value;
    rejects(bad);
    bad = valid;
    bad.total_value = MAX_MONEY + 1;
    rejects(bad);
    bad = valid;
    bad.entries[0].bond.value = -1;
    rejects(bad);
    bad = valid;
    bad.entries[0].bond.value = MAX_MONEY;
    rejects(bad);
    bad = valid;
    bad.entries[1].outpoint = bad.entries[0].outpoint;
    rejects(bad);
    bad = valid;
    std::reverse(bad.entries.begin(), bad.entries.end());
    rejects(bad);
    bad = valid;
    bad.root.SetNull();
    rejects(bad);
    rejects(valid, 0);
    rejects(valid, 2, 0);
    rejects(valid, 2, 500, uint256());
    const auto maximal = Snapshot({MAX_MONEY});
    BOOST_REQUIRE(pos::SelectPublicStake(maximal, 2, uint256S("feed"),
                                          std::numeric_limits<uint64_t>::max(), winner, hash));
    BOOST_CHECK(winner == maximal.entries[0].outpoint);
}

BOOST_AUTO_TEST_CASE(public_proof_validation_and_no_legacy_fallback)
{
    const auto snapshot = Snapshot({COIN, 2 * COIN, 7 * COIN});
    auto params = CreateChainParams(CBaseChainParams::TESTNET)->GetConsensus();
    CBlock block;
    BOOST_REQUIRE(pos::BuildPublicStakeProof(snapshot, params.nPoSNetworkId,
                                             uint256S("feed"), 500, block.posExtension.stake_proof));
    const CBlock valid = block;
    BOOST_CHECK(pos::CheckStructure(block.posExtension.stake_proof) == pos::StructureError::NONE);
    BOOST_CHECK(pos::CheckStakeEligibility(block, snapshot, params) == pos::ValidationError::NONE);
    block.posExtension.stake_proof.version = pos::POS_OBJECT_VERSION;
    BOOST_CHECK(pos::CheckStakeEligibility(block, snapshot, params) == pos::ValidationError::ELIGIBILITY_SCHEME);
    block = valid;
    block.posExtension.stake_proof.vrf_proof[0] = 1;
    BOOST_CHECK(pos::CheckStakeEligibility(block, snapshot, params) == pos::ValidationError::PUBLIC_ELIGIBILITY_INVALID);
    block = valid;
    block.posExtension.stake_proof.vrf_public_key[0] = 2;
    BOOST_CHECK(pos::CheckStakeEligibility(block, snapshot, params) == pos::ValidationError::PUBLIC_ELIGIBILITY_INVALID);
    block = valid;
    block.posExtension.stake_proof.vrf_output[0] ^= 1;
    BOOST_CHECK(pos::CheckStakeEligibility(block, snapshot, params) == pos::ValidationError::PUBLIC_ELIGIBILITY_INVALID);
    block = valid;
    const auto original = block.posExtension.stake_proof.bond_outpoint;
    block.posExtension.stake_proof.bond_outpoint = snapshot.entries[0].outpoint == original
        ? snapshot.entries[1].outpoint : snapshot.entries[0].outpoint;
    BOOST_CHECK(pos::CheckStakeEligibility(block, snapshot, params) == pos::ValidationError::STAKE_INELIGIBLE);
    block = valid;
    block.posExtension.stake_proof.snapshot_epoch++;
    BOOST_CHECK(pos::CheckStakeEligibility(block, snapshot, params) == pos::ValidationError::SNAPSHOT_MISMATCH);
    block = valid;
    params.fPoSPublicEligibility = false;
    BOOST_CHECK(pos::CheckStakeEligibility(block, snapshot, params) == pos::ValidationError::ELIGIBILITY_SCHEME);
    COutPoint winner;
    uint256 changed;
    BOOST_REQUIRE(pos::SelectPublicStake(snapshot, 3, uint256S("feed"), 500, winner, changed));
    BOOST_CHECK(!std::equal(changed.begin(), changed.end(), valid.posExtension.stake_proof.vrf_output));
}

BOOST_AUTO_TEST_SUITE_END()
