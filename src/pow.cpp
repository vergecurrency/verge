// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2023 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow.h>

#include <arith_uint256.h>
#include <chain.h>
#include <primitives/block.h>
#include <uint256.h>
#include <util/system.h>
#include <chainparamsbase.h>
#include <chainparams.h>

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, int algo, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);
    // unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    // Only change once per difficulty adjustment interval
    // KeyNote: We update every block
    // if ((pindexLast->nHeight+1) % params.DifficultyAdjustmentInterval() != 0)
    // {
    //     if (params.fPowAllowMinDifficultyBlocks)
    //     {
    //         // Special difficulty rule for testnet:
    //         // If the new block's timestamp is more than 2* 10 minutes
    //         // then allow mining of a min-difficulty block.
    //         if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*2)
    //             return nProofOfWorkLimit;
    //         else
    //         {
    //             // Return the last non-special-min-difficulty-rules-block
    //             const CBlockIndex* pindex = pindexLast;
    //             while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 && pindex->nBits == nProofOfWorkLimit)
    //                 pindex = pindex->pprev;
    //             return pindex->nBits;
    //         }
    //     }
    //     return pindexLast->nBits;
    // }

    // Go back by what we want to be 14 days worth of blocks
    // int nHeightFirst = pindexLast->nHeight - (params.DifficultyAdjustmentInterval()-1);
    // assert(nHeightFirst >= 0);
    // const CBlockIndex* pindexFirst = pindexLast->GetAncestor(nHeightFirst);
    // assert(pindexFirst);

    // return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);
    return GetNextTargetRequired(pindexLast, algo, params);
}

// unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
// {
//     if (params.fPowNoRetargeting)
//         return pindexLast->nBits;

//     // Limit adjustment step
//     int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
//     if (nActualTimespan < params.nPowTargetTimespan/4)
//         nActualTimespan = params.nPowTargetTimespan/4;
//     if (nActualTimespan > params.nPowTargetTimespan*4)
//         nActualTimespan = params.nPowTargetTimespan*4;

//     // Retarget
//     const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
//     arith_uint256 bnNew;
//     bnNew.SetCompact(pindexLast->nBits);
//     bnNew *= nActualTimespan;
//     bnNew /= params.nPowTargetTimespan;

//     if (bnNew > bnPowLimit)
//         bnNew = bnPowLimit;

//     return bnNew.GetCompact();
// }

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);
    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    // Check proof of work matches claimed amount
    if (Params().NetworkIDString() == CBaseChainParams::TESTNET || Params().NetworkIDString() == CBaseChainParams::REGTEST) {
        if (hash != params.hashGenesisBlock && UintToArith256(hash) > bnTarget) 
            return false;
    } else {
        if (UintToArith256(hash) > bnTarget)
            return false;
    }

    return true;
}



// =================================================================
// verge part
// -----------------------------------------------------------------


// This is MIDAS (Multi Interval Difficulty Adjustment System), a novel getnextwork algorithm.  It responds quickly to
// huge changes in hashing power, is immune to time warp attacks, and regulates the block rate to keep the block height
// close to the block height expected given the nominal block interval and the elapsed time.  How close the
// correspondence between block height and wall clock time is, depends on how stable the hashing power has been.  Maybe
// Bitcoin can wait 2 weeks between updates but no altcoin can.

