// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <smsg/smessage.h>
#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <cstring>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(smsg_tests, BasicTestingSetup)

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
