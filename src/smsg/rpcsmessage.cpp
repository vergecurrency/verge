// Copyright (c) 2014-2016 The ShadowCoin developers
// Copyright (c) 2017-2018 The Particl developers
// Copyright (c) 2017-2018 The Verge Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/server.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>

#include <smsg/smessage.h>
#include <smsg/db.h>
#include <script/ismine.h>
#include <util/strencodings.h>
#include <util/time.h>
#include <core_io.h>
#include <base58.h>
#include <net.h>
#include <rpc/util.h>

#ifdef ENABLE_WALLET
#include <wallet/wallet.h>
#endif

#include <univalue.h>

#include <boost/algorithm/string.hpp>

static bool GetBool(const UniValue& value)
{
    if (value.isBool()) return value.get_bool();
    if (value.isNum()) return value.get_int() != 0;
    if (value.isStr()) {
        std::string s = value.get_str();
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
        return s == "1" || s == "true" || s == "yes" || s == "+";
    }
    return false;
}

static void PushTime(UniValue& obj, const char* key, int64_t ts)
{
    obj.pushKV(key, FormatISO8601DateTime(ts));
}

namespace part {
static bool GetStringBool(const std::string& value, bool& out)
{
    std::string s(value);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    if (s == "1" || s == "true" || s == "yes" || s == "on" || s == "+") { out = true; return true; }
    if (s == "0" || s == "false" || s == "no" || s == "off" || s == "-") { out = false; return true; }
    return false;
}

static bool stringsMatchI(const std::string& haystack, const std::string& needle, int)
{
    std::string h(haystack), n(needle);
    std::transform(h.begin(), h.end(), h.begin(), [](unsigned char c){ return std::tolower(c); });
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c){ return std::tolower(c); });
    return h.find(n) != std::string::npos;
}

static std::string GetTimeString(int64_t ts, char* buf, size_t len)
{
    std::string out = FormatISO8601DateTime(ts);
    if (buf && len > 0) {
        strncpy(buf, out.c_str(), len - 1);
        buf[len - 1] = '\0';
    }
    return out;
}

static std::string BytesReadable(uint64_t bytes)
{
    static const char* suffixes[] = {"B", "KB", "MB", "GB", "TB"};
    double size = static_cast<double>(bytes);
    size_t suffix = 0;
    while (size >= 1024.0 && suffix < 4) {
        size /= 1024.0;
        ++suffix;
    }
    std::ostringstream oss;
    if (suffix == 0) {
        oss << static_cast<uint64_t>(size) << ' ' << suffixes[suffix];
    } else {
        oss.setf(std::ios::fixed);
        oss.precision(2);
        oss << size << ' ' << suffixes[suffix];
    }
    return oss.str();
}

static int64_t strToEpoch(const char* str)
{
    if (!str) return 0;
    return ParseISO8601DateTime(str);
}
} // namespace part

static void EnsureSMSGIsEnabled()
{
    if (!smsg::fSecMsgEnabled)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Secure messaging is disabled.");
};

static size_t CountSmsgEntries(leveldb::DB* pdb, const std::string& prefix)
{
    size_t count = 0;
    std::unique_ptr<leveldb::Iterator> it(pdb->NewIterator(leveldb::ReadOptions()));
    for (it->Seek(prefix); it->Valid(); it->Next()) {
        const leveldb::Slice key = it->key();
        if (key.size() < prefix.size() || memcmp(key.data(), prefix.data(), prefix.size()) != 0) {
            break;
        }
        ++count;
    }
    return count;
}

static constexpr int SMSG_DEFAULT_RETENTION_DAYS = 31;
static constexpr size_t SMSG_MAX_SHARED_CHATKEY_LEN = 256;

static int SmsgMaxRetentionDays()
{
    return smsg::SMSG_MAX_PAID_TTL / smsg::SMSG_SECONDS_IN_DAY;
}

static bool IsSmsgPubKeyString(const std::string& publicKey)
{
    std::vector<uint8_t> keyData;
    if (IsHex(publicKey)) {
        keyData = ParseHex(publicKey);
    } else if (!DecodeBase58(publicKey, keyData)) {
        return false;
    }

    const CPubKey pubKey(keyData);
    return pubKey.IsValid() && pubKey.IsCompressed();
}

static bool SplitSharedChatkey(const std::string& valueIn, std::string& addressOut, std::string& publicKeyOut)
{
    std::string value = valueIn;
    boost::trim(value);
    if (value.empty()) {
        return false;
    }

    CBitcoinAddress plainAddress(value);
    CKeyID keyID;
    if (plainAddress.IsValid() && plainAddress.GetKeyID(keyID)) {
        addressOut = plainAddress.ToString();
        publicKeyOut.clear();
        return true;
    }
    if (value.size() > SMSG_MAX_SHARED_CHATKEY_LEN) {
        return false;
    }

    const size_t dash = value.find('-');
    if (dash == std::string::npos || dash == 0 || dash != value.rfind('-')) {
        return false;
    }

    const std::string candidateAddress = value.substr(0, dash);
    const std::string candidatePublicKey = value.substr(dash + 1);
    CBitcoinAddress address(candidateAddress);
    if (address.IsValid() && address.GetKeyID(keyID) && IsSmsgPubKeyString(candidatePublicKey)) {
        addressOut = address.ToString();
        publicKeyOut = candidatePublicKey;
        return true;
    }

    return false;
}

static void StoreSharedChatkeyIfPresent(const std::string& address, const std::string& publicKey)
{
    if (publicKey.empty()) {
        return;
    }

    std::string mutableAddress = address;
    std::string mutablePublicKey = publicKey;
    const int rv = smsgModule.AddAddress(mutableAddress, mutablePublicKey);
    if (rv != smsg::SMSG_NO_ERROR && rv != smsg::SMSG_PUBKEY_EXISTS) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid shared chatkey: " + smsg::GetString(rv));
    }
}

static UniValue smsgenable(const JSONRPCRequest &request)
{
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            "smsgenable ( \"walletname\" )\n"
            "\nArguments:\n"
            "1. \"walletname\"      (string, optional, default=\"wallet.dat\") enable smsg on a specific wallet.\n"
            "Enable secure messaging.\n"
            "SMSG only be active on one wallet.\n");

    if (smsg::fSecMsgEnabled)
        throw JSONRPCError(RPC_MISC_ERROR, "Secure messaging is already enabled.");

    UniValue result(UniValue::VOBJ);

    std::shared_ptr<CWallet> pwallet;
    std::string walletName = "none";
#ifdef ENABLE_WALLET
    auto vpwallets = GetWallets();

    if (!request.params[0].isNull())
    {
        std::string sFindWallet = request.params[0].get_str();

        for (auto pw : vpwallets)
        {
            if (pw->GetName() != sFindWallet)
                continue;
            pwallet = pw;
            break;
        };
        if (!pwallet)
            throw JSONRPCError(RPC_MISC_ERROR, "Wallet not found: \"" + sFindWallet + "\"");
    } else
    {
        if (vpwallets.size() > 0)
            pwallet = vpwallets[0];
    };
    if (pwallet)
        walletName = pwallet->GetName();
#endif

    result.pushKV("result", (smsgModule.Enable(pwallet) ? "Enabled secure messaging." : "Failed to enable secure messaging."));
    result.pushKV("wallet", walletName);

    return result;
}

static UniValue smsgdisable(const JSONRPCRequest &request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "smsgdisable\n"
            "Disable secure messaging.");

    if (!smsg::fSecMsgEnabled)
        throw JSONRPCError(RPC_MISC_ERROR, "Secure messaging is already disabled.");

    UniValue result(UniValue::VOBJ);

    result.pushKV("result", (smsgModule.Disable() ? "Disabled secure messaging." : "Failed to disable secure messaging."));

    return result;
}

