// Copyright (c) 2014 The ShadowCoin developers
// Copyright (c) 2016 The VERGE developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "anonaddress.h"
#include "base58.h"
#include "state.h"

#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>


bool CStealthAddress::SetEncoded(const std::string& encodedAddress)
{
    data_chunk raw;
    
    if (!DecodeBase58(encodedAddress, raw))
    {
        if (fDebug)
            LogPrintf("CStealthAddress::SetEncoded DecodeBase58 failed.\n");
        return false;
    };
    
    if (!VerifyChecksum(raw))
    {
        if (fDebug)
            LogPrintf("CStealthAddress::SetEncoded verify_checksum failed.\n");
        return false;
    };
    
    if (raw.size() < 1 + 1 + 33 + 1 + 33 + 1 + 1 + 4)
    {
        if (fDebug)
            LogPrintf("CStealthAddress::SetEncoded() too few bytes provided.\n");
        return false;
    };
    
    
    uint8_t* p = &raw[0];
    uint8_t version = *p++;
    
    if (version != Params().Base58Prefix(CChainParams::STEALTH_ADDRESS)[0])
    {
        LogPrintf("CStealthAddress::SetEncoded version mismatch 0x%x != 0x%x.\n", version, Params().Base58Prefix(CChainParams::STEALTH_ADDRESS)[0]);
        return false;
    };
    
    options = *p++;
    
    scan_pubkey.resize(33);
    memcpy(&scan_pubkey[0], p, 33);
    p += 33;
    //uint8_t spend_pubkeys = *p++;
    p++;
    
    spend_pubkey.resize(33);
    memcpy(&spend_pubkey[0], p, 33);
    
    return true;
};

std::string CStealthAddress::Encoded() const
{
    // https://wiki.unsystem.net/index.php/DarkWallet/Stealth#Address_format
    // [version] [options] [scan_key] [N] ... [Nsigs] [prefix_length] ...
    
    data_chunk raw;
    raw = Params().Base58Prefix(CChainParams::STEALTH_ADDRESS);
    
    raw.push_back(options);
    
    raw.insert(raw.end(), scan_pubkey.begin(), scan_pubkey.end());
    raw.push_back(1); // number of spend pubkeys
    raw.insert(raw.end(), spend_pubkey.begin(), spend_pubkey.end());
    raw.push_back(0); // number of signatures
    raw.push_back(0); // ?
    
    AppendChecksum(raw);
    
    return EncodeBase58(raw);
};

int CStealthAddress::SetScanPubKey(CPubKey pk)
{
    scan_pubkey.resize(pk.size());
    memcpy(&scan_pubkey[0], pk.begin(), pk.size());
    return 0;
};


int GenerateRandomSecret(ec_secret& out)
{
    RandAddSeedPerfmon();
    
    uint256 test;
    
    int i;
    // -- check max, try max 32 times
    for (i = 0; i < 32; ++i)
    {
        if (1 != RAND_bytes((uint8_t*) test.begin(), 32))
            return errorN(1, "%s: RAND_bytes ERR_get_error %u.", __func__, ERR_get_error());
        if (test > MIN_SECRET && test < MAX_SECRET)
        {
            memcpy(&out.e[0], test.begin(), 32);
            break;
        };
    };
    
    if (i > 31)
        return errorN(1, "%s: Failed to generate a valid key.", __func__);
    
    return 0;
};

int SecretToPublicKey(const ec_secret& secret, ec_point& out)
{
    // -- public key = private * G
    int rv = 0;
    
    EC_GROUP* ecgrp = EC_GROUP_new_by_curve_name(NID_secp256k1);
    
    if (!ecgrp)
        return errorN(1, "%s: EC_GROUP_new_by_curve_name failed.", __func__);

    BIGNUM* bnIn = BN_bin2bn(&secret.e[0], EC_SECRET_SIZE, BN_new());
    if (!bnIn)
    {
        EC_GROUP_free(ecgrp);
        return errorN(1, "%s: BN_bin2bn failed.", __func__);
    };
    
    EC_POINT* pub = EC_POINT_new(ecgrp);
    
    
    EC_POINT_mul(ecgrp, pub, bnIn, NULL, NULL, NULL);
    
    BIGNUM* bnOut = EC_POINT_point2bn(ecgrp, pub, POINT_CONVERSION_COMPRESSED, BN_new(), NULL);
    if (!bnOut)
    {
        LogPrintf("%s: point2bn failed.\n", __func__);
        rv = 1;
    } else
    {
        out.resize(EC_COMPRESSED_SIZE);
        if (BN_num_bytes(bnOut) != (int) EC_COMPRESSED_SIZE
            || BN_bn2bin(bnOut, &out[0]) != (int) EC_COMPRESSED_SIZE)
        {
            LogPrintf("%s: bnOut incorrect length.\n", __func__);
            rv = 1;
        };
        
        BN_free(bnOut);
    };
    
    EC_POINT_free(pub);
    BN_free(bnIn);
    EC_GROUP_free(ecgrp);
    
    return rv;
};


