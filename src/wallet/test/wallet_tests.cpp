// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2025 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/wallet.h>

#include <algorithm>
#include <memory>
#include <set>
#include <stdint.h>
#include <utility>
#include <vector>

#include <consensus/validation.h>
#include <arith_uint256.h>
#include <bech32.h>
#include <chainparams.h>
#include <key_io.h>
#include <pos/stake.h>
#include <pos/crypto.h>
#include <pos/pqcrypto.h>
#include <pos/vrf.h>
#include <policy/policy.h>
#include <rpc/server.h>
#include <script/pq.h>
#include <test/setup_common.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/staker.h>
#include <wallet/test/wallet_test_fixture.h>

#include <boost/test/unit_test.hpp>
#include <univalue.h>

extern UniValue importmulti(const JSONRPCRequest& request);
extern UniValue dumpwallet(const JSONRPCRequest& request);
extern UniValue importwallet(const JSONRPCRequest& request);

BOOST_FIXTURE_TEST_SUITE(wallet_tests, WalletTestingSetup)

BOOST_FIXTURE_TEST_CASE(pq_address_prefix_metadata_reload, RegTestingSetup)
{
    const CTxDestination destination = PQKeyHash(uint256S("1234"));
    const CTxDestination contact = PQPolicyHash(uint256S("5678"));
    const std::string address = EncodeDestination(destination);
    const std::string contact_address = EncodeDestination(contact);
    const auto previous = [](const std::string& current) {
        const auto decoded = bech32::Decode(current, bech32::Encoding::BECH32M);
        return bech32::Encode("xvgrpq", decoded.second, bech32::Encoding::BECH32M);
    };
    const std::string old_address = previous(address);
    const std::string old_contact = previous(contact_address);
    const fs::path path = GetDataDir() / "pq-prefix-address-book";
    constexpr size_t METADATA_COUNT = 1025;
    {
        auto database = WalletDatabase::Create(path);
        WalletBatch batch(*database, "cr+");
        BOOST_REQUIRE(batch.TxnBegin());
        BOOST_REQUIRE(batch.WriteName(old_address, "Old PQ receive label"));
        BOOST_REQUIRE(batch.WritePurpose(old_address, "receive"));
        BOOST_REQUIRE(batch.WriteDestData(old_address, "note", "old value"));
        BOOST_REQUIRE(batch.WriteDestData(address, "note", "canonical value"));
        BOOST_REQUIRE(batch.WriteName(old_contact, "Old PQ contact label"));
        BOOST_REQUIRE(batch.WriteName(contact_address, "Canonical contact label"));
        BOOST_REQUIRE(batch.WritePurpose(old_contact, "send"));
        for (size_t i = 0; i < METADATA_COUNT; ++i) {
            BOOST_REQUIRE(batch.WriteDestData(old_address, "meta" + std::to_string(i), std::to_string(i)));
        }
        BOOST_REQUIRE(batch.TxnCommit(true));
    }
    for (int reopen = 0; reopen < 3; ++reopen) {
        CWallet wallet("pq-prefix-address-book", WalletDatabase::Create(path));
        bool first_run;
        BOOST_REQUIRE(wallet.LoadWallet(first_run) == DBErrors::LOAD_OK);
        LOCK(wallet.cs_wallet);
        BOOST_CHECK(wallet.mapAddressBook.count(CNoDestination()) == 0);
        const auto& entry = wallet.mapAddressBook.at(destination);
        BOOST_CHECK_EQUAL(entry.name, reopen == 0 ? "Old PQ receive label" : "Updated PQ label");
        BOOST_CHECK_EQUAL(entry.purpose, "receive");
        BOOST_CHECK_EQUAL(entry.destdata.size(), METADATA_COUNT + (reopen == 0 ? 1 : 0));
        for (size_t i = 0; i < METADATA_COUNT; ++i) {
            BOOST_CHECK_EQUAL(entry.destdata.at("meta" + std::to_string(i)), std::to_string(i));
        }
        if (reopen == 0) {
            BOOST_CHECK_EQUAL(entry.destdata.at("note"), "canonical value");
            BOOST_CHECK_EQUAL(wallet.mapAddressBook.at(contact).name, "Canonical contact label");
            BOOST_REQUIRE(wallet.SetAddressBook(destination, "Updated PQ label", "receive"));
            BOOST_REQUIRE(wallet.EraseDestData(destination, "note"));
            BOOST_REQUIRE(wallet.DelAddressBook(contact));
        } else {
            BOOST_CHECK(wallet.mapAddressBook.count(contact) == 0);
        }
        {
            BerkeleyBatch batch(wallet.GetDBHandle());
            BOOST_CHECK(!batch.Exists(std::make_pair(std::string("name"), old_address)));
            BOOST_CHECK(!batch.Exists(std::make_pair(std::string("purpose"), old_address)));
            BOOST_CHECK(!batch.Exists(std::make_pair(std::string("name"), old_contact)));
            BOOST_CHECK(!batch.Exists(std::make_pair(std::string("purpose"), old_contact)));
            BOOST_CHECK(!batch.Exists(std::make_pair(std::string("destdata"), std::make_pair(old_address, std::string("note")))));
        }
        wallet.GetDBHandle().Flush(reopen == 2);
    }
    ReleaseDirectoryLocks();
}

BOOST_FIXTURE_TEST_CASE(pq_first_initialization_and_lazy_classical_keys, RegTestingSetup)
{
    if (!pos::experimental::MLDSA44Available()) return;
    struct RestorePQ { ~RestorePQ() { UpdatePoSPostQuantumForTesting(false); } } restore;
    const auto open = [](const fs::path& path) {
        auto wallet = CWallet::CreateWalletFromFile(path.filename().string(), path);
        if (wallet) UnregisterValidationInterface(wallet.get());
        return wallet;
    };
    const fs::path legacy_path = GetDataDir() / "legacy-first";
    std::set<CKeyID> legacy_keys;
    {
        auto wallet = open(legacy_path);
        BOOST_REQUIRE(wallet);
        LOCK(wallet->cs_wallet);
        BOOST_CHECK(!wallet->IsPQFirst());
        BOOST_CHECK(wallet->IsHDEnabled());
        legacy_keys = wallet->GetKeys();
        BOOST_CHECK(!legacy_keys.empty());
        wallet->GetDBHandle().Flush(false);
    }
    UpdatePoSPostQuantumForTesting(true);
    {
        auto wallet = open(legacy_path);
        BOOST_REQUIRE(wallet);
        LOCK(wallet->cs_wallet);
        BOOST_CHECK(!wallet->IsPQFirst());
        BOOST_CHECK(!wallet->HasPQKeys());
        BOOST_CHECK(wallet->GetKeys() == legacy_keys);
        wallet->GetDBHandle().Flush(true);
    }

    const fs::path path = GetDataDir() / "pq-first";
    const fs::path backup = GetDataDir() / "pq-first-backup";
    const SecureString passphrase{"pq-first-test-passphrase"};
    uint256 id;
    for (int reopen = 0; reopen < 3; ++reopen) {
        auto wallet = open(path);
        BOOST_REQUIRE(wallet);
        LOCK(wallet->cs_wallet);
        BOOST_CHECK(wallet->IsPQFirst());
        BOOST_CHECK(wallet->HasPQKeys());
        BOOST_CHECK(wallet->GetKeys().empty());
        BOOST_CHECK(!wallet->IsHDEnabled());
        BOOST_CHECK_EQUAL(wallet->GetKeyPoolSize(), 0U);
        BOOST_REQUIRE_EQUAL(wallet->mapAddressBook.size(), 1U);
        const CTxDestination dest = wallet->mapAddressBook.begin()->first;
        BOOST_REQUIRE(dest.type() == typeid(PQKeyHash));
        if (reopen == 0) id = boost::get<PQKeyHash>(dest);
        BOOST_CHECK(boost::get<PQKeyHash>(dest) == id);
        BOOST_CHECK_EQUAL(wallet->mapAddressBook.begin()->second.purpose, "receive");
        if (reopen == 0) {
            BOOST_REQUIRE(wallet->EncryptWallet(passphrase));
            BOOST_CHECK(wallet->IsLocked());
        }
        BOOST_CHECK(!wallet->Unlock(SecureString{"wrong-passphrase"}));
        BOOST_REQUIRE(wallet->Unlock(passphrase));
        BOOST_REQUIRE(wallet->TopUpKeyPool());
        BOOST_CHECK(wallet->GetKeys().empty());
        BOOST_CHECK_EQUAL(wallet->GetKeyPoolSize(), 0U);
        if (reopen == 1) BOOST_REQUIRE(wallet->BackupWallet(backup.string()));
        if (reopen == 2) {
            CKeyPool reserved;
            int64_t index;
            BOOST_REQUIRE(wallet->ReserveKeyFromKeyPool(index, reserved, true));
            BOOST_CHECK(reserved.fInternal);
            BOOST_CHECK(wallet->IsHDEnabled());
            BOOST_CHECK_EQUAL(wallet->GetKeys().size(), 3U); // HD seed and one key per branch.
            wallet->KeepKey(index);
            BOOST_REQUIRE(wallet->TopUpKeyPool());
            BOOST_CHECK_EQUAL(wallet->GetKeyPoolSize(), 1U);
        }
        wallet->GetDBHandle().Flush(false);
    }
    {
        auto wallet = open(path);
        BOOST_REQUIRE(wallet);
        LOCK(wallet->cs_wallet);
        BOOST_REQUIRE(wallet->Unlock(passphrase));
        BOOST_CHECK(wallet->IsPQFirst());
        BOOST_CHECK_EQUAL(wallet->GetKeys().size(), 3U);
        BOOST_CHECK_EQUAL(wallet->GetKeyPoolSize(), 1U);
        wallet->GetDBHandle().Flush(true);
    }
    {
        auto wallet = open(backup);
        BOOST_REQUIRE(wallet);
        LOCK(wallet->cs_wallet);
        BOOST_REQUIRE(wallet->Unlock(passphrase));
        BOOST_CHECK(wallet->IsPQFirst());
        BOOST_CHECK(wallet->HavePQKey(id));
        BOOST_CHECK(wallet->GetKeys().empty());
        BOOST_CHECK_EQUAL(wallet->GetKeyPoolSize(), 0U);
        wallet->GetDBHandle().Flush(true);
    }
    ReleaseDirectoryLocks();
}

