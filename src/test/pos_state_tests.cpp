// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <coins.h>
#include <pos/consensus.h>
#include <pos/crypto.h>
#include <pos/state.h>
#include <pos/votepool.h>
#include <pos/vrf.h>
#include <primitives/block.h>
#include <script/standard.h>
#include <shutdown.h>
#include <streams.h>
#include <test/setup_common.h>
#include <txdb.h>
#include <util/system.h>
#include <validation.h>

#include <boost/test/unit_test.hpp>

#include <cstring>
#include <chrono>
#include <map>

namespace {

struct PoSStateTestingSetup : BasicTestingSetup {
    PoSStateTestingSetup() : BasicTestingSetup(CBaseChainParams::REGTEST) {}
};

pos::BondData MakeBond()
{
    pos::BondData bond;
    unsigned char secret[32]{};
    secret[31] = 1;
    unsigned char vrf_secret[32]{};
    BOOST_REQUIRE(pos::GetSchnorrPublicKey(secret,
                                           bond.signing_public_key));
    BOOST_REQUIRE(pos::DeriveVrfKey(secret, vrf_secret,
                                    bond.vrf_public_key));
    bond.reward_key_id.SetHex("01");
    bond.withdrawal_key_id.SetHex("02");
    return bond;
}

CBlock MakeBlock(const std::vector<CTransactionRef>& transactions)
{
    CBlock block;
    block.nVersion = 4;
    block.vtx = transactions;
    return block;
}

CTransactionRef MakeCoinbase()
{
    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vin[0].prevout.SetNull();
    tx.vout.emplace_back(0, CScript() << OP_TRUE);
    return MakeTransactionRef(tx);
}

class IterableCoinsView : public CCoinsView
{
public:
    void Add(const COutPoint& outpoint, const Coin& coin)
    {
        m_coins.emplace(outpoint, coin);
    }

    bool GetCoin(const COutPoint& outpoint, Coin& coin) const override
    {
        const auto it = m_coins.find(outpoint);
        if (it == m_coins.end() || it->second.IsSpent()) return false;
        coin = it->second;
        return true;
    }

