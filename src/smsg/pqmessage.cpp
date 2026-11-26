// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <smsg/smessage.h>

#include <base58.h>
#include <chainparams.h>
#include <crypto/common.h>
#include <crypto/hmac_sha256.h>
#include <hash.h>
#include <random.h>
#include <streams.h>
#include <util/time.h>
#ifdef ENABLE_WALLET
#include <wallet/wallet.h>
#endif

#include <openssl/evp.h>
#include <secp256k1.h>
#include <secp256k1_ecdh.h>
#include <algorithm>
#include <cstring>
#include <memory>

namespace smsg {
extern secp256k1_context* secp256k1_context_smsg;
namespace {
std::vector<unsigned char> AssociatedData(const SecureMessage& message,
        const PQMessagingPublicKey& recipient, const unsigned char* kem_ciphertext)
{
    CDataStream data(SER_NETWORK, 0);
    data << std::string("Verge/SMSG/Envelope/v1") << recipient.network << recipient.Fingerprint();
    data.write(reinterpret_cast<const char*>(message.data() + 4), 2 + 1 + 8 + 16 + 33);
    data.write(reinterpret_cast<const char*>(message.nonce), 4);
    data << message.nPayload;
    data.write(reinterpret_cast<const char*>(kem_ciphertext), MLKEM768_CIPHERTEXT_SIZE);
    return {data.begin(), data.end()};
}

bool ECDH(const CKey& secret, const CPubKey& public_key, PQSecret& shared)
{
    shared.clear();
    secp256k1_pubkey parsed;
    if (!secp256k1_context_smsg || !secret.IsValid() || !public_key.IsCompressed() ||
        !secp256k1_ec_pubkey_parse(secp256k1_context_smsg, &parsed, public_key.begin(), public_key.size())) return false;
    PQSecret result(32);
    if (!secp256k1_ecdh(secp256k1_context_smsg, result.data(), &parsed, secret.begin(), nullptr, nullptr)) return false;
    shared = std::move(result);
    return true;
}

PQSecret DeriveHybridKey(const PQSecret& classical, const PQSecret& pq,
                        const std::vector<unsigned char>& aad)
{
    if (classical.size() != 32 || pq.size() != 32) return {};
    const auto salt = Hash(aad.begin(), aad.end());
    PQSecret extract(32), key(32);
    CHMAC_SHA256(salt.begin(), salt.size()).Write(classical.data(), classical.size())
        .Write(pq.data(), pq.size()).Finalize(extract.data());
    const std::string label = "Verge/SMSG/HybridKey/v1";
    const unsigned char counter = 1;
    CHMAC_SHA256(extract.data(), extract.size())
        .Write(reinterpret_cast<const unsigned char*>(label.data()), label.size()).Write(&counter, 1).Finalize(key.data());
    return key;
}

PQSecret RecipientFilter(const PQSecret& classical, const std::vector<unsigned char>& aad,
                        const unsigned char* ciphertext, size_t size, const unsigned char* tag)
{
    PQSecret filter(32);
    const std::string label = "Verge/SMSG/RecipientFilter/v1";
    CHMAC_SHA256(classical.data(), classical.size())
        .Write(reinterpret_cast<const unsigned char*>(label.data()), label.size())
        .Write(aad.data(), aad.size()).Write(ciphertext, size).Write(tag, 16).Finalize(filter.data());
    return filter;
}

uint256 SignatureHash(const std::vector<unsigned char>& aad, const PQSecret& plaintext)
{
    CHashWriter hash(SER_GETHASH, 0);
    hash << std::string("Verge/SMSG/Message/v1") << aad;
    hash.write(reinterpret_cast<const char*>(plaintext.data()), plaintext.size());
    return hash.GetHash();
}

bool EncryptGCM(const PQSecret& key, const unsigned char* iv,
                const std::vector<unsigned char>& aad, const PQSecret& plaintext,
                std::vector<unsigned char>& ciphertext, unsigned char* tag)
{
    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!ctx || key.size() != 32 || EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, key.data(), iv) != 1) return false;
    int size = 0, tail = 0;
    ciphertext.resize(plaintext.size() + 16);
    if (EVP_EncryptUpdate(ctx.get(), nullptr, &size, aad.data(), aad.size()) != 1 ||
        EVP_EncryptUpdate(ctx.get(), ciphertext.data(), &size, plaintext.data(), plaintext.size()) != 1 ||
        EVP_EncryptFinal_ex(ctx.get(), ciphertext.data() + size, &tail) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, 16, tag) != 1) return false;
    ciphertext.resize(size + tail);
    return ciphertext.size() == plaintext.size();
}

