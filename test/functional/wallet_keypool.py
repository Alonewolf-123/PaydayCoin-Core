#!/usr/bin/env python3
# Copyright (c) 2014-2019 The PaydayCoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the wallet keypool and interaction with wallet encryption/locking."""

import time

from test_framework.test_framework import PaydayCoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error
from test_framework.auxpow import reverseHex
from test_framework.auxpow_testing import computeAuxpow

class KeyPoolTest(PaydayCoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        nodes = self.nodes
        addr_before_encrypting = nodes[0].getnewaddress()
        addr_before_encrypting_data = nodes[0].getaddressinfo(addr_before_encrypting)
        wallet_info_old = nodes[0].getwalletinfo()
        assert addr_before_encrypting_data['hdseedid'] == wallet_info_old['hdseedid']

        # Encrypt wallet and wait to terminate
        nodes[0].encryptwallet('test')
        # Keep creating keys
        addr = nodes[0].getnewaddress()
        addr_data = nodes[0].getaddressinfo(addr)
        wallet_info = nodes[0].getwalletinfo()
        assert addr_before_encrypting_data['hdseedid'] != wallet_info['hdseedid']
        assert addr_data['hdseedid'] == wallet_info['hdseedid']
        assert_raises_rpc_error(-12, "Error: Keypool ran out, please call keypoolrefill first", nodes[0].getnewaddress)
        # test draining with getauxblock
        test_auxpow(nodes)
        # put six (plus 2) new keys in the keypool (100% external-, +100% internal-keys, 1 in min)
        nodes[0].walletpassphrase('test', 12000)
        nodes[0].keypoolrefill(6)
        nodes[0].walletlock()
        wi = nodes[0].getwalletinfo()
        assert_equal(wi['keypoolsize_hd_internal'], 6)
        assert_equal(wi['keypoolsize'], 6)

    def test_auxpow(nodes):
        """
        Test behaviour of getauxpow.  Calling getauxpow should reserve
        a key from the pool, but it should be released again if the
        created block is not actually used.  On the other hand, if the
        auxpow is submitted and turned into a block, the keypool should
        be drained.
        """

        nodes[0].walletpassphrase('test', 12000)
        nodes[0].keypoolrefill(1)
        nodes[0].walletlock()
        assert_equal (nodes[0].getwalletinfo()['keypoolsize'], 1)

        nodes[0].getauxblock()
        assert_equal (nodes[0].getwalletinfo()['keypoolsize'], 1)
        nodes[0].generate(1)
        assert_equal (nodes[0].getwalletinfo()['keypoolsize'], 1)
        auxblock = nodes[0].getauxblock()
        assert_equal (nodes[0].getwalletinfo()['keypoolsize'], 1)

        target = reverseHex(auxblock['_target'])
        solved = computeAuxpow(auxblock['hash'], target, True)
        res = nodes[0].getauxblock(auxblock['hash'], solved)
        assert res
        assert_equal(nodes[0].getwalletinfo()['keypoolsize'], 0)

        assert_raises_rpc_error(-12, 'Keypool ran out', nodes[0].getauxblock)

        # drain the internal keys
        nodes[0].getrawchangeaddress()
        nodes[0].getrawchangeaddress()
        nodes[0].getrawchangeaddress()
        nodes[0].getrawchangeaddress()
        nodes[0].getrawchangeaddress()
        nodes[0].getrawchangeaddress()
        addr = set()
        # the next one should fail
        assert_raises_rpc_error(-12, "Keypool ran out", nodes[0].getrawchangeaddress)

        # drain the external keys
        addr.add(nodes[0].getnewaddress())
        addr.add(nodes[0].getnewaddress())
        addr.add(nodes[0].getnewaddress())
        addr.add(nodes[0].getnewaddress())
        addr.add(nodes[0].getnewaddress())
        addr.add(nodes[0].getnewaddress())
        assert len(addr) == 6
        # the next one should fail
        assert_raises_rpc_error(-12, "Error: Keypool ran out, please call keypoolrefill first", nodes[0].getnewaddress)

        # refill keypool with three new addresses
        nodes[0].walletpassphrase('test', 1)
        nodes[0].keypoolrefill(3)

        # test walletpassphrase timeout
        time.sleep(1.1)
        assert_equal(nodes[0].getwalletinfo()["unlocked_until"], 0)

        # drain the keypool
        for _ in range(3):
            nodes[0].getnewaddress()
        assert_raises_rpc_error(-12, "Keypool ran out", nodes[0].getnewaddress)

        nodes[0].walletpassphrase('test', 100)
        nodes[0].keypoolrefill(100)
        wi = nodes[0].getwalletinfo()
        assert_equal(wi['keypoolsize_hd_internal'], 100)
        assert_equal(wi['keypoolsize'], 100)

if __name__ == '__main__':
    KeyPoolTest().main()
