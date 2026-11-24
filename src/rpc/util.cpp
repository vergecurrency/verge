// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2025 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <key_io.h>
#include <keystore.h>
#include <stealth.h>
#include <rpc/protocol.h>
#include <rpc/util.h>
#include <tinyformat.h>
#include <util/strencodings.h>
#include <algorithm>

PQMultisigPolicy ParsePQMultisigPolicy(const UniValue& required, const UniValue& addresses)
{
    if (!required.isNum() || !addresses.isArray()) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Expected numeric threshold and array of PQ addresses");
    }
    const int threshold = required.get_int();
    if (threshold < 1 || addresses.empty() || addresses.size() > PQ_MULTISIG_MAX_KEYS ||
        static_cast<unsigned int>(threshold) > addresses.size()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "PQ multisig requires 1 <= threshold <= keys <= 5");
    }
    PQMultisigPolicy policy;
    policy.required = threshold;
    for (const auto& address : addresses.getValues()) {
        if (!address.isStr()) throw JSONRPCError(RPC_TYPE_ERROR, "PQ addresses must be strings");
        const auto destination = DecodeDestination(address.get_str());
        const auto* id = boost::get<PQKeyHash>(&destination);
        if (!id || id->IsNull()) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Each cosigner must be a single-key PQ address on this network");
        }
        policy.keys.push_back(*id);
    }
    std::sort(policy.keys.begin(), policy.keys.end());
    if (EncodePQMultisigPolicy(policy).empty()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Duplicate PQ cosigner address");
    }
    return policy;
}

UniValue DescribePQMultisigPolicy(const PQMultisigPolicy& policy)
{
    const uint256 id = GetPQMultisigPolicyId(policy);
    UniValue result(UniValue::VOBJ), addresses(UniValue::VARR);
    for (const auto& key : policy.keys) addresses.push_back(EncodeDestination(PQKeyHash(key)));
    result.pushKV("address", EncodeDestination(PQPolicyHash(id)));
    result.pushKV("policy", HexStr(EncodePQMultisigPolicy(policy)));
    result.pushKV("scriptPubKey", HexStr(GetPQMultisigScript(id)));
    result.pushKV("sigsrequired", static_cast<int>(policy.required));
    result.pushKV("addresses", addresses);
    return result;
}

// Converts a hex string to a public key if possible
CPubKey HexToPubKey(const std::string& hex_in)
{
    if (!IsHex(hex_in)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid public key: " + hex_in);
    }
    CPubKey vchPubKey(ParseHex(hex_in));
    if (!vchPubKey.IsFullyValid()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid public key: " + hex_in);
    }
    return vchPubKey;
}

// Retrieves a public key for an address from the given CKeyStore
CPubKey AddrToPubKey(CKeyStore* const keystore, const std::string& addr_in)
{
    CTxDestination dest = DecodeDestination(addr_in);
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address: " + addr_in);
    }
    CKeyID key = GetKeyForDestination(*keystore, dest);
    if (key.IsNull()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, strprintf("%s does not refer to a key", addr_in));
    }
    CPubKey vchPubKey;
    if (!keystore->GetPubKey(key, vchPubKey)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, strprintf("no full public key for address %s", addr_in));
    }
    if (!vchPubKey.IsFullyValid()) {
       throw JSONRPCError(RPC_INTERNAL_ERROR, "Wallet contains an invalid public key");
    }
    return vchPubKey;
}

// Creates a multisig redeemscript from a given list of public keys and number required.
CScript CreateMultisigRedeemscript(const int required, const std::vector<CPubKey>& pubkeys)
{
    // Gather public keys
    if (required < 1) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "a multisignature address must require at least one key to redeem");
    }
    if ((int)pubkeys.size() < required) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("not enough keys supplied (got %u keys, but need at least %d to redeem)", pubkeys.size(), required));
    }
    if (pubkeys.size() > 16) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Number of keys involved in the multisignature address creation > 16\nReduce the number");
    }

    CScript result = GetScriptForMultisig(required, pubkeys);

    if (result.size() > MAX_SCRIPT_ELEMENT_SIZE) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, (strprintf("redeemScript exceeds size limit: %d > %d", result.size(), MAX_SCRIPT_ELEMENT_SIZE)));
    }

    return result;
}

class DescribeAddressVisitor : public boost::static_visitor<UniValue>
{
public:
    UniValue operator()(const PQPolicyHash& id) const
    {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("isscript", true);
        obj.pushKV("iswitness", false);
        obj.pushKV("ispostquantum", true);
        obj.pushKV("ismultisig", true);
        obj.pushKV("pq_policy_id", id.GetHex());
        return obj;
    }
    UniValue operator()(const PQKeyHash& id) const
    {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("isscript", false);
        obj.pushKV("iswitness", false);
        obj.pushKV("ispostquantum", true);
        obj.pushKV("pq_key_id", id.GetHex());
        return obj;
    }
    explicit DescribeAddressVisitor() {}

    UniValue operator()(const CNoDestination& dest) const
    {
        return UniValue(UniValue::VOBJ);
    }

    UniValue operator()(const CKeyID& keyID) const
    {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("isscript", false);
        obj.pushKV("iswitness", false);
        return obj;
    }

    UniValue operator()(const CScriptID& scriptID) const
    {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("isscript", true);
        obj.pushKV("iswitness", false);
        return obj;
    }

    UniValue operator()(const WitnessV0KeyHash& id) const
    {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("isscript", false);
        obj.pushKV("iswitness", true);
        obj.pushKV("witness_version", 0);
        obj.pushKV("witness_program", HexStr(id.begin(), id.end()));
        return obj;
    }

    UniValue operator()(const WitnessV0ScriptHash& id) const
    {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("isscript", true);
        obj.pushKV("iswitness", true);
        obj.pushKV("witness_version", 0);
        obj.pushKV("witness_program", HexStr(id.begin(), id.end()));
        return obj;
    }

    UniValue operator()(const WitnessUnknown& id) const
    {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("iswitness", true);
        obj.pushKV("witness_version", (int)id.version);
        obj.pushKV("witness_program", HexStr(id.program, id.program + id.length));
        return obj;
    }

    UniValue operator()(const CStealthAddress& id) const
    {
        UniValue obj(UniValue::VOBJ);
        obj.pushKV("isstealth", true);
        obj.pushKV("isscript", false);
        obj.pushKV("iswitness", false);
        return obj;
    }
};

UniValue DescribeAddress(const CTxDestination& dest)
{
    return boost::apply_visitor(DescribeAddressVisitor(), dest);
}