static UniValue smsgoptions(const JSONRPCRequest &request)
{
    if (request.fHelp || request.params.size() > 3)
        throw std::runtime_error(
            "smsgoptions ( list with_description|set \"optname\" \"value\" )\n"
            "List and manage options.\n"
            "\nExamples\n"
            "smsgoptions list 1\n"
            " list possible options with descriptions.\n"
            );

    std::string mode = "list";
    if (request.params.size() > 0)
        mode = request.params[0].get_str();

    UniValue result(UniValue::VOBJ);

    if (mode == "list")
    {
        UniValue options(UniValue::VARR);

        bool fDescriptions = false;
        if (!request.params[1].isNull())
            fDescriptions = GetBool(request.params[1]);

        UniValue option(UniValue::VOBJ);
        option.pushKV("name", "newAddressRecv");
        option.pushKV("value", smsgModule.options.fNewAddressRecv);
        if (fDescriptions)
            option.pushKV("description", "Enable receiving messages for newly created addresses.");
        options.push_back(option);

        option = UniValue(UniValue::VOBJ);
        option.pushKV("name", "newAddressAnon");
        option.pushKV("value", smsgModule.options.fNewAddressAnon);
        if (fDescriptions)
            option.pushKV("description", "Enable receiving anonymous messages for newly created addresses.");
        options.push_back(option);

        option = UniValue(UniValue::VOBJ);
        option.pushKV("name", "scanIncoming");
        option.pushKV("value", smsgModule.options.fScanIncoming);
        if (fDescriptions)
            option.pushKV("description", "Scan incoming blocks for public keys, -smsgscanincoming must also be set");
        options.push_back(option);

        result.pushKV("options", options);
        result.pushKV("result", "Success.");
    } else
    if (mode == "set")
    {
        if (request.params.size() < 3)
        {
            result.pushKV("result", "Too few parameters.");
            result.pushKV("expected", "set <optname> <value>");
            return result;
        };

        std::string optname = request.params[1].get_str();
        std::string value   = request.params[2].get_str();

        std::transform(optname.begin(), optname.end(), optname.begin(), ::tolower);

        bool fValue;
        if (optname == "newaddressrecv")
        {
            if (part::GetStringBool(value, fValue))
            {
                smsgModule.options.fNewAddressRecv = fValue;
            } else
            {
                result.pushKV("result", "Unknown value.");
                return result;
            };
            result.pushKV("set option", std::string("newAddressRecv = ") + (smsgModule.options.fNewAddressRecv ? "true" : "false"));
        } else
        if (optname == "newaddressanon")
        {
            if (part::GetStringBool(value, fValue))
            {
                smsgModule.options.fNewAddressAnon = fValue;
            } else
            {
                result.pushKV("result", "Unknown value.");
                return result;
            };
            result.pushKV("set option", std::string("newAddressAnon = ") + (smsgModule.options.fNewAddressAnon ? "true" : "false"));
        } else
        if (optname == "scanincoming")
        {
            if (part::GetStringBool(value, fValue))
            {
                smsgModule.options.fScanIncoming = fValue;
            } else
            {
                result.pushKV("result", "Unknown value.");
                return result;
            };
            result.pushKV("set option", std::string("scanIncoming = ") + (smsgModule.options.fScanIncoming ? "true" : "false"));
        } else
        {
            result.pushKV("result", "Option not found.");
            return result;
        };
    } else
    {
        result.pushKV("result", "Unknown Mode.");
        result.pushKV("expected", "smsgoptions [list|set <optname> <value>]");
    };

    return result;
}

static UniValue smsglocalkeys(const JSONRPCRequest &request)
{
    if (request.fHelp || request.params.size() > 3)
        throw std::runtime_error(
            "smsglocalkeys ( whitelist|all|wallet|recv +/- \"address\"|anon +/- \"address\" )\n"
            "List and manage keys.");

    EnsureSMSGIsEnabled();

    UniValue result(UniValue::VOBJ);

    std::string mode = "whitelist";
    if (request.params.size() > 0)
    {
        mode = request.params[0].get_str();
    };

    if (mode == "whitelist"
        || mode == "all")
    {
        LOCK(smsgModule.cs_smsg);
        uint32_t nKeys = 0;
        int all = mode == "all" ? 1 : 0;

        UniValue keys(UniValue::VARR);
#ifdef ENABLE_WALLET
        if (smsgModule.pwallet)
        {
            for (auto it = smsgModule.addresses.begin(); it != smsgModule.addresses.end(); ++it)
            {
                if (!all
                    && !it->fReceiveEnabled)
                    continue;

                CKeyID &keyID = it->address;
                std::string sPublicKey;
                CPubKey pubKey;

                if (smsgModule.pwallet)
                {
                    if (!smsgModule.pwallet->GetPubKey(keyID, pubKey))
                        continue;
                    if (!pubKey.IsValid()
                        || !pubKey.IsCompressed())
                    {
                        continue;
                    };
                    sPublicKey = EncodeBase58(pubKey.begin(), pubKey.end());
                };

                UniValue objM(UniValue::VOBJ);
                std::string sLabel = smsgModule.pwallet->mapAddressBook[keyID].name;
                std::string sInfo;
                if (all)
                    sInfo = std::string("Receive ") + (it->fReceiveEnabled ? "on,  " : "off, ");
                sInfo += std::string("Anon ") + (it->fReceiveAnon ? "on" : "off");
                //result.pushKV("key", it->sAddress + " - " + sPublicKey + " " + sInfo + " - " + sLabel);
                const std::string address = CBitcoinAddress(keyID).ToString();
                objM.pushKV("address", address);
                objM.pushKV("public_key", sPublicKey);
                objM.pushKV("chatkey", address + "-" + sPublicKey);
                objM.pushKV("receive", (it->fReceiveEnabled ? "1" : "0"));
                objM.pushKV("anon", (it->fReceiveAnon ? "1" : "0"));
                objM.pushKV("label", sLabel);
                keys.push_back(objM);

                nKeys++;
            };
            result.pushKV("wallet_keys", keys);
        };
#endif

        keys = UniValue(UniValue::VARR);
        for (auto &p : smsgModule.keyStore.mapKeys)
        {
            auto &key = p.second;
            UniValue objM(UniValue::VOBJ);
            CPubKey pk = key.key.GetPubKey();
            const std::string address = CBitcoinAddress(p.first).ToString();
            const std::string publicKey = EncodeBase58(pk.begin(), pk.end());
            objM.pushKV("address", address);
            objM.pushKV("public_key", publicKey);
            objM.pushKV("chatkey", address + "-" + publicKey);
            objM.pushKV("receive", (key.nFlags & smsg::SMK_RECEIVE_ON ? "1" : "0"));
            objM.pushKV("anon", (key.nFlags & smsg::SMK_RECEIVE_ANON ? "1" : "0"));
            objM.pushKV("label", key.sLabel);
            keys.push_back(objM);

            nKeys++;
        };
        result.pushKV("smsg_keys", keys);

        result.pushKV("result", strprintf("%u", nKeys));
    } else
    if (mode == "recv")
    {
        if (request.params.size() < 3)
        {
            result.pushKV("result", "Too few parameters.");
            result.pushKV("expected", "recv <+/-> <address>");
            return result;
        };

        std::string op      = request.params[1].get_str();
        std::string addr    = request.params[2].get_str();

        bool fValue;
        if (!part::GetStringBool(op, fValue))
        {
            result.pushKV("result", "Unknown value.");
            return result;
        };

        CKeyID keyID;
        CBitcoinAddress coinAddress(addr);
        if (!coinAddress.IsValid())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address.");
        if (!coinAddress.GetKeyID(keyID))
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address.");

        if (!smsgModule.SetWalletAddressOption(keyID, "receive", fValue)
            && !smsgModule.SetSmsgAddressOption(keyID, "receive", fValue))
        {
            result.pushKV("result", "Address not found.");
            return result;
        };

        std::string sInfo;
        sInfo = std::string("Receive ") + (fValue ? "on" : "off");
        result.pushKV("result", "Success.");
        result.pushKV("key", coinAddress.ToString() + " " + sInfo);
        return result;
    } else
    if (mode == "anon")
    {
        if (request.params.size() < 3)
        {
            result.pushKV("result", "Too few parameters.");
            result.pushKV("expected", "anon <+/-> <address>");
            return result;
        };

        std::string op      = request.params[1].get_str();
        std::string addr    = request.params[2].get_str();

        bool fValue;
        if (!part::GetStringBool(op, fValue))
        {
            result.pushKV("result", "Unknown value.");
            return result;
        };

        CKeyID keyID;
        CBitcoinAddress coinAddress(addr);
        if (!coinAddress.IsValid())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address.");
        if (!coinAddress.GetKeyID(keyID))
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address.");

        if (!smsgModule.SetWalletAddressOption(keyID, "anon", fValue)
            && !smsgModule.SetSmsgAddressOption(keyID, "anon", fValue))
        {
            result.pushKV("result", "Address not found.");
            return result;
        };

        std::string sInfo;
        sInfo += std::string("Anon ") + (fValue ? "on" : "off");
        result.pushKV("result", "Success.");
        result.pushKV("key", coinAddress.ToString() + " " + sInfo);
        return result;

    } else
    if (mode == "wallet")
    {
#ifdef ENABLE_WALLET
        if (!smsgModule.pwallet)
            throw JSONRPCError(RPC_MISC_ERROR, "No wallet.");
        uint32_t nKeys = 0;
        UniValue keys(UniValue::VOBJ);

        for (const auto &entry : smsgModule.pwallet->mapAddressBook)
        {
            if (!IsMine(*smsgModule.pwallet, entry.first))
                continue;

            CBitcoinAddress coinAddress(entry.first);
            if (!coinAddress.IsValid())
                continue;

            std::string address = coinAddress.ToString();
            std::string sPublicKey;

            CKeyID keyID;
            if (!coinAddress.GetKeyID(keyID))
                continue;

            CPubKey pubKey;
            if (!smsgModule.pwallet->GetPubKey(keyID, pubKey))
                continue;
            if (!pubKey.IsValid()
                || !pubKey.IsCompressed())
            {
                continue;
            };

            sPublicKey = EncodeBase58(pubKey.begin(), pubKey.end());
            UniValue objM(UniValue::VOBJ);

            objM.pushKV("key", address);
            objM.pushKV("publickey", sPublicKey);
            objM.pushKV("label", entry.second.name);

            keys.push_back(objM);
            nKeys++;
        };
        result.pushKV("keys", keys);
        result.pushKV("result", strprintf("%u", nKeys));
#else
        throw JSONRPCError(RPC_MISC_ERROR, "No wallet.");
#endif
    } else
    {
        result.pushKV("result", "Unknown Mode.");
        result.pushKV("expected", "smsglocalkeys [whitelist|all|wallet|recv <+/-> <address>|anon <+/-> <address>]");
    };

    return result;
};