bool DecryptGCM(const PQSecret& key, const unsigned char* iv,
                const std::vector<unsigned char>& aad, const unsigned char* ciphertext,
                size_t ciphertext_size, const unsigned char* tag, PQSecret& plaintext)
{
    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!ctx || key.size() != 32 || EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, key.data(), iv) != 1) return false;
    int size = 0, tail = 0;
    PQSecret result(ciphertext_size + 16);
    if (EVP_DecryptUpdate(ctx.get(), nullptr, &size, aad.data(), aad.size()) != 1 ||
        EVP_DecryptUpdate(ctx.get(), result.data(), &size, ciphertext, ciphertext_size) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, 16, const_cast<unsigned char*>(tag)) != 1 ||
        EVP_DecryptFinal_ex(ctx.get(), result.data() + size, &tail) != 1) return false;
    result.resize(size + tail);
    plaintext = std::move(result);
    return true;
}
} // namespace

bool IsPQSmsg(const SecureMessage& message)
{
    return message.version[0] == 3 && message.version[1] == 1;
}

bool EncryptPQEnvelope(SecureMessage& message, const PQMessagingPublicKey& recipient,
                       const PQMessagingKey* sender_key, const PQMessagingPublicKey* sender,
                       const std::string& plaintext)
{
    if (!recipient.IsValid() || plaintext.size() > SMSG_MAX_MSG_BYTES_PAID ||
        (sender != nullptr) != (sender_key != nullptr) || message.pPayload ||
        (sender && (!sender->IsValid() || sender->network != recipient.network)) ||
        message.nonce[0] < 1 || message.nonce[0] > 31 || !PQMessagingAvailable()) return false;
    message.version[0] = 3;
    message.version[1] = 1;
    message.flags = 0;
    std::fill(message.nonce + 1, message.nonce + 4, 0);
    if (!message.timestamp) message.timestamp = GetTime();
    GetStrongRandBytes(message.iv, sizeof(message.iv));
    CKey ephemeral;
    ephemeral.MakeNewKey(true);
    const auto epk = ephemeral.GetPubKey();
    std::copy(epk.begin(), epk.end(), message.cpkR);
    PQSecret classical, pq;
    std::vector<unsigned char> kem_ciphertext;
    if (!ECDH(ephemeral, recipient.classical, classical) ||
        !EncapsulateMLKEM768(recipient.kem, kem_ciphertext, pq)) return false;
    PQSecret body{static_cast<unsigned char>(sender ? 1 : 0)};
    if (sender) {
        const auto encoded = sender->Encode();
        body.insert(body.end(), encoded.begin(), encoded.end());
    }
    const size_t offset = body.size();
    body.resize(offset + 4);
    WriteLE32(body.data() + offset, plaintext.size());
    body.insert(body.end(), plaintext.begin(), plaintext.end());
    message.nPayload = MLKEM768_CIPHERTEXT_SIZE + body.size() + (sender ? pos::MLDSA44_SIGNATURE_SIZE : 0) + 32;
    const auto aad = AssociatedData(message, recipient, kem_ciphertext.data());
    if (sender) {
        std::vector<unsigned char> signature;
        if (!sender_key->Sign(SignatureHash(aad, body), signature) ||
            signature.size() != pos::MLDSA44_SIGNATURE_SIZE) return false;
        body.insert(body.end(), signature.begin(), signature.end());
    }
    const auto key = DeriveHybridKey(classical, pq, aad);
    std::vector<unsigned char> ciphertext;
    if (!EncryptGCM(key, message.iv, aad, body, ciphertext, message.mac + 16)) return false;
    const auto filter = RecipientFilter(classical, aad, ciphertext.data(), ciphertext.size(), message.mac + 16);
    std::copy(filter.begin(), filter.begin() + 16, message.mac);
    std::unique_ptr<unsigned char[]> payload(new unsigned char[message.nPayload]());
    std::copy(kem_ciphertext.begin(), kem_ciphertext.end(), payload.get());
    std::copy(ciphertext.begin(), ciphertext.end(), payload.get() + kem_ciphertext.size());
    message.pPayload = payload.release();
    return true;
}

