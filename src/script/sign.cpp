// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2023 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/sign.h>
#include <script/pq.h>
#include <pos/pqcrypto.h>

#include <key.h>
#include <policy/policy.h>
#include <pos/stake.h>
#include <primitives/transaction.h>
#include <script/standard.h>
#include <uint256.h>


typedef std::vector<unsigned char> valtype;

static bool SolverForSigning(const CScript& script, txnouttype& type,
                             std::vector<valtype>& solutions)
{
    if (Solver(script, type, solutions)) return true;

    pos::BondData bond;
    pos::UnbondData unbond;
    uint160 withdrawal_key_id;
    if (pos::ParseBondScript(script, bond)) {
        withdrawal_key_id = bond.withdrawal_key_id;
    } else if (pos::ParseUnbondScript(script, unbond)) {
        withdrawal_key_id = unbond.withdrawal_key_id;
    } else {
        return false;
    }

    // Only the stack template is P2PKH; signatures still commit to all metadata.
    type = TX_PUBKEYHASH;
    solutions = {valtype(withdrawal_key_id.begin(), withdrawal_key_id.end())};
    return true;
}

MutableTransactionSignatureCreator::MutableTransactionSignatureCreator(
    const CMutableTransaction* txToIn, unsigned int nInIn,
    const CAmount& amountIn, int nHashTypeIn,
    const ReplayProtectionContext& replay_context)
    : txTo(txToIn), nIn(nInIn), nHashType(nHashTypeIn), amount(amountIn),
      replay_context(replay_context), checker(txTo, nIn, amountIn, replay_context) {}

bool MutableTransactionSignatureCreator::CreateSig(const SigningProvider& provider, std::vector<unsigned char>& vchSig, const CKeyID& address, const CScript& scriptCode, SigVersion sigversion) const
{
    CKey key;
    if (!provider.GetKey(address, key))
        return false;

    // Signing with uncompressed keys is disabled in witness scripts
    if (sigversion == SigVersion::WITNESS_V0 && !key.IsCompressed())
        return false;

    uint256 hash = SignatureHash(scriptCode, *txTo, nIn, nHashType, amount,
                                 sigversion, nullptr, replay_context);
    if (!key.Sign(hash, vchSig))
        return false;
    vchSig.push_back((unsigned char)nHashType);
    return true;
}

bool MutableTransactionSignatureCreator::CreatePQSig(const SigningProvider& provider,
    std::vector<unsigned char>& signature, const uint256& id, const CScript& script) const
{
    signature.clear();
    if (nIn >= txTo->vin.size() || nHashType != (SIGHASH_ALL |
        (replay_context.IsEnabled() ? SIGHASH_POS_FORKID : 0))) return false;
    pos::experimental::MLDSA44Key key;
    if (!provider.GetPQKey(id, key)) return false;
    const auto transaction_hash = SignatureHash(script, *txTo, nIn, nHashType, amount,
        SigVersion::BASE, nullptr, replay_context);
    uint256 hash;
    if (!pos::experimental::GetPQTransactionHash(replay_context.network_id,
        replay_context.activation_height, amount, transaction_hash, hash) ||
        !key.Sign({hash.begin(), hash.end()}, pos::experimental::PQTransactionContext(), signature)) return false;
    signature.push_back(nHashType);
    return true;
}

static bool Sign1(const SigningProvider& provider, const CKeyID& address, const BaseSignatureCreator& creator, const CScript& scriptCode, std::vector<valtype>& ret, SigVersion sigversion)
{
    std::vector<unsigned char> vchSig;
    if (!creator.CreateSig(provider, vchSig, address, scriptCode, sigversion))
        return false;
    ret.push_back(vchSig);
    return true;
}

static bool SignN(const SigningProvider& provider, const std::vector<valtype>& multisigdata, const BaseSignatureCreator& creator, const CScript& scriptCode, std::vector<valtype>& ret, SigVersion sigversion)
{
    int nSigned = 0;
    int nRequired = multisigdata.front()[0];
    for (unsigned int i = 1; i < multisigdata.size()-1 && nSigned < nRequired; i++)
    {
        const valtype& pubkey = multisigdata[i];
        CKeyID keyID = CPubKey(pubkey).GetID();
        if (Sign1(provider, keyID, creator, scriptCode, ret, sigversion))
            ++nSigned;
    }
    return nSigned==nRequired;
}

