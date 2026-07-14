// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/primitives.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace pos {

namespace {

const char* GetDomainTag(HashDomain domain)
{
    switch (domain) {
    case HashDomain::BOND: return "VergePoS/Bond/v1";
    case HashDomain::BLOCK: return "VergePoS/Block/v1";
    case HashDomain::VOTE: return "VergePoS/Vote/v1";
    case HashDomain::EPOCH_SEED: return "VergePoS/EpochSeed/v1";
    case HashDomain::STAKE_ROOT: return "VergePoS/StakeRoot/v1";
    case HashDomain::VOTE_ROOT: return "VergePoS/VoteRoot/v1";
    case HashDomain::STAKE_PROOF: return "VergePoS/StakeProof/v1";
    case HashDomain::UNBOND: return "VergePoS/Unbond/v1";
    case HashDomain::EQUIVOCATION: return "VergePoS/Equivocation/v1";
    }
    throw std::invalid_argument("unknown Verge PoS hash domain");
}

template <size_t N>
bool IsAllZero(const unsigned char (&value)[N])
{
    return std::all_of(value, value + N, [](unsigned char byte) { return byte == 0; });
}

} // namespace

TaggedHashWriter::TaggedHashWriter(HashDomain domain)
{
    const char* tag = GetDomainTag(domain);
    unsigned char tag_hash[CSHA256::OUTPUT_SIZE];
    CSHA256().Write(reinterpret_cast<const unsigned char*>(tag), std::strlen(tag)).Finalize(tag_hash);
    m_hasher.Write(tag_hash, sizeof(tag_hash));
    m_hasher.Write(tag_hash, sizeof(tag_hash));
}

void TaggedHashWriter::write(const char* data, size_t size)
{
    m_hasher.Write(reinterpret_cast<const unsigned char*>(data), size);
}

uint256 TaggedHashWriter::GetHash()
{
    uint256 result;
    m_hasher.Finalize(result.begin());
    return result;
}

bool HasSupportedVersion(const StakeProof& proof)
{
    return proof.version == POS_OBJECT_VERSION;
}

bool HasSupportedVersion(const CheckpointVote& vote)
{
    return vote.version == POS_OBJECT_VERSION;
}

bool IsPoSVersion(int32_t version)
{
    return (version & BLOCK_VERSION_POS) != 0;
}

StructureError CheckStructure(const StakeProof& proof)
{
    if (!HasSupportedVersion(proof)) return StructureError::UNSUPPORTED_VERSION;
    if (proof.bond_outpoint.IsNull()) return StructureError::NULL_BOND_OUTPOINT;
    if (proof.slot == 0) return StructureError::INVALID_SLOT;
    if (proof.snapshot_root.IsNull() || proof.epoch_seed.IsNull()) return StructureError::NULL_COMMITMENT;
    if (proof.vrf_public_key[0] != 0x02 && proof.vrf_public_key[0] != 0x03) return StructureError::INVALID_VRF_PUBLIC_KEY;
    if (IsAllZero(proof.vrf_output)) return StructureError::ZERO_VRF_OUTPUT;
    if (IsAllZero(proof.vrf_proof)) return StructureError::ZERO_VRF_PROOF;
    if (IsAllZero(proof.signing_public_key)) return StructureError::ZERO_SIGNING_PUBLIC_KEY;
    if (IsAllZero(proof.block_signature)) return StructureError::ZERO_SIGNATURE;
    return StructureError::NONE;
}

StructureError CheckStructure(const CheckpointVote& vote)
{
    if (!HasSupportedVersion(vote)) return StructureError::UNSUPPORTED_VERSION;
    if (vote.bond_outpoint.IsNull()) return StructureError::NULL_BOND_OUTPOINT;
    if (vote.source_epoch > vote.target_epoch) return StructureError::INVALID_EPOCH_ORDER;
    if (vote.source_checkpoint_root.IsNull() || vote.target_checkpoint_root.IsNull()) return StructureError::NULL_COMMITMENT;
    if (vote.head_slot == 0 || vote.head_block_root.IsNull()) return StructureError::INVALID_HEAD;
    if (IsAllZero(vote.signature)) return StructureError::ZERO_SIGNATURE;
    return StructureError::NONE;
}

bool HasCanonicalVoteOrder(const std::vector<CheckpointVote>& votes, uint32_t maximum_votes)
{
    if (votes.size() > maximum_votes) return false;
    for (size_t i = 1; i < votes.size(); ++i) {
        const CheckpointVote& previous = votes[i - 1];
        const CheckpointVote& current = votes[i];
        if (current.bond_outpoint < previous.bond_outpoint) return false;
        if (current.bond_outpoint == previous.bond_outpoint &&
            !(GetTaggedHash(HashDomain::VOTE, previous) < GetTaggedHash(HashDomain::VOTE, current))) {
            return false;
        }
    }
    return true;
}

} // namespace pos
