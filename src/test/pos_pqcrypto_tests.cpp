// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/pqcrypto.h>
#include <test/data/mldsa44.json.h>
#include <test/setup_common.h>
#include <util/strencodings.h>
#include <chainparams.h>
#include <bech32.h>
#include <key_io.h>
#include <policy/policy.h>
#include <pos/stake.h>
#include <pos/state.h>
#include <pos/validation.h>
#include <script/ismine.h>
#include <script/pq.h>
#include <script/sign.h>
#include <script/vergeconsensus.h>
#include <streams.h>
#include <coins.h>
#include <consensus/tx_verify.h>
#ifdef ENABLE_WALLET
#include <wallet/crypter.h>
#endif

#include <openssl/err.h>
#include <openssl/opensslv.h>
#include <univalue.h>
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <thread>

namespace pq = pos::experimental;

namespace {
#ifdef ENABLE_WALLET
class PQTestKeyStore : public CCryptoKeyStore {
public:
    using CCryptoKeyStore::EncryptKeys;
    using CCryptoKeyStore::Unlock;
    bool HasPlainPQKeys() const { LOCK(cs_KeyStore); return !mapPQKeys.empty(); }
};
#endif

UniValue Vectors()
{
    UniValue result;
    if (!result.read(std::string(json_tests::mldsa44,
                                json_tests::mldsa44 + sizeof(json_tests::mldsa44)))) {
        throw std::runtime_error("Invalid ML-DSA-44 test vectors");
    }
    return result;
}

bool HaveBackend()
{
    if (pq::MLDSA44Available()) return true;
    BOOST_TEST_MESSAGE("ML-DSA-44 backend unavailable; positive cases require OpenSSL >= 3.5");
    return false;
}
} // namespace

BOOST_FIXTURE_TEST_SUITE(pos_pqcrypto_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(backend_availability_and_fail_closed)
{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_ML_DSA) && !defined(LIBRESSL_VERSION_NUMBER)
    BOOST_REQUIRE(pq::MLDSA44Available());
#else
    BOOST_CHECK(!pq::MLDSA44Available());
    pq::MLDSA44Key key;
    const std::array<unsigned char, 32> seed{};
    BOOST_CHECK(!key.SetSeed(seed.data(), seed.size()));
    BOOST_CHECK(!key.MakeNewKey());
    BOOST_CHECK(!key.IsValid());
    std::vector<unsigned char> public_key(3, 1), signature(4, 1);
    BOOST_CHECK(!key.GetPublicKey(public_key));
    BOOST_CHECK(public_key.empty());
    BOOST_CHECK(!key.Sign({}, {}, signature));
    BOOST_CHECK(signature.empty());
    BOOST_CHECK(!pq::VerifyMLDSA44(
        std::vector<unsigned char>(pq::MLDSA44_PUBLIC_KEY_SIZE), {}, {},
        std::vector<unsigned char>(pq::MLDSA44_SIGNATURE_SIZE)));
    unsigned char secret[32]{};
    secret[31] = 3;
    unsigned char auxiliary[32]{};
    pq::HybridPublicKey hybrid_key;
    pq::HybridSignature hybrid_signature;
    BOOST_CHECK(!pq::SignHybrid(secret, auxiliary, key, 1, pq::HybridPurpose::BLOCK,
                                uint256S("01"), hybrid_key, hybrid_signature));
    BOOST_CHECK(!pq::VerifyHybrid(hybrid_key, 1, pq::HybridPurpose::BLOCK,
                                  uint256S("01"), hybrid_signature));
    BOOST_REQUIRE(pos::GetSchnorrPublicKey(secret, hybrid_key.schnorr.data()));
    hybrid_key.mldsa.resize(pq::MLDSA44_PUBLIC_KEY_SIZE);
    hybrid_signature.mldsa.resize(pq::MLDSA44_SIGNATURE_SIZE);
    uint256 hash;
    BOOST_REQUIRE(pq::GetHybridSigningHash(1, pq::HybridPurpose::BLOCK,
                                          hybrid_key, uint256S("01"), hash));
    BOOST_REQUIRE(pos::SignSchnorr(secret, hash, auxiliary,
                                   hybrid_signature.schnorr.data(),
                                   hybrid_key.schnorr.data()));
    BOOST_CHECK(pos::VerifySchnorr(hybrid_key.schnorr.data(), hash,
                                  hybrid_signature.schnorr.data()));
    BOOST_CHECK(!pq::VerifyHybrid(hybrid_key, 1, pq::HybridPurpose::BLOCK,
                                  uint256S("01"), hybrid_signature));
#endif
}

BOOST_AUTO_TEST_CASE(nist_acvp_key_generation)
{
    if (!HaveBackend()) return;
    const UniValue vectors = Vectors();
    const auto seed = ParseHex(vectors["keygen"]["seed"].get_str());
    const auto expected = ParseHex(vectors["keygen"]["public_key"].get_str());
    pq::MLDSA44Key key;
    BOOST_REQUIRE(key.SetSeed(seed.data(), seed.size()));
    std::vector<unsigned char> public_key;
    BOOST_REQUIRE(key.GetPublicKey(public_key));
    BOOST_CHECK_EQUAL(public_key.size(), pq::MLDSA44_PUBLIC_KEY_SIZE);
    BOOST_CHECK_EQUAL_COLLECTIONS(public_key.begin(), public_key.end(),
                                  expected.begin(), expected.end());
}

BOOST_AUTO_TEST_CASE(hybrid_fixed_transcript_hash)
{
    // Independently computed from the documented byte layout, not SignHybrid.
    const UniValue vectors = Vectors();
    pq::HybridPublicKey public_key;
    const auto schnorr = ParseHex("f9308a019258c31049344f85f89d5229b531c845836f99b08601f113bce036f9");
    std::copy(schnorr.begin(), schnorr.end(), public_key.schnorr.begin());
    public_key.mldsa = ParseHex(vectors["keygen"]["public_key"].get_str());
    uint256 hash;
    BOOST_REQUIRE(pq::GetHybridSigningHash(1, pq::HybridPurpose::BLOCK,
                                          public_key, uint256S("1234"), hash));
    BOOST_CHECK_EQUAL(hash.GetHex(),
                      "61c16e65017556276660fdb235733eac2b4c2f2e9c230d2b831231d839e68fa3");
}

