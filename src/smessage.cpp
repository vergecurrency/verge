// Copyright (c) 2014 The ShadowCoin developers
// Copyright (c) 2018 Verge
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/*
Notes:
    Running with -debug could leave to and from address hashes and public keys in the log.
    
    
    parameters:
        -nosmsg             Disable secure messaging (fNoSmsg)
        -debugsmsg          Show extra debug messages (fDebug)
        -smsgscanchain      Scan the block chain for public key addresses on startup
    
    
    Wallet Locked
        A copy of each incoming message is stored in bucket files ending in _wl.dat
        wl (wallet locked) bucket files are deleted if they expire, like normal buckets
        When the wallet is unlocked all the messages in wl files are scanned.
    
    
    Address Whitelist
        Owned Addresses are stored in smsgAddresses vector
        Saved to smsg.ini
        Modify options using the smsglocalkeys rpc command or edit the smsg.ini file (with client closed)
        
    
*/

#include <smessage.h>

#include <stdint.h>
#include <time.h>
#include <map>
#include <stdexcept>
#include <sstream>
#include <errno.h>

#include <openssl/crypto.h>
#include <openssl/ec.h>
#include <openssl/ecdh.h>
#include <openssl/sha.h>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl_compat.h>

// #include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>


#include <base58.h>
#include <db.h>
#include <init.h> // pwalletMain
#include <txdb.h>


#include <lz4/lz4.c>
#include <xxhash/xxhash.h>
#include <xxhash/xxhash.c>


// On 64 bit system ld is 64bits
#ifdef IS_ARCH_64
#undef PRId64
#undef PRIu64
#undef PRIx64
#define PRId64  "ld"
#define PRIu64  "lu"
#define PRIx64  "lx"
#endif // IS_ARCH_64

#if OPENSSL_VERSION_NUMBER < 0x10100000L
#define EC_KEY_SET_DEFAULT_METHOD(key) ECDSA_set_method((key), ECDSA_OpenSSL())
#else
#define EC_KEY_SET_DEFAULT_METHOD(key) EC_KEY_set_method((key), EC_KEY_OpenSSL())
#endif

// TODO: For buckets older than current, only need to store no. messages and hash in memory

boost::signals2::signal<void (SecMsgStored& inboxHdr)>  NotifySecMsgInboxChanged;
boost::signals2::signal<void (SecMsgStored& outboxHdr)> NotifySecMsgOutboxChanged;
boost::signals2::signal<void ()> NotifySecMsgWalletUnlocked;

bool fSecMsgEnabled = false;

std::map<int64_t, SecMsgBucket> smsgBuckets;
std::vector<SecMsgAddress>      smsgAddresses;
SecMsgOptions                   smsgOptions;

uint32_t nPeerIdCounter = 1;



CCriticalSection cs_smsg;
CCriticalSection cs_smsgDB;

leveldb::DB *smsgDB = NULL;


bool SecMsgCrypter::SetKey(const std::vector<unsigned char>& vchNewKey, unsigned char* chNewIV)
{
    
    if (vchNewKey.size() < sizeof(chKey))
        return false;
    
    return SetKey(&vchNewKey[0], chNewIV);
};

bool SecMsgCrypter::SetKey(const unsigned char* chNewKey, unsigned char* chNewIV)
{
    // -- for EVP_aes_256_cbc() key must be 256 bit, iv must be 128 bit.
    memcpy(&chKey[0], chNewKey, sizeof(chKey));
    memcpy(chIV, chNewIV, sizeof(chIV));
    
    fKeySet = true;
    return true;
};

bool SecMsgCrypter::Encrypt(unsigned char* chPlaintext, uint32_t nPlain, std::vector<unsigned char> &vchCiphertext)
{
    if (!fKeySet)
        return false;
    
    // -- max ciphertext len for a n bytes of plaintext is n + AES_BLOCK_SIZE - 1 bytes
    int nLen = nPlain;
    
    int nCLen = nLen + AES_BLOCK_SIZE, nFLen = 0;
    vchCiphertext = std::vector<unsigned char> (nCLen);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

    bool fOk = (ctx != NULL);

    if (fOk) fOk = EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, &chKey[0], &chIV[0]);
    if (fOk) fOk = EVP_EncryptUpdate(ctx, &vchCiphertext[0], &nCLen, chPlaintext, nLen);
    if (fOk) fOk = EVP_EncryptFinal_ex(ctx, (&vchCiphertext[0])+nCLen, &nFLen);
    EVP_CIPHER_CTX_cleanup(ctx);

    if (!fOk)
        return false;

    vchCiphertext.resize(nCLen + nFLen);
    
    return true;
};

bool SecMsgCrypter::Decrypt(unsigned char* chCiphertext, uint32_t nCipher, std::vector<unsigned char>& vchPlaintext)
{
    if (!fKeySet)
        return false;
    
    // plaintext will always be equal to or lesser than length of ciphertext
    int nPLen = nCipher, nFLen = 0;
    
    vchPlaintext.resize(nCipher);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

    bool fOk = (ctx != NULL);

    if (fOk) fOk = EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, &chKey[0], &chIV[0]);
    if (fOk) fOk = EVP_DecryptUpdate(ctx, &vchPlaintext[0], &nPLen, &chCiphertext[0], nCipher);
    if (fOk) fOk = EVP_DecryptFinal_ex(ctx, (&vchPlaintext[0])+nPLen, &nFLen);
    EVP_CIPHER_CTX_free(ctx);

    if (!fOk)
        return false;
    
    vchPlaintext.resize(nPLen + nFLen);
    
    return true;
};

void SecMsgBucket::hashBucket()
{
    if (fDebug)
        printf("SecMsgBucket::hashBucket()\n");
    
    timeChanged = GetTime();
    
    std::set<SecMsgToken>::iterator it;
    
    void* state = XXH32_init(1);
    
    for (it = setTokens.begin(); it != setTokens.end(); ++it)
    {
        XXH32_update(state, it->sample, 8);
    };
    
    hash = XXH32_digest(state);
    
    if (fDebug)
        printf("Hashed %" PRIszu " messages, hash %u\n", setTokens.size(), hash);
};


bool SecMsgDB::Open(const char* pszMode)
{
    if (smsgDB)
    {
        pdb = smsgDB;
        return true;
    };
    
    bool fCreate = strchr(pszMode, 'c');
    
    fs::path fullpath = GetDataDir() / "smsgDB";
    
    if (!fCreate
        && (!fs::exists(fullpath)
            || !fs::is_directory(fullpath)))
    {
        printf("SecMsgDB::open() - DB does not exist.\n");
        return false;
    };
    
    leveldb::Options options;
    options.create_if_missing = fCreate;
    leveldb::Status s = leveldb::DB::Open(options, fullpath.string(), &smsgDB);
    
    if (!s.ok())
    {
        printf("SecMsgDB::open() - Error opening db: %s.\n", s.ToString().c_str());
        return false;
    };
    
    pdb = smsgDB;
    
    return true;
};


class SecMsgBatchScanner : public leveldb::WriteBatch::Handler
{
public:
    std::string needle;
    bool* deleted;
    std::string* foundValue;
    bool foundEntry;
    
    SecMsgBatchScanner() : foundEntry(false) {}
    
    virtual void Put(const leveldb::Slice& key, const leveldb::Slice& value)
    {
        if (key.ToString() == needle)
        {
            foundEntry = true;
            *deleted = false;
            *foundValue = value.ToString();
        };
    };
    
    virtual void Delete(const leveldb::Slice& key)
    {
        if (key.ToString() == needle)
        {
            foundEntry = true;
            *deleted = true;
        };
    };
};

// When performing a read, if we have an active batch we need to check it first
// before reading from the database, as the rest of the code assumes that once
// a database transaction begins reads are consistent with it. It would be good
// to change that assumption in future and avoid the performance hit, though in
// practice it does not appear to be large.
bool SecMsgDB::ScanBatch(const CDataStream& key, std::string* value, bool* deleted) const
{
    if (!activeBatch)
        return false;
    
    *deleted = false;
    SecMsgBatchScanner scanner;
    scanner.needle = key.str();
    scanner.deleted = deleted;
    scanner.foundValue = value;
    leveldb::Status s = activeBatch->Iterate(&scanner);
    if (!s.ok())
    {
        printf("SecMsgDB ScanBatch error: %s\n", s.ToString().c_str());
        return false;
    };
    
    return scanner.foundEntry;
}

bool SecMsgDB::TxnBegin()
{
    if (activeBatch)
        return true;
    // Modern C++ Migration: Use smart pointer for automatic memory management
    activeBatch = std::make_unique<leveldb::WriteBatch>();
    return true;
}

bool SecMsgDB::TxnCommit()
{
    if (!activeBatch)
        return false;
    
    leveldb::WriteOptions writeOptions;
    writeOptions.sync = true;
    leveldb::Status status = pdb->Write(writeOptions, activeBatch.get());
    // Modern C++ Migration: Smart pointer automatically cleans up when reset
    activeBatch.reset();
    
    if (!status.ok())
    {
        printf("SecMsgDB batch commit failure: %s\n", status.ToString().c_str());
        return false;
    }
    
    return true;
}

bool SecMsgDB::TxnAbort()
{
    // Modern C++ Migration: Smart pointer cleanup
    activeBatch.reset();
    return true;
}

bool SecMsgDB::ReadPK(CKeyID& addr, CPubKey& pubkey)
{
    if (!pdb)
        return false;
    
    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    ssKey.reserve(sizeof(addr) + 2);
    ssKey << 'p';
    ssKey << 'k';
    ssKey << addr;
    std::string strValue;

    bool readFromDb = true;
    if (activeBatch)
    {
        // -- check activeBatch first
        bool deleted = false;
        readFromDb = ScanBatch(ssKey, &strValue, &deleted) == false;
        if (deleted)
            return false;
    };
    
    if (readFromDb)
    {
        leveldb::Status s = pdb->Get(leveldb::ReadOptions(), ssKey.str(), &strValue);
        if (!s.ok())
        {
            if (s.IsNotFound())
                return false;
            printf("LevelDB read failure: %s\n", s.ToString().c_str());
            return false;
        };
    };
    
    try {
        CDataStream ssValue(strValue.data(), strValue.data() + strValue.size(), SER_DISK, CLIENT_VERSION);
        ssValue >> pubkey;
    } catch (std::exception& e) {
        printf("SecMsgDB::ReadPK() unserialize threw: %s.\n", e.what());
        return false;
    }
    
    return true;
};

bool SecMsgDB::WritePK(CKeyID& addr, CPubKey& pubkey)
{
    if (!pdb)
        return false;
    
    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    ssKey.reserve(sizeof(addr) + 2);
    ssKey << 'p';
    ssKey << 'k';
    ssKey << addr;
    CDataStream ssValue(SER_DISK, CLIENT_VERSION);
    ssValue.reserve(sizeof(pubkey));
    ssValue << pubkey;

    if (activeBatch)
    {
        activeBatch->Put(ssKey.str(), ssValue.str());
        return true;
    };
    
    leveldb::WriteOptions writeOptions;
    writeOptions.sync = true;
    leveldb::Status s = pdb->Put(writeOptions, ssKey.str(), ssValue.str());
    if (!s.ok())
    {
        printf("SecMsgDB write failure: %s\n", s.ToString().c_str());
        return false;
    };
    
    return true;
};

bool SecMsgDB::ExistsPK(CKeyID& addr)
{
    if (!pdb)
        return false;
    
    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    ssKey.reserve(sizeof(addr)+2);
    ssKey << 'p';
    ssKey << 'k';
    ssKey << addr;
    std::string unused;
    
    if (activeBatch)
    {
        bool deleted;
        if (ScanBatch(ssKey, &unused, &deleted) && !deleted)
        {
            return true;
        };
    };
    
    leveldb::Status s = pdb->Get(leveldb::ReadOptions(), ssKey.str(), &unused);
    return s.IsNotFound() == false;
};


bool SecMsgDB::NextSmesg(leveldb::Iterator* it, std::string& prefix, unsigned char* chKey, SecMsgStored& smsgStored)
{
    if (!pdb)
        return false;
    
    if (!it->Valid()) // first run
        it->Seek(prefix);
    else
        it->Next();
    
    if (!(it->Valid()
        && it->key().size() == 18
        && memcmp(it->key().data(), prefix.data(), 2) == 0))
        return false;
    
    memcpy(chKey, it->key().data(), 18);
    
    try {
        CDataStream ssValue(it->value().data(), it->value().data() + it->value().size(), SER_DISK, CLIENT_VERSION);
        ssValue >> smsgStored;
    } catch (std::exception& e) {
        printf("SecMsgDB::NextSmesg() unserialize threw: %s.\n", e.what());
        return false;
    }
    
    return true;
};

bool SecMsgDB::NextSmesgKey(leveldb::Iterator* it, std::string& prefix, unsigned char* chKey)
{
    if (!pdb)
        return false;
    
    if (!it->Valid()) // first run
        it->Seek(prefix);
    else
        it->Next();
    
    if (!(it->Valid()
        && it->key().size() == 18
        && memcmp(it->key().data(), prefix.data(), 2) == 0))
        return false;
    
    memcpy(chKey, it->key().data(), 18);
    
    return true;
};

bool SecMsgDB::ReadSmesg(unsigned char* chKey, SecMsgStored& smsgStored)
{
    if (!pdb)
        return false;
    
    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    ssKey.write((const char*)chKey, 18);
    std::string strValue;

    bool readFromDb = true;
    if (activeBatch)
    {
        // -- check activeBatch first
        bool deleted = false;
        readFromDb = ScanBatch(ssKey, &strValue, &deleted) == false;
        if (deleted)
            return false;
    };
    
    if (readFromDb)
    {
        leveldb::Status s = pdb->Get(leveldb::ReadOptions(), ssKey.str(), &strValue);
        if (!s.ok())
        {
            if (s.IsNotFound())
                return false;
            printf("LevelDB read failure: %s\n", s.ToString().c_str());
            return false;
        };
    };
    
    try {
        CDataStream ssValue(strValue.data(), strValue.data() + strValue.size(), SER_DISK, CLIENT_VERSION);
        ssValue >> smsgStored;
    } catch (std::exception& e) {
        printf("SecMsgDB::ReadSmesg() unserialize threw: %s.\n", e.what());
        return false;
    }
    
    return true;
};

bool SecMsgDB::WriteSmesg(unsigned char* chKey, SecMsgStored& smsgStored)
{
    if (!pdb)
        return false;
    
    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    ssKey.write((const char*)chKey, 18);
    CDataStream ssValue(SER_DISK, CLIENT_VERSION);
    ssValue << smsgStored;

    if (activeBatch)
    {
        activeBatch->Put(ssKey.str(), ssValue.str());
        return true;
    };
    
    leveldb::WriteOptions writeOptions;
    writeOptions.sync = true;
    leveldb::Status s = pdb->Put(writeOptions, ssKey.str(), ssValue.str());
    if (!s.ok())
    {
        printf("SecMsgDB write failed: %s\n", s.ToString().c_str());
        return false;
    };
    
    return true;
};

