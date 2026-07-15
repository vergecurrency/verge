// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/crypto.h>
#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <cstring>

BOOST_FIXTURE_TEST_SUITE(pos_crypto_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(schnorr_round_trip_and_domain_failure)
{
    unsigned char secret[32]{};
    secret[31] = 3;
    unsigned char auxiliary[32]{};
    auxiliary[0] = 7;
    const uint256 message = uint256S("1234");

    unsigned char signature[pos::SCHNORR_SIGNATURE_SIZE]{};
    unsigned char public_key[pos::SCHNORR_PUBLIC_KEY_SIZE]{};
    BOOST_REQUIRE(pos::SignSchnorr(secret, message, auxiliary, signature,
                                   public_key));
    unsigned char derived_public[pos::SCHNORR_PUBLIC_KEY_SIZE]{};
    BOOST_REQUIRE(pos::GetSchnorrPublicKey(secret, derived_public));
    BOOST_CHECK_EQUAL_COLLECTIONS(
        public_key, public_key + pos::SCHNORR_PUBLIC_KEY_SIZE,
        derived_public, derived_public + pos::SCHNORR_PUBLIC_KEY_SIZE);
    BOOST_CHECK(pos::VerifySchnorr(public_key, message, signature));

    const uint256 other_message = uint256S("1235");
    BOOST_CHECK(!pos::VerifySchnorr(public_key, other_message, signature));

    signature[0] ^= 1;
    BOOST_CHECK(!pos::VerifySchnorr(public_key, message, signature));
}

BOOST_AUTO_TEST_CASE(reject_invalid_secret_and_public_key)
{
    unsigned char zero_secret[32]{};
    unsigned char auxiliary[32]{};
    unsigned char signature[pos::SCHNORR_SIGNATURE_SIZE]{};
    unsigned char public_key[pos::SCHNORR_PUBLIC_KEY_SIZE]{};
    BOOST_CHECK(!pos::SignSchnorr(zero_secret, uint256S("01"), auxiliary,
                                  signature, public_key));
    BOOST_CHECK(!pos::GetSchnorrPublicKey(zero_secret, public_key));

    std::memset(public_key, 0xff, sizeof(public_key));
    BOOST_CHECK(!pos::VerifySchnorr(public_key, uint256S("01"), signature));
}

BOOST_AUTO_TEST_SUITE_END()
