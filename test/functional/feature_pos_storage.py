#!/usr/bin/env python3
# Copyright (c) 2026 Verge
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Exercise PoS record persistence, optional legacy migration and reindex."""

from test_framework.test_framework import VergeTestFramework
from test_framework.test_node import ErrorMatch
from test_framework.util import assert_equal, connect_nodes_bi, sync_blocks, wait_until


class PoSStorageTest(VergeTestFramework):
    def add_options(self, parser):
        parser.add_argument("--legacy-binary", help="Previous daemon using the single-state database record")

    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.rpc_timewait = 120
        self.extra_args = [["-posactivationheight=1500", "-without-tor"] for _ in range(2)]

    def setup_nodes(self):
        self.add_nodes(2, self.extra_args, binary=[self.options.verged,
            self.options.legacy_binary or self.options.verged])
        self.start_nodes()
        self.import_deterministic_coinbase_privkeys()

    @staticmethod
    def use_binary(node, binary):
        node.binary = binary
        node.args[0] = binary

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

    def restart_args(self, *extra):
        return self.extra_args[1] + ["-mocktime={}".format(self.timestamp)] + list(extra)

    def compare(self):
        sync_blocks(self.nodes, timeout=120)
        first, second = [node.getposinfo() for node in self.nodes]
        for field in ("best_block_hash", "state_block_hash", "tracked_bonds",
                      "tracked_bond_value", "latest_votes", "latest_vote_weight",
                      "initial_snapshot", "latest_checkpoint", "latest_justified", "finalized"):
            assert_equal(first[field], second[field])
        assert_equal(self.nodes[0].gettxoutsetinfo()["hash_serialized_2"],
                     self.nodes[1].gettxoutsetinfo()["hash_serialized_2"])

    def run_test(self):
        source, upgraded = self.nodes
        for node in self.nodes:
            node.setstaking(False)
        address = source.getnewaddress()
        self.mine(source, 721, address)
        source.createbond(1000)
        self.mine(source, 778, address)
        sync_blocks(self.nodes)
        self.timestamp = source.getblockheader(source.getbestblockhash())["time"] // 30 * 30
        self.stake()
        self.compare()

        self.log.info("Upgrade an existing PoS database in place")
        self.stop_node(1)
        self.use_binary(upgraded, self.options.verged)
        self.start_node(1, self.restart_args())
        connect_nodes_bi(self.nodes, 0, 1)
        if self.options.legacy_binary:
            with upgraded.assert_debug_log(["Migrated PoS state to record database version 1"]):
                self.stake()
                self.compare()
        else:
            self.stake()
            self.compare()
        for _ in range(12):
            self.stake(slots=120)
            self.compare()

        self.log.info("Restart and verify durable record state after retention pruning")
        with upgraded.assert_debug_log(["Loaded PoS record state version=1"]):
            self.restart_node(1, self.restart_args("-checklevel=4", "-checkblocks=30"))
        connect_nodes_bi(self.nodes, 0, 1)
        self.compare()

        if self.options.legacy_binary:
            self.log.info("An old binary must not silently reinterpret the migrated state")
            self.stop_node(1)
            self.use_binary(upgraded, self.options.legacy_binary)
            with upgraded.assert_debug_log(["missing state at or after initial snapshot height"]):
                upgraded.assert_start_raises_init_error(self.restart_args(),
                    "Error initializing block database", match=ErrorMatch.PARTIAL_REGEX)
            self.use_binary(upgraded, self.options.verged)
            self.start_node(1, self.restart_args())
            # The deliberate failed startup can outlast an idle HTTP connection.
            source.wait_for_rpc_connection()
            connect_nodes_bi(self.nodes, 0, 1)
            self.compare()

        for flag in ("-reindex-chainstate", "-reindex"):
            self.log.info("Rebuild with %s and compare reconstructed state", flag)
            expected = source.getbestblockhash()
            self.restart_node(1, self.restart_args(flag))
            wait_until(lambda: upgraded.getbestblockhash() == expected, timeout=120)
            connect_nodes_bi(self.nodes, 0, 1)
            self.compare()
        self.stake()
        self.compare()


if __name__ == "__main__":
    PoSStorageTest().main()
