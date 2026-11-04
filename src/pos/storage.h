// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_POS_STORAGE_H
#define VERGE_POS_STORAGE_H

#include <pos/state.h>

#include <set>

class CDBBatch;
class CDBWrapper;

namespace pos {

static constexpr uint32_t STATE_DB_VERSION = 1;
static constexpr char LEGACY_STATE_DB_KEY = 'P';
static constexpr char STATE_DB_METADATA_KEY = 'p';
static constexpr char STATE_DB_RECORD_PREFIX = 'q';

struct StateChanges {
    std::set<COutPoint> bonds, history, votes, lockouts, retired;
    std::set<uint64_t> snapshots, seeds, entropy, checkpoints, justified;
    std::set<uint256> evidence;

    void AddUndo(const StateUndo& undo);
};

struct StateDBMetadata;

// Builds one caller-owned batch. The caller adds undo changes and commits it
// atomically; no record or format marker is written independently.
// Callers serialize access to the state database, normally using cs_main.
class StateStorage {
public:
    static bool Read(CDBWrapper& database, State& state);
    static bool WriteFull(CDBWrapper& database, CDBBatch& batch,
                          const State& state,
                          const uint256* expected_tip = nullptr);
    static bool WriteChanges(CDBWrapper& database, CDBBatch& batch,
                             const State& state, const StateChanges& changes,
                             const uint256& expected_tip);

private:
    static StateDBMetadata Metadata(const State& state);
};

} // namespace pos

#endif // VERGE_POS_STORAGE_H
