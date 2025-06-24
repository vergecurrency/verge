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

/** Default Port to run tor entry node on **/
static const unsigned int DEFAULT_TOR_PORT = 9090;

/** Default Port for handling tor's control port **/
static const unsigned int DEFAULT_TOR_CONTROL_PORT = 9051;

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