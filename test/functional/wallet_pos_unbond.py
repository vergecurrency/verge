#!/usr/bin/env python3
# Copyright (c) 2026 Verge
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Exercise wallet signing and fee funding for maturing and eligible bonds."""

from test_framework.test_framework import VergeTestFramework
from test_framework.util import assert_equal


class PoSUnbondTest(VergeTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [["-posactivationheight=1500", "-without-tor"]]

    @staticmethod
    def mine(node, count, address):
        while count:
            batch = min(count, 100)
            node.generatetoaddress(batch, address)
            count -= batch

    @staticmethod
    def check_unbond(node, bond):
        height = node.getblockcount()
        balance = node.getbalance()
        result = node.unbond(bond["txid"], bond["vout"])
        assert_equal(result["amount"], 1000)
        assert_equal(result["unlock_height"],
                     height + 1 + node.getposinfo()["unbonding_blocks"])
        tx = node.getrawtransaction(result["txid"], True)
        assert any(vin["txid"] == bond["txid"] and
                   vin["vout"] == bond["vout"] for vin in tx["vin"])
        assert len(tx["vin"]) >= 2  # Ordinary inputs fund the fee, not principal.
        assert result["txid"] in node.getrawmempool()
        wallet_tx = node.gettransaction(result["txid"])
        assert wallet_tx["fee"] < 0
        assert_equal(node.getbalance(), balance + wallet_tx["fee"])
        unbonding = next(item for item in node.listunbondings()
                        if item["txid"] == result["txid"])
        assert_equal(unbonding["unlocked"], False)
        assert not any(coin["txid"] == result["txid"] and
                       coin["amount"] == 1000 for coin in node.listunspent(0))
        return result

    def run_test(self):
        node = self.nodes[0]
        node.setstaking(False)
        address = node.getnewaddress()
        self.mine(node, 731, address)
        # Keep one eligible bond to continue block production after withdrawals.
        for _ in range(3):
            node.createbond(1000)
        self.mine(node, 1, address)
        bonds = node.listbonds()
        assert_equal(len(bonds), 3)
        assert_equal(bonds[0]["mature"], False)

        self.log.info("Unbond a maturing bond before activation")
        first = self.check_unbond(node, bonds[0])
        self.mine(node, 767, address)
        assert_equal(node.getblockcount(), 1499)
        assert_equal(node.getstakinginfo()["eligible_owned_bonds"], 2)
        final_pow_time = node.getblockheader(node.getbestblockhash())["time"]
        timestamp = ((final_pow_time // 30) + 1) * 30
        node.setmocktime(timestamp)
        node.generatestake()

        self.log.info("Unbond an eligible bond with PoS replay protection")
        eligible = next(bond for bond in node.listbonds() if bond["eligible"])
        second = self.check_unbond(node, eligible)
        assert_equal(node.getrawtransaction(second["txid"], True)["version"],
                     0x05475658)
        node.setmocktime(timestamp + 30)
        node.generatestake()
        assert_equal(node.getrawmempool(), [])
        unbondings = node.listunbondings()
        assert_equal({item["txid"] for item in unbondings},
                     {first["txid"], second["txid"]})
        assert all(not item["unlocked"] for item in unbondings)
        assert_equal(node.getstakinginfo()["owned_bonds"], 1)

        self.log.info("Verify withdrawals survive wallet reload")
        self.restart_node(0, self.extra_args[0] + [
            "-mocktime={}".format(timestamp + 30)])
        assert_equal({item["txid"] for item in node.listunbondings()},
                     {first["txid"], second["txid"]})


if __name__ == "__main__":
    PoSUnbondTest().main()
