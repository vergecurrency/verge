// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2025 The Verge Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef VERGE_CRYPTO_POW_HASHQUARK_H
#define VERGE_CRYPTO_POW_HASHQUARK_H

#include <uint256.h>
#include <sph_blake.h>
#include <sph_bmw.h>
#include <sph_groestl.h>
#include <sph_jh.h>
#include <sph_keccak.h>
#include <sph_skein.h>

template<typename T1>
inline uint256 HashQuark(const T1 pbegin, const T1 pend)
{
   sph_blake512_context     ctx_blake;
    sph_bmw512_context       ctx_bmw;
    sph_groestl512_context   ctx_groestl;
    sph_jh512_context        ctx_jh;
    sph_keccak512_context    ctx_keccak;
    sph_skein512_context     ctx_skein;
    static unsigned char pblank[1];

#ifndef QT_NO_DEBUG
    //std::string strhash;
    //strhash = "";
#endif

    uint512 mask = 8;
    uint512 zero = 0;
    
    uint512 hash[9];

    sph_blake512_init(&ctx_blake);
    // ZBLAKE;
    sph_blake512 (&ctx_blake, (pbegin == pend ? pblank : static_cast<const void*>(&pbegin[0])), (pend - pbegin) * sizeof(pbegin[0]));
    sph_blake512_close(&ctx_blake, static_cast<void*>(&hash[0]));
    
    sph_bmw512_init(&ctx_bmw);
    // ZBMW;
    sph_bmw512 (&ctx_bmw, static_cast<const void*>(&hash[0]), 64);
    sph_bmw512_close(&ctx_bmw, static_cast<void*>(&hash[1]));

    if ((hash[1] & mask) != zero)
    {
        sph_groestl512_init(&ctx_groestl);
        // ZGROESTL;
        sph_groestl512 (&ctx_groestl, static_cast<const void*>(&hash[1]), 64);
        sph_groestl512_close(&ctx_groestl, static_cast<void*>(&hash[2]));
    }
    else
    {
        sph_skein512_init(&ctx_skein);
        // ZSKEIN;
        sph_skein512 (&ctx_skein, static_cast<const void*>(&hash[1]), 64);
        sph_skein512_close(&ctx_skein, static_cast<void*>(&hash[2]));
    }
    
    sph_groestl512_init(&ctx_groestl);
    // ZGROESTL;
    sph_groestl512 (&ctx_groestl, static_cast<const void*>(&hash[2]), 64);
    sph_groestl512_close(&ctx_groestl, static_cast<void*>(&hash[3]));

    sph_jh512_init(&ctx_jh);
    // ZJH;
    sph_jh512 (&ctx_jh, static_cast<const void*>(&hash[3]), 64);
    sph_jh512_close(&ctx_jh, static_cast<void*>(&hash[4]));

    if ((hash[4] & mask) != zero)
    {
        sph_blake512_init(&ctx_blake);
        // ZBLAKE;
        sph_blake512 (&ctx_blake, static_cast<const void*>(&hash[4]), 64);
        sph_blake512_close(&ctx_blake, static_cast<void*>(&hash[5]));
    }
    else
    {
        sph_bmw512_init(&ctx_bmw);
        // ZBMW;
        sph_bmw512 (&ctx_bmw, static_cast<const void*>(&hash[4]), 64);
        sph_bmw512_close(&ctx_bmw, static_cast<void*>(&hash[5]));
    }
    
    sph_keccak512_init(&ctx_keccak);
    // ZKECCAK;
    sph_keccak512 (&ctx_keccak, static_cast<const void*>(&hash[5]), 64);
    sph_keccak512_close(&ctx_keccak, static_cast<void*>(&hash[6]));

    sph_skein512_init(&ctx_skein);
    // SKEIN;
    sph_skein512 (&ctx_skein, static_cast<const void*>(&hash[6]), 64);
    sph_skein512_close(&ctx_skein, static_cast<void*>(&hash[7]));

    if ((hash[7] & mask) != zero)
    {
        sph_keccak512_init(&ctx_keccak);
        // ZKECCAK;
        sph_keccak512 (&ctx_keccak, static_cast<const void*>(&hash[7]), 64);
        sph_keccak512_close(&ctx_keccak, static_cast<void*>(&hash[8]));
    }
    else
    {
        sph_jh512_init(&ctx_jh);
        // ZJH;
        sph_jh512 (&ctx_jh, static_cast<const void*>(&hash[7]), 64);
        sph_jh512_close(&ctx_jh, static_cast<void*>(&hash[8]));
    }

    return uint256(hash[8]);
}

#endif // VERGE_CRYPTO_POW_HASHQUARK_H
