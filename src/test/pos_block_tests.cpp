// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/block.h>
#include <pos/crypto.h>
#include <pos/consensus.h>
#include <chainparams.h>
#include <consensus/validation.h>
#include <consensus/merkle.h>
#include <miner.h>
#include <primitives/block.h>
#include <netmessagemaker.h>
#include <protocol.h>
#include <streams.h>
#include <test/setup_common.h>
#include <timedata.h>
#include <validation.h>

#include <boost/test/unit_test.hpp>

#include <cstring>
#include <stdexcept>

extern uint64_t nLastBlockTx;
extern uint64_t nLastBlockWeight;

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

BOOST_AUTO_TEST_CASE(corrupt_extension_does_not_invalidate_committed_header)
{
    CBlock block;
    block.nVersion = 2 | pos::BLOCK_VERSION_POS;
    block.posExtension = MakeExtension();
    CMutableTransaction reward;
    reward.nTime = block.nTime;
    reward.vin.resize(1);
    reward.vin[0].prevout.SetNull();
    reward.vin[0].scriptSig = CScript() << OP_0 << OP_0;
    reward.vout.emplace_back(0, CScript() << OP_TRUE);
    block.vtx.push_back(MakeTransactionRef(reward));
    block.hashMerkleRoot = BlockMerkleRoot(block);
    unsigned char secret[32]{};
    secret[31] = 3;
    auto& proof = block.posExtension.stake_proof;
    auto& authorization = block.posExtension.authorization;
    BOOST_REQUIRE(pos::GetSchnorrPublicKey(secret, proof.signing_public_key));
    authorization.stake_proof_hash = pos::GetTaggedHash(pos::HashDomain::STAKE_PROOF, proof);
    unsigned char auxiliary[32]{};
    unsigned char public_key[32];
    BOOST_REQUIRE(pos::SignSchnorr(secret, pos::GetBlockSigningHash(authorization),
                                   auxiliary, authorization.signature, public_key));
    block.hashPoSData = pos::ComputePoSDataHash(block.posExtension);

    CBlock damaged = block;
    damaged.posExtension.authorization.signature[0] ^= 1;
    BOOST_CHECK(damaged.GetHash() == block.GetHash());
    CValidationState damaged_state;
    BOOST_CHECK(!CheckBlock(damaged, damaged_state, Params().GetConsensus()));
    BOOST_CHECK_EQUAL(damaged_state.GetRejectReason(), "bad-pos-signature");
    BOOST_CHECK(damaged_state.CorruptionPossible());
    CValidationState valid_state;
    BOOST_CHECK(CheckBlock(block, valid_state, Params().GetConsensus()));

    damaged = block;
    damaged.fChecked = false;
    std::fill(std::begin(damaged.posExtension.authorization.signature),
              std::end(damaged.posExtension.authorization.signature), 0);
    CValidationState zero_signature;
    BOOST_CHECK(!CheckBlock(damaged, zero_signature, Params().GetConsensus()));
    BOOST_CHECK_EQUAL(zero_signature.GetRejectReason(), "bad-pos-signature");
    BOOST_CHECK(zero_signature.CorruptionPossible());
    damaged = block;
    damaged.fChecked = false;
    damaged.posExtension.stake_proof.snapshot_root = uint256S("1234");
    CValidationState bad_commitment;
    BOOST_CHECK(!CheckBlock(damaged, bad_commitment, Params().GetConsensus()));
    BOOST_CHECK_EQUAL(bad_commitment.GetRejectReason(), "bad-pos-data-hash");
    BOOST_CHECK(bad_commitment.CorruptionPossible());
    damaged = block;
    damaged.fChecked = false;
    damaged.vchBlockSig.resize(MAX_BLOCK_WEIGHT / WITNESS_SCALE_FACTOR);
    CValidationState bad_legacy_signature;
    BOOST_CHECK(damaged.GetHash() == block.GetHash());
    BOOST_CHECK(!CheckBlock(damaged, bad_legacy_signature, Params().GetConsensus()));
    BOOST_CHECK_EQUAL(bad_legacy_signature.GetRejectReason(), "bad-pos-legacy-signature");
    BOOST_CHECK(bad_legacy_signature.CorruptionPossible());
}

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
    BOOST_CHECK_EQUAL(GetSerializeSize(pos_block, SER_NETWORK, 0), pow_size + 604U);

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