BOOST_FIXTURE_TEST_CASE(pq_keys_survive_reload_encryption_and_backup, TestingSetup)
{
    if (!pos::experimental::MLDSA44Available()) return;
    const fs::path path = GetDataDir() / "pq-keys-test.dat";
    const fs::path backup = GetDataDir() / "pq-keys-backup.dat";
    const SecureString passphrase{"pq-test-passphrase"};
    uint256 id, encrypted_id;
    std::vector<unsigned char> public_key;
    for (int reopen = 0; reopen < 3; ++reopen) {
        CWallet wallet("pq-keys-test", WalletDatabase::Create(path));
        bool first_run = false;
        BOOST_REQUIRE(wallet.LoadWallet(first_run) == DBErrors::LOAD_OK);
        LOCK(wallet.cs_wallet);
        if (reopen == 0) {
            BOOST_CHECK(first_run);
            BOOST_CHECK(!wallet.HasPQKeys());
            BOOST_REQUIRE(wallet.GeneratePQKey(id));
            BOOST_REQUIRE(wallet.GetPQPubKey(id, public_key));
            BOOST_CHECK_EQUAL(wallet.GetVersion(), FEATURE_PQ_KEYS);
        } else {
            BOOST_CHECK(!first_run);
            BOOST_CHECK_EQUAL(wallet.GetVersion(), FEATURE_PQ_KEYS);
            BOOST_CHECK(wallet.HavePQKey(id));
        }
        BOOST_CHECK(wallet.HasPQKeys());
        pos::experimental::MLDSA44Key loaded;
        if (reopen == 1) {
            BOOST_REQUIRE(wallet.GetPQKey(id, loaded));
            BOOST_REQUIRE(wallet.EncryptWallet(passphrase));
            BOOST_CHECK(wallet.IsLocked());
            BOOST_CHECK(!wallet.GetPQKey(id, loaded));
            BOOST_CHECK(!loaded.IsValid());
        }
        if (reopen > 0) {
            BOOST_CHECK(wallet.IsCrypted());
            BOOST_CHECK(wallet.IsLocked());
            BOOST_CHECK(!wallet.Unlock(SecureString{"wrong-passphrase"}));
            BOOST_REQUIRE(wallet.Unlock(passphrase));
            if (reopen == 1) BOOST_REQUIRE(wallet.GeneratePQKey(encrypted_id));
            BOOST_CHECK(wallet.HavePQKey(encrypted_id));
        }
        BOOST_REQUIRE(wallet.GetPQKey(id, loaded));
        std::vector<unsigned char> derived, signature;
        BOOST_REQUIRE(loaded.GetPublicKey(derived));
        BOOST_CHECK(derived == public_key);
        BOOST_REQUIRE(loaded.Sign({1, 2, 3}, {}, signature));
        BOOST_CHECK(pos::experimental::VerifyMLDSA44(public_key, {1, 2, 3}, {}, signature));
        if (reopen == 2) {
            BOOST_CHECK(!wallet.BackupWallet((path / "wallet.dat").string()));
            BOOST_REQUIRE(wallet.BackupWallet(backup.string()));
        }
        if (reopen == 2) wallet.GetDBHandle().Flush(true);
    }
    {
        CWallet restored("pq-keys-backup", WalletDatabase::Create(backup));
        bool first_run = true;
        BOOST_REQUIRE(restored.LoadWallet(first_run) == DBErrors::LOAD_OK);
        BOOST_CHECK(!first_run);
        BOOST_REQUIRE(restored.Unlock(passphrase));
        pos::experimental::MLDSA44Key loaded;
        BOOST_CHECK(restored.GetPQKey(id, loaded));
        BOOST_CHECK(restored.GetPQKey(encrypted_id, loaded));
        BOOST_CHECK(restored.Lock());
        BOOST_CHECK(restored.HasPQKeys());
        restored.GetDBHandle().Flush(true);
    }
    ReleaseDirectoryLocks();
}

