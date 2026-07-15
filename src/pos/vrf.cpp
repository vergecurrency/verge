// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/vrf.h>

#include <crypto/hmac_sha256.h>
#include <crypto/sha256.h>
#include <pos/primitives.h>

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/obj_mac.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <vector>

namespace pos {
namespace {

static constexpr unsigned char SUITE = 0x02;
using Bytes32 = std::array<unsigned char, 32>;
using PointPtr = std::unique_ptr<EC_POINT, decltype(&EC_POINT_free)>;
using BnPtr = std::unique_ptr<BIGNUM, decltype(&BN_free)>;
using CtxPtr = std::unique_ptr<BN_CTX, decltype(&BN_CTX_free)>;
using GroupPtr = std::unique_ptr<EC_GROUP, decltype(&EC_GROUP_free)>;

Bytes32 Sha256(const unsigned char* data, size_t size)
{
    Bytes32 output{};
    CSHA256().Write(data, size).Finalize(output.data());
    return output;
}

Bytes32 Hmac(const Bytes32& key, const std::vector<unsigned char>& data)
{
    Bytes32 output{};
    CHMAC_SHA256(key.data(), key.size())
        .Write(data.data(), data.size())
        .Finalize(output.data());
    return output;
}

bool SerializePoint(const EC_GROUP* group, const EC_POINT* point, BN_CTX* ctx,
                    unsigned char output[33])
{
    return EC_POINT_point2oct(group, point, POINT_CONVERSION_COMPRESSED,
                              output, 33, ctx) == 33;
}

bool ParsePoint(const EC_GROUP* group, const unsigned char input[33],
                EC_POINT* point, BN_CTX* ctx)
{
    return EC_POINT_oct2point(group, point, input, 33, ctx) == 1 &&
           EC_POINT_is_at_infinity(group, point) == 0 &&
           EC_POINT_is_on_curve(group, point, ctx) == 1;
}

bool ExpandMessageXmd(const std::vector<unsigned char>& message,
                      unsigned char output[48])
{
    static const unsigned char suite_id[] =
        "P256_XMD:SHA-256_SSWU_NU_";
    std::vector<unsigned char> dst{'E', 'C', 'V', 'R', 'F', '_'};
    dst.insert(dst.end(), suite_id, suite_id + sizeof(suite_id) - 1);
    dst.push_back(SUITE);

    std::vector<unsigned char> dst_prime = dst;
    dst_prime.push_back(static_cast<unsigned char>(dst.size()));

    std::vector<unsigned char> input(64, 0);
    input.insert(input.end(), message.begin(), message.end());
    input.push_back(0);
    input.push_back(48);
    input.push_back(0);
    input.insert(input.end(), dst_prime.begin(), dst_prime.end());
    const Bytes32 b0 = Sha256(input.data(), input.size());

    std::vector<unsigned char> b1_input(b0.begin(), b0.end());
    b1_input.push_back(1);
    b1_input.insert(b1_input.end(), dst_prime.begin(), dst_prime.end());
    const Bytes32 b1 = Sha256(b1_input.data(), b1_input.size());

    std::vector<unsigned char> b2_input(32);
    for (size_t i = 0; i < 32; ++i) b2_input[i] = b0[i] ^ b1[i];
    b2_input.push_back(2);
    b2_input.insert(b2_input.end(), dst_prime.begin(), dst_prime.end());
    const Bytes32 b2 = Sha256(b2_input.data(), b2_input.size());

    std::copy(b1.begin(), b1.end(), output);
    std::copy_n(b2.begin(), 16, output + 32);
    return true;
}

bool MapToCurve(const EC_GROUP* group, const BIGNUM* u, EC_POINT* output,
                BN_CTX* ctx)
{
    BN_CTX_start(ctx);
    BIGNUM* p = BN_CTX_get(ctx);
    BIGNUM* a = BN_CTX_get(ctx);
    BIGNUM* b = BN_CTX_get(ctx);
    BIGNUM* z = BN_CTX_get(ctx);
    BIGNUM* tv1 = BN_CTX_get(ctx);
    BIGNUM* tv2 = BN_CTX_get(ctx);
    BIGNUM* tv3 = BN_CTX_get(ctx);
    BIGNUM* tv4 = BN_CTX_get(ctx);
    BIGNUM* tv5 = BN_CTX_get(ctx);
    BIGNUM* tv6 = BN_CTX_get(ctx);
    BIGNUM* x = BN_CTX_get(ctx);
    BIGNUM* y = BN_CTX_get(ctx);
    BIGNUM* ratio = BN_CTX_get(ctx);
    BIGNUM* inverse = BN_CTX_get(ctx);
    if (inverse == nullptr ||
        !EC_GROUP_get_curve(group, p, a, b, ctx) ||
        !BN_set_word(z, 10) || !BN_sub(z, p, z) ||
        !BN_mod_sqr(tv1, u, p, ctx) ||
        !BN_mod_mul(tv1, z, tv1, p, ctx) ||
        !BN_mod_sqr(tv2, tv1, p, ctx) ||
        !BN_mod_add(tv2, tv2, tv1, p, ctx) ||
        !BN_copy(tv3, tv2) || !BN_add_word(tv3, 1) ||
        !BN_mod_mul(tv3, b, tv3, p, ctx)) {
        BN_CTX_end(ctx);
        return false;
    }

    if (BN_is_zero(tv2)) {
        if (!BN_copy(tv4, z)) {
            BN_CTX_end(ctx);
            return false;
        }
    } else {
        if (!BN_mod_sub(tv4, p, tv2, p, ctx)) {
            BN_CTX_end(ctx);
            return false;
        }
    }

    bool ok =
        BN_mod_mul(tv4, a, tv4, p, ctx) &&
        BN_mod_sqr(tv2, tv3, p, ctx) &&
        BN_mod_sqr(tv6, tv4, p, ctx) &&
        BN_mod_mul(tv5, a, tv6, p, ctx) &&
        BN_mod_add(tv2, tv2, tv5, p, ctx) &&
        BN_mod_mul(tv2, tv2, tv3, p, ctx) &&
        BN_mod_mul(tv6, tv6, tv4, p, ctx) &&
        BN_mod_mul(tv5, b, tv6, p, ctx) &&
        BN_mod_add(tv2, tv2, tv5, p, ctx) &&
        BN_mod_mul(x, tv1, tv3, p, ctx);
    if (!ok || BN_mod_inverse(inverse, tv6, p, ctx) == nullptr ||
        !BN_mod_mul(ratio, tv2, inverse, p, ctx)) {
        BN_CTX_end(ctx);
        return false;
    }

    ERR_clear_error();
    BIGNUM* square_root = BN_mod_sqrt(y, ratio, p, ctx);
    const bool gx1_square = square_root != nullptr;
    if (!gx1_square) {
        ERR_clear_error();
        if (!BN_mod_mul(ratio, z, ratio, p, ctx) ||
            BN_mod_sqrt(y, ratio, p, ctx) == nullptr ||
            !BN_mod_mul(y, tv1, y, p, ctx) ||
            !BN_mod_mul(y, u, y, p, ctx)) {
            BN_CTX_end(ctx);
            return false;
        }
    } else if (!BN_copy(x, tv3)) {
        BN_CTX_end(ctx);
        return false;
    }

    if (BN_is_odd(u) != BN_is_odd(y) && !BN_mod_sub(y, p, y, p, ctx)) {
        BN_CTX_end(ctx);
        return false;
    }
    if (BN_mod_inverse(inverse, tv4, p, ctx) == nullptr ||
        !BN_mod_mul(x, x, inverse, p, ctx) ||
        !EC_POINT_set_affine_coordinates(group, output, x, y, ctx) ||
        EC_POINT_is_on_curve(group, output, ctx) != 1) {
        BN_CTX_end(ctx);
        return false;
    }
    BN_CTX_end(ctx);
    return true;
}

bool EncodeToCurve(const EC_GROUP* group,
                   const unsigned char public_key[33],
                   const unsigned char* input, size_t input_size,
                   EC_POINT* output, BN_CTX* ctx)
{
    std::vector<unsigned char> message(public_key, public_key + 33);
    message.insert(message.end(), input, input + input_size);
    unsigned char uniform[48];
    if (!ExpandMessageXmd(message, uniform)) return false;

    BN_CTX_start(ctx);
    BIGNUM* p = BN_CTX_get(ctx);
    BIGNUM* u = BN_CTX_get(ctx);
    const bool ok = u != nullptr &&
                    EC_GROUP_get_curve(group, p, nullptr, nullptr, ctx) == 1 &&
                    BN_bin2bn(uniform, sizeof(uniform), u) != nullptr &&
                    BN_mod(u, u, p, ctx) == 1 &&
                    MapToCurve(group, u, output, ctx);
    BN_CTX_end(ctx);
    return ok;
}

bool Challenge(const EC_GROUP* group, BN_CTX* ctx,
               const EC_POINT* y, const EC_POINT* h,
               const EC_POINT* gamma, const EC_POINT* u,
               const EC_POINT* v, unsigned char c[16])
{
    std::vector<unsigned char> data{SUITE, 0x02};
    unsigned char encoded[33];
    for (const EC_POINT* point : {y, h, gamma, u, v}) {
        if (!SerializePoint(group, point, ctx, encoded)) return false;
        data.insert(data.end(), encoded, encoded + 33);
    }
    data.push_back(0);
    const Bytes32 hash = Sha256(data.data(), data.size());
    std::copy_n(hash.begin(), 16, c);
    return true;
}

bool ProofToHash(const EC_GROUP* group, BN_CTX* ctx, const EC_POINT* gamma,
                 unsigned char output[32])
{
    std::vector<unsigned char> data{SUITE, 0x03};
    unsigned char encoded[33];
    if (!SerializePoint(group, gamma, ctx, encoded)) return false;
    data.insert(data.end(), encoded, encoded + 33);
    data.push_back(0);
    const Bytes32 hash = Sha256(data.data(), data.size());
    std::copy(hash.begin(), hash.end(), output);
    return true;
}

bool Rfc6979Nonce(const unsigned char secret[32],
                  const unsigned char h_string[33], const BIGNUM* order,
                  BIGNUM* nonce, BN_CTX* ctx)
{
    const Bytes32 h1 = Sha256(h_string, 33);
    BN_CTX_start(ctx);
    BIGNUM* h_bn = BN_CTX_get(ctx);
    BIGNUM* reduced = BN_CTX_get(ctx);
    if (reduced == nullptr || BN_bin2bn(h1.data(), h1.size(), h_bn) == nullptr ||
        !BN_mod(reduced, h_bn, order, ctx)) {
        BN_CTX_end(ctx);
        return false;
    }
    unsigned char h_octets[32];
    if (BN_bn2binpad(reduced, h_octets, 32) != 32) {
        BN_CTX_end(ctx);
        return false;
    }

    Bytes32 k{};
    Bytes32 v;
    v.fill(1);
    std::vector<unsigned char> data(v.begin(), v.end());
    data.push_back(0);
    data.insert(data.end(), secret, secret + 32);
    data.insert(data.end(), h_octets, h_octets + 32);
    k = Hmac(k, data);
    data.assign(v.begin(), v.end());
    v = Hmac(k, data);

    data.assign(v.begin(), v.end());
    data.push_back(1);
    data.insert(data.end(), secret, secret + 32);
    data.insert(data.end(), h_octets, h_octets + 32);
    k = Hmac(k, data);
    data.assign(v.begin(), v.end());
    v = Hmac(k, data);

    for (;;) {
        data.assign(v.begin(), v.end());
        v = Hmac(k, data);
        if (BN_bin2bn(v.data(), v.size(), nonce) != nullptr &&
            !BN_is_zero(nonce) && BN_cmp(nonce, order) < 0) {
            BN_CTX_end(ctx);
            return true;
        }
        data.assign(v.begin(), v.end());
        data.push_back(0);
        k = Hmac(k, data);
        data.assign(v.begin(), v.end());
        v = Hmac(k, data);
    }
}

} // namespace

bool VrfVerify(const unsigned char public_key[VRF_PUBLIC_KEY_SIZE],
               const unsigned char* input, size_t input_size,
               const unsigned char proof[VRF_PROOF_SIZE],
               unsigned char output[VRF_OUTPUT_SIZE])
{
    GroupPtr group(EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1),
                   &EC_GROUP_free);
    CtxPtr ctx(BN_CTX_new(), &BN_CTX_free);
    if (!group || !ctx) return false;

