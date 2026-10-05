// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/consensus.h>
#include <pos/primitives.h>

#include <limits>

namespace pos {

bool GetSlotInfo(uint32_t final_pow_time, uint32_t candidate_time,
                 const Consensus::Params& params, SlotInfo& slot_info)
{
    if (params.nPoSSlotSeconds == 0 || params.nPoSEpochSlots == 0) {
        return false;
    }
    if (candidate_time % params.nPoSSlotSeconds != 0) {
        return false;
    }

    const uint64_t activation_slot = final_pow_time / params.nPoSSlotSeconds + 1;
    const uint64_t global_slot = candidate_time / params.nPoSSlotSeconds;
    if (global_slot < activation_slot) {
        return false;
    }

    const uint64_t relative_slot = global_slot - activation_slot;
    slot_info.activation_slot = activation_slot;
    slot_info.global_slot = global_slot;
    slot_info.relative_slot = relative_slot;
    slot_info.epoch = relative_slot / params.nPoSEpochSlots;
    slot_info.slot_in_epoch = static_cast<uint32_t>(relative_slot % params.nPoSEpochSlots);
    return true;
}

int GetInitialStakeSnapshotHeight(const Consensus::Params& params)
{
    const uint64_t delay = uint64_t{params.nPoSEpochSlots} * params.nPoSSnapshotDelayEpochs;
    if (delay > static_cast<uint64_t>(std::numeric_limits<int>::max()) ||
        params.nPoSActivationHeight < 0 ||
        static_cast<uint64_t>(params.nPoSActivationHeight) < delay) {
        return -1;
    }
    return params.nPoSActivationHeight - static_cast<int>(delay);
}

bool ComputeInitialEpochSeed(uint32_t network_id, int32_t activation_height,
                             const uint256& genesis_hash, uint256& seed)
{
    if (network_id == 0 || activation_height < 0 ||
        genesis_hash.IsNull()) {
        return false;
    }
    TaggedHashWriter writer(HashDomain::EPOCH_SEED);
    writer << network_id << activation_height << genesis_hash;
    seed = writer.GetHash();
    return !seed.IsNull();
}

uint256 ComputeNextEpochSeed(
    const uint256& previous_seed, uint64_t next_epoch,
    const uint256& snapshot_root)
{
    TaggedHashWriter writer(HashDomain::EPOCH_SEED);
    writer << previous_seed << next_epoch << snapshot_root;
    return writer.GetHash();
}

bool PreferFork(CAmount candidate_weight, const uint256& candidate_child,
                CAmount current_weight, const uint256& current_child)
{
    if (candidate_weight != current_weight) {
        return candidate_weight > current_weight;
    }
    return candidate_child < current_child;
}

} // namespace pos
