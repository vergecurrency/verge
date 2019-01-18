#ifndef HASHX13_H
#define HASHX13_H

#include "uint256.h"
#include "sph_blake.h"
#include "sph_bmw.h"
#include "sph_groestl.h"
#include "sph_jh.h"
#include "sph_keccak.h"
#include "sph_skein.h"
#include "sph_luffa.h"
#include "sph_cubehash.h"
#include "sph_shavite.h"
#include "sph_simd.h"
#include "sph_echo.h"
#include "sph_hamsi.h"
#include "sph_fugue.h"

#ifndef QT_NO_DEBUG
#include <string>
#endif

#ifdef GLOBALDEFINED
#define GLOBAL
#else
#define GLOBAL extern
#endif

GLOBAL sph_blake512_context     D_blake;
GLOBAL sph_bmw512_context       D_bmw;
GLOBAL sph_groestl512_context   D_groestl;
GLOBAL sph_jh512_context        D_jh;
GLOBAL sph_keccak512_context    D_keccak;
GLOBAL sph_skein512_context     D_skein;
GLOBAL sph_luffa512_context     D_luffa;
GLOBAL sph_cubehash512_context  D_cubehash;
GLOBAL sph_shavite512_context   D_shavite;
GLOBAL sph_simd512_context      D_simd;
GLOBAL sph_echo512_context      D_echo;
GLOBAL sph_hamsi512_context      D_hamsi;
GLOBAL sph_fugue512_context      D_fugue;

#define fillz13() do { \
    sph_blake512_init(&D_blake); \
    sph_bmw512_init(&D_bmw); \
    sph_groestl512_init(&D_groestl); \
    sph_jh512_init(&D_jh); \
    sph_keccak512_init(&D_keccak); \
    sph_skein512_init(&D_skein); \
    sph_luffa512_init(&D_luffa); \
    sph_cubehash512_init(&D_cubehash); \
    sph_shavite512_init(&D_shavite); \
    sph_simd512_init(&D_simd); \
    sph_echo512_init(&D_echo); \
    sph_hamsi512_init(&D_hamsi); \
    sph_fugue512_init(&D_fugue); \
} while (0) 


#define DBLAKE (memcpy(&ctx_blake, &D_blake, sizeof(D_blake)))
#define DBMW (memcpy(&ctx_bmw, &D_bmw, sizeof(D_bmw)))
#define DGROESTL (memcpy(&ctx_groestl, &D_groestl, sizeof(D_groestl)))
#define DJH (memcpy(&ctx_jh, &D_jh, sizeof(D_jh)))
#define DKECCAK (memcpy(&ctx_keccak, &D_keccak, sizeof(D_keccak)))
#define DSKEIN (memcpy(&ctx_skein, &D_skein, sizeof(D_skein)))
#define DHAMSI (memcpy(&ctx_hamsi, &D_hamsi, sizeof(D_hamsi)))
#define DFUGUE (memcpy(&ctx_fugue, &D_fugue, sizeof(D_fugue)))
template<typename T1>
inline uint256 HashX13(const T1 pbegin, const T1 pend)

{
    sph_blake512_context     ctx_blake;
    sph_bmw512_context       ctx_bmw;
    sph_groestl512_context   ctx_groestl;
    sph_jh512_context        ctx_jh;
    sph_keccak512_context    ctx_keccak;
    sph_skein512_context     ctx_skein;
    sph_luffa512_context     ctx_luffa;
    sph_cubehash512_context  ctx_cubehash;
    sph_shavite512_context   ctx_shavite;
    sph_simd512_context      ctx_simd;
    sph_echo512_context      ctx_echo;
    sph_hamsi512_context      ctx_hamsi;
    sph_fugue512_context      ctx_fugue;
    static unsigned char pblank[1];

#ifndef QT_NO_DEBUG
    //std::string strhash;
    //strhash = "";
#endif
    
    uint512 hash[17];

    sph_blake512_init(&ctx_blake);
    sph_blake512 (&ctx_blake, (pbegin == pend ? pblank : static_cast<const void*>(&pbegin[0])), (pend - pbegin) * sizeof(pbegin[0]));
    sph_blake512_close(&ctx_blake, static_cast<void*>(&hash[0]));
    
    sph_bmw512_init(&ctx_bmw);
    sph_bmw512 (&ctx_bmw, static_cast<const void*>(&hash[0]), 64);
    sph_bmw512_close(&ctx_bmw, static_cast<void*>(&hash[1]));

    sph_groestl512_init(&ctx_groestl);
    sph_groestl512 (&ctx_groestl, static_cast<const void*>(&hash[1]), 64);
    sph_groestl512_close(&ctx_groestl, static_cast<void*>(&hash[2]));

    sph_skein512_init(&ctx_skein);
    sph_skein512 (&ctx_skein, static_cast<const void*>(&hash[2]), 64);
    sph_skein512_close(&ctx_skein, static_cast<void*>(&hash[3]));
    
    sph_jh512_init(&ctx_jh);
    sph_jh512 (&ctx_jh, static_cast<const void*>(&hash[3]), 64);
    sph_jh512_close(&ctx_jh, static_cast<void*>(&hash[4]));
    
    sph_keccak512_init(&ctx_keccak);
    sph_keccak512 (&ctx_keccak, static_cast<const void*>(&hash[4]), 64);
    sph_keccak512_close(&ctx_keccak, static_cast<void*>(&hash[5]));

    sph_luffa512_init(&ctx_luffa);
    sph_luffa512 (&ctx_luffa, static_cast<void*>(&hash[5]), 64);
    sph_luffa512_close(&ctx_luffa, static_cast<void*>(&hash[6]));
    
    sph_cubehash512_init(&ctx_cubehash);
    sph_cubehash512 (&ctx_cubehash, static_cast<const void*>(&hash[6]), 64);
    sph_cubehash512_close(&ctx_cubehash, static_cast<void*>(&hash[7]));
    
    sph_shavite512_init(&ctx_shavite);
    sph_shavite512(&ctx_shavite, static_cast<const void*>(&hash[7]), 64);
    sph_shavite512_close(&ctx_shavite, static_cast<void*>(&hash[8]));
        
    sph_simd512_init(&ctx_simd);
    sph_simd512 (&ctx_simd, static_cast<const void*>(&hash[8]), 64);
    sph_simd512_close(&ctx_simd, static_cast<void*>(&hash[9]));

    sph_echo512_init(&ctx_echo);
    sph_echo512 (&ctx_echo, static_cast<const void*>(&hash[9]), 64);
    sph_echo512_close(&ctx_echo, static_cast<void*>(&hash[10]));

    sph_hamsi512_init(&ctx_hamsi);
    sph_hamsi512 (&ctx_hamsi, static_cast<const void*>(&hash[10]), 64);
    sph_hamsi512_close(&ctx_hamsi, static_cast<void*>(&hash[11]));

    sph_fugue512_init(&ctx_fugue);
    sph_fugue512 (&ctx_fugue, static_cast<const void*>(&hash[11]), 64);
    sph_fugue512_close(&ctx_fugue, static_cast<void*>(&hash[12]));


    return uint256(hash[12]);
}






#endif // HASHX13_H
