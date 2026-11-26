// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <smsg/smessage.h>
#include <test/setup_common.h>
#include <wallet/wallet.h>
#include <secp256k1.h>
#include <boost/test/unit_test.hpp>

#include <cstdio>
#include <limits>

namespace smsg { extern secp256k1_context* secp256k1_context_smsg; }
namespace {
struct PQMessageFixture : BasicTestingSetup {
    bool own_context = false;
    PQMessageFixture()
    {
        if (!smsg::secp256k1_context_smsg) {
            smsg::secp256k1_context_smsg = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
            own_context = true;
        }
    }
    ~PQMessageFixture()
    {
        if (own_context) {
            secp256k1_context_destroy(smsg::secp256k1_context_smsg);
            smsg::secp256k1_context_smsg = nullptr;
        }
    }
};

struct Identity {
    CKey classical;
    smsg::PQMessagingKey secret;
    smsg::PQMessagingPublicKey public_key;
    Identity()
    {
        classical.MakeNewKey(true);
        BOOST_REQUIRE(secret.MakeNewKey());
        BOOST_REQUIRE(secret.GetPublicKey(2, classical.GetPubKey(), public_key));
    }
};
}

BOOST_FIXTURE_TEST_SUITE(smsg_pq_tests, PQMessageFixture)

BOOST_AUTO_TEST_CASE(backend_unavailability_fails_closed)
{
    smsg::PQMessagingKey key;
    if (!smsg::PQMessagingAvailable()) {
        BOOST_CHECK(!key.MakeNewKey());
        BOOST_CHECK(!key.SetSeed(smsg::PQSecret(96, 1)));
        std::vector<unsigned char> ciphertext{1};
        smsg::PQSecret secret{1};
        BOOST_CHECK(!smsg::EncapsulateMLKEM768({}, ciphertext, secret));
        BOOST_CHECK(ciphertext.empty());
        BOOST_CHECK(secret.empty());
    } else BOOST_CHECK(key.MakeNewKey());
}

BOOST_AUTO_TEST_CASE(independent_keys_canonical_chatkeys_and_fingerprints)
{
    if (!smsg::PQMessagingAvailable()) return;
    Identity owner;
    const auto encoded = smsg::EncodePQChatKey(owner.public_key);
    BOOST_CHECK(encoded.size() <= smsg::PQ_CHATKEY_MAX_SIZE);
    smsg::PQMessagingPublicKey decoded;
    BOOST_REQUIRE(smsg::DecodePQChatKey(encoded, decoded));
    BOOST_CHECK(decoded.Encode() == owner.public_key.Encode());
    BOOST_CHECK(decoded.Fingerprint() == owner.public_key.Fingerprint());
    std::string corrupt = encoded;
    corrupt[150] = corrupt[150] == 'A' ? 'B' : 'A';
    BOOST_CHECK(!smsg::DecodePQChatKey(corrupt, decoded));
    BOOST_CHECK(!smsg::DecodePQChatKey(encoded + "=", decoded));
    BOOST_CHECK(!smsg::DecodePQChatKey(std::string(5000, 'A'), decoded));
    BOOST_CHECK(!smsg::PQMessagingPublicKey::Decode({}, decoded));
    auto truncated = owner.public_key.Encode();
    truncated.pop_back();
    BOOST_CHECK(!smsg::PQMessagingPublicKey::Decode(truncated, decoded));
    smsg::PQSecret seed;
    BOOST_REQUIRE(owner.secret.GetSeed(seed));
    BOOST_CHECK_EQUAL(seed.size(), smsg::PQ_MESSAGING_SEED_SIZE);
    smsg::PQMessagingKey restored;
    BOOST_REQUIRE(restored.SetSeed(seed));
    BOOST_REQUIRE(restored.GetPublicKey(2, owner.classical.GetPubKey(), decoded));
    BOOST_CHECK(decoded.Encode() == owner.public_key.Encode());
    decoded.network = 3;
    BOOST_CHECK(decoded.Fingerprint() != owner.public_key.Fingerprint());
    BOOST_CHECK(!restored.SetSeed(smsg::PQSecret(95, 0)));
    BOOST_CHECK(!restored.GetSeed(seed));
}