bool SecMsgDB::ExistsSmesg(unsigned char* chKey)
{
    if (!pdb)
        return false;
    
    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    ssKey.write((const char*)chKey, 18);
    std::string unused;
    
    if (activeBatch)
    {
        bool deleted;
        if (ScanBatch(ssKey, &unused, &deleted) && !deleted)
        {
            return true;
        };
    };
    
    leveldb::Status s = pdb->Get(leveldb::ReadOptions(), ssKey.str(), &unused);
    return s.IsNotFound() == false;
    return true;
};

bool SecMsgDB::EraseSmesg(unsigned char* chKey)
{
    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    ssKey.write((const char*)chKey, 18);
    
    if (activeBatch)
    {
        activeBatch->Delete(ssKey.str());
        return true;
    };
    
    leveldb::WriteOptions writeOptions;
    writeOptions.sync = true;
    leveldb::Status s = pdb->Delete(writeOptions, ssKey.str());
    
    if (s.ok() || s.IsNotFound())
        return true;
    printf("SecMsgDB erase failed: %s\n", s.ToString().c_str());
    return false;
};

void ThreadSecureMsg(void* parg)
{
    // -- bucket management thread
    RenameThread("shadowcoin-smsg"); // Make this thread recognisable
    
    uint32_t delay = 0;
    
    while (fSecMsgEnabled)
    {
        // shutdown thread waits 5 seconds, this should be less
        MilliSleep(1000); // milliseconds
        
        if (!fSecMsgEnabled) // check again after sleep
            break;
        
        delay++;
        if (delay < SMSG_THREAD_DELAY) // check every SMSG_THREAD_DELAY seconds
            continue;
        delay = 0;
        
        int64_t now = GetTime();
        
        if (fDebug)
            printf("SecureMsgThread %" PRId64 " \n", now);
        
        int64_t cutoffTime = now - SMSG_RETENTION;
        
        {
            LOCK(cs_smsg);
            std::map<int64_t, SecMsgBucket>::iterator it;
            it = smsgBuckets.begin();
            
            while (it != smsgBuckets.end())
            {
                //if (fDebug)
                //    printf("Checking bucket %"PRId64", size %"PRIszu" \n", it->first, it->second.setTokens.size());
                if (it->first < cutoffTime)
                {
                    if (fDebug)
                        printf("Removing bucket %" PRId64 " \n", it->first);
                    std::string fileName = boost::lexical_cast<std::string>(it->first) + "_01.dat";
                    fs::path fullPath = GetDataDir() / "smsgStore" / fileName;
                    if (fs::exists(fullPath))
                    {
                        try {
                            fs::remove(fullPath);
                        } catch (const fs::filesystem_error& ex)
                        {
                            printf("Error removing bucket file %s.\n", ex.what());
                        };
                    } else
                        printf("Path %s does not exist \n", fullPath.string().c_str());
                    
                    // -- look for a wl file, it stores incoming messages when wallet is locked
                    fileName = boost::lexical_cast<std::string>(it->first) + "_01_wl.dat";
                    fullPath = GetDataDir() / "smsgStore" / fileName;
                    if (fs::exists(fullPath))
                    {
                        try {
                            fs::remove(fullPath);
                        } catch (const fs::filesystem_error& ex)
                        {
                            printf("Error removing wallet locked file %s.\n", ex.what());
                        };
                    };
                    
                    smsgBuckets.erase(it++);
                } else
                {
                    // -- tick down nLockCount, so will eventually expire if peer never sends data
                    if (it->second.nLockCount > 0)
                    {
                        it->second.nLockCount--;
                        
                        if (it->second.nLockCount == 0)     // lock timed out
                        {
                            uint32_t    nPeerId     = it->second.nLockPeerId;
                            int64_t     ignoreUntil = GetTime() + SMSG_TIME_IGNORE;
                            
                            if (fDebug)
                                printf("Lock on bucket %" PRId64 " for peer %u timed out.\n", it->first, nPeerId);
                            // Modern C++ Migration: Range-based for loop
                            LOCK(cs_vNodes);
                            for (CNode* pnode : vNodes)
                            {
                                if (pnode->smsgData.nPeerId != nPeerId)
                                    continue;
                                pnode->smsgData.ignoreUntil = ignoreUntil;
                                
                                // -- alert peer that they are being ignored
                                std::vector<unsigned char> vchData;
                                vchData.resize(8);
                                memcpy(&vchData[0], &ignoreUntil, 8);
                                pnode->PushMessage("smsgIgnore", vchData);
                                
                                if (fDebug)
                                    printf("This node will ignore peer %u until %" PRId64 ".\n", nPeerId, ignoreUntil);
                                break;
                            }
                            it->second.nLockPeerId = 0;
                        }; // if (it->second.nLockCount == 0)
                    };
                    ++it;
                }; // ! if (it->first < cutoffTime)
            };
        }; // LOCK(cs_smsg);
    };
    
    printf("ThreadSecureMsg exited.\n");
};

void ThreadSecureMsgPow(void* parg)
{
    // -- proof of work thread
    RenameThread("shadowcoin-smsg-pow"); // Make this thread recognisable
    
    int rv;
    std::vector<unsigned char> vchKey;
    SecMsgStored smsgStored;
    
    std::string sPrefix("qm");
    unsigned char chKey[18];
    
    
    while (fSecMsgEnabled)
    {
        // -- sleep at end, then fSecMsgEnabled is tested on wake
        
        SecMsgDB dbOutbox;
        leveldb::Iterator* it;
        {
            LOCK(cs_smsgDB);
            
            if (!dbOutbox.Open("cr+"))
                continue;
            
            // -- fifo (smallest key first)
            it = dbOutbox.pdb->NewIterator(leveldb::ReadOptions());
        }
        // -- break up lock, SecureMsgSetHash will take long
        
        for (;;)
        {
            {
                LOCK(cs_smsgDB); 
                if (!dbOutbox.NextSmesg(it, sPrefix, chKey, smsgStored))
                    break;
            }
            
            unsigned char* pHeader = &smsgStored.vchMessage[0];
            unsigned char* pPayload = &smsgStored.vchMessage[SMSG_HDR_LEN];
            SecureMessage* psmsg = (SecureMessage*) pHeader;
            
            // -- do proof of work
            rv = SecureMsgSetHash(pHeader, pPayload, psmsg->nPayload);
            if (rv == 2) 
                break; // /eave message in db, if terminated due to shutdown
            
            // -- message is removed here, no matter what
            {
                LOCK(cs_smsgDB);
                dbOutbox.EraseSmesg(chKey);
            }
            if (rv != 0)
            {
                printf("SecMsgPow: Could not get proof of work hash, message removed.\n");
                continue;
            };
            
            // -- add to message store
            {
                LOCK(cs_smsg);
                if (SecureMsgStore(pHeader, pPayload, psmsg->nPayload, true) != 0)
                {
                    printf("SecMsgPow: Could not place message in buckets, message removed.\n");
                    continue;
                };
            }
            
            // -- test if message was sent to self
            if (SecureMsgScanMessage(pHeader, pPayload, psmsg->nPayload, true) != 0)
            {
                // message recipient is not this node (or failed)
            };
        };
        
        {
            LOCK(cs_smsg);
            delete it;
        }
        
        // -- shutdown thread waits 5 seconds, this should be less
        MilliSleep(1000); // milliseconds
    };
    
    printf("ThreadSecureMsgPow exited.\n");
};


std::string getTimeString(int64_t timestamp, char *buffer, size_t nBuffer)
{
    struct tm* dt;
    time_t t = timestamp;
    dt = localtime(&t);
    
    strftime(buffer, nBuffer, "%Y-%m-%d %H:%M:%S %z", dt); // %Z shows long strings on windows
    return std::string(buffer); // copies the null-terminated character sequence
};

std::string fsReadable(uint64_t nBytes)
{
    char buffer[128];
    if (nBytes >= 1024ll*1024ll*1024ll*1024ll)
        snprintf(buffer, sizeof(buffer), "%.2f TB", nBytes/1024.0/1024.0/1024.0/1024.0);
    else
    if (nBytes >= 1024*1024*1024)
        snprintf(buffer, sizeof(buffer), "%.2f GB", nBytes/1024.0/1024.0/1024.0);
    else
    if (nBytes >= 1024*1024)
        snprintf(buffer, sizeof(buffer), "%.2f MB", nBytes/1024.0/1024.0);
    else
    if (nBytes >= 1024)
        snprintf(buffer, sizeof(buffer), "%.2f KB", nBytes/1024.0);
    else
        snprintf(buffer, sizeof(buffer), "%" PRIu64 " bytes", nBytes);
    return std::string(buffer);
};

int SecureMsgBuildBucketSet()
{
    /*
        Build the bucket set by scanning the files in the smsgStore dir.
        
        smsgBuckets should be empty
    */
    
    if (fDebug)
        printf("SecureMsgBuildBucketSet()\n");
        
    int64_t  now            = GetTime();
    uint32_t nFiles         = 0;
    uint32_t nMessages      = 0;
    
    fs::path pathSmsgDir = GetDataDir() / "smsgStore";
    fs::directory_iterator itend;
    
    
    if (!fs::exists(pathSmsgDir)
        || !fs::is_directory(pathSmsgDir))
    {
        printf("Message store directory does not exist.\n");
        return 0; // not an error
    }
    
    
    for (fs::directory_iterator itd(pathSmsgDir) ; itd != itend ; ++itd)
    {
        if (!fs::is_regular_file(itd->status()))
            continue;
        
        std::string fileType = (*itd).path().extension().string();
        
        if (fileType.compare(".dat") != 0)
            continue;
            
        std::string fileName = (*itd).path().filename().string();
        
        
        if (fDebug)
            printf("Processing file: %s.\n", fileName.c_str());
        
        nFiles++;
        
        // TODO files must be split if > 2GB
        // time_noFile.dat
        size_t sep = fileName.find_first_of("_");
        if (sep == std::string::npos)
            continue;
        
        std::string stime = fileName.substr(0, sep);
        
        int64_t fileTime = boost::lexical_cast<int64_t>(stime);
        
        if (fileTime < now - SMSG_RETENTION)
        {
            printf("Dropping file %s, expired.\n", fileName.c_str());
            try {
                fs::remove((*itd).path());
            } catch (const fs::filesystem_error& ex)
            {
                printf("Error removing bucket file %s, %s.\n", fileName.c_str(), ex.what());
            };
            continue;
        };
        
        if (boost::algorithm::ends_with(fileName, "_wl.dat"))
        {
            if (fDebug)
                printf("Skipping wallet locked file: %s.\n", fileName.c_str());
            continue;
        };
        
        
        SecureMessage smsg;
        std::set<SecMsgToken>& tokenSet = smsgBuckets[fileTime].setTokens;
        
        {
            LOCK(cs_smsg);
            FILE *fp;
            
            if (!(fp = fopen((*itd).path().string().c_str(), "rb")))
            {
                printf("Error opening file: %s\n", strerror(errno));
                continue;
            };
            
            for (;;)
            {
                long int ofs = ftell(fp);
                SecMsgToken token;
                token.offset = ofs;
                errno = 0;
                if (fread(&smsg.hash[0], sizeof(unsigned char), SMSG_HDR_LEN, fp) != (size_t)SMSG_HDR_LEN)
                {
                    if (errno != 0)
                    {
                        printf("fread header failed: %s\n", strerror(errno));
                    } else
                    {
                        //printf("End of file.\n");
                    };
                    break;
                };
                token.timestamp = smsg.timestamp;
                
                if (smsg.nPayload < 8)
                    continue;
                
                if (fread(token.sample, sizeof(unsigned char), 8, fp) != 8)
                {
                    printf("fread data failed: %s\n", strerror(errno));
                    break;
                };
                
                if (fseek(fp, smsg.nPayload-8, SEEK_CUR) != 0)
                {
                    printf("fseek, strerror: %s.\n", strerror(errno));
                    break;
                };
                
                tokenSet.insert(token);
            };
            
            fclose(fp);
        };
        smsgBuckets[fileTime].hashBucket();
        
        nMessages += tokenSet.size();
        
        if (fDebug)
            printf("Bucket %" PRId64 " contains %" PRIszu " messages.\n", fileTime, tokenSet.size());
    };
    
    printf("Processed %u files, loaded %" PRIszu " buckets containing %u messages.\n", nFiles, smsgBuckets.size(), nMessages);
    
    return 0;
};

int SecureMsgAddWalletAddresses()
{
    if (fDebug)
        printf("SecureMsgAddWalletAddresses()\n");
    
    uint32_t nAdded = 0;
    // Modern C++ Migration: Range-based for loop with auto type deduction
    for (const auto& entry : pwalletMain->mapAddressBook)
    {
        if (!IsMine(*pwalletMain, entry.first))
            continue;
        
        CBitcoinAddress coinAddress(entry.first);
        if (!coinAddress.IsValid())
            continue;
        
        std::string address;
        std::string strPublicKey;
        address = coinAddress.ToString();
        
        
        bool fExists        = 0;
        for (std::vector<SecMsgAddress>::iterator it = smsgAddresses.begin(); it != smsgAddresses.end(); ++it)
        {
            if (address != it->sAddress)
                continue;
            fExists = 1;
            break;
        };
        
        if (fExists)
            continue;
        
        bool recvEnabled    = 1;
        bool recvAnon       = 1;
        
        smsgAddresses.push_back(SecMsgAddress(address, recvEnabled, recvAnon));
        nAdded++;
    };
    
    if (fDebug)
        printf("Added %u addresses to whitelist.\n", nAdded);
    
    return 0;
};


int SecureMsgReadIni()
{
    if (!fSecMsgEnabled)
        return false;
    
    if (fDebug)
        printf("SecureMsgReadIni()\n");
    
    fs::path fullpath = GetDataDir() / "smsg.ini";
    
    
    FILE *fp;
    errno = 0;
    if (!(fp = fopen(fullpath.string().c_str(), "r")))
    {
        printf("Error opening file: %s\n", strerror(errno));
        return 1;
    };
    
    char cLine[512];
    char *pName, *pValue;
    
    char cAddress[64];
    int addrRecv, addrRecvAnon;
    
    while (fgets(cLine, 512, fp))
    {
        cLine[strcspn(cLine, "\n")] = '\0';
        cLine[strcspn(cLine, "\r")] = '\0';
        cLine[511] = '\0'; // for safety
        
        // -- check that line contains a name value pair and is not a comment, or section header
        if (cLine[0] == '#' || cLine[0] == '[' || strcspn(cLine, "=") < 1)
            continue;
        
        if (!(pName = strtok(cLine, "="))
            || !(pValue = strtok(NULL, "=")))
            continue;
        
        if (strcmp(pName, "newAddressRecv") == 0)
        {
            smsgOptions.fNewAddressRecv = (strcmp(pValue, "true") == 0) ? true : false;
        } else
        if (strcmp(pName, "newAddressAnon") == 0)
        {
            smsgOptions.fNewAddressAnon = (strcmp(pValue, "true") == 0) ? true : false;
        } else
        if (strcmp(pName, "key") == 0)
        {
            int rv = sscanf(pValue, "%64[^|]|%d|%d", cAddress, &addrRecv, &addrRecvAnon);
            if (rv == 3)
            {
                smsgAddresses.push_back(SecMsgAddress(std::string(cAddress), addrRecv, addrRecvAnon));
            } else
            {
                printf("Could not parse key line %s, rv %d.\n", pValue, rv);
            }
        } else
        {
            printf("Unknown setting name: '%s'.", pName);
        };
    };
    
    printf("Loaded %" PRIszu " addresses.\n", smsgAddresses.size());
    
    fclose(fp);
    
    return 0;
};

