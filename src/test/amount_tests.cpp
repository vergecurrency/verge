// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2020 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <amount.h>
#include <policy/feerate.h>
#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(amount_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(MoneyRangeTest)
{
    BOOST_CHECK_EQUAL(MoneyRange(CAmount(-1)), false);
    BOOST_CHECK_EQUAL(MoneyRange(MAX_MONEY + CAmount(1)), false);
    BOOST_CHECK_EQUAL(MoneyRange(CAmount(1)), true);
}

BOOST_AUTO_TEST_CASE(GetFeeTest)
{
    CFeeRate feeRate, altFeeRate;

    feeRate = CFeeRate(0);
    // Must always return 0
    BOOST_CHECK_EQUAL(feeRate.GetFee(0), CAmount(0));
    BOOST_CHECK_EQUAL(feeRate.GetFee(1e5), CAmount(0));

    feeRate = CFeeRate(1000);
    // Must always just return the arg
    BOOST_CHECK_EQUAL(feeRate.GetFee(0), CAmount(0));
    BOOST_CHECK_EQUAL(feeRate.GetFee(1), CAmount(1000));
    BOOST_CHECK_EQUAL(feeRate.GetFee(121), CAmount(1000));
    BOOST_CHECK_EQUAL(feeRate.GetFee(999), CAmount(1000));
    BOOST_CHECK_EQUAL(feeRate.GetFee(1e3), CAmount(2000));
    BOOST_CHECK_EQUAL(feeRate.GetFee(9e3), CAmount(10000));

    feeRate = CFeeRate(-1000);
    // Must always just return -1 * arg
    BOOST_CHECK_EQUAL(feeRate.GetFee(0), CAmount(0));
    BOOST_CHECK_EQUAL(feeRate.GetFee(1), CAmount(-1000));
    BOOST_CHECK_EQUAL(feeRate.GetFee(121), CAmount(-1000));
    BOOST_CHECK_EQUAL(feeRate.GetFee(999), CAmount(-1000));
    BOOST_CHECK_EQUAL(feeRate.GetFee(1e3), CAmount(-2000));
    BOOST_CHECK_EQUAL(feeRate.GetFee(9e3), CAmount(-10000));

    feeRate = CFeeRate(123);
    // Truncates the result, if not integer
    BOOST_CHECK_EQUAL(feeRate.GetFee(0), CAmount(0));
    BOOST_CHECK_EQUAL(feeRate.GetFee(8), CAmount(123)); // Special case: returns 1 instead of 0
    BOOST_CHECK_EQUAL(feeRate.GetFee(9), CAmount(123));
    BOOST_CHECK_EQUAL(feeRate.GetFee(121), CAmount(123));
    BOOST_CHECK_EQUAL(feeRate.GetFee(122), CAmount(123));
    BOOST_CHECK_EQUAL(feeRate.GetFee(999), CAmount(123));
    BOOST_CHECK_EQUAL(feeRate.GetFee(1e3), CAmount(246));
    BOOST_CHECK_EQUAL(feeRate.GetFee(9e3), CAmount(1230));

    feeRate = CFeeRate(-123);
    // Truncates the result, if not integer
    BOOST_CHECK_EQUAL(feeRate.GetFee(0), CAmount(0));

    // check alternate constructor
    feeRate = CFeeRate(1000);
    altFeeRate = CFeeRate(feeRate);
    BOOST_CHECK_EQUAL(feeRate.GetFee(100), altFeeRate.GetFee(100));

    feeRate = CFeeRate(10 * CENT);

    BOOST_CHECK_EQUAL(feeRate.GetFee(1001), 20 * CENT);
    BOOST_CHECK_EQUAL(feeRate.GetFee(1000), 20 * CENT);
    BOOST_CHECK_EQUAL(feeRate.GetFee(999), 10 * CENT);
    BOOST_CHECK_EQUAL(feeRate.GetFee(1), 10 * CENT);

    // Check full constructor
    // default value
    BOOST_CHECK(CFeeRate(CAmount(-1), 1000) == CFeeRate(10 * CENT));
    BOOST_CHECK(CFeeRate(CAmount(0), 1000) == CFeeRate(10 * CENT));
    BOOST_CHECK(CFeeRate(CAmount(1), 1000) == CFeeRate(10 * CENT));
    // lost precision (can only resolve satoshis per kB)
    BOOST_CHECK(CFeeRate(CAmount(1), 1001) == CFeeRate(10 * CENT));
    BOOST_CHECK(CFeeRate(CAmount(2), 1001) == CFeeRate(10 * CENT));
    // some more integer checks
    BOOST_CHECK(CFeeRate(CAmount(26), 789) == CFeeRate(10 * CENT));
    BOOST_CHECK(CFeeRate(CAmount(27), 789) == CFeeRate(10 * CENT));
    // Maximum size in bytes, should not crash
    CFeeRate(MAX_MONEY, std::numeric_limits<size_t>::max() >> 1).GetFeePerK();
}

BOOST_AUTO_TEST_CASE(BinaryOperatorTest)
{
    CFeeRate a, b;
    a = CFeeRate(1);
    b = CFeeRate(2);
    BOOST_CHECK(a < b);
    BOOST_CHECK(b > a);
    BOOST_CHECK(a == a);
    BOOST_CHECK(a <= b);
    BOOST_CHECK(a <= a);
    BOOST_CHECK(b >= a);
    BOOST_CHECK(b >= b);
    // a should be 0.00000002 XVG/kB now
    a += a;
    BOOST_CHECK(a == b);
}

BOOST_AUTO_TEST_CASE(ToStringTest)
{
    CFeeRate feeRate;
    feeRate = CFeeRate(1);
    BOOST_CHECK_EQUAL(feeRate.ToString(), "0.00000001 XVG/kB");
}

BOOST_AUTO_TEST_SUITE_END()
