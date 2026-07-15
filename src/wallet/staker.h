// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_WALLET_STAKER_H
#define VERGE_WALLET_STAKER_H

#include <uint256.h>

#include <string>

class CWallet;

bool TryStakeBlock(CWallet& wallet, uint256& block_hash, std::string& error,
                   bool force = false);
void StakeWallets();

#endif // VERGE_WALLET_STAKER_H