int SecureMsgWriteIni()
{
    if (!fSecMsgEnabled)
        return false;
    
    if (fDebug)
        printf("SecureMsgWriteIni()\n");
    
    fs::path fullpath = GetDataDir() / "smsg.ini~";
    
    FILE *fp;
    errno = 0;
    if (!(fp = fopen(fullpath.string().c_str(), "w")))
    {
        printf("Error opening file: %s\n", strerror(errno));
        return 1;
    };
    
    if (fwrite("[Options]\n", sizeof(char), 10, fp) != 10)
    {
        printf("fwrite error: %s\n", strerror(errno));
        fclose(fp);
        return false;
    };
    
    if (fprintf(fp, "newAddressRecv=%s\n", smsgOptions.fNewAddressRecv ? "true" : "false") < 0
        || fprintf(fp, "newAddressAnon=%s\n", smsgOptions.fNewAddressAnon ? "true" : "false") < 0)
    {
        printf("fprintf error: %s\n", strerror(errno));
        fclose(fp);
        return false;
    }
    
    if (fwrite("\n[Keys]\n", sizeof(char), 8, fp) != 8)
    {
        printf("fwrite error: %s\n", strerror(errno));
        fclose(fp);
        return false;
    };
    for (std::vector<SecMsgAddress>::iterator it = smsgAddresses.begin(); it != smsgAddresses.end(); ++it)
    {
        errno = 0;
        if (fprintf(fp, "key=%s|%d|%d\n", it->sAddress.c_str(), it->fReceiveEnabled, it->fReceiveAnon) < 0)
        {
            printf("fprintf error: %s\n", strerror(errno));
            continue;
        };
    };
    
    
    fclose(fp);
    
    
    try {
        fs::path finalpath = GetDataDir() / "smsg.ini";
        fs::rename(fullpath, finalpath);
    } catch (const fs::filesystem_error& ex)
    {
        printf("Error renaming file %s, %s.\n", fullpath.string().c_str(), ex.what());
    };
    return 0;
};


/** called from AppInit2() in init.cpp */
bool SecureMsgStart(bool fDontStart, bool fScanChain)
{
    if (fDontStart)
    {
        printf("Secure messaging not started.\n");
        return false;
    };
    
    printf("Secure messaging starting.\n");
    
    fSecMsgEnabled = true;
    
    if (SecureMsgReadIni() != 0)
        printf("Failed to read smsg.ini\n");
    
    if (smsgAddresses.size() < 1)
    {
        printf("No address keys loaded.\n");
        if (SecureMsgAddWalletAddresses() != 0)
            printf("Failed to load addresses from wallet.\n");
    };
    
    if (fScanChain)
    {
        SecureMsgScanBlockChain();
    };
    
    if (SecureMsgBuildBucketSet() != 0)
    {
        printf("SecureMsg could not load bucket sets, secure messaging disabled.\n");
        fSecMsgEnabled = false;
        return false;
    };
    
    // -- start threads
    if (!NewThread(ThreadSecureMsg, NULL)
        || !NewThread(ThreadSecureMsgPow, NULL))
    {
        printf("SecureMsg could not start threads, secure messaging disabled.\n");
        fSecMsgEnabled = false;
        return false;
    };
    
    return true;
};

/** called from Shutdown() in init.cpp */
bool SecureMsgShutdown()
{
    if (!fSecMsgEnabled)
        return false;
    
    printf("Stopping secure messaging.\n");
    
    
    if (SecureMsgWriteIni() != 0)
        printf("Failed to save smsg.ini\n");
    
    fSecMsgEnabled = false;
    
    if (smsgDB)
    {
        LOCK(cs_smsgDB);
        delete smsgDB;
        smsgDB = NULL;
    };
    
    // -- main program will wait 5 seconds for threads to terminate.
    
    return true;
};

bool SecureMsgEnable()
{
    // -- start secure messaging at runtime
    if (fSecMsgEnabled)
    {
        printf("SecureMsgEnable: secure messaging is already enabled.\n");
        return false;
    };
    
    {
        LOCK(cs_smsg);
        fSecMsgEnabled = true;
        
        smsgAddresses.clear(); // should be empty already
        if (SecureMsgReadIni() != 0)
            printf("Failed to read smsg.ini\n");
        
        if (smsgAddresses.size() < 1)
        {
            printf("No address keys loaded.\n");
            if (SecureMsgAddWalletAddresses() != 0)
                printf("Failed to load addresses from wallet.\n");
        };
        
        smsgBuckets.clear(); // should be empty already
        
        if (SecureMsgBuildBucketSet() != 0)
        {
            printf("SecureMsgEnable: could not load bucket sets, secure messaging disabled.\n");
            fSecMsgEnabled = false;
            return false;
        };
        
    }; // LOCK(cs_smsg);
    
    // -- start threads
    if (!NewThread(ThreadSecureMsg, NULL)
        || !NewThread(ThreadSecureMsgPow, NULL))
    {
        printf("SecureMsgEnable could not start threads, secure messaging disabled.\n");
        fSecMsgEnabled = false;
        return false;
    };
    
    // Modern C++ Migration: Range-based for loop for peer messaging
    {
        LOCK(cs_vNodes);
        for (CNode* pnode : vNodes)
        {
            pnode->PushMessage("smsgPing");
            pnode->PushMessage("smsgPong"); // Send pong as have missed initial ping sent by peer when it connected
        }
    }
    
    printf("Secure messaging enabled.\n");
    return true;
};

bool SecureMsgDisable()
{
    // -- stop secure messaging at runtime
    if (!fSecMsgEnabled)
    {
        printf("SecureMsgDisable: secure messaging is already disabled.\n");
        return false;
    };
    
    {
        LOCK(cs_smsg);
        fSecMsgEnabled = false;
        
        // -- clear smsgBuckets
        std::map<int64_t, SecMsgBucket>::iterator it;
        it = smsgBuckets.begin();
        for (it = smsgBuckets.begin(); it != smsgBuckets.end(); ++it)
        {
            it->second.setTokens.clear();
        };
        smsgBuckets.clear();
        
        // Modern C++ Migration: Range-based for loop for disabling secure messaging
        {
            LOCK(cs_vNodes);
            for (CNode* pnode : vNodes)
            {
                if (!pnode->smsgData.fEnabled)
                    continue;
                
                pnode->PushMessage("smsgDisabled");
                pnode->smsgData.fEnabled = false;
            }
        }
    
        if (SecureMsgWriteIni() != 0)
            printf("Failed to save smsg.ini\n");
        
        smsgAddresses.clear();
        
    }; // LOCK(cs_smsg);
    
    // -- allow time for threads to stop
    MilliSleep(3000); // milliseconds
    // TODO be certain that threads have stopped
    
    if (smsgDB)
    {
        LOCK(cs_smsgDB);
        delete smsgDB;
        smsgDB = NULL;
    };
    
    
    printf("Secure messaging disabled.\n");
    return true;
};


bool SecureMsgReceiveData(CNode* pfrom, std::string strCommand, CDataStream& vRecv)
{
    /*
        Called from ProcessMessage
        Runs in ThreadMessageHandler2
    */
    
    if (fDebug)
        printf("SecureMsgReceiveData() %s %s.\n", pfrom->addrName.c_str(), strCommand.c_str());
    
    {
    // break up?
    LOCK(cs_smsg);
    
    if (strCommand == "smsgInv")
    {
        std::vector<unsigned char> vchData;
        vRecv >> vchData;
        
        if (vchData.size() < 4)
        {
            pfrom->Misbehaving(1);
            return false; // not enough data received to be a valid smsgInv
        };
        
        int64_t now = GetTime();
        
        if (now < pfrom->smsgData.ignoreUntil)
        {
            if (fDebug)
                printf("Node is ignoring peer %u until %" PRId64 ".\n", pfrom->smsgData.nPeerId, pfrom->smsgData.ignoreUntil);
            return false;
        };
        
        uint32_t nBuckets       = smsgBuckets.size();
        uint32_t nLocked        = 0;    // no. of locked buckets on this node
        uint32_t nInvBuckets;           // no. of bucket headers sent by peer in smsgInv
        memcpy(&nInvBuckets, &vchData[0], 4);
        if (fDebug)
            printf("Remote node sent %d bucket headers, this has %d.\n", nInvBuckets, nBuckets);
        
        
        // -- Check no of buckets:
        if (nInvBuckets > (SMSG_RETENTION / SMSG_BUCKET_LEN) + 1) // +1 for some leeway
        {
            printf("Peer sent more bucket headers than possible %u, %u.\n", nInvBuckets, (SMSG_RETENTION / SMSG_BUCKET_LEN));
            pfrom->Misbehaving(1);
            return false;
        };
        
        if (vchData.size() < 4 + nInvBuckets*16)
        {
            printf("Remote node did not send enough data.\n");
            pfrom->Misbehaving(1);
            return false;
        };
        
        std::vector<unsigned char> vchDataOut;
        vchDataOut.reserve(4 + 8 * nInvBuckets); // reserve max possible size
        vchDataOut.resize(4);
        uint32_t nShowBuckets = 0;
        
        
        unsigned char *p = &vchData[4];
        for (uint32_t i = 0; i < nInvBuckets; ++i)
        {
            int64_t time;
            uint32_t ncontent, hash;
            memcpy(&time, p, 8);
            memcpy(&ncontent, p+8, 4);
            memcpy(&hash, p+12, 4);
            
            p += 16;
            
            // Check time valid:
            if (time < now - SMSG_RETENTION)
            {
                if (fDebug)
                    printf("Not interested in peer bucket %" PRId64 ", has expired.\n", time);
                
                if (time < now - SMSG_RETENTION - SMSG_TIME_LEEWAY)
                    pfrom->Misbehaving(1);
                continue;
            };
            if (time > now + SMSG_TIME_LEEWAY)
            {
                if (fDebug)
                    printf("Not interested in peer bucket %" PRId64 ", in the future.\n", time);
                pfrom->Misbehaving(1);
                continue;
            };
            
            if (ncontent < 1)
            {
                if (fDebug)
                    printf("Peer sent empty bucket, ignore %" PRId64 " %u %u.\n", time, ncontent, hash);
                continue;
            };
            
            if (fDebug)
            {
                printf("peer bucket %" PRId64 " %u %u.\n", time, ncontent, hash);
                printf("this bucket %" PRId64 " %" PRIszu " %u.\n", time, smsgBuckets[time].setTokens.size(), smsgBuckets[time].hash);
            };
            
            if (smsgBuckets[time].nLockCount > 0)
            {
                if (fDebug)
                    printf("Bucket is locked %u, waiting for peer %u to send data.\n", smsgBuckets[time].nLockCount, smsgBuckets[time].nLockPeerId);
                nLocked++;
                continue;
            };
            
            // -- if this node has more than the peer node, peer node will pull from this
            //    if then peer node has more this node will pull fom peer
            if (smsgBuckets[time].setTokens.size() < ncontent
                || (smsgBuckets[time].setTokens.size() == ncontent
                    && smsgBuckets[time].hash != hash)) // if same amount in buckets check hash
            {
                if (fDebug)
                    printf("Requesting contents of bucket %" PRId64 ".\n", time);
                
                uint32_t sz = vchDataOut.size();
                vchDataOut.resize(sz + 8);
                memcpy(&vchDataOut[sz], &time, 8);
                
                nShowBuckets++;
            };
        };
        
        // TODO: should include hash?
        memcpy(&vchDataOut[0], &nShowBuckets, 4);
        if (vchDataOut.size() > 4)
        {
            pfrom->PushMessage("smsgShow", vchDataOut);
        } else
        if (nLocked < 1) // Don't report buckets as matched if any are locked
        {
            // -- peer has no buckets we want, don't send them again until something changes
            //    peer will still request buckets from this node if needed (< ncontent)
            vchDataOut.resize(8);
            memcpy(&vchDataOut[0], &now, 8);
            pfrom->PushMessage("smsgMatch", vchDataOut);
            if (fDebug)
                printf("Sending smsgMatch, %" PRId64 ".\n", now);
        };
        
    } else
    if (strCommand == "smsgShow")
    {
        std::vector<unsigned char> vchData;
        vRecv >> vchData;
        
        if (vchData.size() < 4)
            return false;
        
        uint32_t nBuckets;
        memcpy(&nBuckets, &vchData[0], 4);
        
        if (vchData.size() < 4 + nBuckets * 8)
            return false;
        
        if (fDebug)
            printf("smsgShow: peer wants to see content of %u buckets.\n", nBuckets);
        
        std::map<int64_t, SecMsgBucket>::iterator itb;
        std::set<SecMsgToken>::iterator it;
        
        std::vector<unsigned char> vchDataOut;
        int64_t time;
        unsigned char* pIn = &vchData[4];
        for (uint32_t i = 0; i < nBuckets; ++i, pIn += 8)
        {
            memcpy(&time, pIn, 8);
            
            itb = smsgBuckets.find(time);
            if (itb == smsgBuckets.end())
            {
                if (fDebug)
                    printf("Don't have bucket %" PRId64 ".\n", time);
                continue;
            };
            
            std::set<SecMsgToken>& tokenSet = (*itb).second.setTokens;
            
            try {
                vchDataOut.resize(8 + 16 * tokenSet.size());
            } catch (std::exception& e) {
                printf("vchDataOut.resize %" PRIszu " threw: %s.\n", 8 + 16 * tokenSet.size(), e.what());
                continue;
            };
            memcpy(&vchDataOut[0], &time, 8);
            
            unsigned char* p = &vchDataOut[8];
            for (it = tokenSet.begin(); it != tokenSet.end(); ++it)
            {
                memcpy(p, &it->timestamp, 8);
                memcpy(p+8, &it->sample, 8);
                
                p += 16;
            };
            pfrom->PushMessage("smsgHave", vchDataOut);
        };
        
        
    } else
    if (strCommand == "smsgHave")
    {
        // -- peer has these messages in bucket
        std::vector<unsigned char> vchData;
        vRecv >> vchData;
        
        if (vchData.size() < 8)
            return false;
        
        int n = (vchData.size() - 8) / 16;
        
        int64_t time;
        memcpy(&time, &vchData[0], 8);
        
        // -- Check time valid:
        int64_t now = GetTime();
        if (time < now - SMSG_RETENTION)
        {
            if (fDebug)
                printf("Not interested in peer bucket %" PRId64 ", has expired.\n", time);
            return false;
        };
        if (time > now + SMSG_TIME_LEEWAY)
        {
            if (fDebug)
                printf("Not interested in peer bucket %" PRId64 ", in the future.\n", time);
            pfrom->Misbehaving(1);
            return false;
        };
        
        if (smsgBuckets[time].nLockCount > 0)
        {
            if (fDebug)
                printf("Bucket %" PRId64 " lock count %u, waiting for message data from peer %u.\n", time, smsgBuckets[time].nLockCount, smsgBuckets[time].nLockPeerId);
            return false;
        }; 
        
        if (fDebug)
            printf("Sifting through bucket %" PRId64 ".\n", time);
        
        std::vector<unsigned char> vchDataOut;
        vchDataOut.resize(8);
        memcpy(&vchDataOut[0], &vchData[0], 8);
        
        std::set<SecMsgToken>& tokenSet = smsgBuckets[time].setTokens;
        std::set<SecMsgToken>::iterator it;
        SecMsgToken token;
        unsigned char* p = &vchData[8];
        
        for (int i = 0; i < n; ++i)
        {
            memcpy(&token.timestamp, p, 8);
            memcpy(&token.sample, p+8, 8);
            
            it = tokenSet.find(token);
            if (it == tokenSet.end())
            {
                int nd = vchDataOut.size();
                try {
                    vchDataOut.resize(nd + 16);
                } catch (std::exception& e) {
                    printf("vchDataOut.resize %d threw: %s.\n", nd + 16, e.what());
                    continue;
                };
                
                memcpy(&vchDataOut[nd], p, 16);
            };
            
            p += 16;
        };
        
        if (vchDataOut.size() > 8)
        {
            if (fDebug)
            {
                printf("Asking peer for  %" PRIszu " messages.\n", (vchDataOut.size() - 8) / 16);
                printf("Locking bucket %" PRIszu " for peer %u.\n", time, pfrom->smsgData.nPeerId);
            };
            smsgBuckets[time].nLockCount   = 3; // lock this bucket for at most 3 * SMSG_THREAD_DELAY seconds, unset when peer sends smsgMsg
            smsgBuckets[time].nLockPeerId  = pfrom->smsgData.nPeerId;
            pfrom->PushMessage("smsgWant", vchDataOut);
        };
    } else
    if (strCommand == "smsgWant")
    {
        std::vector<unsigned char> vchData;
        vRecv >> vchData;
        
        if (vchData.size() < 8)
            return false;
        
        std::vector<unsigned char> vchOne;
        std::vector<unsigned char> vchBunch;
        
        vchBunch.resize(4+8); // nmessages + bucketTime
        
        int n = (vchData.size() - 8) / 16;
        
        int64_t time;
        uint32_t nBunch = 0;
        memcpy(&time, &vchData[0], 8);
        
        std::map<int64_t, SecMsgBucket>::iterator itb;
        itb = smsgBuckets.find(time);
        if (itb == smsgBuckets.end())
        {
            if (fDebug)
                printf("Don't have bucket %" PRId64 ".\n", time);
            return false;
        };
        
        std::set<SecMsgToken>& tokenSet = itb->second.setTokens;
        std::set<SecMsgToken>::iterator it;
        SecMsgToken token;
        unsigned char* p = &vchData[8];
        for (int i = 0; i < n; ++i)
        {
            memcpy(&token.timestamp, p, 8);
            memcpy(&token.sample, p+8, 8);
            
            it = tokenSet.find(token);
            if (it == tokenSet.end())
            {
                if (fDebug)
                    printf("Don't have wanted message %" PRId64 ".\n", token.timestamp);
            } else
            {
                //printf("Have message at %"PRId64".\n", it->offset); // DEBUG
                token.offset = it->offset;
                //printf("winb before SecureMsgRetrieve %"PRId64".\n", token.timestamp);
                
                // -- place in vchOne so if SecureMsgRetrieve fails it won't corrupt vchBunch
                if (SecureMsgRetrieve(token, vchOne) == 0)
                {
                    nBunch++;
                    vchBunch.insert(vchBunch.end(), vchOne.begin(), vchOne.end()); // append
                } else
                {
                    printf("SecureMsgRetrieve failed %" PRId64 ".\n", token.timestamp);
                };
                
                if (nBunch >= 500
                    || vchBunch.size() >= 96000)
                {
                    if (fDebug)
                        printf("Break bunch %u, %" PRIszu ".\n", nBunch, vchBunch.size());
                    break; // end here, peer will send more want messages if needed.
                };
            };
            p += 16;
        };
        
        if (nBunch > 0)
        {
            if (fDebug)
                printf("Sending block of %u messages for bucket %" PRId64 ".\n", nBunch, time);
            
            memcpy(&vchBunch[0], &nBunch, 4);
            memcpy(&vchBunch[4], &time, 8);
            pfrom->PushMessage("smsgMsg", vchBunch);
        };
    } else
    if (strCommand == "smsgMsg")
    {
        std::vector<unsigned char> vchData;
        vRecv >> vchData;
        
        if (fDebug)
            printf("smsgMsg vchData.size() %" PRIszu ".\n", vchData.size());
        
        SecureMsgReceive(pfrom, vchData);
    } else
    if (strCommand == "smsgMatch")
    {
        std::vector<unsigned char> vchData;
        vRecv >> vchData;
        
        
        if (vchData.size() < 8)
        {
            printf("smsgMatch, not enough data %" PRIszu ".\n", vchData.size());
            pfrom->Misbehaving(1);
            return false;
        };
        
        int64_t time;
        memcpy(&time, &vchData[0], 8);
        
        int64_t now = GetTime();
        if (time > now + SMSG_TIME_LEEWAY)
        {
            printf("Warning: Peer buckets matched in the future: %" PRId64 ".\nEither this node or the peer node has the incorrect time set.\n", time);
            if (fDebug)
                printf("Peer match time set to now.\n");
            time = now;
        };
        
        pfrom->smsgData.lastMatched = time;
        
        if (fDebug)
            printf("Peer buckets matched at %" PRId64 ".\n", time);
        
    } else
    if (strCommand == "smsgPing")
    {
        // -- smsgPing is the initial message, send reply
        pfrom->PushMessage("smsgPong");
    } else
    if (strCommand == "smsgPong")
    {
        if (fDebug)
             printf("Peer replied, secure messaging enabled.\n");
        
        pfrom->smsgData.fEnabled = true;
    } else
    if (strCommand == "smsgDisabled")
    {
        // -- peer has disabled secure messaging.
        
        pfrom->smsgData.fEnabled = false;
        
        if (fDebug)
            printf("Peer %u has disabled secure messaging.\n", pfrom->smsgData.nPeerId);
        
    } else
    if (strCommand == "smsgIgnore")
    {
        // -- peer is reporting that it will ignore this node until time.
        //    Ignore peer too
        std::vector<unsigned char> vchData;
        vRecv >> vchData;
        
        if (vchData.size() < 8)
        {
            printf("smsgIgnore, not enough data %" PRIszu ".\n", vchData.size());
            pfrom->Misbehaving(1);
            return false;
        };
        
        int64_t time;
        memcpy(&time, &vchData[0], 8);
        
        pfrom->smsgData.ignoreUntil = time;
        
        if (fDebug)
            printf("Peer %u is ignoring this node until %" PRId64 ", ignore peer too.\n", pfrom->smsgData.nPeerId, time);
    } else
    {
        // Unknown message
    };
    
    }; //  LOCK(cs_smsg);
    
    return true;
};

