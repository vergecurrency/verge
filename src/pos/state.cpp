// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/state.h>

#include <coins.h>
#include <pos/consensus.h>
#include <pos/primitives.h>
#include <primitives/block.h>

#include <algorithm>
#include <limits>
#include <tuple>

namespace pos {

bool State::InitializeFromCoins(const CCoinsView& coins,
                                const uint256& best_block)
{
    if (!m_best_block.IsNull() || !m_bonds.empty() || !m_bond_history.empty() ||
        !m_snapshots.empty() ||
        !m_epoch_seeds.empty() || !m_vrf_contributions.empty() ||
        !m_lockouts.empty() || !m_applied_evidence.empty() ||
        m_has_pos || best_block.IsNull()) {
        return false;
    }
    std::unique_ptr<CCoinsViewCursor> cursor(coins.Cursor());
    if (!cursor) return false;
    while (cursor->Valid()) {
        COutPoint outpoint;
        Coin coin;
        if (!cursor->GetKey(outpoint) || !cursor->GetValue(coin)) return false;
        BondData data;
        if (!coin.IsSpent() && ParseBondScript(coin.out.scriptPubKey, data)) {
            BondRecord record;
            record.value = coin.out.nValue;
            record.creation_height = coin.nHeight;
            record.data = data;
            if (!m_bonds.emplace(outpoint, record).second ||
                !m_bond_history.emplace(outpoint, record).second) return false;
        }
        cursor->Next();
    }
    m_best_block = best_block;
    return true;
}

uint256 ComputeSnapshotRoot(const std::vector<SnapshotEntry>& entries)
{
    TaggedHashWriter writer(HashDomain::STAKE_ROOT);
    writer << static_cast<uint64_t>(entries.size());
    for (const SnapshotEntry& entry : entries) {
        writer << entry;
    }
    return writer.GetHash();
}

const BondRecord* State::FindBond(const COutPoint& outpoint) const
{
    const auto it = m_bonds.find(outpoint);
    return it == m_bonds.end() ? nullptr : &it->second;
}

const BondRecord* State::FindHistoricalBond(const COutPoint& outpoint) const
{
    const auto it = m_bond_history.find(outpoint);
    return it == m_bond_history.end() ? nullptr : &it->second;
}

const StakeSnapshot* State::FindSnapshot(uint64_t source_epoch) const
{
    const auto it = m_snapshots.find(source_epoch);
    return it == m_snapshots.end() ? nullptr : &it->second;
}

const Checkpoint* State::FindCheckpoint(uint64_t epoch) const
{
    const auto it = m_checkpoints.find(epoch);
    return it == m_checkpoints.end() ? nullptr : &it->second;
}

const Checkpoint* State::FindJustified(uint64_t epoch) const
{
    const auto it = m_justified.find(epoch);
    return it == m_justified.end() ? nullptr : &it->second;
}

const Checkpoint* State::LatestCheckpoint() const
{
    const Checkpoint* latest = nullptr;
    for (const auto& item : m_checkpoints) {
        if (item.first != FINAL_POW_CHECKPOINT_EPOCH) latest = &item.second;
    }
    return latest != nullptr ? latest
                             : FindCheckpoint(FINAL_POW_CHECKPOINT_EPOCH);
}

const Checkpoint* State::LatestJustified() const
{
    const Checkpoint* latest = nullptr;
    for (const auto& item : m_justified) {
        if (item.first != FINAL_POW_CHECKPOINT_EPOCH) latest = &item.second;
    }
    return latest != nullptr ? latest
                             : FindJustified(FINAL_POW_CHECKPOINT_EPOCH);
}

CAmount State::GetVoteWeight(const CheckpointVote& vote) const
{
    const StakeSnapshot* snapshot = FindSnapshot(vote.snapshot_epoch);
    if (snapshot == nullptr) return 0;
    const auto bond = std::lower_bound(
        snapshot->entries.begin(), snapshot->entries.end(),
        vote.bond_outpoint,
        [](const SnapshotEntry& entry, const COutPoint& outpoint) {
            return entry.outpoint < outpoint;
        });
    return bond != snapshot->entries.end() &&
                   bond->outpoint == vote.bond_outpoint
        ? bond->bond.value : 0;
}

bool State::IsEligibilityLocked(const COutPoint& outpoint, uint64_t epoch,
                                int32_t height) const
{
    const auto it = m_lockouts.find(outpoint);
    if (it == m_lockouts.end() || epoch < it->second.activation_epoch) {
        return false;
    }
    return it->second.until_height == 0 || height < it->second.until_height;
}

bool State::IsWithdrawalLocked(const COutPoint& outpoint, int32_t height) const
{
    const auto it = m_lockouts.find(outpoint);
    return it != m_lockouts.end() &&
           (it->second.until_height == 0 || height < it->second.until_height);
}

const uint256* State::FindEpochSeed(uint64_t epoch) const
{
    const auto it = m_epoch_seeds.find(epoch);
    return it == m_epoch_seeds.end() ? nullptr : &it->second;
}

bool State::SetEpochSeed(uint64_t epoch, const uint256& seed, StateUndo& undo)
{
    if (seed.IsNull() || m_epoch_seeds.count(epoch) != 0) return false;
    m_epoch_seeds.emplace(epoch, seed);
    undo.added_epoch_seeds.push_back(epoch);
    return true;
}

bool State::AddSnapshot(uint64_t source_epoch, int32_t source_height,
                        int32_t maturity_height, const Consensus::Params& params,
                        StateUndo& undo)
{
    if (m_snapshots.count(source_epoch) != 0 || maturity_height < 0) return false;

    StakeSnapshot snapshot;
    snapshot.source_epoch = source_epoch;
    snapshot.source_height = source_height;
    for (const auto& item : m_bonds) {
        const BondRecord& bond = item.second;
        const int64_t depth = static_cast<int64_t>(maturity_height) -
                              bond.creation_height;
        if (depth < params.nPoSStakeMaturity) continue;
        const uint64_t eligibility_epoch =
            source_epoch == INITIAL_SNAPSHOT_EPOCH ? 0 : source_epoch + 1;
        if (IsEligibilityLocked(item.first, eligibility_epoch,
                                maturity_height)) continue;
        if (bond.value < params.nPoSMinStake ||
            !MoneyRange(snapshot.total_value + bond.value)) {
            return false;
        }
        snapshot.total_value += bond.value;
        snapshot.entries.push_back(SnapshotEntry{item.first, bond});
    }
    snapshot.root = ComputeSnapshotRoot(snapshot.entries);
    m_snapshots.emplace(source_epoch, std::move(snapshot));
    undo.added_snapshots.push_back(source_epoch);
    return true;
}

bool State::PrepareEpoch(uint64_t block_epoch, int32_t height,
                         const Consensus::Params& params, StateUndo& undo)
{
    if (m_has_pos && block_epoch < m_last_pos_epoch) return false;
    for (auto& item : m_lockouts) {
        LockoutRecord& lockout = item.second;
        if (lockout.until_height != 0 ||
            block_epoch < lockout.activation_epoch) continue;
        LockoutUndo change;
        change.bond_outpoint = item.first;
        change.had_previous = true;
        change.previous = lockout;
        undo.lockout_undo.push_back(change);
        const int64_t until = static_cast<int64_t>(height) +
                              params.nPoSUnbondingBlocks;
        if (until > std::numeric_limits<int32_t>::max()) return false;
        lockout.until_height = static_cast<int32_t>(until);
        const auto vote = m_latest_votes.find(item.first);
        if (vote != m_latest_votes.end()) {
            VoteUndo vote_change;
            vote_change.bond_outpoint = item.first;
            vote_change.had_previous = true;
            vote_change.previous = vote->second;
            undo.vote_undo.push_back(vote_change);
            m_latest_votes.erase(vote);
        }
    }
    uint64_t next_epoch = m_has_pos ? m_last_pos_epoch + 1 : 1;
    while (next_epoch <= block_epoch) {
        const uint64_t completed_epoch = next_epoch - 1;
        if (!AddSnapshot(completed_epoch, height - 1, height - 1,
                         params, undo)) return false;
        const uint256* previous_seed = FindEpochSeed(completed_epoch);
        if (previous_seed == nullptr) return false;
        std::vector<std::array<unsigned char, 32>> outputs;
        const auto contributions = m_vrf_contributions.find(completed_epoch);
        if (contributions != m_vrf_contributions.end()) {
            outputs.reserve(contributions->second.size());
            for (const uint256& value : contributions->second) {
                std::array<unsigned char, 32> output{};
                std::reverse_copy(value.begin(), value.end(), output.begin());
                outputs.push_back(output);
            }
        }
        if (!SetEpochSeed(next_epoch,
                          ComputeNextEpochSeed(*previous_seed, next_epoch,
                                               outputs), undo)) return false;
        ++next_epoch;
    }
    return true;
}

bool State::ApplyBlock(const CBlock& block, int32_t height, bool is_pos,
                       uint64_t block_epoch, uint32_t slot_in_epoch,
                       const Consensus::Params& params, StateUndo& undo)
{
    undo.previous_best_block = m_best_block;
    undo.previous_has_pos = m_has_pos;
    undo.previous_pos_epoch = m_last_pos_epoch;

    if (is_pos) {
        const bool new_checkpoint = !m_has_pos || block_epoch > m_last_pos_epoch;
        if (!PrepareEpoch(block_epoch, height, params, undo)) {
            UndoBlock(undo);
            return false;
        }
        if (!m_has_pos) {
            Checkpoint anchor;
            anchor.epoch = FINAL_POW_CHECKPOINT_EPOCH;
            anchor.root = block.hashPrevBlock;
            if (anchor.root.IsNull() ||
                !m_checkpoints.emplace(anchor.epoch, anchor).second ||
                !m_justified.emplace(anchor.epoch, anchor).second) {
                UndoBlock(undo);
                return false;
            }
            undo.added_checkpoints.push_back(anchor.epoch);
            undo.added_justified.push_back(anchor.epoch);
            undo.previous_finalized = m_finalized;
            undo.finalized_changed = true;
            m_finalized = anchor;
        }
        if (new_checkpoint) {
            Checkpoint checkpoint;
            checkpoint.epoch = block_epoch;
            checkpoint.slot = block.posExtension.stake_proof.slot;
            checkpoint.root = block.GetHash();
            if (!m_checkpoints.emplace(block_epoch, checkpoint).second) {
                UndoBlock(undo);
                return false;
            }
            undo.added_checkpoints.push_back(block_epoch);
        }
        m_has_pos = true;
        m_last_pos_epoch = block_epoch;

        const auto apply_evidence = [&](const COutPoint& outpoint,
                                        const uint256& evidence_id) {
            if (m_applied_evidence.count(evidence_id) != 0) return true;
            LockoutUndo change;
            change.bond_outpoint = outpoint;
            const auto previous = m_lockouts.find(outpoint);
            if (previous != m_lockouts.end()) {
                change.had_previous = true;
                change.previous = previous->second;
            }
            undo.lockout_undo.push_back(change);
            LockoutRecord& lockout = m_lockouts[outpoint];
            const uint64_t activation_epoch = block_epoch + 1;
            if (!change.had_previous ||
                activation_epoch < lockout.activation_epoch) {
                lockout.activation_epoch = activation_epoch;
            }
            if (!m_applied_evidence.emplace(evidence_id).second) return false;
            undo.added_evidence.push_back(evidence_id);
            return true;
        };
        for (const BlockEquivocationEvidence& evidence :
             block.posExtension.block_evidence) {
            if (!apply_evidence(
                    evidence.first.bond_outpoint,
                    GetTaggedHash(HashDomain::EQUIVOCATION, evidence))) {
                UndoBlock(undo);
                return false;
            }
        }
        for (const VoteEquivocationEvidence& evidence :
             block.posExtension.vote_evidence) {
            if (!apply_evidence(
                    evidence.first.bond_outpoint,
                    GetTaggedHash(HashDomain::EQUIVOCATION, evidence))) {
                UndoBlock(undo);
                return false;
            }
        }
    }

    for (const CTransactionRef& tx_ref : block.vtx) {
        const CTransaction& tx = *tx_ref;
        if (!tx.IsCoinBase()) {
            for (const CTxIn& input : tx.vin) {
                if (IsWithdrawalLocked(input.prevout, height)) {
                    UndoBlock(undo);
                    return false;
                }
                const auto it = m_bonds.find(input.prevout);
                if (it != m_bonds.end()) {
                    const auto added = std::find(undo.added_bonds.begin(),
                                                 undo.added_bonds.end(),
                                                 input.prevout);
                    if (added != undo.added_bonds.end()) {
                        undo.added_bonds.erase(added);
                    } else {
                        undo.removed_bonds.push_back(
                            SnapshotEntry{it->first, it->second});
                    }
                    m_bonds.erase(it);
                }
            }
        }

        const uint256 txid = tx.GetHash();
        for (uint32_t i = 0; i < tx.vout.size(); ++i) {
            BondData data;
            if (!ParseBondScript(tx.vout[i].scriptPubKey, data)) continue;
            const COutPoint outpoint(txid, i);
            BondRecord record;
            record.value = tx.vout[i].nValue;
            record.creation_height = height;
            record.data = data;
            if (!m_bonds.emplace(outpoint, record).second) {
                UndoBlock(undo);
                return false;
            }
            undo.added_bonds.push_back(outpoint);
            if (!m_bond_history.emplace(outpoint, record).second) {
                UndoBlock(undo);
                return false;
            }
            undo.added_bond_history.push_back(outpoint);
        }
    }

    const int initial_height = GetInitialStakeSnapshotHeight(params);
    if (height == initial_height) {
        if (!AddSnapshot(INITIAL_SNAPSHOT_EPOCH, height,
                         params.nPoSActivationHeight, params, undo)) {
            UndoBlock(undo);
            return false;
        }
    }
    if (is_pos && slot_in_epoch < 80) {
        uint256 output;
        std::reverse_copy(block.posExtension.stake_proof.vrf_output,
                          block.posExtension.stake_proof.vrf_output + 32,
                          output.begin());
        m_vrf_contributions[block_epoch].push_back(output);
        undo.added_vrf_contribution = true;
        undo.contribution_epoch = block_epoch;
    }
    m_best_block = block.GetHash();
    return true;
}

bool State::ApplyVotes(const std::vector<CheckpointVote>& votes,
                       StateUndo& undo)
{
    for (const CheckpointVote& vote : votes) {
        const Checkpoint* source = FindJustified(vote.source_epoch);
        const Checkpoint* target = FindCheckpoint(vote.target_epoch);
        if (source == nullptr || target == nullptr ||
            source->root != vote.source_checkpoint_root ||
            target->root != vote.target_checkpoint_root) {
            return false;
        }
        const auto previous = m_latest_votes.find(vote.bond_outpoint);
        VoteUndo item;
        item.bond_outpoint = vote.bond_outpoint;
        if (previous != m_latest_votes.end()) {
            item.had_previous = true;
            item.previous = previous->second;
            previous->second = vote;
        } else {
            m_latest_votes.emplace(vote.bond_outpoint, vote);
        }
        undo.vote_undo.push_back(item);
    }

    using Link = std::tuple<uint64_t, uint256, uint64_t, uint256, uint64_t>;
    std::map<Link, CAmount> link_weight;
    for (const auto& item : m_latest_votes) {
        const CheckpointVote& vote = item.second;
        const StakeSnapshot* snapshot = FindSnapshot(vote.snapshot_epoch);
        const CAmount vote_weight = GetVoteWeight(vote);
        if (snapshot == nullptr || snapshot->total_value <= 0 ||
            vote_weight <= 0) continue;
        const Link link(vote.source_epoch, vote.source_checkpoint_root,
                        vote.target_epoch, vote.target_checkpoint_root,
                        vote.snapshot_epoch);
        CAmount& weight = link_weight[link];
        if (!MoneyRange(weight + vote_weight)) return false;
        weight += vote_weight;
    }

    bool changed;
    do {
        changed = false;
        for (const auto& item : link_weight) {
            const uint64_t source_epoch = std::get<0>(item.first);
            const uint256& source_root = std::get<1>(item.first);
            const uint64_t target_epoch = std::get<2>(item.first);
            const uint256& target_root = std::get<3>(item.first);
            const uint64_t snapshot_epoch = std::get<4>(item.first);
            const Checkpoint* source = FindJustified(source_epoch);
            const Checkpoint* target = FindCheckpoint(target_epoch);
            const StakeSnapshot* snapshot = FindSnapshot(snapshot_epoch);
            if (source == nullptr || target == nullptr || snapshot == nullptr ||
                source->root != source_root || target->root != target_root ||
                snapshot->total_value <= 0) continue;
            const CAmount required =
                (snapshot->total_value / 3) * 2 +
                ((snapshot->total_value % 3) * 2 + 2) / 3;
            if (item.second < required || FindJustified(target_epoch) != nullptr) {
                continue;
            }
            m_justified.emplace(target_epoch, *target);
            undo.added_justified.push_back(target_epoch);
            changed = true;

            const Checkpoint* next = nullptr;
            if (source_epoch == FINAL_POW_CHECKPOINT_EPOCH) {
                const auto it = m_checkpoints.begin();
                if (it != m_checkpoints.end() &&
                    it->first != FINAL_POW_CHECKPOINT_EPOCH) next = &it->second;
            } else {
                const auto it = m_checkpoints.upper_bound(source_epoch);
                if (it != m_checkpoints.end() &&
                    it->first != FINAL_POW_CHECKPOINT_EPOCH) next = &it->second;
            }
            if (next != nullptr && next->epoch == target_epoch &&
                (m_finalized.epoch == FINAL_POW_CHECKPOINT_EPOCH ||
                 source_epoch > m_finalized.epoch)) {
                if (!undo.finalized_changed) {
                    undo.previous_finalized = m_finalized;
                    undo.finalized_changed = true;
                }
                m_finalized = *source;
            }
        }
    } while (changed);
    return true;
}

bool State::UndoBlock(const StateUndo& undo)
{
    bool clean = true;
    for (auto it = undo.added_evidence.rbegin();
         it != undo.added_evidence.rend(); ++it) {
        clean = m_applied_evidence.erase(*it) == 1 && clean;
    }
    for (auto it = undo.lockout_undo.rbegin();
         it != undo.lockout_undo.rend(); ++it) {
        if (it->had_previous) {
            m_lockouts[it->bond_outpoint] = it->previous;
        } else {
            clean = m_lockouts.erase(it->bond_outpoint) == 1 && clean;
        }
    }
    for (auto it = undo.vote_undo.rbegin(); it != undo.vote_undo.rend(); ++it) {
        if (it->had_previous) {
            m_latest_votes[it->bond_outpoint] = it->previous;
        } else {
            clean = m_latest_votes.erase(it->bond_outpoint) == 1 && clean;
        }
    }
    for (auto it = undo.added_justified.rbegin();
         it != undo.added_justified.rend(); ++it) {
        clean = m_justified.erase(*it) == 1 && clean;
    }
    for (auto it = undo.added_checkpoints.rbegin();
         it != undo.added_checkpoints.rend(); ++it) {
        clean = m_checkpoints.erase(*it) == 1 && clean;
    }
    if (undo.finalized_changed) {
        m_finalized = undo.previous_finalized;
    }
    if (undo.added_vrf_contribution) {
        auto it = m_vrf_contributions.find(undo.contribution_epoch);
        if (it == m_vrf_contributions.end() || it->second.empty()) {
            clean = false;
        } else {
            it->second.pop_back();
            if (it->second.empty()) m_vrf_contributions.erase(it);
        }
    }
    for (auto it = undo.added_epoch_seeds.rbegin();
         it != undo.added_epoch_seeds.rend(); ++it) {
        clean = m_epoch_seeds.erase(*it) == 1 && clean;
    }
    for (auto it = undo.added_snapshots.rbegin();
         it != undo.added_snapshots.rend(); ++it) {
        clean = m_snapshots.erase(*it) == 1 && clean;
    }
    for (auto it = undo.added_bonds.rbegin(); it != undo.added_bonds.rend(); ++it) {
        clean = m_bonds.erase(*it) == 1 && clean;
    }
    for (auto it = undo.added_bond_history.rbegin();
         it != undo.added_bond_history.rend(); ++it) {
        clean = m_bond_history.erase(*it) == 1 && clean;
    }
    for (auto it = undo.removed_bonds.rbegin();
         it != undo.removed_bonds.rend(); ++it) {
        clean = m_bonds.emplace(it->outpoint, it->bond).second && clean;
    }
    m_has_pos = undo.previous_has_pos;
    m_last_pos_epoch = undo.previous_pos_epoch;
    m_best_block = undo.previous_best_block;
    return clean;
}

} // namespace pos
