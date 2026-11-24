// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/stake.h>
#include <script/pq.h>

#include <pos/crypto.h>
#include <pos/vrf.h>

#include <crypto/common.h>

#include <algorithm>
#include <cstring>
#include <vector>

namespace pos {

namespace {

static constexpr unsigned char BOND_MAGIC[4] = {'V', 'P', 'B', 1};
static constexpr unsigned char UNBOND_MAGIC[4] = {'V', 'P', 'U', 1};
static constexpr size_t BOND_PAYLOAD_SIZE = 4 + SCHNORR_PUBLIC_KEY_SIZE +
                                            VRF_PUBLIC_KEY_SIZE + 20;
static constexpr size_t UNBOND_PAYLOAD_SIZE = 8;

template <size_t N>
bool IsAllZero(const unsigned char (&value)[N])
{
    return std::all_of(value, value + N, [](unsigned char byte) { return byte == 0; });
}

CScript GetKeyHashSuffix(const uint160& key_id)
{
    return CScript() << OP_DUP << OP_HASH160
                     << std::vector<unsigned char>(key_id.begin(), key_id.end())
                     << OP_EQUALVERIFY << OP_CHECKSIG;
}

bool ParseMetadataScript(const CScript& script, std::vector<unsigned char>& payload,
                         uint160& withdrawal_key_id)
{
    CScript::const_iterator cursor = script.begin();
    opcodetype opcode;
    if (!script.GetOp(cursor, opcode, payload) || opcode > OP_PUSHDATA4) return false;
    if (!script.GetOp(cursor, opcode) || opcode != OP_DROP) return false;

    std::vector<unsigned char> key_hash;
    if (!script.GetOp(cursor, opcode) || opcode != OP_DUP) return false;
    if (!script.GetOp(cursor, opcode) || opcode != OP_HASH160) return false;
    if (!script.GetOp(cursor, opcode, key_hash) || key_hash.size() != 20) return false;
    if (!script.GetOp(cursor, opcode) || opcode != OP_EQUALVERIFY) return false;
    if (!script.GetOp(cursor, opcode) || opcode != OP_CHECKSIG) return false;
    if (cursor != script.end()) return false;

    std::copy(key_hash.begin(), key_hash.end(), withdrawal_key_id.begin());
    return true;
}

} // namespace

bool IsValid(const BondData& bond)
{
    if (bond.version == HYBRID_POS_OBJECT_VERSION) {
        return IsValidSchnorrPublicKey(bond.signing_public_key) &&
            IsAllZero(bond.vrf_public_key) && bond.reward_key_id.IsNull() &&
            bond.withdrawal_key_id.IsNull() && !bond.pq_signing_key_id.IsNull() &&
            !bond.pq_reward_key_id.IsNull() && !bond.pq_withdrawal_key_id.IsNull();
    }
    if (bond.version != POS_OBJECT_VERSION || !bond.pq_signing_key_id.IsNull() ||
        !bond.pq_reward_key_id.IsNull() || !bond.pq_withdrawal_key_id.IsNull()) return false;
    return IsValidSchnorrPublicKey(bond.signing_public_key) &&
           IsValidVrfPublicKey(bond.vrf_public_key) &&
           !bond.reward_key_id.IsNull() &&
           !bond.withdrawal_key_id.IsNull();
}

bool IsValid(const UnbondData& unbond)
{
    return unbond.unlock_height != 0 &&
        ((unbond.version == POS_OBJECT_VERSION && !unbond.withdrawal_key_id.IsNull() && unbond.pq_withdrawal_key_id.IsNull()) ||
         (unbond.version == HYBRID_POS_OBJECT_VERSION && unbond.withdrawal_key_id.IsNull() && !unbond.pq_withdrawal_key_id.IsNull()));
}

uint64_t GetUnbondUnlockHeight(const UnbondData& unbond,
                               uint32_t confirmation_height,
                               uint32_t unbonding_blocks)
{
    return std::max<uint64_t>(unbond.unlock_height,
        static_cast<uint64_t>(confirmation_height) + unbonding_blocks);
}

bool HasBondMarker(const CScript& script)
{
    CScript::const_iterator cursor = script.begin();
    opcodetype opcode;
    std::vector<unsigned char> payload;
    return script.GetOp(cursor, opcode, payload) && opcode <= OP_PUSHDATA4 &&
           payload.size() >= sizeof(BOND_MAGIC) &&
           std::equal(BOND_MAGIC, BOND_MAGIC + 3, payload.begin()) &&
           (payload[3] == 1 || payload[3] == 2);
}

bool HasUnbondMarker(const CScript& script)
{
    CScript::const_iterator cursor = script.begin();
    opcodetype opcode;
    std::vector<unsigned char> payload;
    return script.GetOp(cursor, opcode, payload) && opcode <= OP_PUSHDATA4 &&
           payload.size() >= sizeof(UNBOND_MAGIC) &&
           std::equal(UNBOND_MAGIC, UNBOND_MAGIC + 3, payload.begin()) &&
           (payload[3] == 1 || payload[3] == 2);
}

CScript GetBondScript(const BondData& bond)
{
    if (!IsValid(bond)) return CScript();
    if (bond.version == HYBRID_POS_OBJECT_VERSION) {
        std::vector<unsigned char> payload{'V', 'P', 'B', 2};
        payload.insert(payload.end(), std::begin(bond.signing_public_key), std::end(bond.signing_public_key));
        payload.insert(payload.end(), bond.pq_signing_key_id.begin(), bond.pq_signing_key_id.end());
        payload.insert(payload.end(), bond.pq_reward_key_id.begin(), bond.pq_reward_key_id.end());
        CScript script = CScript() << payload << OP_DROP;
        const auto suffix = GetPQScript(bond.pq_withdrawal_key_id);
        script.insert(script.end(), suffix.begin(), suffix.end());
        return script;
    }

    std::vector<unsigned char> payload;
    payload.reserve(BOND_PAYLOAD_SIZE);
    payload.insert(payload.end(), BOND_MAGIC, BOND_MAGIC + sizeof(BOND_MAGIC));
    payload.insert(payload.end(), bond.signing_public_key,
                   bond.signing_public_key + sizeof(bond.signing_public_key));
    payload.insert(payload.end(), bond.vrf_public_key,
                   bond.vrf_public_key + sizeof(bond.vrf_public_key));
    payload.insert(payload.end(), bond.reward_key_id.begin(), bond.reward_key_id.end());

    CScript script;
    script << payload << OP_DROP;
    const CScript suffix = GetKeyHashSuffix(bond.withdrawal_key_id);
    script.insert(script.end(), suffix.begin(), suffix.end());
    return script;
}

CScript GetUnbondScript(const UnbondData& unbond)
{
    if (!IsValid(unbond)) return CScript();

    std::vector<unsigned char> payload;
    payload.insert(payload.end(), UNBOND_MAGIC, UNBOND_MAGIC + sizeof(UNBOND_MAGIC));
    payload[3] = unbond.version;
    unsigned char height[4];
    WriteLE32(height, unbond.unlock_height);
    payload.insert(payload.end(), height, height + sizeof(height));

    CScript script;
    script << payload << OP_DROP;
    const CScript suffix = unbond.version == HYBRID_POS_OBJECT_VERSION
        ? GetPQScript(unbond.pq_withdrawal_key_id) : GetKeyHashSuffix(unbond.withdrawal_key_id);
    script.insert(script.end(), suffix.begin(), suffix.end());
    return script;
}

bool ParseBondScript(const CScript& script, BondData& bond)
{
    uint256 pq_id;
    if (script.size() == 138 && ExtractPQKeyId(script, pq_id) &&
        script[2] == 'V' && script[4] == 'B' && script[5] == 2) {
        BondData parsed;
        parsed.version = 2;
        std::copy(script.begin() + 6, script.begin() + 38, parsed.signing_public_key);
        std::copy(script.begin() + 38, script.begin() + 70, parsed.pq_signing_key_id.begin());
        std::copy(script.begin() + 70, script.begin() + 102, parsed.pq_reward_key_id.begin());
        parsed.pq_withdrawal_key_id = pq_id;
        if (!IsValid(parsed)) return false;
        bond = parsed;
        return true;
    }
    if (script.size() < 2 || script[0] != OP_PUSHDATA1 ||
        script[1] != BOND_PAYLOAD_SIZE) {
        return false;
    }
    std::vector<unsigned char> payload;
    BondData parsed;
    if (!ParseMetadataScript(script, payload, parsed.withdrawal_key_id) ||
        payload.size() != BOND_PAYLOAD_SIZE ||
        !std::equal(BOND_MAGIC, BOND_MAGIC + sizeof(BOND_MAGIC), payload.begin())) {
        return false;
    }

    size_t offset = sizeof(BOND_MAGIC);
    std::copy(payload.begin() + offset,
              payload.begin() + offset + SCHNORR_PUBLIC_KEY_SIZE,
              parsed.signing_public_key);
    offset += SCHNORR_PUBLIC_KEY_SIZE;
    std::copy(payload.begin() + offset,
              payload.begin() + offset + VRF_PUBLIC_KEY_SIZE,
              parsed.vrf_public_key);
    offset += VRF_PUBLIC_KEY_SIZE;
    std::copy(payload.begin() + offset, payload.end(), parsed.reward_key_id.begin());
    if (!IsValid(parsed)) return false;
    bond = parsed;
    return true;
}

bool ParseUnbondScript(const CScript& script, UnbondData& unbond)
{
    uint256 pq_id;
    if (script.size() == 45 && ExtractPQKeyId(script, pq_id) &&
        script[1] == 'V' && script[3] == 'U' && script[4] == 2) {
        UnbondData parsed;
        parsed.version = 2;
        parsed.unlock_height = ReadLE32(script.data() + 5);
        parsed.pq_withdrawal_key_id = pq_id;
        if (!IsValid(parsed)) return false;
        unbond = parsed;
        return true;
    }
    if (script.empty() || script[0] != UNBOND_PAYLOAD_SIZE) {
        return false;
    }
    std::vector<unsigned char> payload;
    UnbondData parsed;
    if (!ParseMetadataScript(script, payload, parsed.withdrawal_key_id) ||
        payload.size() != UNBOND_PAYLOAD_SIZE ||
        !std::equal(UNBOND_MAGIC, UNBOND_MAGIC + sizeof(UNBOND_MAGIC), payload.begin())) {
        return false;
    }
    parsed.unlock_height = ReadLE32(payload.data() + sizeof(UNBOND_MAGIC));
    if (!IsValid(parsed)) return false;
    unbond = parsed;
    return true;
}

} // namespace pos
