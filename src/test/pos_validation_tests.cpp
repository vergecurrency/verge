// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <arith_uint256.h>
#include <chain.h>
#include <chainparams.h>
#include <pos/block.h>
#include <pos/crypto.h>
#include <pos/validation.h>
#include <pos/vrf.h>
#include <primitives/block.h>
#include <pubkey.h>
#include <script/standard.h>
#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <cstring>
#include <limits>

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

BOOST_AUTO_TEST_CASE(checkpoint_vote_freshness_precedes_crypto)
{
    pos::CheckpointVote vote;
    vote.bond_outpoint = COutPoint(uint256S("11"), 0);
    vote.snapshot_epoch = 1;
    vote.source_epoch = 2;
    vote.source_checkpoint_root = uint256S("12");
    vote.target_epoch = 3;
    vote.target_checkpoint_root = uint256S("13");
    vote.head_slot = 100;
    vote.head_block_root = uint256S("14");
    vote.signature[0] = 1;
    pos::State state;

    BOOST_CHECK(pos::CheckCheckpointVote(vote, state, 2, 10, 1000) ==
                pos::ValidationError::VOTE_CHECKPOINT);
    vote.target_epoch = 11;
    BOOST_CHECK(pos::CheckCheckpointVote(vote, state, 2, 10, 1000) ==
                pos::ValidationError::VOTE_CHECKPOINT);
    vote.target_epoch = 4;
    vote.snapshot_epoch = 2;
    BOOST_CHECK(pos::CheckCheckpointVote(vote, state, 2, 10, 1000) ==
                pos::ValidationError::VOTE_SNAPSHOT);
}

BOOST_AUTO_TEST_CASE(checkpoint_link_does_not_require_orphan_headers)
{
    Consensus::Params params = Params().GetConsensus();
    params.nPoSActivationHeight = 1000;
    params.nPoSBondRulesHeight = 1;
    pos::State state;
    pos::StateUndo undo;
    unsigned char secret[32]{};
    secret[31] = 3;
    pos::BondData bond;
    unsigned char vrf_secret[32]{};
    BOOST_REQUIRE(pos::GetSchnorrPublicKey(secret, bond.signing_public_key));
    BOOST_REQUIRE(pos::DeriveVrfKey(secret, vrf_secret, bond.vrf_public_key));
    bond.reward_key_id.SetHex("01");
    bond.withdrawal_key_id.SetHex("02");
    CMutableTransaction create;
    create.vin.emplace_back(COutPoint(uint256S("09"), 0));
    create.vout.emplace_back(params.nPoSMinStake, pos::GetBondScript(bond));
    CBlock creation;
    creation.vtx.push_back(MakeTransactionRef(create));
    BOOST_REQUIRE(state.ApplyBlock(creation, 1, false, 0, 0, params, undo));
    CBlock snapshot;
    BOOST_REQUIRE(state.ApplyBlock(snapshot, 760, false, 0, 0, params, undo));
    BOOST_REQUIRE(state.SetEpochSeed(0, uint256S("01"), undo));
    CBlock checkpoint;
    checkpoint.nVersion = pos::BLOCK_VERSION_POS | 2;
    checkpoint.hashPrevBlock = uint256S("02");
    checkpoint.nTime = 3000;
    checkpoint.posExtension.stake_proof.slot = 100;
    BOOST_REQUIRE(state.ApplyBlock(checkpoint, 1000, true, 0, 0, params, undo));

    pos::CheckpointVote vote;
    vote.bond_outpoint = COutPoint(create.GetHash(), 0);
    vote.snapshot_epoch = pos::INITIAL_SNAPSHOT_EPOCH;
    vote.source_epoch = pos::FINAL_POW_CHECKPOINT_EPOCH;
    vote.source_checkpoint_root = checkpoint.hashPrevBlock;
    vote.target_epoch = 0;
    vote.target_checkpoint_root = checkpoint.GetHash();
    vote.head_slot = 101;
    vote.head_block_root = uint256S("03");
    unsigned char auxiliary[32]{};
    BOOST_REQUIRE(pos::SignSchnorr(secret, pos::GetVoteSigningHash(vote),
                                    auxiliary, vote.signature,
                                    bond.signing_public_key));
    CBlock containing;
    containing.posExtension.votes.push_back(vote);
    BOOST_CHECK(pos::CheckVotesAndEvidence(containing, state, 2,
                    params.nPoSNetworkId, 0, 102, 1001) ==
                pos::ValidationError::NONE);
    containing.posExtension.votes[0].head_block_root = uint256S("ff");
    BOOST_CHECK(pos::CheckVotesAndEvidence(containing, state, 2,
                    params.nPoSNetworkId, 0, 102, 1001) ==
                pos::ValidationError::VOTE_SIGNATURE);
    BOOST_CHECK(pos::CheckVoteHead(vote, state, 102) ==
                pos::ValidationError::NONE);
    vote.head_slot = 103;
    BOOST_CHECK(pos::CheckVoteHead(vote, state, 102) ==
                pos::ValidationError::VOTE_HEAD);
    vote.head_slot = 99;
    BOOST_CHECK(pos::CheckVoteHead(vote, state, 102) ==
                pos::ValidationError::VOTE_HEAD);
    vote.head_slot = 100;
    BOOST_CHECK(pos::CheckVoteHead(vote, state, 102) ==
                pos::ValidationError::VOTE_HEAD);
    vote.head_block_root = checkpoint.GetHash();
    BOOST_CHECK(pos::CheckVoteHead(vote, state, 102) ==
                pos::ValidationError::NONE);
    vote.head_slot = 101;
    BOOST_CHECK(pos::CheckVoteHead(vote, state, 102) ==
                pos::ValidationError::VOTE_HEAD);
    vote.target_checkpoint_root = uint256S("04");
    BOOST_CHECK(pos::CheckVoteHead(vote, state, 102) ==
                pos::ValidationError::VOTE_CHECKPOINT);
}

