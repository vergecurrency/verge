// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/staker.h>

#include <chain.h>
#include <chainparams.h>
#include <miner.h>
#include <pos/consensus.h>
#include <pos/crypto.h>
#include <pos/validation.h>
#include <pos/vrf.h>
#include <random.h>
#include <support/cleanse.h>
#include <timedata.h>
#include <validation.h>
#include <wallet/wallet.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>

namespace {

using PublicKeyBytes = std::array<unsigned char, pos::SCHNORR_PUBLIC_KEY_SIZE>;
std::mutex g_attempt_mutex;
std::map<const CWallet*, uint64_t> g_last_attempted_slot;

bool BuildVote(const pos::State& state, const COutPoint& bond_outpoint,
               const CKey& key, const CBlockIndex* head,
               const CBlockIndex* final_pow, const pos::SlotInfo& current_slot,
               const Consensus::Params& params, pos::CheckpointVote& vote)
{
    const pos::Checkpoint* source = state.LatestJustified();
    const pos::Checkpoint* target = state.LatestCheckpoint();
    if (source == nullptr || target == nullptr || source->root == target->root ||
        target->epoch == pos::FINAL_POW_CHECKPOINT_EPOCH) {
        return false;
    }
    const uint64_t snapshot_epoch = pos::GetRequiredSnapshotEpoch(
        target->epoch, params.nPoSSnapshotDelayEpochs);
    const pos::StakeSnapshot* snapshot = state.FindSnapshot(snapshot_epoch);
    if (snapshot == nullptr) return false;
    const auto member = std::lower_bound(
        snapshot->entries.begin(), snapshot->entries.end(), bond_outpoint,
        [](const pos::SnapshotEntry& entry, const COutPoint& outpoint) {
            return entry.outpoint < outpoint;
        });
    if (member == snapshot->entries.end() ||
        member->outpoint != bond_outpoint) return false;
    pos::SlotInfo head_slot;
    if (!pos::GetSlotInfo(final_pow->nTime, head->nTime, params, head_slot) ||
        head_slot.global_slot >= current_slot.global_slot) return false;

    vote.bond_outpoint = bond_outpoint;
    vote.snapshot_epoch = snapshot_epoch;
    vote.source_epoch = source->epoch;
    vote.source_checkpoint_root = source->root;
    vote.target_epoch = target->epoch;
    vote.target_checkpoint_root = target->root;
    vote.head_slot = head_slot.global_slot;
    vote.head_block_root = head->GetBlockHash();
    unsigned char auxiliary[32];
    GetRandBytes(auxiliary, sizeof(auxiliary));
    unsigned char public_key[pos::SCHNORR_PUBLIC_KEY_SIZE];
    const bool signed_vote = pos::SignSchnorr(
        key.begin(), pos::GetVoteSigningHash(vote), auxiliary,
        vote.signature, public_key);
    memory_cleanse(auxiliary, sizeof(auxiliary));
    return signed_vote &&
        std::equal(public_key, public_key + pos::SCHNORR_PUBLIC_KEY_SIZE,
                   member->bond.data.signing_public_key);
}

} // namespace

