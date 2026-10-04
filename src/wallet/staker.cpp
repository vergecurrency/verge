// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/staker.h>

#include <chain.h>
#include <chainparams.h>
#include <consensus/validation.h>
#include <miner.h>
#include <net.h>
#include <policy/policy.h>
#include <protocol.h>
#include <pos/consensus.h>
#include <pos/crypto.h>
#include <pos/validation.h>
#include <pos/votepool.h>
#include <pos/vrf.h>
#include <random.h>
#include <shutdown.h>
#include <support/cleanse.h>
#include <timedata.h>
#include <util/moneystr.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/wallet.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <exception>
#include <limits>
#include <map>
#include <mutex>

namespace {

using PublicKeyBytes = std::array<unsigned char, pos::SCHNORR_PUBLIC_KEY_SIZE>;
std::mutex g_attempt_mutex;
std::map<const CWallet*, uint64_t> g_last_attempted_slot;
std::map<const CWallet*, int64_t> g_last_bond_attempt;

bool ShouldAttemptAutomaticBond(const CWallet* wallet)
{
    const int64_t now = GetTime();
    std::lock_guard<std::mutex> lock(g_attempt_mutex);
    int64_t& last_attempt = g_last_bond_attempt[wallet];
    if (last_attempt != 0 && now - last_attempt < 60) return false;
    last_attempt = now;
    return true;
}

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
    if (ShutdownRequested()) {
        error = "shutdown requested";
        return false;
    }
    pos::StakeProof proof;
    pos::BondRecord selected_bond;
    CKey selected_key;
    uint32_t block_time = 0;
    std::vector<pos::CheckpointVote> votes;
    std::vector<pos::VoteEquivocationEvidence> vote_evidence;

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
            if (ShutdownRequested()) {
                error = "shutdown requested";
                return false;
            }
            CKey key;
            PublicKeyBytes public_key{};
            if (wallet.GetKey(key_id, key) &&
                pos::GetSchnorrPublicKey(key.begin(), public_key.data())) {
                wallet_keys.emplace(public_key, key);
            }
        }

        for (const pos::SnapshotEntry& entry : snapshot->entries) {
            if (ShutdownRequested()) {
                error = "shutdown requested";
                return false;
            }
            if (state.IsEligibilityLocked(entry.outpoint, slot.epoch,
                                          tip->nHeight + 1)) continue;
            PublicKeyBytes signing_public_key{};
            std::copy(entry.bond.data.signing_public_key,
                      entry.bond.data.signing_public_key +
                          pos::SCHNORR_PUBLIC_KEY_SIZE,
                      signing_public_key.begin());
            const auto owned = wallet_keys.find(signing_public_key);
            if (owned == wallet_keys.end()) continue;
            pos::CheckpointVote vote;
            if (!BuildVote(state, entry.outpoint, owned->second, tip,
                           final_pow, slot, params, vote)) continue;
            const pos::VotePoolResult result = pos::GetVotePool().Add(vote);
            if (g_connman &&
                (result == pos::VotePoolResult::ADDED ||
                 result == pos::VotePoolResult::REPLACED)) {
                const CInv inv(MSG_POS_VOTE, pos::GetVoteSigningHash(vote));
                g_connman->ForEachNode([&inv](CNode* node) {
                    node->PushInventory(inv);
                });
            }
        }

        pos::GetVotePool().Prune(slot.epoch);
        for (const pos::CheckpointVote& vote :
             pos::GetVotePool().GetVotes(params.nPoSMaxVotesPerBlock)) {
            if (pos::CheckCheckpointVote(
                    vote, state, params.nPoSSnapshotDelayEpochs, slot.epoch,
                    tip->nHeight + 1) == pos::ValidationError::NONE) {
                votes.push_back(vote);
            }
        }
        for (const pos::VoteEquivocationEvidence& evidence :
             pos::GetVoteEvidencePool().GetEvidence(
                 pos::MAX_EVIDENCE_PER_BLOCK)) {
            if (pos::CheckVoteEvidence(evidence, state) ==
                pos::ValidationError::NONE) {
                vote_evidence.push_back(evidence);
            }
        }

        for (const pos::SnapshotEntry& entry : snapshot->entries) {
            if (ShutdownRequested()) {
                error = "shutdown requested";
                return false;
            }
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
            break;
        }
    }

    if (!selected_key.IsValid()) {
        error = "wallet has no eligible bond in the current slot";
        return false;
    }
    if (ShutdownRequested()) {
        error = "shutdown requested";
        return false;
    }
    std::unique_ptr<CBlockTemplate> block_template =
        BlockAssembler(Params()).CreateNewPoSBlock(
            proof, selected_bond, selected_key.begin(), block_time, votes, {},
            vote_evidence);
    if (!block_template) {
        error = "failed to construct proof-of-stake block template";
        return false;
    }
    const std::shared_ptr<const CBlock> block =
        std::make_shared<const CBlock>(block_template->block);
    if (ShutdownRequested()) {
        error = "shutdown requested";
        return false;
    }
    if (!ProcessNewBlock(Params(), block, true, nullptr)) {
        error = "proof-of-stake block was not accepted";
        return false;
    }
    block_hash = block->GetHash();
    for (const pos::VoteEquivocationEvidence& evidence : vote_evidence) {
        pos::GetVoteEvidencePool().Remove(
            pos::GetTaggedHash(pos::HashDomain::EQUIVOCATION, evidence));
    }
    return true;
}

