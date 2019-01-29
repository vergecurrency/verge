// Copyright (c) 2014 The ShadowCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "ringsig.h"


bool CRingSignature::Initialise()
{
    ecGrp = EC_GROUP_new_by_curve_name(NID_secp256k1);
    if (!ecGrp) {
        LogPrintf("initialiseRingSigs(): EC_GROUP_new_by_curve_name failed.");
        return false;
    }

    bnCtx = BN_CTX_new();
    if (!bnCtx) {
        LogPrintf("initialiseRingSigs(): BN_CTX_new failed.");
        return false;
    }

    BN_CTX_start(bnCtx);

    // get order and cofactor
    bnOrder = BN_new();
    if (!EC_GROUP_get_order(ecGrp, bnOrder, bnCtx)) {
        LogPrintf("initialiseRingSigs(): EC_GROUP_get_order failed.");
        return false;
    }

    BN_CTX_end(bnCtx);

    return true;
}

int CRingSignature::Finalise()
{
    BN_free(bnOrder);
    BN_CTX_free(bnCtx);
    EC_GROUP_clear_free(ecGrp);

    ecGrp = NULL;
    bnCtx = NULL;
    bnOrder = NULL;

    return 0;
}


int CRingSignature::SplitAmount(int64_t nValue, std::vector<int64_t>& vOut)
{
    // First make sure no output bigger than maxAnonOutput is created
    int64_t maxAnonOutput = 10000 * COIN;
    for (int numOfMaxAnonOutputs = nValue / maxAnonOutput; numOfMaxAnonOutputs > 0; numOfMaxAnonOutputs--) {
        vOut.push_back(1 * maxAnonOutput);
    }
    nValue = nValue % maxAnonOutput;

    // - split amounts into 1, 3, 4, 5
    int64_t nTest = 1;
    int i;

    while (nValue >= nTest) {
        i = (nValue / nTest) % 10;
        switch (i) {
        case 0:
            break;
        case 2:
            vOut.push_back(1 * nTest);
            vOut.push_back(1 * nTest);
            break;
        case 6:
            vOut.push_back(5 * nTest);
            vOut.push_back(1 * nTest);
            break;
        case 7:
            vOut.push_back(3 * nTest);
            vOut.push_back(4 * nTest);
            break;
        case 8:
            vOut.push_back(5 * nTest);
            vOut.push_back(3 * nTest);
            break;
        case 9:
            vOut.push_back(5 * nTest);
            vOut.push_back(4 * nTest);
            break;
        default:
            vOut.push_back(i * nTest);
        }
        nTest *= 10;
    }

    return 0;
}


int CRingSignature::GetOldKeyImage(CPubKey& publicKey, ec_point& keyImage)
{
    // - PublicKey * Hash(PublicKey)
    if (publicKey.size() != ec_compressed_size) {
        LogPrintf("%s: Invalid publicKey.", __func__);
        return 1;
    }

    int rv = 0;

    uint256 pkHash = publicKey.GetHash();

    BN_CTX_start(bnCtx);
    BIGNUM* bnTmp = BN_CTX_get(bnCtx);
    EC_POINT* ptPk = NULL;

    // Hash to BIGNUM
    if (!BN_bin2bn(pkHash.begin(), ec_secret_size, bnTmp)) {
        rv = 1;
        LogPrintf("%s: BN_bin2bn failed.", __func__);
        goto End;
    }

    // PublicKey point
    if (!(ptPk = EC_POINT_new(ecGrp))) {
        rv = 1;
        LogPrintf("%s: EC_POINT_new failed.", __func__);
        goto End;
    }

    if (!EC_POINT_oct2point(ecGrp, ptPk, publicKey.begin(), ec_compressed_size, bnCtx)) {
        rv = 1;
        LogPrintf("%s: EC_POINT_oct2point failed.", __func__);
        goto End;
    }

    // PublicKey * Hash(PublicKey)
    if (!EC_POINT_mul(ecGrp, ptPk, NULL, ptPk, bnTmp, bnCtx)) {
        rv = 1;
        LogPrintf("%s: EC_POINT_mul failed.", __func__);
        goto End;
    }

    try {
        keyImage.resize(ec_compressed_size);
    } catch (std::exception& e) {
        LogPrintf("%s: keyImage.resize threw: %s.\n", __func__, e.what());
        rv = 1;
        goto End;
    }

    // Point to BIGNUM to bin
    if (!(EC_POINT_point2bn(ecGrp, ptPk, POINT_CONVERSION_COMPRESSED, bnTmp, bnCtx)) || BN_num_bytes(bnTmp) != (int)ec_compressed_size || BN_bn2bin(bnTmp, &keyImage[0]) != (int)ec_compressed_size) {
        rv = 1;
        LogPrintf("%s: point -> keyImage failed.", __func__);
    }

End:
    EC_POINT_free(ptPk);
    BN_CTX_end(bnCtx);

    return 0;
}