BOOST_AUTO_TEST_CASE(nist_acvp_external_pure_verification)
{
    if (!HaveBackend()) return;
    const UniValue vectors = Vectors();
    for (const UniValue& test : vectors["verify"].getValues()) {
        const auto public_key = ParseHex(test["public_key"].get_str());
        auto message = ParseHex(test["message"].get_str());
        const auto context = ParseHex(test["context"].get_str());
        const auto signature = ParseHex(test["signature"].get_str());
        BOOST_CHECK_EQUAL(pq::VerifyMLDSA44(public_key, message, context, signature),
                          test["valid"].get_bool());
        if (test["valid"].get_bool()) {
            BOOST_REQUIRE(!message.empty());
            message[0] ^= 1;
            BOOST_CHECK(!pq::VerifyMLDSA44(public_key, message, context, signature));
        }
    }
}

BOOST_AUTO_TEST_CASE(key_lifecycle_and_raw_signing_boundaries)
{
    if (!HaveBackend()) return;
    pq::MLDSA44Key key;
    std::vector<unsigned char> public_key(3, 1), signature(4, 1);
    BOOST_CHECK(!key.GetPublicKey(public_key));
    BOOST_CHECK(public_key.empty());
    BOOST_CHECK(!key.Sign({}, {}, signature));
    BOOST_CHECK(signature.empty());
    BOOST_REQUIRE(key.MakeNewKey());
    BOOST_REQUIRE(key.GetPublicKey(public_key));
    const auto old_public_key = public_key;
    BOOST_REQUIRE(key.MakeNewKey());
    BOOST_REQUIRE(key.GetPublicKey(public_key));
    BOOST_CHECK(public_key != old_public_key);
    for (const size_t message_size : {size_t{0}, size_t{32}, pq::MLDSA44_MAX_MESSAGE_SIZE}) {
        const std::vector<unsigned char> message(message_size, 42);
        const std::vector<unsigned char> context(pq::MLDSA44_MAX_CONTEXT_SIZE, 7);
        BOOST_REQUIRE(key.Sign(message, context, signature));
        BOOST_CHECK_EQUAL(signature.size(), pq::MLDSA44_SIGNATURE_SIZE);
        BOOST_CHECK(pq::VerifyMLDSA44(public_key, message, context, signature));
        BOOST_CHECK(!pq::VerifyMLDSA44(public_key, message, {}, signature));
        auto altered = signature;
        altered.back() ^= 1;
        BOOST_CHECK(!pq::VerifyMLDSA44(public_key, message, context, altered));
    }
    BOOST_REQUIRE(key.Sign({}, {}, signature));
    BOOST_CHECK(pq::VerifyMLDSA44(public_key, {}, {}, signature));
    auto short_key = public_key;
    short_key.pop_back();
    BOOST_CHECK(!pq::VerifyMLDSA44(short_key, {}, {}, signature));
    public_key.push_back(0);
    BOOST_CHECK(!pq::VerifyMLDSA44(public_key, {}, {}, signature));
    public_key.pop_back();
    signature.pop_back();
    BOOST_CHECK(!pq::VerifyMLDSA44(public_key, {}, {}, signature));
    signature.resize(pq::MLDSA44_SIGNATURE_SIZE + 1);
    BOOST_CHECK(!pq::VerifyMLDSA44(public_key, {}, {}, signature));
    const std::vector<unsigned char> oversized_context(pq::MLDSA44_MAX_CONTEXT_SIZE + 1);
    const std::vector<unsigned char> oversized_message(pq::MLDSA44_MAX_MESSAGE_SIZE + 1);
    BOOST_CHECK(!key.Sign({}, oversized_context, signature));
    BOOST_CHECK(signature.empty());
    BOOST_CHECK(!key.Sign(oversized_message, {}, signature));
    BOOST_CHECK(!pq::VerifyMLDSA44(public_key, {}, oversized_context,
                                  std::vector<unsigned char>(pq::MLDSA44_SIGNATURE_SIZE)));
    BOOST_CHECK(!pq::VerifyMLDSA44(public_key, oversized_message, {},
                                  std::vector<unsigned char>(pq::MLDSA44_SIGNATURE_SIZE)));
    std::array<unsigned char, 33> seed{};
    BOOST_CHECK(!key.SetSeed(seed.data(), 31));
    BOOST_CHECK(!key.IsValid());
    BOOST_CHECK(!key.SetSeed(seed.data(), 33));
    BOOST_CHECK(!key.SetSeed(nullptr, 32));
    BOOST_REQUIRE(key.SetSeed(seed.data(), 32));
    key.Reset();
    BOOST_CHECK(!key.IsValid());
    BOOST_CHECK(!key.Sign({}, {}, signature));
    BOOST_CHECK(signature.empty());
    std::vector<unsigned char> aliased{1, 2, 3};
    BOOST_CHECK(!key.Sign(aliased, {}, aliased));
    BOOST_CHECK(aliased == std::vector<unsigned char>({1, 2, 3}));
}