bool EnsureAutomaticStakeBond(CWallet& wallet, uint256& txid,
                              CAmount& amount, std::string& error)
{
    txid.SetNull();
    amount = 0;
    if (ShutdownRequested()) {
        error = "shutdown requested";
        return false;
    }
    LOCK2(cs_main, wallet.cs_wallet);
    if (!wallet.IsStakingEnabled()) {
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

    for (const auto& item : wallet.mapWallet) {
        if (ShutdownRequested()) {
            error = "shutdown requested";
            return false;
        }
        const CWalletTx& wallet_tx = item.second;
        if (wallet_tx.GetDepthInMainChain() < 0) continue;
        for (uint32_t i = 0; i < wallet_tx.tx->vout.size(); ++i) {
            pos::BondData bond;
            if (wallet_tx.GetDepthInMainChain() == 0 &&
                wallet_tx.InMempool() &&
                pos::ParseBondScript(wallet_tx.tx->vout[i].scriptPubKey, bond) &&
                !wallet.IsSpent(item.first, i)) {
                return true;
            }
        }
    }

    const Consensus::Params& params = Params().GetConsensus();
    std::vector<COutput> coins;
    wallet.AvailableCoins(coins, true, nullptr, 1, MAX_MONEY, MAX_MONEY, 0,
                          params.nPoSStakeMaturity);
    std::sort(coins.begin(), coins.end(), [](const COutput& a, const COutput& b) {
        return a.tx->tx->vout[a.i].nValue > b.tx->tx->vout[b.i].nValue;
    });
    CAmount mature_balance = 0;
    CAmount selected_balance = 0;
    // Bound legacy inputs well below the standard transaction weight limit.
    static constexpr size_t MAX_AUTOMATIC_BOND_INPUTS = 100;
    size_t selected_inputs = 0;
    size_t selected_size = 0;
    const size_t input_size_budget = MAX_STANDARD_TX_WEIGHT / 4 - 1000;
    CCoinControl coin_control;
    coin_control.m_min_depth = params.nPoSStakeMaturity;
    for (const COutput& coin : coins) {
        if (ShutdownRequested()) {
            error = "shutdown requested";
            return false;
        }
        pos::BondData bond;
        const CTxOut& output = coin.tx->tx->vout[coin.i];
        if (coin.fSpendable &&
            !pos::ParseBondScript(output.scriptPubKey, bond)) {
            mature_balance += output.nValue;
            const int input_size = CalculateMaximumSignedInputSize(output, &wallet);
            if (input_size > 0 && selected_inputs < MAX_AUTOMATIC_BOND_INPUTS &&
                static_cast<size_t>(input_size) <= input_size_budget - selected_size) {
                selected_balance += output.nValue;
                selected_size += input_size;
                coin_control.Select(COutPoint(coin.tx->GetHash(), coin.i));
                ++selected_inputs;
            }
        }
    }
    const CAmount reserve = wallet.GetStakingReserveBalance();
    if (mature_balance <= reserve ||
        mature_balance - reserve < params.nPoSMinStake) {
        error = "mature balance above the staking reserve is below the minimum bond";
        return false;
    }
    if (std::min(selected_balance, mature_balance - reserve) < params.nPoSMinStake) {
        error = "size-limited automatic bond is below the minimum stake";
        return false;
    }

    WalletBatch batch(wallet.GetDBHandle());
    const CPubKey staking_pubkey = wallet.GenerateNewKey(batch);
    CKey staking_key;
    if (!wallet.GetKey(staking_pubkey.GetID(), staking_key)) {
        error = "failed to retrieve delegated staking key";
        return false;
    }
    const CPubKey reward_pubkey = wallet.GenerateNewKey(batch);
    const CPubKey withdrawal_pubkey = wallet.GenerateNewKey(batch);
    pos::BondData bond;
    unsigned char vrf_secret[32]{};
    const bool keys_ok =
        pos::GetSchnorrPublicKey(staking_key.begin(),
                                 bond.signing_public_key) &&
        pos::DeriveVrfKey(staking_key.begin(), vrf_secret,
                          bond.vrf_public_key);
    memory_cleanse(vrf_secret, sizeof(vrf_secret));
    if (!keys_ok) {
        error = "failed to derive delegated staking credentials";
        return false;
    }
    bond.reward_key_id = reward_pubkey.GetID();
    bond.withdrawal_key_id = withdrawal_pubkey.GetID();
    wallet.SetAddressBook(GetDestinationForKey(reward_pubkey, OutputType::LEGACY),
                          "staking rewards", "receive");
    wallet.SetAddressBook(GetDestinationForKey(withdrawal_pubkey, OutputType::LEGACY),
                          "staking withdrawal", "receive");

    // Keep the reserve across all batches, rather than subtracting it per batch.
    amount = std::min(selected_balance, mature_balance - reserve);
    std::vector<CRecipient> recipients{
        CRecipient{pos::GetBondScript(bond), amount, true}};
    CReserveKey change_key(&wallet);
    CAmount fee = 0;
    int change_position = -1;
    CTransactionRef tx;
    if (!wallet.CreateTransaction(recipients, tx, change_key, fee,
                                  change_position, error, coin_control)) {
        amount = 0;
        return false;
    }
    for (const CTxOut& output : tx->vout) {
        pos::BondData parsed;
        if (pos::ParseBondScript(output.scriptPubKey, parsed)) {
            amount = output.nValue;
            break;
        }
    }
    if (amount < params.nPoSMinStake) {
        error = "automatic bond amount after fees is below the minimum stake";
        amount = 0;
        return false;
    }
    CValidationState validation_state;
    mapValue_t metadata;
    metadata["pos_staking_key"] = HexStr(staking_pubkey);
    metadata["pos_automatic_bond"] = "1";
    if (ShutdownRequested()) {
        error = "shutdown requested";
        amount = 0;
        return false;
    }
    if (!wallet.CommitTransaction(tx, std::move(metadata), {}, "", change_key,
                                  g_connman.get(), validation_state) ||
        !validation_state.IsValid()) {
        error = strprintf("automatic bond transaction rejected: %s",
                          FormatStateMessage(validation_state));
        amount = 0;
        return false;
    }
    txid = tx->GetHash();
    return true;
}

void StakeWallets()
{
    if (ShutdownRequested()) return;
    for (const std::shared_ptr<CWallet>& wallet : GetWallets()) {
        if (ShutdownRequested()) return;
        try {
            uint256 bond_txid;
            CAmount bond_amount = 0;
            std::string bond_error;
            if (ShouldAttemptAutomaticBond(wallet.get()) &&
                EnsureAutomaticStakeBond(*wallet, bond_txid, bond_amount,
                                         bond_error) && !bond_txid.IsNull()) {
                LogPrintf("Created automatic stake bond %s for %s with wallet %s\n",
                          bond_txid.ToString(), FormatMoney(bond_amount),
                          wallet->GetName());
            }
            uint256 block_hash;
            std::string error;
            if (TryStakeBlock(*wallet, block_hash, error)) {
                LogPrintf("Produced proof-of-stake block %s with wallet %s\n",
                          block_hash.ToString(), wallet->GetName());
            }
        } catch (const std::exception& exception) {
            LogPrintf("Staking worker error for wallet %s: %s\n",
                      wallet->GetName(), exception.what());
        } catch (...) {
            LogPrintf("Unknown staking worker error for wallet %s\n",
                      wallet->GetName());
        }
    }
}
