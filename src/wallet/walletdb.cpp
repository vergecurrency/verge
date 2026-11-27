// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2025 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/walletdb.h>

#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <fs.h>
#include <key_io.h>
#include <protocol.h>
#include <serialize.h>
#include <sync.h>
#include <util/system.h>
#include <util/time.h>
#include <wallet/wallet.h>
#include <pos/pqcrypto.h>
#include <script/pq.h>

#include <atomic>

#include <boost/thread.hpp>

//
// WalletBatch
//

bool WalletBatch::WriteName(const std::string& strAddress, const std::string& strName)
{
    return WriteIC(std::make_pair(std::string("name"), strAddress), strName);
}

bool WalletBatch::EraseName(const std::string& strAddress)
{
    // This should only be used for sending addresses, never for receiving addresses,
    // receiving addresses must always have an address book entry if they're not change return.
    return EraseIC(std::make_pair(std::string("name"), strAddress));
}

bool WalletBatch::WritePurpose(const std::string& strAddress, const std::string& strPurpose)
{
    return WriteIC(std::make_pair(std::string("purpose"), strAddress), strPurpose);
}

bool WalletBatch::ErasePurpose(const std::string& strAddress)
{
    return EraseIC(std::make_pair(std::string("purpose"), strAddress));
}

bool WalletBatch::WriteTx(const CWalletTx& wtx)
{
    return WriteIC(std::make_pair(std::string("tx"), wtx.GetHash()), wtx);
}

bool WalletBatch::EraseTx(uint256 hash)
{
    return EraseIC(std::make_pair(std::string("tx"), hash));
}

bool WalletBatch::WriteKey(const CPubKey& vchPubKey, const CPrivKey& vchPrivKey, const CKeyMetadata& keyMeta)
{
    if (!WriteIC(std::make_pair(std::string("keymeta"), vchPubKey), keyMeta, false)) {
        return false;
    }

    // hash pubkey/privkey to accelerate wallet load
    std::vector<unsigned char> vchKey;
    vchKey.reserve(vchPubKey.size() + vchPrivKey.size());
    vchKey.insert(vchKey.end(), vchPubKey.begin(), vchPubKey.end());
    vchKey.insert(vchKey.end(), vchPrivKey.begin(), vchPrivKey.end());

    return WriteIC(std::make_pair(std::string("key"), vchPubKey), std::make_pair(vchPrivKey, Hash(vchKey.begin(), vchKey.end())), false);
}

bool WalletBatch::WriteCryptedKey(const CPubKey& vchPubKey,
                                const std::vector<unsigned char>& vchCryptedSecret,
                                const CKeyMetadata &keyMeta)
{
    if (!WriteIC(std::make_pair(std::string("keymeta"), vchPubKey), keyMeta)) {
        return false;
    }

    if (!WriteIC(std::make_pair(std::string("ckey"), vchPubKey), vchCryptedSecret, false)) {
        return false;
    }
    EraseIC(std::make_pair(std::string("key"), vchPubKey));
    EraseIC(std::make_pair(std::string("wkey"), vchPubKey));
    return true;
}

bool WalletBatch::WriteMasterKey(unsigned int nID, const CMasterKey& kMasterKey)
{
    return WriteIC(std::make_pair(std::string("mkey"), nID), kMasterKey, true);
}

bool WalletBatch::WritePQMultisigPolicy(const uint256& id, const std::vector<unsigned char>& encoded)
{
    PQMultisigPolicy policy;
    return DecodePQMultisigPolicy(encoded, policy) && GetPQMultisigPolicyId(policy) == id &&
        WriteIC(std::make_pair(std::string("pqpolicy"), id), encoded, false);
}

bool WalletBatch::WritePQKey(const std::vector<unsigned char>& public_key,
                              const CKeyingMaterial& seed)
{
    const uint256 id = pos::experimental::GetMLDSA44KeyId(public_key);
    return !id.IsNull() && seed.size() == pos::experimental::MLDSA44_SEED_SIZE &&
        WriteIC(std::make_pair(std::string("pqkey"), id), std::make_pair(public_key, seed), false);
}

bool WalletBatch::WriteSmsgPQKey(const CKeyID& address, const std::vector<unsigned char>& public_key,
                                const CKeyingMaterial& seed, const std::vector<unsigned char>& ciphertext)
{
    smsg::PQMessagingPublicKey key;
    if (!smsg::PQMessagingPublicKey::Decode(public_key, key) || key.classical.GetID() != address) return false;
    if (ciphertext.empty()) {
        return seed.size() == smsg::PQ_MESSAGING_SEED_SIZE &&
            WriteIC(std::make_pair(std::string("smsgpqkey"), address), std::make_pair(public_key, seed), false);
    }
    if (!seed.empty() || ciphertext.size() != 112 ||
        !WriteIC(std::make_pair(std::string("csmsgpqkey"), address), std::make_pair(public_key, ciphertext), false)) return false;
    return EraseIC(std::make_pair(std::string("smsgpqkey"), address));
}

bool WalletBatch::WriteSmsgPQContact(const CKeyID& address, const std::vector<unsigned char>& public_key)
{
    return WriteIC(std::make_pair(std::string("smsgpqcontact"), address), public_key, false);
}

