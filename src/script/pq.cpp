// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <script/pq.h>
#include <hash.h>

#include <algorithm>

std::vector<unsigned char> EncodePQMultisigPolicy(const PQMultisigPolicy& policy)
{
    if (policy.required == 0 || policy.required > policy.keys.size() ||
        policy.keys.empty() || policy.keys.size() > PQ_MULTISIG_MAX_KEYS) return {};
    std::vector<unsigned char> encoded{1, static_cast<unsigned char>(policy.required),
                                     static_cast<unsigned char>(policy.keys.size())};
    for (size_t i = 0; i < policy.keys.size(); ++i) {
        if (policy.keys[i].IsNull() || (i && !(policy.keys[i - 1] < policy.keys[i]))) return {};
        encoded.insert(encoded.end(), policy.keys[i].begin(), policy.keys[i].end());
    }
    return encoded;
}

bool DecodePQMultisigPolicy(const std::vector<unsigned char>& encoded, PQMultisigPolicy& policy)
{
    policy = {};
    if (encoded.size() < 35 || encoded.size() > PQ_MULTISIG_MAX_POLICY_BYTES ||
        encoded[0] != 1 || encoded[2] == 0 || encoded[2] > PQ_MULTISIG_MAX_KEYS ||
        encoded.size() != 3 + size_t(encoded[2]) * 32) return false;
    PQMultisigPolicy decoded;
    decoded.required = encoded[1];
    for (size_t i = 0; i < encoded[2]; ++i) {
        uint256 id;
        std::copy(encoded.begin() + 3 + i * 32, encoded.begin() + 3 + (i + 1) * 32, id.begin());
        decoded.keys.push_back(id);
    }
    if (EncodePQMultisigPolicy(decoded) != encoded) return false;
    policy = std::move(decoded);
    return true;
}

uint256 GetPQMultisigPolicyId(const PQMultisigPolicy& policy)
{
    const auto encoded = EncodePQMultisigPolicy(policy);
    if (encoded.empty()) return {};
    CHashWriter writer(SER_GETHASH, 0);
    writer << std::string("Verge/PQMultisigPolicy/v1") << encoded;
    return writer.GetHash();
}

CScript GetPQMultisigScript(const uint256& policy_id)
{
    if (policy_id.IsNull()) return {};
    return CScript() << OP_4 << std::vector<unsigned char>(policy_id.begin(), policy_id.end()) << OP_NOP4;
}

bool ExtractPQMultisigId(const CScript& script, uint256& policy_id)
{
    policy_id.SetNull();
    if (script.size() != 35 || script[0] != OP_4 || script[1] != 32 || script[34] != OP_NOP4) return false;
    std::copy(script.begin() + 2, script.begin() + 34, policy_id.begin());
    return !policy_id.IsNull();
}

bool IsPQOutputScript(const CScript& script)
{
    uint256 id;
    return ExtractPQKeyId(script, id) || ExtractPQMultisigId(script, id);
}

CScript GetPQMultisigScriptSig(const PQMultisigPolicy& policy,
                              const std::vector<PQMultisigSignature>& signatures)
{
    const auto encoded = EncodePQMultisigPolicy(policy);
    if (encoded.empty() || signatures.size() > policy.required) return {};
    CScript script = CScript() << encoded;
    for (size_t i = 0; i < signatures.size(); ++i) {
        const auto& item = signatures[i];
        if (item.index >= policy.keys.size() || (i && signatures[i - 1].index >= item.index)) return {};
        const CScript proof = GetPQScriptSig(item.signature, item.public_key);
        if (proof.empty()) return {};
        script << static_cast<int64_t>(item.index);
        script.insert(script.end(), proof.begin(), proof.end());
    }
    return script;
}

