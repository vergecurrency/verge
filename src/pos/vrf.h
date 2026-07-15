// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_POS_VRF_H
#define VERGE_POS_VRF_H

#include <pos/primitives.h>

#include <cstddef>
#include <vector>

namespace pos {

bool VrfProve(const unsigned char secret_key[32],
              const unsigned char* input, size_t input_size,
              unsigned char public_key[VRF_PUBLIC_KEY_SIZE],
              unsigned char output[VRF_OUTPUT_SIZE],
              unsigned char proof[VRF_PROOF_SIZE]);

bool VrfVerify(const unsigned char public_key[VRF_PUBLIC_KEY_SIZE],
               const unsigned char* input, size_t input_size,
               const unsigned char proof[VRF_PROOF_SIZE],
               unsigned char output[VRF_OUTPUT_SIZE]);

bool DeriveVrfKey(const unsigned char signing_secret[32],
                  unsigned char vrf_secret[32],
                  unsigned char vrf_public_key[VRF_PUBLIC_KEY_SIZE]);

std::vector<unsigned char> GetVrfInput(uint32_t network_id,
                                       const uint256& epoch_seed,
                                       uint64_t slot,
                                       const COutPoint& bond_outpoint);

} // namespace pos

#endif // VERGE_POS_VRF_H
