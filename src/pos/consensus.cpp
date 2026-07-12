// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/consensus.h>

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

} // namespace pos