bool DecodePQMultisigScriptSig(const CScript& script, PQMultisigPolicy& policy,
                               std::vector<PQMultisigSignature>& signatures)
{
    policy = {};
    signatures.clear();
    if (script.empty() || script.size() > PQ_MULTISIG_MAX_SCRIPTSIG_BYTES) return false;
    auto cursor = script.begin();
    opcodetype opcode;
    std::vector<unsigned char> encoded;
    PQMultisigPolicy decoded;
    if (!script.GetOp(cursor, opcode, encoded) || !DecodePQMultisigPolicy(encoded, decoded)) return false;
    std::vector<PQMultisigSignature> items;
    while (cursor != script.end()) {
        if (items.size() >= decoded.required || !script.GetOp(cursor, opcode)) return false;
        PQMultisigSignature item;
        if (opcode == OP_0) item.index = 0;
        else if (opcode >= OP_1 && opcode <= OP_5) item.index = opcode - OP_1 + 1;
        else return false;
        if (item.index >= decoded.keys.size() || (!items.empty() && items.back().index >= item.index)) return false;
        for (auto* output : {&item.signature, &item.public_key}) {
            const size_t required = output == &item.signature ? PQ_SIGNATURE_BYTES + 1 : PQ_PUBLIC_KEY_BYTES;
            for (size_t offset = 0; offset < required; offset += PQ_CHUNK_BYTES) {
                std::vector<unsigned char> chunk;
                if (!script.GetOp(cursor, opcode, chunk) || opcode > OP_PUSHDATA4 ||
                    chunk.size() != std::min(PQ_CHUNK_BYTES, required - offset)) return false;
                output->insert(output->end(), chunk.begin(), chunk.end());
            }
        }
        items.push_back(std::move(item));
    }
    if (GetPQMultisigScriptSig(decoded, items) != script) return false;
    policy = std::move(decoded);
    signatures = std::move(items);
    return true;
}

size_t GetPQMultisigMaximumScriptSigSize(const PQMultisigPolicy& policy)
{
    const auto encoded = EncodePQMultisigPolicy(policy);
    if (encoded.empty()) return 0;
    return (CScript() << encoded).size() + policy.required * (1 + 3757);
}

CScript GetPQScript(const uint256& key_id)
{
    if (key_id.IsNull()) return {};
    return CScript() << OP_3 << std::vector<unsigned char>(key_id.begin(), key_id.end()) << OP_NOP4;
}

bool ExtractPQKeyId(const CScript& script, uint256& key_id)
{
    key_id.SetNull();
    if (script.size() < 35) return false;
    const size_t prefix = script.size() - 35;
    if (prefix != 0) {
        auto cursor = script.begin();
        opcodetype opcode;
        std::vector<unsigned char> payload;
        if (!script.GetOp(cursor, opcode, payload) ||
            !script.GetOp(cursor, opcode) || opcode != OP_DROP ||
            size_t(cursor - script.begin()) != prefix) return false;
        if (payload.size() < 4 || payload[0] != 'V' || payload[1] != 'P' || payload[3] != 2 ||
            !((payload[2] == 'B' && payload.size() == 100) || (payload[2] == 'U' && payload.size() == 8))) return false;
        const CScript canonical = CScript() << payload << OP_DROP;
        if (!std::equal(canonical.begin(), canonical.end(), script.begin(), cursor)) return false;
    }
    auto begin = script.begin() + prefix;
    if (begin[0] != OP_3 || begin[1] != 32 || begin[34] != OP_NOP4) return false;
    std::copy(begin + 2, begin + 34, key_id.begin());
    return !key_id.IsNull();
}

CScript GetPQScriptSig(const std::vector<unsigned char>& signature,
                       const std::vector<unsigned char>& public_key)
{
    if (signature.size() != PQ_SIGNATURE_BYTES + 1 || public_key.size() != PQ_PUBLIC_KEY_BYTES) return {};
    CScript result;
    for (const auto* value : {&signature, &public_key}) {
        for (size_t offset = 0; offset < value->size(); offset += PQ_CHUNK_BYTES) {
            const size_t end = std::min(value->size(), offset + PQ_CHUNK_BYTES);
            result << std::vector<unsigned char>(value->begin() + offset, value->begin() + end);
        }
    }
    return result;
}

bool DecodePQScriptSig(const CScript& script,
                        std::vector<unsigned char>& signature,
                        std::vector<unsigned char>& public_key)
{
    signature.clear();
    public_key.clear();
    if (script.size() > 3800) return false;
    auto cursor = script.begin();
    for (auto* output : {&signature, &public_key}) {
        const size_t required = output == &signature ? PQ_SIGNATURE_BYTES + 1 : PQ_PUBLIC_KEY_BYTES;
        for (size_t offset = 0; offset < required; offset += PQ_CHUNK_BYTES) {
            opcodetype opcode;
            std::vector<unsigned char> chunk;
            if (!script.GetOp(cursor, opcode, chunk) || opcode > OP_PUSHDATA4 ||
                chunk.size() != std::min(PQ_CHUNK_BYTES, required - offset)) return false;
            output->insert(output->end(), chunk.begin(), chunk.end());
        }
    }
    return cursor == script.end() && GetPQScriptSig(signature, public_key) == script;
}