int StealthSecret(ec_secret& secret, ec_point& pubkey, const ec_point& pkSpend, ec_secret& sharedSOut, ec_point& pkOut)
{
    /*
    
    send:
        secret = ephem_secret, pubkey = scan_pubkey
    
    receive:
        secret = scan_secret, pubkey = ephem_pubkey
        c = H(dP)
    
    Q = public scan key (EC point, 33 bytes)
    d = private scan key (integer, 32 bytes)
    R = public spend key
    f = private spend key

    Q = dG
    R = fG
    
    Sender (has Q and R, not d or f):
    
    P = eG

    c = H(eQ) = H(dP)
    R' = R + cG
    
    
    Recipient gets R' and P
    
    test 0 and infinity?
    */
    
    int rv = 0;
    std::vector<uint8_t> vchOutQ;
    
    BN_CTX* bnCtx   = NULL;
    BIGNUM* bnEphem = NULL;
    BIGNUM* bnQ     = NULL;
    EC_POINT* Q     = NULL;
    BIGNUM* bnOutQ  = NULL;
    BIGNUM* bnc     = NULL;
    EC_POINT* C     = NULL;
    BIGNUM* bnR     = NULL;
    EC_POINT* R     = NULL;
    EC_POINT* Rout  = NULL;
    BIGNUM* bnOutR  = NULL;
    
    EC_GROUP* ecgrp = EC_GROUP_new_by_curve_name(NID_secp256k1);
    
    if (!ecgrp)
        return errorN(1, "%s: EC_GROUP_new_by_curve_name failed.", __func__);
    
    if (!(bnCtx = BN_CTX_new()))
    {
        LogPrintf("%s: BN_CTX_new failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    if (!(bnEphem = BN_bin2bn(&secret.e[0], EC_SECRET_SIZE, BN_new())))
    {
        LogPrintf("%s: bnEphem BN_bin2bn failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    if (!(bnQ = BN_bin2bn(&pubkey[0], pubkey.size(), BN_new())))
    {
        LogPrintf("%s: bnEphem bnQ failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    if (!(Q = EC_POINT_bn2point(ecgrp, bnQ, NULL, bnCtx)))
    {
        LogPrintf("%s: Q EC_POINT_bn2point failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    // -- eQ
    // EC_POINT_mul(const EC_GROUP *group, EC_POINT *r, const BIGNUM *n, const EC_POINT *q, const BIGNUM *m, BN_CTX *ctx);
    // EC_POINT_mul calculates the value generator * n + q * m and stores the result in r. The value n may be NULL in which case the result is just q * m. 
    if (!EC_POINT_mul(ecgrp, Q, NULL, Q, bnEphem, bnCtx))
    {
        LogPrintf("%s: eQ EC_POINT_mul failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    if (!(bnOutQ = EC_POINT_point2bn(ecgrp, Q, POINT_CONVERSION_COMPRESSED, BN_new(), bnCtx)))
    {
        LogPrintf("%s: Q EC_POINT_bn2point failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    
    vchOutQ.resize(EC_COMPRESSED_SIZE);
    if (BN_num_bytes(bnOutQ) != (int) EC_COMPRESSED_SIZE
        || BN_bn2bin(bnOutQ, &vchOutQ[0]) != (int) EC_COMPRESSED_SIZE)
    {
        LogPrintf("%s: bnOutQ incorrect length.\n", __func__);
        rv = 1;
        goto End;
    };
    
    SHA256(&vchOutQ[0], vchOutQ.size(), &sharedSOut.e[0]);
    
    if (!(bnc = BN_bin2bn(&sharedSOut.e[0], EC_SECRET_SIZE, BN_new())))
    {
        LogPrintf("%s: BN_bin2bn failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    // -- cG
    if (!(C = EC_POINT_new(ecgrp)))
    {
        LogPrintf("%s: C EC_POINT_new failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    if (!EC_POINT_mul(ecgrp, C, bnc, NULL, NULL, bnCtx))
    {
        LogPrintf("%s: C EC_POINT_mul failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    if (!(bnR = BN_bin2bn(&pkSpend[0], pkSpend.size(), BN_new())))
    {
        LogPrintf("%s: bnR BN_bin2bn failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    
    if (!(R = EC_POINT_bn2point(ecgrp, bnR, NULL, bnCtx)))
    {
        LogPrintf("%s: R EC_POINT_bn2point failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    if (!EC_POINT_mul(ecgrp, C, bnc, NULL, NULL, bnCtx))
    {
        LogPrintf("%s: C EC_POINT_mul failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    if (!(Rout = EC_POINT_new(ecgrp)))
    {
        LogPrintf("%s: Rout EC_POINT_new failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    if (!EC_POINT_add(ecgrp, Rout, R, C, bnCtx))
    {
        LogPrintf("%s: Rout EC_POINT_add failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    if (!(bnOutR = EC_POINT_point2bn(ecgrp, Rout, POINT_CONVERSION_COMPRESSED, BN_new(), bnCtx)))
    {
        LogPrintf("%s: Rout EC_POINT_bn2point failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    
    pkOut.resize(EC_COMPRESSED_SIZE);
    if (BN_num_bytes(bnOutR) != (int) EC_COMPRESSED_SIZE
        || BN_bn2bin(bnOutR, &pkOut[0]) != (int) EC_COMPRESSED_SIZE)
    {
        LogPrintf("%s: pkOut incorrect length.\n", __func__);
        rv = 1;
        goto End;
    };
    
    End:
    if (bnOutR)     BN_free(bnOutR);
    if (Rout)       EC_POINT_free(Rout);
    if (R)          EC_POINT_free(R);
    if (bnR)        BN_free(bnR);
    if (C)          EC_POINT_free(C);
    if (bnc)        BN_free(bnc);
    if (bnOutQ)     BN_free(bnOutQ);
    if (Q)          EC_POINT_free(Q);
    if (bnQ)        BN_free(bnQ);
    if (bnEphem)    BN_free(bnEphem);
    if (bnCtx)      BN_CTX_free(bnCtx);
    EC_GROUP_free(ecgrp);
    
    return rv;
};


int StealthSecretSpend(ec_secret& scanSecret, ec_point& ephemPubkey, ec_secret& spendSecret, ec_secret& secretOut)
{
    /*
    
    c  = H(dP)
    R' = R + cG     [without decrypting wallet]
       = (f + c)G   [after decryption of wallet]
         Remember: mod curve.order, pad with 0x00s where necessary?
    */
    
    int rv = 0;
    std::vector<uint8_t> vchOutP;
    
    BN_CTX* bnCtx           = NULL;
    BIGNUM* bnScanSecret    = NULL;
    BIGNUM* bnP             = NULL;
    EC_POINT* P             = NULL;
    BIGNUM* bnOutP          = NULL;
    BIGNUM* bnc             = NULL;
    BIGNUM* bnOrder         = NULL;
    BIGNUM* bnSpend         = NULL;
    
    EC_GROUP* ecgrp = EC_GROUP_new_by_curve_name(NID_secp256k1);
    
    if (!ecgrp)
        return errorN(1, "%s: EC_GROUP_new_by_curve_name failed.", __func__);
    
    if (!(bnCtx = BN_CTX_new()))
    {
        LogPrintf("%s: BN_CTX_new failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    if (!(bnScanSecret = BN_bin2bn(&scanSecret.e[0], EC_SECRET_SIZE, BN_new())))
    {
        LogPrintf("%s: bnScanSecret BN_bin2bn failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    if (!(bnP = BN_bin2bn(&ephemPubkey[0], ephemPubkey.size(), BN_new())))
    {
        LogPrintf("%s: bnP BN_bin2bn failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    if (!(P = EC_POINT_bn2point(ecgrp, bnP, NULL, bnCtx)))
    {
        LogPrintf("%s: P EC_POINT_bn2point failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    // -- dP
    if (!EC_POINT_mul(ecgrp, P, NULL, P, bnScanSecret, bnCtx))
    {
        LogPrintf("%s: dP EC_POINT_mul failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    if (!(bnOutP = EC_POINT_point2bn(ecgrp, P, POINT_CONVERSION_COMPRESSED, BN_new(), bnCtx)))
    {
        LogPrintf("%s: P EC_POINT_bn2point failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    
    vchOutP.resize(EC_COMPRESSED_SIZE);
    if (BN_num_bytes(bnOutP) != (int) EC_COMPRESSED_SIZE
        || BN_bn2bin(bnOutP, &vchOutP[0]) != (int) EC_COMPRESSED_SIZE)
    {
        LogPrintf("%s: bnOutP incorrect length.\n", __func__);
        rv = 1;
        goto End;
    };
    
    uint8_t hash1[32];
    SHA256(&vchOutP[0], vchOutP.size(), (uint8_t*)hash1);
    
    
    if (!(bnc = BN_bin2bn(&hash1[0], 32, BN_new())))
    {
        LogPrintf("%s: BN_bin2bn failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    if (!(bnOrder = BN_new())
        || !EC_GROUP_get_order(ecgrp, bnOrder, bnCtx))
    {
        LogPrintf("%s: EC_GROUP_get_order failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    if (!(bnSpend = BN_bin2bn(&spendSecret.e[0], EC_SECRET_SIZE, BN_new())))
    {
        LogPrintf("%s: bnSpend BN_bin2bn failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    //if (!BN_add(r, a, b)) return 0;
    //return BN_nnmod(r, r, m, ctx);
    if (!BN_mod_add(bnSpend, bnSpend, bnc, bnOrder, bnCtx))
    {
        LogPrintf("%s: bnSpend BN_mod_add failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    if (BN_is_zero(bnSpend)) // possible?
    {
        LogPrintf("%s: bnSpend is zero.\n", __func__);
        rv = 1;
        goto End;
    };
    
    int nBytes;
    memset(&secretOut.e[0], 0, EC_SECRET_SIZE);
    if ((nBytes = BN_num_bytes(bnSpend)) > (int)EC_SECRET_SIZE
        || BN_bn2bin(bnSpend, &secretOut.e[EC_SECRET_SIZE-nBytes]) != nBytes)
    {
        LogPrintf("%s: bnSpend incorrect length.\n", __func__);
        rv = 1;
        goto End;
    };
    
    End:
    if (bnSpend)        BN_free(bnSpend);
    if (bnOrder)        BN_free(bnOrder);
    if (bnc)            BN_free(bnc);
    if (bnOutP)         BN_free(bnOutP);
    if (P)              EC_POINT_free(P);
    if (bnP)            BN_free(bnP);
    if (bnScanSecret)   BN_free(bnScanSecret);
    if (bnCtx)          BN_CTX_free(bnCtx);
    EC_GROUP_free(ecgrp);
    
    return rv;
};


int StealthSharedToSecretSpend(const ec_secret& sharedS, const ec_secret& spendSecret, ec_secret& secretOut)
{
    int rv = 0;
    std::vector<uint8_t> vchOutP;
    
    BN_CTX* bnCtx           = NULL;
    BIGNUM* bnc             = NULL;
    BIGNUM* bnOrder         = NULL;
    BIGNUM* bnSpend         = NULL;
    
    EC_GROUP* ecgrp = EC_GROUP_new_by_curve_name(NID_secp256k1);
    
    if (!ecgrp)
        return errorN(1, "%s: EC_GROUP_new_by_curve_name failed.", __func__);
    
    if (!(bnCtx = BN_CTX_new()))
    {
        LogPrintf("%s: BN_CTX_new failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    if (!(bnc = BN_bin2bn(&sharedS.e[0], EC_SECRET_SIZE, BN_new())))
    {
        LogPrintf("%s: BN_bin2bn failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    if (!(bnOrder = BN_new())
        || !EC_GROUP_get_order(ecgrp, bnOrder, bnCtx))
    {
        LogPrintf("%s: EC_GROUP_get_order failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    if (!(bnSpend = BN_bin2bn(&spendSecret.e[0], EC_SECRET_SIZE, BN_new())))
    {
        LogPrintf("%s: bnSpend BN_bin2bn failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    //if (!BN_add(r, a, b)) return 0;
    //return BN_nnmod(r, r, m, ctx);
    if (!BN_mod_add(bnSpend, bnSpend, bnc, bnOrder, bnCtx))
    {
        LogPrintf("%s: bnSpend BN_mod_add failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    if (BN_is_zero(bnSpend)) // possible?
    {
        LogPrintf("%s: bnSpend is zero.\n", __func__);
        rv = 1;
        goto End;
    };
    
    int nBytes;
    memset(&secretOut.e[0], 0, EC_SECRET_SIZE);
    if ((nBytes = BN_num_bytes(bnSpend)) > (int)EC_SECRET_SIZE
        || BN_bn2bin(bnSpend, &secretOut.e[EC_SECRET_SIZE-nBytes]) != nBytes)
    {
        LogPrintf("%s: bnSpend incorrect length.\n", __func__);
        rv = 1;
        goto End;
    };
    
    End:
    if (bnSpend)        BN_free(bnSpend);
    if (bnOrder)        BN_free(bnOrder);
    if (bnc)            BN_free(bnc);
    if (bnCtx)          BN_CTX_free(bnCtx);
    EC_GROUP_free(ecgrp);
    
    return rv;
};

int StealthSharedToPublicKey(const ec_point& pkSpend, const ec_secret &sharedS, ec_point &pkOut)
{
    int rv = 0;
    std::vector<uint8_t> vchOutQ;
    
    BN_CTX *bnCtx   = NULL;
    BIGNUM *bnc     = NULL;
    EC_POINT *C     = NULL;
    BIGNUM *bnR     = NULL;
    EC_POINT *R     = NULL;
    EC_POINT *Rout  = NULL;
    BIGNUM *bnOutR  = NULL;
    
    EC_GROUP *ecgrp = EC_GROUP_new_by_curve_name(NID_secp256k1);
    
    if (!ecgrp)
        return errorN(1, "%s: EC_GROUP_new_by_curve_name failed.", __func__);
    
    if (!(bnCtx = BN_CTX_new()))
    {
        LogPrintf("%s: BN_CTX_new failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    if (!(bnc = BN_bin2bn(&sharedS.e[0], EC_SECRET_SIZE, BN_new())))
    {
        LogPrintf("%s: BN_bin2bn failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    // -- cG
    if (!(C = EC_POINT_new(ecgrp)))
    {
        LogPrintf("%s: C EC_POINT_new failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    if (!EC_POINT_mul(ecgrp, C, bnc, NULL, NULL, bnCtx))
    {
        LogPrintf("%s: C EC_POINT_mul failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    if (!(bnR = BN_bin2bn(&pkSpend[0], pkSpend.size(), BN_new())))
    {
        LogPrintf("%s: bnR BN_bin2bn failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    
    if (!(R = EC_POINT_bn2point(ecgrp, bnR, NULL, bnCtx)))
    {
        LogPrintf("%s: R EC_POINT_bn2point failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    if (!EC_POINT_mul(ecgrp, C, bnc, NULL, NULL, bnCtx))
    {
        LogPrintf("%s: C EC_POINT_mul failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    if (!(Rout = EC_POINT_new(ecgrp)))
    {
        LogPrintf("%s: Rout EC_POINT_new failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    if (!EC_POINT_add(ecgrp, Rout, R, C, bnCtx))
    {
        LogPrintf("%s: Rout EC_POINT_add failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    if (!(bnOutR = EC_POINT_point2bn(ecgrp, Rout, POINT_CONVERSION_COMPRESSED, BN_new(), bnCtx)))
    {
        LogPrintf("%s: Rout EC_POINT_bn2point failed.\n", __func__);
        rv = 1;
        goto End;
    };
    
    
    pkOut.resize(EC_COMPRESSED_SIZE);
    if (BN_num_bytes(bnOutR) != (int) EC_COMPRESSED_SIZE
        || BN_bn2bin(bnOutR, &pkOut[0]) != (int) EC_COMPRESSED_SIZE)
    {
        LogPrintf("%s: pkOut incorrect length.\n", __func__);
        rv = 1;
        goto End;
    };
    
    End:
    if (bnOutR)     BN_free(bnOutR);
    if (Rout)       EC_POINT_free(Rout);
    if (R)          EC_POINT_free(R);
    if (bnR)        BN_free(bnR);
    if (C)          EC_POINT_free(C);
    if (bnc)        BN_free(bnc);
    if (bnCtx)      BN_CTX_free(bnCtx);
    EC_GROUP_free(ecgrp);
    
    return rv;
};

bool IsStealthAddress(const std::string& encodedAddress)
{
    data_chunk raw;
    
    if (!DecodeBase58(encodedAddress, raw))
    {
        //LogPrintf("IsStealthAddress DecodeBase58 failed.\n");
        return false;
    };
    
    if (!VerifyChecksum(raw))
    {
        //LogPrintf("IsStealthAddress verify_checksum failed.\n");
        return false;
    };
    
    if (raw.size() < 1 + 1 + 33 + 1 + 33 + 1 + 1 + 4)
    {
        //LogPrintf("IsStealthAddress too few bytes provided.\n");
        return false;
    };
    
    
    uint8_t* p = &raw[0];
    uint8_t version = *p++;
    
    if (version != Params().Base58Prefix(CChainParams::STEALTH_ADDRESS)[0])
    {
        //LogPrintf("IsStealthAddress version mismatch 0x%x != 0x%x.\n", version, stealth_version_byte);
        return false;
    };
    
    return true;
};