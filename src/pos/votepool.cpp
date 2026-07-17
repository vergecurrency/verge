// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/votepool.h>

#include <serialize.h>

#include <algorithm>

namespace pos {

VotePool& GetVotePool()
{
    static VotePool pool;
    return pool;
}

VoteEvidencePool& GetVoteEvidencePool()
{
    static VoteEvidencePool pool;
    return pool;
}

VotePoolResult VotePool::Add(const CheckpointVote& vote,
                             VoteEquivocationEvidence* detected)
{
    const uint256 hash = GetVoteSigningHash(vote);
    const size_t serialized_size = GetSerializeSize(vote, SER_NETWORK, 0);
    LOCK(m_mutex);
    if (m_votes.count(hash) != 0) return VotePoolResult::DUPLICATE;

    const auto by_bond = m_by_bond.find(vote.bond_outpoint);
    if (by_bond != m_by_bond.end()) {
        const auto current = m_votes.find(by_bond->second);
        if (current != m_votes.end()) {
            const CheckpointVote& previous = current->second.vote;
            VoteEquivocationEvidence evidence;
            evidence.first = previous;
            evidence.second = vote;
            if (GetTaggedHash(HashDomain::VOTE, evidence.second) <
                GetTaggedHash(HashDomain::VOTE, evidence.first)) {
                std::swap(evidence.first, evidence.second);
            }
            if (CheckStructure(evidence) == StructureError::NONE) {
                if (detected != nullptr) *detected = evidence;
                return VotePoolResult::CONFLICT;
            }
            if (vote.target_epoch < previous.target_epoch ||
                (vote.target_epoch == previous.target_epoch &&
                 vote.head_slot < previous.head_slot)) {
                return VotePoolResult::STALE;
            }
            if (vote.target_epoch == previous.target_epoch &&
                (vote.target_checkpoint_root != previous.target_checkpoint_root ||
                 vote.source_checkpoint_root != previous.source_checkpoint_root)) {
                return VotePoolResult::CONFLICT;
            }
            m_bytes -= current->second.serialized_size;
            m_votes.erase(current);
        }
        m_by_bond.erase(by_bond);
        if (m_bytes + serialized_size > MAX_VOTE_POOL_BYTES) {
            return VotePoolResult::FULL;
        }
        m_votes.emplace(hash, Entry{vote, serialized_size});
        m_by_bond.emplace(vote.bond_outpoint, hash);
        m_bytes += serialized_size;
        return VotePoolResult::REPLACED;
    }

    if (m_votes.size() >= MAX_VOTE_POOL_ENTRIES ||
        m_bytes + serialized_size > MAX_VOTE_POOL_BYTES) {
        return VotePoolResult::FULL;
    }
    m_votes.emplace(hash, Entry{vote, serialized_size});
    m_by_bond.emplace(vote.bond_outpoint, hash);
    m_bytes += serialized_size;
    return VotePoolResult::ADDED;
}

bool VotePool::Get(const uint256& hash, CheckpointVote& vote) const
{
    LOCK(m_mutex);
    const auto it = m_votes.find(hash);
    if (it == m_votes.end()) return false;
    vote = it->second.vote;
    return true;
}

bool VotePool::Exists(const uint256& hash) const
{
    LOCK(m_mutex);
    return m_votes.count(hash) != 0;
}

std::vector<CheckpointVote> VotePool::GetVotes(size_t maximum) const
{
    LOCK(m_mutex);
    std::vector<CheckpointVote> result;
    result.reserve(std::min(maximum, m_votes.size()));
    for (const auto& item : m_votes) {
        if (result.size() == maximum) break;
        result.push_back(item.second.vote);
    }
    std::sort(result.begin(), result.end(), [](const CheckpointVote& a,
                                               const CheckpointVote& b) {
        if (a.bond_outpoint != b.bond_outpoint) {
            return a.bond_outpoint < b.bond_outpoint;
        }
        return GetVoteSigningHash(a) < GetVoteSigningHash(b);
    });
    return result;
}

void VotePool::Remove(const uint256& hash)
{
    LOCK(m_mutex);
    const auto it = m_votes.find(hash);
    if (it == m_votes.end()) return;
    m_by_bond.erase(it->second.vote.bond_outpoint);
    m_bytes -= it->second.serialized_size;
    m_votes.erase(it);
}

void VotePool::Prune(uint64_t current_epoch)
{
    LOCK(m_mutex);
    for (auto it = m_votes.begin(); it != m_votes.end();) {
        const uint64_t target = it->second.vote.target_epoch;
        if (target <= current_epoch && current_epoch - target > VOTE_POOL_EPOCH_TTL) {
            m_by_bond.erase(it->second.vote.bond_outpoint);
            m_bytes -= it->second.serialized_size;
            it = m_votes.erase(it);
        } else {
            ++it;
        }
    }
}

void VotePool::Clear()
{
    LOCK(m_mutex);
    m_votes.clear();
    m_by_bond.clear();
    m_bytes = 0;
}

size_t VotePool::Size() const
{
    LOCK(m_mutex);
    return m_votes.size();
}

size_t VotePool::DynamicMemoryUsage() const
{
    LOCK(m_mutex);
    return m_bytes;
}

VotePoolResult VoteEvidencePool::Add(
    const VoteEquivocationEvidence& evidence)
{
    const uint256 hash = GetTaggedHash(HashDomain::EQUIVOCATION, evidence);
    const size_t serialized_size = GetSerializeSize(evidence, SER_NETWORK, 0);
    LOCK(m_mutex);
    if (m_evidence.count(hash) != 0) return VotePoolResult::DUPLICATE;
    if (m_evidence.size() >= MAX_VOTE_EVIDENCE_POOL_ENTRIES ||
        m_bytes + serialized_size > MAX_VOTE_EVIDENCE_POOL_BYTES) {
        return VotePoolResult::FULL;
    }
    m_evidence.emplace(hash, Entry{evidence, serialized_size});
    m_bytes += serialized_size;
    return VotePoolResult::ADDED;
}

bool VoteEvidencePool::Get(const uint256& hash,
                           VoteEquivocationEvidence& evidence) const
{
    LOCK(m_mutex);
    const auto it = m_evidence.find(hash);
    if (it == m_evidence.end()) return false;
    evidence = it->second.evidence;
    return true;
}

bool VoteEvidencePool::Exists(const uint256& hash) const
{
    LOCK(m_mutex);
    return m_evidence.count(hash) != 0;
}

std::vector<VoteEquivocationEvidence> VoteEvidencePool::GetEvidence(
    size_t maximum) const
{
    LOCK(m_mutex);
    std::vector<VoteEquivocationEvidence> result;
    result.reserve(std::min(maximum, m_evidence.size()));
    for (const auto& item : m_evidence) {
        if (result.size() == maximum) break;
        result.push_back(item.second.evidence);
    }
    return result;
}

void VoteEvidencePool::Remove(const uint256& hash)
{
    LOCK(m_mutex);
    const auto it = m_evidence.find(hash);
    if (it == m_evidence.end()) return;
    m_bytes -= it->second.serialized_size;
    m_evidence.erase(it);
}

void VoteEvidencePool::Clear()
{
    LOCK(m_mutex);
    m_evidence.clear();
    m_bytes = 0;
}

size_t VoteEvidencePool::Size() const
{
    LOCK(m_mutex);
    return m_evidence.size();
}

} // namespace pos
