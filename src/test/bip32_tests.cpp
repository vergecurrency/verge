// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2023 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <key.h>
#include <key_io.h>
#include <uint256.h>
#include <util/system.h>
#include <util/strencodings.h>
#include <test/setup_common.h>

#include <string>
#include <vector>

struct TestDerivation {
    std::string pub;
    std::string prv;
    unsigned int nChild;
};

struct TestVector {
    std::string strHexMaster;
    std::vector<TestDerivation> vDerive;

    explicit TestVector(std::string strHexMasterIn) : strHexMaster(strHexMasterIn) {}

    TestVector& operator()(std::string pub, std::string prv, unsigned int nChild) {
        vDerive.push_back(TestDerivation());
        TestDerivation &der = vDerive.back();
        der.pub = pub;
        der.prv = prv;
        der.nChild = nChild;
        return *this;
    }
};

TestVector test1 =
  TestVector("000102030405060708090a0b0c0d0e0f")
    ("ToEA6epvY6iUs9r4R5mvZQyK8EtsCsK1xZ6hEXrGFDKjQpAT8V28vb7LKuKhFM7zoxQ7CzWX7dE7mJNiqBw8t8PU91Bu8oAqabvg9xGoBzHZLxp",
     "TDt9EWvD5T5T44hAac1bLFvmH5fhMx2T6zgUKCmsDWZskySV2A1VDhHjPie5CBJ8nZs7TKXmo5sy2eHhFCSwa4gxqpKBK9NEZaua11yXwhAQ2wp",
     0x80000000)
    ("ToEA6h6LXp1wxQy3ZgudmAqx1GzyXgsNLdahv9HADJhrP1MjVmKKLpeRT3g72cghZGJeDCo5yrmYVwSooauGqA7rCtedLRy6VuptftQMVd9h3VM",
     "TDt9EZBd5ANv9Kp9jD9JY1oQA7mogmaoV5AUzpCmBbwzjAdmPSJfdvppWrzUySeUCHF5ANRUAPCqR18ZxxUfDey9dtPhP23NoYcTi2cPNoMUB6w",
     1)
    ("ToEA6jGTjbZqM7r7Q8x5VRjpc1tdb4rULfJzrTwzoaUfKe9wjJpT4d4jQcaLSh9GyD1zAvR8mHDecXwHPte6JVHjtYUZddowyYqAJcw8ncLBfDe",
     "TDt9EbMkGwvoY2hDZfBkGGhGkrfTk9ZuV6tmw8sbmsiofoRycyooMjF8URtiPVqXoD27cTB9yCoGVi5z6tztLzm8R3t8to16sJPKNpvkun8rYcG",
     0x80000002)
    ("ToEA6msjn8Ph3zhXGJkAk29By1vmpDizcCtzC1Bg3umpVWXP9A1ZsNuAK48FziL7R5TJXSWsNfCFzUWFrDnP1aqGwzdQDXn64Vtr2JrGstysVAH",
     "TDt9Edy2KUkfEuYdRpyqWs6e7rhbyJSRkeUmGg7H2D1xqfoR2pzvAV5ZNsSdwY49mSXc7vCYZfkBv7utL6jdw1gKY9KoFQZf2EdFz6fyYM6oD1B",
     2)
    ("ToEA6p78cZWeEUNWLnDtSHMGrX65u31jEoDBdV4yJsJHox5zWFdvw9bXWdbBg3KVpsc2zke3WfgD3DQNQe8P6tGUfb6f8KLGtgng2zm82T6KJan",
     "TDt9EgCR9uscRPDcWJTZD8Jj1Mrv47jAPEnxi9zaHAYSA7N2PvdHEFmvaSuZcrTcE6w3BiLFBaykpwN3oHe68SDfjXuqeeekaKSy95993rNMzUu",
     1000000000)
    ("ToEA6qpu6F7tMbkhs81LYpv1AE65hGb34J2DRjTXZ9gS22UVDP3KTZrBmjkFJsESg8gkUSvb21c1aR6y14XQCjmKWrFmzrkMhANJqxP3GenZSJd",
     "TDt9EhvBdbUrYWbp2eF1KfsTK4rurMJUCjbzWQP8XSvaNBkX742fkg2aqZ4dFiEp9247o7jVxwxhfyuAoTeTfWEPrVZNz9QyhECgRQhVo1R47Ej",
     0);