static UniValue smsgscanchain(const JSONRPCRequest &request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "smsgscanchain\n"
            "Look for public keys in the block chain.");

    EnsureSMSGIsEnabled();

    UniValue result(UniValue::VOBJ);
    if (!smsgModule.ScanBlockChain())
    {
        result.pushKV("result", "Scan Chain Failed.");
    } else
    {
        result.pushKV("result", "Scan Chain Completed.");
    };
    return result;
}

static UniValue smsgscanbuckets(const JSONRPCRequest &request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "smsgscanbuckets\n"
            "Force rescan of all messages in the bucket store.\n"
            "Wallet must be unlocked if any receiving keys are stored in the wallet.\n");

    EnsureSMSGIsEnabled();

#ifdef ENABLE_WALLET
    if (smsgModule.pwallet && smsgModule.pwallet->IsLocked()
        && smsgModule.addresses.size() > 0)
        throw JSONRPCError(RPC_MISC_ERROR, "Wallet is locked.");
#endif

    UniValue result(UniValue::VOBJ);
    if (!smsgModule.ScanBuckets())
    {
        result.pushKV("result", "Scan Buckets Failed.");
    } else
    {
        result.pushKV("result", "Scan Buckets Completed.");
    };

    return result;
}

static UniValue smsginfo(const JSONRPCRequest &request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "smsginfo\n"
            "\nReturn secure messaging runtime, storage, queue, and peer information.\n");

    UniValue result(UniValue::VOBJ);
    result.pushKV("enabled", smsg::fSecMsgEnabled);

    const uint64_t storageUsed = smsgModule.GetLocalStorageUsageBytes();
    result.pushKV("storage_used_bytes", storageUsed);
    result.pushKV("storage_used", part::BytesReadable(storageUsed));
    result.pushKV("storage_cap_bytes", static_cast<uint64_t>(smsg::SMSG_LOCAL_STORAGE_CAP_BYTES));
    result.pushKV("storage_cap", part::BytesReadable(smsg::SMSG_LOCAL_STORAGE_CAP_BYTES));

    UniValue policy(UniValue::VOBJ);
    policy.pushKV("paid_required", true);
    policy.pushKV("default_retention_days", SMSG_DEFAULT_RETENTION_DAYS);
    policy.pushKV("max_retention_days", SmsgMaxRetentionDays());
    policy.pushKV("max_message_bytes", static_cast<uint64_t>(smsg::SMSG_MAX_MSG_BYTES_PAID));
    policy.pushKV("marker_fee", ValueFromAmount(smsg::SMSG_PAID_MSG_FEE));
    policy.pushKV("marker_fee_atoms", static_cast<int64_t>(smsg::SMSG_PAID_MSG_FEE));
    result.pushKV("policy", policy);

    UniValue options(UniValue::VOBJ);
    options.pushKV("newAddressRecv", smsgModule.options.fNewAddressRecv);
    options.pushKV("newAddressAnon", smsgModule.options.fNewAddressAnon);
    options.pushKV("scanIncoming", smsgModule.options.fScanIncoming);
    result.pushKV("options", options);

    UniValue peers(UniValue::VOBJ);
    int totalPeers = 0;
    int smsgServicePeers = 0;
    int smsgEnabledPeers = 0;
    int ignoredPeers = 0;
    UniValue peerList(UniValue::VARR);
    if (g_connman) {
        g_connman->ForEachNode([&](CNode* pnode) {
            ++totalPeers;

            const uint64_t services = static_cast<uint64_t>(pnode->nServices.load());
            const bool advertisesSmsg = (services & NODE_SMSG) != 0;
            if (advertisesSmsg) {
                ++smsgServicePeers;
            }

            bool enabled = false;
            int64_t ignoreUntil = 0;
            int64_t lastSeen = 0;
            int64_t lastMatched = 0;
            uint32_t rateMessages = 0;
            uint32_t rateBytes = 0;
            uint32_t inventoryMismatches = 0;
            size_t requestedBuckets = 0;
            size_t wantedBuckets = 0;
            size_t wantedTokens = 0;
            {
                LOCK(pnode->smsgData.cs_smsg_net);
                enabled = pnode->smsgData.fEnabled;
                ignoreUntil = pnode->smsgData.ignoreUntil;
                lastSeen = pnode->smsgData.lastSeen;
                lastMatched = pnode->smsgData.lastMatched;
                rateMessages = pnode->smsgData.nRateMessages;
                rateBytes = pnode->smsgData.nRateBytes;
                inventoryMismatches = pnode->smsgData.nInventoryMismatches;
                requestedBuckets = pnode->smsgData.requestedBuckets.size();
                wantedBuckets = pnode->smsgData.wantedTokens.size();
                for (const auto& entry : pnode->smsgData.wantedTokens) {
                    wantedTokens += entry.second.size();
                }
            }
            if (enabled) {
                ++smsgEnabledPeers;
            }
            if (ignoreUntil > GetTime()) {
                ++ignoredPeers;
            }

            UniValue peer(UniValue::VOBJ);
            peer.pushKV("id", static_cast<int64_t>(pnode->GetId()));
            peer.pushKV("address", pnode->addr.ToString());
            peer.pushKV("services", strprintf("%016x", services));
            peer.pushKV("advertises_smsg", advertisesSmsg);
            peer.pushKV("enabled", enabled);
            peer.pushKV("ignored", ignoreUntil > GetTime());
            peer.pushKV("ignore_until", ignoreUntil);
            peer.pushKV("last_seen", lastSeen);
            peer.pushKV("last_matched", lastMatched);
            peer.pushKV("rate_messages", static_cast<uint64_t>(rateMessages));
            peer.pushKV("rate_bytes", static_cast<uint64_t>(rateBytes));
            peer.pushKV("inventory_mismatches", static_cast<uint64_t>(inventoryMismatches));
            peer.pushKV("requested_buckets", static_cast<uint64_t>(requestedBuckets));
            peer.pushKV("wanted_buckets", static_cast<uint64_t>(wantedBuckets));
            peer.pushKV("wanted_tokens", static_cast<uint64_t>(wantedTokens));
            peerList.push_back(peer);
        });
    }
    peers.pushKV("total", totalPeers);
    peers.pushKV("advertising_smsg", smsgServicePeers);
    peers.pushKV("enabled", smsgEnabledPeers);
    peers.pushKV("ignored", ignoredPeers);
    peers.pushKV("details", peerList);
    result.pushKV("peers", peers);

    UniValue counts(UniValue::VOBJ);
    counts.pushKV("local_keys", static_cast<uint64_t>(smsgModule.keyStore.Count()));

    uint64_t bucketCount = 0;
    uint64_t bucketMessages = 0;
    {
        LOCK(smsgModule.cs_smsg);
        bucketCount = smsgModule.buckets.size();
        for (const auto& entry : smsgModule.buckets) {
            bucketMessages += entry.second.setTokens.size();
        }
    }
    counts.pushKV("buckets", bucketCount);
    counts.pushKV("bucket_messages", bucketMessages);
    counts.pushKV("paid_funding_index", static_cast<uint64_t>(smsgModule.GetPaidFundingIndexSize()));
    counts.pushKV("recent_rejected_tokens", static_cast<uint64_t>(smsgModule.GetRecentRejectedTokenCount()));

    {
        LOCK(smsg::cs_smsgDB);
        smsg::SecMsgDB db;
        if (db.Open("cr+")) {
            counts.pushKV("inbox", static_cast<uint64_t>(CountSmsgEntries(db.pdb, "im")));
            counts.pushKV("outbox", static_cast<uint64_t>(CountSmsgEntries(db.pdb, "sm")));
            counts.pushKV("send_queue", static_cast<uint64_t>(CountSmsgEntries(db.pdb, "qm")));
            counts.pushKV("purged", static_cast<uint64_t>(CountSmsgEntries(db.pdb, "pm")));
            counts.pushKV("paid_funding_records", static_cast<uint64_t>(CountSmsgEntries(db.pdb, "fm")));
            counts.pushKV("known_pubkeys", static_cast<uint64_t>(CountSmsgEntries(db.pdb, "pk")));
            counts.pushKV("stored_private_keys", static_cast<uint64_t>(CountSmsgEntries(db.pdb, "sk")));
        }
    }
    result.pushKV("counts", counts);
    result.pushKV("result", "Success.");
    return result;
}