BOOST_AUTO_TEST_CASE(kem_encapsulation_wrong_keys_and_implicit_rejection)
{
    if (!smsg::PQMessagingAvailable()) return;
    Identity recipient, other;
    std::vector<unsigned char> ciphertext;
    smsg::PQSecret sent, received;
    BOOST_REQUIRE(smsg::EncapsulateMLKEM768(recipient.public_key.kem, ciphertext, sent));
    BOOST_CHECK_EQUAL(ciphertext.size(), smsg::MLKEM768_CIPHERTEXT_SIZE);
    BOOST_CHECK_EQUAL(sent.size(), 32U);
    BOOST_REQUIRE(recipient.secret.Decapsulate(ciphertext, received));
    BOOST_CHECK(sent == received);
    BOOST_REQUIRE(other.secret.Decapsulate(ciphertext, received));
    BOOST_CHECK(sent != received);
    ciphertext[0] ^= 1;
    BOOST_REQUIRE(recipient.secret.Decapsulate(ciphertext, received));
    BOOST_CHECK(sent != received);
    ciphertext.pop_back();
    BOOST_CHECK(!recipient.secret.Decapsulate(ciphertext, received));
    BOOST_CHECK(received.empty());
}

BOOST_AUTO_TEST_CASE(hybrid_authenticated_message_round_trip_and_tampering)
{
    if (!smsg::PQMessagingAvailable()) return;
    Identity sender, recipient, other;
    smsg::SecureMessage message(true, 7);
    const std::string text = "PQ message <img src='https://invalid/'>\nwith a second line";
    BOOST_REQUIRE(smsg::EncryptPQEnvelope(message, recipient.public_key, &sender.secret, &sender.public_key, text));
    BOOST_CHECK(smsg::IsPQSmsg(message));
    smsg::MessageData decoded;
    smsg::PQMessagingPublicKey from;
    auto decrypt = [&] {
        return smsg::DecryptPQEnvelope(message, message.pPayload, message.nPayload,
            recipient.classical, recipient.secret, recipient.public_key, decoded, from);
    };
    BOOST_REQUIRE(decrypt());
    BOOST_CHECK(decoded.post_quantum);
    BOOST_CHECK_EQUAL(decoded.sender_fingerprint, sender.public_key.Fingerprint().GetHex());
    BOOST_CHECK_EQUAL(std::string(decoded.vchMessage.begin(), decoded.vchMessage.end() - 1), text);
    BOOST_CHECK(from.Encode() == sender.public_key.Encode());
    for (const size_t offset : {size_t(0), size_t(1087), size_t(1088), size_t(message.nPayload - 33)}) {
        message.pPayload[offset] ^= 1;
        BOOST_CHECK(!decrypt());
        BOOST_CHECK(decoded.vchMessage.empty());
        message.pPayload[offset] ^= 1;
    }
    for (unsigned char* byte : {&message.version[1], &message.flags, &message.iv[0], &message.iv[15],
            &message.cpkR[3], &message.mac[0], &message.mac[16], &message.nonce[0], &message.nonce[3]}) {
        *byte ^= 1;
        BOOST_CHECK(!decrypt());
        *byte ^= 1;
    }
    ++message.timestamp;
    BOOST_CHECK(!decrypt());
    --message.timestamp;
    BOOST_CHECK(!smsg::DecryptPQEnvelope(message, message.pPayload, message.nPayload,
        recipient.classical, other.secret, recipient.public_key, decoded, from));
    auto wrong_network = recipient.public_key;
    wrong_network.network = 3;
    BOOST_CHECK(!smsg::DecryptPQEnvelope(message, message.pPayload, message.nPayload,
        recipient.classical, recipient.secret, wrong_network, decoded, from));
    BOOST_REQUIRE(decrypt());
}