bool DecryptPQEnvelope(const SecureMessage& message, const uint8_t* payload, uint32_t size,
                       const CKey& classical_key, const PQMessagingKey& recipient_key,
                       const PQMessagingPublicKey& recipient, MessageData& plaintext,
                       PQMessagingPublicKey& sender)
{
    plaintext = {};
    sender = {};
    if (!IsPQSmsg(message) || !payload || size != message.nPayload || size > SMSG_MAX_PQ_PAYLOAD ||
        size < MLKEM768_CIPHERTEXT_SIZE + 5 + 32 || message.flags ||
        message.nonce[0] < 1 || message.nonce[0] > 31 || message.nonce[1] || message.nonce[2] || message.nonce[3] ||
        !recipient.IsValid() || !classical_key.IsValid() || classical_key.GetPubKey() != recipient.classical) return false;
    PQSecret classical, pq;
    const CPubKey ephemeral(message.cpkR, message.cpkR + 33);
    if (!ECDH(classical_key, ephemeral, classical)) return false;
    const size_t ciphertext_size = size - MLKEM768_CIPHERTEXT_SIZE - 32;
    const auto ciphertext = payload + MLKEM768_CIPHERTEXT_SIZE;
    const auto aad = AssociatedData(message, recipient, payload);
    const auto filter = RecipientFilter(classical, aad, ciphertext, ciphertext_size, message.mac + 16);
    if (!TimingResistantEqual(std::vector<unsigned char>(filter.begin(), filter.begin() + 16),
            std::vector<unsigned char>(message.mac, message.mac + 16))) return false;
    const std::vector<unsigned char> kem_ciphertext(payload, payload + MLKEM768_CIPHERTEXT_SIZE);
    if (!recipient_key.Decapsulate(kem_ciphertext, pq)) return false;
    const auto key = DeriveHybridKey(classical, pq, aad);
    PQSecret body;
    if (!DecryptGCM(key, message.iv, aad, ciphertext, ciphertext_size, message.mac + 16, body) ||
        body.empty() || body[0] > 1) return false;
    size_t offset = 1;
    const bool identified = body[0] == 1;
    if (identified) {
        if (body.size() < 1 + PQ_MESSAGING_PUBLIC_SIZE + 4 + pos::MLDSA44_SIGNATURE_SIZE) return false;
        std::vector<unsigned char> encoded(body.begin() + 1, body.begin() + 1 + PQ_MESSAGING_PUBLIC_SIZE);
        if (!PQMessagingPublicKey::Decode(encoded, sender) || sender.network != recipient.network) return false;
        offset += PQ_MESSAGING_PUBLIC_SIZE;
    }
    if (body.size() < offset + 4) return false;
    const size_t length = ReadLE32(body.data() + offset);
    offset += 4;
    const size_t sig_size = identified ? pos::MLDSA44_SIGNATURE_SIZE : 0;
    if (length > SMSG_MAX_MSG_BYTES_PAID || body.size() != offset + length + sig_size) return false;
    if (identified) {
        const std::vector<unsigned char> signature(body.end() - sig_size, body.end());
        PQSecret unsigned_body(body.begin(), body.end() - sig_size);
        if (!VerifyPQMessageSignature(sender, SignatureHash(aad, unsigned_body), signature)) return false;
    }
    MessageData result;
    result.timestamp = message.timestamp;
    result.sToAddress = CBitcoinAddress(recipient.classical.GetID()).ToString();
    result.sFromAddress = identified ? CBitcoinAddress(sender.classical.GetID()).ToString() : "anon";
    result.vchMessage.assign(body.begin() + offset, body.begin() + offset + length);
    result.vchMessage.push_back(0);
    result.post_quantum = true;
    if (identified) result.sender_fingerprint = sender.Fingerprint().GetHex();
    plaintext = std::move(result);
    return true;
}