static UniValue smsgaddaddress(const JSONRPCRequest &request)
{
    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "smsgaddaddress \"address\" \"pubkey\"\n"
            "Add address and matching public key to database.");

    EnsureSMSGIsEnabled();

    std::string addr = request.params[0].get_str();
    std::string pubk = request.params[1].get_str();

    UniValue result(UniValue::VOBJ);
    int rv = smsgModule.AddAddress(addr, pubk);
    if (rv != 0)
    {
        result.pushKV("result", "Public key not added to db.");
        result.pushKV("reason", smsg::GetString(rv));
    } else
    {
        result.pushKV("result", "Public key added to db.");
    };

    return result;
}

static UniValue smsgaddlocaladdress(const JSONRPCRequest &request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "smsgaddlocaladdress \"address\"\n"
            "Enable receiving messages on <address>.\n"
            "Key for \"address\" must exist in the wallet.");

    EnsureSMSGIsEnabled();

    std::string addr = request.params[0].get_str();

    UniValue result(UniValue::VOBJ);
    int rv = smsgModule.AddLocalAddress(addr);
    if (rv != 0)
    {
        result.pushKV("result", "Address not added.");
        result.pushKV("reason", smsg::GetString(rv));
    } else
    {
        result.pushKV("result", "Receiving messages enabled for address.");
    };

    return result;
}

static UniValue smsgimportprivkey(const JSONRPCRequest &request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error(
            "smsgimportprivkey \"privkey\" ( \"label\" )\n"
            "\nAdds a private key (as returned by dumpprivkey) to the smsg database.\n"
            "\nThe imported key can receive messages even if the wallet is locked.\n"
            "\nArguments:\n"
            "1. \"privkey\"          (string, required) The private key (see dumpprivkey)\n"
            "2. \"label\"            (string, optional, default=\"\") An optional label\n"
            "\nExamples:\n"
            "\nDump a private key\n"
            + HelpExampleCli("dumpprivkey", "\"myaddress\"") +
            "\nImport the private key\n"
            + HelpExampleCli("smsgimportprivkey", "\"mykey\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("smsgimportprivkey", "\"mykey\", \"testing\"")
        );

    EnsureSMSGIsEnabled();

    CKey key;
    if (!request.params[0].isStr())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key encoding");

    key = DecodeSecret(request.params[0].get_str());
    if (!key.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key encoding");

    std::string strLabel = "";
    if (!request.params[1].isNull())
        strLabel = request.params[1].get_str();

    int rv = smsgModule.ImportPrivkey(key, strLabel);
    if (0 != rv)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Import failed.");

    return NullUniValue;
}

static UniValue smsggetpubkey(const JSONRPCRequest &request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "smsggetpubkey \"address\"\n"
            "Return the base58 encoded compressed public key for an address.\n"
            "\nArguments:\n"
            "1. \"address\"          (string, required) The address to find the pubkey for.\n"
            "\nResult:\n"
            "{\n"
            "  \"address\": \"...\"             (string) address of public key\n"
            "  \"publickey\": \"...\"           (string) public key of address\n"
            "  \"chatkey\": \"...\"             (string) shareable address-public key line\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("smsggetpubkey", "\"myaddress\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("smsggetpubkey", "\"myaddress\""));

    EnsureSMSGIsEnabled();

    std::string address = request.params[0].get_str();
    std::string publicKey;

    UniValue result(UniValue::VOBJ);
    int rv = smsgModule.GetLocalPublicKey(address, publicKey);
    switch (rv)
    {
        case smsg::SMSG_NO_ERROR:
            result.pushKV("address", address);
            result.pushKV("publickey", publicKey);
            result.pushKV("chatkey", address + "-" + publicKey);
            return result; // success, don't check db
        case smsg::SMSG_WALLET_NO_PUBKEY:
            break; // check db
        //case 1:
        default:
            throw JSONRPCError(RPC_INTERNAL_ERROR, smsg::GetString(rv));
    };

    CBitcoinAddress coinAddress(address);
    CKeyID keyID;
    if (!coinAddress.GetKeyID(keyID))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid address.");

    CPubKey cpkFromDB;
    rv = smsgModule.GetStoredKey(keyID, cpkFromDB);

    switch (rv)
    {
        case smsg::SMSG_NO_ERROR:
            if (!cpkFromDB.IsValid()
                || !cpkFromDB.IsCompressed())
            {
                throw JSONRPCError(RPC_INTERNAL_ERROR, "Invalid public key.");
            } else
            {
                publicKey = EncodeBase58(cpkFromDB.begin(), cpkFromDB.end());

                result.pushKV("address", address);
                result.pushKV("publickey", publicKey);
                result.pushKV("chatkey", address + "-" + publicKey);
            };
            break;
        case smsg::SMSG_PUBKEY_NOT_EXISTS:
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Address not found in wallet or db.");
        default:
            throw JSONRPCError(RPC_INTERNAL_ERROR, smsg::GetString(rv));
    };

    return result;
}

static UniValue smsgsend(const JSONRPCRequest &request)
{
    if (request.fHelp || request.params.size() < 3 || request.params.size() > 8)
        throw std::runtime_error(
            "smsgsend \"address_from\" \"address_to|shared_chatkey\" \"message\" ( paid_msg days_retention testfee )\n"
            "Send a paid secure message from \"address_from\" to \"address_to\".\n"
            "\nArguments:\n"
            "1. \"address_from\"       (string, required) The address of the sender.\n"
            "2. \"address_to\"         (string, required) The recipient address or shared \"address-publickey\" chatkey line.\n"
            "3. \"message\"            (string, required) The message.\n"
            "4. paid_msg             (bool, optional, default=true) Must be true; unpaid SMSG is disabled.\n"
            "5. days_retention       (int, optional, default=31) Days the message will be retained by network.\n"
            "6. testfee              (bool, optional, default=false) Don't send the message, only estimate the fee.\n"
            "7. fromfile             (bool, optional, default=false) Send file as message, path specified in \"message\".\n"
            "8. decodehex            (bool, optional, default=false) Decode \"message\" from hex before sending.\n"
            "\nResult:\n"
            "{\n"
            "  \"result\": \"Sent\"/\"Not Sent\"       (string) address of public key\n"
            "  \"msgid\": \"...\"                    (string) if sent, a message identifier\n"
            "  \"txid\": \"...\"                     (string) the txnid of the funding txn\n"
            "  \"fee\": n                          (amount) the marker amount paid\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("smsgsend", "\"myaddress\" \"toaddress\" \"message\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("smsgsend", "\"myaddress\", \"toaddress\", \"message\""));

    EnsureSMSGIsEnabled();

    RPCTypeCheck(request.params,
        {UniValue::VSTR, UniValue::VSTR, UniValue::VSTR,
         UniValue::VBOOL, UniValue::VNUM, UniValue::VBOOL}, true);

    std::string addrFrom  = request.params[0].get_str();
    std::string addrTo    = request.params[1].get_str();
    std::string msg       = request.params[2].get_str();

    bool fPaid = true;
    if (!request.params[3].isNull() && !request.params[3].get_bool()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Unpaid SMSG is disabled; paid SMSG is required.");
    }
    int nRetention = request.params[4].isNull() ? SMSG_DEFAULT_RETENTION_DAYS : request.params[4].get_int();
    bool fTestFee = request.params[5].isNull() ? false : request.params[5].get_bool();
    bool fFromFile = request.params[6].isNull() ? false : request.params[6].get_bool();
    bool fDecodeHex = request.params[7].isNull() ? false : request.params[7].get_bool();

    if (fFromFile && fDecodeHex)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Can't use decodehex with fromfile.");

    if (fDecodeHex)
    {
        if (!IsHex(msg))
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Expect hex encoded message with decodehex.");
        std::vector<uint8_t> vData = ParseHex(msg);
        msg = std::string(vData.begin(), vData.end());
    };

    CAmount nFee;

    CKeyID kiFrom, kiTo;
    CBitcoinAddress coinAddress(addrFrom);
    if (!coinAddress.IsValid() || !coinAddress.GetKeyID(kiFrom))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid from address.");
    std::string publicKeyTo;
    if (!SplitSharedChatkey(addrTo, addrTo, publicKeyTo)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid to address or shared chatkey.");
    }
    StoreSharedChatkeyIfPresent(addrTo, publicKeyTo);

    coinAddress.SetString(addrTo);
    if (!coinAddress.IsValid() || !coinAddress.GetKeyID(kiTo))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid to address.");


    UniValue result(UniValue::VOBJ);
    std::string sError;
    smsg::SecureMessage smsgOut;
    if (smsgModule.Send(kiFrom, kiTo, msg, smsgOut, sError, fPaid, nRetention, fTestFee, &nFee, fFromFile) != 0)
    {
        result.pushKV("result", "Send failed.");
        result.pushKV("error", sError);
    } else
    {
        result.pushKV("result", fTestFee ? "Not Sent." : "Sent.");

        if (!fTestFee)
            result.pushKV("msgid", HexStr(smsgModule.GetMsgID(smsgOut)));

        if (!fTestFee)
        {
            uint256 txid;
            smsgOut.GetFundingTxid(txid);
            result.pushKV("txid", txid.ToString());
        };
        result.pushKV("fee", ValueFromAmount(nFee));
    };

    return result;
}