bool WalletBatch::WriteCryptedPQKey(const std::vector<unsigned char>& public_key,
                                     const std::vector<unsigned char>& ciphertext,
                                     bool erase_plaintext)
{
    const uint256 id = pos::experimental::GetMLDSA44KeyId(public_key);
    if (id.IsNull() || ciphertext.size() != 48 ||
        !WriteIC(std::make_pair(std::string("cpqkey"), id),
                 std::make_pair(public_key, ciphertext), false)) return false;
    return !erase_plaintext || EraseIC(std::make_pair(std::string("pqkey"), id));
}

bool WalletBatch::WriteStealthKeyMeta(const CKeyID& keyId, const CStealthKeyMetadata& sxKeyMeta)
{
    return WriteIC(std::make_pair(std::string("sxKeyMeta"), keyId), sxKeyMeta, true);
}

bool WalletBatch::EraseStealthKeyMeta(const CKeyID& keyId)
{
    return EraseIC(std::make_pair(std::string("sxKeyMeta"), keyId));
}

bool WalletBatch::WriteStealthAddress(const CStealthAddress& sxAddr)
{
    const auto key = std::make_pair(std::string("sxAddr"),
                                    std::make_pair(sxAddr.scan_pubkey, sxAddr.spend_pubkey));
    if (!WriteIC(key, sxAddr, true)) {
        return false;
    }
    // Remove a matching legacy scan-key-only record after successfully writing
    // the collision-safe key. Preserve it if it belongs to a different spend key.
    const auto legacy_key = std::make_pair(std::string("sxAddr"), sxAddr.scan_pubkey);
    CStealthAddress legacy_address;
    if (m_batch.Read(legacy_key, legacy_address) &&
        legacy_address.spend_pubkey == sxAddr.spend_pubkey) {
        m_batch.Erase(legacy_key);
    }
    return true;
}

bool WalletBatch::ReadStealthAddress(CStealthAddress& sxAddr)
{
    const auto key = std::make_pair(std::string("sxAddr"),
                                    std::make_pair(sxAddr.scan_pubkey, sxAddr.spend_pubkey));
    if (m_batch.Read(key, sxAddr)) {
        return true;
    }
    return m_batch.Read(std::make_pair(std::string("sxAddr"), sxAddr.scan_pubkey), sxAddr);
}

bool WalletBatch::WriteCScript(const uint160& hash, const CScript& redeemScript)
{
    return WriteIC(std::make_pair(std::string("cscript"), hash), redeemScript, false);
}

bool WalletBatch::WriteWatchOnly(const CScript &dest, const CKeyMetadata& keyMeta)
{
    if (!WriteIC(std::make_pair(std::string("watchmeta"), dest), keyMeta)) {
        return false;
    }
    return WriteIC(std::make_pair(std::string("watchs"), dest), '1');
}

bool WalletBatch::EraseWatchOnly(const CScript &dest)
{
    if (!EraseIC(std::make_pair(std::string("watchmeta"), dest))) {
        return false;
    }
    return EraseIC(std::make_pair(std::string("watchs"), dest));
}

bool WalletBatch::WriteBestBlock(const CBlockLocator& locator)
{
    WriteIC(std::string("bestblock"), CBlockLocator()); // Write empty block locator so versions that require a merkle branch automatically rescan
    return WriteIC(std::string("bestblock_nomerkle"), locator);
}

bool WalletBatch::ReadBestBlock(CBlockLocator& locator)
{
    if (m_batch.Read(std::string("bestblock"), locator) && !locator.vHave.empty()) return true;
    return m_batch.Read(std::string("bestblock_nomerkle"), locator);
}

bool WalletBatch::WriteOrderPosNext(int64_t nOrderPosNext)
{
    return WriteIC(std::string("orderposnext"), nOrderPosNext);
}

bool WalletBatch::WriteStakingEnabled(bool enabled)
{
    return WriteIC(std::string("staking_enabled"), enabled);
}

bool WalletBatch::WriteStakingReserveBalance(CAmount reserve)
{
    return WriteIC(std::string("staking_reserve_balance"), reserve);
}

bool WalletBatch::ReadPool(int64_t nPool, CKeyPool& keypool)
{
    return m_batch.Read(std::make_pair(std::string("pool"), nPool), keypool);
}

bool WalletBatch::WritePool(int64_t nPool, const CKeyPool& keypool)
{
    return WriteIC(std::make_pair(std::string("pool"), nPool), keypool);
}

bool WalletBatch::ErasePool(int64_t nPool)
{
    return EraseIC(std::make_pair(std::string("pool"), nPool));
}

bool WalletBatch::WriteMinVersion(int nVersion)
{
    return WriteIC(std::string("minversion"), nVersion);
}

bool WalletBatch::ReadAccount(const std::string& strAccount, CAccount& account)
{
    account.SetNull();
    return m_batch.Read(std::make_pair(std::string("acc"), strAccount), account);
}

bool WalletBatch::WriteAccount(const std::string& strAccount, const CAccount& account)
{
    return WriteIC(std::make_pair(std::string("acc"), strAccount), account);
}

bool WalletBatch::EraseAccount(const std::string& strAccount)
{
    return EraseIC(std::make_pair(std::string("acc"), strAccount));
}

bool WalletBatch::WriteAccountingEntry(const uint64_t nAccEntryNum, const CAccountingEntry& acentry)
{
    return WriteIC(std::make_pair(std::string("acentry"), std::make_pair(acentry.strAccount, nAccEntryNum)), acentry);
}

