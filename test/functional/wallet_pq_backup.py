#!/usr/bin/env python3
# Copyright (c) 2026 The Verge Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Recover automatic PQ change and bond credentials from a prior full backup."""

from decimal import Decimal
from pathlib import Path

from test_framework.test_framework import VergeTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error, sync_blocks


class PQBackupTest(VergeTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.rpc_timewait = 300
        self.extra_args = [["-without-tor", "-posactivationheight=1500"] for _ in range(2)]

    def run_test(self):
        sender, observer = self.nodes
        for node in self.nodes:
            node.setstaking(False)
        mining = sender.getnewaddress("mining", "legacy")
        destination = observer.getnewpqaddress("recipient")
        for count in [100] * 7 + [31]:
            sender.generatetoaddress(count, mining)
        sync_blocks(self.nodes)

        self.log.info("No unbacked internal key can receive change or bonding principal")
        assert_equal(sender.getwalletinfo()["pq_backup_required"], True)
        assert_raises_rpc_error(-4, "needs a backup", sender.sendtoaddress, destination, 100)
        assert_raises_rpc_error(-4, "needs a backup", sender.createbond, 2000)
        assert_equal(sender.getrawmempool(), [])

        restored_dir = Path(observer.datadir) / "regtest" / "wallets" / "recovered"
        restored_dir.mkdir()
        sender.backupwallet(str(restored_dir / "wallet.dat"))
        assert_equal(sender.getwalletinfo()["pq_backed_up_internal_keys"], 1000)
        payment = sender.sendtoaddress(destination, 100)
        bond = sender.createbond(2000)
        sender.generatetoaddress(1, mining)
        sync_blocks(self.nodes)
        mature = sum(coin["amount"] for coin in sender.listunspent(720))
        automatic = sender.setstaking(True, mature - Decimal("4000"))
        assert_equal(automatic["status"], "bond_created")
        sender.setstaking(False)
        sender.generatetoaddress(1, mining)
        sync_blocks(self.nodes)
        outputs = sender.getrawtransaction(payment, True)["vout"]
        change = next(v["scriptPubKey"]["addresses"][0] for v in outputs
                      if v["scriptPubKey"].get("addresses", []) != [destination])

        self.log.info("A backup preceding both payments and bonds restores their owners")
        observer.loadwallet(restored_dir.name)
        restored = observer.get_wallet_rpc(restored_dir.name)
        for address in (change, bond["reward_address"], bond["withdrawal_address"]):
            assert_equal(restored.getaddressinfo(address)["ismine"], True)
        owned = {b["txid"] for b in restored.listbonds()}
        assert bond["txid"] in owned
        assert automatic["bond_txid"] in owned
        assert_equal(restored.getwalletinfo()["pq_backed_up_internal_keys"], 0)
        assert restored.getwalletinfo()["pq_internal_keypool_size"] < 1000
        restored.backupwallet(str(Path(observer.datadir) / "renewed-pq-backup.dat"))
        assert_equal(restored.getwalletinfo()["pq_backed_up_internal_keys"], 1000)
        assert restored.sendtoaddress(destination, 1)
        self.restart_node(0)
        assert sender.getwalletinfo()["pq_backed_up_internal_keys"] < 1000
        assert_equal(sender.getwalletinfo()["pq_backup_required"], False)


if __name__ == "__main__":
    PQBackupTest().main()