/**
 * Sign scriptPubKey using signature made with creator.
 * Signatures are returned in scriptSigRet (or returns false if scriptPubKey can't be signed),
 * unless whichTypeRet is TX_SCRIPTHASH, in which case scriptSigRet is the redemption script.
 * Returns false if scriptPubKey could not be completely satisfied.
 */
static bool SignStep(const SigningProvider& provider, const BaseSignatureCreator& creator, const CScript& scriptPubKey,
                     std::vector<valtype>& ret, txnouttype& whichTypeRet, SigVersion sigversion)
{
    CScript scriptRet;
    uint160 h160;
    ret.clear();

    std::vector<valtype> vSolutions;
    if (!SolverForSigning(scriptPubKey, whichTypeRet, vSolutions))
        return false;

    CKeyID keyID;
    switch (whichTypeRet)
    {
    case TX_NONSTANDARD:
    case TX_NULL_DATA:
    case TX_WITNESS_UNKNOWN:
        return false;
    case TX_PUBKEY:
        keyID = CPubKey(vSolutions[0]).GetID();
        return Sign1(provider, keyID, creator, scriptPubKey, ret, sigversion);
    case TX_PUBKEYHASH:
        keyID = CKeyID(uint160(vSolutions[0]));
        if (!Sign1(provider, keyID, creator, scriptPubKey, ret, sigversion))
            return false;
        else
        {
            CPubKey vch;
            provider.GetPubKey(keyID, vch);
            ret.push_back(ToByteVector(vch));
        }
        return true;
    case TX_SCRIPTHASH:
        if (provider.GetCScript(uint160(vSolutions[0]), scriptRet)) {
            ret.push_back(std::vector<unsigned char>(scriptRet.begin(), scriptRet.end()));
            return true;
        }
        return false;

    case TX_MULTISIG:
        ret.push_back(valtype()); // workaround CHECKMULTISIG bug
        return (SignN(provider, vSolutions, creator, scriptPubKey, ret, sigversion));

    case TX_WITNESS_V0_KEYHASH:
        ret.push_back(vSolutions[0]);
        return true;

    case TX_WITNESS_V0_SCRIPTHASH:
        CRIPEMD160().Write(&vSolutions[0][0], vSolutions[0].size()).Finalize(h160.begin());
        if (provider.GetCScript(h160, scriptRet)) {
            ret.push_back(std::vector<unsigned char>(scriptRet.begin(), scriptRet.end()));
            return true;
        }
        return false;

    default:
        return false;
    }
}

static CScript PushAll(const std::vector<valtype>& values)
{
    CScript result;
    for (const valtype& v : values) {
        if (v.size() == 0) {
            result << OP_0;
        } else if (v.size() == 1 && v[0] >= 1 && v[0] <= 16) {
            result << CScript::EncodeOP_N(v[0]);
        } else {
            result << v;
        }
    }
    return result;
}