int CRingSignature::GenerateKeyImage(ec_point& publicKey, ec_secret secret, ec_point& keyImage)
{
    // - keyImage = secret * hash(publicKey) * G

    if (publicKey.size() != ec_compressed_size) {
        LogPrintf("%s: Invalid publicKey.", __func__);
        return 1;
    }

    BN_CTX_start(bnCtx);
    int rv = 0;
    BIGNUM* bnTmp = BN_CTX_get(bnCtx);
    BIGNUM* bnSec = BN_CTX_get(bnCtx);
    EC_POINT* hG = NULL;

    if (!(hG = EC_POINT_new(ecGrp))) {
        rv = 1;
        LogPrintf("%s: EC_POINT_new failed.", __func__);
        goto End;
    }

    if (HashToEC(&publicKey[0], publicKey.size(), bnTmp, hG, true)) {
        rv = 1;
        LogPrintf("%s: HashToEC failed.", __func__);
        goto End;
    }

    if (!(BN_bin2bn(&secret.e[0], ec_secret_size, bnSec))) {
        rv = 1;
        LogPrintf("%s: BN_bin2bn failed.", __func__);
        goto End;
    }

    if (!EC_POINT_mul(ecGrp, hG, NULL, hG, bnSec, bnCtx)) {
        rv = 1;
        LogPrintf("%s: kimg EC_POINT_mul failed.", __func__);
        goto End;
    }

    try {
        keyImage.resize(ec_compressed_size);
    } catch (std::exception& e) {
        LogPrintf("%s: keyImage.resize threw: %s.\n", __func__, e.what());
        rv = 1;
        goto End;
    }

    if ((!(EC_POINT_point2bn(ecGrp, hG, POINT_CONVERSION_COMPRESSED, bnTmp, bnCtx)) || BN_num_bytes(bnTmp) != (int)ec_compressed_size || BN_bn2bin(bnTmp, &keyImage[0]) != (int)ec_compressed_size)) {
        rv = 1;
        LogPrintf("%s: point -> keyImage failed.", __func__);
        goto End;
    }

End:
    EC_POINT_free(hG);
    BN_CTX_end(bnCtx);

    return rv;
}


