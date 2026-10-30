#!/usr/bin/env python3
# Copyright (c) 2026 Verge
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Walletless nodes do not advertise or dispatch secure messaging."""

from test_framework.messages import NODE_NETWORK, msg_generic
from test_framework.mininode import P2PInterface
from test_framework.test_framework import SkipTest, VergeTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error


NODE_SMSG = 1 << 24


class WalletlessMessagingTest(VergeTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [['-without-tor', '-dnsseed=0']]

    def skip_test_if_missing_module(self):
        if self.is_wallet_compiled():
            raise SkipTest('This test requires a --disable-wallet build.')

    def run_test(self):
        node = self.nodes[0]
        assert_equal(int(node.getnetworkinfo()['localservices'], 16) & NODE_SMSG, 0)
        assert_raises_rpc_error(-32601, 'Method not found', node.getwalletinfo)
        assert_raises_rpc_error(-32601, 'Method not found', node.smsgenable)
        peer = node.add_p2p_connection(P2PInterface(), services=NODE_NETWORK | NODE_SMSG)
        try:
            peer.wait_for_verack()
            assert_equal(peer.nServices & NODE_SMSG, 0)
            for command in (b'smsgPing', b'smsgInv', b'smsgMsg'):
                peer.send_and_ping(msg_generic(command, b'\xff' * 32))
            assert_equal(node.getpeerinfo()[0]['banscore'], 0)
        finally:
            peer.peer_disconnect()
            peer.wait_for_disconnect()
        self.restart_node(0)
        assert_equal(node.getblockcount(), 0)
        assert_equal(int(node.getnetworkinfo()['localservices'], 16) & NODE_SMSG, 0)


if __name__ == '__main__':
    WalletlessMessagingTest().main()