    bool ForEachCoin(
        const std::function<bool(const COutPoint&, const Coin&)>& callback)
        const override
    {
        for (const auto& entry : m_coins) {
            if (!entry.second.IsSpent() &&
                !callback(entry.first, entry.second)) {
                return false;
            }
        }
        return true;
    }

private:
    std::map<COutPoint, Coin> m_coins;
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(pos_state_tests, PoSStateTestingSetup)

BOOST_AUTO_TEST_CASE(initialize_from_cached_coins_overlay)
{
    const CAmount value = Params().GetConsensus().nPoSMinStake;
    const CScript bond_script = pos::GetBondScript(MakeBond());
    const COutPoint keep(uint256S("01"), 0);
    const COutPoint spend(uint256S("02"), 0);
    const COutPoint override_coin(uint256S("03"), 0);
    const COutPoint parent_add(uint256S("04"), 0);
    const COutPoint tip_add(uint256S("05"), 0);
    const COutPoint pre_rules(uint256S("06"), 0);
    const COutPoint below_minimum(uint256S("07"), 0);

    IterableCoinsView base;
    base.Add(keep, Coin(CTxOut(value, bond_script), 1, false, 1));
    base.Add(spend, Coin(CTxOut(value, bond_script), 2, false, 2));
    base.Add(override_coin,
             Coin(CTxOut(value, bond_script), 3, false, 3));
    base.Add(pre_rules,
             Coin(CTxOut(value, bond_script), 0, false, 0));
    base.Add(below_minimum,
             Coin(CTxOut(value - 1, bond_script), 7, false, 7));

    CCoinsViewCache parent(&base);
    parent.AddCoin(parent_add,
                   Coin(CTxOut(value, bond_script), 4, false, 4), false);
    CCoinsViewCache tip(&parent);
    BOOST_REQUIRE(tip.SpendCoin(spend));
    tip.AddCoin(override_coin,
                Coin(CTxOut(value, CScript() << OP_TRUE), 5, false, 5), true);
    tip.AddCoin(tip_add,
                Coin(CTxOut(value, bond_script), 6, false, 6), false);

    pos::State state;
    BOOST_REQUIRE(state.InitializeFromCoins(tip, uint256S("10"),
                                            Params().GetConsensus()));
    BOOST_CHECK(state.FindBond(keep) != nullptr);
    BOOST_CHECK(state.FindBond(spend) == nullptr);
    BOOST_CHECK(state.FindBond(override_coin) == nullptr);
    BOOST_CHECK(state.FindBond(parent_add) != nullptr);
    BOOST_CHECK(state.FindBond(tip_add) != nullptr);
    BOOST_CHECK(state.FindBond(pre_rules) == nullptr);
    BOOST_CHECK(state.FindBond(below_minimum) == nullptr);
    BOOST_CHECK_EQUAL(state.Bonds().size(), 3U);
}

BOOST_AUTO_TEST_CASE(track_bonds_snapshots_and_undo)
{
    Consensus::Params params = Params().GetConsensus();
    params.nPoSActivationHeight = 1000;
    params.nPoSEpochSlots = 120;
    params.nPoSSnapshotDelayEpochs = 2;
    params.nPoSStakeMaturity = 720;

    CCoinsView base;
    CCoinsViewCache coins(&base);
    pos::State state;

    CMutableTransaction create;
    create.vin.resize(1);
    create.vin[0].prevout = COutPoint(uint256S("10"), 0);
    create.vout.emplace_back(params.nPoSMinStake,
                             pos::GetBondScript(MakeBond()));
    CBlock creation_block =
        MakeBlock({MakeCoinbase(), MakeTransactionRef(create)});

    pos::StateUndo creation_undo;
    BOOST_REQUIRE(state.ApplyBlock(creation_block, 1, false, 0, 0,
                                   params, creation_undo));
    const COutPoint bond_outpoint(create.GetHash(), 0);
    BOOST_REQUIRE(state.FindBond(bond_outpoint) != nullptr);
    BOOST_CHECK_EQUAL(state.FindBond(bond_outpoint)->creation_height, 1);

    CBlock snapshot_block = MakeBlock({MakeCoinbase()});
    pos::StateUndo snapshot_undo;
    BOOST_REQUIRE(state.ApplyBlock(snapshot_block, 760, false, 0, 0,
                                   params, snapshot_undo));
    const pos::StakeSnapshot* initial =
        state.FindSnapshot(pos::INITIAL_SNAPSHOT_EPOCH);
    BOOST_REQUIRE(initial != nullptr);
    BOOST_CHECK_EQUAL(initial->entries.size(), 1U);
    BOOST_CHECK_EQUAL(initial->total_value, params.nPoSMinStake);
    BOOST_CHECK(!initial->root.IsNull());

    pos::StateUndo epoch_undo;
    BOOST_REQUIRE(state.SetEpochSeed(0, uint256S("01"), epoch_undo));
    snapshot_block.hashPrevBlock = uint256S("aa");
    BOOST_REQUIRE(state.ApplyBlock(snapshot_block, 1100, true, 1, 0,
                                   params, epoch_undo));
    const pos::StakeSnapshot* epoch_zero = state.FindSnapshot(0);
    BOOST_REQUIRE(epoch_zero != nullptr);
    BOOST_CHECK_EQUAL(epoch_zero->entries.size(), 1U);

    BOOST_CHECK(state.UndoBlock(epoch_undo));
    BOOST_CHECK(state.FindSnapshot(0) == nullptr);
    BOOST_CHECK(state.FindEpochSeed(0) == nullptr);
    BOOST_CHECK(state.FindBond(bond_outpoint) != nullptr);

    BOOST_CHECK(state.UndoBlock(snapshot_undo));
    BOOST_CHECK(state.FindSnapshot(pos::INITIAL_SNAPSHOT_EPOCH) == nullptr);

    BOOST_CHECK(state.UndoBlock(creation_undo));
    BOOST_CHECK(state.FindBond(bond_outpoint) == nullptr);
}

BOOST_AUTO_TEST_CASE(stale_pre_activation_snapshot_can_be_replaced)
{
    Consensus::Params params = Params().GetConsensus();
    params.nPoSActivationHeight = 1000;
    params.nPoSEpochSlots = 120;
    params.nPoSSnapshotDelayEpochs = 2;

    pos::State state;
    pos::StateUndo undo;
    BOOST_REQUIRE(state.ApplyBlock(MakeBlock({MakeCoinbase()}), 760, false,
                                   0, 0, params, undo));
    BOOST_REQUIRE(state.FindSnapshot(pos::INITIAL_SNAPSHOT_EPOCH) != nullptr);
    BOOST_CHECK(state.RemovePreActivationSnapshot(800));
    BOOST_CHECK(state.FindSnapshot(pos::INITIAL_SNAPSHOT_EPOCH) == nullptr);
}

BOOST_AUTO_TEST_CASE(initial_seed_entropy_is_post_snapshot_and_reversible)
{
    Consensus::Params params = Params().GetConsensus();
    params.nPoSActivationHeight = 1000;
    params.nPoSEpochSlots = 2;
    params.nPoSSnapshotDelayEpochs = 2;

    pos::State state;
    CBlock block = MakeBlock({MakeCoinbase()});
    block.nTime = 1;
    pos::StateUndo snapshot_undo;
    BOOST_REQUIRE(state.ApplyBlock(block, 996, false, 0, 0,
                                   params, snapshot_undo));
    BOOST_CHECK(state.InitialPoWEntropy().IsNull());

    block.nTime = 2;
    pos::StateUndo first_undo;
    BOOST_REQUIRE(state.ApplyBlock(block, 997, false, 0, 0,
                                   params, first_undo));
    const uint256 first_entropy = state.InitialPoWEntropy();
    BOOST_CHECK(!first_entropy.IsNull());

    block.nTime = 3;
    pos::StateUndo second_undo;
    BOOST_REQUIRE(state.ApplyBlock(block, 998, false, 0, 0,
                                   params, second_undo));
    BOOST_CHECK(state.InitialPoWEntropy() != first_entropy);
    BOOST_CHECK(state.UndoBlock(second_undo));
    BOOST_CHECK(state.InitialPoWEntropy() == first_entropy);
    BOOST_CHECK(state.UndoBlock(first_undo));
    BOOST_CHECK(state.InitialPoWEntropy().IsNull());
}

BOOST_AUTO_TEST_CASE(epoch_seed_commits_to_accepted_vrf_outputs)
{
    Consensus::Params params = Params().GetConsensus();
    params.nPoSActivationHeight = 1000;
    params.nPoSEpochSlots = 2;
    params.nPoSSnapshotDelayEpochs = 2;

    pos::State state;
    pos::StateUndo snapshot_undo;
    BOOST_REQUIRE(state.ApplyBlock(MakeBlock({MakeCoinbase()}), 996, false,
                                   0, 0, params, snapshot_undo));
    const pos::StakeSnapshot* snapshot =
        state.FindSnapshot(pos::INITIAL_SNAPSHOT_EPOCH);
    BOOST_REQUIRE(snapshot != nullptr);

    const uint256 initial_seed = uint256S("11");
    pos::StateUndo seed_undo;
    BOOST_REQUIRE(state.SetEpochSeed(0, initial_seed, seed_undo));

    CBlock first = MakeBlock({MakeCoinbase()});
    first.hashPrevBlock = uint256S("aa");
    first.nTime = 10;
    first.posExtension.stake_proof.slot = 10;
    first.posExtension.stake_proof.bond_outpoint =
        COutPoint(uint256S("21"), 0);
    first.posExtension.stake_proof.vrf_output[31] = 1;
    pos::StateUndo first_undo;
    BOOST_REQUIRE(state.ApplyBlock(first, 1000, true, 0, 0,
                                   params, first_undo));

    CBlock second = MakeBlock({MakeCoinbase()});
    second.nTime = 11;
    second.posExtension.stake_proof.slot = 11;
    second.posExtension.stake_proof.bond_outpoint =
        COutPoint(uint256S("22"), 0);
    second.posExtension.stake_proof.vrf_output[31] = 2;
    pos::StateUndo second_undo;
    BOOST_REQUIRE(state.ApplyBlock(second, 1001, true, 0, 1,
                                   params, second_undo));

    uint256 entropy = pos::MixEpochEntropy(
        uint256(), 0, first.posExtension.stake_proof.slot,
        first.posExtension.stake_proof.bond_outpoint,
        first.posExtension.stake_proof.vrf_output);
    entropy = pos::MixEpochEntropy(
        entropy, 0, second.posExtension.stake_proof.slot,
        second.posExtension.stake_proof.bond_outpoint,
        second.posExtension.stake_proof.vrf_output);

    CBlock transition = MakeBlock({MakeCoinbase()});
    transition.nTime = 12;
    transition.posExtension.stake_proof.slot = 12;
    transition.posExtension.stake_proof.bond_outpoint =
        COutPoint(uint256S("23"), 0);
    transition.posExtension.stake_proof.vrf_output[31] = 3;
    pos::StateUndo transition_undo;
    BOOST_REQUIRE(state.ApplyBlock(transition, 1002, true, 1, 0,
                                   params, transition_undo));
    const uint256* next_seed = state.FindEpochSeed(1);
    BOOST_REQUIRE(next_seed != nullptr);
    BOOST_CHECK(*next_seed == pos::ComputeNextEpochSeed(
        initial_seed, 1, snapshot->root, entropy));

    BOOST_CHECK(state.UndoBlock(transition_undo));
    BOOST_CHECK(state.FindEpochSeed(1) == nullptr);
    BOOST_CHECK(state.UndoBlock(second_undo));
    BOOST_CHECK(state.UndoBlock(first_undo));
}

BOOST_AUTO_TEST_CASE(skipped_epochs_advance_and_undo)
{
    Consensus::Params params = Params().GetConsensus();
    params.nPoSActivationHeight = 1000;
    params.nPoSEpochSlots = 120;
    params.nPoSSnapshotDelayEpochs = 2;
    params.nPoSStakeMaturity = 720;

    pos::State state;
    pos::StateUndo snapshot_undo;
    BOOST_REQUIRE(state.ApplyBlock(MakeBlock({MakeCoinbase()}), 760, false,
                                   0, 0, params, snapshot_undo));
    pos::StateUndo seed_undo;
    BOOST_REQUIRE(state.SetEpochSeed(0, uint256S("02"), seed_undo));

    CBlock block = MakeBlock({MakeCoinbase()});
    block.hashPrevBlock = uint256S("aa");
    block.posExtension.stake_proof.vrf_output[31] = 1;
    pos::StateUndo transition_undo;
    BOOST_REQUIRE(state.ApplyBlock(block, 1001, true, 3, 5,
                                   params, transition_undo));
    BOOST_REQUIRE(state.FindEpochSeed(1) != nullptr);
    BOOST_REQUIRE(state.FindEpochSeed(2) != nullptr);
    BOOST_REQUIRE(state.FindEpochSeed(3) != nullptr);
    BOOST_REQUIRE(state.FindSnapshot(0) != nullptr);
    BOOST_REQUIRE(state.FindSnapshot(1) != nullptr);
    BOOST_REQUIRE(state.FindSnapshot(2) != nullptr);

    BOOST_CHECK(state.UndoBlock(transition_undo));
    BOOST_CHECK(state.FindEpochSeed(1) == nullptr);
    BOOST_CHECK(state.FindEpochSeed(2) == nullptr);
    BOOST_CHECK(state.FindEpochSeed(3) == nullptr);
    BOOST_CHECK(state.FindSnapshot(0) == nullptr);
    BOOST_CHECK(state.FindSnapshot(1) == nullptr);
    BOOST_CHECK(state.FindSnapshot(2) == nullptr);
    BOOST_CHECK(state.UndoBlock(snapshot_undo));
}

BOOST_AUTO_TEST_CASE(epoch_history_is_bounded_and_reversible)
{
    Consensus::Params params = Params().GetConsensus();
    params.nPoSActivationHeight = 1000;
    params.nPoSEpochSlots = 120;
    params.nPoSSnapshotDelayEpochs = 2;
    params.nPoSStakeMaturity = 720;

    pos::State state;
    pos::StateUndo snapshot_undo;
    BOOST_REQUIRE(state.ApplyBlock(MakeBlock({MakeCoinbase()}), 760, false,
                                   0, 0, params, snapshot_undo));
    pos::StateUndo seed_undo;
    BOOST_REQUIRE(state.SetEpochSeed(0, uint256S("0a"), seed_undo));

    CBlock block = MakeBlock({MakeCoinbase()});
    block.hashPrevBlock = uint256S("0b");
    pos::StateUndo transition_undo;
    const uint64_t target_epoch = pos::POS_STATE_RETENTION_EPOCHS + 2;
    BOOST_REQUIRE(state.ApplyBlock(block, 1001, true, target_epoch, 0,
                                   params, transition_undo));
    BOOST_CHECK(state.FindSnapshot(pos::INITIAL_SNAPSHOT_EPOCH) == nullptr);
    BOOST_CHECK(state.FindSnapshot(0) == nullptr);
    BOOST_CHECK(state.FindSnapshot(2) != nullptr);
    BOOST_CHECK(state.FindEpochSeed(0) == nullptr);
    BOOST_CHECK(state.FindEpochSeed(2) != nullptr);

    BOOST_CHECK(state.UndoBlock(transition_undo));
    BOOST_CHECK(state.FindSnapshot(pos::INITIAL_SNAPSHOT_EPOCH) != nullptr);
    BOOST_CHECK(state.FindSnapshot(0) == nullptr);
    BOOST_CHECK(state.FindEpochSeed(0) != nullptr);
    BOOST_CHECK(state.FindEpochSeed(2) == nullptr);
}

BOOST_AUTO_TEST_CASE(final_pow_anchor_bootstraps_finality)
{
    Consensus::Params params = Params().GetConsensus();
    params.nPoSActivationHeight = 1000;
    params.nPoSEpochSlots = 120;
    params.nPoSSnapshotDelayEpochs = 2;
    params.nPoSStakeMaturity = 720;

    pos::State state;
    CMutableTransaction create;
    create.vin.resize(1);
    create.vin[0].prevout = COutPoint(uint256S("20"), 0);
    create.vout.emplace_back(params.nPoSMinStake,
                             pos::GetBondScript(MakeBond()));
    const COutPoint bond_outpoint(create.GetHash(), 0);

    pos::StateUndo creation_undo;
    BOOST_REQUIRE(state.ApplyBlock(
        MakeBlock({MakeCoinbase(), MakeTransactionRef(create)}), 1, false,
        0, 0, params, creation_undo));
    pos::StateUndo snapshot_undo;
    BOOST_REQUIRE(state.ApplyBlock(MakeBlock({MakeCoinbase()}), 760, false,
                                   0, 0, params, snapshot_undo));

    CBlock epoch_zero = MakeBlock({MakeCoinbase()});
    epoch_zero.hashPrevBlock = uint256S("aa");
    epoch_zero.posExtension.stake_proof.slot = 1;
    epoch_zero.posExtension.stake_proof.vrf_output[31] = 1;
    pos::StateUndo epoch_zero_undo;
    BOOST_REQUIRE(state.SetEpochSeed(0, uint256S("03"), epoch_zero_undo));
    BOOST_REQUIRE(state.ApplyBlock(epoch_zero, 1000, true, 0, 0,
                                   params, epoch_zero_undo));

    pos::CheckpointVote vote_zero;
    vote_zero.bond_outpoint = bond_outpoint;
    vote_zero.snapshot_epoch = pos::INITIAL_SNAPSHOT_EPOCH;
    vote_zero.source_epoch = pos::FINAL_POW_CHECKPOINT_EPOCH;
    vote_zero.source_checkpoint_root = epoch_zero.hashPrevBlock;
    vote_zero.target_epoch = 0;
    vote_zero.target_checkpoint_root = epoch_zero.GetHash();
    vote_zero.head_slot = 1;
    vote_zero.head_block_root = epoch_zero.GetHash();
    pos::VotePool vote_pool;
    BOOST_REQUIRE(vote_pool.Add(vote_zero) == pos::VotePoolResult::ADDED);
    BOOST_CHECK(state.CanApplyVote(vote_zero));
    BOOST_REQUIRE(state.ApplyVotes({vote_zero}, epoch_zero_undo));
    BOOST_CHECK(!state.CanApplyVote(vote_zero));
    vote_pool.RemoveObsolete(state);
    BOOST_CHECK_EQUAL(vote_pool.Size(), 0U);
    BOOST_CHECK_EQUAL(vote_pool.DynamicMemoryUsage(), 0U);
    pos::CheckpointVote newer_head = vote_zero;
    newer_head.head_slot = 2;
    newer_head.head_block_root = uint256S("bb");
    BOOST_CHECK(state.CanApplyVote(newer_head));
    BOOST_REQUIRE(vote_pool.Add(newer_head) == pos::VotePoolResult::ADDED);
    vote_pool.RemoveObsolete(state);
    BOOST_CHECK_EQUAL(vote_pool.Size(), 1U);
    pos::CheckpointVote conflicting_checkpoint = newer_head;
    conflicting_checkpoint.target_checkpoint_root = uint256S("cc");
    BOOST_CHECK(!state.CanApplyVote(conflicting_checkpoint));
    BOOST_REQUIRE(state.FindJustified(0) != nullptr);
    BOOST_CHECK_EQUAL(state.Finalized().epoch,
                      pos::FINAL_POW_CHECKPOINT_EPOCH);

    CBlock epoch_one = MakeBlock({MakeCoinbase()});
    epoch_one.hashPrevBlock = epoch_zero.GetHash();
    epoch_one.nTime = 1;
    epoch_one.posExtension.stake_proof.slot = 121;
    epoch_one.posExtension.stake_proof.vrf_output[31] = 2;
    pos::StateUndo epoch_one_undo;
    BOOST_REQUIRE(state.ApplyBlock(epoch_one, 1001, true, 1, 0,
                                   params, epoch_one_undo));

    pos::CheckpointVote vote_one = vote_zero;
    vote_one.source_epoch = 0;
    vote_one.source_checkpoint_root = epoch_zero.GetHash();
    vote_one.target_epoch = 1;
    vote_one.target_checkpoint_root = epoch_one.GetHash();
    vote_one.head_slot = 121;
    vote_one.head_block_root = epoch_one.GetHash();
    BOOST_CHECK(state.CanApplyVote(vote_one));
    BOOST_REQUIRE(state.ApplyVotes({vote_one}, epoch_one_undo));
    BOOST_REQUIRE(state.FindJustified(1) != nullptr);
    BOOST_CHECK_EQUAL(state.Finalized().epoch, 0U);
    BOOST_CHECK(state.Finalized().root == epoch_zero.GetHash());
    BOOST_CHECK(!state.CanApplyVote(vote_zero));
    BOOST_CHECK(!state.ApplyVotes({vote_zero}, epoch_one_undo));

    BOOST_CHECK(state.UndoBlock(epoch_one_undo));
    BOOST_CHECK(state.FindJustified(1) == nullptr);
    BOOST_CHECK_EQUAL(state.Finalized().epoch,
                      pos::FINAL_POW_CHECKPOINT_EPOCH);
    BOOST_CHECK(state.UndoBlock(epoch_zero_undo));
    BOOST_CHECK(state.FindJustified(0) == nullptr);
    BOOST_CHECK(state.Finalized().root.IsNull());
}

BOOST_AUTO_TEST_CASE(incremental_vote_tallies_match_rebuild_after_replace_prune_and_undo)
{
    Consensus::Params params = Params().GetConsensus();
    params.nPoSActivationHeight = 1000;
    params.nPoSStakeMaturity = 1;
    const pos::BondData bond = MakeBond();
    std::vector<CTransactionRef> transactions{MakeCoinbase()};
    for (uint32_t i = 0; i < 128; ++i) {
        CMutableTransaction tx;
        tx.vin.emplace_back(COutPoint(uint256S("01"), i));
        for (uint32_t j = 0; j < 16; ++j) {
            tx.vout.emplace_back(params.nPoSMinStake, pos::GetBondScript(bond));
        }
        transactions.push_back(MakeTransactionRef(tx));
    }
    pos::State state;
    pos::StateUndo create_undo;
    BOOST_REQUIRE(state.ApplyBlock(MakeBlock(transactions), 1, false, 0, 0,
                                   params, create_undo));
    pos::StateUndo snapshot_undo;
    BOOST_REQUIRE(state.ApplyBlock(MakeBlock({MakeCoinbase()}), 760, false,
                                   0, 0, params, snapshot_undo));
    CBlock first = MakeBlock({MakeCoinbase()});
    first.hashPrevBlock = uint256S("aa");
    pos::StateUndo first_undo;
    BOOST_REQUIRE(state.SetEpochSeed(0, uint256S("01"), first_undo));
    BOOST_REQUIRE(state.ApplyBlock(first, 1000, true, 0, 0, params, first_undo));

    auto cold_copy = [](const pos::State& source) {
        CDataStream data(SER_DISK, CLIENT_VERSION);
        data << source;
        pos::State result;
        data >> result;
        return result;
    };
    auto check_same = [](const pos::State& a, const pos::State& b) {
        CDataStream left(SER_DISK, CLIENT_VERSION), right(SER_DISK, CLIENT_VERSION);
        left << a;
        right << b;
        BOOST_CHECK(left.size() == right.size() &&
                    std::equal(left.begin(), left.end(), right.begin()));
    };
    pos::State rebuilt = cold_copy(state);
    std::vector<pos::CheckpointVote> votes;
    for (const auto& item : state.Bonds()) {
        pos::CheckpointVote vote;
        vote.bond_outpoint = item.first;
        vote.snapshot_epoch = pos::INITIAL_SNAPSHOT_EPOCH;
        vote.source_epoch = pos::FINAL_POW_CHECKPOINT_EPOCH;
        vote.source_checkpoint_root = first.hashPrevBlock;
        vote.target_epoch = 0;
        vote.target_checkpoint_root = first.GetHash();
        vote.head_slot = 1;
        vote.head_block_root = first.GetHash();
        votes.push_back(vote);
    }
    BOOST_REQUIRE_EQUAL(votes.size(), 2048U);
    for (size_t start = 0; start < votes.size(); start += 256) {
        const std::vector<pos::CheckpointVote> batch(votes.begin() + start,
                                                     votes.begin() + start + 256);
        rebuilt = cold_copy(rebuilt);
        pos::StateUndo warm_undo, cold_undo;
        BOOST_REQUIRE(state.ApplyVotes(batch, warm_undo));
        BOOST_REQUIRE(rebuilt.ApplyVotes(batch, cold_undo));
        check_same(state, rebuilt);
    }
    for (auto& vote : votes) ++vote.head_slot;
    pos::StateUndo replacement, rebuilt_replacement;
    BOOST_REQUIRE(state.ApplyVotes(votes, replacement));
    rebuilt = cold_copy(rebuilt);
    BOOST_REQUIRE(rebuilt.ApplyVotes(votes, rebuilt_replacement));
    check_same(state, rebuilt);
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        pos::StateUndo unused;
        BOOST_REQUIRE(state.ApplyVotes({}, unused));
    }
    const auto warm_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - start).count();
    const auto cold_start = std::chrono::steady_clock::now();
    int64_t rebuild_us = 0;
    for (int i = 0; i < 100; ++i) {
        pos::State cold = cold_copy(state);
        pos::StateUndo unused;
        const auto apply_start = std::chrono::steady_clock::now();
        BOOST_REQUIRE(cold.ApplyVotes({}, unused));
        rebuild_us += std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - apply_start).count();
        check_same(state, cold);
    }
    BOOST_TEST_MESSAGE("2048 bonds, 100 empty vote applications: warm=" << warm_us
        << "us, rebuild=" << rebuild_us << "us, deserialize/rebuild/compare="
        << std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now() - cold_start).count() << "us");

    CBlock later = MakeBlock({MakeCoinbase()});
    later.hashPrevBlock = first.GetHash();
    later.nTime = 30000;
    pos::StateUndo prune, rebuilt_prune;
    BOOST_REQUIRE(state.ApplyBlock(later, 1001, true, 10, 0, params, prune));
    BOOST_REQUIRE(rebuilt.ApplyBlock(later, 1001, true, 10, 0, params, rebuilt_prune));
    pos::StateUndo empty;
    BOOST_REQUIRE(state.ApplyVotes({}, empty));
    BOOST_REQUIRE(rebuilt.ApplyVotes({}, empty));
    check_same(state, rebuilt);
    BOOST_REQUIRE(state.UndoBlock(prune));
    BOOST_REQUIRE(rebuilt.UndoBlock(rebuilt_prune));
    rebuilt = cold_copy(rebuilt);
    BOOST_REQUIRE(state.ApplyVotes({}, empty));
    BOOST_REQUIRE(rebuilt.ApplyVotes({}, empty));
    check_same(state, rebuilt);
    BOOST_REQUIRE(state.UndoBlock(replacement));
    BOOST_REQUIRE(rebuilt.UndoBlock(rebuilt_replacement));
    check_same(state, rebuilt);
}

