// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/crypto.h>
#include <pos/stake.h>
#include <pos/vrf.h>
#include <key.h>
#include <keystore.h>
#include <policy/policy.h>
#include <script/sign.h>
#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <cstring>

namespace {

uint160 KeyId(const char* hex)
{
    uint160 result;
    result.SetHex(hex);
    return result;
}

} // namespace
BOOST_FIXTURE_TEST_SUITE(pos_stake_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(bond_script_round_trip)
{
    pos::BondData expected;
    unsigned char secret[32]{};
    secret[31] = 1;
    unsigned char vrf_secret[32]{};
    BOOST_REQUIRE(pos::GetSchnorrPublicKey(secret,
                                           expected.signing_public_key));
    BOOST_REQUIRE(pos::DeriveVrfKey(secret, vrf_secret,
                                    expected.vrf_public_key));
    expected.reward_key_id = KeyId("01");
    expected.withdrawal_key_id = KeyId("02");

    const CScript script = pos::GetBondScript(expected);
    BOOST_CHECK(!script.empty());

    pos::BondData decoded;
    BOOST_CHECK(pos::ParseBondScript(script, decoded));
    BOOST_CHECK(decoded.reward_key_id == expected.reward_key_id);
    BOOST_CHECK(decoded.withdrawal_key_id == expected.withdrawal_key_id);
    BOOST_CHECK_EQUAL_COLLECTIONS(decoded.signing_public_key, decoded.signing_public_key + sizeof(decoded.signing_public_key), expected.signing_public_key, expected.signing_public_key + sizeof(expected.signing_public_key));
    BOOST_CHECK_EQUAL_COLLECTIONS(decoded.vrf_public_key, decoded.vrf_public_key + sizeof(decoded.vrf_public_key), expected.vrf_public_key, expected.vrf_public_key + sizeof(expected.vrf_public_key));

    CScript trailing = script;
    trailing << OP_DROP;
    BOOST_CHECK(!pos::ParseBondScript(trailing, decoded));
}

BOOST_AUTO_TEST_CASE(unbond_script_round_trip)
{
    pos::UnbondData expected;
    expected.unlock_height = 20160;
    expected.withdrawal_key_id = KeyId("03");

    const CScript script = pos::GetUnbondScript(expected);
    BOOST_CHECK(!script.empty());

    pos::UnbondData decoded;
    BOOST_CHECK(pos::ParseUnbondScript(script, decoded));
    BOOST_CHECK_EQUAL(decoded.unlock_height, expected.unlock_height);
    BOOST_CHECK(decoded.withdrawal_key_id == expected.withdrawal_key_id);
}

BOOST_AUTO_TEST_CASE(reject_invalid_stake_metadata)
{
    pos::BondData bond;
    BOOST_CHECK(pos::GetBondScript(bond).empty());

    pos::UnbondData unbond;
    BOOST_CHECK(pos::GetUnbondScript(unbond).empty());

    CScript ordinary = CScript() << OP_DUP << OP_HASH160
                                 << std::vector<unsigned char>(20, 1)
                                 << OP_EQUALVERIFY << OP_CHECKSIG;
    BOOST_CHECK(!pos::ParseBondScript(ordinary, bond));
    BOOST_CHECK(!pos::ParseUnbondScript(ordinary, unbond));

    bond.reward_key_id = KeyId("01");
    bond.withdrawal_key_id = KeyId("02");
    // Out-of-field coordinates are unambiguously invalid on both curves.
    std::memset(bond.signing_public_key, 0xff,
                sizeof(bond.signing_public_key));
    std::memset(bond.vrf_public_key, 0xff,
                sizeof(bond.vrf_public_key));
    bond.vrf_public_key[0] = 0x02;
    BOOST_CHECK(!pos::IsValid(bond));
    BOOST_CHECK(pos::GetBondScript(bond).empty());
}

BOOST_AUTO_TEST_CASE(sign_stake_withdrawal_scripts)
{
    unsigned char secret[32]{};
    secret[31] = 1;
    CKey withdrawal;
    withdrawal.MakeNewKey(true);
    CBasicKeyStore owner;
    BOOST_REQUIRE(owner.AddKey(withdrawal));

    pos::BondData bond;
    unsigned char vrf_secret[32]{};
    BOOST_REQUIRE(pos::GetSchnorrPublicKey(secret, bond.signing_public_key));
    BOOST_REQUIRE(pos::DeriveVrfKey(secret, vrf_secret, bond.vrf_public_key));
    bond.withdrawal_key_id = withdrawal.GetPubKey().GetID();
    bond.reward_key_id = KeyId("01");
    pos::UnbondData unbond;
    unbond.unlock_height = 22000;
    unbond.withdrawal_key_id = bond.withdrawal_key_id;
    const std::vector<CScript> scripts{
        pos::GetBondScript(bond), pos::GetUnbondScript(unbond)};

    for (const bool replay_protected : {false, true}) {
        const ReplayProtectionContext context = replay_protected
            ? ReplayProtectionContext(1, 1500) : ReplayProtectionContext();
        const int hash_type = SIGHASH_ALL |
            (replay_protected ? SIGHASH_POS_FORKID : 0);
        for (const CScript& script : scripts) {
            CMutableTransaction spend;
            if (replay_protected) {
                spend.nVersion = CTransaction::POS_REPLAY_PROTECTED_VERSION;
            }
            spend.vin.emplace_back(COutPoint(uint256S("01"), 0));
            spend.vout.emplace_back(COIN, CScript() << OP_TRUE);
            MutableTransactionSignatureCreator creator(
                &spend, 0, 1000 * COIN, hash_type, context);
            SignatureData signatures;
            BOOST_REQUIRE(ProduceSignature(owner, creator, script, signatures));
            BOOST_CHECK(IsSolvable(owner, script));
            SignatureData dummy;
            BOOST_CHECK(ProduceSignature(owner, DUMMY_SIGNATURE_CREATOR,
                                         script, dummy));

            const SignatureData combined = CombineSignatures(
                script, creator.Checker(), SignatureData(), signatures);
            BOOST_CHECK(VerifyScript(combined.scriptSig, script,
                                      &combined.scriptWitness,
                                      STANDARD_SCRIPT_VERIFY_FLAGS,
                                      creator.Checker()));

            CBasicKeyStore stranger;
            CKey delegated_key;
            delegated_key.Set(secret, secret + sizeof(secret), true);
            BOOST_REQUIRE(stranger.AddKey(delegated_key));
            SignatureData missing_key;
            BOOST_CHECK(!ProduceSignature(stranger, creator, script,
                                          missing_key));

            CScript changed_metadata = script;
            // Alter a payload byte while leaving the withdrawal key unchanged.
            changed_metadata[script.size() == 35 ? 5 : 75] ^= 1;
            BOOST_CHECK(!VerifyScript(signatures.scriptSig, changed_metadata,
                                       &signatures.scriptWitness,
                                       STANDARD_SCRIPT_VERIFY_FLAGS,
                                       creator.Checker()));
            CScript malformed = script;
            malformed << OP_DROP;
            BOOST_CHECK(!IsSolvable(owner, malformed));
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