TestVector test2 =
  TestVector("fffcf9f6f3f0edeae7e4e1dedbd8d5d2cfccc9c6c3c0bdbab7b4b1aeaba8a5a29f9c999693908d8a8784817e7b7875726f6c696663605d5a5754514e4b484542")
    ("ToEA6epvY6iUs9r4QhHFfa3baScdT4TSeBMWv7dzYA4ZPCHKRU3L6jtGkoVHbTCyXCm57YkK2HtHANsyYyfN1KJdGvBqzjNJR6RLUKwuTyjmv5e",
     "TDt9EWvD5T5T44hAaDWvSR13jHPTc9AsncwHznZbWTJhjMZMK92gPr4fpcofYF5By5RChNHCYNLRJkKtLjB3cV5fTBdwmZSUhvpJaGbmJc39xAS",
     0)
    ("ToEA6i6fopQSMAg8vxsYoMRUeF29p2ChM9qoUHPMhy61Tc9H7SVJg1ELue4YAvAsKHmWKsEUtWt2ZTiwi963tU13cWr8Gn6yKSZibp4ZuCFDqQD",
     "TDt9EaBxMAmQY5XF6V7DaCNvo5nyy6v8VbRaYxJxgGL9omRK17Uey7QjyTNv7kLzc9G372X2SNk9juk1hEosAmaiPQoLmjrY6Qz3hgWKfy7BcRy",
     0xFFFFFFFF)
    ("ToEA6jFj4RRxj4rF6q5jDoR87ahiCmRbte1VZSTtdMzn7LufZjimHtef7hr8nJQz52Nc7PA2YMjdrg9YNoxPfNVALfJkorj2gApjUCLAjzRSrz1",
     "TDt9EbM1bmnvuyhMGMKPzeNaGRUYMr9335bGe7PVbfEvTWBhTQi7azq4BXAWj6pyLPzX1FqvDPwnnvF88UYdu9BqxiEuCU5srviJVTqrSM8WfhM",
     1)
    ("ToEA6n4hUSQtti9SCSksHTZh5XdwYJJ6zpvkssxkURf7REG7EZavbQBN1cEWd6c7ru4thZcaNzpsVoTNTcDQaGv7bzmfQ8vstiFwPy17zPEAhLs",
     "TDt9Ee9z1nms5czYMxzY4JX9ENQmhP1Y9GWXxYtMSiuFmPY98EaGtWMm5RYtZu2bN7CDKDCcxh2BCTvLTBKnx2iDyC4CLEcgi2ggyGGauzni6xJ",
     0xFFFFFFFE)
    ("ToEA6oEjPQMFW779VPzU1wPET3L8T2mU3oZFuage19MXFzXMguurh57NY8bcFkjd4GbLf1GDBVtaYQnE7FNoGA2dKMU7g3FNT1MMGC5hXLLMMwA",
     "TDt9EfL1vkiDh1xFevE8nnLgbt6xc7UuCF92zFcEySbfc9oPaauCzBHmbwuzCbkyzXaVrnVKGWoAaqVvGJRATycif2jNAB9zrEiKJrH6yj3CXGC",
     2)
    ("ToEA6pbmLqZm2HRPEfBMk1WvRni6r6JaJ8ULZkJXQvi1tnNwm7FRri38ihYcL9ouBqx3B4USTRPzS7oDbgSJ6FiXdcKhKD5TPNe5wVGtC3N27Jk",
     "TDt9Egh3tBvjDCGVQBR2WrUNadUw1B21Sa47eRE8PDxAEweyenEn9pDXnWrzH1Rz9YfZu6M3ZGRz2wLcJrJgCHywi1U5y2fVuer31Dcf4Mqgck3",
     0);