CAmount WalletBatch::GetAccountCreditDebit(const std::string& strAccount)
{
    std::list<CAccountingEntry> entries;
    ListAccountCreditDebit(strAccount, entries);

    CAmount nCreditDebit = 0;
    for (const CAccountingEntry& entry : entries)
        nCreditDebit += entry.nCreditDebit;

    return nCreditDebit;
}

void WalletBatch::ListAccountCreditDebit(const std::string& strAccount, std::list<CAccountingEntry>& entries)
{
    bool fAllAccounts = (strAccount == "*");

    Dbc* pcursor = m_batch.GetCursor();
    if (!pcursor)
        throw std::runtime_error(std::string(__func__) + ": cannot create DB cursor");
    bool setRange = true;
    while (true)
    {
        // Read next record
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        if (setRange)
            ssKey << std::make_pair(std::string("acentry"), std::make_pair((fAllAccounts ? std::string("") : strAccount), uint64_t(0)));
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        int ret = m_batch.ReadAtCursor(pcursor, ssKey, ssValue, setRange);
        setRange = false;
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
        {
            pcursor->close();
            throw std::runtime_error(std::string(__func__) + ": error scanning DB");
        }

        // Unserialize
        std::string strType;
        ssKey >> strType;
        if (strType != "acentry")
            break;
        CAccountingEntry acentry;
        ssKey >> acentry.strAccount;
        if (!fAllAccounts && acentry.strAccount != strAccount)
            break;

        ssValue >> acentry;
        ssKey >> acentry.nEntryNo;
        entries.push_back(acentry);
    }

    pcursor->close();
}

class CWalletScanState {
public:
    struct PQAddressRecord {
        std::string type;
        std::string address;
        std::string key;
        std::string value;
        CTxDestination destination;
    };
    std::vector<PQAddressRecord> pq_address_records;

    void RecordPQAddress(const std::string& type, const std::string& address,
                         const std::string& key, const std::string& value,
                         const CTxDestination& destination)
    {
        if ((boost::get<PQKeyHash>(&destination) || boost::get<PQPolicyHash>(&destination)) &&
            EncodeDestination(destination) != address) {
            pq_address_records.push_back({type, address, key, value, destination});
        }
    }

    unsigned int nKeys;
    unsigned int nCKeys;
    unsigned int nWatchKeys;
    unsigned int nKeyMeta;
    unsigned int m_unknown_records;
    bool fIsEncrypted;
    bool fHasPQKeys{false};
    bool fHasSmsgPQKeys{false};
    bool fAnyUnordered;
    int nFileVersion;
    std::vector<uint256> vWalletUpgrade;

    CWalletScanState() {
        nKeys = nCKeys = nWatchKeys = nKeyMeta = m_unknown_records = 0;
        fIsEncrypted = false;
        fAnyUnordered = false;
        nFileVersion = 0;
    }
};

