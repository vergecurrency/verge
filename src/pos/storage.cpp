// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/storage.h>

#include <dbwrapper.h>

#include <algorithm>
#include <array>
#include <memory>

namespace pos {
namespace {

enum Field : uint8_t {
    BONDS = 1, HISTORY, SNAPSHOTS, SEEDS, ENTROPY, CHECKPOINTS,
    JUSTIFIED, VOTES, LOCKOUTS, EVIDENCE, RETIRED,
};
static constexpr size_t FIELD_COUNT = RETIRED;

struct RecordKey {
    char prefix{STATE_DB_RECORD_PREFIX};
    uint8_t field{0};
    uint256 id;
    uint32_t output{0};

    RecordKey() = default;
    RecordKey(uint8_t kind, const COutPoint& key)
        : field(kind), id(key.hash), output(key.n) {}
    RecordKey(uint8_t kind, const uint256& key) : field(kind), id(key) {}
    RecordKey(uint8_t kind, uint64_t epoch) : field(kind)
    {
        WriteLE64(id.begin(), epoch);
    }

    uint64_t Epoch() const
    {
        if (output != 0 || std::any_of(id.begin() + 8, id.end(),
                                      [](unsigned char c) { return c != 0; })) {
            throw dbwrapper_error("Non-canonical PoS epoch record key");
        }
        return ReadLE64(id.begin());
    }

    template <typename Stream> void Serialize(Stream& stream) const
    {
        stream << prefix << field << id << output;
    }
    template <typename Stream> void Unserialize(Stream& stream)
    {
        stream >> prefix >> field >> id >> output;
        if (!stream.empty()) throw std::ios_base::failure("Trailing PoS record key data");
    }
};

bool HasRecords(CDBWrapper& database)
{
    std::unique_ptr<CDBIterator> cursor(database.NewIterator());
    cursor->Seek(STATE_DB_RECORD_PREFIX);
    char prefix = 0;
    return cursor->Valid() && cursor->GetKey(prefix) && prefix == STATE_DB_RECORD_PREFIX;
}

template <typename Map> void WriteMap(CDBBatch& batch, Field field, const Map& map)
{
    for (const auto& item : map) batch.Write(RecordKey(field, item.first), item.second);
}

template <typename Map, typename Keys>
void WriteKeys(CDBBatch& batch, Field field, const Map& map, const Keys& keys)
{
    for (const auto& key : keys) {
        const auto it = map.find(key);
        if (it == map.end()) batch.Erase(RecordKey(field, key));
        else batch.Write(RecordKey(field, key), it->second);
    }
}

template <typename Map, typename Key>
void ReadRecord(CDBIterator& cursor, Map& map, const Key& key)
{
    typename Map::mapped_type value;
    if (!cursor.GetValue(value) || !map.emplace(key, std::move(value)).second) {
        throw dbwrapper_error("Corrupt or duplicate PoS state record");
    }
}

} // namespace

struct StateDBMetadata {
    uint32_t version{STATE_DB_VERSION};
    uint256 best_block;
    uint256 initial_entropy;
    bool has_pos{false};
    uint64_t last_epoch{0};
    Checkpoint finalized;
    std::array<uint64_t, FIELD_COUNT> counts{};

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(version, best_block, initial_entropy, has_pos, last_epoch, finalized);
        for (auto& count : counts) READWRITE(count);
    }
};

namespace {
bool ReadMetadata(CDBWrapper& database, StateDBMetadata& metadata)
{
    if (!database.Exists(STATE_DB_METADATA_KEY)) {
        if (HasRecords(database)) throw dbwrapper_error("PoS records have no format metadata");
        return false;
    }
    uint32_t version = 0;
    if (!database.Read(STATE_DB_METADATA_KEY, version)) {
        throw dbwrapper_error("Corrupt PoS state format version");
    }
    if (version != STATE_DB_VERSION) {
        throw dbwrapper_error("Unsupported PoS state database version; use a compatible binary");
    }
    if (!database.Read(STATE_DB_METADATA_KEY, metadata)) {
        throw dbwrapper_error("Corrupt PoS state metadata");
    }
    if (database.Exists(LEGACY_STATE_DB_KEY)) {
        throw dbwrapper_error("Mixed legacy and record PoS states; restore the pre-downgrade backup or reindex");
    }
    return true;
}

bool ReadLegacy(CDBWrapper& database, State& state)
{
    if (!database.Exists(LEGACY_STATE_DB_KEY)) return false;
    if (!database.Read(LEGACY_STATE_DB_KEY, state)) {
        throw dbwrapper_error("Corrupt legacy PoS state");
    }
    return true;
}
} // namespace

