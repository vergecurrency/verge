// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2019 The VERGE Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_POW_H
#define VERGE_POW_H

#include <consensus/params.h>

#include <stdint.h>

class CBlockHeader;
class CBlockIndex;
class uint256;

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, int algo, const Consensus::Params&);
// unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params&);

/** Check whether a block hash satisfies the proof-of-work requirement specified by nBits */
bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params&);

void avgRecentTimestamps(const CBlockIndex* pindexLast, int64_t *avgOf5, int64_t *avgOf7, int64_t *avgOf9, int64_t *avgOf17);
unsigned int GetNextTargetRequired_V2(const CBlockIndex* pindexLast, int algo, const Consensus::Params& params);
unsigned int DarkGravityWave3(const CBlockIndex* pindexLast, int algo, const Consensus::Params& params);
unsigned int GetNextTargetRequired_V1(const CBlockIndex* pindexLast, int algo, const Consensus::Params& params);
unsigned int LwmaCalculateNextWorkRequired(const CBlockIndex* pindexLast, int algo, const Consensus::Params& params);
unsigned int GetNextTargetRequired(const CBlockIndex* pindexLast, int algo, const Consensus::Params& params);
unsigned int GetAlgoWeight(int algo);

#endif // VERGE_POW_H
