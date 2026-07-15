// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_POS_CONSENSUS_H
#define VERGE_POS_CONSENSUS_H

#include <consensus/params.h>
#include <amount.h>

#include <cstdint>
#include <array>
#include <vector>

class uint256;

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

bool ComputeInitialEpochSeed(uint32_t network_id, int32_t activation_height,
                             const std::vector<uint256>& predecessor_hashes,
                             uint256& seed);
uint256 ComputeNextEpochSeed(
    const uint256& previous_seed, uint64_t next_epoch,
    const std::vector<std::array<unsigned char, 32>>& vrf_outputs);

/** Compare two post-activation fork children by vote weight, then hash. */
bool PreferFork(CAmount candidate_weight, const uint256& candidate_child,
                CAmount current_weight, const uint256& current_child);

} // namespace pos

#endif // VERGE_POS_CONSENSUS_H
