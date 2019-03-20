#include <boost/test/unit_test.hpp>
// #include <boost/atomic.hpp>
#include <stealth.h>


BOOST_AUTO_TEST_SUITE(stealth_tests)

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

BOOST_AUTO_TEST_SUITE_END()