static bool
ReadKeyValue(CWallet* pwallet, CDataStream& ssKey, CDataStream& ssValue,
             CWalletScanState &wss, std::string& strType, std::string& strErr) EXCLUSIVE_LOCKS_REQUIRED(pwallet->cs_wallet)
{
    try {
        // Unserialize
        // Taking advantage of the fact that pair serialization
        // is just the two items serialized one after the other
        ssKey >> strType;
        if (strType == "name" || strType == "purpose")
        {
            std::string strAddress, value;
            ssKey >> strAddress;
            ssValue >> value;
            const auto destination = DecodeDestination(strAddress);
            wss.RecordPQAddress(strType, strAddress, "", value, destination);
            auto& entry = pwallet->mapAddressBook[destination];
            (strType == "name" ? entry.name : entry.purpose) = value;
        }
        else if (strType == "tx")
        {
            uint256 hash;
            ssKey >> hash;
            CWalletTx wtx(nullptr /* pwallet */, MakeTransactionRef());
            ssValue >> wtx;
            CValidationState state;
            if (!(CheckTransaction(*wtx.tx, state) && (wtx.GetHash() == hash) && state.IsValid()))
                return false;

            // Undo serialize changes in 31600
            if (31404 <= wtx.fTimeReceivedIsTxTime && wtx.fTimeReceivedIsTxTime <= 31703)
            {
                if (!ssValue.empty())
                {
                    char fTmp;
                    char fUnused;
                    ssValue >> fTmp >> fUnused >> wtx.strFromAccount;
                    strErr = strprintf("LoadWallet() upgrading tx ver=%d %d '%s' %s",
                                       wtx.fTimeReceivedIsTxTime, fTmp, wtx.strFromAccount, hash.ToString());
                    wtx.fTimeReceivedIsTxTime = fTmp;
                }
                else
                {
                    strErr = strprintf("LoadWallet() repairing tx ver=%d %s", wtx.fTimeReceivedIsTxTime, hash.ToString());
                    wtx.fTimeReceivedIsTxTime = 0;
                }
                wss.vWalletUpgrade.push_back(hash);
            }

            if (wtx.nOrderPos == -1)
                wss.fAnyUnordered = true;

            pwallet->LoadToWallet(wtx);
        }
        else if (strType == "sxAddr")		
        {
            CStealthAddress sxAddr;
            ssValue >> sxAddr;
            LogPrintf("Wallet ReadKeyValue sxAddr: %s\n", sxAddr.Encoded().c_str());
            pwallet->stealthAddresses.insert(sxAddr);
        }
        else if (strType == "acentry")
        {
            std::string strAccount;
            ssKey >> strAccount;
            uint64_t nNumber;
            ssKey >> nNumber;
            if (nNumber > pwallet->nAccountingEntryNumber) {
                pwallet->nAccountingEntryNumber = nNumber;
            }

            if (!wss.fAnyUnordered)
            {
                CAccountingEntry acentry;
                ssValue >> acentry;
                if (acentry.nOrderPos == -1)
                    wss.fAnyUnordered = true;
            }
        }
        else if (strType == "watchs")
        {
            wss.nWatchKeys++;
            CScript script;
            ssKey >> script;
            char fYes;
            ssValue >> fYes;
            if (fYes == '1')
                pwallet->LoadWatchOnly(script);
        }
        else if (strType == "key" || strType == "wkey")
        {
            CPubKey vchPubKey;
            ssKey >> vchPubKey;
            if (!vchPubKey.IsValid())
            {
                strErr = "Error reading wallet database: CPubKey corrupt";
                return false;
            }
            CKey key;
            CPrivKey pkey;
            uint256 hash;

            if (strType == "key")
            {
                wss.nKeys++;
                ssValue >> pkey;
            } else {
                CWalletKey wkey;
                ssValue >> wkey;
                pkey = wkey.vchPrivKey;
            }

            // Old wallets store keys as "key" [pubkey] => [privkey]
            // ... which was slow for wallets with lots of keys, because the public key is re-derived from the private key
            // using EC operations as a checksum.
            // Newer wallets store keys as "key"[pubkey] => [privkey][hash(pubkey,privkey)], which is much faster while
            // remaining backwards-compatible.
            try
            {
                ssValue >> hash;
            }
            catch (...) {}

            bool fSkipCheck = false;

            if (!hash.IsNull())
            {
                // hash pubkey/privkey to accelerate wallet load
                std::vector<unsigned char> vchKey;
                vchKey.reserve(vchPubKey.size() + pkey.size());
                vchKey.insert(vchKey.end(), vchPubKey.begin(), vchPubKey.end());
                vchKey.insert(vchKey.end(), pkey.begin(), pkey.end());

                if (Hash(vchKey.begin(), vchKey.end()) != hash)
                {
                    strErr = "Error reading wallet database: CPubKey/CPrivKey corrupt";
                    return false;
                }

                fSkipCheck = true;
            }

            if (!key.Load(pkey, vchPubKey, fSkipCheck))
            {
                strErr = "Error reading wallet database: CPrivKey corrupt";
                return false;
            }
            if (!pwallet->LoadKey(key, vchPubKey))
            {
                strErr = "Error reading wallet database: LoadKey failed";
                return false;
            }
        }
        else if (strType == "smsgpqcontact" || strType == "smsgpqkey" || strType == "csmsgpqkey")
        {
            CKeyID address;
            ssKey >> address;
            if (!ssKey.empty() || ReadCompactSize(ssValue) != smsg::PQ_MESSAGING_PUBLIC_SIZE) {
                strErr = "Invalid PQ messaging public key length";
                return false;
            }
            std::vector<unsigned char> public_key(smsg::PQ_MESSAGING_PUBLIC_SIZE);
            ssValue.read(reinterpret_cast<char*>(public_key.data()), public_key.size());
            bool valid = false;
            if (strType == "smsgpqcontact") {
                valid = ssValue.empty() && pwallet->LoadSmsgPQContact(address, public_key);
            } else {
                const bool encrypted = strType == "csmsgpqkey";
                const size_t size = encrypted ? 112 : smsg::PQ_MESSAGING_SEED_SIZE;
                if (ReadCompactSize(ssValue) != size || ssValue.size() != size) {
                    strErr = "Invalid PQ messaging secret length";
                    return false;
                }
                smsg::PQSecret seed;
                std::vector<unsigned char> ciphertext;
                if (encrypted) {
                    ciphertext.resize(size);
                    ssValue.read(reinterpret_cast<char*>(ciphertext.data()), size);
                    wss.fIsEncrypted = true;
                } else {
                    seed.resize(size);
                    ssValue.read(reinterpret_cast<char*>(seed.data()), size);
                }
                valid = pwallet->LoadSmsgPQKey(address, public_key, seed, ciphertext);
            }
            if (!valid) {
                strErr = "Corrupt or unsupported PQ messaging key";
                return false;
            }
            wss.fHasSmsgPQKeys = true;
        }
        else if (strType == "pqpolicy")
        {
            uint256 id;
            ssKey >> id;
            const size_t size = ReadCompactSize(ssValue);
            if (!ssKey.empty() || size > PQ_MULTISIG_MAX_POLICY_BYTES || ssValue.size() != size) {
                strErr = "Invalid PQ multisig policy encoding";
                return false;
            }
            std::vector<unsigned char> encoded(size);
            ssValue.read(reinterpret_cast<char*>(encoded.data()), size);
            PQMultisigPolicy policy;
            if (!DecodePQMultisigPolicy(encoded, policy) || GetPQMultisigPolicyId(policy) != id ||
                !pwallet->LoadPQMultisigPolicy(encoded)) {
                strErr = "Corrupt PQ multisig policy";
                return false;
            }
            wss.fHasPQKeys = true;
        }
        else if (strType == "pqkey" || strType == "cpqkey")
        {
            if (!pos::experimental::MLDSA44Available()) {
                strErr = "Wallet requires an ML-DSA-44 backend";
                return false;
            }
            uint256 id;
            ssKey >> id;
            if (!ssKey.empty() || ReadCompactSize(ssValue) != pos::experimental::MLDSA44_PUBLIC_KEY_SIZE) {
                strErr = "Invalid PQ wallet key encoding";
                return false;
            }
            std::vector<unsigned char> public_key(pos::experimental::MLDSA44_PUBLIC_KEY_SIZE);
            ssValue.read(reinterpret_cast<char*>(public_key.data()), public_key.size());
            const size_t expected_size = strType == "pqkey" ? 32 : 48;
            if (pos::experimental::GetMLDSA44KeyId(public_key) != id ||
                ReadCompactSize(ssValue) != expected_size || ssValue.size() != expected_size) {
                strErr = "Invalid PQ wallet secret encoding";
                return false;
            }
            if (strType == "pqkey") {
                CKeyingMaterial seed(expected_size);
                ssValue.read(reinterpret_cast<char*>(seed.data()), seed.size());
                pos::experimental::MLDSA44Key key;
                std::vector<unsigned char> derived;
                if (!key.SetSeed(seed.data(), seed.size()) ||
                    !key.GetPublicKey(derived) || derived != public_key ||
                    !pwallet->LoadPQKey(key, public_key)) {
                    strErr = "Corrupt PQ wallet key";
                    return false;
                }
                ++wss.nKeys;
            } else {
                std::vector<unsigned char> ciphertext(expected_size);
                ssValue.read(reinterpret_cast<char*>(ciphertext.data()), ciphertext.size());
                if (!pwallet->LoadCryptedPQKey(public_key, ciphertext)) {
                    strErr = "Corrupt encrypted PQ wallet key";
                    return false;
                }
                wss.fIsEncrypted = true;
                ++wss.nCKeys;
            }
            wss.fHasPQKeys = true;
        }
        else if (strType == "mkey")
        {
            unsigned int nID;
            ssKey >> nID;
            CMasterKey kMasterKey;
            ssValue >> kMasterKey;
            if(pwallet->mapMasterKeys.count(nID) != 0)
            {
                strErr = strprintf("Error reading wallet database: duplicate CMasterKey id %u", nID);
                return false;
            }
            pwallet->mapMasterKeys[nID] = kMasterKey;
            if (pwallet->nMasterKeyMaxID < nID)
                pwallet->nMasterKeyMaxID = nID;
        }
        else if (strType == "ckey")
        {
            CPubKey vchPubKey;
            ssKey >> vchPubKey;
            if (!vchPubKey.IsValid())
            {
                strErr = "Error reading wallet database: CPubKey corrupt";
                return false;
            }
            std::vector<unsigned char> vchPrivKey;
            ssValue >> vchPrivKey;
            wss.nCKeys++;

            if (!pwallet->LoadCryptedKey(vchPubKey, vchPrivKey))
            {
                strErr = "Error reading wallet database: LoadCryptedKey failed";
                return false;
            }
            wss.fIsEncrypted = true;
        }
        else if (strType == "keymeta")
        {
            CPubKey vchPubKey;
            ssKey >> vchPubKey;
            CKeyMetadata keyMeta;
            ssValue >> keyMeta;
            wss.nKeyMeta++;
            pwallet->LoadKeyMetadata(vchPubKey.GetID(), keyMeta);
        }
        else if (strType == "sxKeyMeta")
        {
            LogPrintf("WalletDB ReadKeyValue sxKeyMeta\n");

            CKeyID keyId;
            ssKey >> keyId;
            CStealthKeyMetadata sxKeyMeta;
            ssValue >> sxKeyMeta;

            pwallet->mapStealthKeyMeta[keyId] = sxKeyMeta;
        }
        else if (strType == "watchmeta")
        {
            CScript script;
            ssKey >> script;
            CKeyMetadata keyMeta;
            ssValue >> keyMeta;
            wss.nKeyMeta++;
            pwallet->LoadScriptMetadata(CScriptID(script), keyMeta);
        }
        else if (strType == "defaultkey")
        {
            // We don't want or need the default key, but if there is one set,
            // we want to make sure that it is valid so that we can detect corruption
            CPubKey vchPubKey;
            ssValue >> vchPubKey;
            if (!vchPubKey.IsValid()) {
                strErr = "Error reading wallet database: Default Key corrupt";
                return false;
            }
        }
        else if (strType == "pool")
        {
            int64_t nIndex;
            ssKey >> nIndex;
            CKeyPool keypool;
            ssValue >> keypool;

            pwallet->LoadKeyPool(nIndex, keypool);
        }
        else if (strType == "minversion")
        {
            int version;
            ssValue >> version;
            if (!ssKey.empty() || !ssValue.empty() || version < 0 || version > CLIENT_VERSION) {
                strErr = "Invalid wallet minimum version";
                return false;
            }
            pwallet->LoadMinVersion(version);
        }
        else if (strType == "version")
        {
            ssValue >> wss.nFileVersion;
            if (wss.nFileVersion == 10300)
                wss.nFileVersion = 300;
        }
        else if (strType == "cscript")
        {
            uint160 hash;
            ssKey >> hash;
            CScript script;
            ssValue >> script;
            if (!pwallet->LoadCScript(script))
            {
                strErr = "Error reading wallet database: LoadCScript failed";
                return false;
            }
        }
        else if (strType == "orderposnext")
        {
            ssValue >> pwallet->nOrderPosNext;
        }
        else if (strType == "staking_enabled")
        {
            bool enabled = false;
            ssValue >> enabled;
            pwallet->LoadStakingEnabled(enabled);
        }
        else if (strType == "staking_reserve_balance")
        {
            CAmount reserve = 0;
            ssValue >> reserve;
            if (!MoneyRange(reserve)) return false;
            pwallet->LoadStakingReserveBalance(reserve);
        }
        else if (strType == "possigning")
        {
            PoSSigningKey key;
            PoSSigningHistory history;
            ssKey >> key;
            ssValue >> history;
            if (!ssKey.empty() || !ssValue.empty() ||
                !pwallet->LoadPoSSigningHistory(key, history)) return false;
        }
        else if (strType == "destdata")
        {
            std::string strAddress, strKey, strValue;
            ssKey >> strAddress;
            ssKey >> strKey;
            ssValue >> strValue;
            const auto destination = DecodeDestination(strAddress);
            if (!pwallet->LoadDestData(destination, strKey, strValue))
            {
                strErr = "Error reading wallet database: LoadDestData failed";
                return false;
            }
            wss.RecordPQAddress(strType, strAddress, strKey, strValue, destination);
        }
        else if (strType == "hdchain")
        {
            CHDChain chain;
            ssValue >> chain;
            if (!pwallet->SetHDChain(chain, true))
            {
                strErr = "Error reading wallet database: SetHDChain failed";
                return false;
            }
        } else if (strType != "bestblock" && strType != "bestblock_nomerkle"){
            wss.m_unknown_records++;
        }
    } catch (...)
    {
        return false;
    }
    return true;
}

