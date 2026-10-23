#!/usr/bin/env python3
# Copyright (c) 2026 Verge
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Check independent PoS producers converge without orphan-header history."""

from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import VergeTestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes_bi,
    disconnect_nodes,
    sync_blocks,
    sync_mempools,
    wait_until,
)


class PoSVoteHeadsTest(VergeTestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        self.setup_clean_chain = True
        self.rpc_timewait = 120
        self.extra_args = [["-posactivationheight=1500", "-without-tor"]
                           for _ in range(self.num_nodes)]

    def setup_network(self):
        self.setup_nodes()
        connect_nodes_bi(self.nodes, 0, 1)

    @staticmethod
    def mine(node, count, address):
        while count:
            batch = min(count, 100)
            node.generatetoaddress(batch, address)
            count -= batch

    def stake(self, producer):
        for _ in range(100):
            self.timestamp += 30
            for node in self.nodes:
                node.setmocktime(self.timestamp)
            try:
                return producer.generatestake()
            except JSONRPCException as error:
                assert "wallet has no eligible bond" in error.error["message"]
        raise AssertionError("No eligible slot in 100 attempts")

    def run_test(self):
        first, second, fresh = self.nodes
        address = first.getnewaddress()
        self.mine(first, 1, address)
        sync_blocks([first, second])
        self.mine(second, 720, second.getnewaddress())
        sync_blocks([first, second])
        first_bond = first.createbond(1000)
        second_bond = second.createbond(1000)
        # Test consensus rather than transaction-inventory trickle timing.
        second.sendrawtransaction(first.getrawtransaction(first_bond["txid"]))
        first.sendrawtransaction(second.getrawtransaction(second_bond["txid"]))
        sync_mempools([first, second])
        self.mine(first, 778, address)
        sync_blocks([first, second])
        assert_equal(first.getblockcount(), 1499)
        assert_equal(second.getstakinginfo()["eligible_owned_bonds"], 1)
        self.timestamp = first.getblockheader(first.getbestblockhash())["time"]
        self.timestamp = self.timestamp // 30 * 30
        self.stake(first)
        sync_blocks([first, second])

        self.log.info("Partition independent producers and build competing branches")
        disconnect_nodes(first, 1)
        disconnect_nodes(second, 0)
        for _ in range(3):
            self.stake(first)
            self.stake(second)
        assert first.getbestblockhash() != second.getbestblockhash()
        assert_equal(fresh.getblockcount(), 0)
        connect_nodes_bi(self.nodes, 0, 1)
        sync_blocks([first, second], timeout=120)
        common = first.getbestblockhash()
        for node in (first, second):
            assert not any(tip["status"] == "invalid"
                           for tip in node.getchaintips())

        self.log.info("Fresh sync must not require losing-branch headers")
        connect_nodes_bi(self.nodes, 0, 2)
        sync_blocks(self.nodes, timeout=120)
        assert_equal(fresh.getbestblockhash(), common)

        self.log.info("Explicit reconsideration and restart retain convergence")
        disconnect_nodes(fresh, 0)
        disconnect_nodes(first, 2)
        fresh.invalidateblock(common)
        assert fresh.getbestblockhash() != common
        fresh.reconsiderblock(common)
        assert_equal(fresh.getbestblockhash(), common)
        for index in (1, 2):
            self.restart_node(index, self.extra_args[index] + [
                "-mocktime={}".format(self.timestamp),
            ])
            assert_equal(self.nodes[index].getbestblockhash(), common)
        self.restart_node(2, self.extra_args[2] + [
            "-mocktime={}".format(self.timestamp), "-reindex-chainstate",
        ])
        wait_until(lambda: fresh.getbestblockhash() == common, timeout=120)
        assert_equal(fresh.getposinfo()["state_block_hash"], common)
        connect_nodes_bi(self.nodes, 0, 1)
        connect_nodes_bi(self.nodes, 0, 2)
        sync_blocks(self.nodes, timeout=120)


if __name__ == "__main__":
    PoSVoteHeadsTest().main()