bool SecureMsgSendData(CNode* pto, bool fSendTrickle)
{
    /*
        Called from ProcessMessage
        Runs in ThreadMessageHandler2
    */
    
    //printf("SecureMsgSendData() %s.\n", pto->addrName.c_str());
    
    
    int64_t now = GetTime();
    
    if (pto->smsgData.lastSeen == 0)
    {
        // -- first contact
        pto->smsgData.nPeerId = nPeerIdCounter++;
        if (fDebug)
            printf("SecureMsgSendData() new node %s, peer id %u.\n", pto->addrName.c_str(), pto->smsgData.nPeerId);
        // -- Send smsgPing once, do nothing until receive 1st smsgPong (then set fEnabled)
        pto->PushMessage("smsgPing");
        pto->smsgData.lastSeen = GetTime();
        return true;
    } else
    if (!pto->smsgData.fEnabled
        || now - pto->smsgData.lastSeen < SMSG_SEND_DELAY
        || now < pto->smsgData.ignoreUntil)
    {
        return true;
    };
    
    // -- When nWakeCounter == 0, resend bucket inventory.  
    if (pto->smsgData.nWakeCounter < 1)
    {
        pto->smsgData.lastMatched = 0;
        pto->smsgData.nWakeCounter = 10 + GetRandInt(300);  // set to a random time between [10, 300] * SMSG_SEND_DELAY seconds
        
        if (fDebug)
            printf("SecureMsgSendData(): nWakeCounter expired, sending bucket inventory to %s.\n"
            "Now %" PRId64 " next wake counter %u\n", pto->addrName.c_str(), now, pto->smsgData.nWakeCounter);
    };
    pto->smsgData.nWakeCounter--;
    
    {
        LOCK(cs_smsg);
        std::map<int64_t, SecMsgBucket>::iterator it;
        
        uint32_t nBuckets = smsgBuckets.size();
        if (nBuckets > 0) // no need to send keep alive pkts, coin messages already do that
        {
            std::vector<unsigned char> vchData;
            // should reserve?
            vchData.reserve(4 + nBuckets*16); // timestamp + size + hash
            
            uint32_t nBucketsShown = 0;
            vchData.resize(4);
            
            unsigned char* p = &vchData[4];
            for (it = smsgBuckets.begin(); it != smsgBuckets.end(); ++it)
            {
                SecMsgBucket &bkt = it->second;
                
                uint32_t nMessages = bkt.setTokens.size();
                
                if (bkt.timeChanged < pto->smsgData.lastMatched     // peer has this bucket
                    || nMessages < 1)                               // this bucket is empty
                    continue; 
                
                
                uint32_t hash = bkt.hash;
                
                try {
                    vchData.resize(vchData.size() + 16);
                } catch (std::exception& e) {
                    printf("vchData.resize %" PRIszu " threw: %s.\n", vchData.size() + 16, e.what());
                    continue;
                };
                memcpy(p, &it->first, 8);
                memcpy(p+8, &nMessages, 4);
                memcpy(p+12, &hash, 4);
                
                p += 16;
                nBucketsShown++;
                //if (fDebug)
                //    printf("Sending bucket %"PRId64", size %d \n", it->first, it->second.size());
            };
            
            if (vchData.size() > 4)
            {
                memcpy(&vchData[0], &nBucketsShown, 4);
                if (fDebug)
                    printf("Sending %d bucket headers.\n", nBucketsShown);
                
                pto->PushMessage("smsgInv", vchData);
            };
        };
    }
    
    pto->smsgData.lastSeen = GetTime();
    
    return true;
};


static int SecureMsgInsertAddress(CKeyID& hashKey, CPubKey& pubKey, SecMsgDB& addrpkdb)
{
    /* insert key hash and public key to addressdb
        
        should have LOCK(cs_smsg) where db is opened
        
        returns
            0 success
            1 error
            4 address is already in db
    */
    
    
    if (addrpkdb.ExistsPK(hashKey))
    {
        //printf("DB already contains public key for address.\n");
        CPubKey cpkCheck;
        if (!addrpkdb.ReadPK(hashKey, cpkCheck))
        {
            printf("addrpkdb.Read failed.\n");
        } else
        {
            if (cpkCheck != pubKey)
                printf("DB already contains existing public key that does not match .\n");
        };
        return 4;
    };
    
    if (!addrpkdb.WritePK(hashKey, pubKey))
    {
        printf("Write pair failed.\n");
        return 1;
    };
    
    return 0;
};

int SecureMsgInsertAddress(CKeyID& hashKey, CPubKey& pubKey)
{
    int rv;
    {
        LOCK(cs_smsgDB);
        SecMsgDB addrpkdb;
        
        if (!addrpkdb.Open("cr+"))
            return 1;
        
        rv = SecureMsgInsertAddress(hashKey, pubKey, addrpkdb);
    }
    return rv;
};


static bool ScanBlock(CBlock& block, CTxDB& txdb, SecMsgDB& addrpkdb,
    uint32_t& nTransactions, uint32_t& nInputs, uint32_t& nPubkeys, uint32_t& nDuplicates)
{
    // Modern C++ Migration: Range-based for loop for block transactions
    for (CTransaction& tx : block.vtx)
    {
        if (!IsStandardTx(tx))
            continue; // leave out coinbase and others
        
        /*
        Look at the inputs of every tx.
        If the inputs are standard, get the pubkey from scriptsig and
        look for the corresponding output (the input(output of other tx) to the input of this tx)
        get the address from scriptPubKey
        add to db if address is unique.
        
        Would make more sense to do this the other way around, get address first for early out.
        
        */
        
        for (unsigned int i = 0; i < tx.vin.size(); i++)
        {
            CScript *script = &tx.vin[i].scriptSig;
            
            opcodetype opcode;
            valtype vch;
            CScript::const_iterator pc = script->begin();
            CScript::const_iterator pend = script->end();
            
            uint256 prevoutHash;
            CKey key;
            
            // -- matching address is in scriptPubKey of previous tx output
            while (pc < pend)
            {
                if (!script->GetOp(pc, opcode, vch))
                    break;
                // -- opcode is the length of the following data, compressed public key is always 33
                if (opcode == 33)
                {
                    key.SetPubKey(vch);
                    
                    key.SetCompressedPubKey(); // ensure key is compressed
                    CPubKey pubKey = key.GetPubKey();
                    
                    if (!pubKey.IsValid()
                        || !pubKey.IsCompressed())
                    {
                        printf("Public key is invalid %s.\n", ValueString(pubKey.Raw()).c_str());
                        continue;
                    };
                    
                    prevoutHash = tx.vin[i].prevout.hash;
                    CTransaction txOfPrevOutput;
                    if (!txdb.ReadDiskTx(prevoutHash, txOfPrevOutput))
                    {
                        printf("Could not get transaction for hash: %s.\n", prevoutHash.ToString().c_str());
                        continue;
                    };
                    
                    unsigned int nOut = tx.vin[i].prevout.n;
                    if (nOut >= txOfPrevOutput.vout.size())
                    {
                        printf("Output %u, not in transaction: %s.\n", nOut, prevoutHash.ToString().c_str());
                        continue;
                    };
                    
                    CTxOut *txOut = &txOfPrevOutput.vout[nOut];
                    
                    CTxDestination addressRet;
                    if (!ExtractDestination(txOut->scriptPubKey, addressRet))
                    {
                        printf("ExtractDestination failed: %s.\n", prevoutHash.ToString().c_str());
                        break;
                    };
                    
                    
                    CBitcoinAddress coinAddress(addressRet);
                    CKeyID hashKey;
                    if (!coinAddress.GetKeyID(hashKey))
                    {
                        printf("coinAddress.GetKeyID failed: %s.\n", coinAddress.ToString().c_str());
                        break;
                    };
                    
                    int rv = SecureMsgInsertAddress(hashKey, pubKey, addrpkdb);
                    if (rv != 0)
                    {
                        if (rv == 4)
                            nDuplicates++;
                        break;
                    };
                    nPubkeys++;
                    break;
                };
                
                //printf("opcode %d, %s, value %s.\n", opcode, GetOpName(opcode), ValueString(vch).c_str());
            };
            nInputs++;
        };
        nTransactions++;
        
        if (nTransactions % 10000 == 0) // for ScanChainForPublicKeys
        {
            printf("Scanning transaction no. %u.\n", nTransactions);
        };
    };
    return true;
};