bool WalletBatch::IsKeyType(const std::string& strType)
{
    return (strType== "key" || strType == "wkey" ||
            strType == "mkey" || strType == "ckey" ||
            strType == "pqkey" || strType == "cpqkey" || strType == "pqpolicy" ||
            strType == "smsgpqkey" || strType == "csmsgpqkey" || strType == "smsgpqcontact");
}

DBErrors WalletBatch::LoadWallet(CWallet* pwallet)
{
    CWalletScanState wss;
    bool fNoncriticalErrors = false;
    DBErrors result = DBErrors::LOAD_OK;

    LOCK(pwallet->cs_wallet);
    try {
        int nMinVersion = 0;
        if (m_batch.Read((std::string)"minversion", nMinVersion))
        {
            if (nMinVersion > CLIENT_VERSION)
                return DBErrors::TOO_NEW;
            pwallet->LoadMinVersion(nMinVersion);
        }

        // Get cursor
        Dbc* pcursor = m_batch.GetCursor();
        if (!pcursor)
        {
            LogPrintf("Error getting wallet database cursor\n");
            return DBErrors::CORRUPT;
        }

        while (true)
        {
            // Read next record
            CDataStream ssKey(SER_DISK, CLIENT_VERSION);
            CDataStream ssValue(SER_DISK, CLIENT_VERSION);
            int ret = m_batch.ReadAtCursor(pcursor, ssKey, ssValue);
            if (ret == DB_NOTFOUND)
                break;
            else if (ret != 0)
            {
                LogPrintf("Error reading next record from wallet database\n");
                return DBErrors::CORRUPT;
            }

            // Try to be tolerant of single corrupt records:
            std::string strType, strErr;
            if (!ReadKeyValue(pwallet, ssKey, ssValue, wss, strType, strErr))
            {
                // losing keys is considered a catastrophic error, anything else
                // we assume the user can live with:
                if (IsKeyType(strType) || strType == "defaultkey" || strType == "minversion" ||
                    strType == "possigning")
                    result = DBErrors::CORRUPT;
                else
                {
                    // Leave other errors alone, if we try to fix them we might make things worse.
                    fNoncriticalErrors = true; // ... but do warn the user there is something wrong.
                    if (strType == "tx")
                        // Rescan if there is a bad transaction record:
                        gArgs.SoftSetBoolArg("-rescan", true);
                }
            }
            if (!strErr.empty())
                LogPrintf("%s\n", strErr);
        }
        pcursor->close();
    }
    catch (const boost::thread_interrupted&) {
        throw;
    }
    catch (...) {
        result = DBErrors::CORRUPT;
    }

    if (fNoncriticalErrors && result == DBErrors::LOAD_OK)
        result = DBErrors::NONCRITICAL_ERROR;

    // Any wallet corruption at all: skip any rewriting or
    // upgrading, we don't want to make it worse.
    if (result != DBErrors::LOAD_OK)
        return result;
    if (wss.fHasPQKeys && pwallet->GetVersion() < FEATURE_PQ_KEYS) {
        LogPrintf("PQ wallet keys lack their downgrade-protection version\n");
        return DBErrors::CORRUPT;
    }
    if (wss.fHasSmsgPQKeys && pwallet->GetVersion() < FEATURE_SMSG_PQ_KEYS) {
        LogPrintf("PQ messaging keys lack their downgrade-protection version\n");
        return DBErrors::CORRUPT;
    }

    LogPrintf("nFileVersion = %d\n", wss.nFileVersion);

    LogPrintf("Keys: %u plaintext, %u encrypted, %u w/ metadata, %u total. Unknown wallet records: %u\n",
           wss.nKeys, wss.nCKeys, wss.nKeyMeta, wss.nKeys + wss.nCKeys, wss.m_unknown_records);

    // nTimeFirstKey is only reliable if all keys have metadata
    if ((wss.nKeys + wss.nCKeys + wss.nWatchKeys) != wss.nKeyMeta)
        pwallet->UpdateTimeFirstKey(1);

    for (uint256 hash : wss.vWalletUpgrade)
        WriteTx(pwallet->mapWallet.at(hash));

    // Rewrite encrypted wallets of versions 0.4.0 and 0.5.0rc:
    if (wss.fIsEncrypted && (wss.nFileVersion == 40000 || wss.nFileVersion == 50000))
        return DBErrors::NEED_REWRITE;

    if (!wss.pq_address_records.empty()) {
        // Bound BDB locks while renaming metadata; canonical records take precedence.
        const auto move_record = [&](const auto& old_key, const auto& new_key, std::string& value) {
            if (m_batch.Exists(new_key)) {
                if (!m_batch.Read(new_key, value)) return false;
            } else if (!WriteIC(new_key, value)) {
                return false;
            }
            return EraseIC(old_key);
        };
        constexpr size_t BATCH_SIZE = 1000;
        for (size_t start = 0; start < wss.pq_address_records.size(); start += BATCH_SIZE) {
            if (!TxnBegin()) return DBErrors::LOAD_FAIL;
            bool ok = true;
            const size_t end = std::min(start + BATCH_SIZE, wss.pq_address_records.size());
            for (size_t i = start; i < end && ok; ++i) {
                auto& record = wss.pq_address_records[i];
                const auto canonical = EncodeDestination(record.destination);
                if (record.type == "destdata") {
                    ok = move_record(std::make_pair(record.type, std::make_pair(record.address, record.key)),
                                     std::make_pair(record.type, std::make_pair(canonical, record.key)), record.value);
                } else {
                    ok = move_record(std::make_pair(record.type, record.address),
                                     std::make_pair(record.type, canonical), record.value);
                }
            }
            if (!ok || !TxnCommit(true)) {
                TxnAbort();
                return DBErrors::LOAD_FAIL;
            }
        }
        for (const auto& record : wss.pq_address_records) {
            auto& entry = pwallet->mapAddressBook[record.destination];
            if (record.type == "name") entry.name = record.value;
            else if (record.type == "purpose") entry.purpose = record.value;
            else entry.destdata[record.key] = record.value;
        }
    }

    if (wss.nFileVersion < CLIENT_VERSION) // Update
        WriteVersion(CLIENT_VERSION);

    if (wss.fAnyUnordered)
        result = pwallet->ReorderTransactions();

    pwallet->laccentries.clear();
    ListAccountCreditDebit("*", pwallet->laccentries);
    for (CAccountingEntry& entry : pwallet->laccentries) {
        pwallet->wtxOrdered.insert(make_pair(entry.nOrderPos, CWallet::TxPair(nullptr, &entry)));
    }

    return result;
}

