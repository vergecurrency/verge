// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_POS_VALIDATION_H
#define VERGE_POS_VALIDATION_H

#include <amount.h>
#include <pos/state.h>

class CBlock;

namespace pos {

enum class ValidationError {
    NONE,
    SNAPSHOT_MISSING,
    SNAPSHOT_MISMATCH,
    BOND_MISSING,
    BOND_MISMATCH,
    AUTHORIZATION_MISMATCH,
    AUTHORIZATION_SIGNATURE,
    VRF_INVALID,
    VRF_OUTPUT_MISMATCH,
    STAKE_INELIGIBLE,
    VOTE_SNAPSHOT,
    VOTE_BOND,
    VOTE_SIGNATURE,
    EVIDENCE_BOND,
    EVIDENCE_SIGNATURE,
    COMMITMENT_MISSING,
    COMMITMENT_DUPLICATE,
    COMMITMENT_MISMATCH,
    REWARD_DESTINATION,
    REWARD_AMOUNT,
};

uint64_t GetRequiredSnapshotEpoch(uint64_t block_epoch,
                                  uint32_t snapshot_delay_epochs);

ValidationError CheckStakeAuthorization(const CBlock& block,
                                        const StakeSnapshot& snapshot,
                                        uint64_t expected_snapshot_epoch);

ValidationError CheckVrfEligibility(const CBlock& block,
                                    const StakeSnapshot& snapshot,
                                    uint32_t network_id);

ValidationError CheckVotesAndEvidence(const CBlock& block,
                                      const State& state,
                                      uint32_t snapshot_delay_epochs,
                                      uint32_t network_id,
                                      uint64_t current_epoch,
                                      int32_t current_height);

ValidationError CheckReward(const CBlock& block, const BondRecord& bond,
                            CAmount fees);

} // namespace pos

#endif // VERGE_POS_VALIDATION_H