bool SecureMsgScanBlock(CBlock& block)
{
    /*
    scan block for public key addresses
    called from ProcessMessage() in main where strCommand == "block"
    */
    
    if (fDebug)
        printf("SecureMsgScanBlock().\n");
    
    uint32_t nTransactions  = 0;
    uint32_t nInputs        = 0;
    uint32_t nPubkeys       = 0;
    uint32_t nDuplicates    = 0;
    
    {
        LOCK(cs_smsgDB);
        CTxDB txdb("r");
        
        SecMsgDB addrpkdb;
        if (!addrpkdb.Open("cw")
            || !addrpkdb.TxnBegin())
            return false;
        
        ScanBlock(block, txdb, addrpkdb,
            nTransactions, nInputs, nPubkeys, nDuplicates);
        
        addrpkdb.TxnCommit();
    }
    
    if (fDebug)
        printf("Found %u transactions, %u inputs, %u new public keys, %u duplicates.\n", nTransactions, nInputs, nPubkeys, nDuplicates);
    
    return true;
};

bool ScanChainForPublicKeys(CBlockIndex* pindexStart)
{
    printf("Scanning block chain for public keys.\n");
    int64_t nStart = GetTimeMillis();
    
    if (fDebug)
        printf("From height %u.\n", pindexStart->nHeight);
    
    // -- public keys are in txin.scriptSig
    //    matching addresses are in scriptPubKey of txin's referenced output
    
    uint32_t nBlocks        = 0;
    uint32_t nTransactions  = 0;
    uint32_t nInputs        = 0;
    uint32_t nPubkeys       = 0;
    uint32_t nDuplicates    = 0;
    
    {
        LOCK(cs_smsgDB);
    
        CTxDB txdb("r");
        
        SecMsgDB addrpkdb;
        if (!addrpkdb.Open("cw")
            || !addrpkdb.TxnBegin())
            return false;
        
        CBlockIndex* pindex = pindexStart;
        while (pindex)
        {
            nBlocks++;
            CBlock block;
            block.ReadFromDisk(pindex, true);
            
            ScanBlock(block, txdb, addrpkdb,
                nTransactions, nInputs, nPubkeys, nDuplicates);
            
            pindex = pindex->pnext;
        };
        
        addrpkdb.TxnCommit();
    };
    
    printf("Scanned %u blocks, %u transactions, %u inputs\n", nBlocks, nTransactions, nInputs);
    printf("Found %u public keys, %u duplicates.\n", nPubkeys, nDuplicates);
    printf("Took %" PRId64 " ms\n", GetTimeMillis() - nStart);
    
    return true;
};

bool SecureMsgScanBlockChain()
{
    TRY_LOCK(cs_main, lockMain);
    if (lockMain)
    {
        CBlockIndex *pindexScan = pindexGenesisBlock;
        if (pindexScan == NULL)
        {
            printf("Error: pindexGenesisBlock not set.\n");
            return false;
        };
        
        
        try { // -- in try to catch errors opening db, 
            if (!ScanChainForPublicKeys(pindexScan))
                return false;
        } catch (std::exception& e)
        {
            printf("ScanChainForPublicKeys() threw: %s.\n", e.what());
            return false;
        };
    } else
    {
        printf("ScanChainForPublicKeys() Could not lock main.\n");
        return false;
    };
    
    return true;
};

bool SecureMsgScanBuckets()
{
    if (fDebug)
        printf("SecureMsgScanBuckets()\n");
    
    if (!fSecMsgEnabled
        || pwalletMain->IsLocked())
        return false;
    
    int64_t  mStart         = GetTimeMillis();
    int64_t  now            = GetTime();
    uint32_t nFiles         = 0;
    uint32_t nMessages      = 0;
    uint32_t nFoundMessages = 0;
    
    fs::path pathSmsgDir = GetDataDir() / "smsgStore";
    fs::directory_iterator itend;
    
    if (!fs::exists(pathSmsgDir)
        || !fs::is_directory(pathSmsgDir))
    {
        printf("Message store directory does not exist.\n");
        return 0; // not an error
    };
    
    SecureMessage smsg;
    std::vector<unsigned char> vchData;
    
    for (fs::directory_iterator itd(pathSmsgDir) ; itd != itend ; ++itd)
    {
        if (!fs::is_regular_file(itd->status()))
            continue;
        
        std::string fileType = (*itd).path().extension().string();
        
        if (fileType.compare(".dat") != 0)
            continue;
            
        std::string fileName = (*itd).path().filename().string();
        
        
        if (fDebug)
            printf("Processing file: %s.\n", fileName.c_str());
        
        nFiles++;
        
        // TODO files must be split if > 2GB
        // time_noFile.dat
        size_t sep = fileName.find_first_of("_");
        if (sep == std::string::npos)
            continue;
        
        std::string stime = fileName.substr(0, sep);
        
        int64_t fileTime = boost::lexical_cast<int64_t>(stime);
        
        if (fileTime < now - SMSG_RETENTION)
        {
            printf("Dropping file %s, expired.\n", fileName.c_str());
            try {
                fs::remove((*itd).path());
            } catch (const fs::filesystem_error& ex)
            {
                printf("Error removing bucket file %s, %s.\n", fileName.c_str(), ex.what());
            };
            continue;
        };
        
        if (boost::algorithm::ends_with(fileName, "_wl.dat"))
        {
            if (fDebug)
                printf("Skipping wallet locked file: %s.\n", fileName.c_str());
            continue;
        };
        
        {
            LOCK(cs_smsg);
            FILE *fp;
            errno = 0;
            if (!(fp = fopen((*itd).path().string().c_str(), "rb")))
            {
                printf("Error opening file: %s\n", strerror(errno));
                continue;
            };
            
            for (;;)
            {
                errno = 0;
                if (fread(&smsg.hash[0], sizeof(unsigned char), SMSG_HDR_LEN, fp) != (size_t)SMSG_HDR_LEN)
                {
                    if (errno != 0)
                    {
                        printf("fread header failed: %s\n", strerror(errno));
                    } else
                    {
                        //printf("End of file.\n");
                    };
                    break;
                };
                
                try {
                    vchData.resize(smsg.nPayload);
                } catch (std::exception& e)
                {
                    printf("SecureMsgWalletUnlocked(): Could not resize vchData, %u, %s\n", smsg.nPayload, e.what());
                    fclose(fp);
                    return 1;
                };
                
                if (fread(&vchData[0], sizeof(unsigned char), smsg.nPayload, fp) != smsg.nPayload)
                {
                    printf("fread data failed: %s\n", strerror(errno));
                    break;
                };
                
                // -- don't report to gui, 
                int rv = SecureMsgScanMessage(&smsg.hash[0], &vchData[0], smsg.nPayload, false);
                
                if (rv == 0)
                {
                    nFoundMessages++;
                } else
                if (rv != 0)
                {
                    // SecureMsgScanMessage failed
                };
                
                nMessages ++;
            };
            
            fclose(fp);
            
            // -- remove wl file when scanned
            try {
                fs::remove((*itd).path());
            } catch (const filesystem_error& ex)
            {
                printf("Error removing wl file %s - %s\n", fileName.c_str(), ex.what());
                return 1;
            };
        };
    };
    
    printf("Processed %u files, scanned %u messages, received %u messages.\n", nFiles, nMessages, nFoundMessages);
    printf("Took %" PRId64 " ms\n", GetTimeMillis() - mStart);
    
    return true;
}


int SecureMsgWalletUnlocked()
{
    /*
    When the wallet is unlocked scan messages received while wallet was locked.
    */
    if (!fSecMsgEnabled)
        return 0;
    
    
    printf("SecureMsgWalletUnlocked()\n");
    
    if (pwalletMain->IsLocked())
    {
        printf("Error: Wallet is locked.\n");
        return 1;
    };
    
    int64_t  now            = GetTime();
    uint32_t nFiles         = 0;
    uint32_t nMessages      = 0;
    uint32_t nFoundMessages = 0;
    
    fs::path pathSmsgDir = GetDataDir() / "smsgStore";
    fs::directory_iterator itend;
    
    if (!fs::exists(pathSmsgDir)
        || !fs::is_directory(pathSmsgDir))
    {
        printf("Message store directory does not exist.\n");
        return 0; // not an error
    };
    
    SecureMessage smsg;
    std::vector<unsigned char> vchData;
    
    for (fs::directory_iterator itd(pathSmsgDir) ; itd != itend ; ++itd)
    {
        if (!fs::is_regular_file(itd->status()))
            continue;
        
        std::string fileName = (*itd).path().filename().string();
        
        if (!boost::algorithm::ends_with(fileName, "_wl.dat"))
            continue;
        
        if (fDebug)
            printf("Processing file: %s.\n", fileName.c_str());
        
        nFiles++;
        
        // TODO files must be split if > 2GB
        // time_noFile_wl.dat
        size_t sep = fileName.find_first_of("_");
        if (sep == std::string::npos)
            continue;
        
        std::string stime = fileName.substr(0, sep);
        
        int64_t fileTime = boost::lexical_cast<int64_t>(stime);
        
        if (fileTime < now - SMSG_RETENTION)
        {
            printf("Dropping wallet locked file %s, expired.\n", fileName.c_str());
            try {
                fs::remove((*itd).path());
            } catch (const filesystem_error& ex)
            {
                printf("Error removing wl file %s - %s\n", fileName.c_str(), ex.what());
                return 1;
            };
            continue;
        };
        
        {
            LOCK(cs_smsg);
            FILE *fp;
            errno = 0;
            if (!(fp = fopen((*itd).path().string().c_str(), "rb")))
            {
                printf("Error opening file: %s\n", strerror(errno));
                continue;
            };
            
            for (;;)
            {
                errno = 0;
                if (fread(&smsg.hash[0], sizeof(unsigned char), SMSG_HDR_LEN, fp) != (size_t)SMSG_HDR_LEN)
                {
                    if (errno != 0)
                    {
                        printf("fread header failed: %s\n", strerror(errno));
                    } else
                    {
                        //printf("End of file.\n");
                    };
                    break;
                };
                
                try {
                    vchData.resize(smsg.nPayload);
                } catch (std::exception& e)
                {
                    printf("SecureMsgWalletUnlocked(): Could not resize vchData, %u, %s\n", smsg.nPayload, e.what());
                    fclose(fp);
                    return 1;
                };
                
                if (fread(&vchData[0], sizeof(unsigned char), smsg.nPayload, fp) != smsg.nPayload)
                {
                    printf("fread data failed: %s\n", strerror(errno));
                    break;
                };
                
                // -- don't report to gui, 
                int rv = SecureMsgScanMessage(&smsg.hash[0], &vchData[0], smsg.nPayload, false);
                
                if (rv == 0)
                {
                    nFoundMessages++;
                } else
                if (rv != 0)
                {
                    // SecureMsgScanMessage failed
                };
                
                nMessages ++;
            };
            
            fclose(fp);
            
            // -- remove wl file when scanned
            try {
                fs::remove((*itd).path());
            } catch (const filesystem_error& ex)
            {
                printf("Error removing wl file %s - %s\n", fileName.c_str(), ex.what());
                return 1;
            };
        };
    };
    
    printf("Processed %u files, scanned %u messages, received %u messages.\n", nFiles, nMessages, nFoundMessages);
    
    // -- notify gui
    NotifySecMsgWalletUnlocked();
    
    return 0;
};

int SecureMsgWalletKeyChanged(std::string sAddress, std::string sLabel, ChangeType mode)
{
    if (!fSecMsgEnabled)
        return 0;
    
    printf("SecureMsgWalletKeyChanged()\n");
    
    // TODO: default recv and recvAnon
    
    {
        LOCK(cs_smsg);
        
        switch(mode)
        {
            case CT_NEW:
                smsgAddresses.push_back(SecMsgAddress(sAddress, smsgOptions.fNewAddressRecv, smsgOptions.fNewAddressAnon));
                break;
            case CT_DELETED:
                for (std::vector<SecMsgAddress>::iterator it = smsgAddresses.begin(); it != smsgAddresses.end(); ++it)
                {
                    if (sAddress != it->sAddress)
                        continue;
                    smsgAddresses.erase(it);
                    break;
                };
                break;
            default:
                break;
        }
        
    }; // LOCK(cs_smsg);
    
    
    return 0;
};

int SecureMsgScanMessage(unsigned char *pHeader, unsigned char *pPayload, uint32_t nPayload, bool reportToGui)
{
    /* 
    Check if message belongs to this node.
    If so add to inbox db.
    
    if !reportToGui don't fire NotifySecMsgInboxChanged
     - loads messages received when wallet locked in bulk.
    
    returns
        0 success,
        1 error
        2 no match
        3 wallet is locked - message stored for scanning later.
    */
    
    if (fDebug)
        printf("SecureMsgScanMessage()\n");
    
    if (pwalletMain->IsLocked())
    {
        if (fDebug)
            printf("ScanMessage: Wallet is locked, storing message to scan later.\n");
        
        int rv;
        if ((rv = SecureMsgStoreUnscanned(pHeader, pPayload, nPayload)) != 0)
            return 1;
        
        return 3;
    };
    
    std::string addressTo;
    MessageData msg; // placeholder
    bool fOwnMessage = false;
    
    for (std::vector<SecMsgAddress>::iterator it = smsgAddresses.begin(); it != smsgAddresses.end(); ++it)
    {
        if (!it->fReceiveEnabled)
            continue;
        
        CBitcoinAddress coinAddress(it->sAddress);
        addressTo = coinAddress.ToString();
        
        if (!it->fReceiveAnon)
        {
            // -- have to do full decrypt to see address from
            if (SecureMsgDecrypt(false, addressTo, pHeader, pPayload, nPayload, msg) == 0)
            {
                if (fDebug)
                    printf("Decrypted message with %s.\n", addressTo.c_str());
                
                if (msg.sFromAddress.compare("anon") != 0)
                    fOwnMessage = true;
                break;
            };
        } else
        {
            
            if (SecureMsgDecrypt(true, addressTo, pHeader, pPayload, nPayload, msg) == 0)
            {
                if (fDebug)
                    printf("Decrypted message with %s.\n", addressTo.c_str());
                
                fOwnMessage = true;
                break;
            };
        }
    };
    
    if (fOwnMessage)
    {
        // -- save to inbox
        SecureMessage* psmsg = (SecureMessage*) pHeader;
        std::string sPrefix("im");
        unsigned char chKey[18];
        memcpy(&chKey[0],  sPrefix.data(),    2);
        memcpy(&chKey[2],  &psmsg->timestamp, 8);
        memcpy(&chKey[10], pPayload,          8);
        
        SecMsgStored smsgInbox;
        smsgInbox.timeReceived  = GetTime();
        smsgInbox.status        = (SMSG_MASK_UNREAD) & 0xFF;
        smsgInbox.sAddrTo       = addressTo;
        
        // -- data may not be contiguous
        try {
            smsgInbox.vchMessage.resize(SMSG_HDR_LEN + nPayload);
        } catch (std::exception& e) {
            printf("SecureMsgScanMessage(): Could not resize vchData, %u, %s\n", SMSG_HDR_LEN + nPayload, e.what());
            return 1;
        };
        memcpy(&smsgInbox.vchMessage[0], pHeader, SMSG_HDR_LEN);
        memcpy(&smsgInbox.vchMessage[SMSG_HDR_LEN], pPayload, nPayload);
        
        {
            LOCK(cs_smsgDB);
            SecMsgDB dbInbox;
            
            if (dbInbox.Open("cw"))
            {
                if (dbInbox.ExistsSmesg(chKey))
                {
                    if (fDebug)
                        printf("Message already exists in inbox db.\n");
                } else
                {
                    dbInbox.WriteSmesg(chKey, smsgInbox);
                    
                    if (reportToGui)
                        NotifySecMsgInboxChanged(smsgInbox);
                    printf("SecureMsg saved to inbox, received with %s.\n", addressTo.c_str());
                };
            };
        }
    };
    
    return 0;
};