BOOST_AUTO_TEST_CASE(hybrid_requires_both_signatures_and_binds_domain_and_keys)
{
    if (!HaveBackend()) return;
    pq::MLDSA44Key key;
    BOOST_REQUIRE(key.MakeNewKey());
    unsigned char secret[32]{};
    secret[31] = 3;
    unsigned char auxiliary[32]{};
    const uint256 message = uint256S("1234");
    pq::HybridPublicKey public_key;
    pq::HybridSignature signature;
    BOOST_REQUIRE(pq::SignHybrid(secret, auxiliary, key, 1, pq::HybridPurpose::BLOCK,
                                 message, public_key, signature));
    BOOST_CHECK(pq::VerifyHybrid(public_key, 1, pq::HybridPurpose::BLOCK, message, signature));
    BOOST_CHECK(!pq::VerifyHybrid(public_key, 2, pq::HybridPurpose::BLOCK, message, signature));
    BOOST_CHECK(!pq::VerifyHybrid(public_key, 1, pq::HybridPurpose::VOTE, message, signature));
    BOOST_CHECK(!pq::VerifyHybrid(public_key, 1, pq::HybridPurpose::BLOCK,
                                  uint256S("1235"), signature));
    auto altered = signature;
    altered.schnorr.fill(0);
    BOOST_CHECK(!pq::VerifyHybrid(public_key, 1, pq::HybridPurpose::BLOCK, message, altered));
    altered = signature;
    altered.mldsa.clear();
    BOOST_CHECK(!pq::VerifyHybrid(public_key, 1, pq::HybridPurpose::BLOCK, message, altered));
    altered = signature;
    altered.mldsa[0] ^= 1;
    BOOST_CHECK(!pq::VerifyHybrid(public_key, 1, pq::HybridPurpose::BLOCK, message, altered));
    auto other_key = public_key;
    other_key.mldsa[0] ^= 1;
    BOOST_CHECK(!pq::VerifyHybrid(other_key, 1, pq::HybridPurpose::BLOCK, message, signature));
    secret[31] = 4;
    BOOST_REQUIRE(pos::GetSchnorrPublicKey(secret, other_key.schnorr.data()));
    other_key.mldsa = public_key.mldsa;
    BOOST_CHECK(!pq::VerifyHybrid(other_key, 1, pq::HybridPurpose::BLOCK, message, signature));
    uint256 hash;
    BOOST_REQUIRE(pq::GetHybridSigningHash(1, pq::HybridPurpose::BLOCK, public_key, message, hash));
    BOOST_CHECK(pos::VerifySchnorr(public_key.schnorr.data(), hash, signature.schnorr.data()));
    auto aliased_hash = message;
    BOOST_REQUIRE(pq::GetHybridSigningHash(1, pq::HybridPurpose::BLOCK,
                                          public_key, aliased_hash, aliased_hash));
    BOOST_CHECK(hash == aliased_hash);
    BOOST_CHECK(!pq::GetHybridSigningHash(0, pq::HybridPurpose::BLOCK, public_key, message, hash));
    BOOST_CHECK(hash.IsNull());
    BOOST_CHECK(!pq::VerifyHybrid(public_key, 0, pq::HybridPurpose::BLOCK, message, signature));
    BOOST_CHECK(!pq::VerifyHybrid(public_key, 1, static_cast<pq::HybridPurpose>(255), message, signature));
    BOOST_REQUIRE(pq::SignHybrid(secret, auxiliary, key, 2, pq::HybridPurpose::VOTE,
                                 message, public_key, signature));
    BOOST_CHECK(pq::VerifyHybrid(public_key, 2, pq::HybridPurpose::VOTE, message, signature));
    BOOST_CHECK(!pq::SignHybrid(secret, auxiliary, key, 0, pq::HybridPurpose::VOTE,
                               message, public_key, signature));
    BOOST_CHECK(public_key.mldsa.empty());
    BOOST_CHECK(signature.mldsa.empty());
    BOOST_CHECK(std::all_of(signature.schnorr.begin(), signature.schnorr.end(),
                            [](unsigned char c) { return c == 0; }));
}

BOOST_AUTO_TEST_CASE(concurrent_signing_uses_independent_contexts)
{
    if (!HaveBackend()) return;
    pq::MLDSA44Key key;
    BOOST_REQUIRE(key.MakeNewKey());
    std::array<bool, 4> passed{};
    std::vector<std::thread> threads;
    for (size_t i = 0; i < passed.size(); ++i) {
        threads.emplace_back([&, i] {
            unsigned char secret[32]{};
            secret[31] = 3;
            unsigned char auxiliary[32]{};
            bool ok = true;
            for (size_t n = 0; n < 4; ++n) {
                pq::HybridPublicKey public_key;
                pq::HybridSignature signature;
                const auto purpose = n % 2 ? pq::HybridPurpose::BLOCK : pq::HybridPurpose::VOTE;
                ok = ok && pq::SignHybrid(secret, auxiliary, key, 2, purpose,
                                          uint256S("01"), public_key, signature) &&
                    pq::VerifyHybrid(public_key, 2, purpose, uint256S("01"), signature);
            }
            passed[i] = ok;
        });
    }
    for (auto& thread : threads) thread.join();
    for (bool ok : passed) BOOST_CHECK(ok);
}

#ifdef ENABLE_WALLET
BOOST_AUTO_TEST_CASE(pq_keystore_encryption_and_lock_lifecycle)
{
    if (!HaveBackend()) return;
    pq::MLDSA44Key key;
    BOOST_REQUIRE(key.MakeNewKey());
    std::vector<unsigned char> public_key, loaded_public;
    BOOST_REQUIRE(key.GetPublicKey(public_key));
    const uint256 id = pq::GetMLDSA44KeyId(public_key);
    BOOST_CHECK(!id.IsNull());
    BOOST_CHECK(pq::GetMLDSA44KeyId({}).IsNull());
    PQTestKeyStore store;
    BOOST_REQUIRE(store.AddPQKey(key, public_key));
    BOOST_CHECK(store.HavePQKey(id));
    BOOST_CHECK(store.HasPlainPQKeys());
    pq::MLDSA44Key loaded;
    BOOST_REQUIRE(store.GetPQKey(id, loaded));
    BOOST_REQUIRE(loaded.GetPublicKey(loaded_public));
    BOOST_CHECK(loaded_public == public_key);
    auto wrong_public = public_key;
    wrong_public[0] ^= 1;
    BOOST_CHECK(!store.AddPQKey(key, wrong_public));
    CKeyingMaterial master(32, 7), wrong_master(32, 8);
    BOOST_REQUIRE(store.EncryptKeys(master));
    BOOST_CHECK(store.IsCrypted());
    BOOST_CHECK(!store.HasPlainPQKeys());
    BOOST_CHECK(store.IsLocked());
    BOOST_CHECK(store.HavePQKey(id));
    BOOST_REQUIRE(store.GetPQPubKey(id, loaded_public));
    BOOST_CHECK(loaded_public == public_key);
    BOOST_CHECK(!store.GetPQKey(id, loaded));
    BOOST_CHECK(!loaded.IsValid());
    BOOST_CHECK(!store.Unlock(wrong_master));
    BOOST_CHECK(store.IsLocked());
    BOOST_REQUIRE(store.Unlock(master));
    BOOST_REQUIRE(store.GetPQKey(id, loaded));
    std::vector<unsigned char> signature;
    BOOST_REQUIRE(loaded.Sign({1, 2, 3}, {}, signature));
    BOOST_CHECK(pq::VerifyMLDSA44(public_key, {1, 2, 3}, {}, signature));
    pq::MLDSA44Key other;
    BOOST_REQUIRE(other.MakeNewKey());
    std::vector<unsigned char> other_public;
    BOOST_REQUIRE(other.GetPublicKey(other_public));
    BOOST_REQUIRE(store.AddPQKey(other, other_public));
    BOOST_CHECK(!store.HasPlainPQKeys());
    BOOST_REQUIRE(store.Lock());
    BOOST_CHECK(!store.GetPQKey(id, loaded));
    BOOST_CHECK(!store.AddPQKey(other, other_public));
    BOOST_REQUIRE(store.Unlock(master));
    BOOST_REQUIRE(store.GetPQKey(pq::GetMLDSA44KeyId(other_public), loaded));
    BOOST_REQUIRE(loaded.GetPublicKey(loaded_public));
    BOOST_CHECK(loaded_public == other_public);
    loaded_public.assign(1, 1);
    BOOST_CHECK(!store.GetPQPubKey(uint256S("01"), loaded_public));
    BOOST_CHECK(loaded_public.empty());
    BOOST_CHECK(!store.GetPQKey(uint256S("01"), loaded));
    BOOST_CHECK(!loaded.IsValid());
}