static UniValue smsgsendanon(const JSONRPCRequest &request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 3)
        throw std::runtime_error(
            "smsgsendanon \"address_to|shared_chatkey\" \"message\" ( days_retention )\n"
            "DEPRECATED. Send a paid anonymous encrypted message to addrTo.\n"
            "\nArguments:\n"
            "1. \"address_to\"         (string, required) The recipient address or shared \"address-publickey\" chatkey line.\n"
            "2. \"message\"            (string, required) The message.\n"
            "3. days_retention       (int, optional, default=31) Days the message will be retained by network.\n");

    EnsureSMSGIsEnabled();

    RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::VSTR, UniValue::VNUM}, true);

    std::string addrTo    = request.params[0].get_str();
    std::string msg       = request.params[1].get_str();
    int nRetention = request.params[2].isNull() ? SMSG_DEFAULT_RETENTION_DAYS : request.params[2].get_int();

    CKeyID kiFrom, kiTo;
    std::string publicKeyTo;
    if (!SplitSharedChatkey(addrTo, addrTo, publicKeyTo)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid to address or shared chatkey.");
    }
    StoreSharedChatkeyIfPresent(addrTo, publicKeyTo);

    CBitcoinAddress coinAddress(addrTo);
    if (!coinAddress.IsValid() || !coinAddress.GetKeyID(kiTo))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid to address.");

    UniValue result(UniValue::VOBJ);
    std::string sError;
    smsg::SecureMessage smsgOut;
    CAmount nFee = 0;
    if (smsgModule.Send(kiFrom, kiTo, msg, smsgOut, sError, true, nRetention, false, &nFee) != 0)
    {
        result.pushKV("result", "Send failed.");
        result.pushKV("error", sError);
    } else
    {
        result.pushKV("msgid", HexStr(smsgModule.GetMsgID(smsgOut)));
        uint256 txid;
        smsgOut.GetFundingTxid(txid);
        result.pushKV("txid", txid.ToString());
        result.pushKV("fee", ValueFromAmount(nFee));
        result.pushKV("result", "Sent.");
    };

    return result;
}

static UniValue smsginbox(const JSONRPCRequest &request)
{
    if (request.fHelp || request.params.size() > 2)
        throw std::runtime_error(
            "smsginbox ( \"mode\" \"filter\" )\n"
            "Decrypt and display received messages.\n"
            "Warning: clear will delete all messages.\n"
            "\nArguments:\n"
            "1. \"mode\"    (string, optional, default=\"unread\") \"all|unread|clear\" List all messages, unread messages or clear all messages.\n"
            "2. \"filter\"  (string, optional) Filter messages when in list mode. Applied to from, to and text fields.\n"
            "\nResult:\n"
            "{\n"
            "  \"msgid\": \"str\"                    (string) The message identifier\n"
            "  \"received\": \"time\"                (string) Time the message was received\n"
            "  \"sent\": \"time\"                    (string) Time the message was sent\n"
            "  \"daysretention\": int              (int) Number of days message will stay in the network for\n"
            "  \"from\": \"str\"                     (string) Address the message was sent from\n"
            "  \"to\": \"str\"                       (string) Address the message was sent to\n"
            "  \"text\": \"str\"                     (string) Message text\n"
            "}\n");

    EnsureSMSGIsEnabled();

    RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::VSTR}, true);

    std::string mode = request.params[0].isStr() ? request.params[0].get_str() : "unread";
    std::string filter = request.params[1].isStr() ? request.params[1].get_str() : "";

    UniValue result(UniValue::VOBJ);

    {
        LOCK(smsg::cs_smsgDB);

        smsg::SecMsgDB dbInbox;
        if (!dbInbox.Open("cr+"))
            throw std::runtime_error("Could not open DB.");

        uint32_t nMessages = 0;
        std::string sPrefix("im");
        uint8_t chKey[30];

        if (mode == "clear")
        {
            dbInbox.TxnBegin();

            leveldb::Iterator *it = dbInbox.pdb->NewIterator(leveldb::ReadOptions());
            while (dbInbox.NextSmesgKey(it, sPrefix, chKey))
            {
                dbInbox.EraseSmesg(chKey);
                nMessages++;
            };
            delete it;
            dbInbox.TxnCommit();

            result.pushKV("result", strprintf("Deleted %u messages.", nMessages));
        } else
        if (mode == "all"
            || mode == "unread")
        {
            int fCheckReadStatus = mode == "unread" ? 1 : 0;

            smsg::SecMsgStored smsgStored;
            smsg::MessageData msg;

            dbInbox.TxnBegin();

            leveldb::Iterator *it = dbInbox.pdb->NewIterator(leveldb::ReadOptions());
            UniValue messageList(UniValue::VARR);

            while (dbInbox.NextSmesg(it, sPrefix, chKey, smsgStored))
            {
                if (fCheckReadStatus
                    && !(smsgStored.status & SMSG_MASK_UNREAD))
                    continue;
                uint8_t *pHeader = &smsgStored.vchMessage[0];
                const smsg::SecureMessage *psmsg = (smsg::SecureMessage*) pHeader;

                UniValue objM(UniValue::VOBJ);
                objM.pushKV("msgid", HexStr(&chKey[2], &chKey[2] + 28)); // timestamp+hash

                uint32_t nPayload = smsgStored.vchMessage.size() - smsg::SMSG_HDR_LEN;
                int rv = smsgModule.Decrypt(false, smsgStored.addrTo, pHeader, pHeader + smsg::SMSG_HDR_LEN, nPayload, msg);
                if (rv == 0)
                {
                    std::string sAddrTo = CBitcoinAddress(smsgStored.addrTo).ToString();
                    std::string sText = std::string((char*)msg.vchMessage.data());
                    if (filter.size() > 0
                        && !(part::stringsMatchI(msg.sFromAddress, filter, 3) ||
                            part::stringsMatchI(sAddrTo, filter, 3) ||
                            part::stringsMatchI(sText, filter, 3)))
                        continue;

                    PushTime(objM, "received", smsgStored.timeReceived);
                    PushTime(objM, "sent", msg.timestamp);
                    objM.pushKV("paid", UniValue(psmsg->IsPaidVersion()));

                    uint32_t nDaysRetention = psmsg->IsPaidVersion() ? psmsg->nonce[0] : 2;
                    int64_t ttl = smsg::SMSGGetSecondsInDay() * nDaysRetention;
                    objM.pushKV("daysretention", (int)nDaysRetention);
                    PushTime(objM, "expiration", psmsg->timestamp + ttl);

                    uint32_t nPayload = smsgStored.vchMessage.size() - smsg::SMSG_HDR_LEN;
                    objM.pushKV("payloadsize", (int)nPayload);

                    objM.pushKV("from", msg.sFromAddress);
                    objM.pushKV("to", sAddrTo);
                    objM.pushKV("text", sText);
                } else
                {
                    if (filter.size() > 0)
                        continue;

                    objM.pushKV("status", "Decrypt failed");
                    objM.pushKV("error", smsg::GetString(rv));
                };

                messageList.push_back(objM);

                // only set 'read' status if the message decrypted successfully
                if (fCheckReadStatus && rv == 0)
                {
                    smsgStored.status &= ~SMSG_MASK_UNREAD;
                    dbInbox.WriteSmesg(chKey, smsgStored);
                };
                nMessages++;
            };
            delete it;
            dbInbox.TxnCommit();

            result.pushKV("messages", messageList);
            result.pushKV("result", strprintf("%u", nMessages));
        } else
        {
            result.pushKV("result", "Unknown Mode.");
            result.pushKV("expected", "all|unread|clear.");
        };
    } // cs_smsgDB

    return result;
};

static UniValue flushsmgsdb(const JSONRPCRequest &request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "flushsmgsdb\n"
            "\nWipe locally stored secure messages, queued messages, bucket files, and purged markers.\n"
            "Preserves local secure messaging keys and known public keys.\n");

    EnsureSMSGIsEnabled();

    std::string error;
    const int rv = smsgModule.FlushMessageData(error);
    if (rv != smsg::SMSG_NO_ERROR) {
        throw JSONRPCError(RPC_MISC_ERROR, error.empty() ? smsg::GetString(rv) : error);
    }

    UniValue result(UniValue::VOBJ);
    result.pushKV("result", "Local secure message storage flushed.");
    result.pushKV("cap_bytes", static_cast<uint64_t>(smsg::SMSG_LOCAL_STORAGE_CAP_BYTES));
    return result;
};

