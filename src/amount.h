// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2025 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_AMOUNT_H
#define VERGE_AMOUNT_H

#include <stdint.h>

/** Amount in satoshis (Can be negative) */
typedef int64_t CAmount;

// static const CAmount COIN = 100000000;
// static const CAmount CENT = 1000000;
// Use peercoin standard
static const CAmount COIN = 1000000;
static const CAmount CENT = 10000;


/** No amount larger than this (in satoshi) is valid.
 *
 * Note that this constant is *not* the total money supply, which in VERGE
 * currently happens to be less than 16,555,000,000 XVG for various reasons, but
 * rather a sanity check. As this sanity check is used by consensus-critical
 * validation code, the exact value of the MAX_MONEY constant is consensus
 * critical; in unusual circumstances like a(nother) overflow bug (on the bitcoin blockchain) that allowed
 * for the creation of coins out of thin air modification could lead to a fork.
 * */
static const CAmount MAX_MONEY = 16555000000 * COIN;
inline bool MoneyRange(const CAmount& nValue) { return (nValue >= 0 && nValue <= MAX_MONEY); }

#endif //  VERGE_AMOUNT_H
