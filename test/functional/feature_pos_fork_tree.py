#!/usr/bin/env python3
# Copyright (c) 2026 Verge
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Authenticate a three-leaf preference cycle and test tree-wide convergence."""

from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import VergeTestFramework
from test_framework.util import assert_equal, connect_nodes_bi, disconnect_nodes, sync_blocks


class PoSForkTreeTest(VergeTestFramework):
    def set_test_params(self):
        self.num_nodes = 4
        self.setup_clean_chain = True
        self.rpc_timewait = 300
        self.extra_args = [["-posactivationheight=4000", "-without-tor"] for _ in range(4)]

    def mine(self, node, count, address):
        while count:
            batch = min(count, 100)
            node.generatetoaddress(batch, address)
            count -= batch
            sync_blocks(self.nodes)

    def stake(self, node):
        for _ in range(300):
            self.timestamp += 30
            for peer in self.nodes:
                peer.setmocktime(self.timestamp)
            try:
                return node.generatestake()
            except JSONRPCException as error:
                assert "wallet has no eligible bond" in error.error["message"]
        raise AssertionError("No eligible producer slot in 300 attempts")

    def run_test(self):
        first, small, medium, other = self.nodes
        addresses = [node.getnewaddress() for node in self.nodes]
        for node, address in zip(self.nodes, addresses):
            self.mine(node, 721, address)
        for node, amount in zip(self.nodes, (90000, 10000, 20000, 75000)):
            bond = node.createbond(amount)
            raw = node.getrawtransaction(bond["txid"])
            for peer in self.nodes:
                peer.sendrawtransaction(raw)
        self.mine(first, 3999 - 4 * 721, addresses[0])
        self.timestamp = first.getblockheader(first.getbestblockhash())["time"] // 30 * 30
        root = self.stake(first)
        sync_blocks(self.nodes)
        for i in range(3):
            disconnect_nodes(self.nodes[i], i + 1)
            disconnect_nodes(self.nodes[i + 1], i)

        child_a = self.stake(first)
        for node in (small, medium):
            node.submitblock(first.getblock(child_a, 0))
        a1 = self.stake(first)
        small.submitblock(first.getblock(a1, 0))
        a1_tip = self.stake(small)
        a2 = self.stake(medium)
        a2_tip = self.stake(medium)
        b1 = self.stake(other)
        b2 = self.stake(other)
        b_tip = self.stake(other)
        branches = [
            [first.getblock(child_a, 0), first.getblock(a1, 0), small.getblock(a1_tip, 0)],
            [first.getblock(child_a, 0), medium.getblock(a2, 0), medium.getblock(a2_tip, 0)],
            [other.getblock(b1, 0), other.getblock(b2, 0), other.getblock(b_tip, 0)],
        ]
        assert root not in (a1_tip, a2_tip, b_tip)
        for node, order in zip(self.nodes, ((0, 1, 2), (0, 2, 1), (1, 2, 0), (2, 1, 0))):
            for branch in order:
                for raw in branches[branch]:
                    result = node.submitblock(raw)
                    assert result in (None, "duplicate", "inconclusive"), result
            assert_equal(node.getbestblockhash(), a2_tip)
            assert_equal(node.getposinfo()["state_block_hash"], a2_tip)
            assert not any(tip["status"] == "invalid" for tip in node.getchaintips())

        self.log.info("Identical authenticated candidate trees converge across arrival orders and restart")
        for i, node in enumerate(self.nodes):
            self.restart_node(i, self.extra_args[i] + ["-mocktime={}".format(self.timestamp)])
            assert_equal(node.getbestblockhash(), a2_tip)
            assert_equal(node.verifychain(4, 12), True)
        for i in range(3):
            connect_nodes_bi(self.nodes, i, i + 1)
        sync_blocks(self.nodes)


if __name__ == "__main__":
    PoSForkTreeTest().main()
