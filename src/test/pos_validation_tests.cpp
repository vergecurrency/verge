// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/block.h>
#include <pos/crypto.h>
#include <pos/validation.h>
#include <primitives/block.h>
#include <pubkey.h>
#include <script/standard.h>
#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <cstring>

namespace {

struct FixtureData {
    unsigned char secret[32]{};
    pos::StakeSnapshot snapshot;
    CBlock block;
};

FixtureData MakeFixture()
{
    FixtureData fixture;
    fixture.secret[31] = 3;
    unsigned char auxiliary[32]{};

    pos::SnapshotEntry entry;
    entry.outpoint = COutPoint(uint256S("01"), 0);
    entry.bond.value = 1000 * COIN;
    entry.bond.creation_height = 1;
    entry.bond.data.vrf_public_key[0] = 0x02;
    std::memset(entry.bond.data.vrf_public_key + 1, 2,
                pos::VRF_PUBLIC_KEY_SIZE - 1);
    entry.bond.data.reward_key_id.SetHex("03");
    entry.bond.data.withdrawal_key_id.SetHex("04");

    fixture.snapshot.source_epoch = pos::INITIAL_SNAPSHOT_EPOCH;
    fixture.snapshot.source_height = 760;
    fixture.snapshot.total_value = entry.bond.value;
    fixture.snapshot.entries.push_back(entry);
    fixture.snapshot.root =
        pos::ComputeSnapshotRoot(fixture.snapshot.entries);

    pos::StakeProof& proof = fixture.block.posExtension.stake_proof;
    proof.bond_outpoint = entry.outpoint;
    proof.slot = 10;
    proof.snapshot_epoch = fixture.snapshot.source_epoch;
    proof.snapshot_root = fixture.snapshot.root;
    proof.epoch_seed = uint256S("05");
    std::memcpy(proof.vrf_public_key, entry.bond.data.vrf_public_key,
                pos::VRF_PUBLIC_KEY_SIZE);
    std::memset(proof.vrf_output, 6, pos::VRF_OUTPUT_SIZE);
    std::memset(proof.vrf_proof, 7, pos::VRF_PROOF_SIZE);

    pos::BlockAuthorization& authorization =
        fixture.block.posExtension.authorization;
    authorization.network_id = 3;
    authorization.parent_block_root = uint256S("08");
    authorization.candidate_header_hash = uint256S("09");
    authorization.slot = proof.slot;
    authorization.bond_outpoint = proof.bond_outpoint;
    authorization.stake_proof_hash =
        pos::GetTaggedHash(pos::HashDomain::STAKE_PROOF, proof);
    authorization.fee_reward_transaction_hash = uint256S("10");
    authorization.parent_randomness = proof.epoch_seed;

    BOOST_REQUIRE(pos::SignSchnorr(
        fixture.secret, pos::GetBlockSigningHash(authorization), auxiliary,
        authorization.signature, proof.signing_public_key));
    std::memcpy(fixture.snapshot.entries[0].bond.data.signing_public_key,
                proof.signing_public_key, pos::SCHNORR_PUBLIC_KEY_SIZE);
    fixture.snapshot.root =
        pos::ComputeSnapshotRoot(fixture.snapshot.entries);
    proof.snapshot_root = fixture.snapshot.root;
    authorization.stake_proof_hash =
        pos::GetTaggedHash(pos::HashDomain::STAKE_PROOF, proof);
    BOOST_REQUIRE(pos::SignSchnorr(
        fixture.secret, pos::GetBlockSigningHash(authorization), auxiliary,
        authorization.signature, proof.signing_public_key));

    CMutableTransaction reward;
    reward.vin.resize(1);
    reward.vin[0].prevout.SetNull();
    reward.vout.emplace_back(
        CENT, GetScriptForDestination(
                  CKeyID(fixture.snapshot.entries[0].bond.data.reward_key_id)));
    reward.vout.emplace_back(
        0, pos::GetBlockCommitmentScript(
               pos::GetBlockCommitment(fixture.block.posExtension)));
    fixture.block.vtx.push_back(MakeTransactionRef(reward));
    return fixture;
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(pos_validation_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(snapshot_and_authorization)
{
    FixtureData fixture = MakeFixture();
    BOOST_CHECK(pos::CheckStakeAuthorization(
                    fixture.block, fixture.snapshot,
                    pos::INITIAL_SNAPSHOT_EPOCH) ==
                pos::ValidationError::NONE);

    fixture.block.posExtension.authorization.signature[0] ^= 1;
    BOOST_CHECK(pos::CheckStakeAuthorization(
                    fixture.block, fixture.snapshot,
                    pos::INITIAL_SNAPSHOT_EPOCH) ==
                pos::ValidationError::AUTHORIZATION_SIGNATURE);
}

BOOST_AUTO_TEST_CASE(commitment_reward_and_fee_limit)
{
    FixtureData fixture = MakeFixture();
    const pos::BondRecord& bond = fixture.snapshot.entries[0].bond;
    BOOST_CHECK(pos::CheckReward(fixture.block, bond, CENT) ==
                pos::ValidationError::NONE);
    BOOST_CHECK(pos::CheckReward(fixture.block, bond, CENT - 1) ==
                pos::ValidationError::REWARD_AMOUNT);

    CMutableTransaction altered(*fixture.block.vtx.front());
    altered.vout[0].scriptPubKey = CScript() << OP_TRUE;
    fixture.block.vtx[0] = MakeTransactionRef(altered);
    BOOST_CHECK(pos::CheckReward(fixture.block, bond, CENT) ==
                pos::ValidationError::REWARD_DESTINATION);
}

BOOST_AUTO_TEST_CASE(snapshot_epoch_selection)
{
    BOOST_CHECK_EQUAL(pos::GetRequiredSnapshotEpoch(0, 2),
                      pos::INITIAL_SNAPSHOT_EPOCH);
    BOOST_CHECK_EQUAL(pos::GetRequiredSnapshotEpoch(1, 2),
                      pos::INITIAL_SNAPSHOT_EPOCH);
    BOOST_CHECK_EQUAL(pos::GetRequiredSnapshotEpoch(2, 2), 0U);
    BOOST_CHECK_EQUAL(pos::GetRequiredSnapshotEpoch(10, 2), 8U);
}

BOOST_AUTO_TEST_SUITE_END()