void StateChanges::AddUndo(const StateUndo& undo)
{
    bonds.insert(undo.added_bonds.begin(), undo.added_bonds.end());
    for (const auto& item : undo.removed_bonds) bonds.insert(item.outpoint);
    history.insert(undo.added_bond_history.begin(), undo.added_bond_history.end());
    for (const auto& item : undo.removed_bond_history) history.insert(item.outpoint);
    snapshots.insert(undo.added_snapshots.begin(), undo.added_snapshots.end());
    for (const auto& item : undo.removed_snapshots) snapshots.insert(item.source_epoch);
    seeds.insert(undo.added_epoch_seeds.begin(), undo.added_epoch_seeds.end());
    for (const auto& item : undo.removed_epoch_seeds) seeds.insert(item.first);
    for (const auto& item : undo.epoch_entropy_undo) entropy.insert(item.epoch);
    for (const auto& item : undo.removed_epoch_entropy) entropy.insert(item.first);
    checkpoints.insert(undo.added_checkpoints.begin(), undo.added_checkpoints.end());
    justified.insert(undo.added_justified.begin(), undo.added_justified.end());
    for (const auto& item : undo.vote_undo) votes.insert(item.bond_outpoint);
    for (const auto& item : undo.lockout_undo) lockouts.insert(item.bond_outpoint);
    evidence.insert(undo.added_evidence.begin(), undo.added_evidence.end());
    for (const auto& item : undo.removed_evidence) evidence.insert(item.first);
    retired.insert(undo.added_retired_bonds.begin(), undo.added_retired_bonds.end());
    for (const auto& item : undo.removed_retired_bonds) retired.insert(item.first);
}

StateDBMetadata StateStorage::Metadata(const State& state)
{
    StateDBMetadata metadata;
    metadata.best_block = state.m_best_block;
    metadata.initial_entropy = state.m_initial_pow_entropy;
    metadata.has_pos = state.m_has_pos;
    metadata.last_epoch = state.m_last_pos_epoch;
    metadata.finalized = state.m_finalized;
    metadata.counts = {{state.m_bonds.size(), state.m_bond_history.size(),
        state.m_snapshots.size(), state.m_epoch_seeds.size(), state.m_epoch_entropy.size(),
        state.m_checkpoints.size(), state.m_justified.size(), state.m_latest_votes.size(),
        state.m_lockouts.size(), state.m_applied_evidence.size(), state.m_retired_bonds.size()}};
    return metadata;
}

bool StateStorage::Read(CDBWrapper& database, State& state)
{
    StateDBMetadata metadata;
    State loaded;
    if (!ReadMetadata(database, metadata)) {
        if (!ReadLegacy(database, loaded)) return false;
        state = std::move(loaded);
        return true;
    }
    loaded.m_best_block = metadata.best_block;
    loaded.m_initial_pow_entropy = metadata.initial_entropy;
    loaded.m_has_pos = metadata.has_pos;
    loaded.m_last_pos_epoch = metadata.last_epoch;
    loaded.m_finalized = metadata.finalized;
    std::unique_ptr<CDBIterator> cursor(database.NewIterator());
    for (cursor->Seek(STATE_DB_RECORD_PREFIX); cursor->Valid(); cursor->Next()) {
        char prefix = 0;
        if (!cursor->GetKey(prefix)) throw dbwrapper_error("Corrupt PoS state record key");
        if (prefix != STATE_DB_RECORD_PREFIX) break;
        RecordKey key;
        if (!cursor->GetKey(key)) throw dbwrapper_error("Corrupt PoS state record key");
        const COutPoint outpoint(key.id, key.output);
        switch (key.field) {
        case BONDS: ReadRecord(*cursor, loaded.m_bonds, outpoint); break;
        case HISTORY: ReadRecord(*cursor, loaded.m_bond_history, outpoint); break;
        case SNAPSHOTS: ReadRecord(*cursor, loaded.m_snapshots, key.Epoch()); break;
        case SEEDS: ReadRecord(*cursor, loaded.m_epoch_seeds, key.Epoch()); break;
        case ENTROPY: ReadRecord(*cursor, loaded.m_epoch_entropy, key.Epoch()); break;
        case CHECKPOINTS: ReadRecord(*cursor, loaded.m_checkpoints, key.Epoch()); break;
        case JUSTIFIED: ReadRecord(*cursor, loaded.m_justified, key.Epoch()); break;
        case VOTES: ReadRecord(*cursor, loaded.m_latest_votes, outpoint); break;
        case LOCKOUTS: ReadRecord(*cursor, loaded.m_lockouts, outpoint); break;
        case EVIDENCE:
            if (key.output != 0) throw dbwrapper_error("Non-canonical PoS evidence key");
            ReadRecord(*cursor, loaded.m_applied_evidence, key.id);
            break;
        case RETIRED: ReadRecord(*cursor, loaded.m_retired_bonds, outpoint); break;
        default: throw dbwrapper_error("Unknown PoS state record type");
        }
    }
    if (Metadata(loaded).counts != metadata.counts) {
        throw dbwrapper_error("PoS state record count mismatch");
    }
    for (const auto& item : loaded.m_snapshots) {
        if (item.first != item.second.source_epoch) throw dbwrapper_error("PoS snapshot key mismatch");
    }
    for (const auto& item : loaded.m_latest_votes) {
        if (item.first != item.second.bond_outpoint) throw dbwrapper_error("PoS vote key mismatch");
    }
    for (const auto& item : loaded.m_checkpoints) {
        if (item.first != item.second.epoch) throw dbwrapper_error("PoS checkpoint key mismatch");
    }
    for (const auto& item : loaded.m_justified) {
        if (item.first != item.second.epoch) throw dbwrapper_error("PoS justification key mismatch");
    }
    LogPrint(BCLog::BENCH, "Loaded PoS record state version=%u bonds=%u tip=%s\n",
             metadata.version, loaded.Bonds().size(), loaded.BestBlock().ToString());
    state = std::move(loaded);
    return true;
}