BOOST_AUTO_TEST_CASE(fork_weight_uses_only_branch_local_head_and_signed_slot)
{
    Consensus::Params params = Params().GetConsensus();
    params.nPoSActivationHeight = 1;
    std::vector<CBlockIndex> chain(5);
    std::vector<uint256> hashes(5);
    for (size_t i = 0; i < chain.size(); ++i) {
        hashes[i] = ArithToUint256(arith_uint256(i + 1));
        chain[i].phashBlock = &hashes[i];
        chain[i].nHeight = static_cast<int>(i);
        chain[i].nVersion = i == 0 ? 4 : pos::BLOCK_VERSION_POS | 2;
        chain[i].nTime = static_cast<uint32_t>((100 + i * 2) * 30);
        chain[i].pprev = i == 0 ? nullptr : &chain[i - 1];
        chain[i].BuildSkip();
    }
    pos::CheckpointVote vote;
    vote.head_block_root = hashes[2];
    vote.head_slot = 104;
    BOOST_CHECK(pos::FindVoteHead(vote, &chain.back(), params) == &chain[2]);
    BOOST_CHECK(pos::FindVoteHead(vote, &chain[1], params) == nullptr);
    vote.head_slot = 105;
    BOOST_CHECK(pos::FindVoteHead(vote, &chain.back(), params) == nullptr);
    vote.head_slot = 104;
    CBlockIndex orphan(chain[2]);
    const uint256 orphan_hash = uint256S("ff");
    orphan.phashBlock = &orphan_hash;
    vote.head_block_root = orphan_hash;
    BOOST_CHECK(pos::FindVoteHead(vote, &orphan, params) == &orphan);
    BOOST_CHECK(pos::FindVoteHead(vote, &chain.back(), params) == nullptr);
    vote.head_slot = std::numeric_limits<uint64_t>::max();
    BOOST_CHECK(pos::FindVoteHead(vote, &chain.back(), params) == nullptr);
    params.nPoSSlotSeconds = 0;
    BOOST_CHECK(pos::FindVoteHead(vote, &chain.back(), params) == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
