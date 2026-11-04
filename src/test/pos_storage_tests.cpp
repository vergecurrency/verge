// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/storage.h>

#include <chainparams.h>
#include <coins.h>
#include <hash.h>
#include <pos/crypto.h>
#include <pos/vrf.h>
#include <primitives/block.h>
#include <test/setup_common.h>
#include <txdb.h>

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <cstdlib>

namespace {
struct StorageTestingSetup : BasicTestingSetup {
    StorageTestingSetup() : BasicTestingSetup(CBaseChainParams::REGTEST)
    {
        SetDataDir("pos-records");
        ClearDatadirCache();
    }
    ~StorageTestingSetup() { ClearDatadirCache(); }
};

uint256 Number(uint64_t n)
{
    uint256 hash;
    WriteLE64(hash.begin(), n);
    return hash;
}

uint256 StateHash(const pos::State& state)
{
    CHashWriter writer(SER_DISK, CLIENT_VERSION);
    writer << state;
    return writer.GetHash();
}

pos::BondData Bond()
{
    pos::BondData data;
    unsigned char secret[32]{};
    secret[31] = 1;
    unsigned char vrf_secret[32]{};
    BOOST_REQUIRE(pos::GetSchnorrPublicKey(secret, data.signing_public_key));
    BOOST_REQUIRE(pos::DeriveVrfKey(secret, vrf_secret, data.vrf_public_key));
    data.reward_key_id.SetHex("01");
    data.withdrawal_key_id.SetHex("02");
    return data;
}

class GeneratedCoins : public CCoinsView {
    size_t m_count;
    Coin m_coin;
public:
    GeneratedCoins(size_t count, const Consensus::Params& params)
        : m_count(count), m_coin(CTxOut(params.nPoSMinStake,
            pos::GetBondScript(Bond())), 1, false, 1) {}

    bool ForEachCoin(const std::function<bool(const COutPoint&, const Coin&)>& visit) const override
    {
        for (size_t i = 1; i <= m_count; ++i) {
            if (!visit(COutPoint(Number(i), 0), m_coin)) return false;
        }
        return true;
    }
};

CBlock EmptyBlock(const pos::State& state, uint32_t height)
{
    CBlock block;
    block.nVersion = height >= 1000 ? 4 | pos::BLOCK_VERSION_POS : 4;
    block.nTime = height;
    block.hashPrevBlock = state.BestBlock();
    CMutableTransaction reward;
    reward.vin.resize(1);
    reward.vout.emplace_back(0, CScript() << OP_TRUE);
    block.vtx.push_back(MakeTransactionRef(reward));
    return block;
}

pos::State MakeState(size_t bonds, const Consensus::Params& params)
{
    GeneratedCoins coins(bonds, params);
    pos::State state;
    BOOST_REQUIRE(state.InitializeFromCoins(coins, Number(1000000), params));
    pos::StateUndo snapshot;
    BOOST_REQUIRE(state.ApplyBlock(EmptyBlock(state, 760), 760, false, 0, 0,
                                   params, snapshot));
    pos::StateUndo first;
    BOOST_REQUIRE(state.SetEpochSeed(0, Number(999), first));
    BOOST_REQUIRE(state.ApplyBlock(EmptyBlock(state, 1000), 1000, true, 0, 0,
                                   params, first));
    return state;
}

pos::CheckpointVote Vote(const pos::State& state, const COutPoint& outpoint,
                         uint64_t slot)
{
    pos::CheckpointVote vote;
    vote.bond_outpoint = outpoint;
    vote.source_epoch = state.LatestJustified()->epoch;
    vote.source_checkpoint_root = state.LatestJustified()->root;
    vote.target_epoch = state.LatestCheckpoint()->epoch;
    vote.target_checkpoint_root = state.LatestCheckpoint()->root;
    vote.snapshot_epoch = pos::INITIAL_SNAPSHOT_EPOCH;
    vote.head_slot = slot;
    vote.head_block_root = state.BestBlock();
    return vote;
}

void CheckStored(CBlockTreeDB& database, const pos::State& state)
{
    pos::State loaded;
    BOOST_REQUIRE(database.ReadPoSState(loaded));
    BOOST_CHECK(StateHash(loaded) == StateHash(state));
}
} // namespace

BOOST_FIXTURE_TEST_SUITE(pos_storage_tests, StorageTestingSetup)

