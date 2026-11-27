#!/usr/bin/env python3
# Copyright (c) 2026 The Verge Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""New PQ-active wallets start PQ-only and create classical keys on demand."""

from pathlib import Path

from test_framework.test_framework import VergeTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error


class PQFirstWalletTest(VergeTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.rpc_timewait = 300
        self.base_args = ['-without-tor', '-dnsseed=0', '-keypool=3']
        self.extra_args = [list(self.base_args) for _ in range(self.num_nodes)]

    def setup_network(self):
        self.setup_nodes()

    @staticmethod
    def assert_pq_only(wallet):
        info = wallet.getwalletinfo()
        assert_equal(info['keypoolsize'], 0)
        assert 'hdseedid' not in info
        assert info['walletversion'] >= 27000100

    def run_test(self):
        node = self.nodes[0]
        node.setstaking(False)
        self.assert_pq_only(node)
        initial = node.getaddressesbylabel('')
        assert_equal(len(initial), 1)
        initial_address = next(iter(initial))
        assert initial_address.startswith('vergeqxreg1')
        assert_equal(node.getaddressinfo(initial_address)['ismine'], True)
        receiving = node.getnewaddress('PQ default')
        change = node.getrawchangeaddress()
        for address in (receiving, change):
            assert address.startswith('vergeqxreg1')
            assert_equal(node.getaddressinfo(address)['ismine'], True)
        assert_raises_rpc_error(-11, 'Invalid label', node.getnewaddress, '*')
        assert_raises_rpc_error(-11, 'Invalid label', node.getnewpqaddress, '*')
        assert_raises_rpc_error(-5, 'Unknown address type', node.getnewaddress, '', 'unknown')
        self.assert_pq_only(node)

        self.log.info('Encryption, unlock and restart do not populate a classical keypool')
        node.encryptwallet('pq-first-passphrase')
        node.wait_until_stopped()
        self.start_node(0)
        self.assert_pq_only(node)
        assert_raises_rpc_error(-13, 'walletpassphrase', node.getnewaddress)
        node.walletpassphrase('pq-first-passphrase', 300)
        assert node.getnewaddress().startswith('vergeqxreg1')
        self.assert_pq_only(node)
        self.restart_node(0, self.base_args + ['-usehd=1', '-upgradewallet'])
        self.assert_pq_only(node)
        node.walletpassphrase('pq-first-passphrase', 300)
        assert_equal(node.getaddressinfo(receiving)['ismine'], True)

        backup_dir = Path(self.nodes[1].datadir) / 'regtest' / 'wallets' / 'pq-restored'
        backup_dir.mkdir()
        backup = backup_dir / 'wallet.dat'
        node.backupwallet(str(backup))

        self.log.info('Explicit legacy keys are HD-derived on demand without changing the default')
        legacy = node.getnewaddress('compatibility', 'legacy')
        assert_equal(node.getaddressinfo(legacy).get('ispostquantum', False), False)
        assert 'hdseedid' in node.getwalletinfo()
        assert node.getwalletinfo()['keypoolsize'] <= 1
        assert node.getrawchangeaddress('legacy')
        assert node.getnewaddress().startswith('vergeqxreg1')
        before = node.getwalletinfo()['keypoolsize']
        self.restart_node(0)
        node.walletpassphrase('pq-first-passphrase', 300)
        assert_equal(node.getwalletinfo()['keypoolsize'], before)
        assert node.getnewaddress().startswith('vergeqxreg1')
        node.keypoolrefill()
        assert_equal(node.getwalletinfo()['keypoolsize'], 3)

        self.log.info('Explicit defaults preserve access to legacy compatibility addresses')
        self.restart_node(0, self.base_args + ['-addresstype=legacy'])
        node.walletpassphrase('pq-first-passphrase', 300)
        assert_equal(node.getaddressinfo(node.getnewaddress()).get('ispostquantum', False), False)
        assert node.getnewaddress('', 'pq').startswith('vergeqxreg1')
        self.restart_node(0, self.base_args + ['-changetype=legacy'])
        node.walletpassphrase('pq-first-passphrase', 300)
        legacy_change = node.getrawchangeaddress()
        assert_equal(node.getaddressinfo(legacy_change).get('ispostquantum', False), False)
        assert node.getnewaddress().startswith('vergeqxreg1')
        assert node.getrawchangeaddress('pq').startswith('vergeqxreg1')

        self.log.info('A complete backup restores PQ-only initialization and defaults')
        self.restart_node(0)
        # BDB rejects a copy beside its original in the same process.
        observer = self.nodes[1]
        loaded = observer.loadwallet(backup_dir.name)
        restored = observer.get_wallet_rpc(loaded['name'])
        self.assert_pq_only(restored)
        restored.walletpassphrase('pq-first-passphrase', 300)
        assert_equal(restored.getaddressinfo(initial_address)['ismine'], True)
        assert restored.getnewaddress().startswith('vergeqxreg1')
        self.assert_pq_only(restored)
        observer.createwallet('fresh')
        fresh = observer.get_wallet_rpc('fresh')
        self.assert_pq_only(fresh)
        assert fresh.getnewaddress().startswith('vergeqxreg1')


if __name__ == '__main__':
    PQFirstWalletTest().main()