int CRingSignature::GenerateRingSignature(data_chunk& keyImage, uint256& txnHash, int nRingSize, int nSecretOffset, ec_secret secret, const uint8_t* pPubkeys, uint8_t* pSigc, uint8_t* pSigr)
{
    int rv = 0;
    int nBytes;

    BN_CTX_start(bnCtx);

    BIGNUM* bnKS = BN_CTX_get(bnCtx);
    BIGNUM* bnK1 = BN_CTX_get(bnCtx);
    BIGNUM* bnK2 = BN_CTX_get(bnCtx);
    BIGNUM* bnT = BN_CTX_get(bnCtx);
    BIGNUM* bnH = BN_CTX_get(bnCtx);
    BIGNUM* bnSum = BN_CTX_get(bnCtx);
    EC_POINT* ptT1 = NULL;
    EC_POINT* ptT2 = NULL;
    EC_POINT* ptT3 = NULL;
    EC_POINT* ptPk = NULL;
    EC_POINT* ptKi = NULL;
    EC_POINT* ptL = NULL;
    EC_POINT* ptR = NULL;

    uint8_t tempData[66]; // hold raw point data to hash
    uint256 commitHash;
    ec_secret scData1, scData2;

    CHashWriter ssCommitHash(SER_GETHASH, PROTOCOL_VERSION);

    ssCommitHash << txnHash;

    // zero signature
    memset(pSigc, 0, ec_secret_size * nRingSize);
    memset(pSigr, 0, ec_secret_size * nRingSize);


    // ks = random 256 bit int mod P
    if (GenerateRandomSecret(scData1)) {
        LogPrintf("%s: GenerateRandomSecret failed.", __func__);
        rv = 1;
        goto End;
    }

    if (!bnKS || !(BN_bin2bn(&scData1.e[0], ec_secret_size, bnKS))) {
        LogPrintf("%s: BN_bin2bn failed.\n", __func__);
        rv = 1;
        goto End;
    }

    // zero sum
    if (!bnSum || !(BN_zero(bnSum))) {
        LogPrintf("%s: BN_zero failed.\n", __func__);
        rv = 1;
        goto End;
    }

    if (!(ptT1 = EC_POINT_new(ecGrp)) || !(ptT2 = EC_POINT_new(ecGrp)) || !(ptT3 = EC_POINT_new(ecGrp)) || !(ptPk = EC_POINT_new(ecGrp)) || !(ptKi = EC_POINT_new(ecGrp)) || !(ptL = EC_POINT_new(ecGrp)) || !(ptR = EC_POINT_new(ecGrp))) {
        LogPrintf("%s: EC_POINT_new failed.\n", __func__);
        rv = 1;
        goto End;
    }

    // get keyimage as point
    if (!EC_POINT_oct2point(ecGrp, ptKi, &keyImage[0], ec_compressed_size, bnCtx)) {
        rv = 1;
        LogPrintf("%s: extract ptKi failed.", __func__);
        goto End;
    }

    for (int i = 0; i < nRingSize; ++i) {
        if (i == nSecretOffset) {
            // k = random 256 bit int mod P
            // L = k * G
            // R = k * HashToEC(PKi)

            if (!EC_POINT_mul(ecGrp, ptL, bnKS, NULL, NULL, bnCtx)) {
                LogPrintf("%s: EC_POINT_mul failed.\n", __func__);
                rv = 1;
                goto End;
            }

            if (HashToEC(&pPubkeys[i * ec_compressed_size], ec_compressed_size, bnT, ptT1) != 0) {
                LogPrintf("%s: HashToEC failed.\n", __func__);
                rv = 1;
                goto End;
            }

            if (!EC_POINT_mul(ecGrp, ptR, NULL, ptT1, bnKS, bnCtx)) {
                LogPrintf("%s: EC_POINT_mul failed.\n", __func__);
                rv = 1;
                goto End;
            }

        } else {
            // k1 = random 256 bit int mod P
            // k2 = random 256 bit int mod P
            // Li = k1 * Pi + k2 * G
            // Ri = k1 * I + k2 * Hp(Pi)
            // ci = k1
            // ri = k2

            if (GenerateRandomSecret(scData1) != 0 || !bnK1 || !(BN_bin2bn(&scData1.e[0], ec_secret_size, bnK1)) || GenerateRandomSecret(scData2) != 0 || !bnK2 || !(BN_bin2bn(&scData2.e[0], ec_secret_size, bnK2))) {
                LogPrintf("%s: k1 and k2 failed.\n", __func__);
                rv = 1;
                goto End;
            }

            // get Pk i as point
            if (!(bnT = BN_bin2bn(&pPubkeys[i * ec_compressed_size], ec_compressed_size, bnT)) || !(ptPk) || !(ptPk = EC_POINT_bn2point(ecGrp, bnT, ptPk, bnCtx))) {
                LogPrintf("%s: extract ptPk failed.\n", __func__);
                rv = 1;
                goto End;
            }

            // ptT1 = k1 * Pi
            if (!EC_POINT_mul(ecGrp, ptT1, NULL, ptPk, bnK1, bnCtx)) {
                LogPrintf("%s: EC_POINT_mul failed.\n", __func__);
                rv = 1;
                goto End;
            }

            // ptT2 = k2 * G
            if (!EC_POINT_mul(ecGrp, ptT2, bnK2, NULL, NULL, bnCtx)) {
                LogPrintf("%s: EC_POINT_mul failed.\n", __func__);
                rv = 1;
                goto End;
            }

            // ptL = ptT1 + ptT2
            if (!EC_POINT_add(ecGrp, ptL, ptT1, ptT2, bnCtx)) {
                LogPrintf("%s: EC_POINT_add failed.\n", __func__);
                rv = 1;
                goto End;
            }

            // ptT3 = Hp(Pi)
            if (HashToEC(&pPubkeys[i * ec_compressed_size], ec_compressed_size, bnT, ptT3) != 0) {
                LogPrintf("%s: HashToEC failed.\n", __func__);
                rv = 1;
                goto End;
            }

            // ptT1 = k1 * I
            if (!EC_POINT_mul(ecGrp, ptT1, NULL, ptKi, bnK1, bnCtx)) {
                LogPrintf("%s: EC_POINT_mul failed.\n", __func__);
                rv = 1;
                goto End;
            }

            // ptT2 = k2 * ptT3
            if (!EC_POINT_mul(ecGrp, ptT2, NULL, ptT3, bnK2, bnCtx)) {
                LogPrintf("%s: EC_POINT_mul failed.\n", __func__);
                rv = 1;
                goto End;
            }

            // ptR = ptT1 + ptT2
            if (!EC_POINT_add(ecGrp, ptR, ptT1, ptT2, bnCtx)) {
                LogPrintf("%s: EC_POINT_add failed.\n", __func__);
                rv = 1;
                goto End;
            }

            memcpy(&pSigc[i * ec_secret_size], &scData1.e[0], ec_secret_size);
            memcpy(&pSigr[i * ec_secret_size], &scData2.e[0], ec_secret_size);

            // sum = (sum + sigc) % N , sigc == bnK1
            if (!BN_mod_add(bnSum, bnSum, bnK1, bnOrder, bnCtx)) {
                LogPrintf("%s: BN_mod_add failed.\n", __func__);
                rv = 1;
                goto End;
            }
        }

        // -- add ptL and ptR to hash
        if (!(EC_POINT_point2oct(ecGrp, ptL, POINT_CONVERSION_COMPRESSED, &tempData[0], 33, bnCtx) == (int)ec_compressed_size) || !(EC_POINT_point2oct(ecGrp, ptR, POINT_CONVERSION_COMPRESSED, &tempData[33], 33, bnCtx) == (int)ec_compressed_size)) {
            LogPrintf("%s: extract ptL and ptR failed.\n", __func__);
            rv = 1;
            goto End;
        }

        ssCommitHash.write((const char*)&tempData[0], 66);
    }

    commitHash = ssCommitHash.GetHash();

    if (!(bnH) || !(bnH = BN_bin2bn(commitHash.begin(), ec_secret_size, bnH))) {
        LogPrintf("%s: commitHash -> bnH failed.\n", __func__);
        rv = 1;
        goto End;
    }


    if (!BN_mod(bnH, bnH, bnOrder, bnCtx)) // this is necessary
    {
        LogPrintf("%s: BN_mod failed.\n", __func__);
        rv = 1;
        goto End;
    }

    // sigc[nSecretOffset] = (bnH - bnSum) % N
    if (!BN_mod_sub(bnT, bnH, bnSum, bnOrder, bnCtx)) {
        LogPrintf("%s: BN_mod_sub failed.\n", __func__);
        rv = 1;
        goto End;
    }

    if ((nBytes = BN_num_bytes(bnT)) > (int)ec_secret_size || BN_bn2bin(bnT, &pSigc[nSecretOffset * ec_secret_size + (ec_secret_size - nBytes)]) != nBytes) {
        LogPrintf("%s: bnT -> pSigc failed.\n", __func__);
        rv = 1;
        goto End;
    }

    // sigr[nSecretOffset] = (bnKS - sigc[nSecretOffset] * bnSecret) % N
    // reuse bnH for bnSecret
    if (!bnH || !(BN_bin2bn(&secret.e[0], ec_secret_size, bnH))) {
        LogPrintf("%s: BN_bin2bn failed.\n", __func__);
        rv = 1;
        goto End;
    }

    // bnT = sigc[nSecretOffset] * bnSecret , TODO: mod N ?
    if (!BN_mul(bnT, bnT, bnH, bnCtx)) {
        LogPrintf("%s: BN_mul failed.\n", __func__);
        rv = 1;
        goto End;
    }

    if (!BN_mod_sub(bnT, bnKS, bnT, bnOrder, bnCtx)) {
        LogPrintf("%s: BN_mod_sub failed.\n", __func__);
        rv = 1;
        goto End;
    }

    if ((nBytes = BN_num_bytes(bnT)) > (int)ec_secret_size || BN_bn2bin(bnT, &pSigr[nSecretOffset * ec_secret_size + (ec_secret_size - nBytes)]) != nBytes) {
        LogPrintf("%s: bnT -> pSigr failed.\n", __func__);
        rv = 1;
        goto End;
    }

End:
    EC_POINT_free(ptT1);
    EC_POINT_free(ptT2);
    EC_POINT_free(ptT3);
    EC_POINT_free(ptPk);
    EC_POINT_free(ptKi);
    EC_POINT_free(ptL);
    EC_POINT_free(ptR);

    BN_CTX_end(bnCtx);

    return rv;
}