int SecureMsgGetLocalKey(CKeyID& ckid, CPubKey& cpkOut)
{
    if (fDebug)
        printf("SecureMsgGetLocalKey()\n");
    
    CKey key;
    if (!pwalletMain->GetKey(ckid, key))
        return 4;
    
    key.SetCompressedPubKey(); // make sure key is compressed
    
    cpkOut = key.GetPubKey();
    if (!cpkOut.IsValid()
        || !cpkOut.IsCompressed())
    {
        printf("Public key is invalid %s.\n", ValueString(cpkOut.Raw()).c_str());
        return 1;
    };
    
    return 0;
};

int SecureMsgGetLocalPublicKey(std::string& strAddress, std::string& strPublicKey)
{
    /* returns
        0 success,
        1 error
        2 invalid address
        3 address does not refer to a key
        4 address not in wallet
    */
    
    CBitcoinAddress address;
    if (!address.SetString(strAddress))
        return 2; // Invalid coin address
    
    CKeyID keyID;
    if (!address.GetKeyID(keyID))
        return 3;
    
    int rv;
    CPubKey pubKey;
    if ((rv = SecureMsgGetLocalKey(keyID, pubKey)) != 0)
        return rv;
    
    strPublicKey = EncodeBase58(pubKey.Raw());
    
    return 0;
};

int SecureMsgGetStoredKey(CKeyID& ckid, CPubKey& cpkOut)
{
    /* returns
        0 success,
        1 error
        2 public key not in database
    */
    if (fDebug)
        printf("SecureMsgGetStoredKey().\n");
    
    {
        LOCK(cs_smsgDB);
        SecMsgDB addrpkdb;
        
        if (!addrpkdb.Open("r"))
            return 1;
        
        if (!addrpkdb.ReadPK(ckid, cpkOut))
        {
            //printf("addrpkdb.Read failed: %s.\n", coinAddress.ToString().c_str());
            return 2;
        };
    }
    
    return 0;
};

int SecureMsgAddAddress(std::string& address, std::string& publicKey)
{
    /*
        Add address and matching public key to the database
        address and publicKey are in base58
        
        returns
            0 success
            1 error
            2 publicKey is invalid
            3 publicKey != address
            4 address is already in db
            5 address is invalid
    */
    
    CBitcoinAddress coinAddress(address);
    
    if (!coinAddress.IsValid())
    {
        printf("Address is not valid: %s.\n", address.c_str());
        return 5;
    };
    
    CKeyID hashKey;
    
    if (!coinAddress.GetKeyID(hashKey))
    {
        printf("coinAddress.GetKeyID failed: %s.\n", coinAddress.ToString().c_str());
        return 5;
    };
    
    std::vector<unsigned char> vchTest;
    DecodeBase58(publicKey, vchTest);
    CPubKey pubKey(vchTest);
    
    // -- check that public key matches address hash
    CKey keyT;
    if (!keyT.SetPubKey(pubKey))
    {
        printf("SetPubKey failed.\n");
        return 2;
    };
    
    keyT.SetCompressedPubKey();
    CPubKey pubKeyT = keyT.GetPubKey();
    
    CBitcoinAddress addressT(address);
    
    if (addressT.ToString().compare(address) != 0)
    {
        printf("Public key does not hash to address, addressT %s.\n", addressT.ToString().c_str());
        return 3;
    };
    
    return SecureMsgInsertAddress(hashKey, pubKey);
};

int SecureMsgRetrieve(SecMsgToken &token, std::vector<unsigned char>& vchData)
{
    if (fDebug)
        printf("SecureMsgRetrieve() %" PRId64 ".\n", token.timestamp);
    
    // -- has cs_smsg lock from SecureMsgReceiveData
    
    fs::path pathSmsgDir = GetDataDir() / "smsgStore";
    
    //printf("token.offset %"PRId64".\n", token.offset); // DEBUG
    int64_t bucket = token.timestamp - (token.timestamp % SMSG_BUCKET_LEN);
    std::string fileName = boost::lexical_cast<std::string>(bucket) + "_01.dat";
    fs::path fullpath = pathSmsgDir / fileName;
    
    //printf("bucket %"PRId64".\n", bucket);
    //printf("bucket lld %lld.\n", bucket);
    //printf("fileName %s.\n", fileName.c_str());
    
    FILE *fp;
    errno = 0;
    if (!(fp = fopen(fullpath.string().c_str(), "rb")))
    {
        printf("Error opening file: %s\nPath %s\n", strerror(errno), fullpath.string().c_str());
        return 1;
    };
    
    errno = 0;
    if (fseek(fp, token.offset, SEEK_SET) != 0)
    {
        printf("fseek, strerror: %s.\n", strerror(errno));
        fclose(fp);
        return 1;
    };
    
    SecureMessage smsg;
    errno = 0;
    if (fread(&smsg.hash[0], sizeof(unsigned char), SMSG_HDR_LEN, fp) != (size_t)SMSG_HDR_LEN)
    {
        printf("fread header failed: %s\n", strerror(errno));
        fclose(fp);
        return 1;
    };
    
    try {
        vchData.resize(SMSG_HDR_LEN + smsg.nPayload);
    } catch (std::exception& e) {
        printf("SecureMsgRetrieve(): Could not resize vchData, %u, %s\n", SMSG_HDR_LEN + smsg.nPayload, e.what());
        return 1;
    };
    
    memcpy(&vchData[0], &smsg.hash[0], SMSG_HDR_LEN);
    errno = 0;
    if (fread(&vchData[SMSG_HDR_LEN], sizeof(unsigned char), smsg.nPayload, fp) != smsg.nPayload)
    {
        printf("fread data failed: %s. Wanted %u bytes.\n", strerror(errno), smsg.nPayload);
        fclose(fp);
        return 1;
    };
    
    
    fclose(fp);
    
    return 0;
};

int SecureMsgReceive(CNode* pfrom, std::vector<unsigned char>& vchData)
{
    if (fDebug)
        printf("SecureMsgReceive().\n");
    
    if (vchData.size() < 12) // nBunch4 + timestamp8
    {
        printf("Error: not enough data.\n");
        return 1;
    };
    
    uint32_t nBunch;
    int64_t bktTime;
    
    memcpy(&nBunch, &vchData[0], 4);
    memcpy(&bktTime, &vchData[4], 8);
    
    
    // -- check bktTime ()
    //    bucket may not exist yet - will be created when messages are added
    int64_t now = GetTime();
    if (bktTime > now + SMSG_TIME_LEEWAY)
    {
        if (fDebug)
            printf("bktTime > now.\n");
        // misbehave?
        return 1;
    } else
    if (bktTime < now - SMSG_RETENTION)
    {
        if (fDebug)
            printf("bktTime < now - SMSG_RETENTION.\n");
        // misbehave?
        return 1;
    };
    
    std::map<int64_t, SecMsgBucket>::iterator itb;
    
    if (nBunch == 0 || nBunch > 500)
    {
        printf("Error: Invalid no. messages received in bunch %u, for bucket %" PRId64 ".\n", nBunch, bktTime);
        pfrom->Misbehaving(1);
        
        // -- release lock on bucket if it exists
        itb = smsgBuckets.find(bktTime);
        if (itb != smsgBuckets.end())
            itb->second.nLockCount = 0;
        return 1;
    };
    
    uint32_t n = 12;
    
    for (uint32_t i = 0; i < nBunch; ++i)
    {
        if (vchData.size() - n < SMSG_HDR_LEN)
        {
            printf("Error: not enough data sent, n = %u.\n", n);
            break;
        };
        
        SecureMessage* psmsg = (SecureMessage*) &vchData[n];
        
        int rv;
        if ((rv = SecureMsgValidate(&vchData[n], &vchData[n + SMSG_HDR_LEN], psmsg->nPayload)) != 0)
        {
            // message dropped
            if (rv == 2) // invalid proof of work
            {
                pfrom->Misbehaving(10);
            } else
            {
                pfrom->Misbehaving(1);
            };
            continue;
        };
        
        // -- store message, but don't hash bucket
        if (SecureMsgStore(&vchData[n], &vchData[n + SMSG_HDR_LEN], psmsg->nPayload, false) != 0)
        {
            // message dropped
            break; // continue?
        };
        
        if (SecureMsgScanMessage(&vchData[n], &vchData[n + SMSG_HDR_LEN], psmsg->nPayload, true) != 0)
        {
            // message recipient is not this node (or failed)
        };
        
        n += SMSG_HDR_LEN + psmsg->nPayload;
    };
    
    // -- if messages have been added, bucket must exist now
    itb = smsgBuckets.find(bktTime);
    if (itb == smsgBuckets.end())
    {
        if (fDebug)
            printf("Don't have bucket %" PRId64 ".\n", bktTime);
        return 1;
    };
    
    itb->second.nLockCount  = 0; // this node has received data from peer, release lock
    itb->second.nLockPeerId = 0;
    itb->second.hashBucket();
    
    return 0;
};

int SecureMsgStoreUnscanned(unsigned char *pHeader, unsigned char *pPayload, uint32_t nPayload)
{
    /*
    When the wallet is locked a copy of each received message is stored to be scanned later if wallet is unlocked
    */
    
    if (fDebug)
        printf("SecureMsgStoreUnscanned()\n");
    
    if (!pHeader
        || !pPayload)
    {
        printf("Error: null pointer to header or payload.\n");
        return 1;
    };
    
    SecureMessage* psmsg = (SecureMessage*) pHeader;
    
    fs::path pathSmsgDir;
    try {
        pathSmsgDir = GetDataDir() / "smsgStore";
        fs::create_directory(pathSmsgDir);
    } catch (const filesystem_error& ex)
    {
        printf("Error: Failed to create directory %s - %s\n", pathSmsgDir.string().c_str(), ex.what());
        return 1;
    };
    
    int64_t now = GetTime();
    if (psmsg->timestamp > now + SMSG_TIME_LEEWAY)
    {
        printf("Message > now.\n");
        return 1;
    } else
    if (psmsg->timestamp < now - SMSG_RETENTION)
    {
        printf("Message < SMSG_RETENTION.\n");
        return 1;
    };
    
    int64_t bucket = psmsg->timestamp - (psmsg->timestamp % SMSG_BUCKET_LEN);

    std::string fileName = boost::lexical_cast<std::string>(bucket) + "_01_wl.dat";
    fs::path fullpath = pathSmsgDir / fileName;
    
    FILE *fp;
    errno = 0;
    if (!(fp = fopen(fullpath.string().c_str(), "ab")))
    {
        printf("Error opening file: %s\n", strerror(errno));
        return 1;
    };
    
    if (fwrite(pHeader, sizeof(unsigned char), SMSG_HDR_LEN, fp) != (size_t)SMSG_HDR_LEN
        || fwrite(pPayload, sizeof(unsigned char), nPayload, fp) != nPayload)
    {
        printf("fwrite failed: %s\n", strerror(errno));
        fclose(fp);
        return 1;
    };
    
    fclose(fp);
    
    return 0;
};


int SecureMsgStore(unsigned char *pHeader, unsigned char *pPayload, uint32_t nPayload, bool fUpdateBucket)
{
    if (fDebug)
        printf("SecureMsgStore()\n");
    
    if (!pHeader
        || !pPayload)
    {
        printf("Error: null pointer to header or payload.\n");
        return 1;
    };
    
    SecureMessage* psmsg = (SecureMessage*) pHeader;
    
    
    long int ofs;
    fs::path pathSmsgDir;
    try {
        pathSmsgDir = GetDataDir() / "smsgStore";
        fs::create_directory(pathSmsgDir);
    } catch (const filesystem_error& ex)
    {
        printf("Error: Failed to create directory %s - %s\n", pathSmsgDir.string().c_str(), ex.what());
        return 1;
    };
    
    int64_t now = GetTime();
    if (psmsg->timestamp > now + SMSG_TIME_LEEWAY)
    {
        printf("Message > now.\n");
        return 1;
    } else
    if (psmsg->timestamp < now - SMSG_RETENTION)
    {
        printf("Message < SMSG_RETENTION.\n");
        return 1;
    };
    
    int64_t bucket = psmsg->timestamp - (psmsg->timestamp % SMSG_BUCKET_LEN);
    
    {
        // -- must lock cs_smsg before calling
        //LOCK(cs_smsg);
        
        SecMsgToken token(psmsg->timestamp, pPayload, nPayload, 0);
        
        std::set<SecMsgToken>& tokenSet = smsgBuckets[bucket].setTokens;
        std::set<SecMsgToken>::iterator it;
        it = tokenSet.find(token);
        if (it != tokenSet.end())
        {
            printf("Already have message.\n");
            if (fDebug)
            {
                printf("nPayload: %u\n", nPayload);
                printf("bucket: %" PRId64 "\n", bucket);
                
                printf("message ts: %" PRId64, token.timestamp);
                std::vector<unsigned char> vchShow;
                vchShow.resize(8);
                memcpy(&vchShow[0], token.sample, 8);
                printf(" sample %s\n", ValueString(vchShow).c_str());
                /*
                printf("\nmessages in bucket:\n");
                for (it = tokenSet.begin(); it != tokenSet.end(); ++it)
                {
                    printf("message ts: %"PRId64, (*it).timestamp);
                    vchShow.resize(8);
                    memcpy(&vchShow[0], (*it).sample, 8);
                    printf(" sample %s\n", ValueString(vchShow).c_str());
                };
                */
            };
            return 1;
        };
        
        std::string fileName = boost::lexical_cast<std::string>(bucket) + "_01.dat";
        fs::path fullpath = pathSmsgDir / fileName;
        
        FILE *fp;
        errno = 0;
        if (!(fp = fopen(fullpath.string().c_str(), "ab")))
        {
            printf("Error opening file: %s\n", strerror(errno));
            return 1;
        };
        
        // -- on windows ftell will always return 0 after fopen(ab), call fseek to set.
        errno = 0;
        if (fseek(fp, 0, SEEK_END) != 0)
        {
            printf("Error fseek failed: %s\n", strerror(errno));
            return 1;
        };
        
        
        ofs = ftell(fp);
        
        if (fwrite(pHeader, sizeof(unsigned char), SMSG_HDR_LEN, fp) != (size_t)SMSG_HDR_LEN
            || fwrite(pPayload, sizeof(unsigned char), nPayload, fp) != nPayload)
        {
            printf("fwrite failed: %s\n", strerror(errno));
            fclose(fp);
            return 1;
        };
        
        fclose(fp);
        
        token.offset = ofs;
        
        //printf("token.offset: %"PRId64"\n", token.offset); // DEBUG
        tokenSet.insert(token);
        
        if (fUpdateBucket)
            smsgBuckets[bucket].hashBucket();
    };
    
    //if (fDebug)
    printf("SecureMsg added to bucket %" PRId64 ".\n", bucket);
    return 0;
};

