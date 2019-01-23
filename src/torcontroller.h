// Copyright (c) 2018-2019 The VERGE Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * Functionality for communicating with Tor.
 */
#ifndef VERGE_TORCONTROLLER_H
#define VERGE_TORCONTROLLER_H

#include <stdint.h>

#include <string>
#include <cstring>
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <addrdb.h>
#include <addrman.h>
#include <amount.h>
#include <bloom.h>
#include <compat.h>
#include <crypto/siphash.h>
#include <hash.h>
#include <limitedmap.h>
#include <netaddress.h>
#include <policy/feerate.h>
#include <protocol.h>
#include <random.h>
#include <streams.h>
#include <sync.h>
#include <uint256.h>
#include <threadinterrupt.h>
#include <openssl/crypto.h>

#include <atomic>
#include <deque>
#include <stdint.h>
#include <thread>
#include <memory>
#include <condition_variable>
#include <chainparams.h>
#include <clientversion.h>
#include <consensus/consensus.h>
#include <crypto/common.h>
#include <crypto/sha256.h>
#include <primitives/transaction.h>
#include <netbase.h>
#include <scheduler.h>
#include <ui_interface.h>
#include <util/strencodings.h>

/** Default Port to run tor entry node on **/
static const unsigned int DEFAULT_TOR_PORT = 9089;

/** Default Port for handling tor's control port **/
static const unsigned int DEFAULT_TOR_CONTROL_PORT = 9051;

char *convert_str(const std::string &s);
void run_tor();
void StartTor();

#endif /* VERGE_TORCONTROLLER_H */