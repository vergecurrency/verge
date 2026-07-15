// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/block.h>
#include <primitives/block.h>
#include <streams.h>
#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <cstring>

BOOST_FIXTURE_TEST_SUITE(pos_block_tests, BasicTestingSetup)

namespace {

pos::BlockExtension MakeExtension()
{
    pos::BlockExtension extension;
    extension.stake_proof.bond_outpoint = COutPoint(uint256S("01"), 0);
    extension.stake_proof.slot = 2;
    extension.stake_proof.snapshot_root = uint256S("03");
    extension.stake_proof.epoch_seed = uint256S("04");
    extension.stake_proof.vrf_public_key[0] = 0x02;
    std::memset(extension.stake_proof.vrf_output, 1, sizeof(extension.stake_proof.vrf_output));
    std::memset(extension.stake_proof.vrf_proof, 2, sizeof(extension.stake_proof.vrf_proof));
    std::memset(extension.stake_proof.signing_public_key, 3, sizeof(extension.stake_proof.signing_public_key));

    extension.authorization.network_id = 1;
    extension.authorization.parent_block_root = uint256S("05");
    extension.authorization.candidate_header_hash = uint256S("06");
    extension.authorization.slot = extension.stake_proof.slot;
    extension.authorization.bond_outpoint = extension.stake_proof.bond_outpoint;
    extension.authorization.stake_proof_hash =
        pos::GetTaggedHash(pos::HashDomain::STAKE_PROOF, extension.stake_proof);
    extension.authorization.fee_reward_transaction_hash = uint256S("07");
    extension.authorization.parent_randomness = uint256S("08");
    std::memset(extension.authorization.signature, 4, sizeof(extension.authorization.signature));
    return extension;
}

} // namespace

BOOST_AUTO_TEST_CASE(empty_collection_size_and_round_trip)
{
    const pos::BlockExtension expected = MakeExtension();
    BOOST_CHECK_EQUAL(GetSerializeSize(expected, SER_NETWORK, 0), 572U);
    BOOST_CHECK(pos::CheckStructure(expected) == pos::StructureError::NONE);

    CDataStream stream(SER_NETWORK, 0);
    stream << expected;
    pos::BlockExtension decoded;
    stream >> decoded;

    BOOST_CHECK(stream.empty());
    BOOST_CHECK_EQUAL(decoded.version, expected.version);
    BOOST_CHECK(decoded.stake_proof.bond_outpoint == expected.stake_proof.bond_outpoint);
    BOOST_CHECK(decoded.authorization.stake_proof_hash == expected.authorization.stake_proof_hash);
    BOOST_CHECK(decoded.votes.empty());
    BOOST_CHECK(decoded.block_evidence.empty());
    BOOST_CHECK(decoded.vote_evidence.empty());
    BOOST_CHECK(pos::CheckStructure(decoded) == pos::StructureError::NONE);
}

