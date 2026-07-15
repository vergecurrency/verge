// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_POS_STATE_H
#define VERGE_POS_STATE_H

#include <amount.h>
#include <consensus/params.h>
#include <pos/stake.h>
#include <primitives/transaction.h>
#include <serialize.h>
#include <uint256.h>

#include <cstdint>
#include <map>
#include <set>
#include <vector>

class CBlock;
class CCoinsView;
namespace pos {

static constexpr uint64_t INITIAL_SNAPSHOT_EPOCH = UINT64_MAX;

struct BondRecord {
    CAmount value{0};
    int32_t creation_height{0};
    BondData data;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(value, creation_height);
        READWRITE(data.signing_public_key);
        READWRITE(data.vrf_public_key);
        READWRITE(data.reward_key_id, data.withdrawal_key_id);
    }
};

struct SnapshotEntry {
    COutPoint outpoint;
    BondRecord bond;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(outpoint, bond);
    }
};

struct StakeSnapshot {
    uint64_t source_epoch{INITIAL_SNAPSHOT_EPOCH};
    int32_t source_height{0};
    CAmount total_value{0};
    uint256 root;
    std::vector<SnapshotEntry> entries;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(source_epoch, source_height, total_value, root, entries);
    }
};

struct Checkpoint {
    uint64_t epoch{FINAL_POW_CHECKPOINT_EPOCH};
    uint64_t slot{0};
    uint256 root;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(epoch, slot, root);
    }
};

struct VoteUndo {
    COutPoint bond_outpoint;
    bool had_previous{false};
    CheckpointVote previous;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(bond_outpoint, had_previous, previous);
    }
};

struct LockoutRecord {
    uint64_t activation_epoch{0};
    int32_t until_height{0};

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(activation_epoch, until_height);
    }
};

struct LockoutUndo {
    COutPoint bond_outpoint;
    bool had_previous{false};
    LockoutRecord previous;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(bond_outpoint, had_previous, previous);
    }
};

struct StateUndo {
    uint256 previous_best_block;
    std::vector<COutPoint> added_bonds;
    std::vector<COutPoint> added_bond_history;
    std::vector<SnapshotEntry> removed_bonds;
    std::vector<uint64_t> added_snapshots;
    std::vector<uint64_t> added_epoch_seeds;
    bool added_vrf_contribution{false};
    uint64_t contribution_epoch{0};
    bool previous_has_pos{false};
    uint64_t previous_pos_epoch{0};
    std::vector<uint64_t> added_checkpoints;
    std::vector<uint64_t> added_justified;
    std::vector<VoteUndo> vote_undo;
    std::vector<LockoutUndo> lockout_undo;
    std::vector<uint256> added_evidence;
    bool finalized_changed{false};
    Checkpoint previous_finalized;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(previous_best_block, added_bonds, added_bond_history,
                  removed_bonds,
                  added_snapshots, added_epoch_seeds,
                  added_vrf_contribution, contribution_epoch,
                  previous_has_pos, previous_pos_epoch, added_checkpoints,
                  added_justified, vote_undo, lockout_undo, added_evidence,
                  finalized_changed,
                  previous_finalized);
    }
};

class State {
public:
    bool ApplyBlock(const CBlock& block, int32_t height, bool is_pos,
                    uint64_t block_epoch, uint32_t slot_in_epoch,
                    const Consensus::Params& params, StateUndo& undo);
    bool UndoBlock(const StateUndo& undo);
    bool InitializeFromCoins(const CCoinsView& coins,
                             const uint256& best_block);

    const BondRecord* FindBond(const COutPoint& outpoint) const;
    const BondRecord* FindHistoricalBond(const COutPoint& outpoint) const;
    const StakeSnapshot* FindSnapshot(uint64_t source_epoch) const;
    const std::map<COutPoint, BondRecord>& Bonds() const { return m_bonds; }
    const uint256& BestBlock() const { return m_best_block; }
    const uint256* FindEpochSeed(uint64_t epoch) const;
    bool SetEpochSeed(uint64_t epoch, const uint256& seed, StateUndo& undo);
    bool PrepareEpoch(uint64_t block_epoch, int32_t height,
                      const Consensus::Params& params, StateUndo& undo);
    bool ApplyVotes(const std::vector<CheckpointVote>& votes,
                    StateUndo& undo);
    const Checkpoint* FindCheckpoint(uint64_t epoch) const;
    const Checkpoint* FindJustified(uint64_t epoch) const;
    const Checkpoint* LatestCheckpoint() const;
    const Checkpoint* LatestJustified() const;
    const Checkpoint& Finalized() const { return m_finalized; }
    const std::map<COutPoint, CheckpointVote>& LatestVotes() const
    {
        return m_latest_votes;
    }
    CAmount GetVoteWeight(const CheckpointVote& vote) const;
    bool IsEligibilityLocked(const COutPoint& outpoint, uint64_t epoch,
                             int32_t height) const;
    bool IsWithdrawalLocked(const COutPoint& outpoint, int32_t height) const;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_best_block, m_bonds, m_bond_history, m_snapshots, m_epoch_seeds,
                  m_vrf_contributions, m_has_pos, m_last_pos_epoch);
        READWRITE(m_checkpoints, m_justified, m_latest_votes, m_finalized,
                  m_lockouts, m_applied_evidence);
    }

private:
    bool AddSnapshot(uint64_t source_epoch, int32_t source_height,
                     int32_t maturity_height, const Consensus::Params& params,
                     StateUndo& undo);

    uint256 m_best_block;
    std::map<COutPoint, BondRecord> m_bonds;
    std::map<COutPoint, BondRecord> m_bond_history;
    std::map<uint64_t, StakeSnapshot> m_snapshots;
    std::map<uint64_t, uint256> m_epoch_seeds;
    std::map<uint64_t, std::vector<uint256>> m_vrf_contributions;
    bool m_has_pos{false};
    uint64_t m_last_pos_epoch{0};
    std::map<uint64_t, Checkpoint> m_checkpoints;
    std::map<uint64_t, Checkpoint> m_justified;
    std::map<COutPoint, CheckpointVote> m_latest_votes;
    Checkpoint m_finalized;
    std::map<COutPoint, LockoutRecord> m_lockouts;
    std::set<uint256> m_applied_evidence;
};

uint256 ComputeSnapshotRoot(const std::vector<SnapshotEntry>& entries);

} // namespace pos

#endif // VERGE_POS_STATE_H
