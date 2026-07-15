// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_POS_STAKE_H
#define VERGE_POS_STAKE_H

#include <pos/primitives.h>
#include <script/script.h>
#include <uint256.h>

#include <cstdint>

namespace pos {

struct BondData {
    unsigned char signing_public_key[SCHNORR_PUBLIC_KEY_SIZE]{};
    unsigned char vrf_public_key[VRF_PUBLIC_KEY_SIZE]{};
    uint160 reward_key_id;
    uint160 withdrawal_key_id;
};

struct UnbondData {
    uint32_t unlock_height{0};
    uint160 withdrawal_key_id;
};

bool IsValid(const BondData& bond);
bool IsValid(const UnbondData& unbond);
CScript GetBondScript(const BondData& bond);
CScript GetUnbondScript(const UnbondData& unbond);
bool ParseBondScript(const CScript& script, BondData& bond);
bool ParseUnbondScript(const CScript& script, UnbondData& unbond);

} // namespace pos

#endif // VERGE_POS_STAKE_H
