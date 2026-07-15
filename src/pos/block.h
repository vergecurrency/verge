// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_POS_BLOCK_H
#define VERGE_POS_BLOCK_H

#include <pos/primitives.h>
#include <serialize.h>

#include <cstdint>
#include <ios>
#include <vector>

class CBlock;

namespace pos {

static constexpr uint32_t MAX_VOTES_PER_BLOCK = 1024;
static constexpr uint32_t MAX_EVIDENCE_PER_BLOCK = 16;

struct BlockCommitment {
    uint8_t version{POS_OBJECT_VERSION};
    uint256 stake_proof_hash;
    uint256 vote_root;
    uint256 evidence_root;
    uint256 snapshot_root;
    uint256 epoch_seed;
};
struct BlockExtension {
    uint8_t version{POS_OBJECT_VERSION};
    StakeProof stake_proof;
    BlockAuthorization authorization;
    std::vector<CheckpointVote> votes;
    std::vector<BlockEquivocationEvidence> block_evidence;
    std::vector<VoteEquivocationEvidence> vote_evidence;

    template <typename Stream>
    void Serialize(Stream& stream) const
    {
        ::Serialize(stream, version);
        ::Serialize(stream, stake_proof);
        ::Serialize(stream, authorization);
        WriteVector(stream, votes);
        WriteVector(stream, block_evidence);
        WriteVector(stream, vote_evidence);
    }

    template <typename Stream>
    void Unserialize(Stream& stream)
    {
        ::Unserialize(stream, version);
        ::Unserialize(stream, stake_proof);
        ::Unserialize(stream, authorization);
        ReadVector(stream, votes, MAX_VOTES_PER_BLOCK, "too many PoS votes");
        ReadVector(stream, block_evidence, MAX_EVIDENCE_PER_BLOCK, "too much PoS block evidence");
        ReadVector(stream, vote_evidence,
                   MAX_EVIDENCE_PER_BLOCK - static_cast<uint32_t>(block_evidence.size()),
                   "too much combined PoS evidence");
    }

private:
    template <typename Stream, typename T>
    static void WriteVector(Stream& stream, const std::vector<T>& values)
    {
        WriteCompactSize(stream, values.size());
        for (const T& value : values) {
            ::Serialize(stream, value);
        }
    }

    template <typename Stream, typename T>
    static void ReadVector(Stream& stream, std::vector<T>& values,
                           uint32_t maximum, const char* error)
    {
        const uint64_t count = ReadCompactSize(stream);
        if (count > maximum) {
            throw std::ios_base::failure(error);
        }
        values.clear();
        values.reserve(static_cast<size_t>(count));
        for (uint64_t i = 0; i < count; ++i) {
            values.emplace_back();
            ::Unserialize(stream, values.back());
        }
    }
};

uint256 ComputeVoteRoot(const std::vector<CheckpointVote>& votes);
uint256 ComputeEvidenceRoot(
    const std::vector<BlockEquivocationEvidence>& block_evidence,
    const std::vector<VoteEquivocationEvidence>& vote_evidence);
BlockCommitment GetBlockCommitment(const BlockExtension& extension);
CScript GetBlockCommitmentScript(const BlockCommitment& commitment);
bool ParseBlockCommitmentScript(const CScript& script,
                                BlockCommitment& commitment);StructureError CheckStructure(const BlockExtension& extension);
StructureError CheckLinkage(const BlockExtension& extension, const CBlock& block,
                            uint64_t expected_slot, uint32_t expected_network_id);

} // namespace pos

#endif // VERGE_POS_BLOCK_H
