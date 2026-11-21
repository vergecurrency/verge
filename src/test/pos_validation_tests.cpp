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

struct BondValidationFixture {
    Consensus::Params params{Params().GetConsensus()};
    pos::State state;
    pos::BondData data;
    COutPoint outpoint;
    unsigned char secret[32]{};

    BondValidationFixture()
    {
        params.nPoSBondRulesHeight = 1;
        params.nPoSActivationHeight = 1000;
        params.nPoSStakeMaturity = 720;
        params.nPoSUnbondingBlocks = 5;
        secret[31] = 3;
        unsigned char vrf_secret[32]{};
        BOOST_REQUIRE(pos::GetSchnorrPublicKey(secret, data.signing_public_key));
        BOOST_REQUIRE(pos::DeriveVrfKey(secret, vrf_secret, data.vrf_public_key));
        data.reward_key_id.SetHex("01");
        data.withdrawal_key_id.SetHex("02");
        CMutableTransaction tx;
        tx.vin.emplace_back(COutPoint(uint256S("09"), 0));
        tx.vout.emplace_back(params.nPoSMinStake, pos::GetBondScript(data));
        outpoint = COutPoint(tx.GetHash(), 0);
        CBlock creation;
        creation.vtx.push_back(MakeTransactionRef(tx));
        pos::StateUndo undo;
        BOOST_REQUIRE(state.ApplyBlock(creation, 1, false, 0, 0, params, undo));
        CBlock snapshot;
        BOOST_REQUIRE(state.ApplyBlock(snapshot, 760, false, 0, 0, params, undo));
        BOOST_REQUIRE(state.SetEpochSeed(0, uint256S("01"), undo));
    }
};

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

