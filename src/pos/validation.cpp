// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/validation.h>

#include <pos/block.h>
#include <pos/crypto.h>
#include <pos/vrf.h>
#include <primitives/block.h>
#include <pubkey.h>
#include <script/standard.h>
#include <arith_uint256.h>

#include <algorithm>
#include <cstring>
#include <limits>

namespace pos {

uint64_t GetRequiredSnapshotEpoch(uint64_t block_epoch,
                                  uint32_t snapshot_delay_epochs)
{
    if (block_epoch < snapshot_delay_epochs) {
        return INITIAL_SNAPSHOT_EPOCH;
    }
    return block_epoch - snapshot_delay_epochs;
}

ValidationError CheckStakeAuthorization(const CBlock& block,
                                        const StakeSnapshot& snapshot,
                                        uint64_t expected_snapshot_epoch)
{
    const StakeProof& proof = block.posExtension.stake_proof;
    const BlockAuthorization& authorization =
        block.posExtension.authorization;

    if (snapshot.source_epoch != expected_snapshot_epoch ||
        proof.snapshot_epoch != expected_snapshot_epoch ||
        proof.snapshot_root != snapshot.root) {
        return ValidationError::SNAPSHOT_MISMATCH;
    }

    const auto it = std::lower_bound(
        snapshot.entries.begin(), snapshot.entries.end(), proof.bond_outpoint,
        [](const SnapshotEntry& entry, const COutPoint& outpoint) {
            return entry.outpoint < outpoint;
        });
    if (it == snapshot.entries.end() || it->outpoint != proof.bond_outpoint) {
        return ValidationError::BOND_MISSING;
    }

    if (it->bond.value <= 0 ||
        std::memcmp(it->bond.data.signing_public_key,
                    proof.signing_public_key,
                    SCHNORR_PUBLIC_KEY_SIZE) != 0 ||
        std::memcmp(it->bond.data.vrf_public_key, proof.vrf_public_key,
                    VRF_PUBLIC_KEY_SIZE) != 0) {
        return ValidationError::BOND_MISMATCH;
    }

    if (authorization.parent_randomness != proof.epoch_seed) {
        return ValidationError::AUTHORIZATION_MISMATCH;
    }
    if (!VerifySchnorr(proof.signing_public_key,
                       GetBlockSigningHash(authorization),
                       authorization.signature)) {
        return ValidationError::AUTHORIZATION_SIGNATURE;
    }
    return ValidationError::NONE;
}

ValidationError CheckVrfEligibility(const CBlock& block,
                                    const StakeSnapshot& snapshot,
                                    uint32_t network_id)
{
    const StakeProof& proof = block.posExtension.stake_proof;
    const auto it = std::lower_bound(
        snapshot.entries.begin(), snapshot.entries.end(), proof.bond_outpoint,
        [](const SnapshotEntry& entry, const COutPoint& outpoint) {
            return entry.outpoint < outpoint;
        });
    if (it == snapshot.entries.end() || it->outpoint != proof.bond_outpoint ||
        it->bond.value <= 0 || snapshot.total_value <= 0 ||
        it->bond.value > snapshot.total_value) {
        return ValidationError::STAKE_INELIGIBLE;
    }

    const std::vector<unsigned char> input =
        GetVrfInput(network_id, proof.epoch_seed, proof.slot,
                    proof.bond_outpoint);
    unsigned char verified_output[VRF_OUTPUT_SIZE]{};
    if (!VrfVerify(proof.vrf_public_key, input.data(), input.size(),
                   proof.vrf_proof, verified_output)) {
        return ValidationError::VRF_INVALID;
    }
    if (std::memcmp(verified_output, proof.vrf_output,
                    VRF_OUTPUT_SIZE) != 0) {
        return ValidationError::VRF_OUTPUT_MISMATCH;
    }

    uint256 output;
    std::reverse_copy(verified_output, verified_output + VRF_OUTPUT_SIZE,
                      output.begin());
    const arith_uint256 maximum = ~arith_uint256();
    const arith_uint256 total(static_cast<uint64_t>(snapshot.total_value));
    const arith_uint256 value(static_cast<uint64_t>(it->bond.value));
    const arith_uint256 quotient = maximum / total;
    const arith_uint256 remainder = maximum - quotient * total;
    const arith_uint256 threshold =
        quotient * value + (remainder * value) / total;
    if (UintToArith256(output) > threshold) {
        return ValidationError::STAKE_INELIGIBLE;
    }
    return ValidationError::NONE;
}

ValidationError CheckCheckpointVote(const CheckpointVote& vote,
                                    const State& state,
                                    uint32_t snapshot_delay_epochs,
                                    uint64_t current_epoch,
                                    int32_t current_height)
{
    if (CheckStructure(vote) != StructureError::NONE) {
        return ValidationError::VOTE_STRUCTURE;
    }
    if (state.IsEligibilityLocked(vote.bond_outpoint, current_epoch,
                                  current_height)) {
        return ValidationError::VOTE_BOND;
    }
    if (vote.snapshot_epoch != GetRequiredSnapshotEpoch(
            vote.target_epoch, snapshot_delay_epochs)) {
        return ValidationError::VOTE_SNAPSHOT;
    }
    const StakeSnapshot* snapshot = state.FindSnapshot(vote.snapshot_epoch);
    if (snapshot == nullptr) return ValidationError::VOTE_SNAPSHOT;
    const auto it = std::lower_bound(
        snapshot->entries.begin(), snapshot->entries.end(),
        vote.bond_outpoint,
        [](const SnapshotEntry& entry, const COutPoint& outpoint) {
            return entry.outpoint < outpoint;
        });
    if (it == snapshot->entries.end() ||
        it->outpoint != vote.bond_outpoint) {
        return ValidationError::VOTE_BOND;
    }
    const Checkpoint* source = state.FindJustified(vote.source_epoch);
    const Checkpoint* target = state.FindCheckpoint(vote.target_epoch);
    if (source == nullptr || target == nullptr ||
        source->root != vote.source_checkpoint_root ||
        target->root != vote.target_checkpoint_root) {
        return ValidationError::VOTE_CHECKPOINT;
    }
    if (!VerifySchnorr(it->bond.data.signing_public_key,
                       GetVoteSigningHash(vote), vote.signature)) {
        return ValidationError::VOTE_SIGNATURE;
    }
    return ValidationError::NONE;
}

ValidationError CheckVoteEvidence(const VoteEquivocationEvidence& evidence,
                                  const State& state)
{
    if (CheckStructure(evidence) != StructureError::NONE) {
        return ValidationError::VOTE_STRUCTURE;
    }
    const auto snapshot_contains = [&state](uint64_t epoch,
                                            const COutPoint& outpoint) {
        const StakeSnapshot* snapshot = state.FindSnapshot(epoch);
        if (snapshot == nullptr) return false;
        const auto it = std::lower_bound(
            snapshot->entries.begin(), snapshot->entries.end(), outpoint,
            [](const SnapshotEntry& entry, const COutPoint& value) {
                return entry.outpoint < value;
            });
        return it != snapshot->entries.end() && it->outpoint == outpoint;
    };
    if (!snapshot_contains(evidence.first.snapshot_epoch,
                           evidence.first.bond_outpoint) ||
        !snapshot_contains(evidence.second.snapshot_epoch,
                           evidence.second.bond_outpoint)) {
        return ValidationError::VOTE_SNAPSHOT;
    }
    const BondRecord* bond =
        state.FindHistoricalBond(evidence.first.bond_outpoint);
    if (bond == nullptr) return ValidationError::EVIDENCE_BOND;
    if (!VerifySchnorr(bond->data.signing_public_key,
                       GetVoteSigningHash(evidence.first),
                       evidence.first.signature) ||
        !VerifySchnorr(bond->data.signing_public_key,
                       GetVoteSigningHash(evidence.second),
                       evidence.second.signature)) {
        return ValidationError::EVIDENCE_SIGNATURE;
    }
    return ValidationError::NONE;
}

ValidationError CheckVotesAndEvidence(const CBlock& block,
                                      const State& state,
                                      uint32_t snapshot_delay_epochs,
                                      uint32_t network_id,
                                      uint64_t current_epoch,
                                      int32_t current_height)
{
    for (const CheckpointVote& vote : block.posExtension.votes) {
        const ValidationError error = CheckCheckpointVote(
            vote, state, snapshot_delay_epochs, current_epoch,
            current_height);
        if (error != ValidationError::NONE) return error;
    }

    for (const BlockEquivocationEvidence& evidence :
         block.posExtension.block_evidence) {
        if (evidence.first.network_id != network_id ||
            evidence.second.network_id != network_id) {
            return ValidationError::EVIDENCE_SIGNATURE;
        }
        const BondRecord* bond =
            state.FindHistoricalBond(evidence.first.bond_outpoint);
        if (bond == nullptr) return ValidationError::EVIDENCE_BOND;
        if (!VerifySchnorr(bond->data.signing_public_key,
                           GetBlockSigningHash(evidence.first),
                           evidence.first.signature) ||
            !VerifySchnorr(bond->data.signing_public_key,
                           GetBlockSigningHash(evidence.second),
                           evidence.second.signature)) {
            return ValidationError::EVIDENCE_SIGNATURE;
        }
    }

    for (const VoteEquivocationEvidence& evidence :
         block.posExtension.vote_evidence) {
        const ValidationError error = CheckVoteEvidence(evidence, state);
        if (error != ValidationError::NONE) return error;
    }
    return ValidationError::NONE;
}

ValidationError CheckReward(const CBlock& block, const BondRecord& bond,
                            CAmount fees)
{
    if (block.vtx.empty() || !block.vtx.front()->IsCoinBase()) {
        return ValidationError::REWARD_AMOUNT;
    }

    const BlockCommitment expected =
        GetBlockCommitment(block.posExtension);
    const CScript reward_script =
        GetScriptForDestination(CKeyID(bond.data.reward_key_id));
    size_t commitments = 0;
    CAmount claimed = 0;
    for (const CTxOut& output : block.vtx.front()->vout) {
        BlockCommitment commitment;
        if (ParseBlockCommitmentScript(output.scriptPubKey, commitment)) {
            ++commitments;
            if (output.nValue != 0) return ValidationError::REWARD_AMOUNT;
            if (commitment.stake_proof_hash != expected.stake_proof_hash ||
                commitment.vote_root != expected.vote_root ||
                commitment.evidence_root != expected.evidence_root ||
                commitment.snapshot_root != expected.snapshot_root ||
                commitment.epoch_seed != expected.epoch_seed) {
                return ValidationError::COMMITMENT_MISMATCH;
            }
            continue;
        }
        if (output.scriptPubKey.IsUnspendable()) {
            if (output.nValue != 0) return ValidationError::REWARD_AMOUNT;
            continue;
        }
        if (output.scriptPubKey != reward_script) {
            return ValidationError::REWARD_DESTINATION;
        }
        if (!MoneyRange(claimed + output.nValue)) {
            return ValidationError::REWARD_AMOUNT;
        }
        claimed += output.nValue;
    }

    if (commitments == 0) return ValidationError::COMMITMENT_MISSING;
    if (commitments != 1) return ValidationError::COMMITMENT_DUPLICATE;
    if (claimed > fees) return ValidationError::REWARD_AMOUNT;
    return ValidationError::NONE;
}

} // namespace pos