DBErrors WalletBatch::FindWalletTx(std::vector<uint256>& vTxHash, std::vector<CWalletTx>& vWtx)
{
    DBErrors result = DBErrors::LOAD_OK;

    try {
        int nMinVersion = 0;
        if (m_batch.Read((std::string)"minversion", nMinVersion))
        {
            if (nMinVersion > CLIENT_VERSION)
                return DBErrors::TOO_NEW;
        }

        // Get cursor
        Dbc* pcursor = m_batch.GetCursor();
        if (!pcursor)
        {
            LogPrintf("Error getting wallet database cursor\n");
            return DBErrors::CORRUPT;
        }

        while (true)
        {
            // Read next record
            CDataStream ssKey(SER_DISK, CLIENT_VERSION);
            CDataStream ssValue(SER_DISK, CLIENT_VERSION);
            int ret = m_batch.ReadAtCursor(pcursor, ssKey, ssValue);
            if (ret == DB_NOTFOUND)
                break;
            else if (ret != 0)
            {
                LogPrintf("Error reading next record from wallet database\n");
                return DBErrors::CORRUPT;
            }

            std::string strType;
            ssKey >> strType;
            if (strType == "tx") {
                uint256 hash;
                ssKey >> hash;

                CWalletTx wtx(nullptr /* pwallet */, MakeTransactionRef());
                ssValue >> wtx;

                vTxHash.push_back(hash);
                vWtx.push_back(wtx);
            }
        }
        pcursor->close();
    }
    catch (const boost::thread_interrupted&) {
        throw;
    }
    catch (...) {
        result = DBErrors::CORRUPT;
    }

    return result;
}

