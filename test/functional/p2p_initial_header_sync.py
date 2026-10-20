#!/usr/bin/env python3
# Copyright (c) 2026 Verge
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""An empty first peer must not block initial sync from later peers."""

import time

from test_framework.messages import ser_compact_size
from test_framework.mininode import P2PInterface
from test_framework.test_framework import VergeTestFramework
from test_framework.util import assert_equal, connect_nodes, sync_blocks, wait_until


class HeaderAnnouncement:
    command = b"headers"

    def __init__(self, headers):
        self.headers = headers

    def serialize(self):
        # Each header is followed by empty transaction and signature vectors.
        return ser_compact_size(len(self.headers)) + b"".join(
            header + b"\x00\x00" for header in self.headers)


class HeadersOnlyPeer(P2PInterface):
    def on_inv(self, message):
        pass


class InitialHeaderSyncTest(VergeTestFramework):
    def set_test_params(self):
        self.num_nodes = 5
        self.setup_clean_chain = True
        self.extra_args = [["-without-tor"] for _ in range(self.num_nodes)]

    def setup_network(self):
        self.setup_nodes()

    @staticmethod
    def received_headers(node, peer_number):
        return sum(peer.get("bytesrecv_per_msg", {}).get("headers", 0)
                   for peer in node.getpeerinfo()
                   if "testnode{}".format(peer_number) in peer["subver"])

    def run_test(self):
        receiver, empty_peer, miner = self.nodes[:3]
        assert_equal(receiver.getblockchaininfo()["initialblockdownload"], True)
        self.log.info("Connect the fresh receiver to a peer at genesis")
        connect_nodes(receiver, 1)
        wait_until(lambda: self.received_headers(receiver, 1) > 0, timeout=10)
        received = self.received_headers(receiver, 1)
        time.sleep(2)
        assert_equal(self.received_headers(receiver, 1), received)

        self.log.info("Connect a later peer with three blocks already mined")
        miner.generatetoaddress(3, miner.getnewaddress())
        assert_equal(empty_peer.getblockcount(), 0)
        connect_nodes(receiver, 2)
        sync_blocks([receiver, miner], timeout=10)
        assert_equal(receiver.getblockcount(), 3)
        assert_equal(receiver.getblockchaininfo()["initialblockdownload"], False)
        assert any("testnode1" in peer["subver"] for peer in receiver.getpeerinfo())

        self.log.info("Known headers must identify another source for missing blocks")
        receiver = self.nodes[3]
        connect_nodes(receiver, 4)
        wait_until(lambda: self.received_headers(receiver, 4) > 0, timeout=10)
        headers_peer = receiver.add_p2p_connection(HeadersOnlyPeer())
        headers = [bytes.fromhex(miner.getblockheader(miner.getblockhash(height), False))
                   for height in range(1, 4)]
        headers_peer.send_and_ping(HeaderAnnouncement(headers))
        assert_equal(receiver.getblockchaininfo()["headers"], 3)
        assert_equal(receiver.getblockcount(), 0)
        connect_nodes(receiver, 2)
        sync_blocks([receiver, miner], timeout=10)
        assert_equal(receiver.getblockcount(), 3)


if __name__ == "__main__":
    InitialHeaderSyncTest().main()
