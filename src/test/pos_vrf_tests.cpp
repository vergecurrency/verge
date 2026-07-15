// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/vrf.h>
#include <test/setup_common.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace {

void CheckVector(const std::string& secret_hex, const std::string& public_hex,
                 const std::string& input_hex, const std::string& proof_hex,
                 const std::string& output_hex)
{
    const std::vector<unsigned char> secret = ParseHex(secret_hex);
    const std::vector<unsigned char> expected_public = ParseHex(public_hex);
    const std::vector<unsigned char> input = ParseHex(input_hex);
    const std::vector<unsigned char> expected_proof = ParseHex(proof_hex);
    const std::vector<unsigned char> expected_output = ParseHex(output_hex);
    BOOST_REQUIRE_EQUAL(secret.size(), 32U);
    BOOST_REQUIRE_EQUAL(expected_public.size(), pos::VRF_PUBLIC_KEY_SIZE);
    BOOST_REQUIRE_EQUAL(expected_proof.size(), pos::VRF_PROOF_SIZE);
    BOOST_REQUIRE_EQUAL(expected_output.size(), pos::VRF_OUTPUT_SIZE);

    unsigned char public_key[pos::VRF_PUBLIC_KEY_SIZE]{};
    unsigned char proof[pos::VRF_PROOF_SIZE]{};
    unsigned char output[pos::VRF_OUTPUT_SIZE]{};
    BOOST_REQUIRE(pos::VrfProve(secret.data(), input.data(), input.size(),
                                public_key, output, proof));
    BOOST_CHECK_EQUAL_COLLECTIONS(public_key,
                                  public_key + pos::VRF_PUBLIC_KEY_SIZE,
                                  expected_public.begin(),
                                  expected_public.end());
    BOOST_CHECK_EQUAL_COLLECTIONS(proof, proof + pos::VRF_PROOF_SIZE,
                                  expected_proof.begin(),
                                  expected_proof.end());
    BOOST_CHECK_EQUAL_COLLECTIONS(output, output + pos::VRF_OUTPUT_SIZE,
                                  expected_output.begin(),
                                  expected_output.end());

    unsigned char verified[pos::VRF_OUTPUT_SIZE]{};
    BOOST_REQUIRE(pos::VrfVerify(public_key, input.data(), input.size(), proof,
                                 verified));
    BOOST_CHECK_EQUAL_COLLECTIONS(verified, verified + pos::VRF_OUTPUT_SIZE,
                                  expected_output.begin(),
                                  expected_output.end());

    proof[0] ^= 1;
    BOOST_CHECK(!pos::VrfVerify(public_key, input.data(), input.size(), proof,
                                verified));
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(pos_vrf_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(rfc9381_p256_sswu_example_13)
{
    CheckVector(
        "c9afa9d845ba75166b5c215767b1d6934e50c3db36e89b127b8a622b120f6721",
        "0360fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb6",
        "73616d706c65",
        "0331d984ca8fece9cbb9a144c0d53df3c4c7a33080c1e02ddb1a96a365394c7888782fffde7b842c38c20c08de6ec6c2e7027a97000f2c9fa4425d5c03e639fb48fde58114d755985498d7eb234cf4aed9",
        "21e66dc9747430f17ed9efeda054cf4a264b097b9e8956a1787526ed00dc664b");
}

BOOST_AUTO_TEST_CASE(rfc9381_p256_sswu_example_14)
{
    CheckVector(
        "c9afa9d845ba75166b5c215767b1d6934e50c3db36e89b127b8a622b120f6721",
        "0360fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb6",
        "74657374",
        "03f814c0455d32dbc75ad3aea08c7e2db31748e12802db23640203aebf1fa8db2743aad348a3006dc1caad7da28687320740bf7dd78fe13c298867321ce3b36b79ec3093b7083ac5e4daf3465f9f43c627",
        "8e7185d2b420e4f4681f44ce313a26d05613323837da09a69f00491a83ad25dd");
}

BOOST_AUTO_TEST_CASE(delegated_vrf_key_derivation_is_deterministic)
{
    unsigned char signing_secret[32]{};
    signing_secret[31] = 3;
    unsigned char first_secret[32]{};
    unsigned char second_secret[32]{};
    unsigned char first_public[pos::VRF_PUBLIC_KEY_SIZE]{};
    unsigned char second_public[pos::VRF_PUBLIC_KEY_SIZE]{};
    BOOST_REQUIRE(pos::DeriveVrfKey(signing_secret, first_secret,
                                    first_public));
    BOOST_REQUIRE(pos::DeriveVrfKey(signing_secret, second_secret,
                                    second_public));
    BOOST_CHECK_EQUAL_COLLECTIONS(first_secret, first_secret + 32,
                                  second_secret, second_secret + 32);
    BOOST_CHECK_EQUAL_COLLECTIONS(
        first_public, first_public + pos::VRF_PUBLIC_KEY_SIZE,
        second_public, second_public + pos::VRF_PUBLIC_KEY_SIZE);
    BOOST_CHECK(!std::equal(signing_secret, signing_secret + 32,
                            first_secret));
}

BOOST_AUTO_TEST_SUITE_END()