    PointPtr y(EC_POINT_new(group.get()), &EC_POINT_free);
    PointPtr h(EC_POINT_new(group.get()), &EC_POINT_free);
    PointPtr gamma(EC_POINT_new(group.get()), &EC_POINT_free);
    PointPtr u(EC_POINT_new(group.get()), &EC_POINT_free);
    PointPtr v(EC_POINT_new(group.get()), &EC_POINT_free);
    PointPtr temp(EC_POINT_new(group.get()), &EC_POINT_free);
    BnPtr order(BN_new(), &BN_free);
    BnPtr c(BN_bin2bn(proof + 33, 16, nullptr), &BN_free);
    BnPtr s(BN_bin2bn(proof + 49, 32, nullptr), &BN_free);
    BnPtr neg_c(BN_new(), &BN_free);
    if (!y || !h || !gamma || !u || !v || !temp || !order || !c || !s ||
        !neg_c || !ParsePoint(group.get(), public_key, y.get(), ctx.get()) ||
        !ParsePoint(group.get(), proof, gamma.get(), ctx.get()) ||
        !EC_GROUP_get_order(group.get(), order.get(), ctx.get()) ||
        BN_cmp(s.get(), order.get()) >= 0 ||
        !EncodeToCurve(group.get(), public_key, input, input_size, h.get(),
                       ctx.get()) ||
        !BN_mod_sub(neg_c.get(), order.get(), c.get(), order.get(), ctx.get()) ||
        !EC_POINT_mul(group.get(), u.get(), s.get(), y.get(), neg_c.get(),
                      ctx.get()) ||
        !EC_POINT_mul(group.get(), v.get(), nullptr, h.get(), s.get(),
                      ctx.get()) ||
        !EC_POINT_mul(group.get(), temp.get(), nullptr, gamma.get(),
                      neg_c.get(), ctx.get()) ||
        !EC_POINT_add(group.get(), v.get(), v.get(), temp.get(), ctx.get())) {
        return false;
    }

