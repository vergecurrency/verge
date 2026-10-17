// Copyright (c) 2018-2025 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * Functionality for communicating with Tor.
 */
#ifndef VERGE_TORCONTROLLER_H
#define VERGE_TORCONTROLLER_H

#include <stdint.h>
#include <atomic>
#include <deque>
#include <thread>
#include <memory>
#include <string> 
#include <iostream>

#include <boost/thread.hpp>
#include <util/system.h>
#include <threadinterrupt.h>
#include <scheduler.h>

/** Network-specific ports allow mainnet, testnet, and regtest to run together. */
unsigned int GetDefaultTorSocksPort();
unsigned int GetTorSocksPort();
unsigned int GetDefaultTorControlPort();
std::string GetDefaultTorControl();

char *convert_str(const std::string &s);

void run_tor();

/**
 * Initializes a new tor thread within a new thread 
 **/
void InitalizeTorThread();

/**
 * Stops the known thread and tries to kill it
 **/
void StopTorController();

/**
 * Internally starts within the new thread
 **/
void StartTorController();

#endif /* VERGE_TORCONTROLLER_H */
