// Copyright (c) 2014 The ShadowCoin developers
// Copyright (c) 2018 Verge
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef VERGE_SMESSAGE_H
#define VERGE_SMESSAGE_H

#include <leveldb/db.h>
#include <leveldb/write_batch.h>

#include <net.h>
#include <db.h>
#include <wallet.h>
#include <lz4/lz4.h>

// Modern C++ Migration: Smart pointer and algorithm support
#include <memory>
#include <algorithm>


const unsigned int SMSG_HDR_LEN         = 104;               // length of unencrypted header, 4 + 2 + 1 + 8 + 16 + 33 + 32 + 4 +4
const unsigned int SMSG_PL_HDR_LEN      = 1+20+65+4;         // length of encrypted header in payload

const unsigned int SMSG_BUCKET_LEN      = 60 * 10;           // in seconds
const unsigned int SMSG_RETENTION       = 60 * 60 * 48;      // in seconds
const unsigned int SMSG_SEND_DELAY      = 2;                 // in seconds, SecureMsgSendData will delay this long between firing
const unsigned int SMSG_THREAD_DELAY    = 20;

const unsigned int SMSG_TIME_LEEWAY     = 60;
const unsigned int SMSG_TIME_IGNORE     = 90;                // seconds that a peer is ignored for if they fail to deliver messages for a smsgWant


const unsigned int SMSG_MAX_MSG_BYTES   = 4096;              // the user input part

// max size of payload worst case compression
const unsigned int SMSG_MAX_MSG_WORST = LZ4_COMPRESSBOUND(SMSG_MAX_MSG_BYTES+SMSG_PL_HDR_LEN);



#define SMSG_MASK_UNREAD            (1 << 0)



extern bool fSecMsgEnabled;

class SecMsgStored;

// Inbox db changed, called with lock cs_smsgDB held.
extern boost::signals2::signal<void (SecMsgStored& inboxHdr)> NotifySecMsgInboxChanged;

// Outbox db changed, called with lock cs_smsgDB held.
extern boost::signals2::signal<void (SecMsgStored& outboxHdr)> NotifySecMsgOutboxChanged;

// Wallet Unlocked, called after all messages received while locked have been processed.
extern boost::signals2::signal<void ()> NotifySecMsgWalletUnlocked;


class SecMsgBucket;
class SecMsgAddress;
class SecMsgOptions;

extern std::map<int64_t, SecMsgBucket>  smsgBuckets;
extern std::vector<SecMsgAddress>       smsgAddresses;
extern SecMsgOptions                    smsgOptions;

extern CCriticalSection cs_smsg;            // all except inbox and outbox
extern CCriticalSection cs_smsgDB;



#pragma pack(push, 1)
class SecureMessage
{
public:
    SecureMessage()
    {
        nPayload = 0;
        pPayload = nullptr;
    }
    
    // Modern C++ Migration: Vector handles memory automatically
    ~SecureMessage() = default;
    
    unsigned char   hash[4];
    unsigned char   version[2];
    unsigned char   flags;
    int64_t         timestamp;
    unsigned char   iv[16];
    unsigned char   cpkR[33];
    unsigned char   mac[32];
    unsigned char   nonse[4];
    uint32_t        nPayload;
    // Modern C++ Migration: Safe memory management
    std::vector<unsigned char> vchPayload;  // Modern container
    unsigned char*  pPayload;               // Legacy compatibility pointer
        
};
#pragma pack(pop)


class MessageData
{
// -- Decrypted SecureMessage data
public:
    int64_t                     timestamp;
    std::string                 sToAddress;
    std::string                 sFromAddress;
    std::vector<unsigned char>  vchMessage;         // null terminated plaintext
};


class SecMsgToken
{
public:
    SecMsgToken(int64_t ts, unsigned char* p, int np, long int o)
    {
        timestamp = ts;
        
        if (np < 8) // payload will always be > 8, just make sure
            memset(sample, 0, 8);
        else
            memcpy(sample, p, 8);
        offset = o;
    };
    
    SecMsgToken() {};
    
    ~SecMsgToken() {};
    