BOOST_AUTO_TEST_CASE(anonymous_bounded_payloads_and_no_signature_fallback)
{
    if (!smsg::PQMessagingAvailable()) return;
    Identity sender, recipient, other;
    smsg::SecureMessage message(true, 31);
    const std::string text(smsg::SMSG_MAX_MSG_BYTES_PAID, 'x');
    BOOST_REQUIRE(smsg::EncryptPQEnvelope(message, recipient.public_key, nullptr, nullptr, text));
    BOOST_CHECK(message.nPayload <= smsg::SMSG_MAX_PQ_PAYLOAD);
    smsg::MessageData decoded;
    smsg::PQMessagingPublicKey from;
    BOOST_REQUIRE(smsg::DecryptPQEnvelope(message, message.pPayload, message.nPayload,
        recipient.classical, recipient.secret, recipient.public_key, decoded, from));
    BOOST_CHECK_EQUAL(decoded.sFromAddress, "anon");
    BOOST_CHECK_EQUAL(decoded.vchMessage.size(), text.size() + 1);
    smsg::SecureMessage too_long(true, 31), wrong_signer(true, 31), signed_max(true, 31);
    BOOST_CHECK(!smsg::EncryptPQEnvelope(too_long, recipient.public_key, nullptr, nullptr, text + "x"));
    BOOST_REQUIRE(smsg::EncryptPQEnvelope(wrong_signer, recipient.public_key, &other.secret, &sender.public_key, "forged identity"));
    BOOST_CHECK(!smsg::DecryptPQEnvelope(wrong_signer, wrong_signer.pPayload, wrong_signer.nPayload,
        recipient.classical, recipient.secret, recipient.public_key, decoded, from));
    BOOST_REQUIRE(smsg::EncryptPQEnvelope(signed_max, recipient.public_key, &sender.secret, &sender.public_key, text));
    BOOST_CHECK_EQUAL(signed_max.nPayload, smsg::SMSG_MAX_PQ_PAYLOAD);
    BOOST_REQUIRE(smsg::DecryptPQEnvelope(signed_max, signed_max.pPayload, signed_max.nPayload,
        recipient.classical, recipient.secret, recipient.public_key, decoded, from));
}

BOOST_AUTO_TEST_CASE(stored_payload_size_is_checked_before_allocation)
{
    SetDataDir("smsg-pq-payload-bounds");
    ClearDatadirCache();
    const auto directory = GetDataDir() / "smsgstore";
    fs::create_directories(directory);
    smsg::CSMSG module;
    smsg::SecureMessage message(true, 1);
    message.timestamp = 1790000000;
    const int64_t bucket = message.timestamp - message.timestamp % smsg::SMSG_BUCKET_LEN;
    const auto path = directory / (std::to_string(bucket) + "_01.dat");
    const smsg::SecMsgToken token(message.timestamp, nullptr, 0, 0, 1);
    const std::vector<unsigned char> payload(smsg::SMSG_MAX_RELAY_PAYLOAD, 0x42);
    for (const uint32_t size : {0U, smsg::SMSG_MAX_RELAY_PAYLOAD + 1U,
            std::numeric_limits<uint32_t>::max(), smsg::SMSG_MAX_RELAY_PAYLOAD}) {
        message.nPayload = size;
        {
            std::unique_ptr<FILE, decltype(&fclose)> file(fsbridge::fopen(path, "wb"), fclose);
            BOOST_REQUIRE(file);
            BOOST_REQUIRE_EQUAL(fwrite(message.data(), 1, smsg::SMSG_HDR_LEN, file.get()), smsg::SMSG_HDR_LEN);
            if (size == payload.size()) BOOST_REQUIRE_EQUAL(fwrite(payload.data(), 1, size, file.get()), size);
        }
        std::vector<uint8_t> output(3, 0xff);
        LOCK(module.cs_smsg);
        const int result = module.Retrieve(token, output);
        if (size == payload.size()) {
            BOOST_REQUIRE_EQUAL(result, smsg::SMSG_NO_ERROR);
            BOOST_CHECK_EQUAL(output.size(), smsg::SMSG_HDR_LEN + size);
        } else {
            BOOST_CHECK_EQUAL(result, smsg::SMSG_PAYLOAD_OVER_SIZE);
            BOOST_CHECK(output.empty());
        }
    }
}