BOOST_FIXTURE_TEST_CASE(pq_wallet_records_reject_corruption_and_future_versions, TestingSetup)
{
    if (!pos::experimental::MLDSA44Available()) return;
    pos::experimental::MLDSA44Key key;
    BOOST_REQUIRE(key.MakeNewKey());
    std::vector<unsigned char> public_key;
    BOOST_REQUIRE(key.GetPublicKey(public_key));
    CKeyingMaterial seed(32, 7);
    CWallet recovered("pq-key-filter", WalletDatabase::CreateDummy());
    CDataStream record_key(SER_DISK, CLIENT_VERSION), record_value(SER_DISK, CLIENT_VERSION);
    BOOST_REQUIRE(key.GetSeed(seed));
    record_key << std::make_pair(std::string("pqkey"), pos::experimental::GetMLDSA44KeyId(public_key));
    record_value << std::make_pair(public_key, seed);
    // Salvage order need not put the minimum-version record first.
    BOOST_REQUIRE(WalletBatch::RecoverKeysOnlyFilter(&recovered, record_key, record_value));
    BOOST_CHECK(recovered.HavePQKey(pos::experimental::GetMLDSA44KeyId(public_key)));
    record_key.clear();
    record_value.clear();
    record_key << std::string("minversion");
    record_value << int(FEATURE_PQ_KEYS);
    BOOST_CHECK(WalletBatch::RecoverKeysOnlyFilter(&recovered, record_key, record_value));
    BOOST_CHECK_EQUAL(recovered.GetVersion(), FEATURE_PQ_KEYS);
    record_key.clear();
    record_value.clear();
    record_key << std::string("pqfirst");
    record_value << uint8_t{1};
    BOOST_CHECK(WalletBatch::RecoverKeysOnlyFilter(&recovered, record_key, record_value));
    {
        LOCK(recovered.cs_wallet);
        BOOST_CHECK(recovered.IsPQFirst());
    }
    record_value.clear();
    record_value << uint8_t{2};
    BOOST_CHECK(!WalletBatch::RecoverKeysOnlyFilter(&recovered, record_key, record_value));
    const fs::path mode_missing_guard = GetDataDir() / "pq-first-missing-guard";
    {
        auto database = WalletDatabase::Create(mode_missing_guard);
        BOOST_REQUIRE(WalletBatch(*database, "cr+").WritePQFirst());
    }
    {
        CWallet rejected("pq-first-missing-guard", WalletDatabase::Create(mode_missing_guard));
        bool first_run;
        BOOST_CHECK(rejected.LoadWallet(first_run) == DBErrors::CORRUPT);
        rejected.GetDBHandle().Flush(true);
    }
    const fs::path missing_guard = GetDataDir() / "pq-missing-guard";
    {
        auto database = WalletDatabase::Create(missing_guard);
        WalletBatch batch(*database, "cr+");
        BOOST_REQUIRE(batch.WritePQKey(public_key, seed));
        BOOST_CHECK(!batch.WritePQKey(public_key, seed));
    }
    {
        CWallet rejected("pq-missing-guard", WalletDatabase::Create(missing_guard));
        bool first_run;
        BOOST_CHECK(rejected.LoadWallet(first_run) == DBErrors::CORRUPT);
        rejected.GetDBHandle().Flush(true);
    }
    seed[0] ^= 1;
    const fs::path path = GetDataDir() / "pq-corrupt-test.dat";
    {
        auto database = WalletDatabase::Create(path);
        WalletBatch batch(*database, "cr+");
        // A valid-length record with mismatched seed must be fatal, not skipped.
        BOOST_REQUIRE(batch.WriteMinVersion(FEATURE_PQ_KEYS));
        BOOST_REQUIRE(batch.WritePQKey(public_key, seed));
    }
    {
        CWallet rejected("pq-corrupt", WalletDatabase::Create(path));
        bool first_run;
        BOOST_CHECK(rejected.LoadWallet(first_run) == DBErrors::CORRUPT);
        rejected.GetDBHandle().Flush(false);
    }
    {
        auto database = WalletDatabase::Create(path);
        BOOST_REQUIRE(WalletBatch(*database).WriteMinVersion(CLIENT_VERSION + 1));
    }
    {
        CWallet rejected("pq-too-new", WalletDatabase::Create(path));
        bool first_run;
        BOOST_CHECK(rejected.LoadWallet(first_run) == DBErrors::TOO_NEW);
        rejected.GetDBHandle().Flush(true);
    }
    CWallet dummy("pq-failed-write", WalletDatabase::CreateDummy());
    uint256 id = uint256S("1234");
    BOOST_CHECK(!dummy.GeneratePQKey(id));
    BOOST_CHECK(id.IsNull());
    ReleaseDirectoryLocks();
}

BOOST_FIXTURE_TEST_CASE(pq_multisig_persistence_backup_and_corruption, TestingSetup)
{
    if (!pos::experimental::MLDSA44Available()) return;
    const fs::path path = GetDataDir() / "pq-policy-wallet";
    const fs::path backup = GetDataDir() / "pq-policy-backup";
    const SecureString passphrase{"policy-test"};
    PQMultisigPolicy policy;
    policy.required = 2;
    CScript script;
    std::vector<unsigned char> encoded;
    uint256 id;
    for (int reopen = 0; reopen < 3; ++reopen) {
        CWallet wallet("pq-policy-test", WalletDatabase::Create(path));
        bool first;
        BOOST_REQUIRE(wallet.LoadWallet(first) == DBErrors::LOAD_OK);
        LOCK(wallet.cs_wallet);
        if (reopen == 0) {
            uint256 owned;
            BOOST_REQUIRE(wallet.GeneratePQKey(owned));
            policy.keys = {owned, uint256S("01"), uint256S("02")};
            std::sort(policy.keys.begin(), policy.keys.end());
            encoded = EncodePQMultisigPolicy(policy);
            id = GetPQMultisigPolicyId(policy);
            script = GetPQMultisigScript(id);
            BOOST_REQUIRE(wallet.AddPQMultisigPolicy(encoded));
            BOOST_CHECK(wallet.AddPQMultisigPolicy(encoded));
        }
        std::vector<unsigned char> loaded;
        BOOST_REQUIRE(wallet.GetPQMultisigPolicy(id, loaded));
        BOOST_CHECK(loaded == encoded);
        BOOST_CHECK_EQUAL(wallet.GetVersion(), FEATURE_PQ_KEYS);
        BOOST_CHECK_EQUAL(IsMine(wallet, script), ISMINE_WATCH_ONLY);
        BOOST_CHECK(wallet.HaveWatchOnly(script));
        BOOST_CHECK(IsSolvable(wallet, script));
        if (reopen == 0) BOOST_REQUIRE(wallet.EncryptWallet(passphrase));
        BOOST_CHECK(wallet.IsLocked());
        BOOST_REQUIRE(wallet.Unlock(passphrase));
        BOOST_REQUIRE(wallet.GetPQMultisigPolicy(id, loaded));
        if (reopen == 2) BOOST_REQUIRE(wallet.BackupWallet(backup.string()));
        if (reopen == 2) wallet.GetDBHandle().Flush(true);
    }
    {
        CWallet restored("pq-policy-backup", WalletDatabase::Create(backup));
        bool first;
        BOOST_REQUIRE(restored.LoadWallet(first) == DBErrors::LOAD_OK);
        std::vector<unsigned char> loaded;
        BOOST_REQUIRE(restored.GetPQMultisigPolicy(id, loaded));
        BOOST_CHECK(loaded == encoded);
        BOOST_CHECK_EQUAL(IsMine(restored, script), ISMINE_WATCH_ONLY);
        BOOST_REQUIRE(restored.Unlock(passphrase));
        restored.GetDBHandle().Flush(true);
    }
    CWallet recovered("policy-salvage", WalletDatabase::CreateDummy());
    CDataStream key(SER_DISK, CLIENT_VERSION), value(SER_DISK, CLIENT_VERSION);
    key << std::make_pair(std::string("pqpolicy"), id);
    value << encoded;
    BOOST_REQUIRE(WalletBatch::RecoverKeysOnlyFilter(&recovered, key, value));
    BOOST_CHECK_EQUAL(IsMine(recovered, script), ISMINE_WATCH_ONLY);
    encoded[0] = 2;
    value.clear();
    value << encoded;
    BOOST_CHECK(!WalletBatch::RecoverKeysOnlyFilter(&recovered, key, value));
    ReleaseDirectoryLocks();
}

BOOST_FIXTURE_TEST_CASE(pq_text_export_refuses_incomplete_backup, TestingSetup)
{
    if (!pos::experimental::MLDSA44Available()) return;
    auto wallet = std::make_shared<CWallet>("pq-export-test",
        WalletDatabase::Create(GetDataDir() / "pq-export-wallet"));
    bool first_run;
    BOOST_REQUIRE(wallet->LoadWallet(first_run) == DBErrors::LOAD_OK);
    uint256 id;
    BOOST_REQUIRE(wallet->GeneratePQKey(id));
    const fs::path output = GetDataDir() / "incomplete-pq-export.txt";
    JSONRPCRequest request;
    request.params.setArray();
    request.params.push_back(output.string());
    BOOST_REQUIRE(AddWallet(wallet));
    BOOST_CHECK_THROW(::dumpwallet(request), UniValue);
    BOOST_CHECK(!fs::exists(output));
    BOOST_CHECK(RemoveWallet(wallet));
    wallet->GetDBHandle().Flush(true);
    ReleaseDirectoryLocks();
}

BOOST_AUTO_TEST_CASE(staking_attempts_follow_wallet_lifetime)
{
    for (int generation = 0; generation < 2; ++generation) {
        CWallet wallet("attempt-test", WalletDatabase::CreateDummy());
        LOCK(wallet.cs_wallet);
        BOOST_CHECK(wallet.MarkStakingAttempt(0));
        BOOST_CHECK(!wallet.MarkStakingAttempt(0));
        BOOST_CHECK(wallet.MarkStakingAttempt(0, true));
        BOOST_CHECK(!wallet.MarkStakingAttempt(0));
        BOOST_CHECK(wallet.MarkStakingAttempt(1));
        BOOST_CHECK(wallet.MarkAutomaticBondAttempt(100));
        BOOST_CHECK(!wallet.MarkAutomaticBondAttempt(159));
        BOOST_CHECK(wallet.MarkAutomaticBondAttempt(160));
    }
}

