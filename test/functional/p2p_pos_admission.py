#!/usr/bin/env python3
# Copyright (c) 2026 Verge
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Defer a burst of genuine alternate blocks without blaming their sender."""

from pathlib import Path

from feature_pos_activation import IgnoredBlockMessage, PoSActivationTest
from p2p_pos_prefetch import HeaderMessage, RawBlockMessage
from test_framework.messages import MSG_BLOCK, NODE_NETWORK
from test_framework.mininode import MESSAGEMAP, P2PInterface
from test_framework.util import assert_equal, disconnect_nodes, sync_blocks, wait_until


class AdmissionPeer(P2PInterface):
    def __init__(self, blocks):
        super().__init__()
        self.blocks = blocks

    def on_inv(self, message):
        pass

    def on_getdata(self, message):
        for item in message.inv:
            if item.type == MSG_BLOCK and item.hash in self.blocks:
                self.send_message(RawBlockMessage(self.blocks[item.hash]))


class PoSAdmissionTest(PoSActivationTest):
    def set_test_params(self):
        super().set_test_params()
        self.activation_height = 1500
        self.extra_args = [["-posactivationheight=1500", "-without-tor"] for _ in range(2)]

    def run_test(self):
        for i, wallet in enumerate(self.nodes):
            wallet.backupwallet(self.options.tmpdir + "/pq-keypool-{}.dat".format(i))
        producer, observer = self.nodes
        address = producer.getnewaddress("mining", "legacy")
        self.mine_blocks(producer, 721, address)
        producer.createbond(1000)
        self.mine_blocks(producer, 778, address)
        timestamp = producer.getblockheader(producer.getbestblockhash())["time"] // 30 * 30

        def stake():
            nonlocal timestamp
            timestamp += 30
            for node in self.nodes:
                node.setmocktime(timestamp)
            return producer.generatestake()

        root = stake()
        sync_blocks(self.nodes)
        disconnect_nodes(producer, 1)
        disconnect_nodes(observer, 0)
        blocks = {}
        for _ in range(50):
            block = stake()
            blocks[int(block, 16)] = bytes.fromhex(producer.getblock(block, 0))
            producer.invalidateblock(block)
            assert_equal(producer.getbestblockhash(), root)

        MESSAGEMAP[b"block"] = IgnoredBlockMessage
        peer = observer.add_p2p_connection(AdmissionPeer(blocks), services=NODE_NETWORK)
        for raw in blocks.values():
            peer.send_message(RawBlockMessage(raw))
        peer.sync_with_ping()
        log = Path(observer.datadir) / "regtest" / "debug.log"
        wait_until(lambda: "deferring PoS block validation budget" in log.read_text(encoding="utf-8"))
        last = list(blocks)[-1]
        wait_until(lambda: any(tip["hash"] == "{:064x}".format(last)
                              for tip in observer.getchaintips()), timeout=30)
        assert_equal(observer.getpeerinfo()[0]["banscore"], 0)
        assert not any(tip["status"] == "invalid" for tip in observer.getchaintips())

        self.log.info("Deferred valid data retries on the same connection without a ban")
        producer.reconsiderblock("{:064x}".format(last))
        continuation = stake()
        peer.blocks[int(continuation, 16)] = bytes.fromhex(producer.getblock(continuation, 0))
        peer.send_and_ping(HeaderMessage(bytes.fromhex(producer.getblockheader(continuation, False))))
        wait_until(lambda: observer.getbestblockhash() == continuation, timeout=30)
        assert_equal(observer.getpeerinfo()[0]["banscore"], 0)
        assert_equal(observer.getposinfo()["chain_current_for_services"], True)
        observer.disconnect_p2ps()


if __name__ == "__main__":
    PoSAdmissionTest().main()
