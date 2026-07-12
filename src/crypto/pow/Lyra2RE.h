#ifndef VERGE_CRYPTO_POW_LYRA2RE_H
#define VERGE_CRYPTO_POW_LYRA2RE_H

#ifdef __cplusplus
extern "C" {
#endif

int lyra2re_hash(const char* input, char* output);
int lyra2re2_hash(const char* input, char* output);

#ifdef __cplusplus
}
#endif

#endif // VERGE_CRYPTO_POW_LYRA2RE_H
