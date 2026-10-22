#!/usr/bin/env python3
# Copyright (c) 2026 Verge
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the 10-block automatic rollback limit without changing branch scores."""

from test_framework.test_framework import VergeTestFramework
from test_framework.util import assert_equal, connect_nodes, disconnect_nodes, sync_blocks, wait_until


class ReorgLimitTest(VergeTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 3
        self.extra_args = [["-without-tor=1", "-smsg=0"] for _ in range(self.num_nodes)]

    def setup_network(self):
        self.setup_nodes()

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        local, remote, fresh = self.nodes
        limit = 10
        self.log.info("An automatic rollback of exactly %d blocks is allowed", limit)
        local.rpc.generate(limit)
        accepted_tip = remote.rpc.generate(limit + 1)[-1]
        connect_nodes(local, 1)
        sync_blocks([local, remote])
        assert_equal(local.getbestblockhash(), accepted_tip)
        disconnect_nodes(local, 1)

        self.log.info("A competing branch needing %d rollbacks must remain inactive", limit + 1)
        local_tip = local.rpc.generate(limit + 1)[-1]
        blocked_tip = remote.rpc.generate(limit + 2)[-1]
        connect_nodes(local, 1)
        wait_until(lambda: local.getblockchaininfo()["headers"] == 2 * limit + 3)
        wait_until(lambda: local.getblockheader(blocked_tip)["nTx"] > 0)
        assert_equal(local.getbestblockhash(), local_tip)
        assert_equal(local.getblockcount(), 2 * limit + 2)
        info = local.getblockchaininfo()
        assert_equal(info["maxreorgdepth"], limit)
        assert_equal(info["reorgprotectedheight"], limit + 2)
        assert_equal(info["reorgprotectedhash"], local.getblockhash(limit + 2))
        local.preciousblock(blocked_tip)
        local.reconsiderblock(blocked_tip)
        assert_equal(local.getbestblockhash(), local_tip)

        self.log.info("The blocked candidate must not prevent a local extension")
        local_tip = local.rpc.generate(1)[-1]
        assert_equal(local.getbestblockhash(), local_tip)
        disconnect_nodes(local, 1)
        self.restart_node(0, self.extra_args[0])
        assert_equal(local.getbestblockhash(), local_tip)

        self.log.info("A fresh node can still bootstrap a chain longer than %d blocks", limit)
        connect_nodes(fresh, 1)
        sync_blocks([fresh, remote])
        assert_equal(fresh.getbestblockhash(), blocked_tip)

        self.log.info("Explicit operator invalidation can enable deliberate recovery")
        local.invalidateblock(local.getblockhash(limit + 2))
        local.reconsiderblock(blocked_tip)
        assert_equal(local.getbestblockhash(), blocked_tip)


if __name__ == "__main__":
    ReorgLimitTest().main()