#endif

BOOST_AUTO_TEST_CASE(verification_preserves_existing_openssl_errors)
{
    if (!HaveBackend()) return;
    pq::MLDSA44Key key;
    BOOST_REQUIRE(key.MakeNewKey());
    std::vector<unsigned char> public_key;
    BOOST_REQUIRE(key.GetPublicKey(public_key));
    ERR_clear_error();
    ERR_put_error(ERR_LIB_USER, 0, 1, __FILE__, __LINE__);
    const auto previous = ERR_peek_last_error();
    BOOST_REQUIRE(previous != 0);
    BOOST_CHECK(!pq::VerifyMLDSA44(public_key, {}, {},
                                  std::vector<unsigned char>(pq::MLDSA44_SIGNATURE_SIZE)));
    BOOST_CHECK_EQUAL(ERR_peek_last_error(), previous);
    ERR_clear_error();
    BOOST_CHECK(!pq::VerifyMLDSA44(public_key, {}, {},
                                  std::vector<unsigned char>(pq::MLDSA44_SIGNATURE_SIZE)));
    BOOST_CHECK_EQUAL(ERR_peek_error(), 0U);
}

BOOST_AUTO_TEST_SUITE_END()

#ifdef ENABLE_WALLET
BOOST_FIXTURE_TEST_SUITE(pos_pqspend_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(pq_address_network_prefixes)
{
    const std::string network = Params().NetworkIDString();
    struct RestoreNetwork { std::string network; ~RestoreNetwork() { SelectParams(network); } } restore{network};
    const std::string networks[] = {CBaseChainParams::MAIN, CBaseChainParams::TESTNET, CBaseChainParams::REGTEST};
    const std::string prefixes[] = {"xvgpq", "xvgtpq", "xvgrpq"};
    const std::string witnesses[] = {"vg", "vt", "vgrt"};
    const uint256 id = uint256S("123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0");
    const CTxDestination destination = PQKeyHash(id);
    std::vector<std::string> addresses;
    for (size_t i = 0; i < 3; ++i) {
        SelectParams(networks[i]);
        BOOST_CHECK_EQUAL(Params().Bech32PQHRP(), prefixes[i]);
        BOOST_CHECK_EQUAL(Params().Bech32HRP(), witnesses[i]);
        const auto address = EncodeDestination(destination);
        addresses.push_back(address);
        BOOST_CHECK(address.find(prefixes[i] + "1") == 0);
        BOOST_CHECK(DecodeDestination(address) == destination);
        auto upper = address;
        for (char& c : upper) if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
        BOOST_CHECK(DecodeDestination(upper) == destination);
        auto mixed = address;
        mixed[0] = 'X';
        BOOST_CHECK(!IsValidDestinationString(mixed));
        auto damaged = address;
        damaged.back() = damaged.back() == 'q' ? 'p' : 'q';
        BOOST_CHECK(!IsValidDestinationString(damaged));
    }
    for (size_t i = 0; i < 3; ++i) {
        SelectParams(networks[i]);
        for (size_t j = 0; j < 3; ++j) BOOST_CHECK_EQUAL(IsValidDestinationString(addresses[j]), i == j);
    }
}

BOOST_AUTO_TEST_CASE(canonical_scripts_addresses_and_ownership)
{
    if (!HaveBackend()) return;
    pq::MLDSA44Key key;
    BOOST_REQUIRE(key.MakeNewKey());
    std::vector<unsigned char> public_key;
    BOOST_REQUIRE(key.GetPublicKey(public_key));
    const uint256 id = pq::GetMLDSA44KeyId(public_key);
    const CScript script = GetPQScript(id);
    BOOST_CHECK_EQUAL(script.size(), 35U);
    uint256 extracted;
    BOOST_REQUIRE(ExtractPQKeyId(script, extracted));
    BOOST_CHECK(extracted == id);
    const std::string address = EncodeDestination(PQKeyHash(id));
    BOOST_CHECK(DecodeDestination(address) == CTxDestination(PQKeyHash(id)));
    auto damaged = address;
    damaged.back() = damaged.back() == 'q' ? 'p' : 'q';
    BOOST_CHECK(!IsValidDestinationString(damaged));
    txnouttype type;
    BOOST_CHECK(!IsStandard(script, type, false));
    BOOST_CHECK(IsStandard(script, type, true));
    BOOST_CHECK(type == TX_PQ_KEYHASH);
    CBasicKeyStore store;
    BOOST_CHECK_EQUAL(IsMine(store, script), ISMINE_NO);
    BOOST_REQUIRE(store.AddPQKey(key, public_key));
    BOOST_CHECK_EQUAL(IsMine(store, script), ISMINE_SPENDABLE);
    BOOST_REQUIRE(store.AddCScript(script));
    BOOST_CHECK_EQUAL(IsMine(store, GetScriptForDestination(CScriptID(script))), ISMINE_NO);
    pos::BondData bond;
    bond.version = 2;
    unsigned char secret[32]{};
    secret[31] = 1;
    BOOST_REQUIRE(pos::GetSchnorrPublicKey(secret, bond.signing_public_key));
    bond.pq_signing_key_id = uint256S("01");
    bond.pq_reward_key_id = uint256S("02");
    bond.pq_withdrawal_key_id = id;
    const CScript bond_script = pos::GetBondScript(bond);
    BOOST_CHECK_EQUAL(bond_script.size(), 138U);
    pos::BondData parsed;
    BOOST_REQUIRE(pos::ParseBondScript(bond_script, parsed));
    BOOST_CHECK(parsed.pq_withdrawal_key_id == id);
    BOOST_REQUIRE(ExtractPQKeyId(bond_script, extracted));
    BOOST_CHECK(extracted == id);
    pos::UnbondData unbond;
    unbond.version = 2;
    unbond.unlock_height = 100;
    unbond.pq_withdrawal_key_id = id;
    BOOST_CHECK_EQUAL(pos::GetUnbondScript(unbond).size(), 45U);
    pos::UnbondData parsed_unbond;
    BOOST_REQUIRE(pos::ParseUnbondScript(pos::GetUnbondScript(unbond), parsed_unbond));
    BOOST_CHECK(parsed_unbond.pq_withdrawal_key_id == id);
    BOOST_CHECK_EQUAL(parsed_unbond.unlock_height, 100U);
}

BOOST_AUTO_TEST_CASE(pq_multisig_canonical_policy_addresses_and_bounds)
{
    PQMultisigPolicy policy{2, {uint256S("01"), uint256S("02"), uint256S("03")}};
    const auto encoded = EncodePQMultisigPolicy(policy);
    BOOST_REQUIRE_EQUAL(encoded.size(), 99U);
    PQMultisigPolicy parsed;
    BOOST_REQUIRE(DecodePQMultisigPolicy(encoded, parsed));
    BOOST_CHECK_EQUAL(parsed.required, 2U);
    BOOST_CHECK(parsed.keys == policy.keys);
    auto invalid = policy;
    invalid.required = 0;
    BOOST_CHECK(EncodePQMultisigPolicy(invalid).empty());
    invalid.required = 4;
    BOOST_CHECK(EncodePQMultisigPolicy(invalid).empty());
    invalid = policy;
    invalid.keys[1] = invalid.keys[0];
    BOOST_CHECK(EncodePQMultisigPolicy(invalid).empty());
    invalid = policy;
    std::swap(invalid.keys[0], invalid.keys[1]);
    BOOST_CHECK(EncodePQMultisigPolicy(invalid).empty());
    invalid = policy;
    invalid.keys[0].SetNull();
    BOOST_CHECK(EncodePQMultisigPolicy(invalid).empty());
    invalid = policy;
    invalid.keys.resize(6, uint256S("04"));
    BOOST_CHECK(EncodePQMultisigPolicy(invalid).empty());
    for (size_t size = 0; size < encoded.size(); ++size) {
        BOOST_CHECK(!DecodePQMultisigPolicy(std::vector<unsigned char>(encoded.begin(), encoded.begin() + size), parsed));
    }
    auto extra = encoded;
    extra.push_back(0);
    BOOST_CHECK(!DecodePQMultisigPolicy(extra, parsed));
    extra = encoded;
    extra[0] = 2;
    BOOST_CHECK(!DecodePQMultisigPolicy(extra, parsed));
    const auto id = GetPQMultisigPolicyId(policy);
    const CScript script = GetPQMultisigScript(id);
    uint256 extracted;
    BOOST_REQUIRE(ExtractPQMultisigId(script, extracted));
    BOOST_CHECK(extracted == id);
    BOOST_CHECK(!ExtractPQKeyId(script, extracted));
    BOOST_CHECK(IsPQOutputScript(script));
    txnouttype type;
    BOOST_CHECK(!IsStandard(script, type, false));
    BOOST_CHECK(IsStandard(script, type, true));
    BOOST_CHECK(type == TX_PQ_MULTISIG);
    CBasicKeyStore store;
    BOOST_CHECK_EQUAL(IsMine(store, script), ISMINE_NO);
    BOOST_REQUIRE(store.AddPQMultisigPolicy(encoded));
    BOOST_CHECK_EQUAL(IsMine(store, script), ISMINE_WATCH_ONLY);
    BOOST_CHECK(IsSolvable(store, script));
    const auto network = Params().NetworkIDString();
    struct RestoreNetwork { std::string network; ~RestoreNetwork() { SelectParams(network); } } restore{network};
    std::vector<std::string> addresses;
    const std::string networks[] = {CBaseChainParams::MAIN, CBaseChainParams::TESTNET, CBaseChainParams::REGTEST};
    for (const auto& name : networks) {
        SelectParams(name);
        const auto address = EncodeDestination(PQPolicyHash(id));
        BOOST_CHECK(address.find(Params().Bech32PQHRP() + "1") == 0);
        BOOST_CHECK(DecodeDestination(address) == CTxDestination(PQPolicyHash(id)));
        BOOST_CHECK(DecodeDestination(address) != CTxDestination(PQKeyHash(id)));
        addresses.push_back(address);
        const auto bech = bech32::Decode(address, bech32::Encoding::BECH32M);
        BOOST_CHECK(!bech.first.empty());
        auto payload = std::vector<unsigned char>(33, 0);
        payload[0] = 2;
        payload.back() = 1;
        std::vector<unsigned char> converted;
        ConvertBits<8, 5, true>([&](unsigned char c) { converted.push_back(c); }, payload.begin(), payload.end());
        BOOST_CHECK(!IsValidDestinationString(bech32::Encode(Params().Bech32PQHRP(), converted, bech32::Encoding::BECH32M)));
    }
    for (size_t i = 0; i < 3; ++i) {
        SelectParams(networks[i]);
        for (size_t j = 0; j < 3; ++j) BOOST_CHECK_EQUAL(IsValidDestinationString(addresses[j]), i == j);
    }
}

BOOST_AUTO_TEST_CASE(pq_multisig_threshold_signing_merging_and_replay)
{
    if (!HaveBackend()) return;
    std::array<pq::MLDSA44Key, 5> keys;
    std::array<std::vector<unsigned char>, 5> public_keys;
    PQMultisigPolicy policy;
    for (size_t i = 0; i < keys.size(); ++i) {
        BOOST_REQUIRE(keys[i].MakeNewKey());
        BOOST_REQUIRE(keys[i].GetPublicKey(public_keys[i]));
        policy.keys.push_back(pq::GetMLDSA44KeyId(public_keys[i]));
    }
    std::sort(policy.keys.begin(), policy.keys.end());
    for (const int activation : {0, 1500}) {
        const ReplayProtectionContext context(3, activation);
        const int hash_type = SIGHASH_ALL | (activation ? SIGHASH_POS_FORKID : 0);
        const unsigned int flags = STANDARD_SCRIPT_VERIFY_FLAGS | SCRIPT_VERIFY_PQ |
            (activation ? SCRIPT_VERIFY_POS_REPLAY_PROTECTION : 0);
        for (const unsigned int threshold : {1U, 2U, 5U}) {
            policy.required = threshold;
            const auto encoded = EncodePQMultisigPolicy(policy);
            const CScript script = GetPQMultisigScript(GetPQMultisigPolicyId(policy));
            CMutableTransaction tx;
            tx.nVersion = activation ? CTransaction::POS_REPLAY_PROTECTED_VERSION : 1;
            tx.vin.emplace_back(COutPoint(uint256S("1234"), 0));
            tx.vout.emplace_back(COIN, GetPQScript(policy.keys[0]));
            SignatureData merged;
            for (size_t i = 0; i < keys.size(); ++i) {
                CBasicKeyStore signer;
                BOOST_REQUIRE(signer.AddPQKey(keys[i], public_keys[i]));
                BOOST_REQUIRE(signer.AddPQMultisigPolicy(encoded));
                BOOST_CHECK_EQUAL(IsMine(signer, script), threshold == 1 ? ISMINE_SPENDABLE : ISMINE_WATCH_ONLY);
                SignatureData partial;
                BOOST_CHECK_EQUAL(ProduceSignature(signer, MutableTransactionSignatureCreator(
                    &tx, 0, 2 * COIN, hash_type, context), script, partial), threshold == 1);
                const auto checker = MutableTransactionSignatureChecker(&tx, 0, 2 * COIN, context);
                merged = CombineSignatures(script, checker, merged, partial);
                BOOST_CHECK_EQUAL(VerifyScript(merged.scriptSig, script, nullptr, flags, checker), i + 1 >= threshold);
                const auto repeated = CombineSignatures(script, checker, merged, partial);
                PQMultisigPolicy parsed;
                std::vector<PQMultisigSignature> signatures;
                BOOST_REQUIRE(DecodePQMultisigScriptSig(repeated.scriptSig, parsed, signatures));
                BOOST_CHECK_EQUAL(signatures.size(), std::min(i + 1, size_t(threshold)));
                if (i == 0 && threshold > 1) {
                    CBasicKeyStore cosigner;
                    BOOST_REQUIRE(cosigner.AddPQKey(keys[1], public_keys[1]));
                    auto carried = partial;
                    BOOST_CHECK_EQUAL(ProduceSignature(cosigner, MutableTransactionSignatureCreator(
                        &tx, 0, 2 * COIN, hash_type, context), script, carried), threshold == 2);
                    BOOST_REQUIRE(DecodePQMultisigScriptSig(carried.scriptSig, parsed, signatures));
                    BOOST_CHECK_EQUAL(signatures.size(), 2U);
                }
            }
            BOOST_CHECK_EQUAL(merged.scriptSig.size(), GetPQMultisigMaximumScriptSigSize(policy));
            BOOST_CHECK(merged.scriptSig.size() <= PQ_MULTISIG_MAX_SCRIPTSIG_BYTES);
            const auto verifies = [&](const CScript& proof, const CScriptWitness* witness = nullptr) {
                return VerifyScript(proof, script, witness, flags,
                    MutableTransactionSignatureChecker(&tx, 0, 2 * COIN, context));
            };
            BOOST_CHECK(verifies(merged.scriptSig));
            const CScript different_policy = GetPQMultisigScript(uint256S("abcd"));
            BOOST_CHECK(!VerifyScript(merged.scriptSig, different_policy, nullptr, flags,
                MutableTransactionSignatureChecker(&tx, 0, 2 * COIN, context)));
            BOOST_CHECK(!VerifyScript(merged.scriptSig, script, nullptr, flags,
                MutableTransactionSignatureChecker(&tx, 0, 2 * COIN + 1, context)));
            BOOST_CHECK(!VerifyScript(merged.scriptSig, script, nullptr, flags,
                MutableTransactionSignatureChecker(&tx, 0, 2 * COIN, ReplayProtectionContext(2, activation))));
            BOOST_CHECK(!VerifyScript(merged.scriptSig, script, nullptr, flags,
                MutableTransactionSignatureChecker(&tx, 0, 2 * COIN, ReplayProtectionContext(3, activation + 1))));
            tx.vout[0].nValue++;
            BOOST_CHECK(!verifies(merged.scriptSig));
            tx.vout[0].nValue--;
            CScript extra = merged.scriptSig;
            extra << OP_0;
            BOOST_CHECK(!verifies(extra));
            CScript oversized;
            oversized.assign(PQ_MULTISIG_MAX_SCRIPTSIG_BYTES + 1, OP_0);
            BOOST_CHECK(!verifies(oversized));
            auto nonminimal = merged.scriptSig;
            // All five-key policies have a 163-byte canonical PUSHDATA1 header.
            nonminimal.erase(nonminimal.begin(), nonminimal.begin() + 2);
            const unsigned char header[] = {OP_PUSHDATA2, 163, 0};
            nonminimal.insert(nonminimal.begin(), std::begin(header), std::end(header));
            BOOST_CHECK(!verifies(nonminimal));
            CScriptWitness witness;
            witness.stack.push_back({1});
            BOOST_CHECK(!verifies(merged.scriptSig, &witness));
            PQMultisigPolicy parsed;
            std::vector<PQMultisigSignature> signatures;
            BOOST_REQUIRE(DecodePQMultisigScriptSig(merged.scriptSig, parsed, signatures));
            auto damaged = signatures;
            damaged[0].signature[0] ^= 1;
            BOOST_CHECK(!verifies(GetPQMultisigScriptSig(policy, damaged)));
            damaged = signatures;
            damaged[0].public_key[0] ^= 1;
            BOOST_CHECK(!verifies(GetPQMultisigScriptSig(policy, damaged)));
            if (threshold > 1) {
                damaged = signatures;
                damaged[1] = damaged[0];
                BOOST_CHECK(GetPQMultisigScriptSig(policy, damaged).empty());
                damaged = signatures;
                std::swap(damaged[0].index, damaged[1].index);
                BOOST_CHECK(GetPQMultisigScriptSig(policy, damaged).empty());
                signatures.pop_back();
                BOOST_CHECK(!verifies(GetPQMultisigScriptSig(policy, signatures)));
            }
            CScript wrapped = GetScriptForDestination(CScriptID(script));
            CScript redeem = merged.scriptSig;
            redeem << std::vector<unsigned char>(script.begin(), script.end());
            BOOST_CHECK(!VerifyScript(redeem, wrapped, nullptr, flags,
                MutableTransactionSignatureChecker(&tx, 0, 2 * COIN, context)));
            tx.vin[0].scriptSig = merged.scriptSig;
            CCoinsView view;
            CCoinsViewCache coins(&view);
            coins.AddCoin(tx.vin[0].prevout, Coin(CTxOut(2 * COIN, script), 1, false, 0), false);
            BOOST_CHECK_EQUAL(GetTransactionSigOpCost(CTransaction(tx), coins, SCRIPT_VERIFY_PQ), PQ_SIGOP_COST * PQ_MULTISIG_MAX_KEYS);
            CDataStream serialized(SER_NETWORK, PROTOCOL_VERSION);
            serialized << tx;
            vergeconsensus_error error;
            BOOST_CHECK_EQUAL(vergeconsensus_verify_script_with_context(script.data(), script.size(), 2 * COIN,
                reinterpret_cast<const unsigned char*>(serialized.data()), serialized.size(), 0,
                SCRIPT_VERIFY_PQ | (activation ? SCRIPT_VERIFY_POS_REPLAY_PROTECTION : 0), 3, activation, &error), 1);
            BOOST_CHECK(error == vergeconsensus_ERR_OK);
        }
    }
}

BOOST_AUTO_TEST_CASE(spending_binds_network_amount_script_and_transaction)
{
    if (!HaveBackend()) return;
    pq::MLDSA44Key key;
    BOOST_REQUIRE(key.MakeNewKey());
    std::vector<unsigned char> public_key;
    BOOST_REQUIRE(key.GetPublicKey(public_key));
    CBasicKeyStore store;
    BOOST_REQUIRE(store.AddPQKey(key, public_key));
    const CScript script = GetPQScript(pq::GetMLDSA44KeyId(public_key));
    for (const int activation : {0, 1500}) {
        CMutableTransaction tx;
        tx.nVersion = activation ? CTransaction::POS_REPLAY_PROTECTED_VERSION : 1;
        tx.vin.emplace_back(COutPoint(uint256S("1234"), 1));
        tx.vout.emplace_back(COIN, script);
        const ReplayProtectionContext context(3, activation);
        SignatureData data;
        BOOST_REQUIRE(ProduceSignature(store, MutableTransactionSignatureCreator(
            &tx, 0, 2 * COIN, SIGHASH_ALL | (activation ? SIGHASH_POS_FORKID : 0), context), script, data));
        BOOST_CHECK_EQUAL(data.scriptSig.size(), 3757U);
        tx.vin[0].scriptSig = data.scriptSig;
        const unsigned int flags = STANDARD_SCRIPT_VERIFY_FLAGS | SCRIPT_VERIFY_PQ |
            (activation ? SCRIPT_VERIFY_POS_REPLAY_PROTECTION : 0);
        BOOST_CHECK(VerifyScript(data.scriptSig, script, nullptr, flags,
            MutableTransactionSignatureChecker(&tx, 0, 2 * COIN, context)));
        BOOST_CHECK(!VerifyScript(data.scriptSig, script, nullptr, flags,
            MutableTransactionSignatureChecker(&tx, 0, 2 * COIN + 1, context)));
        BOOST_CHECK(!VerifyScript(data.scriptSig, script, nullptr, flags,
            MutableTransactionSignatureChecker(&tx, 0, 2 * COIN, ReplayProtectionContext(2, activation))));
        BOOST_CHECK(!VerifyScript(data.scriptSig, script, nullptr, flags,
            MutableTransactionSignatureChecker(&tx, 0, 2 * COIN, ReplayProtectionContext(3, activation + 1))));
        tx.vout[0].nValue++;
        BOOST_CHECK(!VerifyScript(data.scriptSig, script, nullptr, flags,
            MutableTransactionSignatureChecker(&tx, 0, 2 * COIN, context)));
        tx.vout[0].nValue--;
        CScript extra = data.scriptSig;
        extra << OP_0;
        BOOST_CHECK(!VerifyScript(extra, script, nullptr, flags,
            MutableTransactionSignatureChecker(&tx, 0, 2 * COIN, context)));
        auto malformed = data.scriptSig;
        malformed[3] ^= 1;
        BOOST_CHECK(!VerifyScript(malformed, script, nullptr, flags,
            MutableTransactionSignatureChecker(&tx, 0, 2 * COIN, context)));
        CScript wrapped = GetScriptForDestination(CScriptID(script));
        CScript redeem = data.scriptSig;
        redeem << std::vector<unsigned char>(script.begin(), script.end());
        BOOST_CHECK(!VerifyScript(redeem, wrapped, nullptr, flags,
            MutableTransactionSignatureChecker(&tx, 0, 2 * COIN, context)));
        CScriptWitness witness;
        witness.stack.push_back({1});
        BOOST_CHECK(!VerifyScript(data.scriptSig, script, &witness, flags,
            MutableTransactionSignatureChecker(&tx, 0, 2 * COIN, context)));
        CDataStream encoded(SER_NETWORK, PROTOCOL_VERSION);
        encoded << tx;
        vergeconsensus_error error;
        BOOST_CHECK_EQUAL(vergeconsensus_verify_script_with_context(script.data(), script.size(),
            2 * COIN, reinterpret_cast<const unsigned char*>(encoded.data()), encoded.size(), 0,
            SCRIPT_VERIFY_PQ | (activation ? SCRIPT_VERIFY_POS_REPLAY_PROTECTION : 0), 3, activation, &error), 1);
        BOOST_CHECK(error == vergeconsensus_ERR_OK);
        BOOST_CHECK_EQUAL(vergeconsensus_verify_script_with_amount(script.data(), script.size(),
            2 * COIN, reinterpret_cast<const unsigned char*>(encoded.data()), encoded.size(), 0,
            SCRIPT_VERIFY_PQ, &error), 0);
        BOOST_CHECK(error == vergeconsensus_ERR_CONTEXT_REQUIRED);
        BOOST_CHECK_EQUAL(vergeconsensus_verify_script_with_amount(script.data(), script.size(),
            2 * COIN, reinterpret_cast<const unsigned char*>(encoded.data()), encoded.size(), 0,
            1U << 31, &error), 0);
        BOOST_CHECK(error == vergeconsensus_ERR_INVALID_FLAGS);
    }
}

BOOST_AUTO_TEST_CASE(hybrid_evidence_requires_registered_keys_and_both_signatures)
{
    if (!HaveBackend()) return;
    pq::MLDSA44Key key;
    BOOST_REQUIRE(key.MakeNewKey());
    std::vector<unsigned char> public_key;
    BOOST_REQUIRE(key.GetPublicKey(public_key));
    unsigned char secret[32]{}, auxiliary[32]{};
    secret[31] = 3;
    pos::BondData bond;
    bond.version = 2;
    BOOST_REQUIRE(pos::GetSchnorrPublicKey(secret, bond.signing_public_key));
    bond.pq_signing_key_id = pq::GetMLDSA44KeyId(public_key);
    bond.pq_reward_key_id = uint256S("02");
    bond.pq_withdrawal_key_id = uint256S("03");
    Consensus::Params params = Params().GetConsensus();
    params.nPoSBondRulesHeight = 1;
    CMutableTransaction tx;
    tx.vin.emplace_back(COutPoint(uint256S("01"), 0));
    tx.vout.emplace_back(params.nPoSMinStake, pos::GetBondScript(bond));
    const COutPoint outpoint(tx.GetHash(), 0);
    CBlock creation;
    creation.vtx.push_back(MakeTransactionRef(tx));
    pos::State state;
    pos::StateUndo undo;
    BOOST_REQUIRE(state.ApplyBlock(creation, 1, false, 0, 0, params, undo));
    const auto before = SerializeHash(state);
    const auto sign = [&](auto& statement, pq::HybridPurpose purpose, const uint256& hash) {
        pq::HybridPublicKey hybrid_key;
        pq::HybridSignature signature;
        BOOST_REQUIRE(pq::SignHybrid(secret, auxiliary, key, 3, purpose, hash, hybrid_key, signature));
        std::copy(signature.schnorr.begin(), signature.schnorr.end(), statement.signature);
        statement.pq_public_key = hybrid_key.mldsa;
        statement.pq_signature = signature.mldsa;
    };
    pos::BlockEquivocationEvidence blocks;
    blocks.version = blocks.first.version = 2;
    blocks.first.network_id = 3;
    blocks.first.bond_outpoint = outpoint;
    blocks.first.slot = 10;
    blocks.first.parent_block_root = uint256S("02");
    blocks.first.candidate_header_hash = uint256S("03");
    blocks.first.stake_proof_hash = uint256S("04");
    blocks.first.fee_reward_transaction_hash = uint256S("05");
    blocks.first.parent_randomness = uint256S("06");
    blocks.second = blocks.first;
    blocks.second.candidate_header_hash = uint256S("07");
    sign(blocks.first, pq::HybridPurpose::BLOCK, pos::GetBlockSigningHash(blocks.first));
    sign(blocks.second, pq::HybridPurpose::BLOCK, pos::GetBlockSigningHash(blocks.second));
    if (pos::GetBlockSigningHash(blocks.second) < pos::GetBlockSigningHash(blocks.first)) std::swap(blocks.first, blocks.second);
    BOOST_REQUIRE(pos::CheckBlockEvidence(blocks, state, 3, 10) == pos::ValidationError::NONE);
    blocks.first.pq_signature[0] ^= 1;
    BOOST_CHECK(pos::CheckBlockEvidence(blocks, state, 3, 10) == pos::ValidationError::EVIDENCE_SIGNATURE);
    blocks.first.pq_signature[0] ^= 1;
    blocks.second.signature[0] ^= 1;
    BOOST_CHECK(pos::CheckBlockEvidence(blocks, state, 3, 10) == pos::ValidationError::EVIDENCE_SIGNATURE);
    blocks.second.signature[0] ^= 1;
    BOOST_CHECK(pos::CheckBlockEvidence(blocks, state, 2, 10) == pos::ValidationError::EVIDENCE_SIGNATURE);
    pos::VoteEquivocationEvidence votes;
    votes.version = votes.first.version = 2;
    votes.first.bond_outpoint = outpoint;
    votes.first.source_epoch = pos::FINAL_POW_CHECKPOINT_EPOCH;
    votes.first.source_checkpoint_root = uint256S("02");
    votes.first.target_epoch = 0;
    votes.first.target_checkpoint_root = uint256S("03");
    votes.first.head_slot = 10;
    votes.first.head_block_root = uint256S("04");
    votes.second = votes.first;
    votes.second.target_checkpoint_root = uint256S("05");
    sign(votes.first, pq::HybridPurpose::VOTE, pos::GetVoteSigningHash(votes.first));
    sign(votes.second, pq::HybridPurpose::VOTE, pos::GetVoteSigningHash(votes.second));
    if (pos::GetVoteSigningHash(votes.second) < pos::GetVoteSigningHash(votes.first)) std::swap(votes.first, votes.second);
    BOOST_REQUIRE(pos::CheckVoteEvidence(votes, state, 0, 3) == pos::ValidationError::NONE);
    votes.first.pq_signature[0] ^= 1;
    BOOST_CHECK(pos::CheckVoteEvidence(votes, state, 0, 3) == pos::ValidationError::EVIDENCE_SIGNATURE);
    votes.first.pq_signature[0] ^= 1;
    votes.second.signature[0] ^= 1;
    BOOST_CHECK(pos::CheckVoteEvidence(votes, state, 0, 3) == pos::ValidationError::EVIDENCE_SIGNATURE);
    BOOST_CHECK(SerializeHash(state) == before);
}

BOOST_AUTO_TEST_CASE(canonical_chunk_bounds_and_hash_domains)
{
    BOOST_CHECK_EQUAL(bech32::Decode("A1LQFN3A", bech32::Encoding::BECH32M).first, "a");
    BOOST_CHECK(bech32::Decode("A1LQFN3A").first.empty());
    BOOST_CHECK_EQUAL(bech32::Encode("a", {}, bech32::Encoding::BECH32M), "a1lqfn3a");
    BOOST_CHECK_EQUAL(bech32::Decode("A12UEL5L").first, "a");
    BOOST_CHECK(bech32::Decode("A12UEL5L", bech32::Encoding::BECH32M).first.empty());
    std::vector<unsigned char> signature(2421, 1), public_key(1312, 2), decoded_sig, decoded_key;
    const CScript script = GetPQScriptSig(signature, public_key);
    BOOST_REQUIRE(DecodePQScriptSig(script, decoded_sig, decoded_key));
    BOOST_CHECK(signature == decoded_sig && public_key == decoded_key);
    auto cursor = script.begin();
    size_t chunks = 0;
    while (cursor != script.end()) {
        opcodetype opcode;
        std::vector<unsigned char> chunk;
        BOOST_REQUIRE(script.GetOp(cursor, opcode, chunk));
        BOOST_CHECK(chunk.size() <= 520);
        ++chunks;
    }
    BOOST_CHECK_EQUAL(chunks, 8U);
    CScript noncanonical;
    noncanonical.push_back(OP_PUSHDATA4);
    for (unsigned char byte : {8, 2, 0, 0}) noncanonical.push_back(byte);
    noncanonical.insert(noncanonical.end(), script.begin() + 3, script.end());
    BOOST_CHECK(!DecodePQScriptSig(noncanonical, decoded_sig, decoded_key));
    for (size_t size : {size_t{0}, size_t{1}, size_t{script.size() - 1}}) {
        BOOST_CHECK(!DecodePQScriptSig(CScript(script.begin(), script.begin() + size), decoded_sig, decoded_key));
    }
    uint256 hash, different;
    BOOST_REQUIRE(pq::GetPQTransactionHash(3, 1500, COIN, uint256S("abcd"), hash));
    BOOST_CHECK_EQUAL(hash.GetHex(), "42ac79d4c4bd346d3d341e63e97e9c497c50e852ae5b24b2a6becabcf63ef308");
    BOOST_REQUIRE(pq::GetPQTransactionHash(3, 1500, COIN + 1, uint256S("abcd"), different));
    BOOST_CHECK(hash != different);
    BOOST_CHECK(!pq::GetPQTransactionHash(0, 1500, COIN, uint256S("abcd"), hash));
    BOOST_CHECK(hash.IsNull());
    BOOST_CHECK(!pq::GetPQTransactionHash(3, -1, COIN, uint256S("abcd"), hash));
    BOOST_CHECK(!pq::GetPQTransactionHash(3, 1500, -1, uint256S("abcd"), hash));
}

BOOST_AUTO_TEST_SUITE_END()
#endif