    bool operator <(const SecMsgToken& y) const
    {
        // pack and memcmp from timesent?
        if (timestamp == y.timestamp)
            return memcmp(sample, y.sample, 8) < 0;
        return timestamp < y.timestamp;
    }
    
    int64_t                     timestamp;    // doesn't need to be full 64 bytes?
    unsigned char               sample[8];    // first 8 bytes of payload - a hash
    int64_t                     offset;       // offset
    
};


class SecMsgBucket
{
public:
    SecMsgBucket()
    {
        timeChanged     = 0;
        hash            = 0;
        nLockCount      = 0;
        nLockPeerId     = 0;
    };
    ~SecMsgBucket() {};
    
    void hashBucket();
    
    int64_t                     timeChanged;
    uint32_t                    hash;           // token set should get ordered the same on each node
    uint32_t                    nLockCount;     // set when smsgWant first sent, unset at end of smsgMsg, ticks down in ThreadSecureMsg()
    uint32_t                    nLockPeerId;    // id of peer that bucket is locked for
    std::set<SecMsgToken>       setTokens;
    
};


// -- get at the data
class CBitcoinAddress_B : public CBitcoinAddress
{
public:
    unsigned char getVersion()
    {
        return nVersion;
    }
};

class CKeyID_B : public CKeyID
{
public:
    unsigned int* GetPPN()
    {
        return pn;
    }
};


class SecMsgAddress
{
public:
    SecMsgAddress() {};
    SecMsgAddress(std::string sAddr, bool receiveOn, bool receiveAnon)
    {
        sAddress            = sAddr;
        fReceiveEnabled     = receiveOn;
        fReceiveAnon        = receiveAnon;
    };
    
    std::string     sAddress;
    bool            fReceiveEnabled;
    bool            fReceiveAnon;
    
    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->sAddress);
        READWRITE(this->fReceiveEnabled);
        READWRITE(this->fReceiveAnon);
    );
};

class SecMsgOptions
{
public:
    SecMsgOptions()
    {
        // -- default options
        fNewAddressRecv = true;
        fNewAddressAnon = true;
    }
    
    bool fNewAddressRecv;
    bool fNewAddressAnon;
};


class SecMsgCrypter
{
private:
    unsigned char chKey[32];
    unsigned char chIV[16];
    bool fKeySet;
public:
    
    SecMsgCrypter()
    {
        // Try to keep the key data out of swap (and be a bit over-careful to keep the IV that we don't even use out of swap)
        // Note that this does nothing about suspend-to-disk (which will put all our key data on disk)
        // Note as well that at no point in this program is any attempt made to prevent stealing of keys by reading the memory of the running process.
        LockedPageManager::instance.LockRange(&chKey[0], sizeof chKey);
        LockedPageManager::instance.LockRange(&chIV[0], sizeof chIV);
        fKeySet = false;
    }
    
    ~SecMsgCrypter()
    {
        // clean key
        memset(&chKey, 0, sizeof chKey);
        memset(&chIV, 0, sizeof chIV);
        fKeySet = false;
        
        LockedPageManager::instance.UnlockRange(&chKey[0], sizeof chKey);
        LockedPageManager::instance.UnlockRange(&chIV[0], sizeof chIV);
    }
    
    bool SetKey(const std::vector<unsigned char>& vchNewKey, unsigned char* chNewIV);
    bool SetKey(const unsigned char* chNewKey, unsigned char* chNewIV);
    bool Encrypt(unsigned char* chPlaintext, uint32_t nPlain, std::vector<unsigned char> &vchCiphertext);
    bool Decrypt(unsigned char* chCiphertext, uint32_t nCipher, std::vector<unsigned char>& vchPlaintext);
};


class SecMsgStored
{
public:
    int64_t                         timeReceived;
    char                            status;         // read etc
    uint16_t                        folderId;
    std::string                     sAddrTo;        // when in owned addr, when sent remote addr
    std::string                     sAddrOutbox;    // owned address this copy was encrypted with
    std::vector<unsigned char>      vchMessage;     // message header + encryped payload
    
    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->timeReceived);
        READWRITE(this->status);
        READWRITE(this->folderId);
        READWRITE(this->sAddrTo);
        READWRITE(this->sAddrOutbox);
        READWRITE(this->vchMessage);
    );
};