bool ProduceSignature(const SigningProvider& provider, const BaseSignatureCreator& creator, const CScript& fromPubKey, SignatureData& sigdata)
{
    uint256 pq_id;
    if (ExtractPQMultisigId(fromPubKey, pq_id)) {
        PQMultisigPolicy policy;
        std::vector<unsigned char> encoded;
        std::vector<PQMultisigSignature> existing;
        // A partial transaction carries its policy for cosigners without a local copy.
        if (!sigdata.scriptWitness.IsNull()) return false;
        if (provider.GetPQMultisigPolicy(pq_id, encoded)) {
            if (!DecodePQMultisigPolicy(encoded, policy)) return false;
        } else if (!DecodePQMultisigScriptSig(sigdata.scriptSig, policy, existing)) {
            return false;
        }
        if (GetPQMultisigPolicyId(policy) != pq_id) return false;
        PQMultisigPolicy previous_policy;
        std::vector<PQMultisigSignature> previous;
        if (DecodePQMultisigScriptSig(sigdata.scriptSig, previous_policy, previous) &&
            GetPQMultisigPolicyId(previous_policy) == pq_id) existing = std::move(previous);
        std::map<unsigned int, PQMultisigSignature> signatures;
        for (const auto& item : existing) {
            if (pos::experimental::GetMLDSA44KeyId(item.public_key) == policy.keys[item.index] &&
                creator.Checker().CheckPQSig(item.signature, item.public_key, fromPubKey)) {
                signatures.emplace(item.index, item);
            }
        }
        for (unsigned int i = 0; i < policy.keys.size() && signatures.size() < policy.required; ++i) {
            if (signatures.count(i)) continue;
            PQMultisigSignature item;
            item.index = i;
            if (provider.GetPQPubKey(policy.keys[i], item.public_key) &&
                pos::experimental::GetMLDSA44KeyId(item.public_key) == policy.keys[i] &&
                creator.CreatePQSig(provider, item.signature, policy.keys[i], fromPubKey) &&
                creator.Checker().CheckPQSig(item.signature, item.public_key, fromPubKey)) {
                signatures.emplace(i, std::move(item));
            }
        }
        std::vector<PQMultisigSignature> result;
        for (const auto& item : signatures) {
            if (result.size() == policy.required) break;
            result.push_back(item.second);
        }
        sigdata.scriptSig = GetPQMultisigScriptSig(policy, result);
        sigdata.scriptWitness.stack.clear();
        return result.size() == policy.required && VerifyScript(sigdata.scriptSig, fromPubKey,
            &sigdata.scriptWitness, STANDARD_SCRIPT_VERIFY_FLAGS | SCRIPT_VERIFY_PQ, creator.Checker());
    }
    if (ExtractPQKeyId(fromPubKey, pq_id)) {
        std::vector<unsigned char> signature, public_key;
        if (!provider.GetPQPubKey(pq_id, public_key) ||
            !creator.CreatePQSig(provider, signature, pq_id, fromPubKey)) return false;
        sigdata.scriptSig = GetPQScriptSig(signature, public_key);
        sigdata.scriptWitness.stack.clear();
        return VerifyScript(sigdata.scriptSig, fromPubKey, &sigdata.scriptWitness,
            STANDARD_SCRIPT_VERIFY_FLAGS | SCRIPT_VERIFY_PQ, creator.Checker());
    }
    std::vector<valtype> result;
    txnouttype whichType;
    bool solved = SignStep(provider, creator, fromPubKey, result, whichType, SigVersion::BASE);
    bool P2SH = false;
    CScript subscript;
    sigdata.scriptWitness.stack.clear();

    if (solved && whichType == TX_SCRIPTHASH)
    {
        // Solver returns the subscript that needs to be evaluated;
        // the final scriptSig is the signatures from that
        // and then the serialized subscript:
        subscript = CScript(result[0].begin(), result[0].end());
        solved = solved && SignStep(provider, creator, subscript, result, whichType, SigVersion::BASE) && whichType != TX_SCRIPTHASH;
        P2SH = true;
    }

    if (solved && whichType == TX_WITNESS_V0_KEYHASH)
    {
        CScript witnessscript;
        witnessscript << OP_DUP << OP_HASH160 << ToByteVector(result[0]) << OP_EQUALVERIFY << OP_CHECKSIG;
        txnouttype subType;
        solved = solved && SignStep(provider, creator, witnessscript, result, subType, SigVersion::WITNESS_V0);
        sigdata.scriptWitness.stack = result;
        result.clear();
    }
    else if (solved && whichType == TX_WITNESS_V0_SCRIPTHASH)
    {
        CScript witnessscript(result[0].begin(), result[0].end());
        txnouttype subType;
        solved = solved && SignStep(provider, creator, witnessscript, result, subType, SigVersion::WITNESS_V0) && subType != TX_SCRIPTHASH && subType != TX_WITNESS_V0_SCRIPTHASH && subType != TX_WITNESS_V0_KEYHASH;
        result.push_back(std::vector<unsigned char>(witnessscript.begin(), witnessscript.end()));
        sigdata.scriptWitness.stack = result;
        result.clear();
    }

    if (P2SH) {
        result.push_back(std::vector<unsigned char>(subscript.begin(), subscript.end()));
    }
    sigdata.scriptSig = PushAll(result);

    // Test solution
    return solved && VerifyScript(sigdata.scriptSig, fromPubKey, &sigdata.scriptWitness, STANDARD_SCRIPT_VERIFY_FLAGS, creator.Checker());
}

