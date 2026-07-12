// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_POS_CONSENSUS_H
#define VERGE_POS_CONSENSUS_H

#include <consensus/params.h>

#include <cstdint>

namespace pos {

struct SlotInfo {
    uint64_t activation_slot{0};
    uint64_t global_slot{0};
    uint64_t relative_slot{0};
    uint64_t epoch{0};
    uint32_t slot_in_epoch{0};
};

/** Return false when the timestamp is not canonical for a valid post-activation slot. */
bool GetSlotInfo(uint32_t final_pow_time, uint32_t candidate_time,
                 const Consensus::Params& params, SlotInfo& slot_info);

/** Height whose UTXO state defines the initial delayed stake snapshot. */
int GetInitialStakeSnapshotHeight(const Consensus::Params& params);

} // namespace pos

#endif // VERGE_POS_CONSENSUS_H