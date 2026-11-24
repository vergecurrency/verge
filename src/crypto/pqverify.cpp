// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <pos/pqcrypto.h>
#include <amount.h>
#include <hash.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/opensslv.h>
#include <memory>
#include <string>
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_ML_DSA) && !defined(LIBRESSL_VERSION_NUMBER)
#include <openssl/core_names.h>
#include <openssl/params.h>
#define VERGE_HAVE_MLDSA_VERIFY 1
#endif

namespace pos { namespace experimental {
uint256 GetMLDSA44KeyId(const std::vector<unsigned char>& public_key)
{
    if (public_key.size() != MLDSA44_PUBLIC_KEY_SIZE) return uint256();
    CHashWriter writer(SER_GETHASH, 0);
    writer << std::string("Verge/PQKey/MLDSA44/v1") << public_key;
    return writer.GetHash();
}

const std::vector<unsigned char>& PQTransactionContext()
{
    static const std::string label = "Verge/Transaction/MLDSA44/v1";
    static const std::vector<unsigned char> context(label.begin(), label.end());
    return context;
}

bool GetPQTransactionHash(uint32_t network_id, int32_t activation_height,
                          int64_t amount, const uint256& transaction_hash, uint256& hash)
{
    const auto input = transaction_hash;
    hash.SetNull();
    if (network_id == 0 || activation_height < 0 || !MoneyRange(amount)) return false;
    CHashWriter writer(SER_GETHASH, 0);
    writer << std::string(PQTransactionContext().begin(), PQTransactionContext().end())
           << network_id << activation_height << amount << input;
    hash = writer.GetHash();
    return true;
}

bool VerifyMLDSA44(const std::vector<unsigned char>& public_key,
                   const std::vector<unsigned char>& message,
                   const std::vector<unsigned char>& context,
                   const std::vector<unsigned char>& signature)
{
    if (public_key.size() != MLDSA44_PUBLIC_KEY_SIZE || signature.size() != MLDSA44_SIGNATURE_SIZE ||
        context.size() > MLDSA44_MAX_CONTEXT_SIZE || message.size() > MLDSA44_MAX_MESSAGE_SIZE) return false;
#ifdef VERGE_HAVE_MLDSA_VERIFY
    struct Errors {
        bool marked{ERR_set_mark() == 1};
        ~Errors() { if (marked) ERR_pop_to_mark(); else ERR_clear_error(); }
    } errors;
    std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> importer(
        EVP_PKEY_CTX_new_from_name(nullptr, "ML-DSA-44", nullptr), EVP_PKEY_CTX_free);
    if (!importer || EVP_PKEY_fromdata_init(importer.get()) != 1) return false;
    OSSL_PARAM key_params[] = {OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY,
        const_cast<unsigned char*>(public_key.data()), public_key.size()), OSSL_PARAM_construct_end()};
    EVP_PKEY* raw = nullptr;
    const int imported = EVP_PKEY_fromdata(importer.get(), &raw, EVP_PKEY_PUBLIC_KEY, key_params);
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(raw, EVP_PKEY_free);
    if (imported != 1 || !key) return false;
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> verifier(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    static unsigned char empty = 0;
    OSSL_PARAM params[] = {OSSL_PARAM_construct_octet_string(OSSL_SIGNATURE_PARAM_CONTEXT_STRING,
        context.empty() ? &empty : const_cast<unsigned char*>(context.data()), context.size()), OSSL_PARAM_construct_end()};
    return verifier && EVP_DigestVerifyInit_ex(verifier.get(), nullptr, nullptr, nullptr, nullptr,
        key.get(), params) == 1 && EVP_DigestVerify(verifier.get(), signature.data(), signature.size(),
        message.empty() ? &empty : message.data(), message.size()) == 1;
#else
    return false;
#endif
}
} }
