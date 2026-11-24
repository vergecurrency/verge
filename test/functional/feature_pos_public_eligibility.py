#!/usr/bin/env python3
# Copyright (c) 2026 The Verge Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Check public stake selection against an independent hash/interval implementation."""

from hashlib import sha256
from io import BytesIO
import struct

from test_framework.authproxy import JSONRPCException
from test_framework.messages import deser_compact_size, deser_string
from test_framework.test_framework import VergeTestFramework
from test_framework.util import assert_equal, connect_nodes_bi, sync_blocks, sync_mempools


class PublicEligibilityTest(VergeTestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        self.setup_clean_chain = True
        self.rpc_timewait = 300
        self.extra_args = [["-posactivationheight=1500", "-without-tor"] for _ in range(3)]

    def setup_network(self):
        self.setup_nodes()
        connect_nodes_bi(self.nodes, 0, 1)

    @staticmethod
    def mine(node, count, address):
        while count:
            batch = min(count, 100)
            node.generatetoaddress(batch, address)
            count -= batch

    @staticmethod
    def proof(node, block_hash):
        stream = BytesIO(bytes.fromhex(node.getblock(block_hash, 0)))
        stream.seek(112)
        transactions = node.getblock(block_hash, 2)["tx"]
        assert_equal(deser_compact_size(stream), len(transactions))
        for tx in transactions:
            raw = bytes.fromhex(tx["hex"])
            assert_equal(stream.read(len(raw)), raw)
        deser_string(stream)
        assert_equal(stream.read(1), b"\x02")
        proof = stream.read(295)
        assert_equal(len(proof), 295)
        return proof

    def check_selection(self, node, block_hash):
        proof = self.proof(node, block_hash)
        assert_equal(proof[0], 2)
        assert_equal(proof[117:150], bytes(33))
        assert_equal(proof[182:263], bytes(81))
        tag = sha256(b"VergePoS/PublicEligibility/v1").digest()
        total = sum(bond[2] for bond in self.bonds)
        threshold = (1 << 256) % total
        for draw in range(32):
            digest = sha256(tag + tag + struct.pack("<I", 3) + proof[45:117]
                            + proof[37:45] + struct.pack("<I", draw)).digest()
            value = int.from_bytes(digest, "little")
            if value < threshold:
                continue
            assert_equal(proof[150:182], digest)
            ticket = value % total
            for txid, vout, weight in self.bonds:
                if ticket < weight:
                    assert_equal(proof[1:33], txid)
                    assert_equal(struct.unpack("<I", proof[33:37])[0], vout)
                    return
                ticket -= weight
            raise AssertionError("Public ticket outside stake intervals")
        raise AssertionError("Public selection exhausted its draw bound")

    def run_test(self):
        first, second, fresh = self.nodes
        for node in self.nodes:
            node.setstaking(False)
        address = first.getnewaddress()
        self.mine(first, 1, address)
        sync_blocks([first, second])
        self.mine(second, 720, second.getnewaddress())
        sync_blocks([first, second])
        bonds = [first.createbond(1000), second.createbond(3000)]
        second.sendrawtransaction(first.getrawtransaction(bonds[0]["txid"]))
        first.sendrawtransaction(second.getrawtransaction(bonds[1]["txid"]))
        sync_mempools([first, second])
        self.mine(first, 778, address)
        sync_blocks([first, second])
        assert_equal(first.getblockcount(), 1499)
        self.bonds = sorted((bytes.fromhex(b["txid"])[::-1], b["vout"], weight)
                            for b, weight in zip(bonds, (1000000000, 3000000000)))
        for node in (first, second):
            assert_equal(node.getposinfo()["eligibility_scheme"], "public-stake-weighted-v1")
            assert_equal(node.getstakinginfo()["eligible_owned_bonds"], 1)
        timestamp = first.getblockheader(first.getbestblockhash())["time"] // 30 * 30
        produced = 0
        while produced < 30:
            timestamp += 30
            for node in self.nodes:
                node.setmocktime(timestamp)
            block_hash = None
            for node in (first, second):
                try:
                    block_hash = node.generatestake()
                    break
                except JSONRPCException as error:
                    assert "wallet has no eligible bond" in error.error["message"], error.error
            assert block_hash is not None, "An online owner of every snapshot bond must cover this slot"
            sync_blocks([first, second])
            self.check_selection(first, block_hash)
            produced += 1
        connect_nodes_bi(self.nodes, 0, 2)
        sync_blocks(self.nodes)
        expected = first.getbestblockhash()
        self.restart_node(1, self.extra_args[1] + ["-reindex-chainstate", "-mocktime={}".format(timestamp)])
        connect_nodes_bi(self.nodes, 0, 1)
        sync_blocks(self.nodes)
        assert_equal(second.getbestblockhash(), expected)
        assert_equal(fresh.getbestblockhash(), expected)
        for node in self.nodes:
            assert not any(peer.get("banscore", 0) for peer in node.getpeerinfo())


if __name__ == "__main__":
    PublicEligibilityTest().main()
