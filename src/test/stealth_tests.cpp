// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <boost/test/unit_test.hpp>
#include <base58.h>
#include <key_io.h>
#include <pubkey.h>
#include <script/standard.h>
#include <util/strencodings.h>
#include <stealth.h>


struct StealthTestingSetup {
    ECCVerifyHandle verify_handle;
};

BOOST_FIXTURE_TEST_SUITE(stealth_tests, StealthTestingSetup)

// Credit to ShadowCoin Developers
BOOST_AUTO_TEST_CASE(stealth_key)
{
    const char *testAddr = "smYjKTSpYSAznNCeRiRxb992ey8Xu11mowhp4ee4hBccqWwzRQfKfkCEnK3T7SjowDDmfmqWwZxiDkiPKpiEuw936H5yWYSqnhKL9N";
    
    CStealthAddress sxAddr;
    
    BOOST_CHECK(true == sxAddr.SetEncoded(testAddr));
    
    BOOST_CHECK(HexStr(sxAddr.scan_pubkey.begin(), sxAddr.scan_pubkey.end()) == "029b62c32a5561946b1b43cce1235a3b47d82abde25807cb9df2a65a1941558a8d");
    BOOST_CHECK(HexStr(sxAddr.spend_pubkey.begin(), sxAddr.spend_pubkey.end()) == "02f0e2f682c8a07fdba7a3a97f823261008c7f53156c311d20216af0b6cc8148c3");
    
    BOOST_CHECK(sxAddr.Encoded() == testAddr);

}

BOOST_AUTO_TEST_CASE(stealth_derivation_round_trip)
{
    ec_secret scan_secret{};
    ec_secret spend_secret{};
    ec_secret ephem_secret{};
    scan_secret.e[31] = 1;
    spend_secret.e[31] = 2;
    ephem_secret.e[31] = 3;

    ec_point scan_pubkey;
    ec_point spend_pubkey;
    ec_point ephem_pubkey;
    BOOST_REQUIRE_EQUAL(SecretToPublicKey(scan_secret, scan_pubkey), 0);
    BOOST_REQUIRE_EQUAL(SecretToPublicKey(spend_secret, spend_pubkey), 0);
    BOOST_REQUIRE_EQUAL(SecretToPublicKey(ephem_secret, ephem_pubkey), 0);

    ec_secret sender_shared{};
    ec_point destination_pubkey;
    BOOST_REQUIRE_EQUAL(StealthSecret(ephem_secret, scan_pubkey, spend_pubkey,
                                     sender_shared, destination_pubkey), 0);

    ec_secret receiver_shared{};
    ec_point receiver_destination;
    BOOST_REQUIRE_EQUAL(StealthSecret(scan_secret, ephem_pubkey, spend_pubkey,
                                     receiver_shared, receiver_destination), 0);
    BOOST_CHECK_EQUAL_COLLECTIONS(std::begin(sender_shared.e), std::end(sender_shared.e),
                                  std::begin(receiver_shared.e), std::end(receiver_shared.e));
    BOOST_CHECK_EQUAL_COLLECTIONS(destination_pubkey.begin(), destination_pubkey.end(),
                                  receiver_destination.begin(), receiver_destination.end());

    ec_secret destination_secret{};
    BOOST_REQUIRE_EQUAL(StealthSecretSpend(scan_secret, ephem_pubkey, spend_secret,
                                          destination_secret), 0);
    ec_point destination_from_secret;
    BOOST_REQUIRE_EQUAL(SecretToPublicKey(destination_secret, destination_from_secret), 0);
    BOOST_CHECK_EQUAL_COLLECTIONS(destination_pubkey.begin(), destination_pubkey.end(),
                                  destination_from_secret.begin(), destination_from_secret.end());
}

BOOST_AUTO_TEST_CASE(stealth_spend_secret_is_zero_padded)
{
    ec_secret shared{};
    ec_secret spend{};
    ec_secret derived{};
    spend.e[31] = 1;

    BOOST_REQUIRE_EQUAL(StealthSharedToSecretSpend(shared, spend, derived), 0);
    for (size_t i = 0; i < ec_secret_size - 1; ++i) {
        BOOST_CHECK_EQUAL(derived.e[i], 0);
    }
    BOOST_CHECK_EQUAL(derived.e[ec_secret_size - 1], 1);
}

BOOST_AUTO_TEST_CASE(stealth_address_rejects_noncanonical_fields)
{
    const std::string address = "smYjKTSpYSAznNCeRiRxb992ey8Xu11mowhp4ee4hBccqWwzRQfKfkCEnK3T7SjowDDmfmqWwZxiDkiPKpiEuw936H5yWYSqnhKL9N";
    std::vector<unsigned char> raw;
    BOOST_REQUIRE(DecodeBase58(address, raw));
    BOOST_REQUIRE_EQUAL(raw.size(), 75U);

    raw.resize(raw.size() - 4);
    raw[35] = 2; // Only one spend public key is supported.
    AppendChecksum(raw);
    const std::string malformed = EncodeBase58(raw);

    CStealthAddress decoded;
    BOOST_CHECK(!decoded.SetEncoded(malformed));
    BOOST_CHECK(!IsStealthAddress(malformed));
    BOOST_CHECK(!IsValidDestinationString(malformed));
}

BOOST_AUTO_TEST_CASE(stealth_destination_requires_explicit_derivation)
{
    const std::string address = "smYjKTSpYSAznNCeRiRxb992ey8Xu11mowhp4ee4hBccqWwzRQfKfkCEnK3T7SjowDDmfmqWwZxiDkiPKpiEuw936H5yWYSqnhKL9N";
    CStealthAddress stealth;
    BOOST_REQUIRE(stealth.SetEncoded(address));
    BOOST_CHECK(GetScriptForDestination(stealth).empty());
}

BOOST_AUTO_TEST_SUITE_END()
