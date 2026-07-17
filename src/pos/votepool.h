// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_POS_VOTEPOOL_H
#define VERGE_POS_VOTEPOOL_H

#include <pos/primitives.h>
#include <sync.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

namespace pos {

static constexpr size_t MAX_VOTE_POOL_ENTRIES = 4096;
static constexpr size_t MAX_VOTE_POOL_BYTES = 2 * 1024 * 1024;
static constexpr uint64_t VOTE_POOL_EPOCH_TTL = 3;
static constexpr size_t SERIALIZED_CHECKPOINT_VOTE_SIZE = 229;
static constexpr size_t SERIALIZED_VOTE_EVIDENCE_SIZE = 459;
static constexpr size_t MAX_VOTE_EVIDENCE_POOL_ENTRIES = 256;
static constexpr size_t MAX_VOTE_EVIDENCE_POOL_BYTES = 256 * 1024;

enum class VotePoolResult {
    ADDED,
    REPLACED,
    DUPLICATE,
    STALE,
    CONFLICT,
    FULL,
};

class VotePool {
public:
    VotePoolResult Add(const CheckpointVote& vote,
                       VoteEquivocationEvidence* detected = nullptr);
    bool Get(const uint256& hash, CheckpointVote& vote) const;
    bool Exists(const uint256& hash) const;
    std::vector<CheckpointVote> GetVotes(size_t maximum) const;
    void Remove(const uint256& hash);
    void Prune(uint64_t current_epoch);
    void Clear();
    size_t Size() const;
    size_t DynamicMemoryUsage() const;

private:
    struct Entry {
        CheckpointVote vote;
        size_t serialized_size{0};
    };

    mutable CCriticalSection m_mutex;
    std::map<uint256, Entry> m_votes GUARDED_BY(m_mutex);
    std::map<COutPoint, uint256> m_by_bond GUARDED_BY(m_mutex);
    size_t m_bytes GUARDED_BY(m_mutex){0};
};

VotePool& GetVotePool();

class VoteEvidencePool {
public:
    VotePoolResult Add(const VoteEquivocationEvidence& evidence);
    bool Get(const uint256& hash, VoteEquivocationEvidence& evidence) const;
    bool Exists(const uint256& hash) const;
    std::vector<VoteEquivocationEvidence> GetEvidence(size_t maximum) const;
    void Remove(const uint256& hash);
    void Clear();
    size_t Size() const;

private:
    struct Entry {
        VoteEquivocationEvidence evidence;
        size_t serialized_size{0};
    };

    mutable CCriticalSection m_mutex;
    std::map<uint256, Entry> m_evidence GUARDED_BY(m_mutex);
    size_t m_bytes GUARDED_BY(m_mutex){0};
};

VoteEvidencePool& GetVoteEvidencePool();

} // namespace pos

#endif // VERGE_POS_VOTEPOOL_H