SignatureData DataFromTransaction(const CMutableTransaction& tx, unsigned int nIn)
{
    SignatureData data;
    assert(tx.vin.size() > nIn);
    data.scriptSig = tx.vin[nIn].scriptSig;
    data.scriptWitness = tx.vin[nIn].scriptWitness;
    return data;
}

void UpdateInput(CTxIn& input, const SignatureData& data)
{
    input.scriptSig = data.scriptSig;
    input.scriptWitness = data.scriptWitness;
}

bool SignSignature(const SigningProvider &provider, const CScript& fromPubKey, CMutableTransaction& txTo, unsigned int nIn, const CAmount& amount, int nHashType)
{
    assert(nIn < txTo.vin.size());

    MutableTransactionSignatureCreator creator(&txTo, nIn, amount, nHashType);

    SignatureData sigdata = DataFromTransaction(txTo, nIn);
    bool ret = ProduceSignature(provider, creator, fromPubKey, sigdata);
    UpdateInput(txTo.vin.at(nIn), sigdata);
    return ret;
}

bool SignSignature(const SigningProvider &provider, const CTransaction& txFrom, CMutableTransaction& txTo, unsigned int nIn, int nHashType)
{
    assert(nIn < txTo.vin.size());
    CTxIn& txin = txTo.vin[nIn];
    assert(txin.prevout.n < txFrom.vout.size());
    const CTxOut& txout = txFrom.vout[txin.prevout.n];

    return SignSignature(provider, txout.scriptPubKey, txTo, nIn, txout.nValue, nHashType);
}

static std::vector<valtype> CombineMultisig(const CScript& scriptPubKey, const BaseSignatureChecker& checker,
                               const std::vector<valtype>& vSolutions,
                               const std::vector<valtype>& sigs1, const std::vector<valtype>& sigs2, SigVersion sigversion)
{
    // Combine all the signatures we've got:
    std::set<valtype> allsigs;
    for (const valtype& v : sigs1)
    {
        if (!v.empty())
            allsigs.insert(v);
    }
    for (const valtype& v : sigs2)
    {
        if (!v.empty())
            allsigs.insert(v);
    }

    // Build a map of pubkey -> signature by matching sigs to pubkeys:
    assert(vSolutions.size() > 1);
    unsigned int nSigsRequired = vSolutions.front()[0];
    unsigned int nPubKeys = vSolutions.size()-2;
    std::map<valtype, valtype> sigs;
    for (const valtype& sig : allsigs)
    {
        for (unsigned int i = 0; i < nPubKeys; i++)
        {
            const valtype& pubkey = vSolutions[i+1];
            if (sigs.count(pubkey))
                continue; // Already got a sig for this pubkey

            if (checker.CheckSig(sig, pubkey, scriptPubKey, sigversion))
            {
                sigs[pubkey] = sig;
                break;
            }
        }
    }
    // Now build a merged CScript:
    unsigned int nSigsHave = 0;
    std::vector<valtype> result; result.push_back(valtype()); // pop-one-too-many workaround
    for (unsigned int i = 0; i < nPubKeys && nSigsHave < nSigsRequired; i++)
    {
        if (sigs.count(vSolutions[i+1]))
        {
            result.push_back(sigs[vSolutions[i+1]]);
            ++nSigsHave;
        }
    }
    // Fill any missing with OP_0:
    for (unsigned int i = nSigsHave; i < nSigsRequired; i++)
        result.push_back(valtype());

    return result;
}

namespace
{
struct Stacks
{
    std::vector<valtype> script;
    std::vector<valtype> witness;

    Stacks() {}
    explicit Stacks(const std::vector<valtype>& scriptSigStack_) : script(scriptSigStack_), witness() {}
    explicit Stacks(const SignatureData& data) : witness(data.scriptWitness.stack) {
        EvalScript(script, data.scriptSig, SCRIPT_VERIFY_STRICTENC, BaseSignatureChecker(), SigVersion::BASE);
    }