BOOST_AUTO_TEST_CASE(parent_authentication_rejects_nonmembers_and_bad_proofs_without_mutation)
{
    BondValidationFixture fixture;
    CBlock block = MakeFixture().block;
    auto& proof = block.posExtension.stake_proof;
    proof.bond_outpoint = fixture.outpoint;
    proof.slot = 100;
    proof.snapshot_epoch = pos::INITIAL_SNAPSHOT_EPOCH;
    proof.snapshot_root = fixture.state.FindSnapshot(pos::INITIAL_SNAPSHOT_EPOCH)->root;
    proof.epoch_seed = *fixture.state.FindEpochSeed(0);
    std::copy(std::begin(fixture.data.signing_public_key), std::end(fixture.data.signing_public_key),
              proof.signing_public_key);
    unsigned char vrf_secret[32]{};
    BOOST_REQUIRE(pos::DeriveVrfKey(fixture.secret, vrf_secret, proof.vrf_public_key));
    const auto input = pos::GetVrfInput(fixture.params.nPoSNetworkId, proof.epoch_seed,
                                       proof.slot, proof.bond_outpoint);
    BOOST_REQUIRE(pos::VrfProve(vrf_secret, input.data(), input.size(),
        proof.vrf_public_key, proof.vrf_output, proof.vrf_proof));
    auto& authorization = block.posExtension.authorization;
    authorization.parent_randomness = proof.epoch_seed;
    authorization.bond_outpoint = proof.bond_outpoint;
    authorization.slot = proof.slot;
    authorization.stake_proof_hash = pos::GetTaggedHash(pos::HashDomain::STAKE_PROOF, proof);
    unsigned char auxiliary[32]{};
    unsigned char public_key[32];
    BOOST_REQUIRE(pos::SignSchnorr(fixture.secret, pos::GetBlockSigningHash(authorization),
        auxiliary, authorization.signature, public_key));
    const uint256 before = SerializeHash(fixture.state);
    BOOST_CHECK(pos::CheckProducerAtParent(block, fixture.state, 0, 1000,
        fixture.params) == pos::ValidationError::NONE);
    const CBlock valid = block;
    proof.bond_outpoint.n += 1;
    BOOST_CHECK(pos::CheckProducerAtParent(block, fixture.state, 0, 1000,
        fixture.params) == pos::ValidationError::BOND_MISSING);
    block = valid;
    block.posExtension.stake_proof.vrf_proof[1] ^= 1;
    BOOST_CHECK(pos::CheckProducerAtParent(block, fixture.state, 0, 1000,
        fixture.params) == pos::ValidationError::VRF_INVALID);
    block = valid;
    block.posExtension.stake_proof.epoch_seed = uint256S("bad");
    BOOST_CHECK(pos::CheckProducerAtParent(block, fixture.state, 0, 1000,
        fixture.params) == pos::ValidationError::AUTHORIZATION_MISMATCH);
    BOOST_CHECK(SerializeHash(fixture.state) == before);
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

BOOST_AUTO_TEST_CASE(signed_surround_evidence_includes_final_pow_source)
{
    BondValidationFixture fixture;
    for (uint64_t source : {uint64_t{0}, pos::FINAL_POW_CHECKPOINT_EPOCH}) {
        pos::CheckpointVote outer;
        outer.bond_outpoint = fixture.outpoint;
        outer.source_epoch = source;
        outer.source_checkpoint_root = uint256S("10");
        outer.target_epoch = 3;
        outer.target_checkpoint_root = uint256S("11");
        outer.head_slot = 500;
        outer.head_block_root = uint256S("12");
        pos::CheckpointVote inner = outer;
        inner.source_epoch = 1;
        inner.source_checkpoint_root = uint256S("13");
        inner.target_epoch = 2;
        inner.target_checkpoint_root = uint256S("14");
        unsigned char auxiliary[32]{};
        unsigned char public_key[32];
        BOOST_REQUIRE(pos::SignSchnorr(fixture.secret, pos::GetVoteSigningHash(outer),
                                       auxiliary, outer.signature, public_key));
        BOOST_REQUIRE(pos::SignSchnorr(fixture.secret, pos::GetVoteSigningHash(inner),
                                       auxiliary, inner.signature, public_key));
        pos::VoteEquivocationEvidence evidence;
        evidence.first = outer;
        evidence.second = inner;
        if (pos::GetVoteSigningHash(evidence.second) < pos::GetVoteSigningHash(evidence.first)) {
            std::swap(evidence.first, evidence.second);
        }
        BOOST_CHECK(pos::CheckVoteEvidence(evidence, fixture.state, 3) == pos::ValidationError::NONE);
        BOOST_CHECK(pos::CheckVoteEvidence(evidence, fixture.state, 2) == pos::ValidationError::VOTE_CHECKPOINT);
        evidence.second.signature[0] ^= 1;
        BOOST_CHECK(pos::CheckVoteEvidence(evidence, fixture.state, 3) == pos::ValidationError::EVIDENCE_SIGNATURE);
    }
}

BOOST_AUTO_TEST_CASE(producer_eligibility_obeys_delayed_penalty_not_withdrawal_lock)
{
    BondValidationFixture fixture;
    const pos::BondRecord* stored = fixture.state.FindBond(fixture.outpoint);
    BOOST_REQUIRE(stored != nullptr);
    const pos::BondRecord bond = *stored;
    BOOST_CHECK(pos::CheckBondEligibility(fixture.outpoint, bond, fixture.state, 0, 720,
                                         fixture.params) == pos::ValidationError::BOND_IMMATURE);
    BOOST_CHECK(pos::CheckBondEligibility(fixture.outpoint, bond, fixture.state, 0, 721,
                                         fixture.params) == pos::ValidationError::NONE);
    pos::BlockEquivocationEvidence evidence;
    evidence.first.network_id = fixture.params.nPoSNetworkId;
    evidence.first.parent_block_root = uint256S("20");
    evidence.first.candidate_header_hash = uint256S("21");
    evidence.first.slot = 10;
    evidence.first.bond_outpoint = fixture.outpoint;
    evidence.first.stake_proof_hash = uint256S("22");
    evidence.first.fee_reward_transaction_hash = uint256S("23");
    evidence.first.parent_randomness = uint256S("24");
    evidence.second = evidence.first;
    evidence.second.candidate_header_hash = uint256S("25");
    unsigned char auxiliary[32]{};
    unsigned char public_key[32];
    for (pos::BlockAuthorization* authorization : {&evidence.first, &evidence.second}) {
        BOOST_REQUIRE(pos::SignSchnorr(fixture.secret, pos::GetBlockSigningHash(*authorization),
                                       auxiliary, authorization->signature, public_key));
    }
    if (pos::GetBlockSigningHash(evidence.second) < pos::GetBlockSigningHash(evidence.first)) {
        std::swap(evidence.first, evidence.second);
    }
    BOOST_REQUIRE(pos::CheckBlockEvidence(evidence, fixture.state, fixture.params.nPoSNetworkId, 100) ==
                  pos::ValidationError::NONE);
    CBlock offense;
    offense.nVersion = 2 | pos::BLOCK_VERSION_POS;
    offense.hashPrevBlock = uint256S("aa");
    offense.nTime = 3000;
    offense.posExtension.stake_proof.slot = 100;
    offense.posExtension.block_evidence.push_back(evidence);
    pos::StateUndo offense_undo;
    BOOST_REQUIRE(fixture.state.ApplyBlock(offense, 1000, true, 0, 0, fixture.params, offense_undo));
    BOOST_CHECK(fixture.state.IsWithdrawalLocked(fixture.outpoint, 1000));
    BOOST_CHECK(pos::CheckBondEligibility(fixture.outpoint, bond, fixture.state, 0, 1000,
                                         fixture.params) == pos::ValidationError::NONE);
    pos::StateUndo epoch_undo;
    BOOST_REQUIRE(fixture.state.PrepareEpoch(1, 1001, fixture.params, epoch_undo));
    BOOST_CHECK(pos::CheckBondEligibility(fixture.outpoint, bond, fixture.state, 1, 1001,
                                         fixture.params) == pos::ValidationError::BOND_LOCKED);
    BOOST_CHECK(pos::CheckBondEligibility(fixture.outpoint, bond, fixture.state, 1, 1005,
                                         fixture.params) == pos::ValidationError::BOND_LOCKED);
    BOOST_CHECK(!fixture.state.IsWithdrawalLocked(fixture.outpoint, 1006));
    BOOST_CHECK(pos::CheckBondEligibility(fixture.outpoint, bond, fixture.state, 1, 1006,
                                         fixture.params) == pos::ValidationError::NONE);
    BOOST_CHECK(fixture.state.UndoBlock(epoch_undo));
    BOOST_CHECK(fixture.state.IsWithdrawalLocked(fixture.outpoint, 1000));
    BOOST_CHECK(pos::CheckBondEligibility(fixture.outpoint, bond, fixture.state, 0, 1000,
                                         fixture.params) == pos::ValidationError::NONE);
    BOOST_CHECK(fixture.state.UndoBlock(offense_undo));
    BOOST_CHECK(!fixture.state.IsWithdrawalLocked(fixture.outpoint, 1000));
    auto future_bond = bond;
    future_bond.creation_height = std::numeric_limits<int32_t>::max() - 10;
    BOOST_CHECK(pos::CheckBondEligibility(fixture.outpoint, future_bond, fixture.state, 0,
        std::numeric_limits<int32_t>::max(), fixture.params) == pos::ValidationError::BOND_IMMATURE);
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