int CSMSG::GetPQPublicKey(const CKeyID& address, PQMessagingPublicKey& key, bool create)
{
    key = {};
#ifdef ENABLE_WALLET
    if (!pwallet) return SMSG_WALLET_UNSET;
    if (pwallet->GetSmsgPQPublicKey(address, key)) return SMSG_NO_ERROR;
    if (create && pwallet->HaveKey(address)) {
        if (pwallet->IsLocked()) return SMSG_WALLET_LOCKED;
        if (!pwallet->GenerateSmsgPQKey(address)) return SMSG_GENERAL_ERROR;
        return pwallet->GetSmsgPQPublicKey(address, key) ? SMSG_NO_ERROR : SMSG_GENERAL_ERROR;
    }
    if (pwallet->GetSmsgPQContact(address, key)) return SMSG_NO_ERROR;
#endif
    return SMSG_PUBKEY_NOT_EXISTS;
}

int CSMSG::EncryptPQ(SecureMessage& message, const CKeyID& from, const CKeyID& to, const std::string& plaintext)
{
#ifdef ENABLE_WALLET
    if (!pwallet || pwallet->IsLocked()) return SMSG_WALLET_LOCKED;
    PQMessagingPublicKey recipient, sender;
    PQMessagingKey sender_key;
    if (GetPQPublicKey(to, recipient, false) != SMSG_NO_ERROR) return SMSG_PUBKEY_NOT_EXISTS;
    if (from.IsNull()) return EncryptPQEnvelope(message, recipient, nullptr, nullptr, plaintext) ? SMSG_NO_ERROR : SMSG_ENCRYPT_FAILED;
    if (GetPQPublicKey(from, sender, true) != SMSG_NO_ERROR || !pwallet->GetSmsgPQKey(from, sender_key)) return SMSG_UNKNOWN_KEY_FROM;
    return EncryptPQEnvelope(message, recipient, &sender_key, &sender, plaintext) ? SMSG_NO_ERROR : SMSG_ENCRYPT_FAILED;
#else
    return SMSG_WALLET_UNSET;
#endif
}

int CSMSG::DecryptPQ(const CKey& classical, const CKeyID& address, const SecureMessage& message,
                     const uint8_t* payload, uint32_t size, MessageData& plaintext)
{
#ifdef ENABLE_WALLET
    if (!pwallet || pwallet->IsLocked()) return SMSG_WALLET_LOCKED;
    PQMessagingPublicKey recipient, sender;
    PQMessagingKey key;
    if (!pwallet->GetSmsgPQPublicKey(address, recipient) || !pwallet->GetSmsgPQKey(address, key)) return SMSG_UNKNOWN_KEY;
    if (!DecryptPQEnvelope(message, payload, size, classical, key, recipient, plaintext, sender)) return SMSG_MAC_MISMATCH;
    if (sender.IsValid() && !pwallet->AddSmsgPQContact(sender)) {
        plaintext = {};
        return SMSG_PUBKEY_MISMATCH;
    }
    return SMSG_NO_ERROR;
#else
    return SMSG_WALLET_UNSET;
#endif
}
} // namespace smsg
