#!/usr/bin/env python3
# Copyright (c) 2026 Verge
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Exercise wallet signing and fee funding for maturing and eligible bonds."""

from test_framework.test_framework import VergeTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.util import assert_equal, assert_raises_rpc_error


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
        assert_equal(result["unlock_height_estimated"], True)
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
        assert_equal(unbonding["status"], "unbond_pending")
        assert_equal(unbonding["unlock_height_estimated"], True)
        assert_equal(unbonding["blocks_until_unlock"],
                     node.getposinfo()["unbonding_blocks"])
        assert_raises_rpc_error(-4, "bond already has a pending spend",
                                node.unbond, bond["txid"], bond["vout"])
        assert not any(coin["txid"] == result["txid"] and
                       coin["amount"] == 1000 for coin in node.listunspent(0))
        return result

    def stake(self, node):
        for _ in range(100):
            self.timestamp += 30
            node.setmocktime(self.timestamp)
            try:
                return node.generatestake()
            except JSONRPCException as error:
                assert "wallet has no eligible bond" in error.error["message"]
        raise AssertionError("No eligible slot in 100 attempts")

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
        node.prioritisetransaction(first["txid"], 0, -100000000)
        self.mine(node, 7, address)
        assert first["txid"] in node.getrawmempool()
        pending = node.listunbondings()[0]
        assert_equal(pending["status"], "unbond_pending")
        assert pending["unlock_height"] > first["unlock_height"]
        node.prioritisetransaction(first["txid"], 0, 100000000)
        self.mine(node, 1, address)
        confirmed = node.listunbondings()[0]
        assert_equal(confirmed["status"], "unbonding")
        assert_equal(confirmed["confirmations"], 1)
        assert_equal(confirmed["unlock_height_estimated"], False)
        assert_equal(confirmed["encoded_unlock_height"], first["unlock_height"])
        assert_equal(confirmed["unlock_height"], node.getblockcount() +
                     node.getposinfo()["unbonding_blocks"])
        self.log.info("Reconfirm a withdrawal after a reorg without pre-aging its lock")
        node.invalidateblock(node.getbestblockhash())
        pending = node.listunbondings()[0]
        assert_equal(pending["status"], "unbond_pending")
        assert_equal(pending["confirmations"], 0)
        assert_equal(pending["unlock_height_estimated"], True)
        assert_equal(pending["blocks_until_unlock"],
                     node.getposinfo()["unbonding_blocks"])
        node.prioritisetransaction(first["txid"], 0, -100000000)
        self.mine(node, 2, address)
        node.prioritisetransaction(first["txid"], 0, 100000000)
        self.mine(node, 1, address)
        reconfirmed = node.listunbondings()[0]
        assert_equal(reconfirmed["status"], "unbonding")
        assert_equal(reconfirmed["confirmations"], 1)
        assert_equal(reconfirmed["unlock_height"], confirmed["unlock_height"] + 2)
        assert_equal(reconfirmed["encoded_unlock_height"], first["unlock_height"])
        self.mine(node, 1499 - node.getblockcount(), address)
        assert_equal(node.getblockcount(), 1499)
        assert_equal(node.getstakinginfo()["eligible_owned_bonds"], 2)
        final_pow_time = node.getblockheader(node.getbestblockhash())["time"]
        self.timestamp = final_pow_time // 30 * 30
        self.stake(node)

        self.log.info("Unbond an eligible bond with PoS replay protection")
        eligible = next(bond for bond in node.listbonds() if bond["eligible"])
        second = self.check_unbond(node, eligible)
        assert_equal(node.getrawtransaction(second["txid"], True)["version"],
                     0x05475658)
        node.prioritisetransaction(second["txid"], 0, -100000000)
        self.stake(node)
        assert second["txid"] in node.getrawmempool()
        node.prioritisetransaction(second["txid"], 0, 100000000)
        self.stake(node)
        assert_equal(node.getrawmempool(), [])
        unbondings = node.listunbondings()
        assert_equal({item["txid"] for item in unbondings},
                     {first["txid"], second["txid"]})
        assert all(not item["unlocked"] for item in unbondings)
        assert all(item["status"] == "unbonding" for item in unbondings)
        assert_equal(node.getstakinginfo()["owned_bonds"], 1)

        self.log.info("Verify withdrawals survive wallet reload")
        self.restart_node(0, self.extra_args[0] + [
            "-mocktime={}".format(self.timestamp)])
        assert_equal({item["txid"] for item in node.listunbondings()},
                     {first["txid"], second["txid"]})


if __name__ == "__main__":
    PoSUnbondTest().main()
