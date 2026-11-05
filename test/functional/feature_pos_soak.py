#!/usr/bin/env python3
# Copyright (c) 2026 Verge
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Bounded multi-producer PoS soak with partitions, crashes and recovery."""

import json
from pathlib import Path
import random
import time

from test_framework.authproxy import JSONRPCException
from test_framework.memory import resident_bytes
from test_framework.test_framework import VergeTestFramework
from test_framework.util import assert_equal, connect_nodes_bi, disconnect_nodes, sync_blocks


class PoSSoakTest(VergeTestFramework):
    ACTIVATION_HEIGHT = 2000

    def add_options(self, parser):
        parser.add_argument("--rounds", type=int, default=120)
        parser.add_argument("--duration", type=int, default=0,
                            help="Minimum steady-state wall-clock seconds, in addition to --rounds")
        parser.add_argument("--bonds-per-node", type=int, default=16)
        parser.add_argument("--soak-seed", type=int, default=20261104)

    def set_test_params(self):
        self.num_nodes = 4
        self.setup_clean_chain = True
        self.rpc_timewait = 120
        self.extra_args = [["-posactivationheight={}".format(self.ACTIVATION_HEIGHT),
                            "-without-tor"] for _ in range(4)]

    @staticmethod
    def mine(node, count, address):
        while count:
            batch = min(count, 100)
            node.generatetoaddress(batch, address)
            count -= batch

    def advance(self):
        self.timestamp += 13 * 30
        for node in self.nodes:
            if node.running:
                # Keep real connections active while consensus time advances quickly.
                node.ping()
                node.setmocktime(self.timestamp)

    def mint(self, group):
        order = list(group)
        self.rng.shuffle(order)
        for node in order:
            try:
                node.generatestake()
                sync_blocks(group, timeout=120)
                self.metrics["produced_blocks"] += 1
                return True
            except JSONRPCException as error:
                assert "wallet has no eligible bond" in error.error["message"], error.error
        self.metrics["empty_group_slots"] += 1
        return False

    def compare(self, group=None):
        group = self.nodes if group is None else group
        sync_blocks(group, timeout=120)
        reference = group[0].getposinfo()
        for node in group:
            info = node.getposinfo()
            for field in ("best_block_hash", "state_block_hash", "tracked_bonds",
                          "tracked_bond_value", "latest_votes", "latest_vote_weight",
                          "initial_snapshot", "eligible_snapshot", "latest_checkpoint",
                          "latest_justified", "finalized"):
                assert_equal(info[field], reference[field])
            assert_equal(node.getblockchaininfo()["chaincurrentforservices"], True)
            for peer in node.getpeerinfo():
                self.metrics["maximum_banscore"] = max(self.metrics["maximum_banscore"], peer["banscore"])
                assert_equal(peer["banscore"], 0)
        rss = sum(resident_bytes(node.process.pid) for node in self.nodes if node.running)
        if not self.metrics["initial_observed_total_rss_bytes"]:
            self.metrics["initial_observed_total_rss_bytes"] = rss
        self.metrics["last_observed_total_rss_bytes"] = rss
        self.metrics["peak_observed_total_rss_bytes"] = max(self.metrics["peak_observed_total_rss_bytes"], rss)

    def partition(self):
        disconnect_nodes(self.nodes[1], 2)
        disconnect_nodes(self.nodes[2], 1)
        for _ in range(6):
            self.advance()
            self.mint(self.nodes[:2])
            self.mint(self.nodes[2:])
            self.compare(self.nodes[:2])
            self.compare(self.nodes[2:])
        connect_nodes_bi(self.nodes, 1, 2)
        self.compare()
        self.metrics["partitions"] += 1

    def restart_peer(self, abrupt):
        peer = self.nodes[3]
        if abrupt:
            peer.process.kill()
            peer.process.wait(timeout=30)
            peer.running = False
            peer.process = None
            peer.rpc_connected = False
            peer.rpc = None
            peer.stdout.close()
            peer.stderr.close()
            peer.p2ps.clear()
            self.metrics["abrupt_restarts"] += 1
        else:
            self.stop_node(3)
            self.metrics["clean_restarts"] += 1
        for _ in range(4):
            self.advance()
            self.mint(self.nodes[:3])
        self.start_node(3, self.extra_args[3] + ["-mocktime={}".format(self.timestamp)])
        # Refresh RPC connections after any lengthy restart verification.
        for node in self.nodes[:3]:
            node.wait_for_rpc_connection()
        connect_nodes_bi(self.nodes, 2, 3)
        self.compare()

    def report(self, started):
        self.metrics["elapsed_seconds"] = round(time.monotonic() - started, 3)
        self.metrics["height"] = self.nodes[0].getblockcount()
        self.metrics["finalized"] = self.nodes[0].getposinfo()["finalized"]
        report = Path(self.options.tmpdir) / "pos-soak-results.json"
        report.write_text(json.dumps(self.metrics, indent=2, sort_keys=True) + "\n", encoding="ascii")
        self.log.info("Soak: rounds=%d height=%d partitions=%d elapsed=%.1fs RSS=%d MiB",
                      self.metrics["rounds"], self.metrics["height"], self.metrics["partitions"],
                      self.metrics["elapsed_seconds"], self.metrics["peak_observed_total_rss_bytes"] // (1024 * 1024))

    def run_test(self):
        assert self.options.rounds >= 1 and self.options.duration >= 0
        assert 1 <= self.options.bonds_per_node <= 32
        self.rng = random.Random(self.options.soak_seed)
        self.metrics = {"copyright": "(c) 2026 Verge", "status": "in_progress",
            "rounds": 0, "produced_blocks": 0,
            "empty_group_slots": 0, "partitions": 0, "clean_restarts": 0,
            "abrupt_restarts": 0, "maximum_banscore": 0, "peak_observed_total_rss_bytes": 0,
            "initial_observed_total_rss_bytes": 0, "last_observed_total_rss_bytes": 0,
            "nodes": 4, "activation_height": self.ACTIVATION_HEIGHT,
            "bonds_per_node": self.options.bonds_per_node, "seed": self.options.soak_seed}
        addresses = [node.getnewaddress() for node in self.nodes]
        for index, node in enumerate(self.nodes):
            node.setstaking(False)
            self.mine(node, self.options.bonds_per_node, addresses[index])
            sync_blocks(self.nodes)
        self.mine(self.nodes[0], 720, addresses[0])
        sync_blocks(self.nodes)
        for index, node in enumerate(self.nodes):
            remaining = self.options.bonds_per_node
            while remaining:
                batch = min(remaining, 8)
                for _ in range(batch):
                    node.createbond(1000)
                self.mine(node, 1, addresses[index])
                sync_blocks(self.nodes)
                remaining -= batch
        self.mine(self.nodes[0], self.ACTIVATION_HEIGHT - 1 - self.nodes[0].getblockcount(), addresses[0])
        sync_blocks(self.nodes)
        for node in self.nodes:
            assert_equal(node.getstakinginfo()["eligible_owned_bonds"], self.options.bonds_per_node)
        self.timestamp = self.nodes[0].getblockheader(self.nodes[0].getbestblockhash())["time"] // 30 * 30
        for _ in range(40):
            self.advance()
            if self.mint(self.nodes):
                break
        else:
            raise AssertionError("No eligible bootstrap slot in 40 attempts")
        assert self.nodes[0].getblockcount() >= self.ACTIVATION_HEIGHT
        self.compare()
        started = time.monotonic()
        while (self.metrics["rounds"] < self.options.rounds or
               time.monotonic() - started < self.options.duration):
            self.advance()
            self.mint(self.nodes)
            self.compare()
            self.metrics["rounds"] += 1
            round_number = self.metrics["rounds"]
            if round_number % 20 == 0:
                self.partition()
            if round_number % 36 == 0:
                self.restart_peer(abrupt=round_number % 72 == 0)
            if round_number % 10 == 0:
                self.report(started)
        self.report(started)
        self.log.info("Reindex one participant after the soak and compare UTXO/state roots")
        self.restart_node(3, self.extra_args[3] + ["-mocktime={}".format(self.timestamp), "-reindex-chainstate"])
        for node in self.nodes[:3]:
            node.wait_for_rpc_connection()
        connect_nodes_bi(self.nodes, 2, 3)
        self.compare()
        roots = [node.gettxoutsetinfo()["hash_serialized_2"] for node in self.nodes]
        assert_equal(*roots)
        assert self.nodes[0].getposinfo()["finalized"]["height"] >= self.ACTIVATION_HEIGHT
        for node in self.nodes:
            with (Path(node.datadir) / "regtest" / "debug.log").open(encoding="utf-8") as log:
                for line in log:
                    assert "Misbehaving:" not in line, line
        self.metrics["status"] = "passed"
        self.report(started)


if __name__ == "__main__":
    PoSSoakTest().main()
