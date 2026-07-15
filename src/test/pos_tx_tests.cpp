// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <coins.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <pos/stake.h>
#include <pos/state.h>
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

COutPoint AddTestCoin(CCoinsViewCache& view, const CTxOut& output, int height,
                      bool coinbase = false, uint32_t discriminator = 1)
{
    CMutableTransaction source;
    source.nTime = discriminator;
    source.vout.push_back(output);
    const COutPoint outpoint(source.GetHash(), 0);
    view.AddCoin(outpoint, Coin(output, height, coinbase, source.nTime), false);
    return outpoint;
}

bool Check(const CMutableTransaction& tx, CValidationState& state,
           const CCoinsViewCache& view, int spend_height,
           const pos::State* pos_state = nullptr)
{
    CAmount fee = 0;
    return Consensus::CheckTxInputs(CTransaction(tx), state, view, spend_height,
                                    fee, Params().GetConsensus(), pos_state);
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

BOOST_FIXTURE_TEST_SUITE(pos_tx_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(enforce_bond_minimum_and_coinbase_maturity)
{
    const Consensus::Params& params = Params().GetConsensus();
    CCoinsView base;
    CCoinsViewCache view(&base);
    const CScript ordinary = CScript() << OP_TRUE;

    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vin[0].prevout = AddTestCoin(
        view, CTxOut(params.nPoSMinStake, ordinary), 100, false);
    tx.vout.emplace_back(params.nPoSMinStake - 1, pos::GetBondScript(MakeBond()));

    CValidationState state;
    BOOST_CHECK(!Check(tx, state, view, 1000));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-pos-bond-value");

    CCoinsView coinbase_base;
    CCoinsViewCache coinbase_view(&coinbase_base);
    tx.vin[0].prevout = AddTestCoin(
        coinbase_view, CTxOut(params.nPoSMinStake, ordinary), 900, true, 2);
    tx.vout[0].nValue = params.nPoSMinStake;

    CValidationState immature_state;
    BOOST_CHECK(!Check(tx, immature_state, coinbase_view, 1000));
    BOOST_CHECK_EQUAL(immature_state.GetRejectReason(),
                      "bad-pos-premature-coinbase-bond");

    CValidationState mature_state;
    BOOST_CHECK(Check(tx, mature_state, coinbase_view,
                      900 + params.nPoSStakeMaturity));
}

BOOST_AUTO_TEST_CASE(require_canonical_unbond_transition)
{
    const Consensus::Params& params = Params().GetConsensus();
    const int spend_height = 50000;
    const CAmount principal = params.nPoSMinStake;
    const pos::BondData bond = MakeBond();

    CCoinsView base;
    CCoinsViewCache view(&base);
    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vin[0].prevout =
        AddTestCoin(view, CTxOut(principal, pos::GetBondScript(bond)), 1000);

    pos::UnbondData unbond;
    unbond.unlock_height = spend_height + params.nPoSUnbondingBlocks;
    unbond.withdrawal_key_id = bond.withdrawal_key_id;
    tx.vout.emplace_back(principal, pos::GetUnbondScript(unbond));

    CValidationState valid_state;
    BOOST_CHECK(Check(tx, valid_state, view, spend_height));

    tx.vout[0].nValue -= 1;
    CValidationState principal_state;
    BOOST_CHECK(!Check(tx, principal_state, view, spend_height));
    BOOST_CHECK_EQUAL(principal_state.GetRejectReason(),
                      "bad-pos-unbond-principal");

    tx.vout[0].nValue = principal;
    unbond.unlock_height += 1;
    tx.vout[0].scriptPubKey = pos::GetUnbondScript(unbond);
    CValidationState height_state;
    BOOST_CHECK(!Check(tx, height_state, view, spend_height));
    BOOST_CHECK_EQUAL(height_state.GetRejectReason(), "bad-pos-unbond-height");
}

BOOST_AUTO_TEST_CASE(reject_unbacked_and_premature_unbond)
{
    const Consensus::Params& params = Params().GetConsensus();
    const int spend_height = 70000;
    const CScript ordinary = CScript() << OP_TRUE;

    pos::UnbondData unbond;
    unbond.unlock_height = spend_height + params.nPoSUnbondingBlocks;
    unbond.withdrawal_key_id.SetHex("02");

    CCoinsView ordinary_base;
    CCoinsViewCache ordinary_view(&ordinary_base);
    CMutableTransaction create;
    create.vin.resize(1);
    create.vin[0].prevout =
        AddTestCoin(ordinary_view, CTxOut(params.nPoSMinStake, ordinary), 1000);
    create.vout.emplace_back(params.nPoSMinStake, pos::GetUnbondScript(unbond));

    CValidationState unbacked_state;
    BOOST_CHECK(!Check(create, unbacked_state, ordinary_view, spend_height));
    BOOST_CHECK_EQUAL(unbacked_state.GetRejectReason(),
                      "bad-pos-unbond-without-bond");

    CCoinsView unbond_base;
    CCoinsViewCache unbond_view(&unbond_base);
    CMutableTransaction spend;
    spend.vin.resize(1);
    spend.vin[0].prevout = AddTestCoin(
        unbond_view, CTxOut(params.nPoSMinStake, pos::GetUnbondScript(unbond)),
        spend_height);
    spend.vout.emplace_back(params.nPoSMinStake, ordinary);

    CValidationState premature_state;
    BOOST_CHECK(!Check(spend, premature_state, unbond_view,
                       unbond.unlock_height - 1));
    BOOST_CHECK_EQUAL(premature_state.GetRejectReason(),
                      "bad-pos-premature-unbond-spend");

    CValidationState unlocked_state;
    BOOST_CHECK(Check(spend, unlocked_state, unbond_view,
                      unbond.unlock_height));
}

BOOST_AUTO_TEST_CASE(reject_evidence_locked_bond_withdrawal)
{
    Consensus::Params params = Params().GetConsensus();
    params.nPoSActivationHeight = 1000;
    const pos::BondData bond = MakeBond();

    CMutableTransaction create;
    create.vin.resize(1);
    create.vin[0].prevout = COutPoint(uint256S("30"), 0);
    create.vout.emplace_back(params.nPoSMinStake,
                             pos::GetBondScript(bond));
    const COutPoint bond_outpoint(create.GetHash(), 0);

    pos::State state;
    CBlock creation_block;
    creation_block.nVersion = 4;
    creation_block.vtx = {MakeCoinbase(), MakeTransactionRef(create)};
    pos::StateUndo creation_undo;
    BOOST_REQUIRE(state.ApplyBlock(creation_block, 1, false, 0, 0,
                                   params, creation_undo));

    pos::BlockEquivocationEvidence evidence;
    evidence.first.bond_outpoint = bond_outpoint;
    evidence.second.bond_outpoint = bond_outpoint;
    evidence.first.candidate_header_hash = uint256S("01");
    evidence.second.candidate_header_hash = uint256S("02");
    pos::StateUndo seed_undo;
    BOOST_REQUIRE(state.SetEpochSeed(0, uint256S("04"), seed_undo));
    CBlock offense;
    offense.nVersion = 4;
    offense.hashPrevBlock = uint256S("aa");
    offense.vtx = {MakeCoinbase()};
    offense.posExtension.block_evidence.push_back(evidence);
    pos::StateUndo offense_undo;
    BOOST_REQUIRE(state.ApplyBlock(offense, 1000, true, 0, 0,
                                   params, offense_undo));

    CCoinsView base;
    CCoinsViewCache view(&base);
    view.AddCoin(bond_outpoint,
                 Coin(create.vout[0], 1, false, create.nTime), false);
    CMutableTransaction withdraw;
    withdraw.vin.resize(1);
    withdraw.vin[0].prevout = bond_outpoint;
    withdraw.vout.emplace_back(params.nPoSMinStake,
                               CScript() << OP_TRUE);

    CValidationState validation_state;
    BOOST_CHECK(!Check(withdraw, validation_state, view, 1000, &state));
    BOOST_CHECK_EQUAL(validation_state.GetRejectReason(),
                      "bad-pos-locked-bond-withdrawal");
}

BOOST_AUTO_TEST_SUITE_END()