    SignatureData Output() const {
        SignatureData result;
        result.scriptSig = PushAll(script);
        result.scriptWitness.stack = witness;
        return result;
    }
};
}

static Stacks CombineSignatures(const CScript& scriptPubKey, const BaseSignatureChecker& checker,
                                 const txnouttype txType, const std::vector<valtype>& vSolutions,
                                 Stacks sigs1, Stacks sigs2, SigVersion sigversion)
{
    switch (txType)
    {
    case TX_NONSTANDARD:
    case TX_NULL_DATA:
    case TX_WITNESS_UNKNOWN:
        // Don't know anything about this, assume bigger one is correct:
        if (sigs1.script.size() >= sigs2.script.size())
            return sigs1;
        return sigs2;
    case TX_PUBKEY:
    case TX_PUBKEYHASH:
        // Signatures are bigger than placeholders or empty scripts:
        if (sigs1.script.empty() || sigs1.script[0].empty())
            return sigs2;
        return sigs1;
    case TX_WITNESS_V0_KEYHASH:
        // Signatures are bigger than placeholders or empty scripts:
        if (sigs1.witness.empty() || sigs1.witness[0].empty())
            return sigs2;
        return sigs1;
    case TX_SCRIPTHASH:
        if (sigs1.script.empty() || sigs1.script.back().empty())
            return sigs2;
        else if (sigs2.script.empty() || sigs2.script.back().empty())
            return sigs1;
        else
        {
            // Recur to combine:
            valtype spk = sigs1.script.back();
            CScript pubKey2(spk.begin(), spk.end());

            txnouttype txType2;
            std::vector<std::vector<unsigned char> > vSolutions2;
            SolverForSigning(pubKey2, txType2, vSolutions2);
            sigs1.script.pop_back();
            sigs2.script.pop_back();
            Stacks result = CombineSignatures(pubKey2, checker, txType2, vSolutions2, sigs1, sigs2, sigversion);
            result.script.push_back(spk);
            return result;
        }
    case TX_MULTISIG:
        return Stacks(CombineMultisig(scriptPubKey, checker, vSolutions, sigs1.script, sigs2.script, sigversion));
    case TX_WITNESS_V0_SCRIPTHASH:
        if (sigs1.witness.empty() || sigs1.witness.back().empty())
            return sigs2;
        else if (sigs2.witness.empty() || sigs2.witness.back().empty())
            return sigs1;
        else
        {
            // Recur to combine:
            CScript pubKey2(sigs1.witness.back().begin(), sigs1.witness.back().end());
            txnouttype txType2;
            std::vector<valtype> vSolutions2;
            SolverForSigning(pubKey2, txType2, vSolutions2);
            sigs1.witness.pop_back();
            sigs1.script = sigs1.witness;
            sigs1.witness.clear();
            sigs2.witness.pop_back();
            sigs2.script = sigs2.witness;
            sigs2.witness.clear();
            Stacks result = CombineSignatures(pubKey2, checker, txType2, vSolutions2, sigs1, sigs2, SigVersion::WITNESS_V0);
            result.witness = result.script;
            result.script.clear();
            result.witness.push_back(valtype(pubKey2.begin(), pubKey2.end()));
            return result;
        }
    default:
        return Stacks();
    }
}