BOOST_AUTO_TEST_CASE(block_commitment_round_trip)
{
    const pos::BlockExtension extension = MakeExtension();
    const pos::BlockCommitment expected =
        pos::GetBlockCommitment(extension);
    const CScript script = pos::GetBlockCommitmentScript(expected);

    pos::BlockCommitment decoded;
    BOOST_REQUIRE(pos::ParseBlockCommitmentScript(script, decoded));
    BOOST_CHECK(decoded.stake_proof_hash == expected.stake_proof_hash);
    BOOST_CHECK(decoded.vote_root == expected.vote_root);
    BOOST_CHECK(decoded.evidence_root == expected.evidence_root);
    BOOST_CHECK(decoded.snapshot_root == expected.snapshot_root);
    BOOST_CHECK(decoded.epoch_seed == expected.epoch_seed);

    CScript trailing = script;
    trailing << OP_DROP;
    BOOST_CHECK(!pos::ParseBlockCommitmentScript(trailing, decoded));
}
BOOST_AUTO_TEST_CASE(full_block_serialization_is_version_gated)
{
    CBlock pow_block;
    pow_block.nVersion = 2;
    pow_block.nBits = 1;
    const unsigned int pow_size = GetSerializeSize(pow_block, SER_NETWORK, 0);

    pow_block.posExtension = MakeExtension();
    BOOST_CHECK_EQUAL(GetSerializeSize(pow_block, SER_NETWORK, 0), pow_size);

    CBlock pos_block;
    pos_block.nVersion = 2 | pos::BLOCK_VERSION_POS;
    pos_block.nBits = 1;
    pos_block.posExtension = MakeExtension();
    BOOST_CHECK_EQUAL(GetSerializeSize(pos_block, SER_NETWORK, 0), pow_size + 572U);

    CDataStream stream(SER_NETWORK, 0);
    stream << pos_block;
    CBlock decoded;
    stream >> decoded;
    BOOST_CHECK(stream.empty());
    BOOST_CHECK(pos::IsPoSVersion(decoded.nVersion));
    BOOST_CHECK(decoded.posExtension.authorization.stake_proof_hash ==
                pos_block.posExtension.authorization.stake_proof_hash);
    BOOST_CHECK(pos::CheckStructure(decoded.posExtension) == pos::StructureError::NONE);
}
BOOST_AUTO_TEST_CASE(full_block_linkage)
{
    CMutableTransaction reward;
    reward.vin.resize(1);
    reward.vin[0].prevout.SetNull();
    reward.vout.emplace_back(0, CScript() << OP_TRUE);

    CBlock block;
    block.nVersion = 2 | pos::BLOCK_VERSION_POS;
    block.hashPrevBlock = uint256S("05");
    block.hashMerkleRoot = uint256S("09");
    block.nTime = 30;
    block.vtx.push_back(MakeTransactionRef(reward));
    block.posExtension = MakeExtension();
    block.posExtension.stake_proof.slot = 10;
    block.posExtension.authorization.slot = 10;
    block.posExtension.authorization.parent_block_root = block.hashPrevBlock;
    block.posExtension.authorization.candidate_header_hash = block.GetHash();
    block.posExtension.authorization.stake_proof_hash =
        pos::GetTaggedHash(pos::HashDomain::STAKE_PROOF,
                           block.posExtension.stake_proof);
    block.posExtension.authorization.fee_reward_transaction_hash =
        block.vtx.front()->GetHash();

    BOOST_CHECK(pos::CheckLinkage(block.posExtension, block, 10, 1) ==
                pos::StructureError::NONE);

    block.posExtension.authorization.network_id = 2;
    BOOST_CHECK(pos::CheckLinkage(block.posExtension, block, 10, 1) ==
                pos::StructureError::INVALID_AUTHORIZATION);

    block.posExtension.authorization.network_id = 1;
    block.posExtension.authorization.candidate_header_hash = uint256S("10");
    BOOST_CHECK(pos::CheckLinkage(block.posExtension, block, 10, 1) ==
                pos::StructureError::INVALID_AUTHORIZATION);
}
BOOST_AUTO_TEST_CASE(reject_vote_count_before_allocation)
{
    const pos::BlockExtension expected = MakeExtension();
    CDataStream stream(SER_NETWORK, 0);
    stream << expected.version << expected.stake_proof << expected.authorization;
    WriteCompactSize(stream, pos::MAX_VOTES_PER_BLOCK + 1);

    pos::BlockExtension decoded;
    BOOST_CHECK_THROW(stream >> decoded, std::ios_base::failure);
    BOOST_CHECK(decoded.votes.empty());
}

BOOST_AUTO_TEST_CASE(reject_combined_evidence_count)
{
    const pos::BlockExtension expected = MakeExtension();
    CDataStream stream(SER_NETWORK, 0);
    stream << expected.version << expected.stake_proof << expected.authorization;
    WriteCompactSize(stream, 0);
    WriteCompactSize(stream, pos::MAX_EVIDENCE_PER_BLOCK);
    for (uint32_t i = 0; i < pos::MAX_EVIDENCE_PER_BLOCK; ++i) {
        stream << pos::BlockEquivocationEvidence();
    }
    WriteCompactSize(stream, 1);

    pos::BlockExtension decoded;
    BOOST_CHECK_THROW(stream >> decoded, std::ios_base::failure);
    BOOST_CHECK_EQUAL(decoded.block_evidence.size(), pos::MAX_EVIDENCE_PER_BLOCK);
    BOOST_CHECK(decoded.vote_evidence.empty());
}

BOOST_AUTO_TEST_CASE(reject_mismatched_authorization)
{
    pos::BlockExtension extension = MakeExtension();
    extension.authorization.slot++;
    BOOST_CHECK(pos::CheckStructure(extension) == pos::StructureError::INVALID_AUTHORIZATION);

    extension = MakeExtension();
    extension.authorization.stake_proof_hash = uint256S("09");
    BOOST_CHECK(pos::CheckStructure(extension) == pos::StructureError::INVALID_AUTHORIZATION);
}

BOOST_AUTO_TEST_SUITE_END()
