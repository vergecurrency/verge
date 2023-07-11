// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2023 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/fees.h>

#include <policy/policy.h>
#include <txmempool.h>
#include <util/system.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/wallet.h>
#include <math.h>


CAmount GetRequiredFee(const CWallet& wallet, unsigned int nTxBytes)
{
    return GetRequiredFeeRate(wallet).GetFee(nTxBytes);
}

CAmount GetMinimumFee(const CWallet& wallet, unsigned int nTxBytes, const CCoinControl& coin_control, const CTxMemPool& pool, const CBlockPolicyEstimator& estimator, FeeCalculation* feeCalc)
{
    return GetMinimumFeeRate(wallet, coin_control, pool, estimator, feeCalc).GetFee(nTxBytes);
}

CFeeRate GetRequiredFeeRate(const CWallet& wallet)
{
    return std::max(wallet.m_min_fee, ::minRelayTxFee);
}

CFeeRate GetMinimumFeeRate(const CWallet& wallet, const CCoinControl& coin_control, const CTxMemPool& pool, const CBlockPolicyEstimator& estimator, FeeCalculation* feeCalc)
{
    CFeeRate required_feerate = GetRequiredFeeRate(wallet);

    CFeeRate feerate_needed = required_feerate;
    if (feeCalc) {
        feeCalc->reason = FeeReason::REQUIRED;
    }

    return feerate_needed;
}

CFeeRate GetDiscardRate(const CWallet& wallet, const CBlockPolicyEstimator& estimator)
{
    // unsigned int highest_target = estimator.HighestTargetTracked(FeeEstimateHorizon::LONG_HALFLIFE);
    // CFeeRate discard_rate = estimator.estimateSmartFee(highest_target, nullptr /* FeeCalculation */, false /* conservative */);
    // Don't let discard_rate be greater than longest possible fee estimate if we get a valid fee estimate
    // discard_rate = (discard_rate == CFeeRate(0)) ? wallet.m_discard_rate : std::min(discard_rate, wallet.m_discard_rate);
    // Discard rate must be at least dustRelayFee
    CFeeRate discard_rate = ::dustRelayFee;
    return discard_rate;
}
