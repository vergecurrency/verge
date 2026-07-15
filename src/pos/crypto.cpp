// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/crypto.h>

#include <secp256k1.h>
#include <secp256k1_extrakeys.h>
#include <secp256k1_schnorrsig.h>

namespace pos {

bool VerifySchnorr(const unsigned char public_key[SCHNORR_PUBLIC_KEY_SIZE],
                   const uint256& message,
                   const unsigned char signature[SCHNORR_SIGNATURE_SIZE])
{
    secp256k1_xonly_pubkey parsed;
    if (!secp256k1_xonly_pubkey_parse(secp256k1_context_static, &parsed,
                                      public_key)) {
        return false;
    }
    return secp256k1_schnorrsig_verify(secp256k1_context_static, signature,
                                       message.begin(), message.size(),
                                       &parsed) == 1;
}

bool SignSchnorr(const unsigned char secret_key[32], const uint256& message,
                 const unsigned char auxiliary_randomness[32],
                 unsigned char signature[SCHNORR_SIGNATURE_SIZE],
                 unsigned char public_key[SCHNORR_PUBLIC_KEY_SIZE])
{
    secp256k1_context* context =
        secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    if (context == nullptr) return false;

    secp256k1_keypair keypair;
    secp256k1_xonly_pubkey xonly;
    int parity = 0;
    const bool valid =
        secp256k1_keypair_create(context, &keypair, secret_key) == 1 &&
        secp256k1_keypair_xonly_pub(context, &xonly, &parity, &keypair) == 1 &&
        secp256k1_xonly_pubkey_serialize(context, public_key, &xonly) == 1 &&
        secp256k1_schnorrsig_sign32(context, signature, message.begin(),
                                    &keypair, auxiliary_randomness) == 1;
    secp256k1_context_destroy(context);
    return valid;
}

bool GetSchnorrPublicKey(
    const unsigned char secret_key[32],
    unsigned char public_key[SCHNORR_PUBLIC_KEY_SIZE])
{
    secp256k1_context* context = secp256k1_context_create(
        SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    if (context == nullptr) return false;
    secp256k1_keypair keypair;
    secp256k1_xonly_pubkey xonly;
    int parity = 0;
    const bool ok = secp256k1_keypair_create(context, &keypair, secret_key) &&
        secp256k1_keypair_xonly_pub(context, &xonly, &parity, &keypair) &&
        secp256k1_xonly_pubkey_serialize(context, public_key, &xonly);
    secp256k1_context_destroy(context);
    return ok;
}

} // namespace pos
