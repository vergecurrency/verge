// Copyright (c) 2018-2020 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * Functionality for communicating with Tor.
 */
#ifndef VERGE_TORRELAY_H
#define VERGE_TORRELAY_H

#include <stdint.h>
#include <atomic>
#include <deque>
#include <thread>
#include <memory>
#include <string> 
#include <iostream>

#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <util/system.h>
#include <threadinterrupt.h>
#include <scheduler.h>
#include <networking/torEncoding.h>

/**
 * Initializes a new tor thread within a new thread 
 **/
void InitalizeTorRelayThread();

/**
 * Stops the known thread and tries to kill it
 **/
void StopTorRelay();

/**
 * Internally starts within the new thread
 **/
void StartTorRelay();

#endif /* VERGE_TORRELAY_H */