DBErrors WalletBatch::ZapSelectTx(std::vector<uint256>& vTxHashIn, std::vector<uint256>& vTxHashOut)
{
    // build list of wallet TXs and hashes
    std::vector<uint256> vTxHash;
    std::vector<CWalletTx> vWtx;
    DBErrors err = FindWalletTx(vTxHash, vWtx);
    if (err != DBErrors::LOAD_OK) {
        return err;
    }

    std::sort(vTxHash.begin(), vTxHash.end());
    std::sort(vTxHashIn.begin(), vTxHashIn.end());

    // erase each matching wallet TX
    bool delerror = false;
    std::vector<uint256>::iterator it = vTxHashIn.begin();
    for (uint256 hash : vTxHash) {
        while (it < vTxHashIn.end() && (*it) < hash) {
            it++;
        }
        if (it == vTxHashIn.end()) {
            break;
        }
        else if ((*it) == hash) {
            if(!EraseTx(hash)) {
                LogPrint(BCLog::DB, "Transaction was found for deletion but returned database error: %s\n", hash.GetHex());
                delerror = true;
            }
            vTxHashOut.push_back(hash);
        }
    }

    if (delerror) {
        return DBErrors::CORRUPT;
    }
    return DBErrors::LOAD_OK;
}

DBErrors WalletBatch::ZapWalletTx(std::vector<CWalletTx>& vWtx)
{
    // build list of wallet TXs
    std::vector<uint256> vTxHash;
    DBErrors err = FindWalletTx(vTxHash, vWtx);
    if (err != DBErrors::LOAD_OK)
        return err;

    // erase each wallet TX
    for (uint256& hash : vTxHash) {
        if (!EraseTx(hash))
            return DBErrors::CORRUPT;
    }

    return DBErrors::LOAD_OK;
}