// It is important that none of these intervals (5, 7, 9, 17) have any common divisor; eliminating the existence of
// harmonics is an important part of eliminating the effectiveness of timewarp attacks.
void avgRecentTimestamps(const CBlockIndex* pindexLast, int64_t *avgOf5, int64_t *avgOf7, int64_t *avgOf9, int64_t *avgOf17)
{
  int blockoffset = 0;
  int64_t oldblocktime;
  int64_t blocktime;

  *avgOf5 = *avgOf7 = *avgOf9 = *avgOf17 = 0;
  if (pindexLast)
    blocktime = pindexLast->GetBlockTime();
  else blocktime = 0;

  for (blockoffset = 0; blockoffset < 17; blockoffset++)
  {
    oldblocktime = blocktime;
    if (pindexLast)
    {
      pindexLast = pindexLast->pprev;
      blocktime = pindexLast->GetBlockTime();
    }
    else
    { // genesis block or previous
    blocktime -= GetTargetSpacing(pindexLast->nHeight);
    }
    // for each block, add interval.
    if (blockoffset < 5) *avgOf5 += (oldblocktime - blocktime);
    if (blockoffset < 7) *avgOf7 += (oldblocktime - blocktime);
    if (blockoffset < 9) *avgOf9 += (oldblocktime - blocktime);
    *avgOf17 += (oldblocktime - blocktime);    
  }
  // now we have the sums of the block intervals. Division gets us the averages. 
  *avgOf5 /= 5;
  *avgOf7 /= 7;
  *avgOf9 /= 9;
  *avgOf17 /= 17;
}

//unsigned int GetNextWorkRequired(const CBlockIndex *pindexLast, const CBlockHeader *pblock)
unsigned int GetNextTargetRequired_V2(const CBlockIndex* pindexLast, int algo, const Consensus::Params& params)
{
    const int64_t nTargetTimespan = 1 * 15;  // FIXME: quick work-around for the old global timespan
    int64_t avgOf5;
    int64_t avgOf9;
    int64_t avgOf7;
    int64_t avgOf17;
    int64_t toofast;
    int64_t tooslow;
    int64_t difficultyfactor = 10000;
    int64_t now;
    int64_t BlockHeightTime;

    int64_t nFastInterval = (GetTargetSpacing(pindexLast->nHeight) * 9 ) / 10; // seconds per block desired when far behind schedule
    int64_t nSlowInterval = (GetTargetSpacing(pindexLast->nHeight) * 11) / 10; // seconds per block desired when far ahead of schedule
    int64_t nIntervalDesired;
    
    unsigned int nTargetLimit = UintToArith256(params.powLimit).GetCompact();
    
    if (pindexLast == NULL)
        // Genesis Block
        return nTargetLimit;
   
    // Regulate block times so as to remain synchronized in the long run with the actual time.  The first step is to
    // calculate what interval we want to use as our regulatory goal.  It depends on how far ahead of (or behind)
    // schedule we are.  If we're more than an adjustment period ahead or behind, we use the maximum (nSlowInterval) or minimum
    // (nFastInterval) values; otherwise we calculate a weighted average somewhere in between them.  The closer we are
    // to being exactly on schedule the closer our selected interval will be to our nominal interval (TargetSpacing).

    now = pindexLast->GetBlockTime();
    
    BlockHeightTime = 1412878964 + pindexLast->nHeight * GetTargetSpacing(pindexLast->nHeight); // FIXME: genesis nTime shouldn't be a const int
    
    if (now < BlockHeightTime + nTargetTimespan && now > BlockHeightTime )
    // ahead of schedule by less than one interval.
    nIntervalDesired = ((nTargetTimespan - (now - BlockHeightTime)) * GetTargetSpacing(pindexLast->nHeight) +  
                (now - BlockHeightTime) * nFastInterval) / GetTargetSpacing(pindexLast->nHeight);
    else if (now + nTargetTimespan > BlockHeightTime && now < BlockHeightTime)
    // behind schedule by less than one interval.
    nIntervalDesired = ((nTargetTimespan - (BlockHeightTime - now)) * GetTargetSpacing(pindexLast->nHeight) + 
                (BlockHeightTime - now) * nSlowInterval) / nTargetTimespan;

    // ahead by more than one interval;
    else if (now < BlockHeightTime) nIntervalDesired = nSlowInterval;
    
    // behind by more than an interval. 
    else  nIntervalDesired = nFastInterval;
    
    // find out what average intervals over last 5, 7, 9, and 17 blocks have been. 
    avgRecentTimestamps(pindexLast, &avgOf5, &avgOf7, &avgOf9, &avgOf17);    

    // check for emergency adjustments. These are to bring the diff up or down FAST when a burst miner or multipool
    // jumps on or off.  Once they kick in they can adjust difficulty very rapidly, and they can kick in very rapidly
    // after massive hash power jumps on or off.
    
    // Important note: This is a self-damping adjustment because 8/5 and 5/8 are closer to 1 than 3/2 and 2/3.  Do not
    // screw with the constants in a way that breaks this relationship.  Even though self-damping, it will usually
    // overshoot slightly. But normal adjustment will handle damping without getting back to emergency.
    toofast = (nIntervalDesired * 2) / 3;
    tooslow = (nIntervalDesired * 3) / 2;

    // both of these check the shortest interval to quickly stop when overshot.  Otherwise first is longer and second shorter.
    if (avgOf5 < toofast && avgOf9 < toofast && avgOf17 < toofast)
    {  //emergency adjustment, slow down (longer intervals because shorter blocks)
      difficultyfactor *= 8;
      difficultyfactor /= 5;
    }
    else if (avgOf5 > tooslow && avgOf7 > tooslow && avgOf9 > tooslow)
    {  //emergency adjustment, speed up (shorter intervals because longer blocks)
      difficultyfactor *= 5;
      difficultyfactor /= 8;
    }

    // If no emergency adjustment, check for normal adjustment. 
    else if (((avgOf5 > nIntervalDesired || avgOf7 > nIntervalDesired) && avgOf9 > nIntervalDesired && avgOf17 > nIntervalDesired) ||
         ((avgOf5 < nIntervalDesired || avgOf7 < nIntervalDesired) && avgOf9 < nIntervalDesired && avgOf17 < nIntervalDesired))
    { // At least 3 averages too high or at least 3 too low, including the two longest. This will be executed 3/16 of
      // the time on the basis of random variation, even if the settings are perfect. It regulates one-sixth of the way
      // to the calculated point.
      difficultyfactor *= (6 * nIntervalDesired);
      difficultyfactor /= (avgOf17 +(5 * nIntervalDesired));
    }

    // limit to doubling or halving.  There are no conditions where this will make a difference unless there is an
    // unsuspected bug in the above code.
    if (difficultyfactor > 20000) difficultyfactor = 20000;
    if (difficultyfactor < 5000) difficultyfactor = 5000;

    arith_uint256 bnNew;
    arith_uint256 bnOld;

    bnOld.SetCompact(pindexLast->nBits);

    if (difficultyfactor == 10000) // no adjustment. 
      return(bnOld.GetCompact());

    bnNew = bnOld / difficultyfactor;
    bnNew *= 10000;

    if (bnNew > UintToArith256(params.powLimit))
      bnNew = UintToArith256(params.powLimit);
    return bnNew.GetCompact();
     
}

