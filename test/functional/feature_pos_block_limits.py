#!/usr/bin/env python3
# Copyright (c) 2026 The Verge Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Exercise larger PoS blocks, PQ verification capacity, relay and disk replay."""

from pathlib import Path

from test_framework.test_framework import VergeTestFramework
from test_framework.util import (
    assert_equal, connect_nodes_bi, disconnect_nodes, sync_blocks, wait_until,
)


class PoSBlockLimitsTest(VergeTestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        self.setup_clean_chain = True
        self.rpc_timewait = 300
        self.base_args = ["-posactivationheight=1500", "-without-tor",
                          "-rpcservertimeout=600"]
        self.extra_args = [list(self.base_args) for _ in range(self.num_nodes)]

    @staticmethod
    def mine(node, count, address):
        while count:
            batch = min(count, 100)
            node.generatetoaddress(batch, address)
            count -= batch

    def stake(self):
        self.timestamp += 30
        for node in self.nodes:
            node.setmocktime(self.timestamp)
        return self.nodes[0].generatestake()

    def run_test(self):
        producer, observer, historical = self.nodes
        mining = producer.getnewaddress("mining", "legacy")
        producer.backupwallet(str(Path(self.options.tmpdir) / "producer-backup.dat"))
        self.mine(producer, 721, mining)
        producer.createbond(1000)
        self.mine(producer, 777, mining)
        assert_equal(producer.getblockcount(), 1498)
        self.sync_all()

        self.log.info("PoW templates retain their historical resource limits")
        template = producer.getblocktemplate()
        assert_equal(template["sizelimit"], 4000000)
        assert_equal(template["weightlimit"], 4000000)
        assert_equal(template["sigoplimit"], 80000)

        self.log.info("Funding independent PQ payments before the activation boundary")
        sources = [producer.getnewpqaddress("capacity") for _ in range(2000)]
        recipient = observer.getnewpqaddress("recipient")
        change = producer.getnewpqaddress("change")
        producer.backupwallet(str(Path(self.options.tmpdir) / "funding-keys.dat"))
        raw = producer.createrawtransaction([], {address: 10 for address in sources})
        funded = producer.fundrawtransaction(raw)
        signed = producer.signrawtransactionwithwallet(funded["hex"])
        assert_equal(signed["complete"], True)
        funding_id = producer.sendrawtransaction(signed["hex"])
        funding = producer.getrawtransaction(funding_id, True)
        self.mine(producer, 1, mining)
        self.sync_all()
        assert_equal(producer.getblockcount(), 1499)
        source_set = set(sources)
        outputs = [output for output in funding["vout"]
                   if source_set.intersection(output["scriptPubKey"].get("addresses", []))]
        assert_equal(len(outputs), 2000)
        for other in (producer, observer):
            disconnect_nodes(historical, other.index)
            disconnect_nodes(other, historical.index)

        anchor = producer.getbestblockhash()
        self.timestamp = producer.getblockheader(anchor)["time"] // 30 * 30 + 30
        for node in self.nodes:
            node.setmocktime(self.timestamp)

        self.log.info("Submitting 2,000 real single-input PQ payments")
        for output in outputs:
            raw = producer.createrawtransaction(
                [{"txid": funding_id, "vout": output["n"]}],
                {recipient: 1, change: 8})
            signed = producer.signrawtransactionwithwallet(raw)
            assert_equal(signed["complete"], True)
            producer.sendrawtransaction(signed["hex"])
        # Block relay must work without waiting for every peer's transaction trickle.
        assert_equal(producer.getmempoolinfo()["size"], 2000)

        large_hash = self.stake()
        block = producer.getblock(large_hash)
        self.log.info("Produced PoS block %s: %d bytes, %d payments",
                      large_hash, block["size"], len(block["tx"]) - 1)
        assert 7000000 < block["size"] <= 8000000, block["size"]
        assert block["weight"] <= 32000000
        assert len(block["tx"]) > 1800
        assert_equal(block["height"], 1500)
        sync_blocks([producer, observer], timeout=300)
        assert_equal(observer.getblock(large_hash)["size"], block["size"])

        self.log.info("A historical peer falls back from bounded prefetch to a full block")
        connect_nodes_bi(self.nodes, 0, 2)
        sync_blocks(self.nodes, timeout=300)
        assert_equal(historical.getblock(large_hash)["size"], block["size"])
        assert_equal(historical.getbestblockhash(), large_hash)
        assert all(peer["banscore"] == 0 for peer in historical.getpeerinfo())
        if producer.getmempoolinfo()["size"]:
            self.stake()
            self.sync_all()
        assert_equal(producer.getmempoolinfo()["size"], 0)

        self.log.info("Restart and full reindex preserve the larger block")
        expected_tip = historical.getbestblockhash()
        self.restart_node(2, self.base_args + ["-reindex", "-checkblocks=0",
                                             "-mocktime={}".format(self.timestamp)])
        wait_until(lambda: historical.getbestblockhash() == expected_tip, timeout=300)
        assert_equal(historical.getbestblockhash(), expected_tip)
        assert_equal(historical.getblock(large_hash)["size"], block["size"])


if __name__ == '__main__':
    PoSBlockLimitsTest().main()
