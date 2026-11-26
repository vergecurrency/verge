#!/usr/bin/env python3
# Copyright (c) 2026 The Verge Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""PQ SMSG: authenticated delivery, funding, encrypted persistence and PoS."""

from pathlib import Path
import time

from test_framework.authproxy import JSONRPCException
from test_framework.messages import NODE_NETWORK, msg_generic
from test_framework.mininode import MESSAGEMAP, P2PInterface, mininode_lock
from test_framework.test_framework import VergeTestFramework
from test_framework.util import (
    assert_equal, assert_raises_rpc_error, connect_nodes_bi, sync_blocks,
    sync_mempools, wait_until,
)

NODE_SMSG = 1 << 24
NODE_SMSG_PQ = 1 << 26


class SmsgPing:
    command = b'smsgPing'

    def serialize(self):
        return b''

    def deserialize(self, stream):
        pass

    def __repr__(self):
        return self.command.decode()


class SmsgPong(SmsgPing):
    command = b'smsgPong'


class MessagingPeer(P2PInterface):
    def on_smsgPing(self, message):
        pass

    def on_smsgPong(self, message):
        pass

    def ping_count(self):
        with mininode_lock:
            return self.message_count['smsgPing']


MESSAGEMAP[SmsgPing.command] = SmsgPing
MESSAGEMAP[SmsgPong.command] = SmsgPong