BOOST_AUTO_TEST_CASE(staking_signing_journal_survives_wallet_reload)
{
    const fs::path path = GetDataDir() / "pos-signing-test.dat";
    const Consensus::Params& params = Params().GetConsensus();
    unsigned char secret[32]{};
    secret[31] = 3;
    pos::StakeProof proof;
    proof.bond_outpoint = COutPoint(uint256S("1234"), 0);
    proof.slot = 100;
    BOOST_REQUIRE(pos::GetSchnorrPublicKey(secret, proof.signing_public_key));
    pos::BlockAuthorization authorization;
    authorization.network_id = params.nPoSNetworkId;
    authorization.bond_outpoint = proof.bond_outpoint;
    authorization.slot = proof.slot;
    authorization.candidate_header_hash = uint256S("1111");
    pos::CheckpointVote vote;
    vote.bond_outpoint = proof.bond_outpoint;
    vote.source_epoch = pos::FINAL_POW_CHECKPOINT_EPOCH;
    vote.source_checkpoint_root = uint256S("2222");
    vote.target_epoch = 3;
    vote.target_checkpoint_root = uint256S("3333");
    vote.head_slot = 99;
    vote.head_block_root = uint256S("4444");

    for (int reopen = 0; reopen < 2; ++reopen) {
        CWallet wallet("signing-test", WalletDatabase::Create(path));
        bool first_run;
        BOOST_REQUIRE(wallet.LoadWallet(first_run) == DBErrors::LOAD_OK);
        LOCK(wallet.cs_wallet);
        BOOST_CHECK(wallet.RecordPoSBlockSigning(params, proof, authorization));
        BOOST_CHECK(wallet.RecordPoSVoteSigning(params, vote, proof.signing_public_key));
        auto conflict = authorization;
        conflict.candidate_header_hash = uint256S("9999");
        BOOST_CHECK(!wallet.RecordPoSBlockSigning(params, proof, conflict));
        auto older_proof = proof;
        auto older_authorization = authorization;
        --older_proof.slot;
        --older_authorization.slot;
        BOOST_CHECK(!wallet.RecordPoSBlockSigning(params, older_proof, older_authorization));
        auto conflicting_vote = vote;
        conflicting_vote.target_checkpoint_root = uint256S("aaaa");
        BOOST_CHECK(!wallet.RecordPoSVoteSigning(params, conflicting_vote, proof.signing_public_key));
        conflicting_vote = vote;
        conflicting_vote.source_epoch = 1;
        conflicting_vote.target_epoch = 2;
        BOOST_CHECK(!wallet.RecordPoSVoteSigning(params, conflicting_vote, proof.signing_public_key));
    }

    CWallet wallet("signing-test", WalletDatabase::Create(path));
    bool first_run;
    BOOST_REQUIRE(wallet.LoadWallet(first_run) == DBErrors::LOAD_OK);
    LOCK(wallet.cs_wallet);
    // The same checkpoint link may update its head at a later slot.
    ++vote.head_slot;
    vote.head_block_root = uint256S("5555");
    BOOST_CHECK(wallet.RecordPoSVoteSigning(params, vote, proof.signing_public_key));
    auto advance = vote;
    advance.source_epoch = 2;
    advance.source_checkpoint_root = uint256S("6666");
    advance.target_epoch = 4;
    advance.target_checkpoint_root = uint256S("7777");
    BOOST_CHECK(wallet.RecordPoSVoteSigning(params, advance, proof.signing_public_key));
    vote.target_epoch = 5;
    ++vote.head_slot;
    BOOST_CHECK(!wallet.RecordPoSVoteSigning(params, vote, proof.signing_public_key));
    // Histories are scoped to the network, activation and individual bond.
    auto other_params = params;
    ++other_params.nPoSNetworkId;
    BOOST_CHECK(wallet.RecordPoSVoteSigning(other_params, vote, proof.signing_public_key));
    ++vote.bond_outpoint.n;
    BOOST_CHECK(wallet.RecordPoSVoteSigning(params, vote, proof.signing_public_key));
    auto transition = vote;
    transition.bond_outpoint.n += 10;
    transition.source_epoch = pos::FINAL_POW_CHECKPOINT_EPOCH;
    BOOST_REQUIRE(wallet.RecordPoSVoteSigning(params, transition, proof.signing_public_key));
    auto replacement_anchor = transition;
    replacement_anchor.source_checkpoint_root = uint256S("abcd");
    ++replacement_anchor.head_slot;
    BOOST_CHECK(!wallet.RecordPoSVoteSigning(params, replacement_anchor, proof.signing_public_key));
    ++replacement_anchor.target_epoch;
    replacement_anchor.target_checkpoint_root = uint256S("bcde");
    BOOST_CHECK(wallet.RecordPoSVoteSigning(params, replacement_anchor, proof.signing_public_key));
    BOOST_CHECK(!wallet.RecordPoSVoteSigning(params, transition, proof.signing_public_key));
    PoSSigningKey corrupt_key;
    corrupt_key.domain = uint256S("abcd");
    corrupt_key.bond_outpoint = vote.bond_outpoint;
    corrupt_key.public_key = uint256S("1234");
    PoSSigningHistory corrupt_history;
    corrupt_history.version = 2;
    BOOST_REQUIRE(WalletBatch(wallet.GetDBHandle()).WritePoSSigningHistory(corrupt_key, corrupt_history));
    CWallet rejected("corrupt-journal", WalletDatabase::CreateDummy());
    BOOST_CHECK(WalletBatch(wallet.GetDBHandle()).LoadWallet(&rejected) == DBErrors::CORRUPT);
    wallet.GetDBHandle().Flush(true);
    ReleaseDirectoryLocks();
}

BOOST_AUTO_TEST_CASE(staking_signing_journal_fails_closed)
{
    CWallet wallet("journal-failure", WalletDatabase::CreateDummy());
    LOCK(wallet.cs_wallet);
    pos::CheckpointVote vote;
    vote.bond_outpoint = COutPoint(uint256S("12"), 0);
    vote.source_checkpoint_root = uint256S("23");
    vote.target_epoch = 1;
    vote.target_checkpoint_root = uint256S("34");
    vote.head_slot = 1;
    vote.head_block_root = uint256S("45");
    unsigned char public_key[32]{};
    public_key[0] = 1;
    BOOST_CHECK_THROW(wallet.RecordPoSVoteSigning(Params().GetConsensus(), vote, public_key), std::runtime_error);
    BOOST_CHECK_THROW(wallet.RecordPoSVoteSigning(Params().GetConsensus(), vote, public_key), std::runtime_error);

    PoSSigningKey key;
    key.domain = uint256S("12");
    key.bond_outpoint = vote.bond_outpoint;
    key.public_key = uint256S("34");
    PoSSigningHistory history;
    BOOST_CHECK(!wallet.LoadPoSSigningHistory(key, history));
    history.have_vote = true;
    history.vote = vote;
    BOOST_CHECK(wallet.LoadPoSSigningHistory(key, history));
    BOOST_CHECK(!wallet.LoadPoSSigningHistory(key, history));
    history.version = 2;
    BOOST_CHECK(!wallet.LoadPoSSigningHistory(key, history));
}

static void AddKey(CWallet& wallet, const CKey& key)
{
    LOCK(wallet.cs_wallet);
    wallet.AddKeyPubKey(key, key.GetPubKey());
}

