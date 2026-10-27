#!/usr/bin/env python3
# Copyright (c) 2026 Verge
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Check bounded early PoS header retries without relaxing block validity."""

from pathlib import Path
import struct

from test_framework.messages import MSG_BLOCK
from test_framework.mininode import P2PInterface, mininode_lock
from test_framework.test_framework import VergeTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
    disconnect_nodes,
    wait_until,
)


class PoSHeaderMessage:
    command = b"headers"

    def __init__(self, header):
        self.header = bytes.fromhex(header)

    def serialize(self):
        return b"\x01" + self.header + b"\x00\x00"


class RawBlockMessage:
    command = b"block"

    def __init__(self, block):
        self.block = bytes.fromhex(block)

    def serialize(self):
        return self.block


class HeaderPeer(P2PInterface):
    def __init__(self):
        super().__init__()
        self.block_requests = []

    def on_inv(self, message):
        pass

    def on_getdata(self, message):
        self.block_requests.extend(
            inv.hash for inv in message.inv if inv.type == MSG_BLOCK)

    def requested(self):
        with mininode_lock:
            return list(self.block_requests)


class PoSHeaderTimeTest(VergeTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.rpc_timewait = 300
        self.extra_args = [["-posactivationheight=1500", "-without-tor"]
                           for _ in range(self.num_nodes)]

    @staticmethod
    def mine(node, count, address):
        while count:
            batch = min(count, 100)
            node.generatetoaddress(batch, address)
            count -= batch

    @staticmethod
    def wait_for_log(node, text):
        log = Path(node.datadir) / "regtest" / "debug.log"
        wait_until(lambda: text in log.read_text(encoding="utf-8"))

    def run_test(self):
        producer, observer = self.nodes
        address = producer.getnewaddress()
        self.mine(producer, 721, address)
        producer.createbond(1000)
        self.mine(producer, 778, address)
        self.sync_all()
        assert_equal(observer.getblockcount(), 1499)
        disconnect_nodes(producer, 1)
        disconnect_nodes(observer, 0)
        peer = observer.add_p2p_connection(HeaderPeer())

        timestamp = ((producer.getblockheader(
            producer.getbestblockhash())["time"] // 30) + 1) * 30
        producer.setmocktime(timestamp)
        first = producer.generatestake()
        header = producer.getblockheader(first, False)
        raw = producer.getblock(first, 0)
        observer.setmocktime(timestamp - 1)

        self.log.info("A one-second early announcement must not penalize its peer")
        peer.send_and_ping(RawBlockMessage(raw))
        self.wait_for_log(observer, "time-too-new")
        assert_equal(observer.getblockcount(), 1499)
        peer.send_and_ping(PoSHeaderMessage(header))
        # Neither duplicate headers nor competing next-slot locators may grow
        # the queue or replace the first pending locator.
        for i in range(20):
            peer.send_message(PoSHeaderMessage(header))
            alternate = bytearray.fromhex(header)
            alternate[36] ^= i + 1
            peer.send_message(PoSHeaderMessage(alternate.hex()))
        peer.sync_with_ping()
        assert_equal(peer.requested(), [])
        assert_equal(observer.getpeerinfo()[0]["banscore"], 0)
        assert_raises_rpc_error(-5, "Block not found", observer.getblockheader, first)
        observer.setmocktime(timestamp)
        wait_until(lambda: peer.requested() == [int(first, 16)])
        for _ in range(10):
            peer.send_message(PoSHeaderMessage(header))
        peer.sync_with_ping()
        assert_equal(peer.requested(), [int(first, 16)])
        peer.send_and_ping(RawBlockMessage(raw))
        wait_until(lambda: observer.getbestblockhash() == first)

        self.log.info("Far-future announcements must not be queued")
        second_time = timestamp + 60
        producer.setmocktime(second_time)
        second = producer.generatestake()
        second_header = producer.getblockheader(second, False)
        peer.send_and_ping(PoSHeaderMessage(second_header))
        self.wait_for_log(observer, "ignoring far-future PoS header " + second)
        observer.setmocktime(second_time - 30)
        peer.sync_with_ping()
        assert_equal(peer.requested(), [int(first, 16)])
        assert_equal(observer.getpeerinfo()[0]["banscore"], 0)

        self.log.info("Duplicates must not extend a deferred header's deadline")
        peer.send_and_ping(PoSHeaderMessage(second_header))
        self.wait_for_log(observer, "deferring early PoS header " + second)
        observer.setmocktime(second_time - 1)
        peer.send_and_ping(PoSHeaderMessage(second_header))
        observer.setmocktime(second_time + 30)
        self.wait_for_log(observer, "expiring deferred PoS header " + second)
        assert_equal(peer.requested(), [int(first, 16)])
        assert_equal(observer.getpeerinfo()[0]["banscore"], 0)
        peer.send_and_ping(PoSHeaderMessage(second_header))
        wait_until(lambda: peer.requested() == [int(first, 16), int(second, 16)])
        peer.send_and_ping(RawBlockMessage(producer.getblock(second, 0)))
        wait_until(lambda: observer.getbestblockhash() == second)

        self.log.info("Malformed next-slot headers must still be penalized")
        third_time = second_time + 60
        producer.setmocktime(third_time)
        third = producer.generatestake()
        third_header = producer.getblockheader(third, False)
        malformed = bytearray.fromhex(third_header)
        struct.pack_into("<I", malformed, 72, 1)  # Nonzero nBits.
        peer.send_and_ping(PoSHeaderMessage(malformed.hex()))
        assert_equal(observer.getpeerinfo()[0]["banscore"], 20)

        self.log.info("Duplicate announcements must not reset the withholding timeout")
        peer.send_and_ping(PoSHeaderMessage(third_header))
        observer.setmocktime(third_time)
        wait_until(lambda: peer.requested()[-1] == int(third, 16))
        before = peer.requested()
        observer.setmocktime(third_time + 59)
        peer.send_and_ping(PoSHeaderMessage(third_header))
        assert_equal(peer.requested(), before)
        observer.setmocktime(third_time + 61)
        peer.wait_for_disconnect()
        self.wait_for_log(observer, "for withholding announced PoS block " + third)


if __name__ == "__main__":
    PoSHeaderTimeTest().main()
