#!/usr/bin/env python3
# Copyright (c) 2026 Verge
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Automatic staking bonds a fragmented wallet in bounded transactions."""

from decimal import Decimal

from test_framework.test_framework import VergeTestFramework
from test_framework.util import assert_equal


class BondBatchingTest(VergeTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [["-posactivationheight=3000"]]

    def run_test(self):
        node = self.nodes[0]
        address = node.getnewaddress()
        for _ in range(16):
            node.generatetoaddress(100, address)
        mature = node.listunspent(720)
        # These legacy inputs exceed the standard size limit when combined.
        assert len(mature) > 700
        reserve = sum(coin["amount"] for coin in mature) / 2
        result = node.setstaking(True, reserve)
        assert_equal(result["status"], "bond_created")
        tx = node.getrawtransaction(result["bond_txid"], True)
        assert 0 < len(tx["vin"]) <= 100
        assert tx["size"] < 100000
        assert result["bond_amount"] >= Decimal("1000")
        assert_equal(node.getstakinginfo()["reserve_balance"], reserve)

        # Repeated opt-in must not create a competing bond while one is pending.
        pending = node.getrawmempool()
        assert_equal(node.setstaking(True)["status"], "bond_exists")
        assert_equal(node.getrawmempool(), pending)
        node.generatetoaddress(1, address)
        second = node.setstaking(True)
        assert_equal(second["status"], "bond_created")
        assert second["bond_txid"] != result["bond_txid"]
        tx = node.getrawtransaction(second["bond_txid"], True)
        assert 0 < len(tx["vin"]) <= 100
        node.setstaking(False)


if __name__ == '__main__':
    BondBatchingTest().main()
