#!/usr/bin/env python3
# Copyright (c) 2026 Verge
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Bound PoS downloads without admitting unauthenticated header chains."""

from io import BytesIO
from math import ceil
from pathlib import Path
import time

from test_framework.authproxy import JSONRPCException
from test_framework.messages import (
    MSG_BLOCK, NODE_NETWORK, deser_compact_size, deser_string, deser_uint256,
    ser_compact_size, ser_uint256,
)
from test_framework.mininode import MESSAGEMAP, P2PInterface, mininode_lock
from test_framework.test_framework import VergeTestFramework
from test_framework.util import (
    assert_equal, assert_raises_rpc_error, connect_nodes_bi,
    disconnect_nodes, wait_until,
)

NODE_POS_SYNC = 1 << 25
MAX_BATCH_BYTES = 2 * 1024 * 1024


class BatchRequest:
    command = b"getposblocks"

    def __init__(self, anchor=0):
        self.anchor = anchor

    def serialize(self):
        return ser_uint256(self.anchor)

    def deserialize(self, stream):
        self.anchor = deser_uint256(stream)


class BatchReply:
    command = b"posblocks"

    def __init__(self, anchor=0, blocks=(), more=False, count=None, padding=b""):
        self.raw = (ser_uint256(anchor) + bytes([more]) +
                    ser_compact_size(len(blocks) if count is None else count) +
                    b"".join(blocks) + padding)

    def serialize(self):
        return self.raw

    def deserialize(self, stream):
        self.raw = stream.read()
        decoded = BytesIO(self.raw)
        self.anchor = deser_uint256(decoded)
        self.more = decoded.read(1) == b"\x01"
        self.count = deser_compact_size(decoded)
        self.blocks = decoded.read()


class HeaderMessage:
    command = b"headers"

    def __init__(self, header=None):
        self.header = header

    def serialize(self):
        return b"\x00" if self.header is None else b"\x01" + self.header + b"\x00\x00"


class RawBlockMessage:
    command = b"block"

    def __init__(self, raw):
        self.raw = raw

    def serialize(self):
        return self.raw


class DownloadPeer(P2PInterface):
    def __init__(self, hashes, headers, blocks, serve=False):
        super().__init__()
        self.hashes = hashes
        self.headers = headers
        self.blocks = blocks
        self.positions = {block: i for i, block in enumerate(hashes)}
        self.serve = serve
        self.requests = []
        self.block_requests = []

    def on_inv(self, message):
        pass

    def on_posblocks(self, message):
        pass

    def on_getheaders(self, message):
        if not self.serve:
            return
        index = next((self.positions[block] + 1 for block in message.locator.vHave
                      if block in self.positions), 1)
        time.sleep(0.01)  # Controlled response latency for both transports.
        self.send_message(HeaderMessage(self.headers[index] if index < len(self.hashes) else None))

    def on_getdata(self, message):
        for inv in message.inv:
            if inv.type != MSG_BLOCK:
                continue
            self.block_requests.append(inv.hash)
            if self.serve and inv.hash in self.positions:
                time.sleep(0.01)
                self.send_message(RawBlockMessage(self.blocks[self.positions[inv.hash]]))

    def on_getposblocks(self, message):
        self.requests.append(message.anchor)
        if self.serve:
            index = self.positions[message.anchor] + 1
            batch = self.blocks[index:index + 8]
            time.sleep(0.01)
            self.send_message(BatchReply(message.anchor, batch,
                                         index + len(batch) < len(self.hashes)))

    def request_count(self):
        with mininode_lock:
            return len(self.requests)


MESSAGEMAP[b"getposblocks"] = BatchRequest
MESSAGEMAP[b"posblocks"] = BatchReply


class PoSPrefetchTest(VergeTestFramework):
    def set_test_params(self):
        self.num_nodes = 4
        self.setup_clean_chain = True
        self.rpc_timewait = 300
        self.extra_args = [["-posactivationheight=1500", "-without-tor"]
                           for _ in range(self.num_nodes)]

    def peer(self, node, capable=True, serve=False):
        peer = node.add_p2p_connection(
            DownloadPeer(self.hashes, self.headers, self.blocks, serve),
            services=NODE_NETWORK | (NODE_POS_SYNC if capable else 0))
        peer.send_and_ping(HeaderMessage(self.headers[node.getblockcount() - 1498]))
        return peer

    @staticmethod
    def log_contains(node, text):
        return text in (Path(node.datadir) / "regtest" / "debug.log").read_text(encoding="utf-8")

    def run_test(self):
        producer, observer, legacy, direct = self.nodes
        address = producer.getnewaddress()
        for _ in range(7):
            producer.generatetoaddress(100, address)
        producer.generatetoaddress(21, address)
        producer.createbond(1000)
        for count in [100] * 7 + [78]:
            producer.generatetoaddress(count, address)
        self.sync_all()
        assert_equal(producer.getblockcount(), 1499)
        for a in range(self.num_nodes):
            for b in range(self.num_nodes):
                if a != b:
                    disconnect_nodes(self.nodes[a], b)

        anchor = producer.getbestblockhash()
        timestamp = producer.getblockheader(anchor)["time"] // 30 * 30
        self.hashes = [int(anchor, 16)]
        self.headers = [bytes.fromhex(producer.getblockheader(anchor, False))]
        self.blocks = [bytes.fromhex(producer.getblock(anchor, 0))]
        timestamps = []
        for _ in range(146):
            while True:
                timestamp += 30
                producer.setmocktime(timestamp)
                try:
                    block = producer.generatestake()
                    break
                except JSONRPCException as error:
                    assert "wallet has no eligible bond" in error.error["message"]
            timestamps.append(timestamp)
            self.hashes.append(int(block, 16))
            self.headers.append(bytes.fromhex(producer.getblockheader(block, False)))
            self.blocks.append(bytes.fromhex(producer.getblock(block, 0)))
        for node in (observer, legacy, direct):
            node.setmocktime(timestamp)

        self.log.info("Serving respects the count/byte envelope and unknown anchors")
        server_peer = producer.add_p2p_connection(
            DownloadPeer(self.hashes, self.headers, self.blocks))
        assert server_peer.nServices & NODE_POS_SYNC
        server_peer.send_and_ping(BatchRequest(self.hashes[0]))
        reply = server_peer.last_message["posblocks"]
        assert_equal(reply.anchor, self.hashes[0])
        assert_equal(reply.count, 8)
        assert_equal(reply.more, True)
        assert_equal(reply.blocks, b"".join(self.blocks[1:9]))
        assert len(reply.raw) <= MAX_BATCH_BYTES
        server_peer.send_and_ping(BatchRequest(123))
        assert_equal(server_peer.last_message["posblocks"].count, 0)
        for _ in range(32):
            server_peer.send_and_ping(BatchRequest(self.hashes[0]))
        assert_equal(server_peer.last_message["posblocks"].count, 0)
        producer.disconnect_p2ps()

        self.log.info("One global batch; unsolicited and mismatched replies cannot advance sync")
        peer = self.peer(observer)
        wait_until(lambda: peer.request_count() == 1)
        other = self.peer(observer)
        assert_equal(other.request_count(), 0)
        other.send_and_ping(BatchReply(self.hashes[0], self.blocks[1:3]))
        peer.send_and_ping(BatchReply(123, self.blocks[1:3]))
        assert_equal(observer.getblockcount(), 1499)
        assert_raises_rpc_error(-5, "Block not found", observer.getblockheader,
                                "{:064x}".format(self.hashes[1]))
        observer.disconnect_p2ps()

        self.log.info("Malformed batches are rejected before admitting any prefix")
        bad_replies = [
            BatchReply(self.hashes[0], count=9),
            BatchReply(self.hashes[0], self.blocks[1:2], padding=b"\x00"),
            BatchReply(self.hashes[0], [self.blocks[2], self.blocks[1]]),
            BatchReply(self.hashes[0], padding=b"\x00" * MAX_BATCH_BYTES),
        ]
        truncated = BatchReply()
        truncated.raw = b"\x00"
        bad_replies.append(truncated)
        for bad in bad_replies:
            peer = self.peer(observer)
            wait_until(lambda: peer.request_count() == 1)
            peer.send_and_ping(bad)
            assert_equal(observer.getblockcount(), 1499)
            assert_equal(observer.getpeerinfo()[0]["banscore"], 20)
            observer.disconnect_p2ps()

        self.log.info("Invalid full-block authentication cannot admit descendants")
        peer = self.peer(observer)
        wait_until(lambda: peer.request_count() == 1)
        corrupt = bytearray(self.blocks[1])
        decoded = BytesIO(corrupt)
        decoded.seek(112)
        transactions = producer.getblock("{:064x}".format(self.hashes[1]), 2)["tx"]
        assert_equal(deser_compact_size(decoded), len(transactions))
        for tx in transactions:
            raw = bytes.fromhex(tx["hex"])
            assert_equal(decoded.read(len(raw)), raw)
        deser_string(decoded)  # Legacy block signature vector.
        assert_equal(decoded.read(1), b"\x01")
        # The authorization signature follows the 295-byte proof and 209-byte authorization.
        corrupt[decoded.tell() + 295 + 209] ^= 1
        peer.send_message(BatchReply(self.hashes[0], [bytes(corrupt), self.blocks[2]]))
        peer.wait_for_disconnect()
        assert_equal(observer.getblockcount(), 1499)
        assert_raises_rpc_error(-5, "Block not found", observer.getblockheader,
                                "{:064x}".format(self.hashes[2]))
        observer.disconnect_p2ps()

        self.log.info("Legacy block transport rejects the same damaged body without poisoning its ID")
        peer = self.peer(observer, capable=False)
        wait_until(lambda: self.hashes[1] in peer.block_requests)
        peer.send_message(RawBlockMessage(bytes(corrupt)))
        peer.wait_for_disconnect()
        assert_raises_rpc_error(-5, "Block not found", observer.getblockheader,
                                "{:064x}".format(self.hashes[1]))
        observer.disconnect_p2ps()

        self.log.info("Timeout is fixed and retains the connection for legacy fallback")
        peer = self.peer(observer)
        wait_until(lambda: peer.request_count() == 1)
        observer.setmocktime(timestamp + 59)
        peer.send_and_ping(BatchReply(123, self.blocks[1:2]))
        observer.setmocktime(timestamp + 61)
        wait_until(lambda: self.log_contains(observer, "PoS batch timeout"))
        peer.send_and_ping(HeaderMessage(self.headers[1]))
        wait_until(lambda: self.hashes[1] in peer.block_requests)
        assert_equal(observer.getpeerinfo()[0]["banscore"], 0)
        assert_equal(peer.request_count(), 1)
        observer.disconnect_p2ps()

        self.log.info("Empty/size-limited responses fall back without a retry loop")
        peer = self.peer(observer)
        wait_until(lambda: peer.request_count() == 1)
        peer.send_and_ping(BatchReply(self.hashes[0], more=True))
        peer.send_and_ping(HeaderMessage(self.headers[1]))
        wait_until(lambda: self.hashes[1] in peer.block_requests)
        assert_equal(peer.request_count(), 1)
        assert_equal(observer.getpeerinfo()[0]["banscore"], 0)
        observer.disconnect_p2ps()

        self.log.info("A future-slot tail is not accepted or blamed on honest peers")
        observer.setmocktime(timestamps[0])
        peer = self.peer(observer)
        wait_until(lambda: peer.request_count() == 1)
        peer.send_and_ping(BatchReply(self.hashes[0], self.blocks[1:3], more=True))
        assert_equal(observer.getblockcount(), 1500)
        assert_equal(observer.getpeerinfo()[0]["banscore"], 0)
        observer.disconnect_p2ps()
        # An invented child of the old PoW anchor must not trigger history replay.
        peer = observer.add_p2p_connection(
            DownloadPeer(self.hashes, self.headers, self.blocks),
            services=NODE_NETWORK | NODE_POS_SYNC)
        stale_header = bytearray(self.headers[1])
        stale_header[36] ^= 1
        peer.send_and_ping(HeaderMessage(bytes(stale_header)))
        assert_equal(peer.request_count(), 0)
        observer.disconnect_p2ps()
        observer.setmocktime(timestamp)

        self.log.info("Peers without the capability keep single-header/body download")
        old = self.peer(legacy, capable=False)
        wait_until(lambda: self.hashes[1] in old.block_requests)
        assert_equal(old.request_count(), 0)
        old.send_and_ping(RawBlockMessage(self.blocks[1]))
        assert_equal(legacy.getblockcount(), 1500)
        legacy.disconnect_p2ps()

        self.log.info("Real daemons negotiate and use full batches for historical catch-up")
        connect_nodes_bi(self.nodes, 0, 3)
        wait_until(lambda: direct.getbestblockhash() == producer.getbestblockhash(), timeout=120)
        assert any(peer.get("bytesrecv_per_msg", {}).get("posblocks", 0) > 0
                   for peer in direct.getpeerinfo())
        assert all(peer["banscore"] == 0 for peer in direct.getpeerinfo())

        self.log.info("Catch up 145 blocks with bounded batches versus legacy round trips")
        begin = time.monotonic()
        fast = self.peer(observer, serve=True)
        wait_until(lambda: observer.getbestblockhash() == producer.getbestblockhash(), timeout=120)
        fast_seconds = time.monotonic() - begin
        assert_equal(fast.request_count(), ceil(145 / 8))
        assert_equal(observer.getpeerinfo()[0]["banscore"], 0)
        observer.disconnect_p2ps()
        begin = time.monotonic()
        old = self.peer(legacy, capable=False, serve=True)
        wait_until(lambda: legacy.getbestblockhash() == producer.getbestblockhash(), timeout=120)
        legacy_seconds = time.monotonic() - begin
        assert_equal(old.request_count(), 0)
        assert_equal(len(set(old.block_requests)), 145)
        self.log.info("145 blocks: batch %.3fs / %d requests; legacy %.3fs / %d body requests",
                      fast_seconds, fast.request_count(), legacy_seconds, len(old.block_requests))
        legacy.disconnect_p2ps()

        self.log.info("Reload/reindex preserves the fully validated batch history")
        final_hash = producer.getbestblockhash()
        finality = producer.getposinfo()["finalized"]
        self.restart_node(1, self.extra_args[1] + ["-reindex=1", "-mocktime={}".format(timestamp)])
        wait_until(lambda: observer.getbestblockhash() == final_hash)
        assert_equal(observer.getposinfo()["finalized"], finality)
        connect_nodes_bi(self.nodes, 0, 1)
        self.sync_all()


if __name__ == "__main__":
    PoSPrefetchTest().main()
