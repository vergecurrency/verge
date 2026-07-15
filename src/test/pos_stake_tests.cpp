// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pos/stake.h>
#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <cstring>

namespace {

uint160 KeyId(const char* hex)
{
    uint160 result;
    result.SetHex(hex);
    return result;
}

} // namespace
BOOST_FIXTURE_TEST_SUITE(pos_stake_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(bond_script_round_trip)
{
    pos::BondData expected;
    std::memset(expected.signing_public_key, 1, sizeof(expected.signing_public_key));
    expected.vrf_public_key[0] = 0x02;
    std::memset(expected.vrf_public_key + 1, 2, sizeof(expected.vrf_public_key) - 1);
    expected.reward_key_id = KeyId("01");
    expected.withdrawal_key_id = KeyId("02");

    const CScript script = pos::GetBondScript(expected);
    BOOST_CHECK(!script.empty());

    pos::BondData decoded;
    BOOST_CHECK(pos::ParseBondScript(script, decoded));
    BOOST_CHECK(decoded.reward_key_id == expected.reward_key_id);
    BOOST_CHECK(decoded.withdrawal_key_id == expected.withdrawal_key_id);
    BOOST_CHECK_EQUAL_COLLECTIONS(decoded.signing_public_key, decoded.signing_public_key + sizeof(decoded.signing_public_key), expected.signing_public_key, expected.signing_public_key + sizeof(expected.signing_public_key));
    BOOST_CHECK_EQUAL_COLLECTIONS(decoded.vrf_public_key, decoded.vrf_public_key + sizeof(decoded.vrf_public_key), expected.vrf_public_key, expected.vrf_public_key + sizeof(expected.vrf_public_key));

    CScript trailing = script;
    trailing << OP_DROP;
    BOOST_CHECK(!pos::ParseBondScript(trailing, decoded));
}

BOOST_AUTO_TEST_CASE(unbond_script_round_trip)
{
    pos::UnbondData expected;
    expected.unlock_height = 20160;
    expected.withdrawal_key_id = KeyId("03");

    const CScript script = pos::GetUnbondScript(expected);
    BOOST_CHECK(!script.empty());

    pos::UnbondData decoded;
    BOOST_CHECK(pos::ParseUnbondScript(script, decoded));
    BOOST_CHECK_EQUAL(decoded.unlock_height, expected.unlock_height);
    BOOST_CHECK(decoded.withdrawal_key_id == expected.withdrawal_key_id);
}

BOOST_AUTO_TEST_CASE(reject_invalid_stake_metadata)
{
    pos::BondData bond;
    BOOST_CHECK(pos::GetBondScript(bond).empty());

    pos::UnbondData unbond;
    BOOST_CHECK(pos::GetUnbondScript(unbond).empty());

    CScript ordinary = CScript() << OP_DUP << OP_HASH160
                                 << std::vector<unsigned char>(20, 1)
                                 << OP_EQUALVERIFY << OP_CHECKSIG;
    BOOST_CHECK(!pos::ParseBondScript(ordinary, bond));
    BOOST_CHECK(!pos::ParseUnbondScript(ordinary, unbond));
}

BOOST_AUTO_TEST_SUITE_END()
