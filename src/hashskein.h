// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef HASH_SKEIN
#define HASH_SKEIN

#include "uint256.h"
#include "serialize.h"
#include "sph_skein.h"

#include <openssl/sha.h>
#include <openssl/ripemd.h>
#include <vector>


template<typename T1>
inline uint256 HashSkein(const T1 pbegin, const T1 pend)

{
    sph_skein512_context ctx_skein;
    static unsigned char pblank[1];

    uint512 hash1;
    uint256 hash2;

    sph_skein512_init(&ctx_skein);
    sph_skein512(&ctx_skein, (pbegin == pend ? pblank : static_cast<const void*>(&pbegin[0])), (pend - pbegin) * sizeof(pbegin[0]));
    sph_skein512_close(&ctx_skein, static_cast<void*>(&hash1));
    
    SHA256((unsigned char*)&hash1, 64, (unsigned char*)&hash2);
    
    return hash2;
}


#endif
