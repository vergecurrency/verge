// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <smsg/pqcrypto.h>

#include <crypto/common.h>
#include <hash.h>
#include <random.h>
#include <support/cleanse.h>
#include <util/strencodings.h>

#include <openssl/evp.h>
#include <openssl/opensslv.h>
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_ML_KEM) && !defined(LIBRESSL_VERSION_NUMBER)
#define VERGE_HAVE_SMSG_MLKEM 1
#include <openssl/core_names.h>
#include <openssl/params.h>
#endif

#include <algorithm>
#include <memory>

namespace smsg {
namespace {
const std::vector<unsigned char>& MessageContext()
{
    static const std::string context = "Verge/SMSG/MLDSA44/v1";
    static const std::vector<unsigned char> bytes(context.begin(), context.end());
    return bytes;
}

#ifdef VERGE_HAVE_SMSG_MLKEM
using KeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using ContextPtr = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;

KeyPtr ImportKEM(const unsigned char* bytes, size_t size, bool secret)
{
    ContextPtr ctx(EVP_PKEY_CTX_new_from_name(nullptr, "ML-KEM-768", nullptr), EVP_PKEY_CTX_free);
    if (!ctx || EVP_PKEY_fromdata_init(ctx.get()) != 1) return {nullptr, EVP_PKEY_free};
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_octet_string(secret ? OSSL_PKEY_PARAM_ML_KEM_SEED : OSSL_PKEY_PARAM_PUB_KEY,
                                         const_cast<unsigned char*>(bytes), size),
        OSSL_PARAM_construct_end()
    };
    EVP_PKEY* key = nullptr;
    const int result = EVP_PKEY_fromdata(ctx.get(), &key, secret ? EVP_PKEY_KEYPAIR : EVP_PKEY_PUBLIC_KEY, params);
    KeyPtr out(key, EVP_PKEY_free);
    if (result != 1) out.reset();
    return out;
}
#endif
} // namespace

bool PQMessagingPublicKey::IsValid() const
{
    return network != 0 && classical.IsCompressed() && classical.IsFullyValid() &&
        kem.size() == MLKEM768_PUBLIC_KEY_SIZE && signing.size() == pos::MLDSA44_PUBLIC_KEY_SIZE;
}

std::vector<unsigned char> PQMessagingPublicKey::Encode() const
{
    if (!IsValid()) return {};
    std::vector<unsigned char> out{1, 0, 0, 0, 0};
    WriteLE32(out.data() + 1, network);
    out.insert(out.end(), classical.begin(), classical.end());
    out.insert(out.end(), kem.begin(), kem.end());
    out.insert(out.end(), signing.begin(), signing.end());
    return out;
}

bool PQMessagingPublicKey::Decode(const std::vector<unsigned char>& encoded, PQMessagingPublicKey& out)
{
    out = {};
    if (encoded.size() != PQ_MESSAGING_PUBLIC_SIZE || encoded[0] != 1) return false;
    PQMessagingPublicKey key;
    key.network = ReadLE32(encoded.data() + 1);
    key.classical = CPubKey(encoded.begin() + 5, encoded.begin() + 38);
    key.kem.assign(encoded.begin() + 38, encoded.begin() + 38 + MLKEM768_PUBLIC_KEY_SIZE);
    key.signing.assign(encoded.begin() + 38 + MLKEM768_PUBLIC_KEY_SIZE, encoded.end());
    if (!key.IsValid()) return false;
    out = std::move(key);
    return true;
}

uint256 PQMessagingPublicKey::Fingerprint() const
{
    if (!IsValid()) return {};
    return (CHashWriter(SER_GETHASH, 0) << std::string("Verge/SMSG/Identity/v1") << Encode()).GetHash();
}

bool PQMessagingAvailable()
{
#ifdef VERGE_HAVE_SMSG_MLKEM
    ContextPtr context(EVP_PKEY_CTX_new_from_name(nullptr, "ML-KEM-768", nullptr), EVP_PKEY_CTX_free);
    return context && pos::experimental::MLDSA44Available();
#else
    return false;
#endif
}

std::string EncodePQChatKey(const PQMessagingPublicKey& key)
{
    auto bytes = key.Encode();
    if (bytes.empty()) return {};
    const auto checksum = Hash(bytes.begin(), bytes.end());
    bytes.insert(bytes.end(), checksum.begin(), checksum.begin() + 4);
    std::string encoded = EncodeBase32(bytes.data(), bytes.size());
    for (char& c : encoded) if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    return "pq1" + encoded;
}

bool DecodePQChatKey(const std::string& encoded, PQMessagingPublicKey& key)
{
    key = {};
    if (encoded.size() > PQ_CHATKEY_MAX_SIZE || encoded.compare(0, 3, "pq1") != 0) return false;
    bool invalid = false;
    auto bytes = DecodeBase32(encoded.substr(3).c_str(), &invalid);
    if (invalid || bytes.size() != PQ_MESSAGING_PUBLIC_SIZE + 4) return false;
    const auto hash = Hash(bytes.begin(), bytes.end() - 4);
    if (!std::equal(bytes.end() - 4, bytes.end(), hash.begin())) return false;
    bytes.resize(PQ_MESSAGING_PUBLIC_SIZE);
    return PQMessagingPublicKey::Decode(bytes, key) && EncodePQChatKey(key) == encoded;
}

