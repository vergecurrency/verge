#!/usr/bin/env python3
# Copyright (c) 2026 Verge
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Exercise sustained mining RPCs with wallet-enabled and walletless nodes."""

from concurrent.futures import ThreadPoolExecutor
from base64 import b64encode
import http.client
import json
from pathlib import Path
import time
from urllib.parse import unquote, urlsplit

from test_framework.memory import process_memory
from test_framework.test_framework import VergeTestFramework
from test_framework.util import assert_equal, get_rpc_proxy


class MemoryUsageTest(VergeTestFramework):
    def add_options(self, parser):
        parser.add_argument("--requests", type=int, default=1000,
                            help="Minimum template requests per daemon")
        parser.add_argument("--duration", type=int, default=0,
                            help="Minimum steady-state seconds in addition to --requests")
        parser.add_argument("--reference-binary", help="Run both nodes with an older daemon for comparison")
        parser.add_argument("--disconnect-requests", type=int, default=64,
                            help="Abandoned long-poll requests before the final block")

    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.rpc_timewait = 120
        self.extra_args = [["-without-tor", "-dbcache=32", "-maxmempool=20"],
                           ["-without-tor", "-dbcache=32", "-maxmempool=20", "-disablewallet=1"]]

    def setup_nodes(self):
        binary = self.options.reference_binary or self.options.verged
        self.add_nodes(2, self.extra_args, binary=[binary, binary])
        self.start_nodes()
        self.import_deterministic_coinbase_privkeys()

    @staticmethod
    def templates(url, index, count):
        rpc = get_rpc_proxy(url, index, timeout=120)
        size = 0
        for _ in range(count):
            template = rpc.getblocktemplate({}, "scrypt")
            assert "previousblockhash" in template
            size = max(size, sum(len(tx["data"]) // 2 for tx in template["transactions"]))
        return size

    def sample(self):
        for index, node in enumerate(self.nodes):
            snapshot = {"process": process_memory(node.process.pid), "rpc": node.getmemoryinfo()}
            self.metrics["last"][index] = snapshot
            for key, value in snapshot["process"].items():
                self.metrics["peak"][index][key] = max(self.metrics["peak"][index].get(key, 0), value)
            components = snapshot["rpc"].get("components")
            if not self.options.reference_binary:
                assert components is not None
            if components:
                assert_equal(components["log_buffer_bytes"], 0)
                assert_equal(components["log_buffer_messages"], 0)
                assert components["validation_callbacks_pending"] < 100

    def report(self, started):
        self.metrics["elapsed_seconds"] = round(time.monotonic() - started, 3)
        report = Path(self.options.tmpdir) / "memory-results.json"
        report.write_text(json.dumps(self.metrics, sort_keys=True, indent=2) + "\n", encoding="ascii")

    def abandon_longpolls(self):
        target = self.nodes[1]
        template = target.getblocktemplate({}, "scrypt")
        url = urlsplit(target.url)
        credentials = "{}:{}".format(unquote(url.username), unquote(url.password)).encode("utf-8")
        headers = {"Authorization": "Basic " + b64encode(credentials).decode("ascii"),
                   "Content-Type": "application/json"}
        payload = json.dumps({"id": 1, "method": "getblocktemplate",
                              "params": [{"longpollid": template["longpollid"]}, "scrypt"]})
        for _ in range(self.options.disconnect_requests):
            connection = http.client.HTTPConnection(url.hostname, url.port, timeout=10)
            try:
                connection.request("POST", url.path or "/", payload, headers)
                time.sleep(0.01)
            finally:
                connection.close()
        # A separate producer wakes long-poll workers even if all RPC threads
        # on the target are occupied by clients that have gone away.
        self.nodes[0].generatetoaddress(1, self.mining_address)
        self.sync_all()

    def run_test(self):
        assert self.options.requests > 0 and self.options.duration >= 0
        assert 0 <= self.options.disconnect_requests <= 4096
        source = self.nodes[0]
        address = source.getnewaddress()
        self.mining_address = address
        source.generatetoaddress(100, address)
        source.generatetoaddress(100, address)
        self.sync_all()
        recipients = {source.getnewaddress(): 1 for _ in range(100)}
        for _ in range(20):
            txid = source.sendmany("", recipients)
            # The workload tests RPC allocation, not randomized relay timing.
            self.nodes[1].sendrawtransaction(source.getrawtransaction(txid))
        self.sync_all()
        self.metrics = {"copyright": "(c) 2026 Verge", "status": "in_progress",
                        "reference": bool(self.options.reference_binary),
                        "requests_per_node": 0, "template_transaction_bytes": 0,
                        "abandoned_longpoll_requests": self.options.disconnect_requests,
                        "baseline": [], "last": [{}, {}], "peak": [{}, {}]}
        urls = [node.url for node in self.nodes]
        for index, url in enumerate(urls):
            self.metrics["template_transaction_bytes"] = max(self.metrics["template_transaction_bytes"],
                self.templates(url, index, 50))
            self.metrics["baseline"].append(process_memory(self.nodes[index].process.pid))
        assert self.metrics["template_transaction_bytes"] > 10000
        self.sample()
        started = time.monotonic()
        with ThreadPoolExecutor(max_workers=4) as workers:
            while (self.metrics["requests_per_node"] < self.options.requests or
                   time.monotonic() - started < self.options.duration):
                jobs = [(index, workers.submit(self.templates, urls[index], index, 25))
                        for index in range(2) for _ in range(2)]
                for _, job in jobs:
                    assert job.result() > 10000
                self.metrics["requests_per_node"] += 50
                self.sample()
                if self.metrics["requests_per_node"] % 500 == 0:
                    self.report(started)
                    self.log.info("Memory load: %d requests/node, RSS=%s",
                        self.metrics["requests_per_node"],
                        [entry.get("resident_bytes", 0) for entry in self.metrics["peak"]])
        self.log.info("Abandon long-poll clients, accept another block, and check RPC recovery")
        self.abandon_longpolls()
        for node in self.nodes:
            assert_equal(node.getblocktemplate({}, "scrypt")["previousblockhash"], node.getbestblockhash())
        self.sample()
        for index in range(2):
            for key in ("resident_bytes", "private_bytes", "anonymous_resident_bytes"):
                if key in self.metrics["baseline"][index]:
                    growth = self.metrics["peak"][index][key] - self.metrics["baseline"][index][key]
                    assert growth < 64 * 1024 * 1024, (index, key, growth)
        self.metrics["status"] = "passed"
        self.report(started)


if __name__ == "__main__":
    MemoryUsageTest().main()