class SecMsgDB
{
public:
    SecMsgDB() = default;
    
    ~SecMsgDB() = default; // Modern C++ Migration: Smart pointer handles cleanup automatically
    
    bool Open(const char* pszMode="r+");
    
    bool ScanBatch(const CDataStream& key, std::string* value, bool* deleted) const;
    
    bool TxnBegin();
    bool TxnCommit();
    bool TxnAbort();
    
    bool ReadPK(CKeyID& addr, CPubKey& pubkey);
    bool WritePK(CKeyID& addr, CPubKey& pubkey);
    bool ExistsPK(CKeyID& addr);
    
    bool NextSmesg(leveldb::Iterator* it, std::string& prefix, unsigned char* vchKey, SecMsgStored& smsgStored);
    bool NextSmesgKey(leveldb::Iterator* it, std::string& prefix, unsigned char* vchKey);
    bool ReadSmesg(unsigned char* chKey, SecMsgStored& smsgStored);
    bool WriteSmesg(unsigned char* chKey, SecMsgStored& smsgStored);
    bool ExistsSmesg(unsigned char* chKey);
    bool EraseSmesg(unsigned char* chKey);
    
    leveldb::DB *pdb;       // points to the global instance
    // Modern C++ Migration: Smart pointer for automatic memory management
    std::unique_ptr<leveldb::WriteBatch> activeBatch;
    
};

std::string getTimeString(int64_t timestamp, char *buffer, size_t nBuffer);
std::string fsReadable(uint64_t nBytes);


int SecureMsgBuildBucketSet();
int SecureMsgAddWalletAddresses();

int SecureMsgReadIni();
int SecureMsgWriteIni();

bool SecureMsgStart(bool fDontStart, bool fScanChain);
bool SecureMsgShutdown();

bool SecureMsgEnable();
bool SecureMsgDisable();

bool SecureMsgReceiveData(CNode* pfrom, std::string strCommand, CDataStream& vRecv);
bool SecureMsgSendData(CNode* pto, bool fSendTrickle);


bool SecureMsgScanBlock(CBlock& block);
bool ScanChainForPublicKeys(CBlockIndex* pindexStart);
bool SecureMsgScanBlockChain();
bool SecureMsgScanBuckets();


int SecureMsgWalletUnlocked();
int SecureMsgWalletKeyChanged(std::string sAddress, std::string sLabel, ChangeType mode);

int SecureMsgScanMessage(unsigned char *pHeader, unsigned char *pPayload, uint32_t nPayload, bool reportToGui);

int SecureMsgGetStoredKey(CKeyID& ckid, CPubKey& cpkOut);
int SecureMsgGetLocalKey(CKeyID& ckid, CPubKey& cpkOut);
int SecureMsgGetLocalPublicKey(std::string& strAddress, std::string& strPublicKey);

int SecureMsgAddAddress(std::string& address, std::string& publicKey);

int SecureMsgRetrieve(SecMsgToken &token, std::vector<unsigned char>& vchData);

int SecureMsgReceive(CNode* pfrom, std::vector<unsigned char>& vchData);

int SecureMsgStoreUnscanned(unsigned char *pHeader, unsigned char *pPayload, uint32_t nPayload);
int SecureMsgStore(unsigned char *pHeader, unsigned char *pPayload, uint32_t nPayload, bool fUpdateBucket);
int SecureMsgStore(SecureMessage& smsg, bool fUpdateBucket);



int SecureMsgSend(std::string& addressFrom, std::string& addressTo, std::string& message, std::string& sError);

int SecureMsgValidate(unsigned char *pHeader, unsigned char *pPayload, uint32_t nPayload);
int SecureMsgSetHash(unsigned char *pHeader, unsigned char *pPayload, uint32_t nPayload);

int SecureMsgEncrypt(SecureMessage& smsg, std::string& addressFrom, std::string& addressTo, std::string& message);

int SecureMsgDecrypt(bool fTestOnly, std::string& address, unsigned char *pHeader, unsigned char *pPayload, uint32_t nPayload, MessageData& msg);
int SecureMsgDecrypt(bool fTestOnly, std::string& address, SecureMessage& smsg, MessageData& msg);

#endif // VERGE_SMESSAGE_H