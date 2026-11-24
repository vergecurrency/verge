// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/pqcrypto.h>

#include <hash.h>
#include <random.h>
#include <support/cleanse.h>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/opensslv.h>

#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_ML_DSA) && !defined(LIBRESSL_VERSION_NUMBER)
#define VERGE_HAVE_EXPERIMENTAL_MLDSA44 1
#include <openssl/core_names.h>
#include <openssl/params.h>
#endif

#include <memory>
#include <string>
#include <utility>

namespace pos {
namespace experimental {
namespace {

// Preserve the caller's error queue while discarding local verification errors.
struct ErrorScope {
    const bool marked{ERR_set_mark() == 1};
    ~ErrorScope()
    {
        if (marked) ERR_pop_to_mark();
        else ERR_clear_error();
    }
};

const std::vector<unsigned char>& HybridContext()
{
    static const std::string label = "Verge/PoS/Hybrid-Schnorr-MLDSA44/v1";
    static const std::vector<unsigned char> context(label.begin(), label.end());
    return context;
}

#ifdef VERGE_HAVE_EXPERIMENTAL_MLDSA44
using KeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using KeyContextPtr = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using DigestContextPtr = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
using SignaturePtr = std::unique_ptr<EVP_SIGNATURE, decltype(&EVP_SIGNATURE_free)>;

KeyPtr ImportKey(const unsigned char* data, size_t size, bool secret)
{
    KeyContextPtr context(
        EVP_PKEY_CTX_new_from_name(nullptr, "ML-DSA-44", nullptr),
        &EVP_PKEY_CTX_free);
    if (!context || EVP_PKEY_fromdata_init(context.get()) != 1) {
        return KeyPtr(nullptr, &EVP_PKEY_free);
    }
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_octet_string(
            secret ? OSSL_PKEY_PARAM_ML_DSA_SEED : OSSL_PKEY_PARAM_PUB_KEY,
            const_cast<unsigned char*>(data), size),
        OSSL_PARAM_construct_end(),
    };
    EVP_PKEY* key = nullptr;
    const int result = EVP_PKEY_fromdata(
        context.get(), &key, secret ? EVP_PKEY_KEYPAIR : EVP_PKEY_PUBLIC_KEY,
        params);
    KeyPtr owned(key, &EVP_PKEY_free);
    if (result != 1) owned.reset();
    return owned;
}

OSSL_PARAM ContextParam(const std::vector<unsigned char>& context)
{
    // OpenSSL treats a null parameter buffer as missing, even at length zero.
    static unsigned char empty = 0;
    return OSSL_PARAM_construct_octet_string(
        OSSL_SIGNATURE_PARAM_CONTEXT_STRING,
        context.empty() ? &empty : const_cast<unsigned char*>(context.data()),
        context.size());
}

const unsigned char* MessageData(const std::vector<unsigned char>& message)
{
    static const unsigned char empty = 0;
    return message.empty() ? &empty : message.data();
}
#endif

} // namespace

bool MLDSA44Available()
{
#ifdef VERGE_HAVE_EXPERIMENTAL_MLDSA44
    ErrorScope errors;
    KeyContextPtr context(
        EVP_PKEY_CTX_new_from_name(nullptr, "ML-DSA-44", nullptr),
        &EVP_PKEY_CTX_free);
    SignaturePtr signature(EVP_SIGNATURE_fetch(nullptr, "ML-DSA-44", nullptr),
                           &EVP_SIGNATURE_free);
    return context != nullptr && signature != nullptr;
#else
    return false;
#endif
}

MLDSA44Key::~MLDSA44Key() { Reset(); }

bool MLDSA44Key::GetSeed(
    std::vector<unsigned char, secure_allocator<unsigned char>>& seed) const
{
    if (!seed.empty()) memory_cleanse(seed.data(), seed.size());
    seed.clear();
    if (!IsValid()) return false;
    seed.assign(m_seed.begin(), m_seed.end());
    return true;
}

void MLDSA44Key::Reset()
{
    if (!m_seed.empty()) memory_cleanse(m_seed.data(), m_seed.size());
    m_seed.clear();
}

bool MLDSA44Key::MakeNewKey()
{
    Reset();
    if (!MLDSA44Available()) return false;
    std::vector<unsigned char, secure_allocator<unsigned char>> seed(MLDSA44_SEED_SIZE);
    GetStrongRandBytes(seed.data(), seed.size());
    return SetSeed(seed.data(), seed.size());
}

bool MLDSA44Key::SetSeed(const unsigned char* seed, size_t size)
{
    Reset();
    if (seed == nullptr || size != MLDSA44_SEED_SIZE) return false;
#ifdef VERGE_HAVE_EXPERIMENTAL_MLDSA44
    ErrorScope errors;
    std::vector<unsigned char, secure_allocator<unsigned char>> candidate(seed, seed + size);
    auto key = ImportKey(candidate.data(), candidate.size(), true);
    if (!key) return false;
    m_seed.swap(candidate);
    return true;
#else
    return false;
#endif
}

bool MLDSA44Key::GetPublicKey(std::vector<unsigned char>& public_key) const
{
    public_key.clear();
    if (!IsValid()) return false;
#ifdef VERGE_HAVE_EXPERIMENTAL_MLDSA44
    ErrorScope errors;
    auto key = ImportKey(m_seed.data(), m_seed.size(), true);
    if (!key) return false;
    std::vector<unsigned char> candidate(MLDSA44_PUBLIC_KEY_SIZE);
    size_t size = candidate.size();
    if (EVP_PKEY_get_octet_string_param(key.get(), OSSL_PKEY_PARAM_PUB_KEY,
                                       candidate.data(), candidate.size(), &size) != 1 ||
        size != MLDSA44_PUBLIC_KEY_SIZE) return false;
    public_key.swap(candidate);
    return true;
#else
    return false;
#endif
}

bool MLDSA44Key::Sign(const std::vector<unsigned char>& message,
                      const std::vector<unsigned char>& context,
                      std::vector<unsigned char>& signature) const
{
    // Preserve aliased inputs; other failures erase stale output.
    if (&signature == &message || &signature == &context) return false;
    signature.clear();
    if (!IsValid() || context.size() > MLDSA44_MAX_CONTEXT_SIZE ||
        message.size() > MLDSA44_MAX_MESSAGE_SIZE) return false;
#ifdef VERGE_HAVE_EXPERIMENTAL_MLDSA44
    ErrorScope errors;
    auto key = ImportKey(m_seed.data(), m_seed.size(), true);
    DigestContextPtr signer(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    OSSL_PARAM params[] = {ContextParam(context), OSSL_PARAM_construct_end()};
    if (!key || !signer ||
        EVP_DigestSignInit_ex(signer.get(), nullptr, nullptr, nullptr, nullptr,
                             key.get(), params) != 1) return false;
    std::vector<unsigned char> candidate(MLDSA44_SIGNATURE_SIZE);
    size_t size = candidate.size();
    if (EVP_DigestSign(signer.get(), candidate.data(), &size,
                       MessageData(message), message.size()) != 1 ||
        size != MLDSA44_SIGNATURE_SIZE) return false;
    signature.swap(candidate);
    return true;
#else
    return false;
#endif
}

bool GetHybridSigningHash(uint32_t network_id, HybridPurpose purpose,
                          const HybridPublicKey& public_key,
                          const uint256& message, uint256& hash)
{
    // Allow an aliased message/hash without losing the input.
    const uint256 payload = message;
    hash.SetNull();
    if (network_id == 0 ||
        (purpose != HybridPurpose::BLOCK && purpose != HybridPurpose::VOTE) ||
        public_key.mldsa.size() != MLDSA44_PUBLIC_KEY_SIZE ||
        !IsValidSchnorrPublicKey(public_key.schnorr.data())) return false;
    CHashWriter writer(SER_GETHASH, 0);
    writer << std::string(HybridContext().begin(), HybridContext().end())
           << network_id << static_cast<uint8_t>(purpose);
    writer.write(reinterpret_cast<const char*>(public_key.schnorr.data()),
                 public_key.schnorr.size());
    writer.write(reinterpret_cast<const char*>(public_key.mldsa.data()),
                 public_key.mldsa.size());
    writer << payload;
    hash = writer.GetHash();
    return true;
}

bool SignHybrid(const unsigned char schnorr_secret[32],
                const unsigned char auxiliary_randomness[32],
                const MLDSA44Key& pq_key, uint32_t network_id,
                HybridPurpose purpose, const uint256& message,
                HybridPublicKey& public_key, HybridSignature& signature)
{
    public_key = HybridPublicKey{};
    signature = HybridSignature{};
    if (schnorr_secret == nullptr || auxiliary_randomness == nullptr) return false;
    HybridPublicKey candidate_key;
    HybridSignature candidate_signature;
    uint256 hash;
    if (!pq_key.GetPublicKey(candidate_key.mldsa) ||
        !GetSchnorrPublicKey(schnorr_secret, candidate_key.schnorr.data()) ||
        !GetHybridSigningHash(network_id, purpose, candidate_key, message, hash) ||
        !SignSchnorr(schnorr_secret, hash, auxiliary_randomness,
                     candidate_signature.schnorr.data(), candidate_key.schnorr.data()) ||
        !pq_key.Sign(std::vector<unsigned char>(hash.begin(), hash.end()),
                     HybridContext(), candidate_signature.mldsa)) return false;
    public_key = std::move(candidate_key);
    signature = std::move(candidate_signature);
    return true;
}

bool VerifyHybrid(const HybridPublicKey& public_key, uint32_t network_id,
                  HybridPurpose purpose, const uint256& message,
                  const HybridSignature& signature)
{
    uint256 hash;
    return GetHybridSigningHash(network_id, purpose, public_key, message, hash) &&
        signature.mldsa.size() == MLDSA44_SIGNATURE_SIZE &&
        VerifySchnorr(public_key.schnorr.data(), hash, signature.schnorr.data()) &&
        VerifyMLDSA44(public_key.mldsa,
                       std::vector<unsigned char>(hash.begin(), hash.end()),
                       HybridContext(), signature.mldsa);
}

} // namespace experimental
} // namespace pos