BOOST_AUTO_TEST_CASE(snapshot_excludes_immature_bond)
{
    Consensus::Params params = Params().GetConsensus();
    params.nPoSActivationHeight = 1000;
    params.nPoSEpochSlots = 120;
    params.nPoSSnapshotDelayEpochs = 2;
    params.nPoSStakeMaturity = 720;

    CCoinsView base;
    CCoinsViewCache coins(&base);
    pos::State state;

    CMutableTransaction create;
    create.vin.resize(1);
    create.vin[0].prevout = COutPoint(uint256S("11"), 0);
    create.vout.emplace_back(params.nPoSMinStake,
                             pos::GetBondScript(MakeBond()));

    pos::StateUndo creation_undo;
    BOOST_REQUIRE(state.ApplyBlock(
        MakeBlock({MakeCoinbase(), MakeTransactionRef(create)}), 500, false,
        0, 0, params, creation_undo));

    pos::StateUndo snapshot_undo;
    BOOST_REQUIRE(state.ApplyBlock(MakeBlock({MakeCoinbase()}), 760, false,
                                   0, 0, params, snapshot_undo));
    const pos::StakeSnapshot* initial =
        state.FindSnapshot(pos::INITIAL_SNAPSHOT_EPOCH);
    BOOST_REQUIRE(initial != nullptr);
    BOOST_CHECK(initial->entries.empty());
    BOOST_CHECK_EQUAL(initial->total_value, 0);
}

