// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef VERGE_SCRIPT_PQ_H
#define VERGE_SCRIPT_PQ_H

#include <script/script.h>
#include <uint256.h>

static constexpr size_t PQ_PUBLIC_KEY_BYTES = 1312;
static constexpr size_t PQ_SIGNATURE_BYTES = 2420;
static constexpr size_t PQ_CHUNK_BYTES = 520;
static constexpr unsigned int PQ_SIGOP_COST = 200;
static constexpr size_t PQ_MULTISIG_MAX_KEYS = 5;
static constexpr size_t PQ_MULTISIG_MAX_POLICY_BYTES = 3 + 32 * PQ_MULTISIG_MAX_KEYS;
static constexpr size_t PQ_MULTISIG_MAX_SCRIPTSIG_BYTES = 20000;

struct PQMultisigPolicy
{
    unsigned int required{0};
    std::vector<uint256> keys;
};

struct PQMultisigSignature
{
    unsigned int index{0};
    std::vector<unsigned char> signature;
    std::vector<unsigned char> public_key;
};

std::vector<unsigned char> EncodePQMultisigPolicy(const PQMultisigPolicy& policy);
bool DecodePQMultisigPolicy(const std::vector<unsigned char>& encoded, PQMultisigPolicy& policy);
uint256 GetPQMultisigPolicyId(const PQMultisigPolicy& policy);
CScript GetPQMultisigScript(const uint256& policy_id);
bool ExtractPQMultisigId(const CScript& script, uint256& policy_id);
bool IsPQOutputScript(const CScript& script);
CScript GetPQMultisigScriptSig(const PQMultisigPolicy& policy,
                              const std::vector<PQMultisigSignature>& signatures);
bool DecodePQMultisigScriptSig(const CScript& script, PQMultisigPolicy& policy,
                               std::vector<PQMultisigSignature>& signatures);
size_t GetPQMultisigMaximumScriptSigSize(const PQMultisigPolicy& policy);

CScript GetPQScript(const uint256& key_id);
bool ExtractPQKeyId(const CScript& script, uint256& key_id);
CScript GetPQScriptSig(const std::vector<unsigned char>& signature,
                       const std::vector<unsigned char>& public_key);
bool DecodePQScriptSig(const CScript& script,
                        std::vector<unsigned char>& signature,
                        std::vector<unsigned char>& public_key);

#endif