BOOST_AUTO_TEST_CASE(legacy_migration_and_commit_boundaries)
{
    Consensus::Params params = Params().GetConsensus();
    params.nPoSActivationHeight = 1000;
    pos::State before = MakeState(8, params);
    CBlockTreeDB database(1 << 20, true);
    BOOST_REQUIRE(database.Write(pos::LEGACY_STATE_DB_KEY, before, true));
    CheckStored(database, before);

    pos::State after = before;
    const CBlock block = EmptyBlock(after, 1001);
    pos::StateUndo undo;
    BOOST_REQUIRE(after.ApplyBlock(block, 1001, true, 0, 1, params, undo));
    BOOST_REQUIRE(after.ApplyVotes({Vote(before, before.Bonds().begin()->first, 2)}, undo));
    pos::StateChanges changes;
    changes.AddUndo(undo);
    {
        CDBBatch interrupted(database);
        BOOST_REQUIRE(pos::StateStorage::WriteChanges(database, interrupted, after,
                                                       changes, before.BestBlock()));
        interrupted.Write(std::make_pair('U', block.GetHash()), undo);
        // Discarding the uncommitted batch simulates a pre-commit interruption.
        CheckStored(database, before);
        BOOST_CHECK(database.Exists(pos::LEGACY_STATE_DB_KEY));
        BOOST_CHECK(!database.Exists(pos::STATE_DB_METADATA_KEY));
    }
    pos::StateUndo absent;
    BOOST_CHECK(!database.ReadPoSUndo(block.GetHash(), absent));
    BOOST_REQUIRE(database.WritePoSStateTransition(after, block.GetHash(), undo));
    BOOST_CHECK(!database.Exists(pos::LEGACY_STATE_DB_KEY));
    uint32_t version = 0;
    BOOST_REQUIRE(database.Read(pos::STATE_DB_METADATA_KEY, version));
    BOOST_CHECK_EQUAL(version, pos::STATE_DB_VERSION);
    BOOST_CHECK(database.ReadPoSUndo(block.GetHash(), absent));
    CheckStored(database, after);

    BOOST_CHECK(!database.WritePoSStateTransition(after, block.GetHash(), undo));
    CheckStored(database, after);
    BOOST_REQUIRE(after.UndoBlock(undo));
    BOOST_REQUIRE(database.WritePoSStateRollback(after, block.GetHash()));
    BOOST_CHECK(!database.ReadPoSUndo(block.GetHash(), absent));
    CheckStored(database, before);
    BOOST_REQUIRE(database.WritePoSState(MakeState(2, params)));
    CheckStored(database, MakeState(2, params));
}

BOOST_AUTO_TEST_CASE(unknown_corrupt_and_mixed_formats_are_not_overwritten)
{
    Consensus::Params params = Params().GetConsensus();
    params.nPoSActivationHeight = 1000;
    const pos::State state = MakeState(8, params);
    for (int fault = 0; fault < 5; ++fault) {
        CBlockTreeDB database(1 << 20, true);
        BOOST_REQUIRE(database.WritePoSState(state));
        if (fault == 0) {
            BOOST_REQUIRE(database.Write(pos::STATE_DB_METADATA_KEY, pos::STATE_DB_VERSION + 1));
        } else if (fault == 1) {
            BOOST_REQUIRE(database.Erase(std::make_pair(pos::STATE_DB_RECORD_PREFIX,
                std::make_pair(uint8_t{1}, state.Bonds().begin()->first))));
        } else if (fault == 2) {
            BOOST_REQUIRE(database.Write(pos::LEGACY_STATE_DB_KEY, state));
        } else if (fault == 3) {
            BOOST_REQUIRE(database.Write(std::make_pair(pos::STATE_DB_RECORD_PREFIX,
                std::make_pair(uint8_t{255}, COutPoint(Number(1), 0))), uint32_t{0}));
        } else {
            BOOST_REQUIRE(database.Erase(pos::STATE_DB_METADATA_KEY));
        }
        pos::State loaded = state;
        BOOST_CHECK_THROW(database.ReadPoSState(loaded), dbwrapper_error);
        BOOST_CHECK(StateHash(loaded) == StateHash(state));
        if (fault == 0 || fault == 2 || fault == 4) {
            BOOST_CHECK(!database.WritePoSState(state));
        }
    }
}

BOOST_AUTO_TEST_CASE(all_record_families_round_trip_and_undo)
{
    Consensus::Params params = Params().GetConsensus();
    params.nPoSActivationHeight = 1000;
    params.nPoSUnbondingBlocks = 2;
    pos::State state = MakeState(8, params);
    const pos::State original = state;
    CBlockTreeDB database(1 << 20, true);
    BOOST_REQUIRE(database.WritePoSState(state));
    const COutPoint offender(Number(1), 0);
    std::vector<std::pair<uint256, pos::StateUndo>> history;
    for (uint32_t step = 0; step < 14; ++step) {
        const uint32_t height = 1001 + step;
        const uint64_t epoch = step == 0 ? 0 : step;
        CBlock block = EmptyBlock(state, height);
        if (step == 0) {
            pos::VoteEquivocationEvidence evidence;
            evidence.first.bond_outpoint = offender;
            evidence.first.target_epoch = 0;
            evidence.second = evidence.first;
            block.posExtension.vote_evidence.push_back(evidence);
        }
        if (step == 3) {
            CMutableTransaction spend;
            spend.vin.emplace_back(offender);
            spend.vin.emplace_back(COutPoint(Number(2), 0));
            spend.vout.emplace_back(params.nPoSMinStake, pos::GetBondScript(Bond()));
            block.vtx.push_back(MakeTransactionRef(spend));
        }
        pos::StateUndo undo;
        BOOST_REQUIRE(state.ApplyBlock(block, height, true, epoch, 0, params, undo));
        if (step <= 1) {
            std::vector<pos::CheckpointVote> votes;
            for (const auto& item : state.Bonds()) {
                if (!state.IsEligibilityLocked(item.first, epoch, height)) {
                    votes.push_back(Vote(state, item.first, height));
                }
            }
            BOOST_REQUIRE(state.ApplyVotes(votes, undo));
        } else {
            BOOST_REQUIRE(state.ApplyVotes({}, undo));
        }
        BOOST_REQUIRE(database.WritePoSStateTransition(state, block.GetHash(), undo));
        CheckStored(database, state);
        history.emplace_back(block.GetHash(), std::move(undo));
    }
    BOOST_CHECK(state.Finalized().epoch != pos::FINAL_POW_CHECKPOINT_EPOCH);
    for (auto it = history.rbegin(); it != history.rend(); ++it) {
        BOOST_REQUIRE(state.UndoBlock(it->second));
        BOOST_REQUIRE(database.WritePoSStateRollback(state, it->first));
        CheckStored(database, state);
    }
    BOOST_CHECK(StateHash(state) == StateHash(original));
}

