// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_POS_CRYPTO_H
#define VERGE_POS_CRYPTO_H

#include <pos/primitives.h>
#include <uint256.h>

namespace pos {

bool VerifySchnorr(const unsigned char public_key[SCHNORR_PUBLIC_KEY_SIZE],
                   const uint256& message,
                   const unsigned char signature[SCHNORR_SIGNATURE_SIZE]);

bool SignSchnorr(const unsigned char secret_key[32], const uint256& message,
                 const unsigned char auxiliary_randomness[32],
                 unsigned char signature[SCHNORR_SIGNATURE_SIZE],
                 unsigned char public_key[SCHNORR_PUBLIC_KEY_SIZE]);
bool GetSchnorrPublicKey(
    const unsigned char secret_key[32],
    unsigned char public_key[SCHNORR_PUBLIC_KEY_SIZE]);

} // namespace pos

#endif // VERGE_POS_CRYPTO_H
