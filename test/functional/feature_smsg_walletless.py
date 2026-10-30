#!/usr/bin/env python3
# Copyright (c) 2026 Verge
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Walletless nodes do not advertise or dispatch secure messaging."""

import struct
import time

from test_framework.messages import NODE_NETWORK, msg_generic, ser_string
from test_framework.mininode import MESSAGEMAP, P2PInterface, network_thread_join, network_thread_start
from test_framework.test_framework import SkipTest, VergeTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error, p2p_port


NODE_SMSG = 1 << 24


class WalletlessVersion:
    command = b'version'

    def __init__(self):
        self.nVersion = 90009
        self.nServices = NODE_NETWORK | NODE_SMSG

    def serialize(self):
        # Verge version messages include timestamps and 41-byte addresses.
        address = struct.pack('<IQ', 0, NODE_NETWORK) + bytes(41) + struct.pack('>H', 0)
        return (struct.pack('<iQq', self.nVersion, self.nServices, int(time.time())) +
                address + address + struct.pack('<Q', 1) +
                ser_string(b'/verge-walletless-test/') + struct.pack('<iB', 0, 1))

    def deserialize(self, payload):
        self.nVersion, self.nServices = struct.unpack('<iQ', payload.read(12))
        payload.read()


class WalletlessPeer(P2PInterface):
    def peer_connect(self, *args, services=NODE_NETWORK | NODE_SMSG, **kwargs):
        super().peer_connect(*args, services=services, send_version=False, **kwargs)
        version = WalletlessVersion()
        version.nServices = services
        self.send_message(version, True)


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
        assert_equal(node.getblockchaininfo()['chain'], 'regtest')
        assert_equal(int(node.getnetworkinfo()['localservices'], 16) & NODE_SMSG, 0)
        assert_raises_rpc_error(-32601, 'Method not found', node.getwalletinfo)
        assert_raises_rpc_error(-32601, 'Method not found', node.smsgenable)
        original_version = MESSAGEMAP[b'version']
        MESSAGEMAP[b'version'] = WalletlessVersion
        peer = WalletlessPeer()
        peer.peer_connect('127.0.0.1', p2p_port(0))
        network_thread_start()
        try:
            peer.wait_for_verack()
            assert_equal(peer.nServices & NODE_SMSG, 0)
            for command in (b'smsgPing', b'smsgInv', b'smsgMsg'):
                peer.send_and_ping(msg_generic(command, b'\xff' * 32))
            assert_equal(node.getpeerinfo()[0]['banscore'], 0)
        finally:
            peer.peer_disconnect()
            network_thread_join()
            MESSAGEMAP[b'version'] = original_version
        self.restart_node(0)
        assert_equal(node.getblockcount(), 0)
        assert_equal(int(node.getnetworkinfo()['localservices'], 16) & NODE_SMSG, 0)


if __name__ == '__main__':
    WalletlessMessagingTest().main()
