// Copyright (c) 2014 The ShadowCoin developers
// Copyright (c) 2016 The VERGE developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_STEALTH_H
#define VERGE_STEALTH_H

#include <stdlib.h> 
#include <stdio.h> 
#include <vector>
#include <inttypes.h>

#include "util.h"
#include "serialize.h"
#include "key.h"
#include "hash.h"
#include "types.h"

const uint32_t MAX_STEALTH_NARRATION_SIZE = 48;

typedef uint32_t stealth_bitfield;

struct stealth_prefix
{
    uint8_t number_bits;
    stealth_bitfield bitfield;
};

const uint256 MAX_SECRET("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364140");
const uint256 MIN_SECRET(16000); // increase? min valid key is 1

class CStealthAddress
{
public:
    CStealthAddress()
    {
        options = 0;
    };
    
    uint8_t options;
    ec_point scan_pubkey;
    ec_point spend_pubkey;
    //std::vector<ec_point> spend_pubkeys;
    size_t number_signatures;
    stealth_prefix prefix;
    
    mutable std::string label;
    data_chunk scan_secret;
    data_chunk spend_secret;
    
    bool SetEncoded(const std::string& encodedAddress);
    std::string Encoded() const;
    
    int SetScanPubKey(CPubKey pk);
    
    
    bool operator <(const CStealthAddress& y) const
    {
        return memcmp(&scan_pubkey[0], &y.scan_pubkey[0], EC_COMPRESSED_SIZE) < 0;
    };
    
    bool operator ==(const CStealthAddress& y) const
    {
        return memcmp(&scan_pubkey[0], &y.scan_pubkey[0], EC_COMPRESSED_SIZE) == 0;
    };
    
    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->options);
        READWRITE(this->scan_pubkey);
        READWRITE(this->spend_pubkey);
        READWRITE(this->label);
        
        READWRITE(this->scan_secret);
        READWRITE(this->spend_secret);
    );
    
};

int GenerateRandomSecret(ec_secret& out);

int SecretToPublicKey(const ec_secret& secret, ec_point& out);

int StealthSecret(ec_secret& secret, ec_point& pubkey, const ec_point& pkSpend, ec_secret& sharedSOut, ec_point& pkOut);
int StealthSecretSpend(ec_secret& scanSecret, ec_point& ephemPubkey, ec_secret& spendSecret, ec_secret& secretOut);
int StealthSharedToSecretSpend(const ec_secret& sharedS, const ec_secret& spendSecret, ec_secret& secretOut);

int StealthSharedToPublicKey(const ec_point& pkSpend, const ec_secret &sharedS, ec_point &pkOut);

bool IsStealthAddress(const std::string& encodedAddress);


#endif  // VERGE_STEALTH_H