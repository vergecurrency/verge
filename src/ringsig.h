// Copyright (c) 2014 The ShadowCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_RINGSIG_H
#define VERGE_RINGSIG_H

#include "stealth.h"
#include "logging.h"
#include "base58.h"
#include "chainparams.h"
#include "key.h"
#include "stealth.h"

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/err.h>
#include <openssl/obj_mac.h>
#include <openssl/rand.h>

class CPubKey;

enum ringsigType
{
    RING_SIG_1 = 1,
    RING_SIG_2,
};

const uint32_t MIN_ANON_OUT_SIZE = 1 + 1 + 1 + 33 + 1 + 33; // OP_RETURN ANON_TOKEN lenPk pkTo lenR R [lenEnarr enarr]
const uint32_t MIN_ANON_IN_SIZE = 2 + (33 + 32 + 32); // 2byte marker (cpubkey + sigc + sigr)
const uint32_t MAX_ANON_NARRATION_SIZE = 48;
const uint32_t MIN_RING_SIZE = 2;
const uint32_t MAX_RING_SIZE_OLD = 200;
const uint32_t MAX_RING_SIZE = 32; // already overkill

const int MIN_ANON_SPEND_DEPTH = 9;
const int ANON_TXN_VERSION = 1000;

// MAX_MONEY = 200000000000000000; most complex possible value can be represented by 36 outputs

class CRingSignature {

public:

    CRingSignature(){
        ecGrp = NULL;
        bnCtx = NULL;
        bnOrder = NULL;
    }

    bool Initialise();
    int Finalise();

    int SplitAmount(int64_t nValue, std::vector<int64_t> &vOut);

    int GetOldKeyImage(CPubKey &pubkey, ec_point &keyImage);

    int GenerateKeyImage(ec_point &publicKey, ec_secret secret, ec_point &keyImage);

    int GenerateRingSignature(data_chunk &keyImage, uint256 &txnHash, int nRingSize, int nSecretOffset, ec_secret secret, const uint8_t *pPubkeys, uint8_t *pSigc, uint8_t *pSigr);
    int VerifyRingSignature(data_chunk &keyImage, uint256 &txnHash, int nRingSize, const uint8_t *pPubkeys, const uint8_t *pSigc, const uint8_t *pSigr);

    int GenerateRingSignatureAB(data_chunk &keyImage, uint256 &txnHash, int nRingSize, int nSecretOffset, ec_secret secret, const uint8_t *pPubkeys, data_chunk &sigC, uint8_t *pSigS);
    int VerifyRingSignatureAB(data_chunk &keyImage, uint256 &txnHash, int nRingSize, const uint8_t *pPubkeys, const data_chunk &sigC, const uint8_t *pSigS);

private:
    EC_GROUP* ecGrp;
    BN_CTX* bnCtx;
    BIGNUM* bnOrder;

    int HashToEC(const uint8_t* p, uint32_t len, BIGNUM* bnTmp, EC_POINT* ptRet, bool fNew = false);
};

#endif  // VERGE_RINGSIG_H
