// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/block.h>
#include <primitives/block.h>

namespace pos {

namespace {

template <typename T>
bool HasCanonicalEvidenceOrder(const std::vector<T>& evidence)
{
    for (size_t i = 1; i < evidence.size(); ++i) {
        if (!(GetTaggedHash(HashDomain::EQUIVOCATION, evidence[i - 1]) <
              GetTaggedHash(HashDomain::EQUIVOCATION, evidence[i]))) {
            return false;
        }
    }
    return true;
}

} // namespace

uint256 ComputeVoteRoot(const std::vector<CheckpointVote>& votes)
{
    TaggedHashWriter writer(HashDomain::VOTE_ROOT);
    writer << static_cast<uint64_t>(votes.size());
    for (const CheckpointVote& vote : votes) writer << vote;
    return writer.GetHash();
}

uint256 ComputeEvidenceRoot(
    const std::vector<BlockEquivocationEvidence>& block_evidence,
    const std::vector<VoteEquivocationEvidence>& vote_evidence)
{
    TaggedHashWriter writer(HashDomain::EVIDENCE_ROOT);
    writer << static_cast<uint64_t>(block_evidence.size());
    for (const BlockEquivocationEvidence& evidence : block_evidence) {
        writer << evidence;
    }
    writer << static_cast<uint64_t>(vote_evidence.size());
    for (const VoteEquivocationEvidence& evidence : vote_evidence) {
        writer << evidence;
    }
    return writer.GetHash();
}

BlockCommitment GetBlockCommitment(const BlockExtension& extension)
{
    BlockCommitment commitment;
    commitment.stake_proof_hash =
        GetTaggedHash(HashDomain::STAKE_PROOF, extension.stake_proof);
    commitment.vote_root = ComputeVoteRoot(extension.votes);
    commitment.evidence_root =
        ComputeEvidenceRoot(extension.block_evidence, extension.vote_evidence);
    commitment.snapshot_root = extension.stake_proof.snapshot_root;
    commitment.epoch_seed = extension.stake_proof.epoch_seed;
    return commitment;
}

CScript GetBlockCommitmentScript(const BlockCommitment& commitment)
{
    std::vector<unsigned char> payload;
    payload.reserve(164);
    payload.insert(payload.end(), {'V', 'P', 'C', commitment.version});
    payload.insert(payload.end(), commitment.stake_proof_hash.begin(),
                   commitment.stake_proof_hash.end());
    payload.insert(payload.end(), commitment.vote_root.begin(),
                   commitment.vote_root.end());
    payload.insert(payload.end(), commitment.evidence_root.begin(),
                   commitment.evidence_root.end());
    payload.insert(payload.end(), commitment.snapshot_root.begin(),
                   commitment.snapshot_root.end());
    payload.insert(payload.end(), commitment.epoch_seed.begin(),
                   commitment.epoch_seed.end());
    return CScript() << OP_RETURN << payload;
}

bool ParseBlockCommitmentScript(const CScript& script,
                                BlockCommitment& commitment)
{
    CScript::const_iterator it = script.begin();
    opcodetype opcode;
    std::vector<unsigned char> payload;
    if (!script.GetOp(it, opcode) || opcode != OP_RETURN ||
        !script.GetOp(it, opcode, payload) || it != script.end() ||
        payload.size() != 164 || payload[0] != 'V' || payload[1] != 'P' ||
        payload[2] != 'C' || payload[3] != POS_OBJECT_VERSION) {
        return false;
    }

    commitment = BlockCommitment();
    size_t offset = 4;
    std::copy_n(payload.begin() + offset, 32,
                commitment.stake_proof_hash.begin());
    offset += 32;
    std::copy_n(payload.begin() + offset, 32, commitment.vote_root.begin());
    offset += 32;
    std::copy_n(payload.begin() + offset, 32,
                commitment.evidence_root.begin());
    offset += 32;
    std::copy_n(payload.begin() + offset, 32,
                commitment.snapshot_root.begin());
    offset += 32;
    std::copy_n(payload.begin() + offset, 32, commitment.epoch_seed.begin());
    return true;
}
StructureError CheckStructure(const BlockExtension& extension)
{
    if (extension.version != POS_OBJECT_VERSION) return StructureError::UNSUPPORTED_VERSION;
    if (CheckStructure(extension.stake_proof) != StructureError::NONE) {
        return StructureError::INVALID_AUTHORIZATION;
    }
    if (CheckStructure(extension.authorization) != StructureError::NONE) {
        return StructureError::INVALID_AUTHORIZATION;
    }
    if (extension.authorization.bond_outpoint != extension.stake_proof.bond_outpoint ||
        extension.authorization.slot != extension.stake_proof.slot ||
        extension.authorization.stake_proof_hash !=
            GetTaggedHash(HashDomain::STAKE_PROOF, extension.stake_proof)) {
        return StructureError::INVALID_AUTHORIZATION;
    }
    if (!HasCanonicalVoteOrder(extension.votes, MAX_VOTES_PER_BLOCK)) {
        return StructureError::INVALID_AUTHORIZATION;
    }
    for (const CheckpointVote& vote : extension.votes) {
        if (CheckStructure(vote) != StructureError::NONE) {
            return StructureError::INVALID_AUTHORIZATION;
        }
    }
    if (extension.block_evidence.size() > MAX_EVIDENCE_PER_BLOCK ||
        extension.vote_evidence.size() >
            MAX_EVIDENCE_PER_BLOCK - extension.block_evidence.size()) {
        return StructureError::NON_CANONICAL_EVIDENCE;
    }
    if (!HasCanonicalEvidenceOrder(extension.block_evidence) ||
        !HasCanonicalEvidenceOrder(extension.vote_evidence)) {
        return StructureError::NON_CANONICAL_EVIDENCE;
    }
    for (const BlockEquivocationEvidence& evidence : extension.block_evidence) {
        if (CheckStructure(evidence) != StructureError::NONE) {
            return StructureError::NOT_EQUIVOCATION;
        }
    }
    for (const VoteEquivocationEvidence& evidence : extension.vote_evidence) {
        if (CheckStructure(evidence) != StructureError::NONE) {
            return StructureError::NOT_EQUIVOCATION;
        }
    }
    return StructureError::NONE;
}

StructureError CheckLinkage(const BlockExtension& extension, const CBlock& block,
                            uint64_t expected_slot, uint32_t expected_network_id)
{
    if (block.vtx.empty()) return StructureError::INVALID_AUTHORIZATION;

    const StakeProof& proof = extension.stake_proof;
    const BlockAuthorization& authorization = extension.authorization;
    if (proof.slot != expected_slot ||
        authorization.slot != expected_slot ||
        authorization.network_id != expected_network_id ||
        authorization.parent_block_root != block.hashPrevBlock ||
        authorization.candidate_header_hash != block.GetHash() ||
        authorization.bond_outpoint != proof.bond_outpoint ||
        authorization.stake_proof_hash !=
            GetTaggedHash(HashDomain::STAKE_PROOF, proof) ||
        authorization.fee_reward_transaction_hash != block.vtx.front()->GetHash()) {
        return StructureError::INVALID_AUTHORIZATION;
    }
    return StructureError::NONE;
}
} // namespace pos