int CRingSignature::VerifyRingSignature(data_chunk& keyImage, uint256& txnHash, int nRingSize, const uint8_t* pPubkeys, const uint8_t* pSigc, const uint8_t* pSigr)
{
    int rv = 0;

    BN_CTX_start(bnCtx);

    BIGNUM* bnT = BN_CTX_get(bnCtx);
    BIGNUM* bnH = BN_CTX_get(bnCtx);
    BIGNUM* bnC = BN_CTX_get(bnCtx);
    BIGNUM* bnR = BN_CTX_get(bnCtx);
    BIGNUM* bnSum = BN_CTX_get(bnCtx);
    EC_POINT* ptT1 = NULL;
    EC_POINT* ptT2 = NULL;
    EC_POINT* ptT3 = NULL;
    EC_POINT* ptPk = NULL;
    EC_POINT* ptKi = NULL;
    EC_POINT* ptL = NULL;
    EC_POINT* ptR = NULL;

    uint8_t tempData[66]; // hold raw point data to hash
    uint256 commitHash;
    CHashWriter ssCommitHash(SER_GETHASH, PROTOCOL_VERSION);

    ssCommitHash << txnHash;

    // zero sum
    if (!bnSum || !(BN_zero(bnSum))) {
        LogPrintf("%s: BN_zero failed.\n", __func__);
        rv = 1;
        goto End;
    }

    if (!(ptT1 = EC_POINT_new(ecGrp)) || !(ptT2 = EC_POINT_new(ecGrp)) || !(ptT3 = EC_POINT_new(ecGrp)) || !(ptPk = EC_POINT_new(ecGrp)) || !(ptKi = EC_POINT_new(ecGrp)) || !(ptL = EC_POINT_new(ecGrp)) || !(ptR = EC_POINT_new(ecGrp))) {
        LogPrintf("%s: EC_POINT_new failed.\n", __func__);
        rv = 1;
        goto End;
    }

    // get keyimage as point
    if (!EC_POINT_oct2point(ecGrp, ptKi, &keyImage[0], ec_compressed_size, bnCtx)) {
        LogPrintf("%s: extract ptKi failed.", __func__);
        rv = 1;
        goto End;
    }

    for (int i = 0; i < nRingSize; ++i) {
        // Li = ci * Pi + ri * G
        // Ri = ci * I + ri * Hp(Pi)

        if (!bnC || !(bnC = BN_bin2bn(&pSigc[i * ec_secret_size], ec_secret_size, bnC)) || !bnR || !(bnR = BN_bin2bn(&pSigr[i * ec_secret_size], ec_secret_size, bnR))) {
            LogPrintf("%s: extract bnC and bnR failed.\n", __func__);
            rv = 1;
            goto End;
        }

        // get Pk i as point
        if (!(bnT = BN_bin2bn(&pPubkeys[i * ec_compressed_size], ec_compressed_size, bnT)) || !(ptPk) || !(ptPk = EC_POINT_bn2point(ecGrp, bnT, ptPk, bnCtx))) {
            LogPrintf("%s: extract ptPk failed.\n", __func__);
            rv = 1;
            goto End;
        }

        // ptT1 = ci * Pi
        if (!EC_POINT_mul(ecGrp, ptT1, NULL, ptPk, bnC, bnCtx)) {
            LogPrintf("%s: EC_POINT_mul failed.\n", __func__);
            rv = 1;
            goto End;
        }

        // ptT2 = ri * G
        if (!EC_POINT_mul(ecGrp, ptT2, bnR, NULL, NULL, bnCtx)) {
            LogPrintf("%s: EC_POINT_mul failed.\n", __func__);
            rv = 1;
            goto End;
        }

        // ptL = ptT1 + ptT2
        if (!EC_POINT_add(ecGrp, ptL, ptT1, ptT2, bnCtx)) {
            LogPrintf("%s: EC_POINT_add failed.\n", __func__);
            rv = 1;
            goto End;
        }

        // ptT3 = Hp(Pi)
        if (HashToEC(&pPubkeys[i * ec_compressed_size], ec_compressed_size, bnT, ptT3) != 0) {
            LogPrintf("%s: HashToEC failed.\n", __func__);
            rv = 1;
            goto End;
        }

        // ptT1 = k1 * I
        if (!EC_POINT_mul(ecGrp, ptT1, NULL, ptKi, bnC, bnCtx)) {
            LogPrintf("%s: EC_POINT_mul failed.\n", __func__);
            rv = 1;
            goto End;
        }

        // ptT2 = k2 * ptT3
        if (!EC_POINT_mul(ecGrp, ptT2, NULL, ptT3, bnR, bnCtx)) {
            LogPrintf("%s: EC_POINT_mul failed.\n", __func__);
            rv = 1;
            goto End;
        }

        // ptR = ptT1 + ptT2
        if (!EC_POINT_add(ecGrp, ptR, ptT1, ptT2, bnCtx)) {
            LogPrintf("%s: EC_POINT_add failed.\n", __func__);
            rv = 1;
            goto End;
        }

        // sum = (sum + ci) % N
        if (!BN_mod_add(bnSum, bnSum, bnC, bnOrder, bnCtx)) {
            LogPrintf("%s: BN_mod_add failed.\n", __func__);
            rv = 1;
            goto End;
        }

        // -- add ptL and ptR to hash
        if (!(EC_POINT_point2oct(ecGrp, ptL, POINT_CONVERSION_COMPRESSED, &tempData[0], 33, bnCtx) == (int)ec_compressed_size) || !(EC_POINT_point2oct(ecGrp, ptR, POINT_CONVERSION_COMPRESSED, &tempData[33], 33, bnCtx) == (int)ec_compressed_size)) {
            LogPrintf("%s: extract ptL and ptR failed.\n", __func__);
            rv = 1;
            goto End;
        }

        ssCommitHash.write((const char*)&tempData[0], 66);
    }

    commitHash = ssCommitHash.GetHash();

    if (!(bnH) || !(bnH = BN_bin2bn(commitHash.begin(), ec_secret_size, bnH))) {
        LogPrintf("%s: commitHash -> bnH failed.\n", __func__);
        rv = 1;
        goto End;
    }

    if (!BN_mod(bnH, bnH, bnOrder, bnCtx)) {
        LogPrintf("%s: BN_mod failed.\n", __func__);
        rv = 1;
        goto End;
    }

    // bnT = (bnH - bnSum) % N
    if (!BN_mod_sub(bnT, bnH, bnSum, bnOrder, bnCtx)) {
        LogPrintf("%s: BN_mod_sub failed.\n", __func__);
        rv = 1;
        goto End;
    }

    // test bnT == 0  (bnSum == bnH)
    if (!BN_is_zero(bnT)) {
        LogPrintf("%s: signature does not verify.\n", __func__);
        rv = 2;
    }

End:

    EC_POINT_free(ptT1);
    EC_POINT_free(ptT2);
    EC_POINT_free(ptT3);
    EC_POINT_free(ptPk);
    EC_POINT_free(ptKi);
    EC_POINT_free(ptL);
    EC_POINT_free(ptR);

    BN_CTX_end(bnCtx);

    return rv;
}