unsigned int DarkGravityWave3(const CBlockIndex* pindexLast, int algo, const Consensus::Params& params) {
    /* difficulty formula, DarkGravity v3, written by Evan Duffield - evan@darkcoin.io */
    const CBlockIndex *BlockLastSolved = pindexLast;
    const CBlockIndex *BlockReading = pindexLast;

    long long nActualTimespan = 0;
    long long LastBlockTime = 0;
    long long PastBlocksMin = 12;
    long long PastBlocksMax = 12;
    long long CountBlocks = 0;
    arith_uint256 PastDifficultyAverage;
    arith_uint256 PastDifficultyAveragePrev;

    if (BlockLastSolved == NULL || BlockLastSolved->nHeight == 0 || BlockLastSolved->nHeight < PastBlocksMin) {
        return UintToArith256(params.powLimit).GetCompact();
    }

    while (BlockReading && BlockReading->nHeight > 0) {
        if (PastBlocksMax > 0 && CountBlocks >= PastBlocksMax)
        	break;

        // we only consider proof-of-work blocks for the configured mining algo here
        if (BlockReading->GetBlockHeader().GetAlgo() != algo) {
        	BlockReading = BlockReading->pprev;
            continue;
        }

        CountBlocks++;

		if (CountBlocks <= PastBlocksMin) {
			if (CountBlocks == 1)
				PastDifficultyAverage.SetCompact(BlockReading->nBits);
			else
				PastDifficultyAverage =
					((PastDifficultyAveragePrev * CountBlocks) + (arith_uint256().SetCompact(BlockReading->nBits))) / (CountBlocks + 1);

			PastDifficultyAveragePrev = PastDifficultyAverage;
		}

        if (LastBlockTime > 0){
        	long long Diff = (LastBlockTime - BlockReading->GetBlockTime());
            nActualTimespan += Diff;
        }
        LastBlockTime = BlockReading->GetBlockTime();

        if (BlockReading->pprev == NULL) {
            assert(BlockReading);
            break;
        }
        BlockReading = BlockReading->pprev;
    }

    if (!CountBlocks)
        return UintToArith256(params.powLimit).GetCompact();

    arith_uint256 bnNew(PastDifficultyAverage);

    long long nTargetTimespan = CountBlocks * GetTargetSpacing(pindexLast->nHeight);

    if (nActualTimespan < nTargetTimespan/3)
        nActualTimespan = nTargetTimespan/3;

    if (nActualTimespan > nTargetTimespan*3)
        nActualTimespan = nTargetTimespan*3;

    // Retarget
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;

    if (bnNew > UintToArith256(params.powLimit))
        bnNew = UintToArith256(params.powLimit);

    return bnNew.GetCompact();
}

