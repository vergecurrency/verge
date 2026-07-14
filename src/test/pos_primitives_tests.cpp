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

    BOOST_CHECK_EQUAL(GetSerializeSize(proof, SER_NETWORK, 0), 359U);
    BOOST_CHECK_EQUAL(GetSerializeSize(vote, SER_NETWORK, 0), 229U);
    BOOST_CHECK(pos::HasSupportedVersion(proof));
    BOOST_CHECK(pos::HasSupportedVersion(vote));
    BOOST_CHECK(pos::IsPoSVersion(pos::BLOCK_VERSION_POS));
    BOOST_CHECK(!pos::IsPoSVersion(0));

    proof.version = 2;
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
    std::memset(expected.block_signature, 0x08, sizeof(expected.block_signature));

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
    BOOST_CHECK_EQUAL_COLLECTIONS(decoded.block_signature, decoded.block_signature + sizeof(decoded.block_signature), expected.block_signature, expected.block_signature + sizeof(expected.block_signature));
    BOOST_CHECK(stream.empty());
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
    std::memset(proof.block_signature, 4, sizeof(proof.block_signature));
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