static UniValue smsgoutbox(const JSONRPCRequest &request)
{
    if (request.fHelp || request.params.size() > 2)
        throw std::runtime_error(
            "smsgoutbox ( \"mode\" \"filter\" )\n"
            "Decrypt and display all sent messages.\n"
            "Warning: \"mode\"=\"clear\" will delete all sent messages.\n"
            "\nArguments:\n"
            "1. \"mode\"    (string, optional, default=\"all\") \"all|clear\" List or clear messages.\n"
            "2. \"filter\"  (string, optional) Filter messages when in list mode. Applied to from, to and text fields.\n"
            "\nResult:\n"
            "{\n"
            "  \"msgid\": \"str\"                    (string) The message identifier\n"
            "  \"sent\": \"time\"                    (string) Time the message was sent\n"
            "  \"from\": \"str\"                     (string) Address the message was sent from\n"
            "  \"to\": \"str\"                       (string) Address the message was sent to\n"
            "  \"text\": \"str\"                     (string) Message text\n"
            "}\n");

    EnsureSMSGIsEnabled();

    RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::VSTR}, true);

    std::string mode = request.params[0].isStr() ? request.params[0].get_str() : "all";
    std::string filter = request.params[1].isStr() ? request.params[1].get_str() : "";


    UniValue result(UniValue::VOBJ);

    std::string sPrefix("sm");
    uint8_t chKey[30];
    memset(&chKey[0], 0, sizeof(chKey));

    {
        LOCK(smsg::cs_smsgDB);

        smsg::SecMsgDB dbOutbox;
        if (!dbOutbox.Open("cr+"))
            throw std::runtime_error("Could not open DB.");

        uint32_t nMessages = 0;

        if (mode == "clear")
        {
            dbOutbox.TxnBegin();

            leveldb::Iterator *it = dbOutbox.pdb->NewIterator(leveldb::ReadOptions());
            while (dbOutbox.NextSmesgKey(it, sPrefix, chKey))
            {
                dbOutbox.EraseSmesg(chKey);
                nMessages++;
            };
            delete it;
            dbOutbox.TxnCommit();

            result.pushKV("result", strprintf("Deleted %u messages.", nMessages));
        } else
        if (mode == "all")
        {
            smsg::SecMsgStored smsgStored;
            smsg::MessageData msg;
            leveldb::Iterator *it = dbOutbox.pdb->NewIterator(leveldb::ReadOptions());

            UniValue messageList(UniValue::VARR);

            while (dbOutbox.NextSmesg(it, sPrefix, chKey, smsgStored))
            {
                uint8_t *pHeader = &smsgStored.vchMessage[0];
                const smsg::SecureMessage *psmsg = (smsg::SecureMessage*) pHeader;

                UniValue objM(UniValue::VOBJ);
                objM.pushKV("msgid", HexStr(&chKey[2], &chKey[2] + 28)); // timestamp+hash

                uint32_t nPayload = smsgStored.vchMessage.size() - smsg::SMSG_HDR_LEN;
                int rv = smsgModule.Decrypt(false, smsgStored.addrOutbox, pHeader, pHeader + smsg::SMSG_HDR_LEN, nPayload, msg);
                if (rv == 0)
                {
                    std::string sAddrTo = CBitcoinAddress(smsgStored.addrTo).ToString();
                    std::string sText = std::string((char*)msg.vchMessage.data());
                    if (filter.size() > 0
                        && !(part::stringsMatchI(msg.sFromAddress, filter, 3) ||
                            part::stringsMatchI(sAddrTo, filter, 3) ||
                            part::stringsMatchI(sText, filter, 3)))
                        continue;

                    PushTime(objM, "sent", msg.timestamp);
                    objM.pushKV("paid", UniValue(psmsg->IsPaidVersion()));

                    uint32_t nDaysRetention = psmsg->IsPaidVersion() ? psmsg->nonce[0] : 2;
                    int64_t ttl = smsg::SMSGGetSecondsInDay() * nDaysRetention;
                    objM.pushKV("daysretention", (int)nDaysRetention);
                    PushTime(objM, "expiration", psmsg->timestamp + ttl);

                    uint32_t nPayload = smsgStored.vchMessage.size() - smsg::SMSG_HDR_LEN;
                    objM.pushKV("payloadsize", (int)nPayload);

                    objM.pushKV("from", msg.sFromAddress);
                    objM.pushKV("to", sAddrTo);
                    objM.pushKV("text", sText);
                } else
                {
                    if (filter.size() > 0)
                        continue;

                    objM.pushKV("status", "Decrypt failed");
                    objM.pushKV("error", smsg::GetString(rv));
                };
                messageList.push_back(objM);
                nMessages++;
            };
            delete it;

            result.pushKV("messages" ,messageList);
            result.pushKV("result", strprintf("%u", nMessages));
        } else
        {
            result.pushKV("result", "Unknown Mode.");
            result.pushKV("expected", "all|clear.");
        };
    }

    return result;
};


static UniValue smsgbuckets(const JSONRPCRequest &request)
{
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            "smsgbuckets ( stats|dump )\n"
            "Display some statistics.");

    EnsureSMSGIsEnabled();

    std::string mode = "stats";
    if (request.params.size() > 0)
    {
        mode = request.params[0].get_str();
    };

    UniValue result(UniValue::VOBJ);
    UniValue arrBuckets(UniValue::VARR);

    char cbuf[256];
    if (mode == "stats")
    {
        uint32_t nBuckets = 0;
        uint32_t nMessages = 0;
        uint64_t nBytes = 0;
        {
            LOCK(smsgModule.cs_smsg);
            std::map<int64_t, smsg::SecMsgBucket>::iterator it;
            it = smsgModule.buckets.begin();

            for (it = smsgModule.buckets.begin(); it != smsgModule.buckets.end(); ++it)
            {
                std::set<smsg::SecMsgToken> &tokenSet = it->second.setTokens;

                std::string sBucket = std::to_string(it->first);
                std::string sFile = sBucket + "_01.dat";
                std::string sHash = std::to_string(it->second.hash);

                size_t nActiveMessages = it->second.CountActive();

                nBuckets++;
                nMessages += nActiveMessages;

                UniValue objM(UniValue::VOBJ);
                objM.pushKV("bucket", sBucket);
                PushTime(objM, "time", it->first);
                objM.pushKV("no. messages", strprintf("%u", tokenSet.size()));
                objM.pushKV("active messages", strprintf("%u", nActiveMessages));
                objM.pushKV("hash", sHash);
                objM.pushKV("last changed", part::GetTimeString(it->second.timeChanged, cbuf, sizeof(cbuf)));

                    fs::path fullPath = GetDataDir() / "smsgstore" / sFile;
    if (!fs::exists(fullPath))
                {
                    // If there is a file for an empty bucket something is wrong.
                    if (tokenSet.size() == 0)
                        objM.pushKV("file size", "Empty bucket.");
                    else
                        objM.pushKV("file size, error", "File not found.");
                } else
                {
                    try {
                        uint64_t nFBytes = 0;
                        nFBytes = fs::file_size(fullPath);
                        nBytes += nFBytes;
                        objM.pushKV("file size", part::BytesReadable(nFBytes));
                    } catch (const filesystem_error& ex)
                    {
                        objM.pushKV("file size, error", ex.what());
                    };
                };

                arrBuckets.push_back(objM);
            };
        }; // cs_smsg

        UniValue objM(UniValue::VOBJ);
        objM.pushKV("numbuckets", (int)nBuckets);
        objM.pushKV("numpurged", (int)smsgModule.setPurged.size());
        objM.pushKV("messages", (int)nMessages);
        objM.pushKV("size", part::BytesReadable(nBytes));
        result.pushKV("buckets", arrBuckets);
        result.pushKV("total", objM);

    } else
    if (mode == "dump")
    {
        {
            LOCK(smsgModule.cs_smsg);
            std::map<int64_t, smsg::SecMsgBucket>::iterator it;
            it = smsgModule.buckets.begin();

            for (it = smsgModule.buckets.begin(); it != smsgModule.buckets.end(); ++it)
            {
                std::string sFile = std::to_string(it->first) + "_01.dat";

                try {
                    fs::path fullPath = GetDataDir() / "smsgstore" / sFile;
                    fs::remove(fullPath);
                } catch (const filesystem_error& ex)
                {
                    //objM.push_back(Pair("file size, error", ex.what()));
                    LogPrintf("Error removing bucket file %s.\n", ex.what());
                };
            };
            smsgModule.buckets.clear();
        }; // cs_smsg

        result.pushKV("result", "Removed all buckets.");
    } else
    {
        result.pushKV("result", "Unknown Mode.");
        result.pushKV("expected", "stats|dump.");
    };

    return result;
};

static bool sortMsgAsc(const std::pair<int64_t, UniValue> &a, const std::pair<int64_t, UniValue> &b)
{
    return a.first < b.first;
};

static bool sortMsgDesc(const std::pair<int64_t, UniValue> &a, const std::pair<int64_t, UniValue> &b)
{
    return a.first > b.first;
};