unsigned int GetNextTargetRequired_V1(const CBlockIndex* pindexLast, int algo, const Consensus::Params& params)
{
    const int64_t nTargetTimespan = 1 * 60;  // quick work-around for the old global timespan
    unsigned int nStakeTargetSpacing = 30;		// 30 seconds POS block spacing
    const int64_t nTargetSpacingWorkMax = 2 * nStakeTargetSpacing; // 1 minutes

    if (pindexLast == NULL)
        return UintToArith256(params.powLimit).GetCompact(); // genesis block

    // Proof-of-Stake blocks has own target limit
    arith_uint256 bnTargetLimit = UintToArith256(params.powLimit);

    const CBlockIndex* pindexPrev = pindexLast->GetAncestor(pindexLast->nHeight);
    if (pindexPrev == nullptr || pindexPrev->pprev == nullptr)
        return bnTargetLimit.GetCompact(); // first block
    const CBlockIndex* pindexPrevPrev = pindexPrev->GetAncestor(pindexLast->nHeight - 1); 
    if (pindexPrevPrev->pprev == NULL)
        return bnTargetLimit.GetCompact(); // second block

    if(pindexLast->nHeight < 450)
	    return bnTargetLimit.GetCompact();

    int64_t nActualSpacing = pindexPrev->GetBlockTime() - pindexPrevPrev->GetBlockTime();

    if(nActualSpacing < 0)
    {
	    nActualSpacing = 1;
    }
    else if(nActualSpacing > nTargetTimespan)
    {
	    nActualSpacing = nTargetTimespan;
    }

    // ppcoin: target change every block
    // ppcoin: retarget with exponential moving toward target spacing
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexPrev->nBits);

    int64_t nTargetSpacing = std::min(nTargetSpacingWorkMax, (int64_t) nStakeTargetSpacing * (1 + pindexLast->nHeight - pindexPrev->nHeight));
    int64_t nInterval = nTargetTimespan / nTargetSpacing;

    bnNew *= ((nInterval - 1) * nTargetSpacing + nActualSpacing + nActualSpacing);
    bnNew /= ((nInterval + 1) * nTargetSpacing);

    if (bnNew > bnTargetLimit)
        bnNew = bnTargetLimit;

    return bnNew.GetCompact();
}

