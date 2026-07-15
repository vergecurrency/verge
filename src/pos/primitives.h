// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_POS_PRIMITIVES_H
#define VERGE_POS_PRIMITIVES_H

#include <crypto/sha256.h>
#include <primitives/transaction.h>
#include <serialize.h>
#include <uint256.h>

#include <cstdint>
#include <vector>

namespace pos {

static constexpr uint8_t POS_OBJECT_VERSION = 1;
static constexpr int32_t BLOCK_VERSION_POS = (1 << 15);
static constexpr uint64_t FINAL_POW_CHECKPOINT_EPOCH = UINT64_MAX;
static constexpr size_t SCHNORR_PUBLIC_KEY_SIZE = 32;
static constexpr size_t SCHNORR_SIGNATURE_SIZE = 64;
static constexpr size_t VRF_PUBLIC_KEY_SIZE = 33;
static constexpr size_t VRF_OUTPUT_SIZE = 32;
static constexpr size_t VRF_PROOF_SIZE = 81;

struct StakeProof {
    uint8_t version{POS_OBJECT_VERSION};
    COutPoint bond_outpoint;
    uint64_t slot{0};
    uint64_t snapshot_epoch{0};
    uint256 snapshot_root;
    uint256 epoch_seed;
    unsigned char vrf_public_key[VRF_PUBLIC_KEY_SIZE]{};
    unsigned char vrf_output[VRF_OUTPUT_SIZE]{};
    unsigned char vrf_proof[VRF_PROOF_SIZE]{};
    unsigned char signing_public_key[SCHNORR_PUBLIC_KEY_SIZE]{};

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(version, bond_outpoint, slot, snapshot_epoch, snapshot_root,
                  epoch_seed, vrf_public_key, vrf_output, vrf_proof,
                  signing_public_key);
    }
};

struct BlockAuthorization {
    uint8_t version{POS_OBJECT_VERSION};
    uint32_t network_id{0};
    uint256 parent_block_root;
    uint256 candidate_header_hash;
    uint64_t slot{0};
    COutPoint bond_outpoint;
    uint256 stake_proof_hash;
    uint256 fee_reward_transaction_hash;
    uint256 parent_randomness;
    unsigned char signature[SCHNORR_SIGNATURE_SIZE]{};

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(version, network_id, parent_block_root, candidate_header_hash,
                  slot, bond_outpoint, stake_proof_hash,
                  fee_reward_transaction_hash, parent_randomness, signature);
    }
};
struct CheckpointVote {
    uint8_t version{POS_OBJECT_VERSION};
    COutPoint bond_outpoint;
    uint64_t snapshot_epoch{0};
    uint64_t source_epoch{0};
    uint256 source_checkpoint_root;
    uint64_t target_epoch{0};
    uint256 target_checkpoint_root;
    uint64_t head_slot{0};
    uint256 head_block_root;
    unsigned char signature[SCHNORR_SIGNATURE_SIZE]{};

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(version, bond_outpoint, snapshot_epoch, source_epoch,
                  source_checkpoint_root, target_epoch, target_checkpoint_root,
                  head_slot, head_block_root, signature);
    }
};

struct BlockEquivocationEvidence {
    uint8_t version{POS_OBJECT_VERSION};
    BlockAuthorization first;
    BlockAuthorization second;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(version, first, second);
    }
};

struct VoteEquivocationEvidence {
    uint8_t version{POS_OBJECT_VERSION};
    CheckpointVote first;
    CheckpointVote second;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(version, first, second);
    }
};
enum class HashDomain {
    BOND,
    BLOCK,
    VOTE,
    EPOCH_SEED,
    STAKE_ROOT,
    VOTE_ROOT,
    STAKE_PROOF,
    UNBOND,
    EQUIVOCATION,
    EVIDENCE_ROOT,
    SLOT,
    VRF_KEY,
};

enum class StructureError {
    NONE,
    UNSUPPORTED_VERSION,
    NULL_BOND_OUTPOINT,
    INVALID_SLOT,
    NULL_COMMITMENT,
    INVALID_VRF_PUBLIC_KEY,
    ZERO_VRF_OUTPUT,
    ZERO_VRF_PROOF,
    ZERO_SIGNING_PUBLIC_KEY,
    ZERO_SIGNATURE,
    INVALID_EPOCH_ORDER,
    INVALID_HEAD,
    INVALID_NETWORK,
    INVALID_AUTHORIZATION,
    NON_CANONICAL_EVIDENCE,
    NOT_EQUIVOCATION,
};

class TaggedHashWriter {
private:
    CSHA256 m_hasher;

public:
    explicit TaggedHashWriter(HashDomain domain);

    int GetType() const { return SER_GETHASH; }
    int GetVersion() const { return 0; }
    void write(const char* data, size_t size);
    uint256 GetHash();

    template <typename T>
    TaggedHashWriter& operator<<(const T& object)
    {
        ::Serialize(*this, object);
        return *this;
    }
};

template <typename T>
uint256 GetTaggedHash(HashDomain domain, const T& object)
{
    TaggedHashWriter writer(domain);
    writer << object;
    return writer.GetHash();
}
uint256 GetBlockSigningHash(const BlockAuthorization& authorization);
uint256 GetVoteSigningHash(const CheckpointVote& vote);
bool HasSupportedVersion(const StakeProof& proof);
bool HasSupportedVersion(const CheckpointVote& vote);
bool HasSupportedVersion(const BlockAuthorization& authorization);
bool IsPoSVersion(int32_t version);
StructureError CheckStructure(const StakeProof& proof);
StructureError CheckStructure(const CheckpointVote& vote);
StructureError CheckStructure(const BlockAuthorization& authorization);
StructureError CheckStructure(const BlockEquivocationEvidence& evidence);
StructureError CheckStructure(const VoteEquivocationEvidence& evidence);
bool HasCanonicalVoteOrder(const std::vector<CheckpointVote>& votes, uint32_t maximum_votes);

} // namespace pos

#endif // VERGE_POS_PRIMITIVES_H