BOOST_AUTO_TEST_CASE(equivocation_lockout_activation_expiry_and_undo)
{
    Consensus::Params params = Params().GetConsensus();
    params.nPoSActivationHeight = 1000;
    params.nPoSStakeMaturity = 720;
    params.nPoSUnbondingBlocks = 5;
    params.nPoSSnapshotDelayEpochs = 2;

    pos::State state;
    CMutableTransaction create;
    create.vin.resize(1);
    create.vin[0].prevout = COutPoint(uint256S("30"), 0);
    create.vout.emplace_back(params.nPoSMinStake,
                             pos::GetBondScript(MakeBond()));
    const COutPoint bond_outpoint(create.GetHash(), 0);
    pos::StateUndo creation_undo;
    BOOST_REQUIRE(state.ApplyBlock(
        MakeBlock({MakeCoinbase(), MakeTransactionRef(create)}), 1, false,
        0, 0, params, creation_undo));
    pos::StateUndo snapshot_undo;
    BOOST_REQUIRE(state.ApplyBlock(MakeBlock({MakeCoinbase()}), 760, false,
                                   0, 0, params, snapshot_undo));

    pos::BlockEquivocationEvidence evidence;
    evidence.first.bond_outpoint = bond_outpoint;
    evidence.second.bond_outpoint = bond_outpoint;
    evidence.first.slot = 10;
    evidence.second.slot = 10;
    evidence.first.candidate_header_hash = uint256S("01");
    evidence.second.candidate_header_hash = uint256S("02");

    CMutableTransaction withdraw;
    withdraw.vin.resize(1);
    withdraw.vin[0].prevout = bond_outpoint;
    CBlock invalid = MakeBlock({MakeCoinbase(), MakeTransactionRef(withdraw)});
    invalid.hashPrevBlock = uint256S("aa");
    invalid.posExtension.block_evidence.push_back(evidence);
    pos::StateUndo invalid_undo;
    BOOST_REQUIRE(state.SetEpochSeed(0, uint256S("04"), invalid_undo));
    BOOST_CHECK(!state.ApplyBlock(invalid, 1000, true, 0, 0,
                                  params, invalid_undo));
    BOOST_CHECK(!state.IsWithdrawalLocked(bond_outpoint, 1000));

    pos::StateUndo seed_undo;
    BOOST_REQUIRE(state.SetEpochSeed(0, uint256S("04"), seed_undo));
    CBlock offense = MakeBlock({MakeCoinbase()});
    offense.hashPrevBlock = uint256S("aa");
    offense.posExtension.block_evidence.push_back(evidence);
    pos::StateUndo offense_undo;
    BOOST_REQUIRE(state.ApplyBlock(offense, 1000, true, 0, 0,
                                   params, offense_undo));
    BOOST_CHECK(state.IsWithdrawalLocked(bond_outpoint, 1000));
    BOOST_CHECK(!state.IsEligibilityLocked(bond_outpoint, 0, 1000));

    CBlock activation = MakeBlock({MakeCoinbase()});
    activation.hashPrevBlock = offense.GetHash();
    activation.posExtension.stake_proof.vrf_output[31] = 1;
    pos::StateUndo activation_undo;
    BOOST_REQUIRE(state.ApplyBlock(activation, 1001, true, 1, 0,
                                   params, activation_undo));
    BOOST_CHECK(state.IsEligibilityLocked(bond_outpoint, 1, 1001));
    BOOST_CHECK(state.IsWithdrawalLocked(bond_outpoint, 1005));
    BOOST_CHECK(!state.IsEligibilityLocked(bond_outpoint, 1, 1006));
    BOOST_CHECK(!state.IsWithdrawalLocked(bond_outpoint, 1006));

    CBlock replay = MakeBlock({MakeCoinbase()});
    replay.hashPrevBlock = activation.GetHash();
    replay.posExtension.block_evidence.push_back(evidence);
    replay.posExtension.stake_proof.vrf_output[31] = 2;
    pos::StateUndo replay_undo;
    BOOST_REQUIRE(state.ApplyBlock(replay, 1006, true, 1, 1,
                                   params, replay_undo));
    BOOST_CHECK(!state.IsEligibilityLocked(bond_outpoint, 1, 1006));
    BOOST_CHECK(!state.IsWithdrawalLocked(bond_outpoint, 1006));

    pos::BlockEquivocationEvidence later_evidence = evidence;
    later_evidence.first.slot = 20;
    later_evidence.second.slot = 20;
    later_evidence.first.candidate_header_hash = uint256S("03");
    later_evidence.second.candidate_header_hash = uint256S("04");
    CBlock renewal = MakeBlock({MakeCoinbase()});
    renewal.hashPrevBlock = replay.GetHash();
    renewal.posExtension.block_evidence.push_back(later_evidence);
    renewal.posExtension.stake_proof.vrf_output[31] = 3;
    pos::StateUndo renewal_undo;
    BOOST_REQUIRE(state.ApplyBlock(renewal, 1007, true, 1, 2,
                                   params, renewal_undo));
    BOOST_CHECK(state.IsWithdrawalLocked(bond_outpoint, 1007));
    BOOST_CHECK(!state.IsEligibilityLocked(bond_outpoint, 1, 1007));

    CBlock renewal_activation = MakeBlock({MakeCoinbase()});
    renewal_activation.hashPrevBlock = renewal.GetHash();
    renewal_activation.posExtension.stake_proof.vrf_output[31] = 4;
    pos::StateUndo renewal_activation_undo;
    BOOST_REQUIRE(state.ApplyBlock(renewal_activation, 1008, true, 2, 0,
                                   params, renewal_activation_undo));
    BOOST_CHECK(state.IsEligibilityLocked(bond_outpoint, 2, 1008));
    BOOST_CHECK(state.IsWithdrawalLocked(bond_outpoint, 1012));
    BOOST_CHECK(!state.IsEligibilityLocked(bond_outpoint, 2, 1013));

    BOOST_CHECK(state.UndoBlock(renewal_activation_undo));
    BOOST_CHECK(state.UndoBlock(renewal_undo));
    BOOST_CHECK(state.UndoBlock(replay_undo));

    BOOST_CHECK(state.UndoBlock(activation_undo));
    BOOST_CHECK(!state.IsEligibilityLocked(bond_outpoint, 0, 1000));
    BOOST_CHECK(state.IsWithdrawalLocked(bond_outpoint, 1000));
    BOOST_CHECK(state.UndoBlock(offense_undo));
    BOOST_CHECK(!state.IsWithdrawalLocked(bond_outpoint, 1000));
    BOOST_CHECK(state.UndoBlock(seed_undo));
}

