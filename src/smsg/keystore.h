// Copyright (c) 2018 The Particl Core developers
// Copyright (c) 2018 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_SMSG_KEYSTORE_H
#define VERGE_SMSG_KEYSTORE_H

#include <key.h>
#include <pubkey.h>
#include <sync.h>

namespace smsg {

enum SMSGKeyFlagTypes
{
    SMK_RECEIVE_ON       = (1 << 1),
    SMK_RECEIVE_ANON     = (1 << 2),
};

class SecMsgKey
{
public:
    int64_t nCreateTime = 0; // 0 means unknown
    uint32_t nFlags = 0; // SMSGKeyFlagTypes
    std::string sLabel;
    CKey key;
    CPubKey pubkey;
    std::string hdKeypath; //optional HD/bip32 keypath
    CKeyID hdMasterKeyID; //id of the HD masterkey used to derive this key


    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        CPrivKey privkey;
        if (!ser_action.ForRead() && key.IsValid()) {
            privkey = key.GetPrivKey();
        }

        READWRITE(nCreateTime);
        READWRITE(nFlags);
        READWRITE(sLabel);
        READWRITE(privkey);
        READWRITE(pubkey);
        READWRITE(hdKeypath);
        READWRITE(hdMasterKeyID);

        if (ser_action.ForRead()) {
            if (!privkey.empty()) {
                if (!key.Load(privkey, pubkey, true)) {
                    throw std::ios_base::failure("Failed to load secure messaging private key");
                }
            } else {
                key = CKey();
            }
        }
    };
};

class SecMsgKeyStore
{
protected:
    mutable CCriticalSection cs_KeyStore;

public:
    std::map<CKeyID, SecMsgKey> mapKeys;

    bool AddKey(const CKeyID &idk, SecMsgKey &key);
    bool HaveKey(const CKeyID &idk) const;
    bool EraseKey(const CKeyID &idk);
    bool GetPubKey(const CKeyID &idk, CPubKey &pk);
    size_t Count() const;

    bool Clear();
};

} // namespace smsg

#endif //VERGE_SMSG_KEYSTORE_H