TestVector test3 =
  TestVector("4b381541583be4423346c643850da4b320e46a87ae3d2a4e6da11eba819cd4acba45d239319ac14f863b8d5ab5a0d0c64d2e8a1e7d1457df2e5a3c51c73235be")
    ("ToEA6epvY6iUs9r4PkjRBxYPnRsfKhnYQ3WndTQYwT67etN7PiGbkt8Ei4ip5SjxLoLiHdWArHmHZ3hpzHJGr2M4hFqz7yGX9esBG3TbzVdjxDo",
     "TDt9EWvD5T5T44hAZGy5xoVqwGeVUnVyYV6Zi8L9ukLG13e9HPFx3zJdmt3C2EoMv6eXn4wx7J2XDB4bhNnMqvE6Ef9CHr17uNAGrzG7xYqJd6B",
      0x80000000)
    ("ToEA6hC8H4YcYXDQbHppR6stqgdL24PvYtvh2weVkgmxbCHUejQ4LNu6nXWXPAaU59wTV111dFX2PzuKckEcnGHFGW7vo5DQhWMykDpWeoJwYt1",
     "TDt9EZHQpQuajS4Wkp4VBwqLzXQAB97MhLWU7ca6iz26wMZWYQPQdV5VrLpuL19iG3yTEDbN8EpPruh4LvRWR8oqzRLpiyvAeXS5z1Dmuvfnaee",
      0);

static void RunTest(const TestVector &test) {
    std::vector<unsigned char> seed = ParseHex(test.strHexMaster);
    CExtKey key;
    CExtPubKey pubkey;
    key.SetSeed(seed.data(), seed.size());
    pubkey = key.Neuter();
    for (const TestDerivation &derive : test.vDerive) {
        unsigned char data[74];
        key.Encode(data);
        pubkey.Encode(data);

        // Test private key
        
        BOOST_CHECK(EncodeExtKey(key) == derive.prv);
        BOOST_CHECK(DecodeExtKey(derive.prv) == key); //ensure a base58 decoded key also matches
        
        // std::cout << EncodeExtKey(key);
        // Test public key
        BOOST_CHECK(EncodeExtPubKey(pubkey) == derive.pub);
        BOOST_CHECK(DecodeExtPubKey(derive.pub) == pubkey); //ensure a base58 decoded pubkey also matches

        // Derive new keys
        CExtKey keyNew;
        BOOST_CHECK(key.Derive(keyNew, derive.nChild));
        CExtPubKey pubkeyNew = keyNew.Neuter();
        if (!(derive.nChild & 0x80000000)) {
            // Compare with public derivation
            CExtPubKey pubkeyNew2;
            BOOST_CHECK(pubkey.Derive(pubkeyNew2, derive.nChild));
            BOOST_CHECK(pubkeyNew == pubkeyNew2);
        }
        key = keyNew;
        pubkey = pubkeyNew;

        CDataStream ssPub(SER_DISK, CLIENT_VERSION);
        ssPub << pubkeyNew;
        BOOST_CHECK(ssPub.size() == 75);

        CDataStream ssPriv(SER_DISK, CLIENT_VERSION);
        ssPriv << keyNew;
        BOOST_CHECK(ssPriv.size() == 75);

        CExtPubKey pubCheck;
        CExtKey privCheck;
        ssPub >> pubCheck;
        ssPriv >> privCheck;

        BOOST_CHECK(pubCheck == pubkeyNew);
        BOOST_CHECK(privCheck == keyNew);
    }
}

BOOST_FIXTURE_TEST_SUITE(bip32_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(bip32_test1) {
    RunTest(test1);
}

BOOST_AUTO_TEST_CASE(bip32_test2) {
    RunTest(test2);
}

BOOST_AUTO_TEST_CASE(bip32_test3) {
    RunTest(test3);
}

BOOST_AUTO_TEST_SUITE_END()
