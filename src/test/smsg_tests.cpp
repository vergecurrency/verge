// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <smsg/smessage.h>
#include <test/setup_common.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cstring>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(smsg_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(smsg_hkdf_sha256_kdf_vector)
{
    uint256 ecdhSecret;
    std::vector<uint8_t> ecdhBytes(32);
    for (size_t i = 0; i < ecdhBytes.size(); ++i) {
        ecdhBytes[i] = static_cast<uint8_t>(i);
    }
    std::copy(ecdhBytes.begin(), ecdhBytes.end(), ecdhSecret.begin());

    std::vector<uint8_t> ephemeralPubkey(33);
    ephemeralPubkey[0] = 0x02;
    for (size_t i = 1; i < ephemeralPubkey.size(); ++i) {
        ephemeralPubkey[i] = static_cast<uint8_t>(0x1f + i);
    }

    CKeyID destination;
    for (size_t i = 0; i < destination.size(); ++i) {
        destination.begin()[i] = static_cast<uint8_t>(0x40 + i);
    }

    uint8_t iv[16];
    for (size_t i = 0; i < sizeof(iv); ++i) {
        iv[i] = static_cast<uint8_t>(0x60 + i);
    }

    std::vector<uint8_t> key_e;
    std::vector<uint8_t> key_m;
    smsg::DeriveSmsgKeys(ecdhSecret, ephemeralPubkey.data(), destination, iv, key_e, key_m);

    BOOST_CHECK_EQUAL(HexStr(key_e), "d677270733bccd77b633251f93ca3ed2e775b2f8a82198c25da08f8da2c46bf9");
    BOOST_CHECK_EQUAL(HexStr(key_m), "146dc161b04a3480d3681d78e35cdc4d911b6a7f32dffa627b611cd30927b192");
}

BOOST_AUTO_TEST_CASE(decrypted_payload_shape_rejects_malformed_inputs)
{
    bool anonymous = false;
    uint32_t lenData = 0;
    uint32_t lenPlain = 0;

    BOOST_CHECK_EQUAL(smsg::ValidateDecryptedPayloadShape({}), smsg::SMSG_GENERAL_ERROR);
    BOOST_CHECK_EQUAL(smsg::ValidateDecryptedPayloadShape({250}), smsg::SMSG_GENERAL_ERROR);

    std::vector<uint8_t> shortSigned(smsg::SMSG_PL_HDR_LEN - 1, 0);
    BOOST_CHECK_EQUAL(smsg::ValidateDecryptedPayloadShape(shortSigned), smsg::SMSG_GENERAL_ERROR);

    std::vector<uint8_t> signedMismatch(smsg::SMSG_PL_HDR_LEN + 2, 0);
    uint32_t declaredLen = 3;
    std::memcpy(&signedMismatch[1 + 20 + 65], &declaredLen, sizeof(declaredLen));
    BOOST_CHECK_EQUAL(smsg::ValidateDecryptedPayloadShape(signedMismatch), smsg::SMSG_GENERAL_ERROR);

    std::vector<uint8_t> signedTooLong(smsg::SMSG_PL_HDR_LEN, 0);
    declaredLen = smsg::SMSG_MAX_MSG_BYTES + 1;
    std::memcpy(&signedTooLong[1 + 20 + 65], &declaredLen, sizeof(declaredLen));
    BOOST_CHECK_EQUAL(smsg::ValidateDecryptedPayloadShape(signedTooLong), smsg::SMSG_MESSAGE_TOO_LONG);

    std::vector<uint8_t> anonTooLong(9, 0);
    anonTooLong[0] = 250;
    declaredLen = smsg::SMSG_MAX_AMSG_BYTES + 1;
    std::memcpy(&anonTooLong[5], &declaredLen, sizeof(declaredLen));
    BOOST_CHECK_EQUAL(smsg::ValidateDecryptedPayloadShape(anonTooLong), smsg::SMSG_MESSAGE_TOO_LONG);

    std::vector<uint8_t> validSigned(smsg::SMSG_PL_HDR_LEN + 5, 0);
    declaredLen = 5;
    std::memcpy(&validSigned[1 + 20 + 65], &declaredLen, sizeof(declaredLen));
    BOOST_CHECK_EQUAL(smsg::ValidateDecryptedPayloadShape(validSigned, &anonymous, &lenData, &lenPlain), smsg::SMSG_NO_ERROR);
    BOOST_CHECK(!anonymous);
    BOOST_CHECK_EQUAL(lenData, 5U);
    BOOST_CHECK_EQUAL(lenPlain, 5U);
}

BOOST_AUTO_TEST_SUITE_END()