int SecureMsgStore(SecureMessage& smsg, bool fUpdateBucket)
{
    return SecureMsgStore(&smsg.hash[0], smsg.pPayload, smsg.nPayload, fUpdateBucket);
};
  
int SecureMsgValidate(unsigned char *pHeader, unsigned char *pPayload, uint32_t nPayload)
{
    /*
    returns
        0 success
        1 error
        2 invalid hash
        3 checksum mismatch
        4 invalid version
        5 payload is too large
    */
    SecureMessage* psmsg = (SecureMessage*) pHeader;
    
    if (psmsg->version[0] != 1)
        return 4;
    
    if (nPayload > SMSG_MAX_MSG_WORST)
        return 5;
    
    unsigned char civ[32];
    unsigned char sha256Hash[32];
    int rv = 2; // invalid
    
    uint32_t nonse;
    memcpy(&nonse, &psmsg->nonse[0], 4);
    
    if (fDebug)
        printf("SecureMsgValidate() nonse %u.\n", nonse);
    
    for (int i = 0; i < 32; i+=4)
        memcpy(civ+i, &nonse, 4);
    
    HMAC_CTX *ctx = HMAC_CTX_new();
    
    unsigned int nBytes;
    if (!HMAC_Init_ex(ctx, &civ[0], 32, EVP_sha256(), NULL)
        || !HMAC_Update(ctx, (unsigned char*) pHeader+4, SMSG_HDR_LEN-4)
        || !HMAC_Update(ctx, (unsigned char*) pPayload, nPayload)
        || !HMAC_Update(ctx, pPayload, nPayload)
        || !HMAC_Final(ctx, sha256Hash, &nBytes)
        || nBytes != 32)
    {
        if (fDebug)
            printf("HMAC error.\n");
        rv = 1; // error
    } else
    {
        if (sha256Hash[31] == 0
            && sha256Hash[30] == 0
            && (~(sha256Hash[29]) & ((1<<0) | (1<<1) | (1<<2)) ))
        {
            if (fDebug)
                printf("Hash Valid.\n");
            rv = 0; // smsg is valid
        };
        
        if (memcmp(psmsg->hash, sha256Hash, 4) != 0)
        {
             if (fDebug)
                printf("Checksum mismatch.\n");
            rv = 3; // checksum mismatch
        }
    }

    HMAC_CTX_free(ctx);
    
    return rv;
};

int SecureMsgSetHash(unsigned char *pHeader, unsigned char *pPayload, uint32_t nPayload)
{
    /*  proof of work and checksum
        
        May run in a thread, if shutdown detected, return.
        
        returns:
            0 success
            1 error
            2 stopped due to node shutdown
        
    */
    
    SecureMessage* psmsg = (SecureMessage*) pHeader;
    
    int64_t nStart = GetTimeMillis();
    unsigned char civ[32];
    unsigned char sha256Hash[32];
    
    //std::vector<unsigned char> vchHash;
    //vchHash.resize(32);
    
    bool found = false;
    HMAC_CTX *ctx = HMAC_CTX_new();
    
    uint32_t nonse = 0;
    
    //CBigNum bnTarget(2);
    //bnTarget = bnTarget.pow(256 - 40);
    
    // -- break for HMAC_CTX_cleanup
    for (;;)
    {
        if (!fSecMsgEnabled)
           break;
        
        //psmsg->timestamp = GetTime();
        //memcpy(&psmsg->timestamp, &now, 8);
        memcpy(&psmsg->nonse[0], &nonse, 4);
        
        for (int i = 0; i < 32; i+=4)
            memcpy(civ+i, &nonse, 4);
        
        unsigned int nBytes;
        if (!HMAC_Init_ex(ctx, &civ[0], 32, EVP_sha256(), NULL)
            || !HMAC_Update(ctx, (unsigned char*) pHeader+4, SMSG_HDR_LEN-4)
            || !HMAC_Update(ctx, (unsigned char*) pPayload, nPayload)
            || !HMAC_Update(ctx, pPayload, nPayload)
            || !HMAC_Final(ctx, sha256Hash, &nBytes)
            //|| !HMAC_Final(&ctx, &vchHash[0], &nBytes)
            || nBytes != 32)
            break;
        
        /*
        if (CBigNum(vchHash) <= bnTarget)
        {
            found = true;
            if (fDebug)
                printf("Match %u\n", nonse);
            break;
        };
        */
        
        if (sha256Hash[31] == 0
            && sha256Hash[30] == 0
            && (~(sha256Hash[29]) & ((1<<0) | (1<<1) | (1<<2)) ))
        //    && sha256Hash[29] == 0)
        {
            found = true;
            //if (fDebug)
            //    printf("Match %u\n", nonse);
            break;
        }
        
        //if (nonse >= UINT32_MAX)
        if (nonse >= 4294967295U)
        {
            if (fDebug)
                printf("No match %u\n", nonse);
            break;
            //return 1; 
        }    
        nonse++;
    };
    
    HMAC_CTX_free(ctx);
    
    if (!fSecMsgEnabled)
    {
        if (fDebug)
            printf("SecureMsgSetHash() stopped, shutdown detected.\n");
        return 2;
    };
    
    if (!found)
    {
        if (fDebug)
            printf("SecureMsgSetHash() failed, took %" PRId64 " ms, nonse %u\n", GetTimeMillis() - nStart, nonse);
        return 1;
    };
    
    memcpy(psmsg->hash, sha256Hash, 4);
    //memcpy(psmsg->hash, &vchHash[0], 4);
    
    if (fDebug)
        printf("SecureMsgSetHash() took %" PRId64 " ms, nonse %u\n", GetTimeMillis() - nStart, nonse);
    
    return 0;
};

int SecureMsgEncrypt(SecureMessage& smsg, std::string& addressFrom, std::string& addressTo, std::string& message)
{
    /* Create a secure message
        
        Using similar method to bitmessage.
        If bitmessage is secure this should be too.
        https://bitmessage.org/wiki/Encryption
        
        Some differences:
        bitmessage seems to use curve sect283r1
        *coin addresses use secp256k1
        
        returns
            2       message is too long.
            3       addressFrom is invalid.
            4       addressTo is invalid.
            5       Could not get public key for addressTo.
            6       ECDH_compute_key failed
            7       Could not get private key for addressFrom.
            8       Could not allocate memory.
            9       Could not compress message data.
            10      Could not generate MAC.
            11      Encrypt failed.
    */
    
    if (fDebug)
        printf("SecureMsgEncrypt(%s, %s, ...)\n", addressFrom.c_str(), addressTo.c_str());
    
    
    if (message.size() > SMSG_MAX_MSG_BYTES)
    {
        printf("Message is too long, %" PRIszu ".\n", message.size());
        return 2;
    };
    
    smsg.version[0] = 1;
    smsg.version[1] = 1;
    smsg.timestamp = GetTime();
    
    
    bool fSendAnonymous;
    CBitcoinAddress coinAddrFrom;
    CKeyID ckidFrom;
    CKey keyFrom;
    
    if (addressFrom.compare("anon") == 0)
    {
        fSendAnonymous = true;
        
    } else
    {
        fSendAnonymous = false;
        
        if (!coinAddrFrom.SetString(addressFrom))
        {
            printf("addressFrom is not valid.\n");
            return 3;
        };
        
        if (!coinAddrFrom.GetKeyID(ckidFrom))
        {
            printf("coinAddrFrom.GetKeyID failed: %s.\n", coinAddrFrom.ToString().c_str());
            return 3;
        };
    };
    
    
    CBitcoinAddress coinAddrDest;
    CKeyID ckidDest;
    
    if (!coinAddrDest.SetString(addressTo))
    {
        printf("addressTo is not valid.\n");
        return 4;
    };
    
    if (!coinAddrDest.GetKeyID(ckidDest))
    {
        printf("coinAddrDest.GetKeyID failed: %s.\n", coinAddrDest.ToString().c_str());
        return 4;
    };
    
    // -- public key K is the destination address
    CPubKey cpkDestK;
    if (SecureMsgGetStoredKey(ckidDest, cpkDestK) != 0
        && SecureMsgGetLocalKey(ckidDest, cpkDestK) != 0) // maybe it's a local key (outbox?)
    {
        printf("Could not get public key for destination address.\n");
        return 5;
    };
    
    
    // -- Generate 16 random bytes as IV.
    RandAddSeedPerfmon();
    RAND_bytes(&smsg.iv[0], 16);
    
    
    // -- Generate a new random EC key pair with private key called r and public key called R.
    CKey keyR;
    keyR.MakeNewKey(true); // make compressed key
    
    
    // -- Do an EC point multiply with public key K and private key r. This gives you public key P. 
    CKey keyK;
    if (!keyK.SetPubKey(cpkDestK))
    {
        printf("Could not set pubkey for K: %s.\n", ValueString(cpkDestK.Raw()).c_str());
        return 4; // address to is invalid
    };
    
    std::vector<unsigned char> vchP;
    vchP.resize(32);
    EC_KEY* pkeyr = keyR.GetECKey();
    EC_KEY* pkeyK = keyK.GetECKey();
    
    // always seems to be 32, worth checking?
    //int field_size = EC_GROUP_get_degree(EC_KEY_get0_group(pkeyr));
    //int secret_len = (field_size+7)/8;
    //printf("secret_len %d.\n", secret_len);
    
    // -- ECDH_compute_key returns the same P if fed compressed or uncompressed public keys
    EC_KEY_SET_DEFAULT_METHOD(pkeyr);
    int lenP = ECDH_compute_key(&vchP[0], 32, EC_KEY_get0_public_key(pkeyK), pkeyr, NULL);
    
    if (lenP != 32)
    {
        printf("ECDH_compute_key failed, lenP: %d.\n", lenP);
        return 6;
    };
    
    CPubKey cpkR = keyR.GetPubKey();
    if (!cpkR.IsValid()
        || !cpkR.IsCompressed())
    {
        printf("Could not get public key for key R.\n");
        return 1;
    };
    
    memcpy(smsg.cpkR, &cpkR.Raw()[0], 33);
    
    
    // -- Use public key P and calculate the SHA512 hash H.
    //    The first 32 bytes of H are called key_e and the last 32 bytes are called key_m.
    std::vector<unsigned char> vchHashed;
    vchHashed.resize(64); // 512
    SHA512(&vchP[0], vchP.size(), (unsigned char*)&vchHashed[0]);
    std::vector<unsigned char> key_e(&vchHashed[0], &vchHashed[0]+32);
    std::vector<unsigned char> key_m(&vchHashed[32], &vchHashed[32]+32);
    
    
    std::vector<unsigned char> vchPayload;
    std::vector<unsigned char> vchCompressed;
    unsigned char* pMsgData;
    uint32_t lenMsgData;
    
    uint32_t lenMsg = message.size();
    if (lenMsg > 128)
    {
        // -- only compress if over 128 bytes
        int worstCase = LZ4_compressBound(message.size());
        try {
            vchCompressed.resize(worstCase);
        } catch (std::exception& e) {
            printf("vchCompressed.resize %u threw: %s.\n", worstCase, e.what());
            return 8;
        };
        
        int lenComp = LZ4_compress((char*)message.c_str(), (char*)&vchCompressed[0], lenMsg);
        if (lenComp < 1)
        {
            printf("Could not compress message data.\n");
            return 9;
        };
        
        pMsgData = &vchCompressed[0];
        lenMsgData = lenComp;
        
    } else
    {
        // -- no compression
        pMsgData = (unsigned char*)message.c_str();
        lenMsgData = lenMsg;
    };
    
    if (fSendAnonymous)
    {
        try {
            vchPayload.resize(9 + lenMsgData);
        } catch (std::exception& e) {
            printf("vchPayload.resize %u threw: %s.\n", 9 + lenMsgData, e.what());
            return 8;
        };
        
        memcpy(&vchPayload[9], pMsgData, lenMsgData);
        
        vchPayload[0] = 250; // id as anonymous message
        // -- next 4 bytes are unused - there to ensure encrypted payload always > 8 bytes
        memcpy(&vchPayload[5], &lenMsg, 4); // length of uncompressed plain text
    } else
    {
        try {
            vchPayload.resize(SMSG_PL_HDR_LEN + lenMsgData);
        } catch (std::exception& e) {
            printf("vchPayload.resize %u threw: %s.\n", SMSG_PL_HDR_LEN + lenMsgData, e.what());
            return 8;
        };
        memcpy(&vchPayload[SMSG_PL_HDR_LEN], pMsgData, lenMsgData);
        // -- compact signature proves ownership of from address and allows the public key to be recovered, recipient can always reply.
        if (!pwalletMain->GetKey(ckidFrom, keyFrom))
        {
            printf("Could not get private key for addressFrom.\n");
            return 7;
        };
        
        // -- sign the plaintext
        std::vector<unsigned char> vchSignature;
        vchSignature.resize(65);
        keyFrom.SignCompact(Hash(message.begin(), message.end()), vchSignature);
        
        // -- Save some bytes by sending address raw
        vchPayload[0] = (static_cast<CBitcoinAddress_B*>(&coinAddrFrom))->getVersion(); // vchPayload[0] = coinAddrDest.nVersion;
        memcpy(&vchPayload[1], (static_cast<CKeyID_B*>(&ckidFrom))->GetPPN(), 20); // memcpy(&vchPayload[1], ckidDest.pn, 20);
        
        memcpy(&vchPayload[1+20], &vchSignature[0], vchSignature.size());
        memcpy(&vchPayload[1+20+65], &lenMsg, 4); // length of uncompressed plain text
    };
    
    
    SecMsgCrypter crypter;
    crypter.SetKey(key_e, smsg.iv);
    std::vector<unsigned char> vchCiphertext;
    
    if (!crypter.Encrypt(&vchPayload[0], vchPayload.size(), vchCiphertext))
    {
        printf("crypter.Encrypt failed.\n");
        return 11;
    };
    
    // Modern C++ Migration: Use vector instead of raw array for better memory safety
    try {
        smsg.vchPayload.resize(vchCiphertext.size());
        std::copy(vchCiphertext.begin(), vchCiphertext.end(), smsg.vchPayload.begin());
        smsg.nPayload = vchCiphertext.size();
        smsg.pPayload = smsg.vchPayload.data(); // Compatibility pointer
    } catch (std::exception& e)
    {
        printf("Could not allocate pPayload, exception: %s.\n", e.what());
        return 8;
    }
    
    
    // -- Calculate a 32 byte MAC with HMACSHA256, using key_m as salt
    //    Message authentication code, (hash of timestamp + destination + payload)
    bool fHmacOk = true;
    unsigned int nBytes = 32;
    HMAC_CTX *ctx = HMAC_CTX_new();
    
    if (!HMAC_Init_ex(ctx, &key_m[0], 32, EVP_sha256(), NULL)
        || !HMAC_Update(ctx, (unsigned char*) &smsg.timestamp, sizeof(smsg.timestamp))
        || !HMAC_Update(ctx, &vchCiphertext[0], vchCiphertext.size())
        || !HMAC_Final(ctx, smsg.mac, &nBytes)
        || nBytes != 32)
        fHmacOk = false;
    
    HMAC_CTX_free(ctx);
    
    if (!fHmacOk)
    {
        printf("Could not generate MAC.\n");
        return 10;
    };
    
    
    return 0;
};