BOOST_FIXTURE_TEST_CASE(wallet_keys_contacts_encryption_restart_backup_and_no_replacement, TestingSetup)
{
    if (!smsg::PQMessagingAvailable()) return;
    const auto path = GetDataDir() / "smsg-pq-wallet.dat";
    const auto backup = GetDataDir() / "smsg-pq-backup.dat";
    const SecureString passphrase{"pq-messaging-wallet-test"};
    CKey classical, contact_classical;
    classical.MakeNewKey(true);
    contact_classical.MakeNewKey(true);
    const auto address = classical.GetPubKey().GetID();
    smsg::PQMessagingKey contact_secret, replacement;
    smsg::PQMessagingPublicKey contact, conflict, original;
    BOOST_REQUIRE(contact_secret.MakeNewKey());
    BOOST_REQUIRE(replacement.MakeNewKey());
    BOOST_REQUIRE(contact_secret.GetPublicKey(Params().GetConsensus().nPoSNetworkId, contact_classical.GetPubKey(), contact));
    BOOST_REQUIRE(replacement.GetPublicKey(contact.network, contact_classical.GetPubKey(), conflict));
    for (int reopen = 0; reopen < 3; ++reopen) {
        CWallet wallet("smsg-pq-test", WalletDatabase::Create(path));
        bool first = false;
        BOOST_REQUIRE(wallet.LoadWallet(first) == DBErrors::LOAD_OK);
        LOCK(wallet.cs_wallet);
        if (!reopen) {
            BOOST_REQUIRE(wallet.AddKeyPubKey(classical, classical.GetPubKey()));
            BOOST_REQUIRE(wallet.GenerateSmsgPQKey(address));
            BOOST_REQUIRE(wallet.GetSmsgPQPublicKey(address, original));
            BOOST_REQUIRE(wallet.AddSmsgPQContact(contact));
        } else BOOST_REQUIRE(wallet.Unlock(passphrase));
        smsg::PQMessagingKey loaded;
        smsg::PQMessagingPublicKey public_key;
        BOOST_REQUIRE(wallet.GetSmsgPQKey(address, loaded));
        BOOST_REQUIRE(loaded.GetPublicKey(original.network, original.classical, public_key));
        BOOST_CHECK(public_key.Encode() == original.Encode());
        BOOST_REQUIRE(wallet.GetSmsgPQContact(contact.classical.GetID(), public_key));
        BOOST_CHECK(public_key.Encode() == contact.Encode());
        BOOST_CHECK(!wallet.AddSmsgPQContact(conflict));
        BOOST_CHECK_EQUAL(wallet.GetVersion(), FEATURE_SMSG_PQ_KEYS);
        if (!reopen) {
            BOOST_REQUIRE(wallet.EncryptWallet(passphrase));
            BOOST_CHECK(wallet.IsLocked());
            BOOST_CHECK(!wallet.GetSmsgPQKey(address, loaded));
            BOOST_CHECK(!wallet.GenerateSmsgPQKey(contact.classical.GetID()));
        }
        if (reopen == 2) {
            BOOST_REQUIRE(wallet.BackupWallet(backup.string()));
            wallet.GetDBHandle().Flush(true);
        }
    }
    {
        CWallet restored("smsg-pq-backup", WalletDatabase::Create(backup));
        bool first = true;
        BOOST_REQUIRE(restored.LoadWallet(first) == DBErrors::LOAD_OK);
        BOOST_REQUIRE(restored.Unlock(passphrase));
        smsg::PQMessagingKey secret;
        BOOST_REQUIRE(restored.GetSmsgPQKey(address, secret));
        smsg::PQMessagingPublicKey public_key;
        BOOST_REQUIRE(restored.GetSmsgPQContact(contact.classical.GetID(), public_key));
        BOOST_CHECK(public_key.Encode() == contact.Encode());
        restored.GetDBHandle().Flush(true);
    }
    ReleaseDirectoryLocks();
}

BOOST_AUTO_TEST_SUITE_END()
