// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2018 The VERGE Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/block.h>

#include <hash.h>
#include <tinyformat.h>
#include <util/strencodings.h>
#include <crypto/common.h>

#include <crypto/pow/scrypt.h>
#include <crypto/pow/hashgroestl.h>
#include <crypto/pow/hashblake.h>
#include <crypto/pow/hashx17.h>
#include <crypto/pow/Lyra2RE.h>

int ALGO = ALGO_SCRYPT;

uint256 CBlockHeader::GetHash() const 
{
    return GetPoWHash(ALGO_SCRYPT);
}

uint256 CBlockHeader::GetSerializedHash() const 
{
    return SerializeHash(*this);
}

uint256 CBlockHeader::GetPoWHash(int algo) const
{
    uint256 thash;
    switch (algo)
    {
        case ALGO_GROESTL:
            return HashGroestl(BEGIN(nVersion), END(nNonce));
        case ALGO_BLAKE:
            return HashBlake(BEGIN(nVersion), END(nNonce));
        case ALGO_X17:
            return HashX17(BEGIN(nVersion), END(nNonce));
        case ALGO_LYRA2RE:
            lyra2re2_hash(BEGIN(nVersion), BEGIN(thash));
	    break;
        case ALGO_SCRYPT:
        default:
            scrypt_1024_1_1_256(BEGIN(nVersion), BEGIN(thash));

    }
    return thash;
}

bool CBlock::SignBlock(const CKeyStore& keystore)
{
    std::vector<valtype> vSolutions;
    txnouttype whichType;

    for(unsigned int i = 0; i < vtx[0]->vout.size(); i++)
    {
        const CTxOut& txout = vtx[0]->vout[i];

        if (!Solver(txout.scriptPubKey, whichType, vSolutions))
            continue;

        if (whichType == TX_PUBKEY)
        {
            // Sign
            valtype& vchPubKey = vSolutions[0];
            CKey key;
            CPubKey pubKey(vchPubKey);

            if (!keystore.GetKey(pubKey.GetID(), key)){
                LogPrintf("[SignBlock] Key not found for singature\n");
                continue;
            }
            if (key.GetPubKey() != pubKey){
                LogPrintf("[SignBlock] Keys not identical (generated vs found)\n");
                continue;
            }
            if(!key.Sign(GetHash(), vchBlockSig)){
                LogPrintf("[SignBlock] Could not sign block\n");
                continue;
            }

            LogPrintf("BlockSign successfully done with: TX_PUBKEY\n");
            return true;
        } else if (whichType == TX_PUBKEYHASH) {
            // Sign
            valtype& vchPubKey = vSolutions[0];
            CKey key;
            CKeyID keyID = CKeyID(uint160(vchPubKey));

            if (!keystore.GetKey(keyID, key)) {
                LogPrintf("[SignBlock] Key not found for singature\n");
                continue;
            }
            if (key.GetPubKey().GetID() != keyID){
                LogPrintf("[SignBlock] Keys not identical (generated vs found)\n");
                continue;
            }
            if(!key.Sign(GetHash(), vchBlockSig)){
                LogPrintf("[SignBlock] Could not sign block\n");
                continue;
            }

            LogPrintf("BlockSign successfully done with: TX_PUBKEYHASH\n");
            return true;
        }
    }

    LogPrintf("Sign failed\n");
    return false;
}

bool CBlock::CheckBlockSignature() const 
{
    uint256 genesisBlockHash = uint256S("0x00000fc63692467faeb20cdb3b53200dc601d75bdfa1001463304cc790d77278");
    uint256 genesisTestBlockHash = uint256S("0x65b4e101cacf3e1e4f3a9237e3a74ffd1186e595d8b78fa8ea22c21ef5bf9347");
    if (GetHash() == (gArgs.IsArgSet("-testnet") ? genesisTestBlockHash : genesisBlockHash))
        return vchBlockSig.empty();

    std::vector<valtype> vSolutions;
    txnouttype whichType;

    // check for block signature in Proof Of Work (old shit why we have to do this.)
    for(unsigned int i = 0; i < vtx[0]->vout.size(); i++)
    {
        const CTxOut& txout = vtx[0]->vout[i];

        if (!Solver(txout.scriptPubKey, whichType, vSolutions))
            return false;

        if (whichType == TX_PUBKEY)
        {
            // Verify
            valtype& vchPubKey = vSolutions[0];
            CPubKey key(vchPubKey);
            if (!key.IsFullyValid())
                continue;
            if (vchBlockSig.empty())
                continue;
            if(!key.Verify(GetHash(), vchBlockSig))
                continue;

            return true;
        }
    }
    
    return false;
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, algo=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%u, blocksignature=%s)\n",
        GetHash().ToString(),
	GetAlgoName(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce,
        vtx.size(),
        HexStr(vchBlockSig).c_str());
    for (const auto& tx : vtx) {
        s << "  " << tx->ToString() << "\n";
    }
    return s.str();
}
