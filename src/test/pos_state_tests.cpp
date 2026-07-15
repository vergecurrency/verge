// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <coins.h>
#include <pos/state.h>
#include <primitives/block.h>
#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <cstring>

namespace {

pos::BondData MakeBond()
{
    pos::BondData bond;
    std::memset(bond.signing_public_key, 1, sizeof(bond.signing_public_key));
    bond.vrf_public_key[0] = 0x02;
    std::memset(bond.vrf_public_key + 1, 2, sizeof(bond.vrf_public_key) - 1);
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

} // namespace

BOOST_FIXTURE_TEST_SUITE(pos_state_tests, BasicTestingSetup)

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

BOOST_AUTO_TEST_CASE(skipped_epochs_advance_and_undo)
{
    Consensus::Params params = Params().GetConsensus();
    params.nPoSActivationHeight = 1000;
    params.nPoSEpochSlots = 120;
    params.nPoSSnapshotDelayEpochs = 2;
    params.nPoSStakeMaturity = 720;

    pos::State state;
    pos::StateUndo seed_undo;
    BOOST_REQUIRE(state.SetEpochSeed(0, uint256S("02"), seed_undo));

    CBlock block = MakeBlock({MakeCoinbase()});
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
    BOOST_REQUIRE(state.ApplyVotes({vote_zero}, epoch_zero_undo));
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
    BOOST_REQUIRE(state.ApplyVotes({vote_one}, epoch_one_undo));
    BOOST_REQUIRE(state.FindJustified(1) != nullptr);
    BOOST_CHECK_EQUAL(state.Finalized().epoch, 0U);
    BOOST_CHECK(state.Finalized().root == epoch_zero.GetHash());

    BOOST_CHECK(state.UndoBlock(epoch_one_undo));
    BOOST_CHECK(state.FindJustified(1) == nullptr);
    BOOST_CHECK_EQUAL(state.Finalized().epoch,
                      pos::FINAL_POW_CHECKPOINT_EPOCH);
    BOOST_CHECK(state.UndoBlock(epoch_zero_undo));
    BOOST_CHECK(state.FindJustified(0) == nullptr);
    BOOST_CHECK(state.Finalized().root.IsNull());
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

    pos::BlockEquivocationEvidence evidence;
    evidence.first.bond_outpoint = bond_outpoint;
    evidence.second.bond_outpoint = bond_outpoint;
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

    BOOST_CHECK(state.UndoBlock(activation_undo));
    BOOST_CHECK(!state.IsEligibilityLocked(bond_outpoint, 0, 1000));
    BOOST_CHECK(state.IsWithdrawalLocked(bond_outpoint, 1000));
    BOOST_CHECK(state.UndoBlock(offense_undo));
    BOOST_CHECK(!state.IsWithdrawalLocked(bond_outpoint, 1000));
    BOOST_CHECK(state.UndoBlock(seed_undo));
}

BOOST_AUTO_TEST_SUITE_END()
