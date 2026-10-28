// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_WALLET_STAKER_H
#define VERGE_WALLET_STAKER_H

#include <uint256.h>
#include <amount.h>

#include <cstdint>
#include <string>
#include <vector>

class CWallet;

struct WalletStakeBondInfo
{
    uint256 txid;
    uint32_t vout{0};
    CAmount amount{0};
    int creation_height{0};
    int confirmations{0};
    bool mature{false};
    bool eligible{false};
    bool eligibility_locked{false};
    bool withdrawal_locked{false};
    bool unbond_pending{false};
    int64_t unlock_height{0};
    int64_t blocks_until_unlock{0};
};

struct WalletUnbondingInfo
{
    uint256 txid;
    uint32_t vout{0};
    CAmount amount{0};
    int confirmations{0};
    uint32_t encoded_unlock_height{0};
    int64_t unlock_height{0};
    int64_t blocks_until_unlock{0};
    uint160 withdrawal_key_id;
};

struct WalletUnbondResult
{
    uint256 txid;
    int64_t unlock_height{0};
    CAmount amount{0};
};

bool TryStakeBlock(CWallet& wallet, uint256& block_hash, std::string& error,
                   bool force = false);
bool EnsureAutomaticStakeBond(CWallet& wallet, uint256& txid,
                              CAmount& amount, std::string& error);
std::vector<WalletStakeBondInfo> GetWalletStakeBonds(CWallet& wallet);
std::vector<WalletUnbondingInfo> GetWalletUnbondings(CWallet& wallet);
bool UnbondWalletStake(CWallet& wallet, const uint256& txid, uint32_t vout,
                       WalletUnbondResult& result, std::string& error);
void StakeWallets();

#endif // VERGE_WALLET_STAKER_H
