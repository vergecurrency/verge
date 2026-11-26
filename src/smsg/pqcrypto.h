// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_SMSG_PQCRYPTO_H
#define VERGE_SMSG_PQCRYPTO_H

#include <pos/pqcrypto.h>
#include <pubkey.h>
#include <support/allocators/secure.h>

namespace smsg {

using PQSecret = std::vector<unsigned char, secure_allocator<unsigned char>>;
static constexpr size_t MLKEM768_PUBLIC_KEY_SIZE = 1184;
static constexpr size_t MLKEM768_CIPHERTEXT_SIZE = 1088;
static constexpr size_t PQ_MESSAGING_SEED_SIZE = 96;
static constexpr size_t PQ_MESSAGING_PUBLIC_SIZE = 2534;
static constexpr size_t PQ_CHATKEY_MAX_SIZE = 4200;
static constexpr size_t PQ_MAX_LOCAL_KEYS = 256;

struct PQMessagingPublicKey {
    uint32_t network = 0;
    CPubKey classical;
    std::vector<unsigned char> kem;
    std::vector<unsigned char> signing;

    bool IsValid() const;
    std::vector<unsigned char> Encode() const;
    static bool Decode(const std::vector<unsigned char>& encoded, PQMessagingPublicKey& out);
    uint256 Fingerprint() const;
};

bool PQMessagingAvailable();
std::string EncodePQChatKey(const PQMessagingPublicKey& key);
bool DecodePQChatKey(const std::string& encoded, PQMessagingPublicKey& key);
bool EncapsulateMLKEM768(const std::vector<unsigned char>& public_key,
                       std::vector<unsigned char>& ciphertext, PQSecret& secret);

class PQMessagingKey {
    PQSecret m_seed;
public:
    PQMessagingKey() = default;
    PQMessagingKey(const PQMessagingKey&) = delete;
    PQMessagingKey& operator=(const PQMessagingKey&) = delete;
    ~PQMessagingKey();
    void Reset();
    bool MakeNewKey();
    bool SetSeed(const PQSecret& seed);
    bool GetSeed(PQSecret& seed) const;
    bool GetPublicKey(uint32_t network, const CPubKey& classical, PQMessagingPublicKey& key) const;
    bool Decapsulate(const std::vector<unsigned char>& ciphertext, PQSecret& secret) const;
    bool Sign(const uint256& hash, std::vector<unsigned char>& signature) const;
};

bool VerifyPQMessageSignature(const PQMessagingPublicKey& key, const uint256& hash,
                              const std::vector<unsigned char>& signature);

} // namespace smsg
#endif
