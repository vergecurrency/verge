#!/usr/bin/env python3
# Copyright (c) 2026 Verge
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Check automatic staking does not block validation callbacks or P2P."""

from test_framework.mininode import P2PInterface
from test_framework.test_framework import VergeTestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes_bi,
    sync_blocks,
    wait_until,
)


class HandshakePeer(P2PInterface):
    def on_inv(self, message):
        # This peer checks responsiveness without downloading blocks.
        pass


class PoSWorkerTest(VergeTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.rpc_timewait = 60
        self.extra_args = [["-posactivationheight=1500", "-without-tor"]
                           for _ in range(self.num_nodes)]

    @staticmethod
    def mine_blocks(node, count, address):
        while count:
            batch = min(count, 100)
            node.generatetoaddress(batch, address)
            count -= batch

    def advance_slot(self):
        node, observer = self.nodes
        previous_height = node.getblockcount()
        previous_time = node.getblockheader(node.getbestblockhash())["time"]
        timestamp = ((previous_time // 30) + 1) * 30
        observer.setmocktime(timestamp)
        node.setmocktime(timestamp)
        wait_until(lambda: node.getblockcount() == previous_height + 1,
                   timeout=30)
        sync_blocks(self.nodes, timeout=30)
        node.syncwithvalidationinterfacequeue()

    def run_test(self):
        node, observer = self.nodes
        address = node.getnewaddress()
        self.mine_blocks(node, 721, address)
        node.createbond(1000)
        self.mine_blocks(node, 778, address)
        self.sync_all()
        assert_equal(node.getblockcount(), 1499)
        assert_equal(node.getstakinginfo()["eligible_owned_bonds"], 1)

        final_pow_time = node.getblockheader(node.getbestblockhash())["time"]
        timestamp = ((final_pow_time // 30) + 1) * 30
        observer.setmocktime(timestamp)
        node.setmocktime(timestamp)
        node.generatestake()
        self.sync_all()
        assert_equal(node.getblockcount(), 1500)

        self.log.info("Produce automatic blocks with transaction callback load")
        # Preserve liquid funds for transactions instead of auto-bonding them.
        node.setstaking(True, node.getbalance())
        for _ in range(12):
            txids = [node.sendtoaddress(node.getnewaddress(), 1)
                     for _ in range(12)]
            self.advance_slot()
            assert_equal(node.getrawmempool(), [])
            block = node.getblock(node.getbestblockhash())
            assert set(txids).issubset(set(block["tx"]))

        self.log.info("Check a fresh handshake and restart with staking enabled")
        probe = node.add_p2p_connection(HandshakePeer())
        probe.sync_with_ping(timeout=10)
        final_hash = node.getbestblockhash()
        final_header = node.getblockheader(final_hash, False)
        final_time = node.getblockheader(final_hash)["time"]
        self.restart_node(0, self.extra_args[0] + [
            "-mocktime={}".format(final_time),
        ])
        assert_equal(node.getbestblockhash(), final_hash)
        assert_equal(node.getblockheader(final_hash, False), final_header)
        assert_equal(node.getstakinginfo()["enabled"], True)
        connect_nodes_bi(self.nodes, 0, 1)
        self.advance_slot()
        assert_equal(node.getblockcount(), 1513)
        node.setstaking(False)


if __name__ == "__main__":
    PoSWorkerTest().main()
