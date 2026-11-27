#!/usr/bin/env python3
# Copyright (c) 2026 The Verge Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Check PoS RPC credential counts against active PQ bond destinations."""

from pathlib import Path

from test_framework.test_framework import VergeTestFramework
from test_framework.util import assert_equal


class PoSInfoTest(VergeTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [["-without-tor", "-posactivationheight=1500"]]

    def check_counts(self, bonds, rewards, withdrawals):
        info = self.nodes[0].getposinfo()
        assert_equal(info["tracked_bonds"], bonds)
        assert_equal(info["unique_reward_keys"], rewards)
        assert_equal(info["unique_withdrawal_keys"], withdrawals)

    def stake(self):
        self.timestamp += 30
        self.nodes[0].setmocktime(self.timestamp)
        return self.nodes[0].generatestake()

    def run_test(self):
        node = self.nodes[0]
        node.setstaking(False)
        self.check_counts(0, 0, 0)
        trusted = node.getposinfo()["trusted_checkpoint"]
        assert_equal(trusted["configured"], False)
        assert_equal(trusted["refresh_recommended"], False)
        assert_equal(trusted["maximum_age_blocks"], 20160)
        mining = node.getnewaddress("mining", "legacy")
        for count in [100] * 7 + [31]:
            node.generatetoaddress(count, mining)

        self.log.info("Count full PQ IDs and deduplicate each owner credential separately")
        node.backupwallet(str(Path(node.datadir) / "pq-internal-backup.dat"))
        bonds = [node.createbond(2000) for _ in range(2)]
        bonds.append(node.createbond(
            2000, bonds[1]["reward_address"], bonds[0]["withdrawal_address"]))
        node.generatetoaddress(1, mining)
        self.check_counts(3, 2, 2)
        for bond in bonds:
            info = node.getbondinfo(bond["txid"], bond["vout"])
            assert_equal(info["credential_version"], 2)
            assert_equal(info["reward_address"], bond["reward_address"])
            assert_equal(info["withdrawal_address"], bond["withdrawal_address"])
        self.restart_node(0)
        self.check_counts(3, 2, 2)

        self.log.info("The same credential counts apply after PoS activation")
        while node.getblockcount() < 1499:
            node.generatetoaddress(min(100, 1499 - node.getblockcount()), mining)
        self.timestamp = node.getblockheader(node.getbestblockhash())["time"] // 30 * 30
        self.stake()
        assert_equal(node.getposinfo()["active"], True)
        self.check_counts(3, 2, 2)

        self.log.info("Unbonded credentials stop contributing to active-bond counts")
        checkpoint = node.getbestblockhash()
        self.restart_node(0, self.extra_args[0] + [
            "-mocktime={}".format(self.timestamp),
            "-postrustedcheckpoint=1500:" + checkpoint])
        trusted = node.getposinfo()["trusted_checkpoint"]
        assert_equal(trusted["configured"], True)
        assert_equal(trusted["age_blocks"], 0)
        assert_equal(trusted["refresh_recommended"], False)
        node.unbond(bonds[0]["txid"], bonds[0]["vout"])
        self.stake()
        self.check_counts(2, 1, 2)
        node.unbond(bonds[1]["txid"], bonds[1]["vout"])
        self.stake()
        self.check_counts(1, 1, 1)
        node.unbond(bonds[2]["txid"], bonds[2]["vout"])
        tip = self.stake()
        self.check_counts(0, 0, 0)

        self.log.info("Reorganizations and restart reproduce the same counters")
        node.invalidateblock(tip)
        self.check_counts(1, 1, 1)
        node.reconsiderblock(tip)
        self.check_counts(0, 0, 0)
        self.restart_node(0, self.extra_args[0] + ["-mocktime={}".format(self.timestamp)])
        self.check_counts(0, 0, 0)


if __name__ == "__main__":
    PoSInfoTest().main()