PQMessagingKey::~PQMessagingKey() { Reset(); }
void PQMessagingKey::Reset()
{
    if (!m_seed.empty()) memory_cleanse(m_seed.data(), m_seed.size());
    m_seed.clear();
}
bool PQMessagingKey::MakeNewKey()
{
    Reset();
    if (!PQMessagingAvailable()) return false;
    m_seed.resize(PQ_MESSAGING_SEED_SIZE);
    for (size_t offset = 0; offset < m_seed.size(); offset += 32) {
        GetStrongRandBytes(m_seed.data() + offset, 32);
    }
    return true;
}
bool PQMessagingKey::SetSeed(const PQSecret& seed)
{
    Reset();
    if (seed.size() != PQ_MESSAGING_SEED_SIZE || !PQMessagingAvailable()) return false;
    m_seed = seed;
    return true;
}
bool PQMessagingKey::GetSeed(PQSecret& seed) const
{
    seed.clear();
    if (m_seed.size() != PQ_MESSAGING_SEED_SIZE) return false;
    seed = m_seed;
    return true;
}
bool PQMessagingKey::GetPublicKey(uint32_t network, const CPubKey& classical, PQMessagingPublicKey& key) const
{
    key = {};
#ifdef VERGE_HAVE_SMSG_MLKEM
    if (m_seed.size() != PQ_MESSAGING_SEED_SIZE) return false;
    auto kem = ImportKEM(m_seed.data() + 32, 64, true);
    pos::experimental::MLDSA44Key signing;
    PQMessagingPublicKey result;
    result.network = network;
    result.classical = classical;
    result.kem.resize(MLKEM768_PUBLIC_KEY_SIZE);
    size_t size = result.kem.size();
    if (!kem || EVP_PKEY_get_octet_string_param(kem.get(), OSSL_PKEY_PARAM_PUB_KEY,
            result.kem.data(), result.kem.size(), &size) != 1 || size != result.kem.size() ||
        !signing.SetSeed(m_seed.data(), 32) || !signing.GetPublicKey(result.signing) || !result.IsValid()) return false;
    key = std::move(result);
    return true;
#else
    return false;
#endif
}

bool EncapsulateMLKEM768(const std::vector<unsigned char>& public_key,
                       std::vector<unsigned char>& ciphertext, PQSecret& secret)
{
    ciphertext.clear();
    secret.clear();
#ifdef VERGE_HAVE_SMSG_MLKEM
    if (public_key.size() != MLKEM768_PUBLIC_KEY_SIZE) return false;
    auto key = ImportKEM(public_key.data(), public_key.size(), false);
    ContextPtr ctx(key ? EVP_PKEY_CTX_new_from_pkey(nullptr, key.get(), nullptr) : nullptr, EVP_PKEY_CTX_free);
    if (!ctx || EVP_PKEY_encapsulate_init(ctx.get(), nullptr) != 1) return false;
    std::vector<unsigned char> ct(MLKEM768_CIPHERTEXT_SIZE);
    PQSecret shared(32);
    size_t ct_size = ct.size(), secret_size = shared.size();
    if (EVP_PKEY_encapsulate(ctx.get(), ct.data(), &ct_size, shared.data(), &secret_size) != 1 ||
        ct_size != ct.size() || secret_size != shared.size()) return false;
    ciphertext = std::move(ct);
    secret = std::move(shared);
    return true;
#else
    return false;
#endif
}

bool PQMessagingKey::Decapsulate(const std::vector<unsigned char>& ciphertext, PQSecret& secret) const
{
    secret.clear();
#ifdef VERGE_HAVE_SMSG_MLKEM
    if (m_seed.size() != PQ_MESSAGING_SEED_SIZE || ciphertext.size() != MLKEM768_CIPHERTEXT_SIZE) return false;
    auto key = ImportKEM(m_seed.data() + 32, 64, true);
    ContextPtr ctx(key ? EVP_PKEY_CTX_new_from_pkey(nullptr, key.get(), nullptr) : nullptr, EVP_PKEY_CTX_free);
    if (!ctx || EVP_PKEY_decapsulate_init(ctx.get(), nullptr) != 1) return false;
    PQSecret shared(32);
    size_t size = shared.size();
    if (EVP_PKEY_decapsulate(ctx.get(), shared.data(), &size, ciphertext.data(), ciphertext.size()) != 1 ||
        size != shared.size()) return false;
    secret = std::move(shared);
    return true;
#else
    return false;
#endif
}

bool PQMessagingKey::Sign(const uint256& hash, std::vector<unsigned char>& signature) const
{
    signature.clear();
    pos::experimental::MLDSA44Key key;
    return m_seed.size() == PQ_MESSAGING_SEED_SIZE && key.SetSeed(m_seed.data(), 32) &&
        key.Sign({hash.begin(), hash.end()}, MessageContext(), signature);
}

bool VerifyPQMessageSignature(const PQMessagingPublicKey& key, const uint256& hash,
                              const std::vector<unsigned char>& signature)
{
    return key.IsValid() && pos::experimental::VerifyMLDSA44(key.signing,
        {hash.begin(), hash.end()}, MessageContext(), signature);
}
} // namespace smsg