BOOST_AUTO_TEST_CASE(full_block_serialization_ignores_client_version_flag_bits)
{
    static const int CLIENT_VERSION_27 = 27000000;
    static const int OLD_BLOCKHEADERONLY_FLAG = (1 << 17);
    BOOST_REQUIRE((CLIENT_VERSION_27 & OLD_BLOCKHEADERONLY_FLAG) != 0);
    BOOST_REQUIRE((CLIENT_VERSION_27 & SER_BLOCKHEADERONLY) == 0);

    CBlock block;
    block.nVersion = 2 | pos::BLOCK_VERSION_POS;
    block.nBits = 1;
    block.vchBlockSig = {0x30, 0x01, 0x01};
    block.posExtension = MakeExtension();

    CDataStream stream(SER_DISK, CLIENT_VERSION_27);
    stream << block;
    CBlock decoded;
    stream >> decoded;

    BOOST_CHECK(stream.empty());
    BOOST_CHECK(decoded.vchBlockSig == block.vchBlockSig);
    BOOST_CHECK(decoded.posExtension.authorization.stake_proof_hash ==
                block.posExtension.authorization.stake_proof_hash);
}

BOOST_AUTO_TEST_CASE(pos_header_serialization_omits_extension)
{
    CBlock block;
    block.nVersion = 2 | pos::BLOCK_VERSION_POS;
    block.nBits = 0;
    block.nNonce = 0;
    block.posExtension = MakeExtension();

    std::vector<CBlock> headers{block};
    CSerializedNetMsg message = CNetMsgMaker(PROTOCOL_VERSION).Make(
        SER_BLOCKHEADERONLY, NetMsgType::HEADERS, headers);
    BOOST_CHECK_EQUAL(message.data.size(), 115U);
    CDataStream stream(message.data, SER_NETWORK, PROTOCOL_VERSION);

    BOOST_CHECK_EQUAL(ReadCompactSize(stream), 1U);
    CBlock decoded;
    stream.SetVersion(stream.GetVersion() | SER_BLOCKHEADERONLY);
    stream >> decoded;
    BOOST_CHECK_EQUAL(ReadCompactSize(stream), 0U);
    BOOST_CHECK(stream.empty());
    BOOST_CHECK(pos::IsPoSVersion(decoded.nVersion));
    BOOST_CHECK(decoded.vchBlockSig.empty());
    BOOST_CHECK(decoded.posExtension.votes.empty());
    BOOST_CHECK(decoded.posExtension.authorization.stake_proof_hash.IsNull());
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
    block.posExtension.authorization.candidate_header_hash =
        pos::GetPoSHeaderSigningHash(block);
    block.posExtension.authorization.stake_proof_hash =
        pos::GetTaggedHash(pos::HashDomain::STAKE_PROOF,
                           block.posExtension.stake_proof);
    block.posExtension.authorization.fee_reward_transaction_hash =
        block.vtx.front()->GetHash();
    block.hashPoSData = pos::ComputePoSDataHash(block.posExtension);
    block.hash.SetNull();

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

BOOST_AUTO_TEST_CASE(unsigned_pos_authorization_is_committed_to_block_id)
{
    CBlock block;
    block.nVersion = 2 | pos::BLOCK_VERSION_POS;
    block.hashPrevBlock = uint256S("05");
    block.hashMerkleRoot = uint256S("09");
    block.nTime = 30;
    block.posExtension = MakeExtension();
    block.posExtension.authorization.candidate_header_hash =
        pos::GetPoSHeaderSigningHash(block);
    block.hashPoSData = pos::ComputePoSDataHash(block.posExtension);
    block.hash.SetNull();
    const uint256 original = block.GetHash();
    const uint256 signing_hash = pos::GetPoSHeaderSigningHash(block);

    block.posExtension.authorization.signature[1] ^= 1;
    block.hashPoSData = pos::ComputePoSDataHash(block.posExtension);
    block.hash.SetNull();
    BOOST_CHECK(block.GetHash() == original);
    BOOST_CHECK(pos::GetPoSHeaderSigningHash(block) == signing_hash);
    block.posExtension.authorization.parent_randomness = uint256S("aa");
    block.hashPoSData = pos::ComputePoSDataHash(block.posExtension);
    block.hash.SetNull();
    BOOST_CHECK(block.GetHash() != original);
}

BOOST_AUTO_TEST_CASE(valid_signature_variants_have_one_candidate_id)
{
    CBlock first;
    first.nVersion = 2 | pos::BLOCK_VERSION_POS;
    first.posExtension = MakeExtension();
    unsigned char secret[32]{};
    secret[31] = 3;
    auto& proof = first.posExtension.stake_proof;
    auto& authorization = first.posExtension.authorization;
    BOOST_REQUIRE(pos::GetSchnorrPublicKey(secret, proof.signing_public_key));
    authorization.stake_proof_hash = pos::GetTaggedHash(pos::HashDomain::STAKE_PROOF, proof);
    unsigned char auxiliary[32]{};
    unsigned char public_key[32];
    BOOST_REQUIRE(pos::SignSchnorr(secret, pos::GetBlockSigningHash(authorization),
                                   auxiliary, authorization.signature, public_key));
    first.hashPoSData = pos::ComputePoSDataHash(first.posExtension);
    CBlock second = first;
    auxiliary[0] = 1;
    auto& variant = second.posExtension.authorization;
    BOOST_REQUIRE(pos::SignSchnorr(secret, pos::GetBlockSigningHash(variant),
                                   auxiliary, variant.signature, public_key));
    BOOST_CHECK(!std::equal(std::begin(authorization.signature),
                            std::end(authorization.signature), variant.signature));
    BOOST_CHECK(pos::VerifySchnorr(public_key, pos::GetBlockSigningHash(authorization),
                                   authorization.signature));
    BOOST_CHECK(pos::VerifySchnorr(public_key, pos::GetBlockSigningHash(variant), variant.signature));
    second.hashPoSData = pos::ComputePoSDataHash(second.posExtension);
    BOOST_CHECK(first.GetHash() == second.GetHash());

    // Identity serialization is exactly the normal extension encoding with
    // this one signature zeroed, including all CompactSize boundaries.
    for (size_t count : {size_t{0}, size_t{252}, size_t{253}, size_t{1024}}) {
        auto extension = first.posExtension;
        extension.votes.resize(count);
        extension.block_evidence.resize(pos::MAX_EVIDENCE_PER_BLOCK);
        auto reference = extension;
        std::fill(std::begin(reference.authorization.signature),
                  std::end(reference.authorization.signature), 0);
        BOOST_CHECK(pos::ComputePoSDataHash(extension) ==
                    pos::GetTaggedHash(pos::HashDomain::POS_DATA, reference));
        const uint256 original = pos::ComputePoSDataHash(extension);
        extension.stake_proof.vrf_proof[0] ^= 1;
        BOOST_CHECK(pos::ComputePoSDataHash(extension) != original);
        extension = reference;
        extension.block_evidence[0].first.candidate_header_hash = uint256S("abcd");
        BOOST_CHECK(pos::ComputePoSDataHash(extension) != original);
    }
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

BOOST_AUTO_TEST_CASE(hybrid_identity_excludes_all_signature_randomness)
{
    pos::BlockExtension extension = MakeExtension();
    extension.version = pos::HYBRID_POS_OBJECT_VERSION;
    extension.stake_proof.version = pos::PUBLIC_STAKE_PROOF_VERSION;
    std::fill(std::begin(extension.stake_proof.vrf_public_key), std::end(extension.stake_proof.vrf_public_key), 0);
    std::fill(std::begin(extension.stake_proof.vrf_proof), std::end(extension.stake_proof.vrf_proof), 0);
    auto& authorization = extension.authorization;
    authorization.version = extension.version;
    authorization.pq_public_key.assign(pos::MLDSA44_PUBLIC_KEY_SIZE, 1);
    authorization.pq_signature.assign(pos::MLDSA44_SIGNATURE_SIZE, 2);
    authorization.stake_proof_hash = pos::GetTaggedHash(pos::HashDomain::STAKE_PROOF, extension.stake_proof);
    pos::CheckpointVote vote;
    vote.version = extension.version;
    vote.bond_outpoint = extension.stake_proof.bond_outpoint;
    vote.pq_public_key = authorization.pq_public_key;
    vote.pq_signature = authorization.pq_signature;
    extension.votes.push_back(vote);
    pos::BlockEquivocationEvidence evidence;
    evidence.version = extension.version;
    evidence.first = evidence.second = authorization;
    extension.block_evidence.push_back(evidence);
    const uint256 original = pos::ComputePoSDataHash(extension);
    const auto vote_root = pos::ComputeVoteRoot(extension.votes);
    const auto evidence_root = pos::ComputeEvidenceRoot(extension.block_evidence, {});
    authorization.signature[0] ^= 1;
    authorization.pq_signature[0] ^= 1;
    extension.votes[0].signature[0] ^= 1;
    extension.votes[0].pq_signature[0] ^= 1;
    extension.block_evidence[0].first.signature[0] ^= 1;
    extension.block_evidence[0].second.pq_signature[0] ^= 1;
    BOOST_CHECK(pos::ComputePoSDataHash(extension) == original);
    BOOST_CHECK(pos::ComputeVoteRoot(extension.votes) == vote_root);
    BOOST_CHECK(pos::ComputeEvidenceRoot(extension.block_evidence, {}) == evidence_root);
    ++extension.votes[0].head_slot;
    BOOST_CHECK(pos::ComputePoSDataHash(extension) != original);
    BOOST_CHECK(pos::ComputeVoteRoot(extension.votes) != vote_root);
    auto commitment = pos::GetBlockCommitment(extension);
    pos::BlockCommitment decoded;
    BOOST_REQUIRE(pos::ParseBlockCommitmentScript(pos::GetBlockCommitmentScript(commitment), decoded));
    BOOST_CHECK_EQUAL(decoded.version, extension.version);
    CDataStream stream(SER_NETWORK, 0);
    stream << extension.version << extension.stake_proof << authorization;
    WriteCompactSize(stream, pos::MAX_HYBRID_VOTES_PER_BLOCK + 1);
    pos::BlockExtension rejected;
    BOOST_CHECK_THROW(stream >> rejected, std::ios_base::failure);
    BOOST_CHECK(rejected.votes.empty());
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

BOOST_AUTO_TEST_CASE(mixed_evidence_uses_one_fixed_budget)
{
    BOOST_CHECK_EQUAL(pos::MAX_VOTES_PER_BLOCK, 1024U);
    BOOST_CHECK_EQUAL(pos::MAX_EVIDENCE_PER_BLOCK, 16U);
    for (size_t vote_count = 0; vote_count <= pos::MAX_EVIDENCE_PER_BLOCK; ++vote_count) {
        pos::BlockExtension extension = MakeExtension();
        extension.vote_evidence.resize(vote_count);
        extension.block_evidence.resize(pos::RemainingEvidenceBudget(vote_count));
        BOOST_CHECK_EQUAL(extension.vote_evidence.size() + extension.block_evidence.size(), 16U);
        CDataStream stream(SER_NETWORK, 0);
        stream << extension;
        pos::BlockExtension decoded;
        BOOST_CHECK_NO_THROW(stream >> decoded);
        BOOST_CHECK_EQUAL(decoded.vote_evidence.size(), vote_count);
        extension.vote_evidence.emplace_back();
        CDataStream over_limit(SER_NETWORK, 0);
        over_limit << extension;
        BOOST_CHECK_THROW(over_limit >> decoded, std::ios_base::failure);
    }
    BOOST_CHECK_EQUAL(pos::RemainingEvidenceBudget(17), 0U);
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

namespace {

struct PoSAssemblerTestingSetup : RegTestingSetup {
    std::unique_ptr<CChainParams> params{CreateChainParams(CBaseChainParams::REGTEST)};
    CBlockIndex* original_tip{chainActive.Tip()};
    const uint256 replacement_hash{uint256S("02")};
    CBlockIndex replacement;
    uint32_t block_time;
    unsigned char secret[32]{};
    pos::StakeProof proof;

    PoSAssemblerTestingSetup()
    {
        params->UpdatePoSPostQuantumForTesting(false);
        params->UpdatePoSActivationHeight(1);
        block_time = static_cast<uint32_t>(GetAdjustedTime() / 30 * 30);
        replacement.pprev = original_tip;
        replacement.nHeight = original_tip->nHeight + 1;
        replacement.nTime = block_time;
        replacement.nVersion = 2 | pos::BLOCK_VERSION_POS;
        replacement.phashBlock = &replacement_hash;
        replacement.BuildSkip();
        secret[31] = 3;
        proof = pos_block_tests::MakeExtension().stake_proof;
        BOOST_REQUIRE(pos::GetSchnorrPublicKey(secret, proof.signing_public_key));
    }

    ~PoSAssemblerTestingSetup()
    {
        LOCK(cs_main);
        chainActive.SetTip(original_tip);
    }
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(pos_assembler_tests, PoSAssemblerTestingSetup)

BOOST_AUTO_TEST_CASE(discard_proof_when_parent_advances)
{
    LOCK(cs_main);
    const uint256 selected_parent = original_tip->GetBlockHash();
    chainActive.SetTip(&replacement);
    std::unique_ptr<CBlockTemplate> block_template;

    // The arriving parent occupies the slot selected by the staking worker.
    BOOST_CHECK_NO_THROW(block_template = BlockAssembler(*params).CreateNewPoSBlock(
        proof, pos::BondRecord(), secret, block_time,
        selected_parent, [](const pos::BlockAuthorization&) { return true; }));
    BOOST_CHECK(!block_template);

    // A later slot must not silently reuse proof/votes from a stale parent.
    BOOST_CHECK_NO_THROW(block_template = BlockAssembler(*params).CreateNewPoSBlock(
        proof, pos::BondRecord(), secret, block_time + 30,
        selected_parent, [](const pos::BlockAuthorization&) { return true; }));
    BOOST_CHECK(!block_template);
}

BOOST_AUTO_TEST_CASE(discard_proof_when_parent_replaced_at_same_height)
{
    LOCK(cs_main);
    replacement.nTime = block_time - 30;
    chainActive.SetTip(&replacement);
    const uint256 selected_parent = replacement.GetBlockHash();
    const uint256 competing_hash = uint256S("03");
    CBlockIndex competing = replacement;
    competing.phashBlock = &competing_hash;
    chainActive.SetTip(&competing);
    std::unique_ptr<CBlockTemplate> block_template;

    BOOST_CHECK_NO_THROW(block_template = BlockAssembler(*params).CreateNewPoSBlock(
        proof, pos::BondRecord(), secret, block_time,
        selected_parent, [](const pos::BlockAuthorization&) { return true; }));
    BOOST_CHECK(!block_template);
    chainActive.SetTip(&replacement);
}

BOOST_AUTO_TEST_CASE(discard_nonadvancing_slot_with_current_parent)
{
    LOCK(cs_main);
    chainActive.SetTip(&replacement);

    for (const uint32_t timestamp : {block_time - 30, block_time}) {
        std::unique_ptr<CBlockTemplate> block_template;
        BOOST_CHECK_NO_THROW(block_template = BlockAssembler(*params).CreateNewPoSBlock(
            proof, pos::BondRecord(), secret, timestamp,
            replacement.GetBlockHash(), [](const pos::BlockAuthorization&) { return true; }));
        BOOST_CHECK(!block_template);
    }
}

BOOST_AUTO_TEST_CASE(current_parent_and_advancing_slot_still_validate)
{
    LOCK(cs_main);
    chainActive.SetTip(&replacement);

    // A matching parent is not enough to bypass validation of this dummy proof.
    BOOST_CHECK_THROW(BlockAssembler(*params).CreateNewPoSBlock(
        proof, pos::BondRecord(), secret, block_time + 30,
        replacement.GetBlockHash(), [](const pos::BlockAuthorization&) { return true; }), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(reserve_attachment_weight_before_transaction_selection)
{
    LOCK2(cs_main, mempool.cs);
    chainActive.SetTip(&replacement);
    mempool.clear();
    TestMemPoolEntryHelper entry;
    for (uint32_t i = 0; i < 600; ++i) {
        CMutableTransaction tx;
        tx.nTime = block_time;
        tx.vin.emplace_back(COutPoint(uint256S("1234"), i));
        tx.vout.emplace_back(1, CScript() << std::vector<unsigned char>(1000, 0));
        mempool.addUnchecked(tx.GetHash(), entry.Fee(COIN).FromTx(tx));
    }
    BlockAssembler::Options options;
    options.nBlockMaxWeight = 2000000;
    uint64_t empty_count = 0;
    for (const bool attachments : {false, true}) {
        std::vector<pos::CheckpointVote> votes;
        std::vector<pos::BlockEquivocationEvidence> evidence;
        if (attachments) {
            votes.resize(pos::MAX_VOTES_PER_BLOCK);
            evidence.resize(pos::MAX_EVIDENCE_PER_BLOCK);
        }
        bool reached_signing = false;
        // Stop before signing/validation: these are assembly-only fixtures.
        auto block_template = BlockAssembler(*params, options).CreateNewPoSBlock(
            proof, pos::BondRecord(), secret, block_time + 30,
            replacement.GetBlockHash(), [&](const pos::BlockAuthorization&) {
                reached_signing = true;
                BOOST_CHECK(nLastBlockWeight <= options.nBlockMaxWeight);
                BOOST_CHECK(nLastBlockWeight > options.nBlockMaxWeight - 10000);
                return false;
            }, votes, evidence);
        BOOST_CHECK(reached_signing);
        BOOST_CHECK(!block_template);
        if (!attachments) empty_count = nLastBlockTx;
        else BOOST_CHECK(nLastBlockTx < empty_count);
    }
    options.nBlockMaxWeight = 4000;
    bool reached_signing = false;
    auto block_template = BlockAssembler(*params, options).CreateNewPoSBlock(
        proof, pos::BondRecord(), secret, block_time + 30,
        replacement.GetBlockHash(), [&](const pos::BlockAuthorization&) {
            reached_signing = true;
            return false;
        }, std::vector<pos::CheckpointVote>(pos::MAX_VOTES_PER_BLOCK));
    BOOST_CHECK(!reached_signing);
    BOOST_CHECK(!block_template);
    mempool.clear();
}

BOOST_AUTO_TEST_CASE(reserve_hybrid_attachment_weight_before_transaction_selection)
{
    if (!pos::experimental::MLDSA44Available()) return;
    LOCK2(cs_main, mempool.cs);
    chainActive.SetTip(&replacement);
    params->UpdatePoSPostQuantumForTesting(true);
    mempool.clear();
    TestMemPoolEntryHelper entry;
    for (uint32_t i = 0; i < 700; ++i) {
        CMutableTransaction tx;
        tx.nTime = block_time;
        tx.vin.emplace_back(COutPoint(uint256S("1234"), i));
        tx.vout.emplace_back(1, CScript() << std::vector<unsigned char>(1000, 0));
        mempool.addUnchecked(tx.GetHash(), entry.Fee(COIN).FromTx(tx));
    }
    pos::experimental::MLDSA44Key key;
    BOOST_REQUIRE(key.MakeNewKey());
    std::vector<unsigned char> public_key;
    BOOST_REQUIRE(key.GetPublicKey(public_key));
    pos::BondRecord bond;
    bond.data.version = 2;
    bond.data.pq_signing_key_id = pos::experimental::GetMLDSA44KeyId(public_key);
    bond.data.pq_reward_key_id = bond.data.pq_signing_key_id;
    bond.data.pq_withdrawal_key_id = bond.data.pq_signing_key_id;
    pos::CheckpointVote vote;
    vote.version = 2;
    vote.pq_public_key = public_key;
    vote.pq_signature.resize(pos::MLDSA44_SIGNATURE_SIZE);
    pos::BlockEquivocationEvidence evidence;
    evidence.version = evidence.first.version = evidence.second.version = 2;
    evidence.first.pq_public_key = evidence.second.pq_public_key = public_key;
    evidence.first.pq_signature.resize(pos::MLDSA44_SIGNATURE_SIZE);
    evidence.second.pq_signature.resize(pos::MLDSA44_SIGNATURE_SIZE);
    const std::vector<pos::CheckpointVote> votes(pos::MAX_HYBRID_VOTES_PER_BLOCK, vote);
    const std::vector<pos::BlockEquivocationEvidence> proofs(pos::MAX_EVIDENCE_PER_BLOCK, evidence);
    BlockAssembler::Options options;
    options.nBlockMaxWeight = 3996000;
    bool reached = false;
    // Assembly-only fixture: stop before signing these synthetic attachments.
    BOOST_CHECK(!BlockAssembler(*params, options).CreateNewPoSBlock(
        proof, bond, secret, block_time + 30, replacement.GetBlockHash(),
        [&](const pos::BlockAuthorization& authorization) {
            reached = true;
            BOOST_CHECK_EQUAL(authorization.version, 2);
            BOOST_CHECK_EQUAL(authorization.pq_public_key.size(), 1312U);
            BOOST_CHECK_EQUAL(authorization.pq_signature.size(), 2420U);
            BOOST_CHECK(nLastBlockWeight <= options.nBlockMaxWeight);
            BOOST_CHECK(nLastBlockWeight > options.nBlockMaxWeight - 10000);
            return false;
        }, votes, proofs, {}, &key));
    BOOST_CHECK(reached);
    reached = false;
    options.nBlockMaxWeight = 2000000;
    BOOST_CHECK(!BlockAssembler(*params, options).CreateNewPoSBlock(
        proof, bond, secret, block_time + 30, replacement.GetBlockHash(),
        [&](const pos::BlockAuthorization&) { reached = true; return false; }, votes, proofs, {}, &key));
    BOOST_CHECK(!reached);
    mempool.clear();
}

BOOST_AUTO_TEST_CASE(rejected_candidate_does_not_allocate_index_or_block_data)
{
    CBlock block;
    block.nVersion = 2 | pos::BLOCK_VERSION_POS;
    block.hashPrevBlock = original_tip->GetBlockHash();
    block.nTime = block_time;
    block.nBits = 0;
    block.nNonce = 0;
    block.posExtension = pos_block_tests::MakeExtension();
    pos::SlotInfo slot;
    BOOST_REQUIRE(pos::GetSlotInfo(original_tip->nTime, block_time,
                                   params->GetConsensus(), slot));
    block.posExtension.stake_proof.slot = slot.global_slot;
    std::copy(proof.signing_public_key,
              proof.signing_public_key + pos::SCHNORR_PUBLIC_KEY_SIZE,
              block.posExtension.stake_proof.signing_public_key);
    CMutableTransaction reward;
    reward.nVersion = CTransaction::POS_REPLAY_PROTECTED_VERSION;
    reward.nTime = block_time;
    reward.vin.resize(1);
    reward.vin[0].prevout.SetNull();
    reward.vin[0].scriptSig = CScript() << 1 << OP_0;
    reward.vout.emplace_back(0, CScript() << OP_TRUE);
    reward.vout.emplace_back(0, pos::GetBlockCommitmentScript(
        pos::GetBlockCommitment(block.posExtension)));
    block.vtx.push_back(MakeTransactionRef(reward));
    block.hashMerkleRoot = BlockMerkleRoot(block);
    auto& authorization = block.posExtension.authorization;
    authorization.network_id = params->GetConsensus().nPoSNetworkId;
    authorization.parent_block_root = block.hashPrevBlock;
    authorization.slot = slot.global_slot;
    authorization.candidate_header_hash = pos::GetPoSHeaderSigningHash(block);
    authorization.stake_proof_hash = pos::GetTaggedHash(
        pos::HashDomain::STAKE_PROOF, block.posExtension.stake_proof);
    authorization.fee_reward_transaction_hash = reward.GetHash();
    unsigned char auxiliary[32]{};
    unsigned char public_key[32];
    BOOST_REQUIRE(pos::SignSchnorr(secret, pos::GetBlockSigningHash(authorization),
                                   auxiliary, authorization.signature, public_key));
    block.hashPoSData = pos::ComputePoSDataHash(block.posExtension);
    CValidationState state;
    BOOST_REQUIRE(CheckBlock(block, state, params->GetConsensus()));
    const size_t index_size = mapBlockIndex.size();
    const auto file_size = GetBlockFileInfo(0)->nSize;
    // The isolated fixture has no stake history. Full authentication must
    // fail before this structurally well-formed candidate is persisted.
    BOOST_CHECK(!ProcessNewBlock(*params, std::make_shared<const CBlock>(block), true, nullptr));
    BOOST_CHECK_EQUAL(mapBlockIndex.size(), index_size);
    BOOST_CHECK(LookupBlockIndex(block.GetHash()) == nullptr);
    BOOST_CHECK_EQUAL(GetBlockFileInfo(0)->nSize, file_size);
}

BOOST_AUTO_TEST_SUITE_END()
