#!/usr/bin/env python3
# Copyright (c) 2026 Verge
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Exercise bonding, PoS activation, production, restart, and reconnect."""

from io import BytesIO

from test_framework.messages import deser_compact_size, deser_string, msg_posvote
from test_framework.test_framework import VergeTestFramework
from test_framework.mininode import MESSAGEMAP, P2PInterface
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
    connect_nodes_bi,
    disconnect_nodes,
    wait_until,
)


class MalformedPoSMessage:
    def __init__(self, command, size):
        self.command = command
        self.size = size

    def serialize(self):
        return b"\x00" * self.size


class IgnoredBlockMessage:
    command = b"block"

    def deserialize(self, payload):
        payload.read()


class KnownPoSHeader:
    command = b"headers"

    def __init__(self, header):
        self.header = bytes.fromhex(header)

    def serialize(self):
        return b"\x01" + self.header + b"\x00\x00"


class PoSActivationTest(VergeTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.rpc_timewait = 300
        self.extra_args = [["-posactivationheight=1500", "-without-tor"],
                           ["-posactivationheight=1500", "-without-tor"]]

    @staticmethod
    def next_slot(timestamp, slots=1):
        return ((timestamp // 30) + slots) * 30

    @staticmethod
    def mine_blocks(node, count, address):
        while count:
            batch = min(count, 100)
            node.generatetoaddress(batch, address)
            count -= batch

    @staticmethod
    def first_vote(node, block_hash):
        stream = BytesIO(bytes.fromhex(node.getblock(block_hash, 0)))
        stream.seek(112)  # PoS header, including its signed-extension commitment.
        transactions = node.getblock(block_hash, 2)["tx"]
        assert_equal(deser_compact_size(stream), len(transactions))
        for tx in transactions:
            raw_tx = bytes.fromhex(tx["hex"])
            assert_equal(stream.read(len(raw_tx)), raw_tx)
        deser_string(stream)  # Legacy block-signature vector.
        assert_equal(stream.read(1), b"\x01")
        assert_equal(len(stream.read(295 + 273)), 295 + 273)
        assert deser_compact_size(stream) > 0
        vote = msg_posvote()
        vote.deserialize(stream)
        return vote

    def run_test(self):
        node = self.nodes[0]
        observer = self.nodes[1]
        mining_address = node.getnewaddress()

        self.mine_blocks(node, 721, mining_address)
        bond = node.createbond(1000)
        assert_equal(bond["amount"], 1000)
        self.mine_blocks(node, 1, mining_address)

        # Hold a valid legacy transaction across activation. It must not be
        # accepted by the PoS chain even though its inputs remain unspent.
        legacy_input = node.listunspent(1)[0]
        legacy_raw = node.createrawtransaction(
            [{"txid": legacy_input["txid"], "vout": legacy_input["vout"]}],
            {node.getnewaddress(): legacy_input["amount"] - 1})
        legacy_signed = node.signrawtransactionwithwallet(legacy_raw)
        assert_equal(legacy_signed["complete"], True)
        assert_equal(node.decoderawtransaction(legacy_signed["hex"])["version"], 1)

        self.mine_blocks(node, 777, mining_address)
        assert_equal(node.getblockcount(), 1499)
        staking_info = node.getstakinginfo()
        self.log.info("Pre-activation staking state: %s", staking_info)
        assert_equal(staking_info["tracked_bonds"], 1)
        assert_equal(staking_info["initial_snapshot_bonds"], 1)
        pos_info = node.getposinfo()
        assert_equal(pos_info["active"], False)
        assert_equal(pos_info["next_block_proof"], "pos")
        assert_equal(pos_info["tracked_bonds"], 1)
        assert_equal(pos_info["eligible_snapshot"]["bonds"], 1)
        assert_equal(pos_info["network"]["connections"], len(node.getpeerinfo()))
        bond_info = node.getbondinfo(bond["txid"], bond["vout"])
        assert_equal(bond_info["active"], True)
        assert_equal(bond_info["amount"], 1000)
        assert_equal(bond_info["eligible"], True)
        wallet_bonds = node.listbonds()
        assert_equal(len(wallet_bonds), 1)
        assert_equal(wallet_bonds[0]["txid"], bond["txid"])
        assert_equal(node.listunbondings(), [])

        final_pow_time = node.getblockheader(node.getbestblockhash())["time"]
        epoch_zero_time = self.next_slot(final_pow_time)
        for peer in self.nodes:
            peer.setmocktime(epoch_zero_time)
        epoch_zero = node.generatestake()
        assert_equal(node.getblockcount(), 1500)
        pos_block = node.getblock(epoch_zero, 2)
        assert all(tx["version"] == 0x05475658 for tx in pos_block["tx"])
        assert_raises_rpc_error(
            -26, "legacy-tx-after-pos-activation",
            node.sendrawtransaction, legacy_signed["hex"])
        pos_raw = node.createrawtransaction([], {node.getnewaddress(): 1})
        assert_equal(node.decoderawtransaction(pos_raw)["version"], 0x05475658)
        self.sync_all()
        pos_info = observer.getposinfo()
        assert_equal(pos_info["active"], True)
        assert_equal(pos_info["current_epoch"], 0)
        assert_equal(pos_info["latest_checkpoint"]["height"], 1500)
        assert "peers_at_tip" in pos_info["network"]

        MESSAGEMAP[b"block"] = IgnoredBlockMessage
        disconnect_nodes(node, 1)
        disconnect_nodes(observer, 0)
        vote_peer = observer.add_p2p_connection(P2PInterface())
        relay_peer = observer.add_p2p_connection(P2PInterface())
        for peer in self.nodes:
            peer.setmocktime(epoch_zero_time + 120 * 30)
        epoch_one = node.generatestake()
        # Keep the observer at epoch zero while testing standalone relay;
        # otherwise block inclusion can prune the vote before GETDATA arrives.
        vote = self.first_vote(node, epoch_one)
        vote_peer.send_message(vote)
        vote_peer.sync_with_ping()
        wait_until(lambda: "posvote" in relay_peer.last_message)
        assert_equal(relay_peer.last_message["posvote"].serialize(), vote.serialize())
        assert_equal(len(vote.serialize()), 229)
        known = KnownPoSHeader(observer.getblockheader(epoch_zero, False))
        before = vote_peer.message_count["getheaders"]
        vote_peer.send_message(known)
        vote_peer.sync_with_ping()
        wait_until(lambda: vote_peer.message_count["getheaders"] == before + 1)
        for _ in range(10):
            vote_peer.send_message(known)
        vote_peer.sync_with_ping()
        assert_equal(vote_peer.message_count["getheaders"], before + 1)
        # A valid relay burst may exhaust CPU credits, but is not evidence
        # that the peer is malicious. Malformed messages are still penalized.
        for _ in range(400):
            vote_peer.send_message(vote)
        vote_peer.sync_with_ping()
        assert_equal(max(peer.get("banscore", 0)
                         for peer in observer.getpeerinfo()), 0)
        assert_equal(node.getblockcount(), 1501)

        vote_peer.send_message(MalformedPoSMessage(b"posvote", 228))
        vote_peer.send_message(MalformedPoSMessage(b"posvevid", 458))
        vote_peer.sync_with_ping()
        wait_until(lambda: any(peer.get("banscore", 0) >= 40
                               for peer in observer.getpeerinfo()))
        observer.disconnect_p2ps()
        connect_nodes_bi(self.nodes, 0, 1)
        self.sync_all()
        disconnect_nodes(node, 1)
        disconnect_nodes(observer, 0)

        for peer in self.nodes:
            peer.setmocktime(epoch_zero_time + 240 * 30)
        epoch_two = node.generatestake()
        assert_equal(node.getblockcount(), 1502)
        assert_equal(node.getbestblockhash(), epoch_two)
        assert observer.getbestblockhash() != epoch_two

        for peer in self.nodes:
            peer.setmocktime(epoch_zero_time + 360 * 30)
        epoch_three = node.generatestake()
        for peer in self.nodes:
            peer.setmocktime(epoch_zero_time + 480 * 30)
        epoch_four = node.generatestake()
        assert_equal(node.getblockcount(), 1504)
        assert_equal(node.getbestblockhash(), epoch_four)

        # PoS peers intentionally serve one header and its fully validated
        # block at a time. Reconnecting several blocks behind must continue
        # requesting headers until the peer's tip is reached.
        observer.setmocktime(epoch_zero_time + 480 * 30)
        connect_nodes_bi(self.nodes, 0, 1)
        self.sync_all()
        assert_equal(observer.getbestblockhash(), epoch_four)

        # Keep liquid funds out of automatic bonding while checking persisted
        # staking settings; transaction trickle timers use this test's mock time.
        assert_equal(node.setstaking(True, node.getbalance())["enabled"], True)
        staking_info = node.getstakinginfo()
        assert_equal(staking_info["enabled"], True)
        assert_equal(staking_info["active"], True)

        self.restart_node(0, [
            "-posactivationheight=1500",
            "-without-tor",
            "-mocktime={}".format(epoch_zero_time + 480 * 30),
        ])
        node = self.nodes[0]
        assert_equal(node.getbestblockhash(), epoch_four)
        assert_equal(node.getstakinginfo()["enabled"], True)

        self.restart_node(1, [
            "-posactivationheight=1500",
            "-without-tor",
            "-mocktime={}".format(epoch_zero_time + 480 * 30),
            "-reindex-chainstate",
        ])
        wait_until(lambda: self.nodes[1].getbestblockhash() == epoch_four)
        connect_nodes_bi(self.nodes, 0, 1)
        self.sync_all()
        assert_equal(self.nodes[1].getbestblockhash(), epoch_four)

        node.invalidateblock(epoch_four)
        assert_equal(node.getbestblockhash(), epoch_three)
        node.reconsiderblock(epoch_four)
        assert_equal(node.getbestblockhash(), epoch_four)
        assert_equal(node.getblockheader(epoch_zero)["height"], 1500)

        node.setstaking(False)
        unbond = node.unbond(bond["txid"], bond["vout"])
        assert_equal(unbond["amount"], 1000)
        assert_equal(unbond["unlock_height"],
                     node.getblockcount() + 1 +
                     node.getposinfo()["unbonding_blocks"])
        unbondings = node.listunbondings()
        assert_equal(len(unbondings), 1)
        assert_equal(unbondings[0]["txid"], unbond["txid"])
        assert_equal(unbondings[0]["unlock_height"],
                     unbond["unlock_height"])


if __name__ == "__main__":
    PoSActivationTest().main()
