#!/usr/bin/env python3
# Copyright (c) 2026 The Verge Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Exercise mandatory hybrid staking and the complete native PQ coin lifecycle."""

from decimal import Decimal
from io import BytesIO
from pathlib import Path

from test_framework.authproxy import JSONRPCException
from test_framework.messages import deser_compact_size, deser_string
from test_framework.test_framework import VergeTestFramework
from test_framework.util import (
    assert_equal, assert_raises_rpc_error, connect_nodes_bi, disconnect_nodes,
    sync_blocks, sync_mempools, wait_until,
)


class PostQuantumTest(VergeTestFramework):
    UNBONDING_BLOCKS = 240
    def set_test_params(self):
        self.num_nodes = 3
        self.setup_clean_chain = True
        self.rpc_timewait = 300
        self.base_args = ["-posactivationheight=1500", "-posunbondingblocks={}".format(self.UNBONDING_BLOCKS), "-without-tor"]
        self.extra_args = [list(self.base_args) for _ in range(3)]

    def setup_network(self):
        self.setup_nodes()
        connect_nodes_bi(self.nodes, 0, 1)

    @staticmethod
    def mine(node, count, address):
        while count:
            batch = min(count, 100)
            node.generatetoaddress(batch, address)
            count -= batch

    def stake(self, slots=1):
        for _ in range(200):
            self.timestamp += slots * 30
            for node in self.nodes:
                node.setmocktime(self.timestamp)
            try:
                return self.nodes[0].generatestake()
            except JSONRPCException as error:
                assert "wallet has no eligible bond" in error.error["message"], error.error
        raise AssertionError("No local eligible slot in 200 attempts")

    @staticmethod
    def extension(node, block_hash):
        stream = BytesIO(bytes.fromhex(node.getblock(block_hash, 0)))
        stream.seek(112)
        transactions = node.getblock(block_hash, 2)["tx"]
        assert_equal(deser_compact_size(stream), len(transactions))
        for tx in transactions:
            raw = bytes.fromhex(tx["hex"])
            assert_equal(stream.read(len(raw)), raw)
        deser_string(stream)
        assert_equal(stream.read(1), b"\x02")
        assert_equal(len(stream.read(295)), 295)
        return stream.tell()

    @staticmethod
    def spend_output(node, txid, vout, destination):
        output = node.getrawtransaction(txid, True)["vout"][vout]
        raw = node.createrawtransaction([{"txid": txid, "vout": vout}],
                                        {destination: output["value"] - Decimal("1")})
        signed = node.signrawtransactionwithwallet(raw)
        assert_equal(signed["complete"], True)
        return signed["hex"]

    def exercise_multisig(self, mining, destination, post_activation):
        producer, owner, observer = self.nodes
        cosigners = [node.getnewpqaddress("multisig cosigner") for node in self.nodes]
        policy = producer.createpqmultisig(2, cosigners)
        assert_equal(producer.createpqmultisig(2, cosigners[::-1]), policy)
        for node in self.nodes:
            assert_equal(node.addpqmultisigaddress(2, cosigners, "custody"), policy)
            info = node.getaddressinfo(policy["address"])
            assert_equal(info["ismine"], False)
            assert_equal(info["iswatchonly"], True)
            assert_equal(info["ismultisig"], True)
            assert_equal(info["iswitness"], False)
            assert_equal(info["sigsrequired"], 2)
        assert_raises_rpc_error(-8, "Duplicate", producer.createpqmultisig, 2, cosigners[:1] * 2)
        assert_raises_rpc_error(-8, "1 <=", producer.createpqmultisig, 0, cosigners)
        assert_raises_rpc_error(-8, "1 <=", producer.createpqmultisig, 2, cosigners * 2)
        assert_raises_rpc_error(-5, "single-key", producer.createpqmultisig, 1, [policy["address"]])
        assert_raises_rpc_error(-5, "single-key", producer.createpqmultisig, 1, [mining])
        funding = producer.sendtoaddress(policy["address"], 100)
        if post_activation:
            self.stake()
        else:
            self.mine(producer, 1, mining)
        sync_blocks([producer, owner])
        output = next(v for v in producer.getrawtransaction(funding, True)["vout"]
                      if policy["address"] in v["scriptPubKey"].get("addresses", []))
        coin = next(c for c in owner.listunspent(1, 9999999, [policy["address"]]))
        assert_equal(coin["spendable"], False)
        assert_equal(coin["solvable"], True)
        raw = owner.createrawtransaction([{"txid": funding, "vout": output["n"]}], {destination: 95})
        funded = owner.fundrawtransaction(raw, {"includeWatching": True})
        assert funded["fee"] > Decimal("0.5"), funded
        raw = funded["hex"]
        first = producer.signrawtransactionwithwallet(raw)
        assert_equal(first["complete"], False)
        assert_raises_rpc_error(-26, "", producer.sendrawtransaction, first["hex"])
        duplicate = producer.combinerawtransaction([first["hex"], first["hex"]])
        assert_raises_rpc_error(-26, "", producer.sendrawtransaction, duplicate)
        second = owner.signrawtransactionwithwallet(raw)
        assert_equal(second["complete"], False)
        complete = owner.signrawtransactionwithwallet(first["hex"])
        assert_equal(complete["complete"], True)
        combined = producer.combinerawtransaction([first["hex"], second["hex"]])
        # A third, unsynchronized cosigner can sign from explicit prevout data.
        third = observer.signrawtransactionwithwallet(raw, [{"txid": funding, "vout": output["n"],
            "scriptPubKey": policy["scriptPubKey"], "amount": output["value"]}]) if not post_activation else second
        assert_equal(third["complete"], False)
        decoded = producer.decoderawtransaction(combined)
        assert_equal(decoded["version"], 0x05475658 if post_activation else 1)
        assert len(bytes.fromhex(decoded["vin"][0]["scriptSig"]["hex"])) > 7500
        txid = producer.sendrawtransaction(combined)
        if post_activation:
            self.stake()
        else:
            self.mine(producer, 1, mining)
        sync_blocks([producer, owner])
        assert producer.getrawtransaction(txid, True)["confirmations"] > 0
        assert_equal(owner.listunspent(1, 9999999, [policy["address"]]), [])
        if post_activation:
            self.multisig_policy = policy

    def run_test(self):
        producer, owner, observer = self.nodes
        for node in self.nodes:
            node.setstaking(False)
            info = node.getposinfo()
            assert_equal(info["authentication_scheme"], "schnorr-mldsa44-v2")
            assert_equal(info["pq_spending_active"], True)
        mining = producer.getnewaddress()
        recipient = owner.getnewpqaddress("PQ incoming")
        destination = producer.getnewpqaddress("PQ return")
        assert_equal(owner.getaddressinfo(recipient)["ispostquantum"], True)
        assert_equal(owner.getaddressinfo(recipient)["ismine"], True)
        damaged = recipient[:-1] + ("q" if recipient[-1] != "q" else "p")
        assert_raises_rpc_error(-5, "", producer.sendtoaddress, damaged, 1)

        self.log.info("Fund and spend native PQ coins before PoS activation")
        self.mine(producer, 731, mining)
        funded = producer.sendtoaddress(recipient, 10000)
        self.mine(producer, 1, mining)
        sync_blocks([producer, owner])
        output = next(v for v in owner.getrawtransaction(funded, True)["vout"]
                      if recipient in v["scriptPubKey"].get("addresses", []))
        signed = self.spend_output(owner, funded, output["n"], destination)
        decoded = owner.decoderawtransaction(signed)
        script_sig = bytes.fromhex(decoded["vin"][0]["scriptSig"]["hex"])
        assert_equal(len(script_sig), 3757)
        altered = bytearray.fromhex(signed)
        offset = altered.find(script_sig)
        assert offset > 0
        altered[offset + 3] ^= 1
        assert_raises_rpc_error(-26, "", owner.sendrawtransaction, altered.hex())
        returned = owner.sendrawtransaction(signed)
        sync_mempools([producer, owner])
        self.mine(producer, 1, mining)
        sync_blocks([producer, owner])
        assert producer.getrawtransaction(returned, True)["confirmations"] > 0

        self.log.info("Exercise native 2-of-3 PQ custody before activation")
        self.exercise_multisig(mining, destination, False)

        self.log.info("Create hybrid bonds and release a maturing bond with PQ authorization")
        bonds = [producer.createbond(2000) for _ in range(3)]
        self.mine(producer, 1, mining)
        for bond in bonds:
            info = producer.getbondinfo(bond["txid"], bond["vout"])
            assert_equal(info["credential_version"], 2)
            assert_equal(info["reward_address"], bond["reward_address"])
            assert_equal(info["withdrawal_address"], bond["withdrawal_address"])
            tx = producer.getrawtransaction(bond["txid"], True)
            script = bytes.fromhex(tx["vout"][bond["vout"]]["scriptPubKey"]["hex"])
            assert_equal(len(script), 138)
            assert_equal(script[2:6], b"VPB\x02")
        withdrawal = producer.unbond(bonds[0]["txid"], bonds[0]["vout"])
        self.mine(producer, 1, mining)
        unbond = producer.listunbondings()[0]
        assert_equal(unbond["status"], "unbonding")
        unsigned_too_early = self.spend_output(producer, withdrawal["txid"], unbond["vout"], destination)
        assert_raises_rpc_error(-26, "", producer.sendrawtransaction, unsigned_too_early)
        self.mine(producer, self.UNBONDING_BLOCKS, mining)
        assert_equal(producer.listunbondings()[0]["unlocked"], True)
        released = producer.sendrawtransaction(unsigned_too_early)
        self.mine(producer, 1, mining)
        assert producer.getrawtransaction(released, True)["confirmations"] > 0
        self.mine(producer, 1499 - producer.getblockcount(), mining)
        sync_blocks([producer, owner])
        self.timestamp = producer.getblockheader(producer.getbestblockhash())["time"] // 30 * 30

        self.log.info("Require both producer signatures and accept the undamaged same-ID candidate")
        disconnect_nodes(producer, 1)
        disconnect_nodes(owner, 0)
        block_hash = self.stake()
        raw = bytes.fromhex(producer.getblock(block_hash, 0))
        auth = self.extension(producer, block_hash)
        assert_equal(raw[auth], 2)
        for offset, length in ((auth + 209, 64), (auth + 273 + 1312, 2420)):
            invalid = bytearray(raw)
            invalid[offset:offset + length] = bytes(length)
            assert_equal(owner.submitblock(invalid.hex()), "bad-pos-signature")
            assert_raises_rpc_error(-5, "Block not found", owner.getblockheader, block_hash)
        assert_equal(owner.submitblock(raw.hex()), None)
        connect_nodes_bi(self.nodes, 0, 1)
        sync_blocks([producer, owner])

        self.log.info("Spend protected coins and withdraw an eligible bond after activation")
        self.exercise_multisig(mining, destination, True)
        transfer = producer.sendtoaddress(recipient, 100)
        self.stake()
        sync_blocks([producer, owner])
        assert_equal(owner.getrawtransaction(transfer, True)["version"], 0x05475658)
        output = next(v for v in owner.getrawtransaction(transfer, True)["vout"]
                      if recipient in v["scriptPubKey"].get("addresses", []))
        post = self.spend_output(owner, transfer, output["n"], destination)
        returned = owner.sendrawtransaction(post)
        def pools_synced():
            if set(producer.getrawmempool()) == set(owner.getrawmempool()):
                return True
            # Keep simulated chain time progressing during the PoS relay wait.
            self.timestamp += 30
            for node in self.nodes:
                node.setmocktime(self.timestamp)
            return False
        wait_until(pools_synced, timeout=60)
        self.stake()
        sync_blocks([producer, owner])
        assert producer.getrawtransaction(returned, True)["confirmations"] > 0
        eligible = next(b for b in producer.listbonds() if b["eligible"])
        withdrawal = producer.unbond(eligible["txid"], eligible["vout"])
        self.stake()
        unbond = next(u for u in producer.listunbondings() if u["txid"] == withdrawal["txid"])
        locked = self.spend_output(producer, withdrawal["txid"], unbond["vout"], destination)
        assert_raises_rpc_error(-26, "", producer.sendrawtransaction, locked)
        for _ in range(self.UNBONDING_BLOCKS):
            self.stake()
        unbond = next(u for u in producer.listunbondings() if u["txid"] == withdrawal["txid"])
        assert_equal(unbond["unlocked"], True)
        released = producer.sendrawtransaction(locked)
        self.stake()
        assert producer.getrawtransaction(released, True)["confirmations"] > 0

        self.log.info("Advance hybrid votes/finality and synchronize a fresh observer")
        for _ in range(5):
            self.stake(slots=120)
            sync_blocks([producer, owner])
        assert producer.getposinfo()["finalized"]["height"] >= 1500
        connect_nodes_bi(self.nodes, 0, 2)
        sync_blocks(self.nodes)
        expected = producer.getbestblockhash()
        expected_utxo = producer.gettxoutsetinfo()["hash_serialized_2"]
        self.restart_node(2, self.base_args + ["-reindex-chainstate", "-mocktime={}".format(self.timestamp)])
        connect_nodes_bi(self.nodes, 0, 2)
        sync_blocks(self.nodes)
        assert_equal(observer.getbestblockhash(), expected)
        assert_equal(observer.gettxoutsetinfo()["hash_serialized_2"], expected_utxo)
        assert_equal(observer.getposinfo()["eligible_snapshot"]["root"],
                     producer.getposinfo()["eligible_snapshot"]["root"])

        self.log.info("Encrypted owner keys survive restart and complete-wallet backup")
        owner.encryptwallet("isolated PQ test password")
        owner.wait_until_stopped()
        self.start_node(1, self.base_args + ["-mocktime={}".format(self.timestamp)])
        assert_raises_rpc_error(-13, "", owner.getnewpqaddress)
        owner.walletpassphrase("isolated PQ test password", 120)
        assert_equal(owner.getaddressinfo(recipient)["ismine"], True)
        assert_equal(owner.getaddressinfo(self.multisig_policy["address"])["policy"], self.multisig_policy["policy"])
        # Restore on another process: loading a BDB copy beside its original
        # in one process is prohibited by the duplicate-file-ID guard.
        restored_dir = Path(observer.datadir) / "regtest" / "wallets" / "pq-restored"
        restored_dir.mkdir()
        backup = restored_dir / "wallet.dat"
        owner.backupwallet(str(backup))
        assert backup.is_file() and backup.stat().st_size > 0
        restored = observer.loadwallet(restored_dir.name)
        restored_wallet = observer.get_wallet_rpc(restored["name"])
        restored_wallet.walletpassphrase("isolated PQ test password", 120)
        assert_equal(restored_wallet.getaddressinfo(recipient)["ismine"], True)
        assert_equal(restored_wallet.getaddressinfo(self.multisig_policy["address"])["policy"], self.multisig_policy["policy"])
        restored_wallet.getnewpqaddress("restored PQ key")
        connect_nodes_bi(self.nodes, 0, 1)
        sync_blocks(self.nodes)
        for node in self.nodes:
            assert not any(peer.get("banscore", 0) for peer in node.getpeerinfo())
        assert_equal(producer.getbestblockhash(), expected)


if __name__ == "__main__":
    PostQuantumTest().main()