    unsigned char expected_c[16];
    if (!Challenge(group.get(), ctx.get(), y.get(), h.get(), gamma.get(),
                   u.get(), v.get(), expected_c) ||
        std::memcmp(expected_c, proof + 33, 16) != 0) {
        return false;
    }
    return ProofToHash(group.get(), ctx.get(), gamma.get(), output);
}

bool DeriveVrfKey(const unsigned char signing_secret[32],
                  unsigned char vrf_secret[32],
                  unsigned char vrf_public_key[VRF_PUBLIC_KEY_SIZE])
{
    static const unsigned char derivation_input[] = {
        'V', 'e', 'r', 'g', 'e', 'P', 'o', 'S', 'V', 'R', 'F', 'K', 'e', 'y'
    };
    for (uint32_t counter = 0; counter < 256; ++counter) {
        TaggedHashWriter writer(HashDomain::VRF_KEY);
        writer.write(reinterpret_cast<const char*>(signing_secret), 32);
        writer << counter;
        const uint256 candidate = writer.GetHash();
        std::copy(candidate.begin(), candidate.end(), vrf_secret);
        unsigned char output[VRF_OUTPUT_SIZE]{};
        unsigned char proof[VRF_PROOF_SIZE]{};
        if (VrfProve(vrf_secret, derivation_input, sizeof(derivation_input),
                     vrf_public_key, output, proof)) {
            return true;
        }
    }
    std::memset(vrf_secret, 0, 32);
    std::memset(vrf_public_key, 0, VRF_PUBLIC_KEY_SIZE);
    return false;
}