BOOST_AUTO_TEST_CASE(disk_backed_delta_cost_and_recovery_stress)
{
    const bool extended = std::getenv("VERGE_POS_STORAGE_STRESS") != nullptr;
    const std::vector<size_t> sizes = extended ? std::vector<size_t>{1000, 10000, 100000}
                                               : std::vector<size_t>{1000};
    Consensus::Params params = Params().GetConsensus();
    params.nPoSActivationHeight = 1000;
    for (size_t count : sizes) {
        SetDataDir("pos-storage-" + std::to_string(count));
        ClearDatadirCache();
        pos::State state = MakeState(count, params);
        const auto initial_hash = StateHash(state);
        std::vector<std::pair<uint256, pos::StateUndo>> history;
        size_t full_bytes = 0, maximum_delta = 0, total_delta = 0;
        int64_t write_us = 0;
        {
            CBlockTreeDB database(1 << 20);
            CDBBatch initial(database);
            BOOST_REQUIRE(pos::StateStorage::WriteFull(database, initial, state));
            full_bytes = initial.SizeEstimate();
            BOOST_REQUIRE(database.WriteBatch(initial, true));
            for (uint32_t i = 0; i < 64; ++i) {
                const CBlock block = EmptyBlock(state, 1001 + i);
                pos::StateUndo undo;
                BOOST_REQUIRE(state.ApplyBlock(block, 1001 + i, true, 0, i + 1, params, undo));
                BOOST_REQUIRE(state.ApplyVotes({Vote(state, COutPoint(Number(i + 1), 0), i + 2)}, undo));
                pos::StateChanges changes;
                changes.AddUndo(undo);
                CDBBatch pending(database);
                BOOST_REQUIRE(pos::StateStorage::WriteChanges(database, pending, state,
                                                               changes, undo.previous_best_block));
                pending.Write(std::make_pair('U', block.GetHash()), undo);
                if (i == 0) {
                    pos::State unchanged;
                    BOOST_REQUIRE(database.ReadPoSState(unchanged));
                    BOOST_CHECK(StateHash(unchanged) == initial_hash);
                }
                maximum_delta = std::max(maximum_delta, pending.SizeEstimate());
                total_delta += pending.SizeEstimate();
                BOOST_CHECK_LT(pending.SizeEstimate(), 4096U);
                const auto start = std::chrono::steady_clock::now();
                BOOST_REQUIRE(database.WritePoSStateTransition(state, block.GetHash(), undo));
                write_us += std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - start).count();
                history.emplace_back(block.GetHash(), std::move(undo));
                if (i % 16 == 15) CheckStored(database, state);
            }
        }
        // Reopen the real database, then recover a multi-block suffix atomically.
        {
            CBlockTreeDB database(1 << 20);
            CheckStored(database, state);
            std::vector<uint256> hashes;
            for (auto it = history.rbegin(); it != history.rend(); ++it) {
                hashes.push_back(it->first);
                BOOST_REQUIRE(state.UndoBlock(it->second));
            }
            std::vector<uint256> bad_order = hashes;
            std::swap(bad_order[0], bad_order[1]);
            BOOST_CHECK(!database.WritePoSStateRecovery(state, bad_order));
            BOOST_REQUIRE(database.WritePoSStateRecovery(state, hashes));
            BOOST_CHECK(StateHash(state) == initial_hash);
            CheckStored(database, state);
            pos::StateUndo removed;
            for (const auto& hash : hashes) BOOST_CHECK(!database.ReadPoSUndo(hash, removed));
        }
        BOOST_TEST_MESSAGE("PoS disk stress bonds=" << count << " full_batch_bytes=" << full_bytes
            << " max_delta_bytes=" << maximum_delta << " delta_bytes_64_blocks=" << total_delta
            << " durable_write_us=" << write_us);
    }
}

BOOST_AUTO_TEST_SUITE_END()