static UniValue smsgview(const JSONRPCRequest &request)
{
    if (request.fHelp || request.params.size() > 6)
        throw std::runtime_error(
            "smsgview  ( \"address/label\"|(asc/desc|-from yyyy-mm-dd|-to yyyy-mm-dd) )\n"
            "View messages by address."
            "Setting address to '*' will match all addresses"
            "'abc*' will match addresses with labels beginning 'abc'"
            "'*abc' will match addresses with labels ending 'abc'"
            "Full date/time format for from and to is yyyy-mm-ddThh:mm:ss"
            "From and to will accept incomplete inputs like: -from 2016");

    EnsureSMSGIsEnabled();

#ifdef ENABLE_WALLET
    if (smsgModule.pwallet->IsLocked())
        throw JSONRPCError(RPC_MISC_ERROR, "Wallet is locked.");

    char cbuf[256];
    bool fMatchAll = false;
    bool fDesc = false;
    int64_t tFrom = 0, tTo = 0;
    std::vector<CKeyID> vMatchAddress;
    std::string sTemp;

    if (request.params.size() > 0)
    {
        sTemp = request.params[0].get_str();

        // Blank address or "*" will match all
        if (sTemp.length() < 1) // Error instead?
            fMatchAll = true;
        else
        if (sTemp.length() == 1 && sTemp[0] == '*')
            fMatchAll = true;

        if (!fMatchAll)
        {
            CBitcoinAddress checkValid(sTemp);

            if (checkValid.IsValid())
            {
                CKeyID ki;
                checkValid.GetKeyID(ki);
                vMatchAddress.push_back(ki);
            } else
            {
                // Lookup address by label, can match multiple addresses

                // TODO: Use Boost.Regex?
                int matchType = 0; // 0 full match, 1 startswith, 2 endswith
                if (sTemp[0] == '*')
                {
                    matchType = 1;
                    sTemp.erase(0, 1);
                } else
                if (sTemp[sTemp.length()-1] == '*')
                {
                    matchType = 2;
                    sTemp.erase(sTemp.length()-1, 1);
                };

                std::map<CTxDestination, CAddressBookData>::iterator itl;

                for (itl = smsgModule.pwallet->mapAddressBook.begin(); itl != smsgModule.pwallet->mapAddressBook.end(); ++itl)
                {
                    if (part::stringsMatchI(itl->second.name, sTemp, matchType))
                    {
                        CBitcoinAddress checkValid(itl->first);
                        if (checkValid.IsValid())
                        {
                            CKeyID ki;
                            checkValid.GetKeyID(ki);
                            vMatchAddress.push_back(ki);
                        } else
                        {
                            LogPrintf("Warning: matched invalid address: %s\n", checkValid.ToString().c_str());
                        };
                    };
                };
            };
        };
    } else
    {
        fMatchAll = true;
    };

    size_t i = 1;
    while (i < request.params.size())
    {
        sTemp = request.params[i].get_str();
        if (sTemp == "-from")
        {
            if (i >= request.params.size()-1)
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Argument required for: " + sTemp);
            i++;
            sTemp = request.params[i].get_str();
            tFrom = part::strToEpoch(sTemp.c_str());
            if (tFrom < 0)
                throw JSONRPCError(RPC_INVALID_PARAMETER, "from format error: " + std::string(strerror(errno)));
        } else
        if (sTemp == "-to")
        {
            if (i >= request.params.size()-1)
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Argument required for: " + sTemp);
            i++;
            sTemp = request.params[i].get_str();
            tTo = part::strToEpoch(sTemp.c_str());
            if (tTo < 0)
                throw JSONRPCError(RPC_INVALID_PARAMETER, "to format error: " + std::string(strerror(errno)));
        } else
        if (sTemp == "asc")
        {
            fDesc = false;
        } else
        if (sTemp == "desc")
        {
            fDesc = true;
        } else
        {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Unknown parameter: " + sTemp);
        };

        i++;
    };

    if (!fMatchAll && vMatchAddress.size() < 1)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "No address found.");

    UniValue result(UniValue::VOBJ);

    std::map<CKeyID, std::string> mLabelCache;
    std::vector<std::pair<int64_t, UniValue> > vMessages;

    std::vector<std::string> vPrefixes;
    vPrefixes.push_back("im");
    vPrefixes.push_back("sm");

    uint8_t chKey[30];
    size_t nMessages = 0;
    UniValue messageList(UniValue::VARR);

    size_t debugEmptySent = 0;

    {
        LOCK(smsg::cs_smsgDB);
        smsg::SecMsgDB dbMsg;
        if (!dbMsg.Open("cr"))
            throw std::runtime_error("Could not open DB.");

        std::vector<std::string>::iterator itp;
        std::vector<CKeyID>::iterator its;
        for (itp = vPrefixes.begin(); itp < vPrefixes.end(); ++itp)
        {
            bool fInbox = *itp == std::string("im");

            dbMsg.TxnBegin();

            leveldb::Iterator *it = dbMsg.pdb->NewIterator(leveldb::ReadOptions());
            smsg::SecMsgStored smsgStored;
            smsg::MessageData msg;

            while (dbMsg.NextSmesg(it, *itp, chKey, smsgStored))
            {
                if (!fInbox && smsgStored.addrOutbox.IsNull())
                {
                    debugEmptySent++;
                    continue;
                };

                uint32_t nPayload = smsgStored.vchMessage.size() - smsg::SMSG_HDR_LEN;
                int rv;
                if ((rv = smsgModule.Decrypt(false, fInbox ? smsgStored.addrTo : smsgStored.addrOutbox,
                    &smsgStored.vchMessage[0], &smsgStored.vchMessage[smsg::SMSG_HDR_LEN], nPayload, msg)) == 0)
                {
                    if ((tFrom > 0 && msg.timestamp < tFrom)
                        || (tTo > 0 && msg.timestamp > tTo))
                        continue;

                    CKeyID kiFrom;
                    CBitcoinAddress addrFrom(msg.sFromAddress);
                    if (addrFrom.IsValid())
                        addrFrom.GetKeyID(kiFrom);

                    if (!fMatchAll)
                    {
                        bool fSkip = true;

                        for (its = vMatchAddress.begin(); its < vMatchAddress.end(); ++its)
                        {
                            if (*its == kiFrom
                                || *its == smsgStored.addrTo)
                            {
                                fSkip = false;
                                break;
                            };
                        };

                        if (fSkip)
                            continue;
                    };

                    // Get labels for addresses, cache found labels.
                    std::string lblFrom, lblTo;
                    std::map<CKeyID, std::string>::iterator itl;

                    if ((itl = mLabelCache.find(kiFrom)) != mLabelCache.end())
                    {
                        lblFrom = itl->second;
                    } else
                    {
                        CBitcoinAddress address_parsed(kiFrom);
                        std::map<CTxDestination, CAddressBookData>::iterator
                            mi(smsgModule.pwallet->mapAddressBook.find(address_parsed.Get()));
                        if (mi != smsgModule.pwallet->mapAddressBook.end())
                            lblFrom = mi->second.name;
                        mLabelCache[kiFrom] = lblFrom;
                    };

                    if ((itl = mLabelCache.find(smsgStored.addrTo)) != mLabelCache.end())
                    {
                        lblTo = itl->second;
                    } else
                    {
                        CBitcoinAddress address_parsed(smsgStored.addrTo);
                        std::map<CTxDestination, CAddressBookData>::iterator
                            mi(smsgModule.pwallet->mapAddressBook.find(address_parsed.Get()));
                        if (mi != smsgModule.pwallet->mapAddressBook.end())
                            lblTo = mi->second.name;
                        mLabelCache[smsgStored.addrTo] = lblTo;
                    };

                    std::string sFrom = kiFrom.IsNull() ? "anon" : CBitcoinAddress(kiFrom).ToString();
                    std::string sTo = CBitcoinAddress(smsgStored.addrTo).ToString();
                    if (lblFrom.length() != 0)
                        sFrom += " (" + lblFrom + ")";
                    if (lblTo.length() != 0)
                        sTo += " (" + lblTo + ")";

                    UniValue objM(UniValue::VOBJ);
                    PushTime(objM, "sent", msg.timestamp);
                    objM.pushKV("from", sFrom);
                    objM.pushKV("to", sTo);
                    objM.pushKV("text", std::string((char*)&msg.vchMessage[0]));

                    vMessages.push_back(std::make_pair(msg.timestamp, objM));
                } else
                {
                    LogPrintf("%s: SecureMsgDecrypt failed, %s.\n", __func__, HexStr(chKey, chKey+18).c_str());
                };
            };
            delete it;

            dbMsg.TxnCommit();
        };
    } // cs_smsgDB


    std::sort(vMessages.begin(), vMessages.end(), fDesc ? sortMsgDesc : sortMsgAsc);

    std::vector<std::pair<int64_t, UniValue> >::iterator itm;
    for (itm = vMessages.begin(); itm < vMessages.end(); ++itm)
    {
        messageList.push_back(itm->second);
        nMessages++;
    };

    result.pushKV("messages", messageList);

    if (LogAcceptCategory(BCLog::SMSG))
        result.pushKV("debug empty sent", (int)debugEmptySent);

    result.pushKV("result", strprintf("Displayed %u messages.", nMessages));
    if (tFrom > 0)
        result.pushKV("from", part::GetTimeString(tFrom, cbuf, sizeof(cbuf)));
    if (tTo > 0)
        result.pushKV("to", part::GetTimeString(tTo, cbuf, sizeof(cbuf)));
#else
    UniValue result(UniValue::VOBJ);
    throw JSONRPCError(RPC_MISC_ERROR, "No wallet.");
#endif
    return result;
}