bool TryStakeBlock(CWallet& wallet, uint256& block_hash, std::string& error,
                   bool force)
{
    block_hash.SetNull();
    pos::StakeProof proof;
    pos::BondRecord selected_bond;
    CKey selected_key;
    uint32_t block_time = 0;
    std::vector<pos::CheckpointVote> votes;

    {
        LOCK2(cs_main, wallet.cs_wallet);
        if (!force && !wallet.IsStakingEnabled()) {
            error = "staking is disabled";
            return false;
        }
        if (wallet.IsLocked()) {
            error = "wallet is locked";
            return false;
        }
        if (IsInitialBlockDownload()) {
            error = "chain is still synchronizing";
            return false;
        }
        const Consensus::Params& params = Params().GetConsensus();
        CBlockIndex* tip = chainActive.Tip();
        if (tip == nullptr || !params.IsPoSActive(tip->nHeight + 1)) {
            error = "proof of stake is not active at the next height";
            return false;
        }
        const CBlockIndex* final_pow =
            chainActive[params.nPoSActivationHeight - 1];
        if (final_pow == nullptr) {
            error = "final proof-of-work anchor is unavailable";
            return false;
        }
        const uint64_t slot_seconds = params.nPoSSlotSeconds;
        if (slot_seconds == 0) {
            error = "invalid slot duration";
            return false;
        }
        const uint64_t adjusted = static_cast<uint64_t>(GetAdjustedTime());
        const uint64_t candidate = (adjusted / slot_seconds) * slot_seconds;
        if (candidate <= tip->nTime ||
            candidate > std::numeric_limits<uint32_t>::max()) {
            error = "no new canonical slot is available";
            return false;
        }
        block_time = static_cast<uint32_t>(candidate);
        pos::SlotInfo slot;
        if (!pos::GetSlotInfo(final_pow->nTime, block_time, params, slot)) {
            error = "candidate time is not a canonical proof-of-stake slot";
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(g_attempt_mutex);
            uint64_t& last_slot = g_last_attempted_slot[&wallet];
            if (!force && last_slot == slot.global_slot) {
                error = "current slot was already evaluated";
                return false;
            }
            last_slot = slot.global_slot;
        }

        pos::State state = GetPoSStateSnapshot();
        pos::StateUndo preparation_undo;
        if (state.FindEpochSeed(0) == nullptr &&
            tip->nHeight == params.nPoSActivationHeight - 1) {
            std::vector<uint256> predecessors;
            predecessors.reserve(120);
            for (int height = params.nPoSActivationHeight - 120;
                 height < params.nPoSActivationHeight; ++height) {
                predecessors.push_back(chainActive[height]->GetBlockHash());
            }
            uint256 initial_seed;
            if (!pos::ComputeInitialEpochSeed(
                    params.nPoSNetworkId, params.nPoSActivationHeight,
                    predecessors, initial_seed) ||
                !state.SetEpochSeed(0, initial_seed, preparation_undo)) {
                error = "failed to derive the initial epoch seed";
                return false;
            }
        }
        if (!state.PrepareEpoch(slot.epoch, tip->nHeight + 1, params,
                                preparation_undo)) {
            error = "failed to prepare skipped epoch state";
            return false;
        }
        const uint256* stored_seed = state.FindEpochSeed(slot.epoch);
        if (stored_seed == nullptr) {
            error = "epoch seed is unavailable";
            return false;
        }
        const uint256 epoch_seed = *stored_seed;
        const uint64_t snapshot_epoch = pos::GetRequiredSnapshotEpoch(
            slot.epoch, params.nPoSSnapshotDelayEpochs);
        const pos::StakeSnapshot* snapshot = state.FindSnapshot(snapshot_epoch);
        if (snapshot == nullptr) {
            error = "required stake snapshot is unavailable";
            return false;
        }
        if (snapshot->total_value <= 0) {
            error = "required stake snapshot is empty";
            return false;
        }

        std::map<PublicKeyBytes, CKey> wallet_keys;
        for (const CKeyID& key_id : wallet.GetKeys()) {
            CKey key;
            PublicKeyBytes public_key{};
            if (wallet.GetKey(key_id, key) &&
                pos::GetSchnorrPublicKey(key.begin(), public_key.data())) {
                wallet_keys.emplace(public_key, key);
            }
        }
        for (const pos::SnapshotEntry& entry : snapshot->entries) {
            if (state.IsEligibilityLocked(entry.outpoint, slot.epoch,
                                          tip->nHeight + 1)) continue;
            PublicKeyBytes signing_public_key{};
            std::copy(entry.bond.data.signing_public_key,
                      entry.bond.data.signing_public_key +
                          pos::SCHNORR_PUBLIC_KEY_SIZE,
                      signing_public_key.begin());
            const auto owned = wallet_keys.find(signing_public_key);
            if (owned == wallet_keys.end()) continue;

            unsigned char vrf_secret[32]{};
            unsigned char vrf_public_key[pos::VRF_PUBLIC_KEY_SIZE]{};
            if (!pos::DeriveVrfKey(owned->second.begin(), vrf_secret,
                                   vrf_public_key) ||
                !std::equal(vrf_public_key,
                            vrf_public_key + pos::VRF_PUBLIC_KEY_SIZE,
                            entry.bond.data.vrf_public_key)) {
                memory_cleanse(vrf_secret, sizeof(vrf_secret));
                continue;
            }
            proof.bond_outpoint = entry.outpoint;
            proof.slot = slot.global_slot;
            proof.snapshot_epoch = snapshot_epoch;
            proof.snapshot_root = snapshot->root;
            proof.epoch_seed = epoch_seed;
            std::copy(signing_public_key.begin(), signing_public_key.end(),
                      proof.signing_public_key);
            std::copy(vrf_public_key,
                      vrf_public_key + pos::VRF_PUBLIC_KEY_SIZE,
                      proof.vrf_public_key);
            const std::vector<unsigned char> input = pos::GetVrfInput(
                params.nPoSNetworkId, epoch_seed, slot.global_slot,
                entry.outpoint);
            const bool proved = pos::VrfProve(
                vrf_secret, input.data(), input.size(), proof.vrf_public_key,
                proof.vrf_output, proof.vrf_proof);
            memory_cleanse(vrf_secret, sizeof(vrf_secret));
            if (!proved) continue;
            CBlock eligibility;
            eligibility.posExtension.stake_proof = proof;
            if (pos::CheckVrfEligibility(eligibility, *snapshot,
                                         params.nPoSNetworkId) !=
                pos::ValidationError::NONE) continue;
            selected_bond = entry.bond;
            selected_key = owned->second;
            pos::CheckpointVote vote;
            if (BuildVote(state, entry.outpoint, selected_key, tip, final_pow,
                          slot, params, vote)) {
                votes.push_back(vote);
            }
            break;
        }
    }

    if (!selected_key.IsValid()) {
        error = "wallet has no eligible bond in the current slot";
        return false;
    }
    std::unique_ptr<CBlockTemplate> block_template =
        BlockAssembler(Params()).CreateNewPoSBlock(
            proof, selected_bond, selected_key.begin(), block_time, votes);
    if (!block_template) {
        error = "failed to construct proof-of-stake block template";
        return false;
    }
    const std::shared_ptr<const CBlock> block =
        std::make_shared<const CBlock>(block_template->block);
    if (!ProcessNewBlock(Params(), block, true, nullptr)) {
        error = "proof-of-stake block was not accepted";
        return false;
    }
    block_hash = block->GetHash();
    return true;
}

void StakeWallets()
{
    for (const std::shared_ptr<CWallet>& wallet : GetWallets()) {
        uint256 block_hash;
        std::string error;
        if (TryStakeBlock(*wallet, block_hash, error)) {
            LogPrintf("Produced proof-of-stake block %s with wallet %s\n",
                      block_hash.ToString(), wallet->GetName());
        }
    }
}