unsigned int LwmaCalculateNextWorkRequired(const CBlockIndex* pindexLast, int algo, const Consensus::Params& params)
{

    // LWMA for BTC clones
    // Algorithm by zawy, LWMA idea by Tom Harding
    // Code by h4x3rotab of BTC Gold, modified/updated by zawy
    // https://github.com/zawy12/difficulty-algorithms/issues/3#issuecomment-388386175
    //  FTL must be changed to about N*T/20 = 360 for T=120 and N=60 coins.
    //  FTL is MAX_FUTURE_BLOCK_TIME in chain.h.
    //  FTL in Ignition, Numus, and others can be found in main.h as DRIFT.
    //  Some coins took out a variable, and need to change the 2*60*60 here:
    //  if (block.GetBlockTime() > nAdjustedTime + 2 * 60 * 60)

    const int64_t FTL = GetMaxClockDrift(pindexLast->nHeight);
    const int64_t T = GetTargetSpacing(pindexLast->nHeight);
    const int64_t N = 60; // Avg Window
    const int64_t k = N*(N+1)*T/2; 
    const int height = pindexLast->nHeight + 1;
    const bool debugprint = false; // TODO: implement
    assert(height > N);

    arith_uint256 sum_target;
    int64_t t = 0, j = 0;
    int64_t solvetime;

    std::vector<const CBlockIndex*> SameAlgoBlocks;
    for (int c = height-1; SameAlgoBlocks.size() < (N + 1); c--){
        const CBlockIndex* block = pindexLast->GetAncestor(c); // -1 after execution
        if (block->GetBlockHeader().GetAlgo() == algo){
            SameAlgoBlocks.push_back(block);
        }
        if (c < 100){ // If there are not enough blocks with this algo, return with an algo that *can* use less blocks
            return GetNextTargetRequired_V1(pindexLast, algo, params);
        }
    }
    // Creates vector with {block1000, block997, block993}, so we start at the back

    // Loop through N most recent blocks. starting with the lowest blockheight
    for (int i = N; i > 0; i--) {
        const CBlockIndex* block = SameAlgoBlocks[i-1]; // pindexLast->GetAncestor(i);
        const CBlockIndex* block_Prev = SameAlgoBlocks[i]; //block->GetAncestor(i - 1);
        solvetime = block->GetBlockTime() - block_Prev->GetBlockTime();
        solvetime = std::max(-FTL, std::min(solvetime, 6*T));
        
        j++;
        t += solvetime * j;  // Weighted solvetime sum.

        // Target sum divided by a factor, (k N^2).
        // The factor is a part of the final equation. However we divide sum_target here to avoid
        // potential overflow.
        arith_uint256 target;
        target.SetCompact(block->nBits);
        sum_target += target / (k * N);
    }
    // Keep t reasonable in case strange solvetimes occurred.
    if (t < k/10 ) {   t = k/10;  }

    const arith_uint256 pow_limit = UintToArith256(params.powLimit);
    arith_uint256 next_target = t * sum_target;
    if (next_target > pow_limit) {
        next_target = pow_limit;
    }

    return next_target.GetCompact();
}



unsigned int GetNextTargetRequired(const CBlockIndex* pindexLast, int algo, const Consensus::Params& params)
{
    if(gArgs.GetChainName() == "main" || gArgs.GetChainName() == "regtest"){
        if (pindexLast->nHeight < params.MULTI_ALGO_SWITCH_BLOCK)
        {
            return GetNextTargetRequired_V1(pindexLast, algo, params);
        } 

        return DarkGravityWave3(pindexLast, algo, params);//Then DarkGravityWave3
    }
    else
    {
        if (pindexLast->nHeight < 340000){//first 340000 blocks with default retarget
            return GetNextTargetRequired_V1(pindexLast, algo, params);
        }
        return LwmaCalculateNextWorkRequired(pindexLast, algo, params); // And finally LWMA (XSHGPU implementation)
    }
}

unsigned int GetAlgoWeight(int algo)
{
    switch (algo)
    {
        case ALGO_GROESTL:
            return (unsigned int)(0.005 * 100000);
        case ALGO_BLAKE:
            return (unsigned int)(0.00015 * 100000);
        case ALGO_X17:
            return (unsigned int)(6 * 100000);
        case ALGO_LYRA2RE:
            return (unsigned int)(6 * 100000);
        case ALGO_SCRYPT:
            return (unsigned int)(1.4 * 100000);
        default: // Lowest
            printf("GetAlgoWeight(): can't find algo %d", algo);
            return (unsigned int)(0.00015 * 100000);
    }
}