int CRingSignature::GenerateRingSignatureAB(data_chunk& keyImage, uint256& txnHash, int nRingSize, int nSecretOffset, ec_secret secret, const uint8_t* pPubkeys, data_chunk& sigC, uint8_t* pSigS)
{
    // https://bitcointalk.org/index.php?topic=972541.msg10619684

    assert(nRingSize < 200);

    RandAddSeedPerfmon();

    memset(pSigS, 0, ec_secret_size * nRingSize);

    int rv = 0;
    int nBytes;

    uint256 tmpPkHash;
    uint256 tmpHash;

    uint8_t tempData[66]; // hold raw point data to hash
    ec_secret sAlpha;

    if (0 != GenerateRandomSecret(sAlpha)) {
        LogPrintf("%s: GenerateRandomSecret failed.", __func__);
        return 1;
    }

    CHashWriter ssPkHash(SER_GETHASH, PROTOCOL_VERSION);
    CHashWriter ssCjHash(SER_GETHASH, PROTOCOL_VERSION);

    uint256 test;
    for (int i = 0; i < nRingSize; ++i) {
        ssPkHash.write((const char*)&pPubkeys[i * ec_compressed_size], ec_compressed_size);

        if (i == nSecretOffset)
            continue;

        int k;
        // NOTE: necessary to clamp?
        for (k = 0; k < 32; ++k) {
            if (1 != RAND_bytes(&pSigS[i * ec_secret_size], 32)) {
                LogPrintf("%s: RAND_bytes ERR_get_error %lu.", __func__, ERR_get_error());
                return 1;
            }

            memcpy(test.begin(), &pSigS[i * ec_secret_size], 32);
            arith_uint256 toTest = UintToArith256(test);
            if (toTest > MIN_SECRET && toTest < MAX_SECRET)
                break;
        }

        if (k > 31) {
            LogPrintf("%s: Failed to generate a valid key.", __func__);
            return 1;
        }
    }

    tmpPkHash = ssPkHash.GetHash();

    BN_CTX_start(bnCtx);
    BIGNUM* bnT = BN_CTX_get(bnCtx);
    BIGNUM* bnT2 = BN_CTX_get(bnCtx);
    BIGNUM* bnS = BN_CTX_get(bnCtx);
    BIGNUM* bnC = BN_CTX_get(bnCtx);
    BIGNUM* bnCj = BN_CTX_get(bnCtx);
    BIGNUM* bnA = BN_CTX_get(bnCtx);
    EC_POINT* ptKi = NULL;
    EC_POINT* ptPk = NULL;
    EC_POINT* ptT1 = NULL;
    EC_POINT* ptT2 = NULL;
    EC_POINT* ptT3 = NULL;
    EC_POINT* ptT4 = NULL;

    if (!(ptKi = EC_POINT_new(ecGrp)) || !(ptPk = EC_POINT_new(ecGrp)) || !(ptT1 = EC_POINT_new(ecGrp)) || !(ptT2 = EC_POINT_new(ecGrp)) || !(ptT3 = EC_POINT_new(ecGrp)) || !(ptT4 = EC_POINT_new(ecGrp))) {
        LogPrintf("%s: EC_POINT_new failed.\n", __func__);
        rv = 1;
        goto End;
    }

    // get keyimage as point
    if (!EC_POINT_oct2point(ecGrp, ptKi, &keyImage[0], ec_compressed_size, bnCtx)) {
        LogPrintf("%s: extract ptKi failed.", __func__);
        rv = 1;
        goto End;
    }

    // c_{j+1} = h(P_1,...,P_n,alpha*G,alpha*H(P_j))
    if (!bnA || !(BN_bin2bn(&sAlpha.e[0], ec_secret_size, bnA))) {
        LogPrintf("%s: BN_bin2bn failed.\n", __func__);
        rv = 1;
        goto End;
    }

    // ptT1 = alpha * G
    if (!EC_POINT_mul(ecGrp, ptT1, bnA, NULL, NULL, bnCtx)) {
        LogPrintf("%s: EC_POINT_mul failed.\n", __func__);
        rv = 1;
        goto End;
    }

    // ptT3 = H(Pj)

    if (HashToEC(&pPubkeys[nSecretOffset * ec_compressed_size], ec_compressed_size, bnT2, ptT3) != 0) {
        LogPrintf("%s: HashToEC failed.\n", __func__);
        rv = 1;
        goto End;
    }

    ssCjHash.write((const char*)tmpPkHash.begin(), 32);

    // ptT2 = alpha * H(P_j)
    // ptT2 = alpha * ptT3
    if (!EC_POINT_mul(ecGrp, ptT2, NULL, ptT3, bnA, bnCtx)) {
        LogPrintf("%s: EC_POINT_mul failed.\n", __func__);
        rv = 1;
        goto End;
    }

    if (!(EC_POINT_point2oct(ecGrp, ptT1, POINT_CONVERSION_COMPRESSED, &tempData[0], 33, bnCtx) == (int)ec_compressed_size) || !(EC_POINT_point2oct(ecGrp, ptT2, POINT_CONVERSION_COMPRESSED, &tempData[33], 33, bnCtx) == (int)ec_compressed_size)) {
        LogPrintf("%s: extract ptL and ptR failed.\n", __func__);
        rv = 1;
        goto End;
    }

    ssCjHash.write((const char*)&tempData[0], 66);
    tmpHash = ssCjHash.GetHash();

    if (!bnC || !(BN_bin2bn(tmpHash.begin(), ec_secret_size, bnC)) // bnC lags i by 1
        || !BN_mod(bnC, bnC, bnOrder, bnCtx)) {
        LogPrintf("%s: hash -> bnC failed.\n", __func__);
        rv = 1;
        goto End;
    }


    // c_{j+2} = h(P_1,...,P_n,s_{j+1}*G+c_{j+1}*P_{j+1},s_{j+1}*H(P_{j+1})+c_{j+1}*I_j)
    for (int k = 0, ib = (nSecretOffset + 1) % nRingSize, i = (nSecretOffset + 2) % nRingSize;
         k < nRingSize;
         ++k, ib = i, i = (i + 1) % nRingSize) {
        if (k == nRingSize - 1) {
            // s_j = alpha - c_j*x_j mod n.
            if (!bnT || !BN_bin2bn(&secret.e[0], ec_secret_size, bnT)) {
                LogPrintf("%s: BN_bin2bn failed.\n", __func__);
                rv = 1;
                goto End;
            }

            if (!BN_mul(bnT2, bnCj, bnT, bnCtx)) {
                LogPrintf("%s: BN_mul failed.\n", __func__);
                rv = 1;
                goto End;
            }

            if (!BN_mod_sub(bnS, bnA, bnT2, bnOrder, bnCtx)) {
                LogPrintf("%s: BN_mod_sub failed.\n", __func__);
                rv = 1;
                goto End;
            }

            if (!bnS || (nBytes = BN_num_bytes(bnS)) > (int)ec_secret_size || BN_bn2bin(bnS, &pSigS[nSecretOffset * ec_secret_size + (ec_secret_size - nBytes)]) != nBytes) {
                LogPrintf("%s: bnS -> pSigS failed.\n", __func__);
                rv = 1;
                goto End;
            }

            if (nSecretOffset != nRingSize - 1)
                break;
        }

        if (!bnS || !(BN_bin2bn(&pSigS[ib * ec_secret_size], ec_secret_size, bnS))) {
            LogPrintf("%s: BN_bin2bn failed.\n", __func__);
            rv = 1;
            goto End;
        }

        // bnC is from last round (ib)
        if (!EC_POINT_oct2point(ecGrp, ptPk, &pPubkeys[ib * ec_compressed_size], ec_compressed_size, bnCtx)) {
            LogPrintf("%s: EC_POINT_oct2point failed.\n", __func__);
            rv = 1;
            goto End;
        }

        // ptT1 = s_{j+1}*G+c_{j+1}*P_{j+1}
        if (!EC_POINT_mul(ecGrp, ptT1, bnS, ptPk, bnC, bnCtx)) {
            LogPrintf("%s: EC_POINT_mul failed.\n", __func__);
            rv = 1;
            goto End;
        }

        //s_{j+1}*H(P_{j+1})+c_{j+1}*I_j

        if (HashToEC(&pPubkeys[ib * ec_compressed_size], ec_compressed_size, bnT2, ptT2) != 0) {
            LogPrintf("%s: HashToEC failed.\n", __func__);
            rv = 1;
            goto End;
        }

        // ptT3 = s_{j+1}*H(P_{j+1})
        if (!EC_POINT_mul(ecGrp, ptT3, NULL, ptT2, bnS, bnCtx)) {
            LogPrintf("%s: EC_POINT_mul failed.\n", __func__);
            rv = 1;
            goto End;
        }

        // ptT4 = c_{j+1}*I_j
        if (!EC_POINT_mul(ecGrp, ptT4, NULL, ptKi, bnC, bnCtx)) {
            LogPrintf("%s: EC_POINT_mul failed.\n", __func__);
            rv = 1;
            goto End;
        }

        // ptT2 = ptT3 + ptT4
        if (!EC_POINT_add(ecGrp, ptT2, ptT3, ptT4, bnCtx)) {
            LogPrintf("%s: EC_POINT_add failed.\n", __func__);
            rv = 1;
            goto End;
        }

        if (!(EC_POINT_point2oct(ecGrp, ptT1, POINT_CONVERSION_COMPRESSED, &tempData[0], 33, bnCtx) == (int)ec_compressed_size) || !(EC_POINT_point2oct(ecGrp, ptT2, POINT_CONVERSION_COMPRESSED, &tempData[33], 33, bnCtx) == (int)ec_compressed_size)) {
            LogPrintf("%s: extract ptL and ptR failed.\n", __func__);
            rv = 1;
            goto End;
        }

        CHashWriter ssCHash(SER_GETHASH, PROTOCOL_VERSION);
        ssCHash.write((const char*)tmpPkHash.begin(), 32);
        ssCHash.write((const char*)&tempData[0], 66);
        tmpHash = ssCHash.GetHash();

        if ((!bnC || !BN_bin2bn(tmpHash.begin(), ec_secret_size, bnC) // bnC lags i by 1
                || !BN_mod(bnC, bnC, bnOrder, bnCtx))) {
            rv = 1;
            LogPrintf("%s: hash -> bnC failed.", __func__);
            goto End;
        }

        if (i == nSecretOffset && !BN_copy(bnCj, bnC)) {
            LogPrintf("%s: BN_copy failed.\n", __func__);
            rv = 1;
            goto End;
        }

        if (i == 0) {
            memset(tempData, 0, ec_secret_size);
            if ((nBytes = BN_num_bytes(bnC)) > (int)ec_secret_size || BN_bn2bin(bnC, &tempData[0 + (ec_secret_size - nBytes)]) != nBytes) {
                LogPrintf("%s: bnC -> sigC failed.\n", __func__);
                rv = 1;
                goto End;
            }
            try {
                sigC.resize(32);
            } catch (std::exception& e) {
                LogPrintf("%s: sigC.resize failed.\n", __func__);
                rv = 1;
                goto End;
            }
            memcpy(&sigC[0], tempData, ec_secret_size);
        }
    }

End:
    EC_POINT_free(ptKi);
    EC_POINT_free(ptPk);
    EC_POINT_free(ptT1);
    EC_POINT_free(ptT2);
    EC_POINT_free(ptT3);
    EC_POINT_free(ptT4);

    BN_CTX_end(bnCtx);

    return rv;
}