SignatureData CombineSignatures(const CScript& scriptPubKey, const BaseSignatureChecker& checker,
                          const SignatureData& scriptSig1, const SignatureData& scriptSig2)
{
    uint256 pq_id;
    if (ExtractPQMultisigId(scriptPubKey, pq_id)) {
        PQMultisigPolicy policy;
        bool have_policy = false;
        std::map<unsigned int, PQMultisigSignature> merged;
        for (const auto* candidate : {&scriptSig1, &scriptSig2}) {
            PQMultisigPolicy parsed;
            std::vector<PQMultisigSignature> signatures;
            if (!candidate->scriptWitness.IsNull() ||
                !DecodePQMultisigScriptSig(candidate->scriptSig, parsed, signatures) ||
                GetPQMultisigPolicyId(parsed) != pq_id) continue;
            policy = parsed;
            have_policy = true;
            for (const auto& item : signatures) {
                if (pos::experimental::GetMLDSA44KeyId(item.public_key) == policy.keys[item.index] &&
                    checker.CheckPQSig(item.signature, item.public_key, scriptPubKey)) merged.emplace(item.index, item);
            }
        }
        if (!have_policy) return {};
        std::vector<PQMultisigSignature> signatures;
        for (const auto& item : merged) {
            if (signatures.size() == policy.required) break;
            signatures.push_back(item.second);
        }
        return SignatureData(GetPQMultisigScriptSig(policy, signatures));
    }
    if (ExtractPQKeyId(scriptPubKey, pq_id)) {
        for (const auto* candidate : {&scriptSig1, &scriptSig2}) {
            if (VerifyScript(candidate->scriptSig, scriptPubKey, &candidate->scriptWitness,
                STANDARD_SCRIPT_VERIFY_FLAGS | SCRIPT_VERIFY_PQ, checker)) return *candidate;
        }
        return {};
    }
    txnouttype txType;
    std::vector<std::vector<unsigned char> > vSolutions;
    SolverForSigning(scriptPubKey, txType, vSolutions);

    return CombineSignatures(scriptPubKey, checker, txType, vSolutions, Stacks(scriptSig1), Stacks(scriptSig2), SigVersion::BASE).Output();
}

namespace {
/** Dummy signature checker which accepts all signatures. */
class DummySignatureChecker final : public BaseSignatureChecker
{
public:
    DummySignatureChecker() {}
    bool CheckSig(const std::vector<unsigned char>& scriptSig, const std::vector<unsigned char>& vchPubKey, const CScript& scriptCode, SigVersion sigversion) const override { return true; }
    bool CheckPQSig(const std::vector<unsigned char>&, const std::vector<unsigned char>&, const CScript&) const override { return true; }
};
const DummySignatureChecker DUMMY_CHECKER;

class DummySignatureCreator final : public BaseSignatureCreator {
public:
    bool CreatePQSig(const SigningProvider&, std::vector<unsigned char>& signature,
        const uint256&, const CScript&) const override
    {
        signature.assign(PQ_SIGNATURE_BYTES + 1, 0);
        signature.back() = SIGHASH_ALL;
        return true;
    }
    DummySignatureCreator() {}
    const BaseSignatureChecker& Checker() const override { return DUMMY_CHECKER; }
    bool CreateSig(const SigningProvider& provider, std::vector<unsigned char>& vchSig, const CKeyID& keyid, const CScript& scriptCode, SigVersion sigversion) const override
    {
        // Create a dummy signature that is a valid DER-encoding
        vchSig.assign(72, '\000');
        vchSig[0] = 0x30;
        vchSig[1] = 69;
        vchSig[2] = 0x02;
        vchSig[3] = 33;
        vchSig[4] = 0x01;
        vchSig[4 + 33] = 0x02;
        vchSig[5 + 33] = 32;
        vchSig[6 + 33] = 0x01;
        vchSig[6 + 33 + 32] = SIGHASH_ALL;
        return true;
    }
};
}

const BaseSignatureCreator& DUMMY_SIGNATURE_CREATOR = DummySignatureCreator();

bool IsSolvable(const SigningProvider& provider, const CScript& script)
{
    uint256 policy_id;
    if (ExtractPQMultisigId(script, policy_id)) {
        std::vector<unsigned char> encoded;
        PQMultisigPolicy policy;
        return provider.GetPQMultisigPolicy(policy_id, encoded) &&
            DecodePQMultisigPolicy(encoded, policy) && GetPQMultisigPolicyId(policy) == policy_id;
    }
    // This check is to make sure that the script we created can actually be solved for and signed by us
    // if we were to have the private keys. This is just to make sure that the script is valid and that,
    // if found in a transaction, we would still accept and relay that transaction.
    SignatureData sigs;
    if (ProduceSignature(provider, DUMMY_SIGNATURE_CREATOR, script, sigs)) {
        // VerifyScript check is just defensive, and should never fail.
        assert(VerifyScript(sigs.scriptSig, script, &sigs.scriptWitness, STANDARD_SCRIPT_VERIFY_FLAGS | SCRIPT_VERIFY_PQ, DUMMY_CHECKER));
        return true;
    }
    return false;
}