static UniValue smsgone(const JSONRPCRequest &request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 3)
        throw std::runtime_error(
            "smsg \"msgid\" ( options )\n"
            "View smsg by msgid.\n"
            "\nArguments:\n"
            "1. \"msgid\"              (string, required) The id of the message to view.\n"
            "2. options              (json, optional) Options object.\n"
            "{\n"
            "       \"delete\": bool                 (bool, optional) Delete msg if true.\n"
            "       \"setread\": bool                (bool, optional) Set read status to value.\n"
            "       \"encoding\": str                (string, optional, default=\"ascii\") Display message data in encoding, values: \"hex\".\n"
            "}\n"
            "\nResult:\n"
            "{\n"
            "  \"msgid\": \"...\"                    (string) The message identifier\n"
            "  \"location\": \"str\"                 (string) inbox|outbox|sending\n"
            "  \"received\": int                     (int) Time the message was received\n"
            "  \"to\": \"str\"                       (string) Address the message was sent to\n"
            "  \"read\": bool                        (bool) Read status\n"
            "  \"sent\": int                         (int) Time the message was created\n"
            "  \"paid\": bool                        (bool) Paid message\n"
            "  \"daysretention\": int                (int) Number of days message will stay in the network for\n"
            "  \"expiration\": int                   (int) Time the message will be dropped from the network\n"
            "  \"payloadsize\": int                  (int) Size of user message\n"
            "  \"from\": \"str\"                     (string) Address the message was sent from\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("smsg", "\"msgid\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("smsg", "\"msgid\""));

    EnsureSMSGIsEnabled();

    RPCTypeCheckObj(request.params,
        {
            {"msgid",             UniValueType(UniValue::VSTR)},
            {"option",            UniValueType(UniValue::VOBJ)},
        }, true, false);

    std::string sMsgId = request.params[0].get_str();

    if (!IsHex(sMsgId) || sMsgId.size() != 56)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "msgid must be 28 bytes in hex string.");
    std::vector<uint8_t> vMsgId = ParseHex(sMsgId.c_str());
    std::string sType;

    uint8_t chKey[30];
    chKey[1] = 'm';
    memcpy(chKey+2, vMsgId.data(), 28);
    smsg::SecMsgStored smsgStored;
    UniValue result(UniValue::VOBJ);

    UniValue options = request.params[1];
    {
        LOCK(smsg::cs_smsgDB);
        smsg::SecMsgDB dbMsg;
        if (!dbMsg.Open("cr+"))
            throw std::runtime_error("Could not open DB.");

        if ((chKey[0] = 'i') && dbMsg.ReadSmesg(chKey, smsgStored)) {
            sType = "inbox";
        } else
        if ((chKey[0] = 's') && dbMsg.ReadSmesg(chKey, smsgStored)) {
            sType = "outbox";
        } else
        if ((chKey[0] = 'q') && dbMsg.ReadSmesg(chKey, smsgStored)) {
            sType = "sending";
        } else {
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Unknown message id.");
        };

        if (options.isObject())
        {
            options = request.params[1].get_obj();
            if (options["delete"].isBool() && options["delete"].get_bool() == true)
            {
                if (!dbMsg.EraseSmesg(chKey))
                    throw JSONRPCError(RPC_INTERNAL_ERROR, "EraseSmesg failed.");
                result.pushKV("operation", "Deleted");
            } else
            {
                // Can't mix delete and other operations
                if (options["setread"].isBool())
                {
                    bool nv = options["setread"].get_bool();
                    if (nv)
                        smsgStored.status &= ~SMSG_MASK_UNREAD;
                    else
                        smsgStored.status |= SMSG_MASK_UNREAD;

                    if (!dbMsg.WriteSmesg(chKey, smsgStored))
                        throw JSONRPCError(RPC_INTERNAL_ERROR, "WriteSmesg failed.");
                    result.pushKV("operation", strprintf("Set read status to: %s", nv ? "true" : "false"));
                };
            };
        };
    }

    const smsg::SecureMessage *psmsg = (smsg::SecureMessage*) &smsgStored.vchMessage[0];

    result.pushKV("msgid", sMsgId);
    result.pushKV("location", sType);
    PushTime(result, "received", smsgStored.timeReceived);
    result.pushKV("to", CBitcoinAddress(smsgStored.addrTo).ToString());
    //result.pushKV("addressoutbox", CBitcoinAddress(smsgStored.addrOutbox).ToString());
    result.pushKV("read", UniValue(bool(!(smsgStored.status & SMSG_MASK_UNREAD))));

    PushTime(result, "sent", psmsg->timestamp);
    result.pushKV("paid", UniValue(psmsg->IsPaidVersion()));

    uint32_t nDaysRetention = psmsg->IsPaidVersion() ? psmsg->nonce[0] : 2;
    int64_t ttl = smsg::SMSGGetSecondsInDay() * nDaysRetention;
    result.pushKV("daysretention", (int)nDaysRetention);
    PushTime(result, "expiration", psmsg->timestamp + ttl);


    smsg::MessageData msg;
    bool fInbox = sType == "inbox" ? true : false;
    uint32_t nPayload = smsgStored.vchMessage.size() - smsg::SMSG_HDR_LEN;
    result.pushKV("payloadsize", (int)nPayload);

    std::string sEnc;
    if (options.isObject() && options["encoding"].isStr())
        sEnc = options["encoding"].get_str();

    int rv;
    if ((rv = smsgModule.Decrypt(false, fInbox ? smsgStored.addrTo : smsgStored.addrOutbox,
        &smsgStored.vchMessage[0], &smsgStored.vchMessage[smsg::SMSG_HDR_LEN], nPayload, msg)) == 0)
    {
        result.pushKV("from", msg.sFromAddress);

        if (sEnc == "")
        {
            // TODO: detect non ascii chars
            if (msg.vchMessage.size() < smsg::SMSG_MAX_MSG_BYTES)
                result.pushKV("text", std::string((char*)msg.vchMessage.data()));
            else
                result.pushKV("hex", HexStr(msg.vchMessage));
        } else
        if (sEnc == "ascii")
        {
            result.pushKV("text", std::string((char*)msg.vchMessage.data()));
        } else
        if (sEnc == "hex")
        {
            result.pushKV("hex", HexStr(msg.vchMessage));
        } else
        {
            result.pushKV("unknown_encoding", sEnc);
        };
    } else
    {
        result.pushKV("error", "decrypt failed");
    };

    return result;
}

static UniValue smsgpurge(const JSONRPCRequest &request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "smsgpurge \"msgid\"\n"
            "Purge smsg by msgid.\n"
            "\nArguments:\n"
            "1. \"msgid\"              (string, required) The id of the message to purge.\n"
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("smsgpurge", "\"msgid\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("smsgpurge", "\"msgid\""));

    EnsureSMSGIsEnabled();

    if (!request.params[0].isStr())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "msgid must be a string.");

    std::string sMsgId = request.params[0].get_str();

    if (!IsHex(sMsgId) || sMsgId.size() != 56)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "msgid must be 28 bytes in hex string.");
    std::vector<uint8_t> vMsgId = ParseHex(sMsgId.c_str());

    std::string sError;
    if (smsg::SMSG_NO_ERROR != smsgModule.Purge(vMsgId, sError))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Error: " + sError);

    return NullUniValue;
}

static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         argNames
  //  --------------------- ------------------------  -----------------------  ----------
    { "smsg",               "smsgenable",             &smsgenable,             {} },
    { "smsg",               "smsgdisable",            &smsgdisable,            {} },
    { "smsg",               "smsgoptions",            &smsgoptions,            {} },
    { "smsg",               "smsglocalkeys",          &smsglocalkeys,          {} },
    { "smsg",               "smsgscanchain",          &smsgscanchain,          {} },
    { "smsg",               "smsgscanbuckets",        &smsgscanbuckets,        {} },
    { "smsg",               "smsginfo",               &smsginfo,               {} },
    { "smsg",               "flushsmgsdb",            &flushsmgsdb,            {} },
    { "smsg",               "smsgaddaddress",         &smsgaddaddress,         {"address","pubkey"} },
    { "smsg",               "smsgaddlocaladdress",    &smsgaddlocaladdress,    {"address"} },
    { "smsg",               "smsgimportprivkey",      &smsgimportprivkey,      {"privkey","label"} },
    { "smsg",               "smsggetpubkey",          &smsggetpubkey,          {"address"} },
    { "smsg",               "smsgsend",               &smsgsend,               {"address_from","address_to","message","paid_msg","days_retention","testfee","fromfile","decodehex"} },
    { "smsg",               "smsgsendanon",           &smsgsendanon,           {"address_to","message","days_retention"} },
    { "smsg",               "smsginbox",              &smsginbox,              {"mode","filter"} },
    { "smsg",               "smsgoutbox",             &smsgoutbox,             {"mode","filter"} },
    { "smsg",               "smsgbuckets",            &smsgbuckets,            {"mode"} },
    { "smsg",               "smsgview",               &smsgview,               {}},
    { "smsg",               "smsg",                   &smsgone,                {"msgid","options"}},
    { "smsg",               "smsgpurge",              &smsgpurge,              {"msgid"}},
};

void RegisterSmsgRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