bool VrfProve(const unsigned char secret_key[32],
              const unsigned char* input, size_t input_size,
              unsigned char public_key[VRF_PUBLIC_KEY_SIZE],
              unsigned char output[VRF_OUTPUT_SIZE],
              unsigned char proof[VRF_PROOF_SIZE])
{
    GroupPtr group(EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1),
                   &EC_GROUP_free);
    CtxPtr ctx(BN_CTX_new(), &BN_CTX_free);
    BnPtr secret(BN_bin2bn(secret_key, 32, nullptr), &BN_free);
    BnPtr order(BN_new(), &BN_free);
    BnPtr nonce(BN_new(), &BN_free);
    BnPtr challenge(BN_new(), &BN_free);
    BnPtr response(BN_new(), &BN_free);
    BnPtr temp_bn(BN_new(), &BN_free);
    if (!group || !ctx || !secret || !order || !nonce || !challenge ||
        !response || !temp_bn ||
        !EC_GROUP_get_order(group.get(), order.get(), ctx.get()) ||
        BN_is_zero(secret.get()) || BN_cmp(secret.get(), order.get()) >= 0) {
        return false;
    }

    PointPtr y(EC_POINT_new(group.get()), &EC_POINT_free);
    PointPtr h(EC_POINT_new(group.get()), &EC_POINT_free);
    PointPtr gamma(EC_POINT_new(group.get()), &EC_POINT_free);
    PointPtr nonce_g(EC_POINT_new(group.get()), &EC_POINT_free);
    PointPtr nonce_h(EC_POINT_new(group.get()), &EC_POINT_free);
    if (!y || !h || !gamma || !nonce_g || !nonce_h ||
        !EC_POINT_mul(group.get(), y.get(), secret.get(), nullptr, nullptr,
                      ctx.get()) ||
        !SerializePoint(group.get(), y.get(), ctx.get(), public_key) ||
        !EncodeToCurve(group.get(), public_key, input, input_size, h.get(),
                       ctx.get()) ||
        !EC_POINT_mul(group.get(), gamma.get(), nullptr, h.get(), secret.get(),
                      ctx.get())) {
        return false;
    }

    unsigned char h_string[33];
    if (!SerializePoint(group.get(), h.get(), ctx.get(), h_string) ||
        !Rfc6979Nonce(secret_key, h_string, order.get(), nonce.get(),
                      ctx.get()) ||
        !EC_POINT_mul(group.get(), nonce_g.get(), nonce.get(), nullptr,
                      nullptr, ctx.get()) ||
        !EC_POINT_mul(group.get(), nonce_h.get(), nullptr, h.get(),
                      nonce.get(), ctx.get()) ||
        !SerializePoint(group.get(), gamma.get(), ctx.get(), proof)) {
        return false;
    }

    unsigned char c_bytes[16];
    if (!Challenge(group.get(), ctx.get(), y.get(), h.get(), gamma.get(),
                   nonce_g.get(), nonce_h.get(), c_bytes) ||
        BN_bin2bn(c_bytes, 16, challenge.get()) == nullptr ||
        !BN_mod_mul(temp_bn.get(), challenge.get(), secret.get(), order.get(),
                    ctx.get()) ||
        !BN_mod_add(response.get(), nonce.get(), temp_bn.get(), order.get(),
                    ctx.get()) ||
        BN_bn2binpad(response.get(), proof + 49, 32) != 32) {
        return false;
    }
    std::copy(c_bytes, c_bytes + 16, proof + 33);
    return ProofToHash(group.get(), ctx.get(), gamma.get(), output);
}

std::vector<unsigned char> GetVrfInput(uint32_t network_id,
                                       const uint256& epoch_seed,
                                       uint64_t slot,
                                       const COutPoint& bond_outpoint)
{
    TaggedHashWriter writer(HashDomain::SLOT);
    writer << network_id << epoch_seed << slot << bond_outpoint;
    const uint256 hash = writer.GetHash();
    return std::vector<unsigned char>(hash.begin(), hash.end());
}

} // namespace pos
