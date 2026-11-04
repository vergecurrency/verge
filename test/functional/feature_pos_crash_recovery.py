#!/usr/bin/env python3
# Copyright (c) 2026 Verge
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Recover PoS state and UTXOs after abrupt exits and interrupted flushes."""

import http.client

from test_framework.messages import msg_generic
from test_framework.mininode import P2PInterface
from test_framework.test_framework import VergeTestFramework
from test_framework.util import assert_equal, connect_nodes_bi, sync_blocks


class BlockDeliveryPeer(P2PInterface):
    def on_inv(self, message):
        pass


class PoSCrashRecoveryTest(VergeTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.rpc_timewait = 60
        self.base_args = ["-posactivationheight=1500", "-without-tor"]
        self.crash_args = ["-dbbatchsize=1", "-dbcrashratio=1"]
        self.extra_args = [list(self.base_args) for _ in range(self.num_nodes)]

    @staticmethod
    def mine(node, count, address):
        while count:
            batch = min(count, 100)
            node.generatetoaddress(batch, address)
            count -= batch

    def stake(self, slots=1):
        self.timestamp += slots * 30
        for node in self.nodes:
            if node.running:
                node.setmocktime(self.timestamp)
        return self.nodes[0].generatestake()

    def restart_args(self, crash=False):
        args = self.base_args + ["-mocktime={}".format(self.timestamp), "-connect=0"]
        return args + self.crash_args if crash else args

    @staticmethod
    def forget_exited_process(node):
        node.running = False
        node.process = None
        node.rpc_connected = False
        node.rpc = None
        node.stdout.close()
        node.stderr.close()
        node.p2ps.clear()

    def kill_node(self, node):
        node.process.kill()
        node.process.wait(timeout=30)
        self.forget_exited_process(node)

    def crash_during(self, node, operation):
        with node.assert_debug_log(["Simulating a crash. Goodbye."]):
            try:
                operation()
            except (OSError, http.client.HTTPException):
                pass
            assert_equal(node.process.wait(timeout=30), 0)
            self.forget_exited_process(node)

    @staticmethod
    def assert_state_matches_tip(node):
        info = node.getposinfo()
        assert_equal(info["state_block_hash"], node.getbestblockhash())
        assert_equal(node.getblockchaininfo()["initialblockdownload"], False)
        assert not any(tip["status"] == "invalid" for tip in node.getchaintips())
        return info

    def assert_same_state(self):
        source, recovered = self.nodes
        source_info = self.assert_state_matches_tip(source)
        recovered_info = self.assert_state_matches_tip(recovered)
        for field in ("state_block_hash", "tracked_bonds", "tracked_bond_value",
                      "eligible_snapshot", "latest_justified", "finalized"):
            assert_equal(recovered_info[field], source_info[field])
        # Querying the UTXO hash deliberately flushes both databases only after
        # restart has succeeded; doing it before the crash would mask the bug.
        assert_equal(recovered.gettxoutsetinfo()["hash_serialized_2"],
                     source.gettxoutsetinfo()["hash_serialized_2"])

    def run_test(self):
        source, recovered = self.nodes
        address = source.getnewaddress()
        self.mine(source, 721, address)
        source.createbond(1000)
        self.mine(source, 778, address)
        sync_blocks(self.nodes)
        self.timestamp = source.getblockheader(source.getbestblockhash())["time"]
        self.timestamp = self.timestamp // 30 * 30
        self.stake()
        sync_blocks(self.nodes)

        self.log.info("Abrupt exit after finality advances must not require reindex")
        # Save an early durable UTXO tip, then advance several checkpoints.
        self.restart_node(1, self.restart_args())
        connect_nodes_bi(self.nodes, 0, 1)
        for _ in range(6):
            self.stake(slots=120)
            sync_blocks(self.nodes)
        expected = source.getbestblockhash()
        assert recovered.getposinfo()["finalized"]["height"] > 1500
        self.kill_node(recovered)
        self.start_node(1, self.restart_args())
        assert_equal(recovered.getbestblockhash(), expected)
        self.assert_same_state()

        self.log.info("Recover an unflushed suffix and resume normal synchronization")
        connect_nodes_bi(self.nodes, 0, 1)
        for _ in range(4):
            self.stake()
            sync_blocks(self.nodes)
        expected = source.getbestblockhash()
        self.kill_node(recovered)
        self.start_node(1, self.restart_args())
        self.assert_state_matches_tip(recovered)
        connect_nodes_bi(self.nodes, 0, 1)
        sync_blocks(self.nodes)
        assert_equal(recovered.getbestblockhash(), expected)
        self.assert_same_state()

        self.log.info("Crash during the UTXO flush that precedes finality pruning")
        self.restart_node(1, self.restart_args(crash=True))
        # Deliver complete blocks explicitly to control when the flush starts.
        peer = recovered.add_p2p_connection(BlockDeliveryPeer())
        first = self.stake(slots=120)
        peer.send_and_ping(msg_generic(b"block", bytes.fromhex(source.getblock(first, 0))))
        assert_equal(recovered.getbestblockhash(), first)
        parent = first
        second = self.stake()
        self.crash_during(recovered,
                          lambda: peer.send_message(msg_generic(
                              b"block", bytes.fromhex(source.getblock(second, 0)))))

        self.log.info("A second crash during startup replay must remain recoverable")
        with recovered.assert_debug_log(["Replaying blocks", "Simulating a crash. Goodbye."]):
            recovered.start(self.restart_args(crash=True))
            assert_equal(recovered.process.wait(timeout=30), 0)
            self.forget_exited_process(recovered)
        with recovered.assert_debug_log(["Replaying blocks"]):
            self.start_node(1, self.restart_args())
        assert_equal(recovered.getbestblockhash(), second)
        self.assert_same_state()

        self.log.info("Keep PoS undo while disconnecting the durable UTXO tip")
        self.restart_node(1, self.restart_args(crash=True))
        self.crash_during(recovered, lambda: recovered.invalidateblock(second))
        with recovered.assert_debug_log([
                "Replaying blocks",
                "LoadPoSState: recovered state to chainstate tip {}".format(parent)]):
            self.start_node(1, self.restart_args())
        # The crash preceded persistence of invalidateblock's failure flag;
        # the already validated block may be selected again during startup.
        assert_equal(recovered.getbestblockhash(), second)
        self.assert_same_state()

        self.log.info("Recovered nodes can keep producing and following PoS blocks")
        connect_nodes_bi(self.nodes, 0, 1)
        final_block = self.stake()
        sync_blocks(self.nodes)
        assert_equal(recovered.getbestblockhash(), final_block)
        self.assert_same_state()


if __name__ == "__main__":
    PoSCrashRecoveryTest().main()