void MaybeCompactWalletDB()
{
    static std::atomic<bool> fOneThread(false);
    if (fOneThread.exchange(true)) {
        return;
    }
    if (!gArgs.GetBoolArg("-flushwallet", DEFAULT_FLUSHWALLET)) {
        return;
    }

    for (const std::shared_ptr<CWallet>& pwallet : GetWallets()) {
        WalletDatabase& dbh = pwallet->GetDBHandle();

        unsigned int nUpdateCounter = dbh.nUpdateCounter;

        if (dbh.nLastSeen != nUpdateCounter) {
            dbh.nLastSeen = nUpdateCounter;
            dbh.nLastWalletUpdate = GetTime();
        }

        if (dbh.nLastFlushed != nUpdateCounter && GetTime() - dbh.nLastWalletUpdate >= 2) {
            if (BerkeleyBatch::PeriodicFlush(dbh)) {
                dbh.nLastFlushed = nUpdateCounter;
            }
        }
    }

    fOneThread = false;
}

//
// Try to (very carefully!) recover wallet file if there is a problem.
//
bool WalletBatch::Recover(const fs::path& wallet_path, void *callbackDataIn, bool (*recoverKVcallback)(void* callbackData, CDataStream ssKey, CDataStream ssValue), std::string& out_backup_filename)
{
    return BerkeleyBatch::Recover(wallet_path, callbackDataIn, recoverKVcallback, out_backup_filename);
}

bool WalletBatch::Recover(const fs::path& wallet_path, std::string& out_backup_filename)
{
    // recover without a key filter callback
    // results in recovering all record types
    return WalletBatch::Recover(wallet_path, nullptr, nullptr, out_backup_filename);
}

bool WalletBatch::RecoverKeysOnlyFilter(void *callbackData, CDataStream ssKey, CDataStream ssValue)
{
    CWallet *dummyWallet = reinterpret_cast<CWallet*>(callbackData);
    CWalletScanState dummyWss;
    std::string strType, strErr;
    bool fReadOK;
    {
        // Required in LoadKeyMetadata():
        LOCK(dummyWallet->cs_wallet);
        fReadOK = ReadKeyValue(dummyWallet, ssKey, ssValue,
                               dummyWss, strType, strErr);
    }
    if (!IsKeyType(strType) && strType != "hdchain" && strType != "possigning" &&
        strType != "minversion")
        return false;
    if (!fReadOK)
    {
        LogPrintf("WARNING: WalletBatch::Recover skipping %s: %s\n", strType, strErr);
        return false;
    }

    return true;
}

bool WalletBatch::VerifyEnvironment(const fs::path& wallet_path, std::string& errorStr)
{
    return BerkeleyBatch::VerifyEnvironment(wallet_path, errorStr);
}

bool WalletBatch::VerifyDatabaseFile(const fs::path& wallet_path, std::string& warningStr, std::string& errorStr)
{
    return BerkeleyBatch::VerifyDatabaseFile(wallet_path, warningStr, errorStr, WalletBatch::Recover);
}

bool WalletBatch::WriteDestData(const std::string &address, const std::string &key, const std::string &value)
{
    return WriteIC(std::make_pair(std::string("destdata"), std::make_pair(address, key)), value);
}

bool WalletBatch::EraseDestData(const std::string &address, const std::string &key)
{
    return EraseIC(std::make_pair(std::string("destdata"), std::make_pair(address, key)));
}


bool WalletBatch::WriteHDChain(const CHDChain& chain)
{
    return WriteIC(std::string("hdchain"), chain);
}

bool WalletBatch::TxnBegin()
{
    return m_batch.TxnBegin();
}

bool WalletBatch::WritePoSSigningHistory(const PoSSigningKey& key,
                                       const PoSSigningHistory& history)
{
    return WriteIC(std::make_pair(std::string("possigning"), key), history);
}

bool WalletBatch::TxnCommit(bool sync)
{
    return m_batch.TxnCommit(sync);
}

bool WalletBatch::TxnAbort()
{
    return m_batch.TxnAbort();
}

bool WalletBatch::ReadVersion(int& nVersion)
{
    return m_batch.ReadVersion(nVersion);
}

bool WalletBatch::WriteVersion(int nVersion)
{
    return m_batch.WriteVersion(nVersion);
}
