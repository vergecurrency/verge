#!/usr/bin/env python3
# Copyright (c) 2026 Verge
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Exercise bonding, PoS activation, production, restart, and reconnect."""

from test_framework.test_framework import VergeTestFramework
from test_framework.mininode import MESSAGEMAP, P2PInterface
from test_framework.util import assert_equal, connect_nodes_bi, disconnect_nodes, wait_until


class MalformedPoSMessage:
    def __init__(self, command, size):
        self.command = command
        self.size = size

    def serialize(self):
        return b"\x00" * self.size


class IgnoredBlockMessage:
    command = b"block"

    def deserialize(self, payload):
        payload.read()


class PoSActivationTest(VergeTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.rpc_timewait = 300
        self.extra_args = [["-posactivationheight=1500"],
                           ["-posactivationheight=1500"]]

    @staticmethod
    def next_slot(timestamp, slots=1):
        return ((timestamp // 30) + slots) * 30

    @staticmethod
    def mine_blocks(node, count, address):
        while count:
            batch = min(count, 100)
            node.generatetoaddress(batch, address)
            count -= batch

    def run_test(self):
        node = self.nodes[0]
        observer = self.nodes[1]
        mining_address = node.getnewaddress()

        self.mine_blocks(node, 721, mining_address)
        bond = node.createbond(1000)
        assert_equal(bond["amount"], 1000)
        self.mine_blocks(node, 1, mining_address)
        self.mine_blocks(node, 777, mining_address)
        assert_equal(node.getblockcount(), 1499)
        staking_info = node.getstakinginfo()
        self.log.info("Pre-activation staking state: %s", staking_info)
        assert_equal(staking_info["tracked_bonds"], 1)
        assert_equal(staking_info["initial_snapshot_bonds"], 1)

        final_pow_time = node.getblockheader(node.getbestblockhash())["time"]
        epoch_zero_time = self.next_slot(final_pow_time)
        node.setmocktime(epoch_zero_time)
        epoch_zero = node.generatestake()
        assert_equal(node.getblockcount(), 1500)
        self.sync_all()

        MESSAGEMAP[b"block"] = IgnoredBlockMessage
        vote_peer = node.add_p2p_connection(P2PInterface())
        node.setmocktime(epoch_zero_time + 120 * 30)
        epoch_one = node.generatestake()
        vote_peer.sync_with_ping()
        wait_until(lambda: "posvote" in vote_peer.last_message)
        assert_equal(len(vote_peer.last_message["posvote"].serialize()), 229)
        assert_equal(node.getblockcount(), 1501)
        self.sync_all()

        vote_peer.send_message(MalformedPoSMessage(b"posvote", 228))
        vote_peer.send_message(MalformedPoSMessage(b"posvevid", 458))
        vote_peer.sync_with_ping()
        wait_until(lambda: any(peer.get("banscore", 0) >= 40
                               for peer in node.getpeerinfo()))

        disconnect_nodes(node, 1)
        disconnect_nodes(observer, 0)

        node.setmocktime(epoch_zero_time + 240 * 30)
        epoch_two = node.generatestake()
        assert_equal(node.getblockcount(), 1502)
        assert_equal(node.getbestblockhash(), epoch_two)
        assert observer.getbestblockhash() != epoch_two
        observer.setmocktime(epoch_zero_time + 240 * 30)
        connect_nodes_bi(self.nodes, 0, 1)
        self.sync_all()
        assert_equal(observer.getbestblockhash(), epoch_two)

        assert_equal(node.setstaking(True), True)
        staking_info = node.getstakinginfo()
        assert_equal(staking_info["enabled"], True)
        assert_equal(staking_info["active"], True)

        self.restart_node(0, [
            "-posactivationheight=1500",
            "-mocktime={}".format(epoch_zero_time + 240 * 30),
        ])
        node = self.nodes[0]
        assert_equal(node.getbestblockhash(), epoch_two)
        assert_equal(node.getstakinginfo()["enabled"], True)

        self.restart_node(1, [
            "-posactivationheight=1500",
            "-mocktime={}".format(epoch_zero_time + 240 * 30),
            "-reindex-chainstate",
        ])
        self.sync_all()
        assert_equal(self.nodes[1].getbestblockhash(), epoch_two)

        node.invalidateblock(epoch_two)
        assert_equal(node.getbestblockhash(), epoch_one)
        node.reconsiderblock(epoch_two)
        assert_equal(node.getbestblockhash(), epoch_two)
        assert_equal(node.getblockheader(epoch_zero)["height"], 1500)


if __name__ == "__main__":
    PoSActivationTest().main()