class PQMessagingTest(VergeTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 3
        self.rpc_timewait = 300
        self.base_args = ['-without-tor', '-dnsseed=0', '-smsg=1', '-smsgpeers=0',
                          '-debug=smsg', '-posactivationheight=1500']
        self.extra_args = [list(self.base_args) for _ in range(self.num_nodes)]

    def setup_network(self):
        self.setup_nodes()
        self.connect_relay()

    def connect_relay(self):
        connect_nodes_bi(self.nodes, 0, 2)
        connect_nodes_bi(self.nodes, 1, 2)

    def mine(self, count):
        while count:
            batch = min(count, 100)
            self.nodes[0].generatetoaddress(batch, self.mining)
            count -= batch
        sync_blocks(self.nodes)

    @staticmethod
    def messages(node):
        return node.smsginbox('all').get('messages', [])

    def wait_for(self, predicate, timeout=120):
        next_tick = 0

        def check():
            nonlocal next_tick
            if time.monotonic() >= next_tick:
                self.timestamp += 5
                for node in self.nodes:
                    if node.running:
                        node.setmocktime(self.timestamp)
                next_tick = time.monotonic() + 1
            return predicate()

        # Relay scheduling uses node time, so it must advance with mocktime enabled.
        wait_until(check, timeout=timeout)

    def received(self, node, message):
        self.wait_for(lambda: any(m.get('msgid') == message['msgid'] for m in self.messages(node)))
        item = next(m for m in self.messages(node) if m.get('msgid') == message['msgid'])
        assert_equal(item['post_quantum'], True)
        return item

    def stake(self):
        for _ in range(200):
            self.timestamp += 30
            for node in self.nodes:
                node.setmocktime(self.timestamp)
            try:
                block = self.nodes[0].generatestake()
                sync_blocks(self.nodes)
                return block
            except JSONRPCException as error:
                assert 'wallet has no eligible bond' in error.error['message'], error.error
        raise AssertionError('No eligible PoS slot in 200 attempts')

    def run_test(self):
        alice, bob, relay = self.nodes
        self.timestamp = int(time.time())
        for node in self.nodes:
            node.setmocktime(self.timestamp)
            node.setstaking(False)
            assert int(node.getnetworkinfo()['localservices'], 16) & NODE_SMSG_PQ
        self.mining = alice.getnewaddress('mining')
        self.mine(731)
        pq_funding = bob.getnewpqaddress('message funding')
        funded = alice.sendtoaddress(pq_funding, 100)
        self.mine(1)
        assert bob.gettransaction(funded)['confirmations'] > 0
        bond = alice.createbond(2000)
        self.mine(1)
        assert_equal(alice.getbondinfo(bond['txid'], bond['vout'])['credential_version'], 2)

        self.log.info('Create independent PQ messaging identities and pin full contact bundles')
        a = alice.smsgnewaddress('Alice messaging')
        b = bob.smsgnewaddress('Bob messaging')
        for node, identity in ((alice, a), (bob, b)):
            assert identity['publickey'].startswith('pq1')
            assert_equal(len(identity['fingerprint']), 64)
            for _ in range(2):
                public = node.smsggetpubkey(identity['address'])
                assert_equal(public['chatkey'], identity['chatkey'])
                assert_equal(public['fingerprint'], identity['fingerprint'])
        assert 'added' in alice.smsgaddaddress(b['address'], b['publickey'])['result']
        assert 'added' in bob.smsgaddaddress(a['address'], a['publickey'])['result']
        damaged = b['publickey'][:80] + ('A' if b['publickey'][80] != 'A' else 'B') + b['publickey'][81:]
        assert 'not added' in alice.smsgaddaddress(b['address'], damaged)['result']

        self.log.info('Fee estimates and absent PQ contacts never broadcast a fallback message')
        before = set(alice.getrawmempool())
        estimate = alice.smsgsend(a['address'], b['chatkey'], 'fee estimate', True, 31, True)
        assert_equal(estimate['result'], 'Not Sent.')
        assert_equal(estimate['post_quantum'], True)
        assert_equal(set(alice.getrawmempool()), before)
        missing = alice.smsgsend(a['address'], bob.getnewaddress(), 'no fallback')
        assert_equal(missing['result'], 'Send failed.')
        assert 'PQ recipient chatkey' in missing['error']
        assert_equal(set(alice.getrawmempool()), before)

        self.log.info('Confirmed funding is required, and the relay cannot decrypt messages')
        message = alice.smsgsend(a['address'], b['address'], 'PQ before activation')
        assert_equal(message['result'], 'Sent.')
        assert_equal(message['post_quantum'], True)
        assert not any(m.get('msgid') == message['msgid'] for m in self.messages(bob))
        tx = alice.getrawtransaction(message['txid'], True)
        marker = next(v for v in tx['vout'] if v['scriptPubKey']['hex'].startswith('6a'))
        assert '534d534734' in marker['scriptPubKey']['hex']
        self.mine(1)
        item = self.received(bob, message)
        assert_equal(item['text'], 'PQ before activation')
        assert_equal(item['sender_fingerprint'], a['fingerprint'])
        assert_equal(self.messages(relay), [])
        assert any(m.get('msgid') == message['msgid'] and m.get('post_quantum')
                   for m in alice.smsgoutbox('all')['messages'])

        self.log.info('Maximum-size identified messages and anonymous messages retain PQ encryption')
        maximum = alice.smsgsend(a['address'], b['address'], 'x' * 5120)
        anonymous = alice.smsgsendanon(b['chatkey'], 'anonymous PQ content')
        assert_equal(anonymous['post_quantum'], True)
        self.mine(1)
        assert_equal(self.received(bob, maximum)['text'], 'x' * 5120)
        assert_equal(self.received(bob, anonymous)['from'], 'anon')
        rejected = alice.smsgsend(a['address'], b['address'], 'x' * 5121)
        assert_equal(rejected['result'], 'Send failed.')

        self.log.info('Locked recipients retain ciphertext and decrypt after restart and unlock')
        bob.encryptwallet('pq-messaging-passphrase')
        bob.wait_until_stopped()
        self.start_node(1, extra_args=self.base_args)
        bob.setmocktime(self.timestamp)
        connect_nodes_bi(self.nodes, 1, 2)
        assert_equal(bob.smsggetpubkey(b['address'])['fingerprint'], b['fingerprint'])
        assert_raises_rpc_error(-13, 'Unlock', bob.smsgnewaddress)
        locked_message = alice.smsgsend(a['address'], b['address'], 'received while locked')
        self.mine(1)
        self.wait_for(lambda: bob.smsgbuckets()['total']['messages'] > 0)
        bob.walletpassphrase('pq-messaging-passphrase', 300)
        assert_equal(self.received(bob, locked_message)['text'], 'received while locked')
        backup = Path(self.options.tmpdir) / 'pq-messaging-wallet-backup.dat'
        bob.backupwallet(str(backup))
        assert backup.exists()
        assert_equal(bob.smsggetpubkey(a['address'])['fingerprint'], a['fingerprint'])

        self.log.info('Offline recipients recover messages from a relay')
        self.stop_node(1)
        offline = alice.smsgsend(a['address'], b['address'], 'offline PQ delivery')
        alice.generatetoaddress(1, self.mining)
        sync_blocks([alice, relay])
        time.sleep(5)
        self.start_node(1, extra_args=self.base_args)
        bob.setmocktime(self.timestamp)
        connect_nodes_bi(self.nodes, 1, 2)
        sync_blocks(self.nodes)
        bob.walletpassphrase('pq-messaging-passphrase', 300)
        assert_equal(self.received(bob, offline)['text'], 'offline PQ delivery')

        self.log.info('Post-fork PQ spending still funds SMSG and confirmed PoS blocks authorize relay')
        self.mine(1499 - alice.getblockcount())
        self.timestamp = max(self.timestamp, alice.getblockheader(alice.getbestblockhash())['time']) // 30 * 30
        self.stake()
        bob.walletpassphrase('pq-messaging-passphrase', 300)
        post_fork = bob.smsgsend(b['address'], a['address'], 'PQ after activation')
        assert_equal(post_fork['result'], 'Sent.')
        tx = bob.getrawtransaction(post_fork['txid'], True)
        assert_equal(tx['version'], 0x05475658)
        assert any(len(bytes.fromhex(v['scriptSig']['hex'])) >= 3757 for v in tx['vin'])
        assert not any(m.get('msgid') == post_fork['msgid'] for m in self.messages(alice))
        sync_mempools(self.nodes)
        assert post_fork['txid'] in alice.getrawmempool()
        self.stake()
        assert bob.getrawtransaction(post_fork['txid'], True)['confirmations'] > 0
        assert_equal(self.received(alice, post_fork)['text'], 'PQ after activation')
        self.log.info('Legacy-only peers remain connected without receiving unsupported PQ SMSG')
        peer = relay.add_p2p_connection(P2PInterface(), services=NODE_NETWORK | NODE_SMSG)
        try:
            peer.wait_for_verack()
            peer.send_and_ping(msg_generic(b'smsgPing', b''))
            assert_equal(relay.getpeerinfo()[-1]['banscore'], 0)
        finally:
            peer.peer_disconnect()
            peer.wait_for_disconnect()

        self.log.info('Unanswered PQ handshakes retry, but an explicit disable stops retries')
        peer = relay.add_p2p_connection(MessagingPeer(), services=NODE_NETWORK | NODE_SMSG_PQ)
        try:
            peer.wait_for_verack()
            self.wait_for(lambda: peer.ping_count() > 0)
            before = peer.ping_count()
            self.timestamp += 31
            self.wait_for(lambda: peer.ping_count() > before)
            peer.send_and_ping(msg_generic(b'smsgDisabled', b''))
            before = peer.ping_count()
            self.timestamp += 31
            for node in self.nodes:
                node.setmocktime(self.timestamp)
            peer.sync_with_ping()
            time.sleep(1)
            assert_equal(peer.ping_count(), before)
        finally:
            peer.peer_disconnect()
            peer.wait_for_disconnect()


if __name__ == '__main__':
    PQMessagingTest().main()