bool StateStorage::WriteFull(CDBWrapper& database, CDBBatch& batch,
                            const State& state, const uint256* expected_tip)
{
    StateDBMetadata previous;
    const bool current = ReadMetadata(database, previous);
    if (expected_tip != nullptr) {
        if (!current) {
            State legacy;
            if (!ReadLegacy(database, legacy)) return false;
            previous.best_block = legacy.BestBlock();
        }
        if (previous.best_block != *expected_tip) return false;
    }
    std::unique_ptr<CDBIterator> cursor(database.NewIterator());
    for (cursor->Seek(STATE_DB_RECORD_PREFIX); cursor->Valid(); cursor->Next()) {
        char prefix = 0;
        if (!cursor->GetKey(prefix)) throw dbwrapper_error("Corrupt PoS record key during reset");
        if (prefix != STATE_DB_RECORD_PREFIX) break;
        RecordKey key;
        if (!cursor->GetKey(key)) throw dbwrapper_error("Corrupt PoS record key during reset");
        batch.Erase(key);
    }
    WriteMap(batch, BONDS, state.m_bonds);
    WriteMap(batch, HISTORY, state.m_bond_history);
    WriteMap(batch, SNAPSHOTS, state.m_snapshots);
    WriteMap(batch, SEEDS, state.m_epoch_seeds);
    WriteMap(batch, ENTROPY, state.m_epoch_entropy);
    WriteMap(batch, CHECKPOINTS, state.m_checkpoints);
    WriteMap(batch, JUSTIFIED, state.m_justified);
    WriteMap(batch, VOTES, state.m_latest_votes);
    WriteMap(batch, LOCKOUTS, state.m_lockouts);
    WriteMap(batch, EVIDENCE, state.m_applied_evidence);
    WriteMap(batch, RETIRED, state.m_retired_bonds);
    batch.Write(STATE_DB_METADATA_KEY, Metadata(state));
    batch.Erase(LEGACY_STATE_DB_KEY);
    return true;
}

bool StateStorage::WriteChanges(CDBWrapper& database, CDBBatch& batch,
                               const State& state, const StateChanges& changes,
                               const uint256& expected_tip)
{
    StateDBMetadata previous;
    if (!ReadMetadata(database, previous)) {
        State legacy;
        if (ReadLegacy(database, legacy)) {
            if (legacy.BestBlock() != expected_tip) return false;
        } else if (!expected_tip.IsNull()) {
            return false;
        }
        return WriteFull(database, batch, state);
    }
    if (previous.best_block != expected_tip) return false;
    WriteKeys(batch, BONDS, state.m_bonds, changes.bonds);
    WriteKeys(batch, HISTORY, state.m_bond_history, changes.history);
    WriteKeys(batch, SNAPSHOTS, state.m_snapshots, changes.snapshots);
    WriteKeys(batch, SEEDS, state.m_epoch_seeds, changes.seeds);
    WriteKeys(batch, ENTROPY, state.m_epoch_entropy, changes.entropy);
    WriteKeys(batch, CHECKPOINTS, state.m_checkpoints, changes.checkpoints);
    WriteKeys(batch, JUSTIFIED, state.m_justified, changes.justified);
    WriteKeys(batch, VOTES, state.m_latest_votes, changes.votes);
    WriteKeys(batch, LOCKOUTS, state.m_lockouts, changes.lockouts);
    WriteKeys(batch, EVIDENCE, state.m_applied_evidence, changes.evidence);
    WriteKeys(batch, RETIRED, state.m_retired_bonds, changes.retired);
    batch.Write(STATE_DB_METADATA_KEY, Metadata(state));
    return true;
}

} // namespace pos