BOOST_AUTO_TEST_CASE(journal_keeps_recovery_undo_until_explicit_pruning)
{
    SetDataDir("pos-journal");
    ClearDatadirCache();
    CBlockTreeDB database(1 << 20, true);
    Consensus::Params params = Params().GetConsensus();
    params.nPoSActivationHeight = 1000;
    pos::State state;
    CBlock first = MakeBlock({MakeCoinbase()});
    first.nTime = 1;
    pos::StateUndo first_undo;
    BOOST_REQUIRE(state.ApplyBlock(first, 1, false, 0, 0, params, first_undo));
    BOOST_REQUIRE(database.WritePoSStateTransition(state, first.GetHash(), first_undo));
    CBlock second = MakeBlock({MakeCoinbase()});
    second.hashPrevBlock = first.GetHash();
    second.nTime = 2;
    pos::StateUndo second_undo;
    BOOST_REQUIRE(state.ApplyBlock(second, 2, false, 0, 0, params, second_undo));
    BOOST_REQUIRE(database.WritePoSStateTransition(state, second.GetHash(), second_undo));

    pos::State loaded;
    pos::StateUndo undo;
    BOOST_REQUIRE(database.ReadPoSState(loaded));
    BOOST_CHECK(loaded.BestBlock() == second.GetHash());
    BOOST_REQUIRE(database.ReadPoSUndo(second.GetHash(), undo));
    BOOST_REQUIRE(loaded.UndoBlock(undo));
    BOOST_CHECK(loaded.BestBlock() == first.GetHash());
    BOOST_REQUIRE(database.ReadPoSUndo(first.GetHash(), undo));

    BOOST_REQUIRE(database.PrunePoSUndo({first.GetHash()}));
    BOOST_CHECK(!database.ReadPoSUndo(first.GetHash(), undo));
    BOOST_CHECK(database.ReadPoSUndo(second.GetHash(), undo));
    BOOST_REQUIRE(database.ReadPoSState(loaded));
    BOOST_CHECK(loaded.BestBlock() == second.GetHash());
    ClearDatadirCache();
}

BOOST_FIXTURE_TEST_CASE(failed_block_flush_does_not_commit_utxos, TestChain100Setup)
{
    FlushStateToDisk();
    const uint256 saved_tip = pcoinsdbview->GetBestBlock();
    const CBlock next = CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
    BOOST_REQUIRE(next.GetHash() != saved_tip);
    BOOST_REQUIRE(pcoinsdbview->GetBestBlock() == saved_tip);

    const fs::path block_file = GetBlocksDir() / "blk00000.dat";
    const fs::path backup = GetBlocksDir() / "blk00000.saved";
    fs::rename(block_file, backup);
    const auto restore = [&] {
        fs::remove(block_file);
        fs::rename(backup, block_file);
        AbortShutdown();
    };
    try {
        fs::create_directory(block_file);
        FlushStateToDisk();
        BOOST_CHECK(ShutdownRequested());
        BOOST_CHECK(pcoinsdbview->GetBestBlock() == saved_tip);
    } catch (...) {
        restore();
        throw;
    }
    restore();
    FlushStateToDisk();
    BOOST_CHECK(pcoinsdbview->GetBestBlock() == next.GetHash());
}

BOOST_AUTO_TEST_SUITE_END()
