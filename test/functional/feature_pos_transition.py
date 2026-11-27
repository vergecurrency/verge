#!/usr/bin/env python3
# Copyright (c) 2026 Verge
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Converge across final-PoW races without weakening certified finality."""

from feature_pos_activation import PoSActivationTest
from test_framework.authproxy import JSONRPCException
from test_framework.util import (
    assert_equal, assert_raises_rpc_error, connect_nodes_bi, disconnect_nodes,
    sync_blocks, wait_until,
)


class PoSTransitionTest(PoSActivationTest):
    def set_test_params(self):
        super().set_test_params()

    def stake(self, producer):
        for _ in range(100):
            self.timestamp += 30
            for node in self.nodes:
                node.setmocktime(self.timestamp)
            try:
                return producer.generatestake()
            except JSONRPCException as error:
                assert "wallet has no eligible bond" in error.error["message"]
        raise AssertionError("No eligible producer slot in 100 attempts")

    def isolate(self):
        disconnect_nodes(self.nodes[0], 1)
        disconnect_nodes(self.nodes[1], 0)

    def args(self, *extra):
        return self.extra_args[0] + ["-mocktime={}".format(self.timestamp)] + list(extra)

    def run_test(self):
        for i, wallet in enumerate(self.nodes):
            wallet.backupwallet(self.options.tmpdir + "/pq-keypool-{}.dat".format(i))
        first, second = self.nodes
        address = first.getnewaddress("mining", "legacy")
        self.mine_blocks(first, 721, address)
        self.mine_blocks(second, 720, second.getnewaddress("mining", "legacy"))
        first_bond = first.createbond(1500)
        second_bond = second.createbond(1000)
        first.sendrawtransaction(second.getrawtransaction(second_bond["txid"]))
        second.sendrawtransaction(first.getrawtransaction(first_bond["txid"]))
        self.mine_blocks(first, self.activation_height - 2 - 1441, address)
        self.isolate()
        pow_a = first.generatetoaddress(1, address)[0]
        pow_b = second.generatetoaddress(1, second.getnewaddress("mining", "legacy"))[0]
        assert pow_a != pow_b
        self.timestamp = max(first.getblockheader(pow_a)["time"],
                             second.getblockheader(pow_b)["time"]) // 30 * 30
        a0 = self.stake(first)
        assert_equal(first.getposinfo()["finalized"], {})

        self.log.info("P2P must fetch a competing last-PoW body and follow its PoS successor")
        connect_nodes_bi(self.nodes, 0, 1)
        sync_blocks(self.nodes, timeout=120)
        assert_equal(second.getbestblockhash(), a0)
        assert_equal(second.getposinfo()["finalized"], {})
        self.isolate()
        a1 = self.stake(first)
        second.invalidateblock(pow_a)
        assert_equal(second.getbestblockhash(), pow_b)
        b0 = self.stake(second)
        b1 = self.stake(second)
        assert_equal(second.getposinfo()["finalized"], {})
        second.reconsiderblock(pow_a)

        for block_hash in (pow_b, b0, b1):
            first.submitblock(second.getblock(block_hash, 0))
        for block_hash in (pow_a, a0, a1):
            second.submitblock(first.getblock(block_hash, 0))
        selected = a1 if bytes.fromhex(pow_a)[::-1] < bytes.fromhex(pow_b)[::-1] else b1
        for node in self.nodes:
            assert_equal(node.getbestblockhash(), selected)
            assert_equal(node.getposinfo()["finalized"], {})
        winner, loser = (first, second) if selected == a1 else (second, first)
        selected_pow = pow_a if selected == a1 else pow_b
        connect_nodes_bi(self.nodes, 0, 1)
        sync_blocks(self.nodes)

        self.log.info("Reindex and restart preserve the same uncertified transition choice")
        self.restart_node(0, self.args("-reindex-chainstate"))
        wait_until(lambda: first.getbestblockhash() == selected, timeout=120)
        self.restart_node(1, self.args())
        assert_equal(second.getbestblockhash(), selected)
        connect_nodes_bi(self.nodes, 0, 1)
        sync_blocks(self.nodes)

        self.log.info("A later target epoch permits safe voting after an uncertified anchor change")
        activation_slot = first.getblockheader(selected_pow)["time"] // 30 + 1
        self.timestamp = max(self.timestamp, (activation_slot + 120) * 30 - 30)
        self.stake(winner)
        sync_blocks(self.nodes)
        self.stake(loser)
        sync_blocks(self.nodes)
        self.stake(winner)
        sync_blocks(self.nodes)
        assert_equal(first.getposinfo()["latest_justified"]["epoch"], 1)

        self.timestamp = max(self.timestamp, (activation_slot + 240) * 30 - 30)
        for producer in (winner, loser, winner):
            self.stake(producer)
            sync_blocks(self.nodes)
        finality = first.getposinfo()["finalized"]
        assert_equal(finality["epoch"], 1)
        assert_raises_rpc_error(-20, "cannot invalidate finalized PoS history",
                                first.invalidateblock, finality["hash"])

        self.log.info("Signed PoS RPC submission works with a locked or absent wallet")
        self.isolate()
        first.encryptwallet("isolated-transition-test")
        first.wait_until_stopped()
        self.start_node(0, self.args())
        assert_equal(first.getwalletinfo()["unlocked_until"], 0)
        signed = self.stake(second)
        raw = second.getblock(signed, 0)
        assert_equal(first.submitblock(raw), None)
        assert_equal(first.submitblock(raw), "duplicate")
        assert_equal(first.getbestblockhash(), signed)

        self.restart_node(0, self.args("-disablewallet"))
        signed = self.stake(second)
        raw = second.getblock(signed, 0)
        stream = self.extension_stream(second, signed)
        offset = stream.tell() + 1 + 295 + 273 - 64
        damaged = bytearray.fromhex(raw)
        damaged[offset:offset + 64] = bytes(64)
        assert_equal(first.submitblock(damaged.hex()), "bad-pos-signature")
        assert_equal(first.submitblock(raw), None)
        assert_equal(first.getbestblockhash(), signed)
        assert_equal(first.getposinfo()["finalized"], second.getposinfo()["finalized"])


if __name__ == "__main__":
    PoSTransitionTest().main()
