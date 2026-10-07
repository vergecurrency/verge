// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/crypto.h>
#include <key.h>
#include <keystore.h>
#include <policy/policy.h>
#include <script/sign.h>
#include <script/standard.h>
#include <streams.h>
#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <cstring>

BOOST_FIXTURE_TEST_SUITE(pos_crypto_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(schnorr_round_trip_and_domain_failure)
{
    unsigned char secret[32]{};
    secret[31] = 3;
    unsigned char auxiliary[32]{};
    auxiliary[0] = 7;
    const uint256 message = uint256S("1234");

    unsigned char signature[pos::SCHNORR_SIGNATURE_SIZE]{};
    unsigned char public_key[pos::SCHNORR_PUBLIC_KEY_SIZE]{};
    BOOST_REQUIRE(pos::SignSchnorr(secret, message, auxiliary, signature,
                                   public_key));
    unsigned char derived_public[pos::SCHNORR_PUBLIC_KEY_SIZE]{};
    BOOST_REQUIRE(pos::GetSchnorrPublicKey(secret, derived_public));
    BOOST_CHECK_EQUAL_COLLECTIONS(
        public_key, public_key + pos::SCHNORR_PUBLIC_KEY_SIZE,
        derived_public, derived_public + pos::SCHNORR_PUBLIC_KEY_SIZE);
    BOOST_CHECK(pos::VerifySchnorr(public_key, message, signature));

    const uint256 other_message = uint256S("1235");
    BOOST_CHECK(!pos::VerifySchnorr(public_key, other_message, signature));

    signature[0] ^= 1;
    BOOST_CHECK(!pos::VerifySchnorr(public_key, message, signature));
}

BOOST_AUTO_TEST_CASE(reject_invalid_secret_and_public_key)
{
    unsigned char zero_secret[32]{};
    unsigned char auxiliary[32]{};
    unsigned char signature[pos::SCHNORR_SIGNATURE_SIZE]{};
    unsigned char public_key[pos::SCHNORR_PUBLIC_KEY_SIZE]{};
    BOOST_CHECK(!pos::SignSchnorr(zero_secret, uint256S("01"), auxiliary,
                                  signature, public_key));
    BOOST_CHECK(!pos::GetSchnorrPublicKey(zero_secret, public_key));

    std::memset(public_key, 0xff, sizeof(public_key));
    BOOST_CHECK(!pos::VerifySchnorr(public_key, uint256S("01"), signature));
}

BOOST_AUTO_TEST_CASE(transaction_replay_protection)
{
    std::vector<unsigned char> secret(32, 0);
    secret.back() = 1;
    CKey key;
    key.Set(secret.begin(), secret.end(), true);
    BOOST_REQUIRE(key.IsValid());

    CBasicKeyStore keystore;
    BOOST_REQUIRE(keystore.AddKeyPubKey(key, key.GetPubKey()));
    const CScript script_pub_key =
        GetScriptForDestination(key.GetPubKey().GetID());
    const CAmount amount = 25 * COIN;

    CMutableTransaction spend;
    spend.nVersion = 1;
    spend.nTime = 1700000000;
    spend.nLockTime = 0;
    spend.vin.emplace_back(
        COutPoint(uint256S("01"), 0), CScript(), CTxIn::SEQUENCE_FINAL);
    spend.vout.emplace_back(amount - CENT, CScript() << OP_TRUE);

    const ReplayProtectionContext mainnet_context(1, 15000000);
    const ReplayProtectionContext testnet_context(2, 3500);
    const unsigned int legacy_flags = STANDARD_SCRIPT_VERIFY_FLAGS;
    const unsigned int pos_flags = STANDARD_SCRIPT_VERIFY_FLAGS |
        SCRIPT_VERIFY_POS_REPLAY_PROTECTION;
    ScriptError error = SCRIPT_ERR_UNKNOWN_ERROR;

    CMutableTransaction legacy_spend(spend);
    SignatureData legacy_signature;
    BOOST_REQUIRE(ProduceSignature(
        keystore,
        MutableTransactionSignatureCreator(
            &legacy_spend, 0, amount, SIGHASH_ALL),
        script_pub_key, legacy_signature));
    UpdateInput(legacy_spend.vin[0], legacy_signature);
    const CTransaction legacy_tx(legacy_spend);
    BOOST_CHECK(VerifyScript(
        legacy_tx.vin[0].scriptSig, script_pub_key, nullptr, legacy_flags,
        TransactionSignatureChecker(&legacy_tx, 0, amount), &error));
    BOOST_CHECK(!VerifyScript(
        legacy_tx.vin[0].scriptSig, script_pub_key, nullptr, pos_flags,
        TransactionSignatureChecker(&legacy_tx, 0, amount,
                                    mainnet_context),
        &error));

    CMutableTransaction pos_spend(spend);
    pos_spend.nVersion = CTransaction::POS_REPLAY_PROTECTED_VERSION;
    const uint256 pos_signature_hash = SignatureHash(
        script_pub_key, pos_spend, 0,
        SIGHASH_ALL | SIGHASH_POS_FORKID, amount, SigVersion::BASE,
        nullptr, mainnet_context);
    BOOST_CHECK_EQUAL(
        pos_signature_hash.GetHex(),
        "f01c4c99170e98487365ecf490cdc61bd33840bed7ba6f44c2297bad31f4c289");
    SignatureData pos_signature;
    BOOST_REQUIRE(ProduceSignature(
        keystore,
        MutableTransactionSignatureCreator(
            &pos_spend, 0, amount,
            SIGHASH_ALL | SIGHASH_POS_FORKID, mainnet_context),
        script_pub_key, pos_signature));
    UpdateInput(pos_spend.vin[0], pos_signature);
    const CTransaction pos_tx(pos_spend);
    BOOST_CHECK(VerifyScript(
        pos_tx.vin[0].scriptSig, script_pub_key, nullptr, pos_flags,
        TransactionSignatureChecker(&pos_tx, 0, amount, mainnet_context),
        &error));

    // Legacy PoW and other Verge networks derive different signature hashes.
    BOOST_CHECK(!VerifyScript(
        pos_tx.vin[0].scriptSig, script_pub_key, nullptr, legacy_flags,
        TransactionSignatureChecker(&pos_tx, 0, amount), &error));
    BOOST_CHECK(!VerifyScript(
        pos_tx.vin[0].scriptSig, script_pub_key, nullptr, pos_flags,
        TransactionSignatureChecker(&pos_tx, 0, amount, testnet_context),
        &error));

    // Signatureless scripts are protected by the mandatory transaction
    // envelope rather than being made invalid by the signature interpreter.
    const CScript anyone_can_spend = CScript() << OP_TRUE;
    BOOST_CHECK(VerifyScript(
        CScript(), anyone_can_spend, nullptr, pos_flags,
        TransactionSignatureChecker(&pos_tx, 0, amount, mainnet_context),
        &error));

    CDataStream encoded(SER_NETWORK, PROTOCOL_VERSION);
    encoded << pos_spend;
    BOOST_REQUIRE(encoded.size() >= 12);
    BOOST_CHECK_EQUAL(encoded[8], 0x00);
    BOOST_CHECK_EQUAL(encoded[9], 0x58);
    BOOST_CHECK_EQUAL(encoded[10], 0x56);
    BOOST_CHECK_EQUAL(encoded[11], 0x47);

    CDataStream legacy_reader(encoded.begin(), encoded.end(), SER_NETWORK,
                              PROTOCOL_VERSION);
    int32_t legacy_version = 0;
    uint32_t legacy_time = 0;
    std::vector<CTxIn> legacy_inputs;
    legacy_reader >> legacy_version >> legacy_time >> legacy_inputs;
    BOOST_CHECK_EQUAL(legacy_version,
                      CTransaction::POS_REPLAY_PROTECTED_VERSION);
    BOOST_CHECK(legacy_inputs.empty());

    CMutableTransaction decoded;
    encoded >> decoded;
    BOOST_CHECK_EQUAL(decoded.nVersion,
                      CTransaction::POS_REPLAY_PROTECTED_VERSION);
    BOOST_CHECK(decoded.GetHash() == pos_spend.GetHash());

    CDataStream malformed(SER_NETWORK, PROTOCOL_VERSION);
    malformed << pos_spend;
    malformed[9] ^= 1;
    CMutableTransaction malformed_tx;
    BOOST_CHECK_THROW(malformed >> malformed_tx, std::ios_base::failure);
}

BOOST_AUTO_TEST_SUITE_END()