BOOST_FIXTURE_TEST_CASE(rescan, TestChain100Setup)
{
    // Cap last block file size, and mine new block in a new block file.
    CBlockIndex* const nullBlock = nullptr;
    CBlockIndex* oldTip = chainActive.Tip();
    GetBlockFileInfo(oldTip->GetBlockPos().nFile)->nSize = MAX_BLOCKFILE_SIZE;
    CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
    CBlockIndex* newTip = chainActive.Tip();

    LOCK(cs_main);

    // Verify ScanForWalletTransactions picks up transactions in both the old
    // and new block files.
    {
        CWallet wallet("dummy", WalletDatabase::CreateDummy());
        AddKey(wallet, coinbaseKey);
        WalletRescanReserver reserver(&wallet);
        reserver.reserve();
        BOOST_CHECK_EQUAL(nullBlock, wallet.ScanForWalletTransactions(oldTip, nullptr, reserver));
        BOOST_CHECK_EQUAL(wallet.GetImmatureBalance(), 400000 * COIN);
    }

    // Prune the older block file.
    PruneOneBlockFile(oldTip->GetBlockPos().nFile);
    UnlinkPrunedFiles({oldTip->GetBlockPos().nFile});

    // Verify ScanForWalletTransactions only picks transactions in the new block
    // file.
    {
        CWallet wallet("dummy", WalletDatabase::CreateDummy());
        AddKey(wallet, coinbaseKey);
        WalletRescanReserver reserver(&wallet);
        reserver.reserve();
        BOOST_CHECK_EQUAL(oldTip, wallet.ScanForWalletTransactions(oldTip, nullptr, reserver));
        BOOST_CHECK_EQUAL(wallet.GetImmatureBalance(), 200000 * COIN);
    }

    // Verify importmulti RPC returns failure for a key whose creation time is
    // before the missing block, and success for a key whose creation time is
    // after.
    {
        std::shared_ptr<CWallet> wallet = std::make_shared<CWallet>("dummy", WalletDatabase::CreateDummy());
        AddWallet(wallet);
        UniValue keys;
        keys.setArray();
        UniValue key;
        key.setObject();
        key.pushKV("scriptPubKey", HexStr(GetScriptForRawPubKey(coinbaseKey.GetPubKey())));
        key.pushKV("timestamp", 0);
        key.pushKV("internal", UniValue(true));
        keys.push_back(key);
        key.clear();
        key.setObject();
        CKey futureKey;
        futureKey.MakeNewKey(true);
        key.pushKV("scriptPubKey", HexStr(GetScriptForRawPubKey(futureKey.GetPubKey())));
        key.pushKV("timestamp", newTip->GetBlockTimeMax() + TIMESTAMP_WINDOW + 1);
        key.pushKV("internal", UniValue(true));
        keys.push_back(key);
        JSONRPCRequest request;
        request.params.setArray();
        request.params.push_back(keys);

        UniValue response = importmulti(request);
        BOOST_CHECK_EQUAL(response.write(),
            strprintf("[{\"success\":false,\"error\":{\"code\":-1,\"message\":\"Rescan failed for key with creation "
                      "timestamp %d. There was an error reading a block from time %d, which is after or within %d "
                      "seconds of key creation, and could contain transactions pertaining to the key. As a result, "
                      "transactions and coins using this key may not appear in the wallet. This error could be caused "
                      "by pruning or data corruption (see verged log for details) and could be dealt with by "
                      "downloading and rescanning the relevant blocks (see -reindex and -rescan "
                      "options).\"}},{\"success\":true}]",
                              0, oldTip->GetBlockTimeMax(), TIMESTAMP_WINDOW));
        RemoveWallet(wallet);
    }
}

// Verify importwallet RPC starts rescan at earliest block with timestamp
// greater or equal than key birthday. Previously there was a bug where
// importwallet RPC would start the scan at the latest block with timestamp less
// than or equal to key birthday.
BOOST_FIXTURE_TEST_CASE(importwallet_rescan, TestChain100Setup)
{
    // Create two blocks with same timestamp to verify that importwallet rescan
    // will pick up both blocks, not just the first.
    const int64_t BLOCK_TIME = chainActive.Tip()->GetBlockTimeMax() + 5;
    SetMockTime(BLOCK_TIME);
    m_coinbase_txns.emplace_back(CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey())).vtx[0]);
    m_coinbase_txns.emplace_back(CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey())).vtx[0]);

    // Set key birthday to block time increased by the timestamp window, so
    // rescan will start at the block time.
    const int64_t KEY_TIME = BLOCK_TIME + TIMESTAMP_WINDOW;
    SetMockTime(KEY_TIME);
    m_coinbase_txns.emplace_back(CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey())).vtx[0]);

    LOCK(cs_main);
	
	std::string backup_file = (SetDataDir("importwallet_rescan") / "wallet.backup").string();

    // Import key into wallet and call dumpwallet to create backup file.
    {
        std::shared_ptr<CWallet> wallet = std::make_shared<CWallet>("dummy", WalletDatabase::CreateDummy());
        LOCK(wallet->cs_wallet);
        wallet->mapKeyMetadata[coinbaseKey.GetPubKey().GetID()].nCreateTime = KEY_TIME;
        wallet->AddKeyPubKey(coinbaseKey, coinbaseKey.GetPubKey());

        JSONRPCRequest request;
        request.params.setArray();
        request.params.push_back(backup_file);
        AddWallet(wallet);
        ::dumpwallet(request);
        RemoveWallet(wallet);
    }

    // Call importwallet RPC and verify all blocks with timestamps >= BLOCK_TIME
    // were scanned, and no prior blocks were scanned.
    {
        std::shared_ptr<CWallet> wallet = std::make_shared<CWallet>("dummy", WalletDatabase::CreateDummy());

        JSONRPCRequest request;
        request.params.setArray();
        request.params.push_back(backup_file);
        AddWallet(wallet);
        ::importwallet(request);
        RemoveWallet(wallet);

        LOCK(wallet->cs_wallet);
        BOOST_CHECK_EQUAL(wallet->mapWallet.size(), 3U);
        BOOST_CHECK_EQUAL(m_coinbase_txns.size(), 123U);
        for (size_t i = 0; i < m_coinbase_txns.size(); ++i) {
            bool found = wallet->GetWalletTx(m_coinbase_txns[i]->GetHash());
            bool expected = i >= 120;
            BOOST_CHECK_EQUAL(found, expected);
        }
    }

    SetMockTime(0);
}

// Check that GetImmatureCredit() returns a newly calculated value instead of
// the cached value after a MarkDirty() call.
//
// This is a regression test written to verify a bugfix for the immature credit
// function. Similar tests probably should be written for the other credit and
// debit functions.
BOOST_FIXTURE_TEST_CASE(coin_mark_dirty_immature_credit, TestChain100Setup)
{
    CWallet wallet("dummy", WalletDatabase::CreateDummy());
    CWalletTx wtx(&wallet, m_coinbase_txns.back());
    LOCK2(cs_main, wallet.cs_wallet);
    wtx.hashBlock = chainActive.Tip()->GetBlockHash();
    wtx.nIndex = 0;

    // Call GetImmatureCredit() once before adding the key to the wallet to
    // cache the current immature credit amount, which is 0.
    BOOST_CHECK_EQUAL(wtx.GetImmatureCredit(), 0);

    // Invalidate the cached value, add the key, and make sure a new immature
    // credit amount is calculated.
    wtx.MarkDirty();
    wallet.AddKeyPubKey(coinbaseKey, coinbaseKey.GetPubKey());
    BOOST_CHECK_EQUAL(wtx.GetImmatureCredit(), 200000*COIN);
}

static int64_t AddTx(CWallet& wallet, uint32_t lockTime, int64_t mockTime, int64_t blockTime)
{
    CMutableTransaction tx;
    tx.nLockTime = lockTime;
    SetMockTime(mockTime);
    CBlockIndex* block = nullptr;
    if (blockTime > 0) {
        LOCK(cs_main);
        auto inserted = mapBlockIndex.emplace(GetRandHash(), new CBlockIndex);
        assert(inserted.second);
        const uint256& hash = inserted.first->first;
        block = inserted.first->second;
        block->nTime = blockTime;
        block->phashBlock = &hash;
    }
    
    CWalletTx wtx(&wallet, MakeTransactionRef(tx));

    if (block) {
        wtx.SetMerkleBranch(block, 0);
    }

    {
        LOCK(cs_main);
        wallet.AddToWallet(wtx);
    }
    LOCK(wallet.cs_wallet);
    return wallet.mapWallet.at(wtx.GetHash()).nTimeSmart;
}

// Simple test to verify assignment of CWalletTx::nSmartTime value. Could be
// expanded to cover more corner cases of smart time logic.
BOOST_AUTO_TEST_CASE(ComputeTimeSmart)
{
    // New transaction should use clock time if lower than block time.
    BOOST_CHECK_EQUAL(AddTx(m_wallet, 1, 100, 120), 100);

    // Test that updating existing transaction does not change smart time.
    BOOST_CHECK_EQUAL(AddTx(m_wallet, 1, 100, 220), 100);

    // New transaction should use clock time if there's no block time.
    BOOST_CHECK_EQUAL(AddTx(m_wallet, 2, 300, 0), 300);

    // New transaction should use block time if lower than clock time.
    BOOST_CHECK_EQUAL(AddTx(m_wallet, 3, 420, 400), 400);

    // New transaction should use latest entry time if higher than
    // min(block time, clock time).
    BOOST_CHECK_EQUAL(AddTx(m_wallet, 4, 500, 390), 400);

    // If there are future entries, new transaction should use time of the
    // newest entry that is no more than 300 seconds ahead of the clock time.
    BOOST_CHECK_EQUAL(AddTx(m_wallet, 5, 50, 600), 300);

    // Reset mock time for other tests.
    SetMockTime(0);
}

