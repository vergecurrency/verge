#include <boost/test/unit_test.hpp>
#include <util/strencodings.h>
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
    
    BOOST_CHECK(HexStr(sxAddr.spend_secret.begin(), sxAddr.spend_secret.end()) == "");
    BOOST_CHECK(HexStr(sxAddr.scan_secret.begin(), sxAddr.scan_secret.end()) == "");

    BOOST_CHECK(sxAddr.Encoded() == testAddr);
}

BOOST_AUTO_TEST_CASE(stealth_key_generation)
{
        CStealthAddress sxAddr;
        std::string error; 
        std::string label;
        GenerateNewStealthAddress(error, label ,sxAddr);

        BOOST_ASSERT(error == "");
        BOOST_ASSERT(IsStealthAddress(sxAddr.Encoded()));

        BOOST_CHECK(sxAddr.Encoded() == sxAddr.Encoded());
}

BOOST_AUTO_TEST_CASE(stealth_key_import_export)
{
        std::string exportedKey = "jhMoDUsSgcLDLyexLwwCRQSRpqr7azoS89Sm6dcmZaX8PYz9JNm51cipJMdkFfNcucVQ2QL7LvwYqH9HkDeHUivY4233DmxnEsfgXdCACGwFWbnDjMwcxBNUx5eXTXTRHbSAMcmigYtPfGgLP2TAmW8acpiMzME83gRWVv3LVXHvZ3x1cyZAAEpp";
        std::string wrongPrefix = "4HcixyKYoLDLNjLxtUqqoG1SpaYHzV9ATUxQyjw8mjnZkqUHAL99xcLAjUp9HZveBF2VNctM97pUGLjcFufFzFsuSVFPQ6jYRYRwhCuq9JQR9Zqs1LUtgzL3BUaDpCKGbUGfwp7zsG7jypAL9zHrZLmQ93WvTnpirS1e6h2pyXsixeGpvVCjnV1mX";
        std::string wrongEncoding = "5FD7Yrqu4fyQhr5JEKXLkQV9LwU2EJrJ6Lj9iok6vvVGsi9GMgcZBXPmUFrk1dW6WByFZFWB5D5QX4Cb45SS0T1P3MbtGdHkLNGsAnoVsFLtZoMA1ajrco1eY9np6ejSyLH7MP6PsYrZoxjLLFyjVfZoTnryTs3NUUgLthe4GQ8pLWyEtsbV6qa6m";
        std::string wrongLength = "5FD7Yrqu4fyQhr5JEKXLkQV9LwU2EJrJ6Lj9iok6vvVGsi9GMgcZBXPmUFrk1dW6WByF5D5QX4Cb45SS0T1P3MbtGdHkLNGsAnoVsFLtZoMA1ajrco1eY9np6ejSyLH7MP6PsYrZoxjLLFyjVfZoTnryTs3NUUgLthe4GQ8pLWyEtsbV6qa6m";
        std::string addressInput = "smYjKTSpYSAznNCeRiRxb992ey8Xu11mowhp4ee4hBccqWwzRQfKfkCEnK3T7SjowDDmfmqWwZxiDkiPKpiEuw936H5yWYSqnhKL9N";

        bool imported = false;
        CStealthAddress sxAddr;

        imported = sxAddr.Import(wrongPrefix);
        BOOST_CHECK(imported == false);

        imported = sxAddr.Import(wrongEncoding);
        BOOST_CHECK(imported == false);

        imported = sxAddr.Import(wrongLength);
        BOOST_CHECK(imported == false);

        imported = sxAddr.Import(addressInput);
        BOOST_CHECK(imported == false);

        imported = sxAddr.Import(exportedKey);
        BOOST_CHECK(imported == true);
        BOOST_ASSERT(IsStealthAddress(sxAddr.Encoded()));
        BOOST_CHECK(sxAddr.Export() == exportedKey);
}

BOOST_AUTO_TEST_SUITE_END()
