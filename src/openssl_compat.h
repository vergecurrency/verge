#if OPENSSL_VERSION_NUMBER < 0x10100000L

#include <cstdlib>
#include <cstring>
#include <openssl/engine.h>
#include <openssl/hmac.h>

inline void ECDSA_SIG_get0(const ECDSA_SIG *sig, const BIGNUM **pr, const BIGNUM **ps)
{
    if (pr != NULL)
        *pr = sig->r;
    if (ps != NULL)
        *ps = sig->s;
}

inline int ECDSA_SIG_set0(ECDSA_SIG *sig, BIGNUM *r, BIGNUM *s)
{
    if (r == NULL || s == NULL)
        return 0;
    BN_clear_free(sig->r);
    BN_clear_free(sig->s);
    sig->r = r;
    sig->s = s;
    return 1;
}

inline EVP_CIPHER_CTX *EVP_CIPHER_CTX_new()
{
    EVP_CIPHER_CTX *ctx = (EVP_CIPHER_CTX *)OPENSSL_malloc(sizeof(*ctx));

    if (ctx != NULL)
        EVP_CIPHER_CTX_init(ctx);

    return ctx;
}

inline HMAC_CTX *HMAC_CTX_new()
{
    HMAC_CTX *ctx = (HMAC_CTX *)OPENSSL_malloc(sizeof(*ctx));

    if (ctx != NULL)
        HMAC_CTX_init(ctx);

    return ctx;
}

inline void HMAC_CTX_free(HMAC_CTX *ctx)
{
    if (ctx != NULL)
    {
        HMAC_CTX_cleanup(ctx);
        OPENSSL_free(ctx);
    }
}
#endif