BOOST_AUTO_TEST_CASE(LoadReceiveRequests)
{
    CTxDestination dest = CKeyID();
    LOCK(m_wallet.cs_wallet);
    m_wallet.AddDestData(dest, "misc", "val_misc");
    m_wallet.AddDestData(dest, "rr0", "val_rr0");
    m_wallet.AddDestData(dest, "rr1", "val_rr1");

    auto values = m_wallet.GetDestValues("rr");
    BOOST_CHECK_EQUAL(values.size(), 2U);
    BOOST_CHECK_EQUAL(values[0], "val_rr0");
    BOOST_CHECK_EQUAL(values[1], "val_rr1");
}

class ListCoinsTestingSetup : public TestChain100Setup
{
public:
    ListCoinsTestingSetup()
    {
        CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
        wallet = MakeUnique<CWallet>("mock", WalletDatabase::CreateMock());
        bool firstRun;
        wallet->LoadWallet(firstRun);
        AddKey(*wallet, coinbaseKey);
        WalletRescanReserver reserver(wallet.get());
        reserver.reserve();
        wallet->ScanForWalletTransactions(chainActive.Genesis(), nullptr, reserver);
    }

    ~ListCoinsTestingSetup()
    {
        wallet.reset();
    }

    CWalletTx& AddTx(CRecipient recipient)
    {
        CTransactionRef tx;
        CReserveKey reservekey(wallet.get());
        CAmount fee;
        int changePos = -1;
        std::string error;
        CCoinControl dummy;
        BOOST_CHECK(wallet->CreateTransaction({recipient}, tx, reservekey, fee, changePos, error, dummy));
        CValidationState state;
        BOOST_CHECK(wallet->CommitTransaction(tx, {}, {}, {}, reservekey, nullptr, state));
        CMutableTransaction blocktx;
        {
            LOCK(wallet->cs_wallet);
            blocktx = CMutableTransaction(*wallet->mapWallet.at(tx->GetHash()).tx);
        }
        CreateAndProcessBlock({CMutableTransaction(blocktx)}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
        LOCK(wallet->cs_wallet);
        auto it = wallet->mapWallet.find(tx->GetHash());
        BOOST_CHECK(it != wallet->mapWallet.end());
        it->second.SetMerkleBranch(chainActive.Tip(), 1);
        return it->second;
    }

    std::unique_ptr<CWallet> wallet;
};

BOOST_FIXTURE_TEST_CASE(pq_balance_breakdown_and_legacy_migration, ListCoinsTestingSetup)
{
    if (!pos::experimental::MLDSA44Available()) return;
    UpdatePoSPostQuantumForTesting(true);
    struct RestorePQ { ~RestorePQ() { UpdatePoSPostQuantumForTesting(false); } } restore;
    LOCK2(cs_main, wallet->cs_wallet);
    const auto before = wallet->GetProtectionBalances();
    BOOST_CHECK_EQUAL(before.legacy.available, wallet->GetBalance());
    BOOST_CHECK_EQUAL(before.legacy.immature, wallet->GetImmatureBalance());
    BOOST_CHECK_EQUAL(before.pq.Total(), 0);
    uint256 pq_id;
    BOOST_REQUIRE(wallet->GeneratePQKey(pq_id));
    const CScript legacy = GetScriptForDestination(coinbaseKey.GetPubKey().GetID());
    pos::BondData bond;
    bond.version = 2;
    BOOST_REQUIRE(pos::GetSchnorrPublicKey(coinbaseKey.begin(), bond.signing_public_key));
    bond.pq_signing_key_id = bond.pq_reward_key_id = bond.pq_withdrawal_key_id = pq_id;
    pos::UnbondData unbond;
    unbond.version = 2;
    unbond.unlock_height = chainActive.Height() + Params().GetConsensus().nPoSUnbondingBlocks;
    unbond.pq_withdrawal_key_id = pq_id;
    CKey watch;
    watch.MakeNewKey(true);
    const CScript watch_script = GetScriptForDestination(watch.GetPubKey().GetID());
    BOOST_REQUIRE(wallet->AddWatchOnly(watch_script, 0));
    CMutableTransaction funding;
    funding.vin.emplace_back(COutPoint(uint256S("ca11"), 0));
    for (size_t i = 0; i < 101; ++i) funding.vout.emplace_back(1000 * COIN, legacy);
    funding.vout.emplace_back(50000 * COIN, GetPQScript(pq_id));
    funding.vout.emplace_back(2000 * COIN, pos::GetBondScript(bond));
    funding.vout.emplace_back(3000 * COIN, pos::GetUnbondScript(unbond));
    funding.vout.emplace_back(4000 * COIN, watch_script);
    CWalletTx wtx(wallet.get(), MakeTransactionRef(funding));
    wtx.SetMerkleBranch(chainActive.Tip(), 1);
    BOOST_REQUIRE(wallet->AddToWallet(wtx));
    wallet->LockCoin(COutPoint(wtx.GetHash(), 0));
    const auto balances = wallet->GetProtectionBalances();
    BOOST_CHECK_EQUAL(balances.legacy.available, before.legacy.available + 101000 * COIN);
    BOOST_CHECK_EQUAL(balances.legacy.immature, before.legacy.immature);
    BOOST_CHECK_EQUAL(balances.pq.available, 50000 * COIN);
    BOOST_CHECK_EQUAL(balances.pq.staked, 5000 * COIN);
    BOOST_CHECK_EQUAL(balances.pq.Total(), 55000 * COIN);
    BOOST_CHECK_EQUAL(wallet->GetBalance(ISMINE_WATCH_ONLY), 4000 * COIN);

    CReserveKey reserve(wallet.get());
    CTransactionRef tx;
    WalletPQMigration migration;
    std::string error;
    BOOST_REQUIRE_MESSAGE(wallet->CreateLegacyPQMigration(tx, reserve, migration, error), error);
    BOOST_REQUIRE(tx);
    BOOST_CHECK_EQUAL(tx->vin.size(), 100U);
    BOOST_CHECK_EQUAL(migration.inputs, 100U);
    BOOST_CHECK_EQUAL(migration.remaining, 1000 * COIN);
    BOOST_CHECK_EQUAL(migration.amount + migration.fee, before.legacy.available + 99000 * COIN);
    BOOST_CHECK(migration.fee > 0 && migration.fee <= maxTxFee);
    BOOST_REQUIRE_EQUAL(tx->vout.size(), 1U);
    BOOST_CHECK_EQUAL(tx->vout[0].nValue, migration.amount);
    uint256 destination;
    BOOST_REQUIRE(ExtractPQKeyId(tx->vout[0].scriptPubKey, destination));
    BOOST_CHECK(wallet->HavePQKey(destination));
    BOOST_CHECK(destination != pq_id);
    BOOST_CHECK_EQUAL(migration.address, EncodeDestination(PQKeyHash(destination)));
    BOOST_CHECK(wallet->mapAddressBook.at(PQKeyHash(destination)).purpose == "receive");
    BOOST_CHECK(wallet->mapWallet.count(tx->GetHash()) == 0); // Preparation must not send.
    for (const auto& input : tx->vin) {
        BOOST_CHECK(!wallet->IsLockedCoin(input.prevout.hash, input.prevout.n));
        const auto& prev = wallet->mapWallet.at(input.prevout.hash);
        BOOST_CHECK(prev.GetDepthInMainChain() > 0);
        uint256 input_id;
        pos::BondData parsed;
        BOOST_CHECK(!ExtractPQKeyId(prev.tx->vout[input.prevout.n].scriptPubKey, input_id));
        BOOST_CHECK(!pos::ParseBondScript(prev.tx->vout[input.prevout.n].scriptPubKey, parsed));
    }

    // Locked coins, PQ coins and staking principal cannot fund a second sweep.
    for (const auto& input : tx->vin) wallet->LockCoin(input.prevout);
    for (size_t i = 0; i < 101; ++i) wallet->LockCoin(COutPoint(wtx.GetHash(), i));
    BOOST_CHECK(!wallet->CreateLegacyPQMigration(tx, reserve, migration, error));
    BOOST_CHECK(!tx);
    BOOST_CHECK(error.find("No confirmed") != std::string::npos);
    BOOST_REQUIRE(wallet->EncryptWallet(SecureString{"migration-test"}));
    BOOST_CHECK(!wallet->CreateLegacyPQMigration(tx, reserve, migration, error));
    BOOST_CHECK(error.find("Unlock") != std::string::npos);
}

BOOST_FIXTURE_TEST_CASE(pq_multisig_owned_balance_and_wallet_spending, ListCoinsTestingSetup)
{
    if (!pos::experimental::MLDSA44Available()) return;
    UpdatePoSPostQuantumForTesting(true);
    struct RestorePQ { ~RestorePQ() { UpdatePoSPostQuantumForTesting(false); } } restore;
    LOCK2(cs_main, wallet->cs_wallet);
    uint256 id;
    BOOST_REQUIRE(wallet->GeneratePQKey(id));
    PQMultisigPolicy policy{1, {id, uint256S("01")}};
    std::sort(policy.keys.begin(), policy.keys.end());
    BOOST_REQUIRE(wallet->AddPQMultisigPolicy(EncodePQMultisigPolicy(policy)));
    const CScript script = GetPQMultisigScript(GetPQMultisigPolicyId(policy));
    BOOST_CHECK_EQUAL(IsMine(*wallet, script), ISMINE_SPENDABLE);
    CMutableTransaction funding;
    funding.vin.emplace_back(COutPoint(uint256S("babb"), 0));
    funding.vout.emplace_back(100 * COIN, script);
    CWalletTx wtx(wallet.get(), MakeTransactionRef(funding));
    wtx.SetMerkleBranch(chainActive.Tip(), 1);
    BOOST_REQUIRE(wallet->AddToWallet(wtx));
    BOOST_CHECK_EQUAL(wallet->GetProtectionBalances().pq.available, 100 * COIN);
    CCoinControl control;
    control.Select(COutPoint(wtx.GetHash(), 0));
    CReserveKey reserve(wallet.get());
    CTransactionRef tx;
    CAmount fee;
    int change = -1;
    std::string error;
    BOOST_REQUIRE_MESSAGE(wallet->CreateTransaction({CRecipient{GetPQScript(id), 10 * COIN, false}},
        tx, reserve, fee, change, error, control), error);
    BOOST_REQUIRE_EQUAL(tx->vin.size(), 1U);
    BOOST_CHECK(tx->vin[0].prevout == COutPoint(wtx.GetHash(), 0));
    BOOST_CHECK_EQUAL(tx->vin[0].scriptSig.size(), GetPQMultisigMaximumScriptSigSize(policy));
    BOOST_CHECK(VerifyScript(tx->vin[0].scriptSig, script, nullptr, STANDARD_SCRIPT_VERIFY_FLAGS | SCRIPT_VERIFY_PQ,
        TransactionSignatureChecker(tx.get(), 0, 100 * COIN, ReplayProtectionContext(3, 0))));
    CAmount output_value = 0;
    for (const auto& output : tx->vout) output_value += output.nValue;
    BOOST_CHECK_EQUAL(output_value + fee, 100 * COIN);
}

BOOST_FIXTURE_TEST_CASE(pq_balance_pending_and_migration_activation, ListCoinsTestingSetup)
{
    if (!pos::experimental::MLDSA44Available()) return;
    UpdatePoSPostQuantumForTesting(true);
    struct RestorePQ { ~RestorePQ() { UpdatePoSPostQuantumForTesting(false); } } restore;
    LOCK2(cs_main, wallet->cs_wallet);
    uint256 id;
    BOOST_REQUIRE(wallet->GeneratePQKey(id));
    pos::BondData bond;
    bond.version = 2;
    BOOST_REQUIRE(pos::GetSchnorrPublicKey(coinbaseKey.begin(), bond.signing_public_key));
    bond.pq_signing_key_id = bond.pq_reward_key_id = bond.pq_withdrawal_key_id = id;
    CMutableTransaction funding;
    funding.vin.emplace_back(COutPoint(uint256S("ca12"), 0));
    funding.vout.emplace_back(2000 * COIN, GetPQScript(id));
    funding.vout.emplace_back(3000 * COIN, pos::GetBondScript(bond));
    CWalletTx wtx(wallet.get(), MakeTransactionRef(funding));
    BOOST_REQUIRE(wallet->AddToWallet(wtx));
    BOOST_CHECK_EQUAL(wallet->GetProtectionBalances().pq.Total(), 0);
    wallet->mapWallet.at(wtx.GetHash()).fInMempool = true;
    auto balances = wallet->GetProtectionBalances();
    BOOST_CHECK_EQUAL(balances.pq.available, 0);
    BOOST_CHECK_EQUAL(balances.pq.pending, 5000 * COIN);
    BOOST_CHECK_EQUAL(balances.pq.staked, 0);
    wallet->mapWallet.at(wtx.GetHash()).setAbandoned();
    wallet->mapWallet.at(wtx.GetHash()).fInMempool = false;
    BOOST_CHECK_EQUAL(wallet->GetProtectionBalances().pq.Total(), 0);
    UpdatePoSPostQuantumForTesting(false);
    CReserveKey reserve(wallet.get());
    CTransactionRef tx;
    WalletPQMigration migration;
    std::string error;
    BOOST_CHECK(!wallet->CreateLegacyPQMigration(tx, reserve, migration, error));
    BOOST_CHECK(!tx);
    BOOST_CHECK(error.find("not active") != std::string::npos);
}

BOOST_FIXTURE_TEST_CASE(pq_legacy_migration_mempool_and_accounting, ListCoinsTestingSetup)
{
    if (!pos::experimental::MLDSA44Available()) return;
    UpdatePoSPostQuantumForTesting(true);
    struct RestorePQ { ~RestorePQ() { UpdatePoSPostQuantumForTesting(false); } } restore;
    LOCK2(cs_main, wallet->cs_wallet);
    const auto before = wallet->GetProtectionBalances();
    CReserveKey reserve(wallet.get());
    CTransactionRef tx;
    WalletPQMigration migration;
    std::string error;
    BOOST_REQUIRE_MESSAGE(wallet->CreateLegacyPQMigration(tx, reserve, migration, error), error);
    BOOST_CHECK_EQUAL(migration.inputs, 1U);
    BOOST_CHECK_EQUAL(migration.remaining, 0);
    CValidationState check;
    bool missing = false;
    BOOST_REQUIRE_MESSAGE(AcceptToMemoryPool(mempool, check, tx, &missing, nullptr, false, maxTxFee, true), FormatStateMessage(check));
    BOOST_CHECK(!missing);
    BOOST_CHECK(!mempool.exists(tx->GetHash()));
    BOOST_CHECK(wallet->mapWallet.count(tx->GetHash()) == 0);
    wallet->SetBroadcastTransactions(true);
    CValidationState state;
    BOOST_REQUIRE(wallet->CommitTransaction(tx, {}, {}, {}, reserve, nullptr, state));
    BOOST_CHECK(state.IsValid());
    BOOST_CHECK(mempool.exists(tx->GetHash()));
    const auto after = wallet->GetProtectionBalances();
    BOOST_CHECK_EQUAL(after.legacy.available, 0);
    BOOST_CHECK_EQUAL(after.legacy.immature, before.legacy.immature);
    BOOST_CHECK_EQUAL(after.pq.Total(), migration.amount);
    BOOST_CHECK_EQUAL(after.pq.Total() + migration.fee, before.legacy.available);
}

BOOST_FIXTURE_TEST_CASE(ListCoins, ListCoinsTestingSetup)
{
    std::string coinbaseAddress = coinbaseKey.GetPubKey().GetID().ToString();

    // Confirm ListCoins initially returns 1 coin grouped under coinbaseKey
    // address.
    auto list = wallet->ListCoins();
    BOOST_CHECK_EQUAL(list.size(), 1U);
    BOOST_CHECK_EQUAL(boost::get<CKeyID>(list.begin()->first).ToString(), coinbaseAddress);
    BOOST_CHECK_EQUAL(list.begin()->second.size(), 1U);

    // Check initial balance from one mature coinbase transaction.
    BOOST_CHECK_EQUAL(200000 * COIN, wallet->GetAvailableBalance());

    // Add a transaction creating a change address, and confirm ListCoins still
    // returns the coin associated with the change address underneath the
    // coinbaseKey pubkey, even though the change address has a different
    // pubkey.
    AddTx(CRecipient{GetScriptForRawPubKey({}), 1 * COIN, false }); //subtract fee
    list = wallet->ListCoins();
    BOOST_CHECK_EQUAL(list.size(), 1U);
    BOOST_CHECK_EQUAL(boost::get<CKeyID>(list.begin()->first).ToString(), coinbaseAddress);
    BOOST_CHECK_EQUAL(list.begin()->second.size(), 2U);

    // Lock both coins. Confirm number of available coins drops to 0.
    {
        LOCK2(cs_main, wallet->cs_wallet);
        std::vector<COutput> available;
        wallet->AvailableCoins(available);
        BOOST_CHECK_EQUAL(available.size(), 2U);
    }
    for (const auto& group : list) {
        for (const auto& coin : group.second) {
            LOCK(wallet->cs_wallet);
            wallet->LockCoin(COutPoint(coin.tx->GetHash(), coin.i));
        }
    }
    {
        LOCK2(cs_main, wallet->cs_wallet);
        std::vector<COutput> available;
        wallet->AvailableCoins(available);
        BOOST_CHECK_EQUAL(available.size(), 0U);
    }
    // Confirm ListCoins still returns same result as before, despite coins
    // being locked.
    list = wallet->ListCoins();
    BOOST_CHECK_EQUAL(list.size(), 1U);
    BOOST_CHECK_EQUAL(boost::get<CKeyID>(list.begin()->first).ToString(), coinbaseAddress);
    BOOST_CHECK_EQUAL(list.begin()->second.size(), 2U);
}

BOOST_FIXTURE_TEST_CASE(unbond_balance_unlock_and_reorg, ListCoinsTestingSetup)
{
    LOCK2(cs_main, wallet->cs_wallet);
    CBlockIndex* const tip = chainActive.Tip();
    const uint32_t delay = Params().GetConsensus().nPoSUnbondingBlocks;
    std::vector<CBlockIndex> extension(delay - 2);
    std::vector<uint256> hashes(extension.size());
    for (size_t i = 0; i < extension.size(); ++i) {
        CBlockIndex& index = extension[i];
        hashes[i] = ArithToUint256(arith_uint256(i + 1));
        index.phashBlock = &hashes[i];
        index.pprev = i == 0 ? tip : &extension[i - 1];
        index.nHeight = index.pprev->nHeight + 1;
        index.nTime = index.pprev->nTime + 1;
        index.BuildSkip();
    }
    struct RestoreTip {
        CBlockIndex* original;
        ~RestoreTip() { chainActive.SetTip(original); }
    } restore{tip};
    CBlockIndex* const unlocked_tip = &extension.back();
    chainActive.SetTip(unlocked_tip);
    const CAmount ordinary_balance = wallet->GetBalance();
    const CAmount legacy_balance = wallet->GetLegacyBalance(ISMINE_SPENDABLE, 1, nullptr);
    pos::UnbondData unbond;
    unbond.unlock_height = tip->nHeight + 1;
    unbond.withdrawal_key_id = coinbaseKey.GetPubKey().GetID();

    CMutableTransaction tx;
    tx.vin.emplace_back(COutPoint(uint256S("1234"), 0));
    tx.vout.emplace_back(1000 * COIN, pos::GetUnbondScript(unbond));
    pos::BondData bond;
    unsigned char vrf_secret[32]{};
    BOOST_REQUIRE(pos::GetSchnorrPublicKey(coinbaseKey.begin(), bond.signing_public_key));
    BOOST_REQUIRE(pos::DeriveVrfKey(coinbaseKey.begin(), vrf_secret, bond.vrf_public_key));
    bond.reward_key_id = bond.withdrawal_key_id = unbond.withdrawal_key_id;
    tx.vout.emplace_back(2000 * COIN, pos::GetBondScript(bond));
    CWalletTx wtx(wallet.get(), MakeTransactionRef(tx));
    wtx.SetMerkleBranch(tip->pprev, 1);
    BOOST_REQUIRE(wallet->AddToWallet(wtx));
    const CWalletTx& stored = wallet->mapWallet.at(wtx.GetHash());
    BOOST_CHECK_EQUAL(stored.GetUnbondUnlockHeight(unbond),
                      tip->pprev->nHeight + delay);
    BOOST_CHECK_EQUAL(wallet->IsMine(tx.vout[0]), ISMINE_SPENDABLE);
    BOOST_CHECK_EQUAL(wallet->IsMine(tx.vout[1]), ISMINE_SPENDABLE);
    BOOST_CHECK_EQUAL(stored.GetCredit(ISMINE_SPENDABLE), 3000 * COIN);
    BOOST_CHECK_EQUAL(stored.GetAvailableCredit(), 1000 * COIN);
    BOOST_CHECK_EQUAL(wallet->GetBalance(), ordinary_balance + 1000 * COIN);
    BOOST_CHECK_EQUAL(wallet->GetLegacyBalance(ISMINE_SPENDABLE, 1, nullptr),
                      legacy_balance + 1000 * COIN);

    // Reusing the credit cache must not expose locked funds after a rewind.
    chainActive.SetTip(unlocked_tip->pprev);
    BOOST_CHECK_EQUAL(stored.GetAvailableCredit(), 0);
    std::vector<COutput> coins;
    wallet->AvailableCoins(coins);
    BOOST_CHECK(std::none_of(coins.begin(), coins.end(),
        [&wtx](const COutput& coin) { return coin.tx->GetHash() == wtx.GetHash(); }));
    chainActive.SetTip(unlocked_tip);

    BOOST_CHECK_EQUAL(stored.GetAvailableCredit(), 1000 * COIN);
    wallet->AvailableCoins(coins);
    BOOST_CHECK(std::any_of(coins.begin(), coins.end(),
        [&wtx](const COutput& coin) {
            return coin.tx->GetHash() == wtx.GetHash() && coin.i == 0;
        }));
    BOOST_CHECK(std::none_of(coins.begin(), coins.end(),
        [&wtx](const COutput& coin) {
            return coin.tx->GetHash() == wtx.GetHash() && coin.i == 1;
        }));

    CKey other;
    other.MakeNewKey(true);
    unbond.withdrawal_key_id = other.GetPubKey().GetID();
    BOOST_CHECK_EQUAL(wallet->IsMine(CTxOut(
        1000 * COIN, pos::GetUnbondScript(unbond))), ISMINE_NO);
}

BOOST_FIXTURE_TEST_CASE(unbond_pending_wallet_rows, ListCoinsTestingSetup)
{
    LOCK2(cs_main, wallet->cs_wallet);
    pos::UnbondData unbond;
    unbond.unlock_height = chainActive.Height() + 1 +
        Params().GetConsensus().nPoSUnbondingBlocks;
    unbond.withdrawal_key_id = coinbaseKey.GetPubKey().GetID();
    CMutableTransaction tx;
    tx.vin.emplace_back(COutPoint(uint256S("1235"), 0));
    tx.vout.emplace_back(1000 * COIN, pos::GetUnbondScript(unbond));
    CWalletTx wtx(wallet.get(), MakeTransactionRef(tx));
    BOOST_REQUIRE(wallet->AddToWallet(wtx));
    const auto rows = GetWalletStakeBonds(*wallet);
    BOOST_REQUIRE_EQUAL(rows.size(), 1U);
    BOOST_CHECK(rows[0].unbond_pending);
    BOOST_CHECK_EQUAL(rows[0].confirmations, 0);
    BOOST_CHECK_EQUAL(rows[0].blocks_until_unlock,
                      Params().GetConsensus().nPoSUnbondingBlocks);
    BOOST_CHECK_EQUAL(wallet->mapWallet.at(wtx.GetHash()).GetAvailableCredit(), 0);
    wallet->mapWallet.at(wtx.GetHash()).setAbandoned();
    BOOST_CHECK(GetWalletStakeBonds(*wallet).empty());
}

BOOST_AUTO_TEST_SUITE_END()
