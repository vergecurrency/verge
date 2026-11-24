// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_POS_PQCRYPTO_H
#define VERGE_POS_PQCRYPTO_H

#include <pos/crypto.h>
#include <support/allocators/secure.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// Experimental trial cryptography; wire formats and activation live in PoS consensus.
namespace pos {
namespace experimental {

static constexpr size_t MLDSA44_SEED_SIZE = 32;
using pos::MLDSA44_PUBLIC_KEY_SIZE;
using pos::MLDSA44_SIGNATURE_SIZE;
static constexpr size_t MLDSA44_MAX_CONTEXT_SIZE = 255;
static constexpr size_t MLDSA44_MAX_MESSAGE_SIZE = 65536;

// Missing/disabled OpenSSL ML-DSA support must never fall back to Schnorr.
bool MLDSA44Available();
uint256 GetMLDSA44KeyId(const std::vector<unsigned char>& public_key);
const std::vector<unsigned char>& PQTransactionContext();
bool GetPQTransactionHash(uint32_t network_id, int32_t activation_height,
                          int64_t amount, const uint256& transaction_hash,
                          uint256& hash);

// Const operations may run concurrently; import/reset require exclusive access.
class MLDSA44Key {
    std::vector<unsigned char, secure_allocator<unsigned char>> m_seed;

public:
    MLDSA44Key() = default;
    ~MLDSA44Key();
    MLDSA44Key(const MLDSA44Key&) = delete;
    MLDSA44Key& operator=(const MLDSA44Key&) = delete;

    void Reset();
    bool IsValid() const { return m_seed.size() == MLDSA44_SEED_SIZE; }
    bool MakeNewKey();
    // Import independent entropy, never a revealed classical signing scalar.
    bool SetSeed(const unsigned char* seed, size_t size);
    bool GetPublicKey(std::vector<unsigned char>& public_key) const;
    bool GetSeed(std::vector<unsigned char, secure_allocator<unsigned char>>& seed) const;
    bool Sign(const std::vector<unsigned char>& message,
              const std::vector<unsigned char>& context,
              std::vector<unsigned char>& signature) const;
};

bool VerifyMLDSA44(const std::vector<unsigned char>& public_key,
                   const std::vector<unsigned char>& message,
                   const std::vector<unsigned char>& context,
                   const std::vector<unsigned char>& signature);

enum class HybridPurpose : uint8_t { BLOCK = 1, VOTE = 2 };

struct HybridPublicKey {
    std::array<unsigned char, SCHNORR_PUBLIC_KEY_SIZE> schnorr{};
    std::vector<unsigned char> mldsa;
};

struct HybridSignature {
    std::array<unsigned char, SCHNORR_SIGNATURE_SIZE> schnorr{};
    std::vector<unsigned char> mldsa;
};

bool GetHybridSigningHash(uint32_t network_id, HybridPurpose purpose,
                          const HybridPublicKey& public_key,
                          const uint256& message, uint256& hash);
bool SignHybrid(const unsigned char schnorr_secret[32],
                const unsigned char auxiliary_randomness[32],
                const MLDSA44Key& pq_key, uint32_t network_id,
                HybridPurpose purpose, const uint256& message,
                HybridPublicKey& public_key, HybridSignature& signature);
bool VerifyHybrid(const HybridPublicKey& public_key, uint32_t network_id,
                  HybridPurpose purpose, const uint256& message,
                  const HybridSignature& signature);

} // namespace experimental
} // namespace pos

#endif // VERGE_POS_PQCRYPTO_H