int CRingSignature::VerifyRingSignatureAB(data_chunk& keyImage, uint256& txnHash, int nRingSize, const uint8_t* pPubkeys, const data_chunk& sigC, const uint8_t* pSigS)
{
    // https://bitcointalk.org/index.php?topic=972541.msg10619684

    // forall_{i=1..n} compute e_i=s_i*G+c_i*P_i and E_i=s_i*H(P_i)+c_i*I_j and c_{i+1}=h(P_1,...,P_n,e_i,E_i)
    // check c_{n+1}=c_1

    if (sigC.size() != ec_secret_size) {
        LogPrintf("%s: sigC size !=  ec_secret_size.", __func__);
        return 1;
    }
    if (keyImage.size() != ec_compressed_size) {
        LogPrintf("%s: keyImage size !=  ec_compressed_size.", __func__);
        return 1;
    }

    int rv = 0;

    uint256 tmpPkHash;
    uint256 tmpHash;

    uint8_t tempData[66]; // hold raw point data to hash
    CHashWriter ssPkHash(SER_GETHASH, PROTOCOL_VERSION);
    CHashWriter ssCjHash(SER_GETHASH, PROTOCOL_VERSION);

    for (int i = 0; i < nRingSize; ++i) {
        ssPkHash.write((const char*)&pPubkeys[i * ec_compressed_size], ec_compressed_size);
    }

    tmpPkHash = ssPkHash.GetHash();

    BN_CTX_start(bnCtx);

    BIGNUM* bnC = BN_CTX_get(bnCtx);
    BIGNUM* bnC1 = BN_CTX_get(bnCtx);
    BIGNUM* bnT = BN_CTX_get(bnCtx);
    BIGNUM* bnS = BN_CTX_get(bnCtx);
    EC_POINT* ptKi = NULL;
    EC_POINT* ptT1 = NULL;
    EC_POINT* ptT2 = NULL;
    EC_POINT* ptT3 = NULL;
    EC_POINT* ptPk = NULL;

    if (!(ptKi = EC_POINT_new(ecGrp)) || !(ptT1 = EC_POINT_new(ecGrp)) || !(ptT2 = EC_POINT_new(ecGrp)) || !(ptT3 = EC_POINT_new(ecGrp)) || !(ptPk = EC_POINT_new(ecGrp))) {
        LogPrintf("%s: EC_POINT_new failed.\n", __func__);
        rv = 1;
        goto End;
    }

    // get keyimage as point
    if (!EC_POINT_oct2point(ecGrp, ptKi, &keyImage[0], ec_compressed_size, bnCtx)) {
        rv = 1;
        LogPrintf("%s: extract ptKi failed.", __func__);
        goto End;
    }

    if (!bnC1 || !BN_bin2bn(&sigC[0], ec_secret_size, bnC1)) {
        LogPrintf("%s: BN_bin2bn failed.\n", __func__);
        rv = 1;
        goto End;
    }

    if (!BN_copy(bnC, bnC1)) {
        LogPrintf("%s: BN_copy failed.\n", __func__);
        rv = 1;
        goto End;
    }

    for (int i = 0; i < nRingSize; ++i) {
        if (!bnS || !(BN_bin2bn(&pSigS[i * ec_secret_size], ec_secret_size, bnS))) {
            LogPrintf("%s: BN_bin2bn failed.\n", __func__);
            rv = 1;
            goto End;
        }

        // ptT2 <- pk
        if (!EC_POINT_oct2point(ecGrp, ptPk, &pPubkeys[i * ec_compressed_size], ec_compressed_size, bnCtx)) {
            LogPrintf("%s: EC_POINT_oct2point failed.\n", __func__);
            rv = 1;
            goto End;
        }

        // ptT1 = e_i=s_i*G+c_i*P_i
        if (!EC_POINT_mul(ecGrp, ptT1, bnS, ptPk, bnC, bnCtx)) {
            LogPrintf("%s: EC_POINT_mul failed.\n", __func__);
            rv = 1;
            goto End;
        }

        if (!(EC_POINT_point2oct(ecGrp, ptT1, POINT_CONVERSION_COMPRESSED, &tempData[0], 33, bnCtx) == (int)ec_compressed_size)) {
            LogPrintf("%s: extract ptT1 failed.\n", __func__);
            rv = 1;
            goto End;
        }

        // ptT2 =E_i=s_i*H(P_i)+c_i*I_j

        // ptT2 =H(P_i)
        if (HashToEC(&pPubkeys[i * ec_compressed_size], ec_compressed_size, bnT, ptT2) != 0) {
            LogPrintf("%s: HashToEC failed.\n", __func__);
            rv = 1;
            goto End;
        }

        // ptT3 = s_i*ptT2
        if (!EC_POINT_mul(ecGrp, ptT3, NULL, ptT2, bnS, bnCtx)) {
            LogPrintf("%s: EC_POINT_mul failed.\n", __func__);
            rv = 1;
            goto End;
        }

        // ptT1 = c_i*I_j
        if (!EC_POINT_mul(ecGrp, ptT1, NULL, ptKi, bnC, bnCtx)) {
            LogPrintf("%s: EC_POINT_mul failed.\n", __func__);
            rv = 1;
            goto End;
        }

        // ptT2 = ptT3 + ptT1
        if (!EC_POINT_add(ecGrp, ptT2, ptT3, ptT1, bnCtx)) {
            LogPrintf("%s: EC_POINT_add failed.\n", __func__);
            rv = 1;
            goto End;
        }

        if (!(EC_POINT_point2oct(ecGrp, ptT2, POINT_CONVERSION_COMPRESSED, &tempData[33], 33, bnCtx) == (int)ec_compressed_size)) {
            LogPrintf("%s: extract ptT2 failed.\n", __func__);
            rv = 1;
            goto End;
        }

        CHashWriter ssCHash(SER_GETHASH, PROTOCOL_VERSION);
        ssCHash.write((const char*)tmpPkHash.begin(), 32);
        ssCHash.write((const char*)&tempData[0], 66);
        tmpHash = ssCHash.GetHash();

        if (!bnC || !(BN_bin2bn(tmpHash.begin(), ec_secret_size, bnC)) || !BN_mod(bnC, bnC, bnOrder, bnCtx)) {
            LogPrintf("%s: tmpHash -> bnC failed.\n", __func__);
            rv = 1;
            goto End;
        }
    }

    // bnT = (bnC - bnC1) % N
    if (!BN_mod_sub(bnT, bnC, bnC1, bnOrder, bnCtx)) {
        LogPrintf("%s: BN_mod_sub failed.\n", __func__);
        rv = 1;
        goto End;
    }

    // test bnT == 0  (bnC == bnC1)
    if (!BN_is_zero(bnT)) {
        LogPrintf("%s: signature does not verify.\n", __func__);
        rv = 2;
    }

End:

    BN_CTX_end(bnCtx);

    EC_POINT_free(ptKi);
    EC_POINT_free(ptT1);
    EC_POINT_free(ptT2);
    EC_POINT_free(ptT3);
    EC_POINT_free(ptPk);

    return rv;
}

int CRingSignature::HashToEC(const uint8_t* p, uint32_t len, BIGNUM* bnTmp, EC_POINT* ptRet, bool fNew)
{
    // - bn(hash(data)) * (G + bn1)
    int count = 0;
    uint256 pkHash = Hash(p, p + len);
    BIGNUM* bnOne = BN_CTX_get(bnCtx);
    BN_one(bnOne);

    if (!bnTmp || !BN_bin2bn(pkHash.begin(), ec_secret_size, bnTmp)) {
        LogPrintf("%s: BN_bin2bn failed.", __func__);
        return 1;
    }

    if (fNew)
        while (!EC_POINT_set_compressed_coordinates_GFp(ecGrp, ptRet, bnTmp, 0, bnCtx) && count < 100) {
            count += 1;

            if (count == 100) {
                LogPrintf("%s: Failed to find a valid point for public key.", __func__);
                return 1;
            }

            BN_add(bnTmp, bnTmp, bnOne);
        }
    else if (!EC_POINT_mul(ecGrp, ptRet, bnTmp, NULL, NULL, bnCtx)) {
        LogPrintf("%s: EC_POINT_mul failed.", __func__);
        return 1;
    }

    return 0;
}