int SecureMsgSend(std::string& addressFrom, std::string& addressTo, std::string& message, std::string& sError)
{
    /* Encrypt secure message, and place it on the network
        Make a copy of the message to sender's first address and place in send queue db
        proof of work thread will pick up messages from  send queue db
        
    */
    
    if (fDebug)
        printf("SecureMsgSend(%s, %s, ...)\n", addressFrom.c_str(), addressTo.c_str());
    
    if (pwalletMain->IsLocked())
    {
        sError = "Wallet is locked, wallet must be unlocked to send and recieve messages.";
        printf("Wallet is locked, wallet must be unlocked to send and recieve messages.\n");
        return 1;
    };
    
    if (message.size() > SMSG_MAX_MSG_BYTES)
    {
        std::ostringstream oss;
        oss << message.size() << " > " << SMSG_MAX_MSG_BYTES;
        sError = "Message is too long, " + oss.str();
        printf("Message is too long, %" PRIszu ".\n", message.size());
        return 1;
    };
    
    
    int rv;
    SecureMessage smsg;
    
    if ((rv = SecureMsgEncrypt(smsg, addressFrom, addressTo, message)) != 0)
    {
        printf("SecureMsgSend(), encrypt for recipient failed.\n");
        
        switch(rv)
        {
            case 2:  sError = "Message is too long.";                       break;
            case 3:  sError = "Invalid addressFrom.";                       break;
            case 4:  sError = "Invalid addressTo.";                         break;
            case 5:  sError = "Could not get public key for addressTo.";    break;
            case 6:  sError = "ECDH_compute_key failed.";                   break;
            case 7:  sError = "Could not get private key for addressFrom."; break;
            case 8:  sError = "Could not allocate memory.";                 break;
            case 9:  sError = "Could not compress message data.";           break;
            case 10: sError = "Could not generate MAC.";                    break;
            case 11: sError = "Encrypt failed.";                            break;
            default: sError = "Unspecified Error.";                         break;
        };
        
        return rv;
    };
    
    
    // -- Place message in send queue, proof of work will happen in a thread.
    std::string sPrefix("qm");
    unsigned char chKey[18];
    memcpy(&chKey[0],  sPrefix.data(),  2);
    memcpy(&chKey[2],  &smsg.timestamp, 8);
    memcpy(&chKey[10], &smsg.pPayload,  8);
    
    SecMsgStored smsgSQ;
    
    smsgSQ.timeReceived  = GetTime();
    smsgSQ.sAddrTo       = addressTo;
    
    try {
        smsgSQ.vchMessage.resize(SMSG_HDR_LEN + smsg.nPayload);
    } catch (std::exception& e) {
        printf("smsgSQ.vchMessage.resize %u threw: %s.\n", SMSG_HDR_LEN + smsg.nPayload, e.what());
        sError = "Could not allocate memory.";
        return 8;
    };
    
    memcpy(&smsgSQ.vchMessage[0], &smsg.hash[0], SMSG_HDR_LEN);
    memcpy(&smsgSQ.vchMessage[SMSG_HDR_LEN], smsg.pPayload, smsg.nPayload);
    
    {
        LOCK(cs_smsgDB);
        SecMsgDB dbSendQueue;
        if (dbSendQueue.Open("cw"))
        {
            dbSendQueue.WriteSmesg(chKey, smsgSQ);
            //NotifySecMsgSendQueueChanged(smsgOutbox);
        };
    }
    
    // TODO: only update outbox when proof of work thread is done.
    
    //  -- for outbox create a copy encrypted for owned address
    //     if the wallet is encrypted private key needed to decrypt will be unavailable
    
    if (fDebug)
        printf("Encrypting message for outbox.\n");
    
    std::string addressOutbox = "None";
    CBitcoinAddress coinAddrOutbox;
    
    // Modern C++ Migration: Range-based for loop with auto type deduction
    for (const auto& entry : pwalletMain->mapAddressBook)
    {
        // -- get first owned address
        if (!IsMine(*pwalletMain, entry.first))
            continue;
        
        const CBitcoinAddress& address = entry.first;
        
        addressOutbox = address.ToString();
        if (!coinAddrOutbox.SetString(addressOutbox)) // test valid
            continue;
        break;
    }
    
    if (addressOutbox == "None")
    {
        printf("Warning: SecureMsgSend() could not find an address to encrypt outbox message with.\n");
    } else
    {
        if (fDebug)
            printf("Encrypting a copy for outbox, using address %s\n", addressOutbox.c_str());
        
        SecureMessage smsgForOutbox;
        if ((rv = SecureMsgEncrypt(smsgForOutbox, addressFrom, addressOutbox, message)) != 0)
        {
            printf("SecureMsgSend(), encrypt for outbox failed, %d.\n", rv);
        } else
        {
            // -- save sent message to db
            std::string sPrefix("sm");
            unsigned char chKey[18];
            memcpy(&chKey[0],  sPrefix.data(),           2);
            memcpy(&chKey[2],  &smsgForOutbox.timestamp, 8);
            memcpy(&chKey[10], &smsgForOutbox.pPayload,  8);   // sample
            
            SecMsgStored smsgOutbox;
            
            smsgOutbox.timeReceived  = GetTime();
            smsgOutbox.sAddrTo       = addressTo;
            smsgOutbox.sAddrOutbox   = addressOutbox;
            
            try {
                smsgOutbox.vchMessage.resize(SMSG_HDR_LEN + smsgForOutbox.nPayload);
            } catch (std::exception& e) {
                printf("smsgOutbox.vchMessage.resize %u threw: %s.\n", SMSG_HDR_LEN + smsgForOutbox.nPayload, e.what());
                sError = "Could not allocate memory.";
                return 8;
            };
            memcpy(&smsgOutbox.vchMessage[0], &smsgForOutbox.hash[0], SMSG_HDR_LEN);
            memcpy(&smsgOutbox.vchMessage[SMSG_HDR_LEN], smsgForOutbox.pPayload, smsgForOutbox.nPayload);
            
            
            {
                LOCK(cs_smsgDB);
                SecMsgDB dbSent;
                
                if (dbSent.Open("cw"))
                {
                    dbSent.WriteSmesg(chKey, smsgOutbox);
                    NotifySecMsgOutboxChanged(smsgOutbox);
                };
            }
        };
    };
    
    if (fDebug)
        printf("Secure message queued for sending to %s.\n", addressTo.c_str());
    
    return 0;
};


int SecureMsgDecrypt(bool fTestOnly, std::string& address, unsigned char *pHeader, unsigned char *pPayload, uint32_t nPayload, MessageData& msg)
{
    /* Decrypt secure message
        
        address is the owned address to decrypt with.
        
        validate first in SecureMsgValidate
        
        returns
            1       Error
            2       Unknown version number
            3       Decrypt address is not valid.
            8       Could not allocate memory
    */
    
    if (fDebug)
        printf("SecureMsgDecrypt(), using %s, testonly %d.\n", address.c_str(), fTestOnly);
    
    if (!pHeader
        || !pPayload)
    {
        printf("Error: null pointer to header or payload.\n");
        return 1;
    };
    
    SecureMessage* psmsg = (SecureMessage*) pHeader;
    
    
    if (psmsg->version[0] != 1)
    {
        printf("Unknown version number.\n");
        return 2;
    };
    
    
    
    // -- Fetch private key k, used to decrypt
    CBitcoinAddress coinAddrDest;
    CKeyID ckidDest;
    CKey keyDest;
    if (!coinAddrDest.SetString(address))
    {
        printf("Address is not valid.\n");
        return 3;
    };
    if (!coinAddrDest.GetKeyID(ckidDest))
    {
        printf("coinAddrDest.GetKeyID failed: %s.\n", coinAddrDest.ToString().c_str());
        return 3;
    };
    if (!pwalletMain->GetKey(ckidDest, keyDest))
    {
        printf("Could not get private key for addressDest.\n");
        return 3;
    };
    
    
    
    CKey keyR;
    std::vector<unsigned char> vchR(psmsg->cpkR, psmsg->cpkR+33); // would be neater to override CPubKey() instead
    CPubKey cpkR(vchR);
    if (!cpkR.IsValid())
    {
        printf("Could not get public key for key R.\n");
        return 1;
    };
    if (!keyR.SetPubKey(cpkR))
    {
        printf("Could not set pubkey for R: %s.\n", ValueString(cpkR.Raw()).c_str());
        return 1;
    };
    
    cpkR = keyR.GetPubKey();
    if (!cpkR.IsValid()
        || !cpkR.IsCompressed())
    {
        printf("Could not get compressed public key for key R.\n");
        return 1;
    };
    
    
    // -- Do an EC point multiply with private key k and public key R. This gives you public key P.
    std::vector<unsigned char> vchP;
    vchP.resize(32);
    EC_KEY* pkeyk = keyDest.GetECKey();
    EC_KEY* pkeyR = keyR.GetECKey();
    
    EC_KEY_SET_DEFAULT_METHOD(pkeyk);
    int lenPdec = ECDH_compute_key(&vchP[0], 32, EC_KEY_get0_public_key(pkeyR), pkeyk, NULL);
    
    if (lenPdec != 32)
    {
        printf("ECDH_compute_key failed, lenPdec: %d.\n", lenPdec);
        return 1;
    };
    
    
    // -- Use public key P to calculate the SHA512 hash H. 
    //    The first 32 bytes of H are called key_e and the last 32 bytes are called key_m. 
    std::vector<unsigned char> vchHashedDec;
    vchHashedDec.resize(64);    // 512 bits
    SHA512(&vchP[0], vchP.size(), (unsigned char*)&vchHashedDec[0]);
    std::vector<unsigned char> key_e(&vchHashedDec[0], &vchHashedDec[0]+32);
    std::vector<unsigned char> key_m(&vchHashedDec[32], &vchHashedDec[32]+32);
    
    
    // -- Message authentication code, (hash of timestamp + destination + payload)
    unsigned char MAC[32];
    bool fHmacOk = true;
    unsigned int nBytes = 32;
    HMAC_CTX *ctx = HMAC_CTX_new();
    
    if (!HMAC_Init_ex(ctx, &key_m[0], 32, EVP_sha256(), NULL)
        || !HMAC_Update(ctx, (unsigned char*) &psmsg->timestamp, sizeof(psmsg->timestamp))
        || !HMAC_Update(ctx, pPayload, nPayload)
        || !HMAC_Final(ctx, MAC, &nBytes)
        || nBytes != 32)
        fHmacOk = false;
    
    HMAC_CTX_free(ctx);
    
    if (!fHmacOk)
    {
        printf("Could not generate MAC.\n");
        return 1;
    };
    
    if (memcmp(MAC, psmsg->mac, 32) != 0)
    {
        if (fDebug)
            printf("MAC does not match.\n"); // expected if message is not to address on node
        
        return 1;
    };
    
    if (fTestOnly)
        return 0;
    
    SecMsgCrypter crypter;
    crypter.SetKey(key_e, psmsg->iv);
    std::vector<unsigned char> vchPayload;
    if (!crypter.Decrypt(pPayload, nPayload, vchPayload))
    {
        printf("Decrypt failed.\n");
        return 1;
    };
    
    msg.timestamp = psmsg->timestamp;
    uint32_t lenData;
    uint32_t lenPlain;
    
    unsigned char* pMsgData;
    bool fFromAnonymous;
    if ((uint32_t)vchPayload[0] == 250)
    {
        fFromAnonymous = true;
        lenData = vchPayload.size() - (9);
        memcpy(&lenPlain, &vchPayload[5], 4);
        pMsgData = &vchPayload[9];
    } else
    {
        fFromAnonymous = false;
        lenData = vchPayload.size() - (SMSG_PL_HDR_LEN);
        memcpy(&lenPlain, &vchPayload[1+20+65], 4);
        pMsgData = &vchPayload[SMSG_PL_HDR_LEN];
    };
    
    try {
        msg.vchMessage.resize(lenPlain + 1);
    } catch (std::exception& e) {
        printf("msg.vchMessage.resize %u threw: %s.\n", lenPlain + 1, e.what());
        return 8;
    };
    
    
    if (lenPlain > 128)
    {
        // -- decompress
        if (LZ4_decompress_safe((char*) pMsgData, (char*) &msg.vchMessage[0], lenData, lenPlain) != (int) lenPlain)
        {
            printf("Could not decompress message data.\n");
            return 1;
        };
    } else
    {
        // -- plaintext
        memcpy(&msg.vchMessage[0], pMsgData, lenPlain);
    };
    
    msg.vchMessage[lenPlain] = '\0';
    
    if (fFromAnonymous)
    {
        // -- Anonymous sender
        msg.sFromAddress = "anon";
    } else
    {
        std::vector<unsigned char> vchUint160;
        vchUint160.resize(20);
        
        memcpy(&vchUint160[0], &vchPayload[1], 20);
        
        uint160 ui160(vchUint160);
        CKeyID ckidFrom(ui160);
        
        CBitcoinAddress coinAddrFrom;
        coinAddrFrom.Set(ckidFrom);
        if (!coinAddrFrom.IsValid())
        {
            printf("From Addess is invalid.\n");
            return 1;
        };
        
        std::vector<unsigned char> vchSig;
        vchSig.resize(65);
        
        memcpy(&vchSig[0], &vchPayload[1+20], 65);
        
        CKey keyFrom;
        keyFrom.SetCompactSignature(Hash(msg.vchMessage.begin(), msg.vchMessage.end()-1), vchSig);
        CPubKey cpkFromSig = keyFrom.GetPubKey();
        if (!cpkFromSig.IsValid())
        {
            printf("Signature validation failed.\n");
            return 1;
        };
        
        // -- get address for the compressed public key
        CBitcoinAddress coinAddrFromSig;
        coinAddrFromSig.Set(cpkFromSig.GetID());
        
        if (!(coinAddrFrom == coinAddrFromSig))
        {
            printf("Signature validation failed.\n");
            return 1;
        };
        
        cpkFromSig = keyFrom.GetPubKey();
        
        int rv = 5;
        try {
            rv = SecureMsgInsertAddress(ckidFrom, cpkFromSig);
        } catch (std::exception& e) {
            printf("SecureMsgInsertAddress(), exception: %s.\n", e.what());
            //return 1;
        };
        
        switch(rv)
        {
            case 0:
                printf("Sender public key added to db.\n");
                break;
            case 4:
                printf("Sender public key already in db.\n");
                break;
            default:
                printf("Error adding sender public key to db.\n");
                break;
        };
        
        msg.sFromAddress = coinAddrFrom.ToString();
    };
    
    if (fDebug)
        printf("Decrypted message for %s.\n", address.c_str());
    
    return 0;
};

int SecureMsgDecrypt(bool fTestOnly, std::string& address, SecureMessage& smsg, MessageData& msg)
{
    return SecureMsgDecrypt(fTestOnly, address, &smsg.hash[0], smsg.pPayload, smsg.nPayload, msg);
};
