// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/primitives.h>
#include <streams.h>
#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cstring>

BOOST_FIXTURE_TEST_SUITE(pos_primitives_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(fixed_sizes_and_versions)
{
    pos::StakeProof proof;
    pos::CheckpointVote vote;
    pos::BlockAuthorization authorization;
    pos::BlockEquivocationEvidence block_evidence;
    pos::VoteEquivocationEvidence vote_evidence;

    BOOST_CHECK_EQUAL(GetSerializeSize(proof, SER_NETWORK, 0), 295U);
    BOOST_CHECK_EQUAL(GetSerializeSize(vote, SER_NETWORK, 0), 229U);
    BOOST_CHECK_EQUAL(GetSerializeSize(authorization, SER_NETWORK, 0), 273U);
    BOOST_CHECK_EQUAL(GetSerializeSize(block_evidence, SER_NETWORK, 0), 547U);
    BOOST_CHECK_EQUAL(GetSerializeSize(vote_evidence, SER_NETWORK, 0), 459U);
    BOOST_CHECK(pos::HasSupportedVersion(proof));
    BOOST_CHECK(pos::HasSupportedVersion(vote));
    BOOST_CHECK(pos::IsPoSVersion(pos::BLOCK_VERSION_POS));
    BOOST_CHECK(!pos::IsPoSVersion(0));

    proof.version = pos::PUBLIC_STAKE_PROOF_VERSION;
    BOOST_CHECK(pos::HasSupportedVersion(proof));
    proof.version = 255;
    vote.version = 0;
    BOOST_CHECK(!pos::HasSupportedVersion(proof));
    BOOST_CHECK(!pos::HasSupportedVersion(vote));
}

BOOST_AUTO_TEST_CASE(round_trip)
{
    pos::StakeProof expected;
    expected.bond_outpoint = COutPoint(uint256S("01"), 7);
    expected.slot = 42;
    expected.snapshot_epoch = 3;
    expected.snapshot_root = uint256S("02");
    expected.epoch_seed = uint256S("03");
    std::memset(expected.vrf_public_key, 0x04, sizeof(expected.vrf_public_key));
    std::memset(expected.vrf_output, 0x05, sizeof(expected.vrf_output));
    std::memset(expected.vrf_proof, 0x06, sizeof(expected.vrf_proof));
    std::memset(expected.signing_public_key, 0x07, sizeof(expected.signing_public_key));

    CDataStream stream(SER_NETWORK, 0);
    stream << expected;
    pos::StakeProof decoded;
    stream >> decoded;

    BOOST_CHECK_EQUAL(decoded.version, expected.version);
    BOOST_CHECK(decoded.bond_outpoint == expected.bond_outpoint);
    BOOST_CHECK_EQUAL(decoded.slot, expected.slot);
    BOOST_CHECK_EQUAL(decoded.snapshot_epoch, expected.snapshot_epoch);
    BOOST_CHECK(decoded.snapshot_root == expected.snapshot_root);
    BOOST_CHECK(decoded.epoch_seed == expected.epoch_seed);
    BOOST_CHECK_EQUAL_COLLECTIONS(decoded.vrf_public_key, decoded.vrf_public_key + sizeof(decoded.vrf_public_key), expected.vrf_public_key, expected.vrf_public_key + sizeof(expected.vrf_public_key));
    BOOST_CHECK_EQUAL_COLLECTIONS(decoded.vrf_output, decoded.vrf_output + sizeof(decoded.vrf_output), expected.vrf_output, expected.vrf_output + sizeof(expected.vrf_output));
    BOOST_CHECK_EQUAL_COLLECTIONS(decoded.vrf_proof, decoded.vrf_proof + sizeof(decoded.vrf_proof), expected.vrf_proof, expected.vrf_proof + sizeof(expected.vrf_proof));
    BOOST_CHECK_EQUAL_COLLECTIONS(decoded.signing_public_key, decoded.signing_public_key + sizeof(decoded.signing_public_key), expected.signing_public_key, expected.signing_public_key + sizeof(expected.signing_public_key));
    BOOST_CHECK(stream.empty());
}

BOOST_AUTO_TEST_CASE(hybrid_fixed_wire_and_legacy_reuse)
{
    pos::BlockAuthorization authorization;
    authorization.version = pos::HYBRID_POS_OBJECT_VERSION;
    authorization.pq_public_key.assign(pos::MLDSA44_PUBLIC_KEY_SIZE, 1);
    authorization.pq_signature.assign(pos::MLDSA44_SIGNATURE_SIZE, 2);
    BOOST_CHECK_EQUAL(GetSerializeSize(authorization, SER_NETWORK, 0), 4005U);
    CDataStream encoded(SER_NETWORK, 0);
    encoded << authorization;
    pos::BlockAuthorization decoded;
    encoded >> decoded;
    BOOST_CHECK(encoded.empty());
    BOOST_CHECK(decoded.pq_public_key == authorization.pq_public_key);
    BOOST_CHECK(decoded.pq_signature == authorization.pq_signature);
    // Reading an old record into a reused object must clear all PQ fields.
    encoded << pos::BlockAuthorization();
    encoded >> decoded;
    BOOST_CHECK(decoded.pq_public_key.empty());
    BOOST_CHECK(decoded.pq_signature.empty());
    BOOST_CHECK_EQUAL(GetSerializeSize(decoded, SER_NETWORK, 0), 273U);
    auto invalid = authorization;
    invalid.pq_signature.pop_back();
    BOOST_CHECK_THROW(encoded << invalid, std::ios_base::failure);
    encoded.clear();
    invalid = authorization;
    invalid.version = pos::POS_OBJECT_VERSION;
    BOOST_CHECK_THROW(encoded << invalid, std::ios_base::failure);
    encoded.clear();
    encoded << authorization;
    encoded.resize(encoded.size() - 1);
    BOOST_CHECK_THROW(encoded >> decoded, std::ios_base::failure);
    encoded.clear();
    encoded << uint8_t{255};
    BOOST_CHECK_THROW(encoded >> decoded, std::ios_base::failure);
    pos::CheckpointVote vote;
    vote.version = pos::HYBRID_POS_OBJECT_VERSION;
    vote.pq_public_key = authorization.pq_public_key;
    vote.pq_signature = authorization.pq_signature;
    BOOST_CHECK_EQUAL(GetSerializeSize(vote, SER_NETWORK, 0), 3961U);
    pos::BlockEquivocationEvidence block_evidence;
    block_evidence.version = pos::HYBRID_POS_OBJECT_VERSION;
    block_evidence.first = block_evidence.second = authorization;
    BOOST_CHECK_EQUAL(GetSerializeSize(block_evidence, SER_NETWORK, 0), 8011U);
    pos::VoteEquivocationEvidence vote_evidence;
    vote_evidence.version = pos::HYBRID_POS_OBJECT_VERSION;
    vote_evidence.first = vote_evidence.second = vote;
    BOOST_CHECK_EQUAL(GetSerializeSize(vote_evidence, SER_NETWORK, 0), 7923U);
}

BOOST_AUTO_TEST_CASE(tagged_hash_vector)
{
    const uint8_t payload = 0;
    BOOST_CHECK_EQUAL(
        pos::GetTaggedHash(pos::HashDomain::BOND, payload).GetHex(),
        "fcddea5118e33d74d2ea8e5f2c223df59b6acdcde827941f1436766440f4a4d6");
    BOOST_CHECK(pos::GetTaggedHash(pos::HashDomain::BOND, payload) !=
                pos::GetTaggedHash(pos::HashDomain::VOTE, payload));
}

BOOST_AUTO_TEST_CASE(structural_checks)
{
    pos::StakeProof proof;
    BOOST_CHECK(pos::CheckStructure(proof) == pos::StructureError::NULL_BOND_OUTPOINT);

    proof.bond_outpoint = COutPoint(uint256S("01"), 0);
    BOOST_CHECK(pos::CheckStructure(proof) == pos::StructureError::INVALID_SLOT);
    proof.slot = 1;
    BOOST_CHECK(pos::CheckStructure(proof) == pos::StructureError::NULL_COMMITMENT);
    proof.snapshot_root = uint256S("02");
    proof.epoch_seed = uint256S("03");
    BOOST_CHECK(pos::CheckStructure(proof) == pos::StructureError::INVALID_VRF_PUBLIC_KEY);
    proof.vrf_public_key[0] = 0x02;
    std::memset(proof.vrf_output, 1, sizeof(proof.vrf_output));
    std::memset(proof.vrf_proof, 2, sizeof(proof.vrf_proof));
    std::memset(proof.signing_public_key, 3, sizeof(proof.signing_public_key));
    BOOST_CHECK(pos::CheckStructure(proof) == pos::StructureError::NONE);

    pos::CheckpointVote vote;
    vote.bond_outpoint = COutPoint(uint256S("04"), 0);
    vote.source_epoch = 2;
    vote.target_epoch = 1;
    BOOST_CHECK(pos::CheckStructure(vote) == pos::StructureError::INVALID_EPOCH_ORDER);
    vote.source_epoch = 1;
    vote.source_checkpoint_root = uint256S("05");
    vote.target_checkpoint_root = uint256S("06");
    vote.head_slot = 10;
    vote.head_block_root = uint256S("07");
    std::memset(vote.signature, 5, sizeof(vote.signature));
    BOOST_CHECK(pos::CheckStructure(vote) == pos::StructureError::NONE);

    vote.source_epoch = pos::FINAL_POW_CHECKPOINT_EPOCH;
    vote.target_epoch = 0;
    BOOST_CHECK(pos::CheckStructure(vote) == pos::StructureError::NONE);
    vote.target_epoch = pos::FINAL_POW_CHECKPOINT_EPOCH;
    BOOST_CHECK(pos::CheckStructure(vote) == pos::StructureError::INVALID_EPOCH_ORDER);
}

BOOST_AUTO_TEST_CASE(anchor_surround_ordering)
{
    BOOST_CHECK(pos::CheckpointEpochLess(pos::FINAL_POW_CHECKPOINT_EPOCH, 0));
    BOOST_CHECK(!pos::CheckpointEpochLess(0, pos::FINAL_POW_CHECKPOINT_EPOCH));
    BOOST_CHECK(!pos::CheckpointEpochLess(pos::FINAL_POW_CHECKPOINT_EPOCH,
                                        pos::FINAL_POW_CHECKPOINT_EPOCH));
    for (const uint64_t source : {uint64_t{0}, pos::FINAL_POW_CHECKPOINT_EPOCH}) {
        pos::CheckpointVote outer;
        outer.bond_outpoint = COutPoint(uint256S("11"), 0);
        outer.source_epoch = source;
        outer.source_checkpoint_root = uint256S("12");
        outer.target_epoch = 3;
        outer.target_checkpoint_root = uint256S("13");
        outer.head_slot = 10;
        outer.head_block_root = uint256S("14");
        outer.signature[0] = 1;
        pos::CheckpointVote inner = outer;
        inner.source_epoch = 1;
        inner.target_epoch = 2;
        pos::VoteEquivocationEvidence evidence;
        evidence.first = outer;
        evidence.second = inner;
        if (pos::GetVoteSigningHash(evidence.second) < pos::GetVoteSigningHash(evidence.first)) {
            std::swap(evidence.first, evidence.second);
        }
        BOOST_CHECK(pos::CheckStructure(evidence) == pos::StructureError::NONE);
        std::swap(evidence.first, evidence.second);
        BOOST_CHECK(pos::CheckStructure(evidence) == pos::StructureError::NON_CANONICAL_EVIDENCE);
        inner.source_epoch = source;
        evidence.first = outer;
        evidence.second = inner;
        if (pos::GetVoteSigningHash(evidence.second) < pos::GetVoteSigningHash(evidence.first)) {
            std::swap(evidence.first, evidence.second);
        }
        BOOST_CHECK(pos::CheckStructure(evidence) == pos::StructureError::NOT_EQUIVOCATION);
    }
}

BOOST_AUTO_TEST_CASE(signing_hash_excludes_signature)
{
    pos::BlockAuthorization authorization;
    authorization.network_id = 1;
    authorization.parent_block_root = uint256S("01");
    authorization.candidate_header_hash = uint256S("02");
    authorization.slot = 3;
    authorization.bond_outpoint = COutPoint(uint256S("04"), 0);
    authorization.stake_proof_hash = uint256S("05");
    authorization.fee_reward_transaction_hash = uint256S("06");
    authorization.parent_randomness = uint256S("07");

    const uint256 unsigned_hash = pos::GetBlockSigningHash(authorization);
    std::memset(authorization.signature, 1, sizeof(authorization.signature));
    BOOST_CHECK(pos::GetBlockSigningHash(authorization) == unsigned_hash);
    BOOST_CHECK(pos::GetTaggedHash(pos::HashDomain::EQUIVOCATION, authorization) != unsigned_hash);
    BOOST_CHECK(pos::CheckStructure(authorization) == pos::StructureError::NONE);
}

BOOST_AUTO_TEST_CASE(equivocation_structure)
{
    pos::BlockAuthorization first;
    first.network_id = 1;
    first.parent_block_root = uint256S("01");
    first.candidate_header_hash = uint256S("02");
    first.slot = 3;
    first.bond_outpoint = COutPoint(uint256S("04"), 0);
    first.stake_proof_hash = uint256S("05");
    first.fee_reward_transaction_hash = uint256S("06");
    first.parent_randomness = uint256S("07");
    std::memset(first.signature, 1, sizeof(first.signature));

    pos::BlockAuthorization second = first;
    second.candidate_header_hash = uint256S("08");
    std::memset(second.signature, 2, sizeof(second.signature));

    pos::BlockEquivocationEvidence block_evidence;
    block_evidence.first = first;
    block_evidence.second = second;
    if (pos::GetBlockSigningHash(block_evidence.second) <
        pos::GetBlockSigningHash(block_evidence.first)) {
        std::swap(block_evidence.first, block_evidence.second);
    }
    BOOST_CHECK(pos::CheckStructure(block_evidence) == pos::StructureError::NONE);

    block_evidence.second.parent_block_root = uint256S("09");
    if (pos::GetBlockSigningHash(block_evidence.second) <
        pos::GetBlockSigningHash(block_evidence.first)) {
        std::swap(block_evidence.first, block_evidence.second);
    }
    BOOST_CHECK(pos::CheckStructure(block_evidence) ==
                pos::StructureError::NONE);

    pos::CheckpointVote vote_first;
    vote_first.bond_outpoint = COutPoint(uint256S("0a"), 0);
    vote_first.source_checkpoint_root = uint256S("0b");
    vote_first.target_epoch = 2;
    vote_first.target_checkpoint_root = uint256S("0c");
    vote_first.head_slot = 3;
    vote_first.head_block_root = uint256S("0d");
    std::memset(vote_first.signature, 3, sizeof(vote_first.signature));

    pos::CheckpointVote vote_second = vote_first;
    vote_second.target_checkpoint_root = uint256S("0e");
    std::memset(vote_second.signature, 4, sizeof(vote_second.signature));

    pos::VoteEquivocationEvidence vote_evidence;
    vote_evidence.first = vote_first;
    vote_evidence.second = vote_second;
    if (pos::GetVoteSigningHash(vote_evidence.second) <
        pos::GetVoteSigningHash(vote_evidence.first)) {
        std::swap(vote_evidence.first, vote_evidence.second);
    }
    BOOST_CHECK(pos::CheckStructure(vote_evidence) == pos::StructureError::NONE);

    const uint256 block_id = pos::GetBlockEvidenceId(block_evidence);
    const uint256 vote_id = pos::GetVoteEvidenceId(vote_evidence);
    const uint256 block_penalty = pos::GetBlockEvidencePenaltyKey(
        block_evidence.first.bond_outpoint);
    const uint256 vote_penalty = pos::GetVoteEvidencePenaltyKey(
        vote_evidence.first.bond_outpoint);
    block_evidence.first.signature[0] ^= 1;
    vote_evidence.first.signature[0] ^= 1;
    BOOST_CHECK(pos::GetBlockEvidenceId(block_evidence) == block_id);
    BOOST_CHECK(pos::GetVoteEvidenceId(vote_evidence) == vote_id);
    block_evidence.first.parent_randomness = uint256S("10");
    vote_evidence.first.head_block_root = uint256S("11");
    BOOST_CHECK(pos::GetBlockEvidenceId(block_evidence) != block_id);
    BOOST_CHECK(pos::GetVoteEvidenceId(vote_evidence) != vote_id);
    BOOST_CHECK(pos::GetBlockEvidencePenaltyKey(
                    block_evidence.first.bond_outpoint) == block_penalty);
    BOOST_CHECK(pos::GetVoteEvidencePenaltyKey(
                    vote_evidence.first.bond_outpoint) == vote_penalty);
}
BOOST_AUTO_TEST_CASE(canonical_vote_order)
{
    pos::CheckpointVote first;
    first.bond_outpoint = COutPoint(uint256S("01"), 0);
    first.source_checkpoint_root = uint256S("02");
    first.target_checkpoint_root = uint256S("03");
    first.head_slot = 1;
    first.head_block_root = uint256S("04");
    std::memset(first.signature, 1, sizeof(first.signature));

    pos::CheckpointVote second = first;
    second.bond_outpoint = COutPoint(uint256S("02"), 0);

    std::vector<pos::CheckpointVote> votes{first, second};
    BOOST_CHECK(pos::HasCanonicalVoteOrder(votes, 2));
    std::reverse(votes.begin(), votes.end());
    BOOST_CHECK(!pos::HasCanonicalVoteOrder(votes, 2));
    BOOST_CHECK(!pos::HasCanonicalVoteOrder(votes, 1));

    votes = {first, first};
    BOOST_CHECK(!pos::HasCanonicalVoteOrder(votes, 2));
}
BOOST_AUTO_TEST_SUITE_END()
