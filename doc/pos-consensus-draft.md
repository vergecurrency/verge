<!-- Copyright (c) 2026 The Verge Core developers -->

# Verge PoS-Only Consensus Draft

Status: Public testnet validation on the `regtest` branch. This document defines the PoS consensus rules intended for mainnet activation after successful testnet validation, independent review, release-readiness approval, and confirmation of the production activation height.

## Scope

Verge remains proof of work before a network-specific activation height. Beginning with the block at `nPoSActivationHeight`, proof-of-work blocks are invalid and every new block must satisfy the proof-of-stake rules below. The implementation is enabled on regtest and testnet for validation before the same documented transition is activated on mainnet at the configured production height.

This transition does not mint new XVG. A valid PoS block may claim at most the transaction fees in that block. An empty block therefore pays zero.

## Confirmed Parameters

| Parameter | Initial value |
| --- | ---: |
| Target slot duration | 30 seconds |
| Minimum staking output | 1,000 XVG |
| Stake maturity | 720 confirmations |
| PoS subsidy | 0 XVG |
| Producer reward | Current block transaction fees only |
| Future-time tolerance | No future canonical slot |
| Staking authorization | Delegated staking keys |
| Principal maturity after staking | Preserved |
| Fee reward staking eligibility | 720 confirmations |
| Expected eligible producers | 1 per slot |
| Bonding | Automatically created after staking opt-in |
| Delegation granularity | Entire bonded UTXO |
| Unbonding lock | 20,160 accepted blocks |
| Initial epoch seed | Tagged hash of network ID, activation height, genesis hash, and post-snapshot PoW entropy |
| Post-activation PoW fallback | None |
| Mainnet activation height | 15,000,000 (provisional, not approved for release) |
| Mainnet bond-rules height | Derived from activation minus unbonding and stake maturity; currently 14,979,120 |
| Testnet activation height | 3,000 |
| Testnet bond-rules height | 1 |
| Regtest activation | Disabled unless -posactivationheight=<height> is set |

All amounts are evaluated in base units. A staking output exactly equal to 1,000 XVG is eligible.

## Activation

Each network has an explicit `nPoSActivationHeight` and an earlier or equal
`nPoSBondRulesHeight`. Reserved bond and unbond scripts are interpreted by
consensus only at and after the bond-rules height, which provides a deliberate
upgrade boundary instead of changing the meaning of scripts for legacy miners
immediately. The clean-chain public testnet trial activates bond rules at
height 1 and PoS at height 3,000. Mainnet schedules bond rules at height
14,979,120 and PoS at height 15,000,000 in the current configuration. The owner
will choose a different production activation height after testing, targeting
2027; no replacement height is approved yet. These provisional values are real
consensus settings, not a disabled mainnet deployment. Do not ship this branch
as the mainnet activation release until the chosen height and phase gates are
approved. The mainnet bond-rules height is derived from the activation height
minus the configured unbonding window and stake maturity, so changing the
activation constant keeps the pre-bonding window aligned.

For this clean testnet trial, the initial stake snapshot is recorded at height
2,760. A bond must be created by height 2,280 to have 720 confirmations at
activation and must remain unspent through the snapshot. Block 2,999 is the
last valid PoW block; block 3,000 must be produced by an eligible PoS bond.
The November 22 final clean-chain testnet trial uses P2P
message-start bytes `52 4c 7f 07`,
derived from the first four bytes of
`SHA256("Verge PoS Testnet Reset 2026-11-22 final trial height 3000")`. This separates
it from the previous trial's magic `69 c1 da 8e` and earlier testnet identities,
so an old testnet node cannot exchange P2P messages with the clean network.
The testnet genesis block and height-3,000 PoS activation are unchanged.
Changing magic does not reset stored history: every participant in this trial
must start with fresh testnet blockchain data, keeping mainnet data and wallet
backups separate and intact.
Every seed and
participant still must upgrade because old binaries cannot join the new network.
Operators should use a fresh testnet wallet or, after making a verified backup,
start once with `-zapwallettxes=2` so transactions from the discarded chain are
removed before rescanning the reset chain. Automatic bonding treats an
unconfirmed bond as pending only while that transaction is in the mempool.
Minimum-chain-work, assume-valid, and chain transaction estimates are reset to
zero for this new chain. Stable anchors must be selected from this reset before
those bootstrap values are populated again.

Regtest activation is disabled by default and requires `-posactivationheight=<height>`. The override is rejected on testnet and mainnet. Automated tests use small explicit heights, while manual regtest networks may choose an upcoming height.

## Deployment Portability

The same PoS consensus code path must be used on regtest, public testnet, and mainnet. Network classes provide parameters such as activation height, slot duration, maturity, minimum stake, target limits, checkpoint intervals, and deployment state; they do not select separate consensus algorithms.

Regtest may expose deterministic test controls and an activation-height override, but those controls must be unavailable on public networks. Promotion follows regtest to public testnet to mainnet by changing reviewed network parameters, not by copying or rewriting the implementation. Mainnet release approval and the final activation height remain gated on Phase 6; the currently configured height is not a disabled deployment.

Database records, block serialization, wallet staking records, RPC schemas, and P2P messages must be forward-compatible across those environments so testnet exercises the actual production format. A node must log the configured activation height and reject ambiguous or conflicting activation settings at startup.

Wallet staking is explicitly opt-in and disabled by default. `setstaking true`
persists the preference and automatically bonds all mature spendable funds above
the wallet's saved reserve balance. `setstaking true 10000` keeps 10,000 XVG
liquid; omitting the reserve uses the saved value, which defaults to zero.
Automatic bonding splits fragmented balances into size-limited transactions,
selecting at most 100 inputs per batch and budgeting their maximum signed size.
The saved reserve applies to the total mature unbonded balance across batches.
A pending bond pauses additional batches until it confirms. Newly created bonds
still require stake maturity and the delayed snapshot before production.
The daemon equivalents are `staking=1` and `stakingreservebalance=10000`. Locking an
encrypted wallet pauses bonding and signing without changing the preference.
The Qt wallet exposes the same per-wallet policy under Wallet options as
automatic staking and the amount to keep available.
Automatic bonding generates a wallet-encrypted delegated secp256k1 key,
deterministically derives the separate P-256 VRF key under
`VergePoS/VRFKey/v1`, and creates wallet-controlled reward and withdrawal
destinations. The lower-level `createbond` RPC remains available for explicit
delegation. `unbond` preserves the complete principal in the consensus unbond
script and funds its transaction fee from separate wallet inputs.
At activation:

1. The block at height `nPoSActivationHeight - 1` is the final PoW block.
2. The block at `nPoSActivationHeight` must be PoS.
3. Bond and unbond consensus rules activate at `nPoSBondRulesHeight`. Wallets refuse to create either transaction before that height. This permits a reviewed preparation interval while PoW still secures the chain without silently reinterpreting old scripts before the upgrade boundary.
4. A bond must satisfy the 720-confirmation maturity rule and appear in the stake snapshot selected two epochs before its first eligible PoS epoch. Merely bonding immediately before activation does not bypass either delay.
5. The initial epoch seed is a domain-separated SHA-256 commitment to the network identifier, activation height, network genesis hash, and an ordered accumulator of every accepted PoW block hash after the initial stake snapshot through the final PoW block. The validator set is therefore fixed before the seed is known. A final PoW producer can withhold its own candidate block, but cannot select new bond keys or outpoints after learning the completed seed without reorganizing the snapshot.
6. Existing unbonded UTXOs remain spendable but are not eligible stake until explicitly bonded.
7. PoW difficulty and algorithm-frequency rules no longer determine validity after activation.
8. No emergency PoW fallback exists after activation. If no valid PoS producer is available, the chain pauses until an eligible producer returns.
9. Old nodes remain on incompatible PoW consensus. This is intentionally a hard fork.

### Post-Fork Transaction Replay Protection

Explicit two-way transaction replay protection is a mainnet release
requirement. PoS block-version enforcement prevents legacy PoW blocks from
entering the upgraded chain, but it does not by itself prevent an ordinary
transaction spending a shared pre-fork UTXO from being confirmed independently
on both chains.

Beginning with the activation block, every transaction, including the
coinbase-like fee reward, must use PoS transaction format 5, identified by the
reserved `nVersion` value `0x05475658`. Its wire encoding is
`nVersion || nTime || marker || vin || vout || nLockTime`, where the marker is
the uint32 value `0x47565800`. The version serializes as bytes `58 56 47 05`
(`XVG\x05`) and the marker as `00 58 56 47` (`\0XVG`). The marker's first byte
causes a legacy parser to decode an empty input vector and reject the transaction.
Upgraded nodes reject the reserved version before activation,
reject every other transaction version after activation, and reject protected
encodings whose marker is absent or non-canonical. This envelope protects
signatureless and nonstandard scripts as well as ordinary signed spends.

#### Transaction And Signer Integration Specification

"Format 5" names this revision of the protected transaction format; it does
not mean that `nVersion` is the integer 5. Implementations must use the exact
reserved value and field order below. All fixed-width integer fields use
little-endian encoding, and vectors use the existing canonical CompactSize
encoding.

| Field | Encoding |
| --- | --- |
| `nVersion` | signed 32-bit `0x05475658`; bytes `58 56 47 05` |
| `nTime` | unsigned 32-bit little-endian |
| replay marker | unsigned 32-bit `0x47565800`; bytes `00 58 56 47` |
| `vin` | CompactSize count followed by unchanged transaction inputs |
| `vout` | CompactSize count followed by unchanged transaction outputs |
| `nLockTime` | unsigned 32-bit little-endian |

The transaction ID remains the double-SHA-256 hash of this complete canonical
wire serialization, including the replay marker. Address encodings, input and
output structures, script execution, and ordinary transfer public keys remain
unchanged. Ordinary `CHECKSIG` and `CHECKMULTISIG` spends continue to use
strict-DER secp256k1 ECDSA signatures followed by a one-byte hash type.

The post-fork hash type must set `SIGHASH_POS_FORKID = 0x40`. Consequently the
standard values are `0x41` for `SIGHASH_ALL`, `0x42` for `SIGHASH_NONE`, and
`0x43` for `SIGHASH_SINGLE`; `SIGHASH_ANYONECANPAY = 0x80` may be ORed with
any of them. The complete hash type, including `0x40` and optionally `0x80`,
is used by the existing signature serializer and is serialized as a signed
32-bit little-endian value at the end of its preimage.

For non-witness (`SigVersion::BASE`) inputs, calculate the digest as follows:

```text
hash_type      = base_hash_type | 0x40 [| 0x80]
legacy_preimage = LegacySignatureSerialize(
    nVersion, nTime, vin, vout, nLockTime,
    scriptCode, input_index, hash_type
) || int32_le(hash_type)
legacy_hash    = SHA256d(legacy_preimage)

domain_preimage =
    ASCII("VergePoS/TxSignature/v1") ||
    uint32_le(network_id) ||
    int32_le(activation_height) ||
    uint256_wire(legacy_hash)

pos_signature_hash = SHA256d(domain_preimage)
```

`LegacySignatureSerialize` is the existing Verge BASE signature algorithm,
including its established `SIGHASH_NONE`, `SIGHASH_SINGLE`,
`SIGHASH_ANYONECANPAY`, script replacement, and sequence handling. It commits
to the protected `nVersion` but does not insert the replay marker between
`nTime` and `vin`. The marker is a fixed invariant implied by the reserved
version and is independently required by transaction deserialization and
consensus. `uint256_wire` means the 32 raw bytes used by Verge serialization,
which are the reverse of the conventional big-endian hash hex displayed by
RPCs. The ASCII domain tag has no terminating NUL byte.

Network identifiers are `1` for mainnet, `2` for testnet, and `3` for regtest.
An external signer must obtain the final activation height from the applicable
release's chain parameters; signatures are intentionally invalid on a network
or activation schedule other than the one to which they commit.

The fixed transaction signature-hash vector in
`src/test/pos_crypto_tests.cpp` uses these inputs:

| Parameter | Value |
| --- | --- |
| Network ID | `1` |
| Activation height | `15,000,000` |
| Private key used to derive `scriptCode` | scalar `1`, compressed public key |
| `scriptCode` | `76a914751e76e8199196d454941c45d1b3a323f1433bd688ac` |
| Spent amount | `25,000,000` base units (`25 XVG`) |
| Transaction version | `0x05475658` |
| Transaction time | `1,700,000,000` |
| Input | transaction ID `01`, output `0`, sequence `0xffffffff` |
| Output | `24,990,000` base units to `OP_TRUE` |
| Lock time | `0` |
| Input index | `0` |
| Hash type | `0x41` (`SIGHASH_ALL | SIGHASH_POS_FORKID`) |
| Expected displayed signature hash | `f01c4c99170e98487365ecf490cdc61bd33840bed7ba6f44c2297bad31f4c289` |

The corresponding unsigned transaction and intermediate hash vectors are:

```text
unsigned transaction wire serialization:
5856470500f15365005856470101000000000000000000000000000000000000000000000000000000000000000000000000ffffffff0130517d0100000000015100000000

legacy signature preimage:
5856470500f15365010100000000000000000000000000000000000000000000000000000000000000000000001976a914751e76e8199196d454941c45d1b3a323f1433bd688acffffffff0130517d010000000001510000000041000000

legacy hash (displayed big-endian):
815bf116067f4ae405eb21049e51d4a50f055b838c4dc3101da00c50dca86fe1

domain preimage (contains legacy hash in uint256 wire order):
5665726765506f532f54785369676e61747572652f763101000000c0e1e400e16fa8dc500ca01d10c34d8c835b050fa5d4519e0421eb05e44a7f0616f15b81

final signature hash (displayed big-endian):
f01c4c99170e98487365ecf490cdc61bd33840bed7ba6f44c2297bad31f4c289
```

This vector is consensus test data and must be reproduced by hardware wallets,
offline signers, and exchange signing systems before mainnet activation.

Every signature evaluated after activation must additionally set hash-type bit
`0x40` and use the versioned Verge PoS signature-hash domain. The legacy
signature hash is wrapped as
`SHA256d("VergePoS/TxSignature/v1" || network_id || activation_height || legacy_hash)`,
using canonical serialization for the numeric fields. Legacy transaction
signatures are invalid in post-activation blocks, while the format 5 envelope
makes post-activation transactions invalid under legacy PoW validation.
Before activation, historical transaction encoding and signature semantics
remain unchanged. Existing pre-activation UTXOs remain spendable after
activation without first moving balances.

The wallet and all signing RPCs must select the correct signature domain from
the height of the block for which the transaction is being prepared. At the
activation boundary, nodes must revalidate the mempool and remove transactions
whose signatures are valid only under the pre-activation rules. User-facing
RPC results must distinguish a transaction that needs to be recreated from an
ordinary policy rejection. Hardware wallets, offline signing, multisig, P2SH,
bonding, unbonding, fee funding, and every other spend path must implement the
same rule; no legacy-authorized transaction class may bypass the domain check.

The exact signature-hash marker, domain tag, serialized preimage, and network
commitment must remain covered by fixed cross-platform vectors before mainnet
parameters are selected. Tests must prove both directions: a legacy
PoW node rejects a post-activation transaction, and an upgraded node rejects a
legacy-signed transaction in a post-activation block. Mainnet activation is
blocked until this protection has passed functional fork-boundary tests and an
independent consensus review. This requirement may be waived only if the
project and every material exchange explicitly treat the legacy PoW branch as
valueless and unsupported; the default release assumption is that it is not
waived.

## Block Representation

A dedicated version bit identifies PoS. A PoS header appends a 32-byte `hashPoSData` commitment to the canonical PoS extension encoding described below; historical PoW headers retain their original serialization. After activation, the five PoW algorithm version encodings are invalid for new blocks.

For `hashPoSData`, serialize the extension in its normal wire order, replacing
only the producer authorization's 64-byte `signature` array with 64 zero bytes,
and apply the existing `POS_DATA` tagged hash. All other fields, including the
stake proof, votes and evidence with their signatures, remain committed exactly
as transmitted. The wire format still contains the full producer signature.
Two valid Schnorr signatures for the same authorization therefore identify one
candidate, rather than creating different block IDs through signing randomness.
The signature is verified before admission and before any header-invalidity
decision. A missing or invalid signature is a corrupt candidate body, not proof
that this block ID is permanently invalid. Authorization field changes still
change the ID, and conflicting candidate headers remain evidence-eligible.
This change applies to the fresh testnet and the future mainnet PoS activation;
it is not compatible with PoS history from the previous trial. Transaction
formats, transaction signature hashes and historical PoW block IDs are unchanged.
Historical blocks through activation height minus one retain their original header interpretation and proof-of-work validation. At and after activation, the PoS version bit is mandatory, proof-of-work blocks are invalid, and both nBits and nNonce must be zero. The in-memory null-header sentinel uses nVersion equal to zero so a valid PoS header with zero nBits is not mistaken for an empty object. PoS timestamps must be exact 30-second slot boundaries, must map to a slot strictly later than the parent block's slot, and cannot exceed the canonical slot containing network-adjusted time; skipped slots remain valid.

The first transaction remains structurally coinbase-like so existing block indexing can be adapted incrementally. It creates no subsidy and may pay at most the block's transaction fees to the owner-controlled reward destination committed by the selected bond.

A consensus-defined stake proof references exactly one bonded UTXO without spending or recreating it. The proof commits to the selected outpoint, slot, stake snapshot, VRF output and proof, delegated staking public key; block authorization and checkpoint votes are serialized separately. Principal therefore remains locked in its original bonded output while producing blocks.

The exact serialization must be domain-separated and covered by fixed test vectors before Phase 2 is considered complete. Witness serialization is not used.

### Header Relay Encoding

PoS `headers` messages contain the canonical extended header, an empty
transaction vector, and an empty legacy block-signature vector. They never
contain the PoS extension itself. The 32-byte extension commitment makes any
change to the stake proof, authorization signature, votes, or evidence produce
a different block ID.

A PoS header is only a locator for its full block and cannot become a
fork-choice candidate by itself. Header relay stops after one PoS header. A
node requests and fully validates that block before accepting a PoS descendant
header, preventing unauthenticated header chains from poisoning best-header
state or consuming unbounded index memory.

The same rule applies to compact-block announcements: a PoS compact header is
treated only as a locator and the full block is requested. A peer that announces
but withholds that full block is disconnected after 60 seconds so another peer
can supply it.

An otherwise well-formed announcement for the next canonical slot can arrive
slightly before the receiver's adjusted clock reaches that slot. This is not
peer misconduct. Each peer may have at most one such header deferred for up to
60 seconds; duplicate or replacement announcements do not extend that deadline.
When the slot arrives, normal header checks run again and the full block is
requested once. Announcements farther than one slot ahead are ignored, not
queued. Deferred headers never enter the block index or fork-choice state,
and actual blocks still cannot be accepted ahead of the current canonical
slot. Duplicate announcements for an outstanding block request neither resend
that request nor reset its 60-second withholding timeout.

### PoS Block Extension Limits

PoS uses version bit `1 << 15`, which is distinct from the five PoW algorithm bits. The conditional PoS block extension follows the existing transaction vector and block-signature field and contains canonical stake-proof, checkpoint-vote, and equivocation-evidence objects.

| Field | Consensus limit |
| --- | ---: |
| RFC 9381 P-256 VRF proof | exactly 81 bytes |
| VRF output | exactly 32 bytes |
| BIP340 block or vote signature | exactly 64 bytes |
| Checkpoint votes per block | 1,024 maximum |
| Equivocation evidence records per block | 16 maximum |

All extension bytes count toward the existing serialized block-size and weight limits. Counts are decoded with existing canonical compact-size rules but are rejected against their PoS-specific maxima before allocation. Vote entries and evidence records are sorted canonically and unique.
The version 1 extension serializes fields in this exact order:

| Field | Encoding |
| --- | --- |
| extension version | fixed 1 byte |
| stake proof | fixed 295 bytes |
| block authorization | fixed 273 bytes |
| checkpoint votes | canonical compact-size count, then 229-byte entries |
| block equivocation evidence | canonical compact-size count, then 547-byte entries |
| vote equivocation evidence | canonical compact-size count, then 459-byte entries |

The minimum extension is 572 bytes, including three zero-valued one-byte collection counts. The decoder rejects a vote count above 1,024 immediately after reading that count. It rejects a block-evidence count above 16 immediately, then limits vote evidence to 16 minus the block-evidence count. No collection is reserved or populated until its count passes the applicable bound. Locally constructed extensions repeat the same non-overflowing limits during structural validation.

The coinbase-like transaction commits to tagged roots for the stake proof, vote vector, evidence vector, stake snapshot, and epoch seed. The header commits to the canonical extension with only the producer authorization signature normalized to zero. The actual signature remains mandatory and is independently verified; changing its encoding cannot change the block ID or win a fork-choice tie. Vote and evidence signatures remain committed. The extension is invalid if either commitment differs. Full-block validation also requires the authorization network identifier (mainnet 1, testnet 2, regtest 3), parent root, canonical global slot, candidate signing-header hash, bond outpoint, tagged stake-proof hash, and fee-reward transaction hash to match the block and stake proof exactly.

## Stake Eligibility

An outpoint is eligible for a candidate block when all of the following are true at the parent block:

- it exists and is unspent;
- its value is at least 1,000 XVG;
- its creating transaction is at least 720 blocks deep;
- its script type is supported by the staking verifier;
- it is not subject to an active staking lock or reward-maturity rule; and
- its ownership proof is valid for the candidate block and slot.

Selection weight is linear in value. Each eligible base unit has equal probability. Splitting or combining outputs therefore does not change expected aggregate selection weight, apart from integer rounding and the minimum-output rule. Coin age beyond 720 confirmations adds no weight.

Bonding is an explicit on-chain transaction that the wallet creates automatically after staking is enabled. A bonded output delegates its entire value to one staking key; users who want separate delegations may use `createbond` to split funds before bonding. Block production never spends the bonded principal. Beginning unbonding disables new eligibility and locks withdrawal for 20,160 blocks.
### Bond Output Encoding

A version 1 bond output uses a 117-byte script. It pushes an exact 89-byte metadata payload, drops that payload, and then executes an ordinary key-hash signature check for the withdrawal credential:

~~~text
PUSHDATA(89) || "VPB" || 0x01 ||
xonly_signing_key[32] ||
compressed_vrf_key[33] ||
reward_key_id[20] ||
OP_DROP ||
OP_DUP OP_HASH160 withdrawal_key_id[20] OP_EQUALVERIFY OP_CHECKSIG
~~~

The parser requires the exact field sizes, a fully parseable BIP340 secp256k1 x-only signing public key, a fully parseable compressed P-256 VRF public key, nonzero reward and withdrawal key identifiers, minimal push encoding produced by the script builder, and no trailing opcodes. Once bond rules activate, any output beginning with the reserved bond or unbond magic is rejected if the complete payload is malformed, and every transaction including coinbase-like transactions must satisfy the minimum bond value. The script remains cryptographically spendable by the withdrawal key, but consensus rejects spending a live bond except through the explicit equal-principal unbond transition. Script validity alone never authorizes an ordinary bond spend.

## Slots And Timestamps

Time is divided into 30-second slots. A block commits to one slot derived from its timestamp. Nodes calculate slots using consensus arithmetic, not local scheduling state.

A valid block must:

- use a slot strictly later than its parent slot;
- use the canonical timestamp for that slot;
- not exceed the permitted future-slot tolerance; and
- satisfy transaction timestamp rules.

Empty slots are allowed. The next eligible producer may build on the latest valid block using a later slot; no empty placeholder blocks are inserted. This prevents a single offline winner from halting the chain.

A node accepts no candidate from a canonical slot later than the slot containing
network-adjusted time. A future-slot block is not accepted early or retained as
valid pending work.

## Slot And Epoch Numbering

For the final PoW block, define:

```text
activation_slot = floor(final_pow_block.nTime / 30) + 1
```

A post-activation candidate timestamp must be divisible by 30 and defines:

```text
global_slot   = candidate.nTime / 30
relative_slot = global_slot - activation_slot
epoch         = relative_slot / 120
slot_in_epoch = relative_slot % 120
```

`relative_slot` must be non-negative and strictly greater than the parent PoS block's relative slot. The activation block may use `activation_slot` or any later permissible slot. Empty slots advance time and epoch numbering but do not create blocks.

The initial stake snapshot is taken from chain state at height `nPoSActivationHeight - 240`. A bond in that snapshot is initially eligible only if it also has 720 confirmations at activation. Accepted PoW block hashes from the next height through `nPoSActivationHeight - 1` are accumulated in chain order and committed into the initial epoch seed. Subsequent epoch `e` uses the canonical bonded-stake snapshot from the end of epoch `e - 2`. Epochs 0 and 1 use the initial snapshot. Snapshot membership, values, credentials, and total eligible bonded value are committed by a canonical Merkle root.
## Randomness And Fork Choice

The consensus design uses:

1. Use the RFC 9381 `ECVRF-P256-SHA256-SSWU` ciphersuite for private slot eligibility and publicly verifiable proofs. P-256 support is already available through the required OpenSSL dependency, but the ECVRF protocol wrapper remains new consensus code and must pass RFC vectors and independent audit.
2. Give each bonded delegation a separate VRF public key. The online delegated credential produces VRF proofs; the unrestricted XVG spending key is not used for slot evaluation.
3. Snapshot bonded stake two epochs before it is used for leader selection and voting, and commit the canonical snapshot root in every applicable PoS block. This prevents a producer from moving stake after seeing the randomness for the target epoch.
4. Derive each later epoch seed as a tagged hash of the previous seed, next epoch number, required delayed stake-snapshot root, and an ordered accumulator of the completed epoch's accepted VRF outputs. The delayed snapshot fixes eligible keys before that entropy is revealed. Signatures, transaction ordering, reward destinations, and arbitrary nonce fields are excluded. A selected producer can still withhold an eligible block, but cannot substitute arbitrary entropy or register a favorable key after seeing the completed seed.
5. Use deterministic, stake-weighted tree descent from the latest finalized checkpoint for temporary fork choice. Each competing branch is first replayed against its own UTXO and PoS state, and each bonded delegation has one monotonic latest vote weighted by that branch's delayed snapshot. The tree projects the maximum authenticated branch-view support onto each child edge, rather than adding incompatible histories together. Finalized history is never reconsidered by ordinary fork choice.

This design does not treat a Schnorr or ECDSA signature as random output. Signatures authorize blocks and votes; VRF proofs determine private slot eligibility. Users do not manually operate either mechanism.

Primary references:

- RFC 9381, Verifiable Random Functions: https://www.rfc-editor.org/rfc/rfc9381.html
## Stake Lottery

Each eligible bonded delegation evaluates one independently testable kernel per slot:

```text
vrf_input = H("verge-pos-slot-v1" || network_id || epoch_seed || slot || stake_outpoint)
(vrf_output, vrf_proof) = ECVRF_prove(delegated_vrf_key, vrf_input)
threshold = floor(MAX_UINT256 * bonded_value / total_eligible_bonded_value)
winner if uint256(vrf_output) <= threshold
```

The threshold calculation uses overflow-safe wide arithmetic and the two-epoch-delayed stake snapshot. The sum of all eligibility probabilities is approximately one, so the expected number of eligible producers is one per slot. Empty and multiply-won slots are normal; temporary competing branches are resolved by the approved fork-choice rule. No PoS difficulty retarget is required.

The kernel must not include mutable transaction ordering, reward destination, signature bytes, or arbitrary nonce fields. This limits grinding by a candidate producer. `parent_randomness` is the deterministic epoch seed derived from the previous seed, epoch number, delayed snapshot root, and the accepted VRF-output accumulator for the completed epoch. Only outputs committed by accepted blocks contribute.

Slot eligibility uses RFC 9381 `ECVRF-P256-SHA256-SSWU`. The VRF proof and output are consensus data. The pseudocode hash denotes the verified VRF output mapped to an unsigned 256-bit integer.

## Authorization And Key Separation

A block must contain a signature from the delegated block/vote signing key committed by the selected bond. The signed message is domain-separated and commits to:

- network identifier;
- parent block hash;
- candidate header hash;
- slot;
- staking outpoint;
- canonical stake-proof hash;
- fee-reward transaction hash; and
- parent randomness.

The design requires a restricted staking key delegated by the spending key. A staking key may authorize block production and checkpoint voting but must not authorize principal withdrawal or arbitrary payments. The bonded output is consensus-locked until a valid unbonding transition completes.

Delegated staking authorization is mandatory. Each bond commits to three distinct credentials:

- an owner withdrawal credential that controls principal after unbonding;
- a delegated secp256k1 signing public key restricted to block authorization and checkpoint voting; and
- a delegated P-256 VRF public key restricted to slot eligibility proofs.

The bond also commits to an owner-controlled reward destination. Fee rewards may only be paid to that destination; neither delegated online key can redirect principal or rewards. Direct use of the unrestricted withdrawal key for online staking is not supported.

Changing the delegated signing key, VRF key, or reward destination requires completing unbonding and creating a new bond. In-place rotation is forbidden so stake snapshots identify one immutable credential set. The bond format, authorization domains, and revocation behavior are consensus-critical and require fixed vectors.

## Post-Quantum Work (2026-11-22)

This section tracks the requested experimental implementation for the next
fresh testnet and regtest, not an activated mainnet upgrade. Existing classical
PoS signatures, transaction signatures and bond withdrawals remain
quantum-vulnerable. Adding ML-DSA signing alone does not make Verge
quantum-resistant. Public eligibility is approved and implemented for both
trial networks; mainnet still uses its existing provisional parameters.
Mandatory hybrid authentication and PQ spending are now integrated for these
fresh trial networks. The end-to-end verification checklist below must pass
before launching the trial; this is not a mainnet-readiness certification.

### Completed Foundation

- `src/pos/pqcrypto.{h,cpp}` provides experimental FIPS 204 ML-DSA-44 key
  generation, seed import, pure signing and verification through OpenSSL's
  existing EVP implementation. No new cryptographic algorithm is implemented
  locally. Backends older than OpenSSL 3.5 or built without ML-DSA fail closed;
  they cannot substitute a classical signature for a missing PQ signature.
- Private 32-byte seeds use the wallet's cleansing/locked-pool allocator and
  are erased on reset/destruction. Backend-expanded private material is
  transient and owned/freed by OpenSSL, not guaranteed locked by this wrapper.
  PQ seeds must come from independent entropy, not an exposed Schnorr scalar
  or a public VRF credential. Encrypted wallet persistence and full-wallet
  backup support are implemented below. Secret-export RPCs are not provided.
- The experimental hybrid verifier requires **both** Schnorr and ML-DSA-44.
  Both sign the same key-bound transcript digest. ML-DSA uses its standard
  pure-message encoding and the context
  `Verge/PoS/Hybrid-Schnorr-MLDSA44/v1`.
- The transcript digest is double SHA-256 of the existing `CHashWriter`
  serialization: CompactSize-prefixed ASCII context string, little-endian
  `uint32` network ID, purpose byte (`1` block or `2` vote), raw 32-byte
  Schnorr public key, raw 1,312-byte ML-DSA-44 public key, and raw 32-byte
  message digest. Network ID zero and unknown purposes are rejected. This
  transcript is mandatory for version-2 trial block/vote authentication.
- Raw ML-DSA helpers require exact 1,312-byte public keys and 2,420-byte
  signatures, at most 255 context bytes and at most 65,536 message bytes.
  Hybrid messages are fixed 32-byte transcript digests. The exact wire bounds
  and versioned consensus objects are specified below.
- Regression fixtures use NIST ACVP FIPS 204 key-generation and external
  pure-verification cases pinned in `test/data/mldsa44.json`. Additional tests
  cover both required signatures, key/message/network/purpose binding,
  malformed lengths, context separation, failed-output clearing, concurrent
  signing and unavailable-backend rejection. Execution results are recorded
  below after verification, not inferred from source inspection.

### Approved Public Eligibility (2026-11-23)

The owner approved public eligibility and requires the integrated trial on
both public testnet and regtest, not only an opt-in private regtest. The public
testnet has not launched; no live v1 trial must be preserved. Mainnet's
provisional activation heights and its existing eligibility scheme remain
unchanged until the complete upgrade is reviewed and explicitly scheduled.

`fPoSPublicEligibility` is enabled for testnet and regtest. Public stake proofs
use version `2` and retain the fixed proof layout during integration: the
33-byte VRF key and 81-byte VRF proof are zero; the previous 32-byte output
field carries the public selection hash. Private v1 proofs are rejected on
these networks; public v2 proofs are rejected by the legacy/mainnet scheme.
Version-2 hybrid authentication is required alongside public eligibility on
these trial networks; mainnet's existing scheme remains unchanged.

There is one weighted winner per canonical slot. Entries are ordered by their
outpoints, have strictly positive values, and their checked sum must equal the
authenticated delayed snapshot's total, bounded by `MAX_MONEY`. A bond's
interval length equals its atomic-unit value; splitting a balance does not
increase its total probability mass. Offline or penalty-locked winners leave
an empty slot. Unbonding removes a bond from future snapshots, not frozen
snapshots already in use; the withdrawal lock covers this transition. There
is no node-local fallback producer.

The selector takes an already authenticated snapshot; it does not trust a
snapshot or root supplied by a peer. For draw counter `0..31`, compute a
tagged SHA-256 hash using `VergePoS/PublicEligibility/v1` over little-endian
network ID (`uint32`), snapshot source epoch (`uint64`), raw snapshot root
(`uint256`), raw authenticated epoch seed (`uint256`), global slot (`uint64`)
and draw counter (`uint32`). Interpret the result as a little-endian unsigned
256-bit integer `x`. With total atomic value `W`, reject `x < 2^256 mod W`;
otherwise use ticket `x mod W` and select its cumulative-value interval. The
first accepted draw is mandatory. Exhausting all 32 draws fails closed; the
counter is not a producer-selectable nonce. Producers and branch validation
use the same selector and still enforce bond authorization, maturity, locks,
snapshot ancestry and the expected parent-state epoch seed independently.

The fixed selector vector uses network `2`, source epoch `7`, snapshot root
`0xabcd`, seed `0xfeed`, global slot `500` and weights `[1,2,7] * COIN`.
The accepted hash in `uint256::GetHex()` notation is
`bd951cf22db16bf3cdcce77477ced070b548e65bd738459a8492c721fc8e053b`;
ticket `6,132,027` chooses the third bond. This vector fixes byte order and
arithmetic; its root is an authenticated-input fixture, not a real snapshot.

Producer predictability, targeted denial of service, entropy withholding,
initial-PoW seed grinding, validator-set changes and partitions remain review
gates. Public selection removes the elliptic-curve eligibility dependency;
it does not itself establish the security of the complete PQ consensus.

### Integration Decisions

The requested scope is end-to-end quantum-resistant staking, not merely adding
an optional second producer signature. Integration review confirms that the
existing `BondData` has classical withdrawal credentials, `BondRecord`
serialization commits the existing credential layout into snapshots, and
`BlockExtension` carries only v1 authorization/vote/evidence objects. Replacing
signature calls without changing these contracts is not a complete upgrade.

The eligibility alternatives considered were:

- Preserve private eligibility and select an independently reviewed PQ VRF.
  The current OpenSSL ML-DSA primitive is not a VRF, and this review has not
  identified an audited, production-ready replacement to adopt directly.
- Adopt public hash-based stake selection with delayed epoch randomness.
  This removes the elliptic-curve VRF dependency, but future eligible producers
  become predictable, changing the targeted-denial-of-service threat model.
  Exact weighted sampling, snapshot freezing, entropy withholding/grinding
  bounds and partition behavior need a separate specification and tests.
  Hashing randomized signatures, trying extra nonces, or letting the producer
  select a favorable PQ public key must not create extra lottery attempts.

Public proposer selection is a known design family, not proof that our
particular proposed construction is secure. Ethereum's
[proposer-selection specification](https://ethereum.github.io/consensus-specs/specs/phase0/beacon-chain/)
is useful context; Algorand's
[PQ roadmap](https://algorand.co/blog/algorand-post-quantum-cryptography-roadmap)
also identifies PQ VRF work as a distinct research/integration task. Neither
source supplies an approved replacement for this implementation.

Legacy coin ownership is a separate migration decision. Existing ECDSA-only
outputs cannot be made quantum-safe by accepting an arbitrary new PQ key from
the next spender. Holders must authorize a transfer/credential registration
while their existing keys remain trustworthy. No automatic reassignment,
forced freeze, migration deadline or destruction of unmigrated balances is
approved. Leaving legacy spending available preserves compatibility but also
leaves those unmigrated outputs exposed; release claims must say so.

Implementation map for the next stages:

- `pos/stake.*`, `pos/state.*`, `pos/storage.*`: versioned immutable signing,
  reward and withdrawal credentials; snapshot roots; persistence and undo.
- `pos/primitives.*`, `pos/block.*`: bounded PQ wire encodings, signing
  domains, block identity and canonical evidence rules.
- `pos/validation.*`, `validation.cpp`, `net_processing.cpp`: mandatory
  bond-bound hybrid authentication, branch replay and bounded peer admission.
- `wallet/staker.cpp`, `miner.cpp`: selection, mandatory hybrid block/vote
  production and exact capacity reservation.
- Wallet keystore/database/encryption, signing journal and backup/recovery:
  independently generated durable PQ credentials, with no secret reuse or
  downgrade path to classical-only authorization.
- Transaction script/signing/validation, wallet RPC/GUI and external signers:
  PQ-protected transfers, reward ownership, unbonding and withdrawals.

The trial integrates public eligibility, registered PQ credentials, mandatory
hybrid authentication and native PQ spending. Unmigrated legacy coins retain
their existing classical authorization, not an automatic PQ guarantee.

### Remaining Integration Gates

Current integration checkpoint (2026-11-23): `fPoSPostQuantum` is enabled on
testnet and regtest only. PQ spending and version-2 bond creation begin with
their height-1 bond rules. At PoS activation (public testnet height 3000), only
version-2 extensions, producer authorizations, votes and evidence are accepted.
The build refuses startup on these networks without an ML-DSA backend. Mainnet
keeps its existing activation heights and classical rules. Fresh trial chain
data is required; existing classical trial histories are not compatible.
Independent encrypted wallet keys, synchronous key creation, downgrade refusal
and complete-wallet backup/reload support are implemented.

V2 block authorization and checkpoint votes retain their existing fields,
including the Schnorr signature, and append exactly 1,312 ML-DSA-44 public-key
bytes and 2,420 signature bytes, with no length prefixes. Encoded sizes are
4,005 and 3,961 bytes, respectively. Block/vote evidence contains two matching
version-2 statements and is 8,011 / 7,923 bytes. Unsupported versions, mixed
versions, non-exact key/signature lengths and truncated inputs are rejected.
V1 encoding is unchanged and cannot carry uncommitted PQ fields.

V2 extensions allow at most 192 votes and 16 combined evidence objects. The
count is checked before allocating elements. V2 producer, vote and evidence
signatures are zeroed for candidate identity and vote/evidence commitments;
the actual signatures remain mandatory authentication data. Unsigned fields
and registered public keys remain committed, so fresh signing randomness
cannot produce another candidate ID for the same statements.

V2 bond credentials commit a Schnorr signing key and independent 256-bit PQ
signing, reward and withdrawal key IDs. The key-ID construction is double
SHA-256 of the serialized string `Verge/PQKey/MLDSA44/v1` followed by the
CompactSize-prefixed 1,312-byte public key. Hybrid validation requires the
provided key to match the bond's immutable signing ID before cryptography.
Legacy reward/withdrawal IDs and the unused VRF field must be zero in v2.
Legacy bond record encoding remains 117 bytes. A versioned record begins
with the reserved signed-amount sentinel `-1`, byte version `2`, actual
positive amount, creation height, Schnorr key and the three PQ IDs, totaling
149 bytes. No valid legacy amount collides with that marker. Trial state writes
use record schema version 2, which earlier binaries refuse. Snapshot, undo and
retired-bond records retain the versioned immutable credentials.

### Native PQ Spending and Withdrawals

PQ addresses use Bech32m with dedicated lowercase prefixes: `vergeqx1` on
mainnet, `vergeqxtest1` on testnet and `vergeqxreg1` on regtest. The trailing
`1` is the required separator, not part of the human-readable prefix. The older
`xvgpq1`, `xvgtpq1` and `xvgrpq1` forms remain accepted as input aliases only
on their original networks, for both single-key and multisig destinations.
Newly encoded addresses always use the new prefix and its recomputed checksum;
manually replacing an address prefix does not produce a valid address.
Wallet loading normalizes address-book and destination-metadata keys in bounded,
atomic batches, preserving existing canonical records if both are present.
Keys, output scripts, transaction IDs and consensus activation are unchanged;
no chain reindex or coin migration is required for this encoding change. Older
software and external explorers still need an update to recognize the new
prefixes. Existing witness HRPs
(`vg`, `vt`, `vgrt`) and all legacy address versions are unchanged. Bech32m
also accepts an entirely uppercase spelling, but never mixed case. The prefix
is followed by
the 32-byte key ID converted from eight-bit to five-bit groups with zero
padding. There is no witness-version byte. These are not SegWit addresses.
`getnewpqaddress [label]` persists an independent ML-DSA key before returning
the address. `getnewaddress` defaults to PQ in PQ-first wallets as described
below; explicit legacy addresses and legacy PoW mining destinations remain
available. On a PQ trial network, automatic change uses a new PQ key unless
an explicit coin-control change destination is supplied.

The native script is `OP_3 PUSH32 <PQ key ID> OP_NOP4` (35 bytes). The PQ
consensus flag handles this exact native form, the native multisig form below,
and the exact version-2 bond/unbond wrappers below. Executing `OP_NOP4` through the ordinary interpreter
with that flag fails, including P2SH-wrapped attempts. No witness activation,
stack-element-limit increase, classical fallback or unrestricted PQ opcode is
introduced. Legacy/mainnet flags retain existing opcode semantics.

The scriptSig has eight canonical pushes: the 2,420-byte ML-DSA signature plus
one sighash byte is split into `[520,520,520,520,341]` bytes, followed by the
1,312-byte public key split into `[520,520,272]`. Total encoded scriptSig size
is 3,757 bytes. Wrong chunk sizes, nonminimal pushes, extra chunks, a nonempty
witness or a key that does not match the funding output are rejected. All bytes
count as base transaction weight; each PQ input additionally costs 200 sigop
units. Standard transaction/block weight and sigop budgets still apply.

Only `SIGHASH_ALL` is supported, with the mandatory PoS fork-ID bit after
activation. Compute the existing BASE signature hash over the full funding
script and transaction, using its normal PoS replay domain when active. Then
double-SHA256 the serialized CompactSize-prefixed ASCII string
`Verge/Transaction/MLDSA44/v1`, little-endian network ID (`uint32`), activation
height (`int32`, zero before PoS), input amount (`int64`) and raw 32-byte
transaction signature hash. Pure ML-DSA signs that digest as a 32-byte message
using the same ASCII string as its FIPS 204 context. This binds input value
and network even before PoS; it is not the block/vote hybrid transcript.
For network `3`, activation height `1500`, amount `1000000` and inner hash
`0xabcd`, the wrapper digest in `uint256::GetHex()` notation is
`42ac79d4c4bd346d3d341e63e97e9c497c50e852ae5b24b2a6becabcf63ef308`.

Version-2 bonds have a canonical 100-byte pushed payload: `VPB`, byte `2`,
Schnorr signing key (32), PQ signing ID (32), PQ reward ID (32), followed by
`OP_DROP` and the native withdrawal script. The total is 138 bytes. Version-2
unbond outputs push `VPU`, byte `2`, little-endian unlock height (`uint32`),
then `OP_DROP` and that same native withdrawal script (45 bytes total).
Unbonding must preserve principal, credential version and withdrawal ID;
ordinary spendable inputs fund fees. Full confirmation-relative unbonding and
penalty locks remain enforced. The released output is spendable only with its
registered PQ withdrawal key after the lock, not with the online staking key.
Rewards go only to the committed PQ reward destination.

Block/vote signatures still require both Schnorr and ML-DSA. Coin spending and
withdrawals require ML-DSA alone against their PQ owner ID, without exposing a
classical fallback. The wallet's durable anti-equivocation journal precedes
both halves of every hybrid block/vote signature. Signature-only invalid
variants must not permanently poison their signature-independent candidate ID.

For bounded isolated withdrawal testing only, regtest accepts
`-posunbondingblocks=240..20160` (minimum is the snapshot delay in slots).
Shorter delays are refused: historical producer credentials must not expire
while a delayed snapshot can still select them. Mainnet and public testnet
refuse this override and retain 20,160 blocks. All nodes in an isolated regtest
must agree on it.
The C verification library API is version 2 and adds
`vergeconsensus_verify_script_with_context` with amount, network ID and
activation height. Older context-free calls refuse required PQ/replay flags.

### Native PQ Multisig Custody

Native PQ multisig uses ML-DSA-44 without SegWit, P2SH, a script tree or a
classical fallback. A policy requires `1 <= m <= n <= 5` distinct nonzero PQ
key IDs. Its canonical encoding is byte version `1`, byte `m`, byte `n`, then
the `n` raw 32-byte key IDs in strictly increasing serialized-byte order
(`uint256::operator<`, not the order of displayed reversed hexadecimal IDs).
Duplicate keys, unknown versions, invalid thresholds and trailing bytes fail.
The policy ID is double-SHA256 of the CompactSize-prefixed ASCII string
`Verge/PQMultisigPolicy/v1` followed by the CompactSize-prefixed policy bytes.
The exact funding script is `OP_4 PUSH32 <policy ID> OP_NOP4` (35 bytes).

Multisig addresses use the same network-specific PQ Bech32m HRPs as single-key
addresses, with a distinct 33-byte payload: byte `1` followed by the raw policy
ID, converted to five-bit groups with zero padding. Single-key addresses retain
their untagged 32-byte payload. Other lengths/tags are rejected. This tag is
not a witness version. An address commits to the policy, not merely its owner
keys; changing the threshold changes both output and address.

A canonical scriptSig first pushes the complete policy (at most 163 bytes),
then exactly `m` entries in increasing cosigner-index order. Each entry is a
minimal integer opcode for its zero-based index, followed by the same eight
signature/public-key chunk pushes as a single-key spend. Each public key must
hash to the policy ID at its index, and every signature must verify against
the full funding script, amount, network and transaction under the PQ sighash
above. Duplicate indices, signatures counted twice, reordered entries, extra
data, nonminimal chunks, nonempty witness or wrapped variants are rejected.
The parser is bounded to 20,000 bytes and five signers. A 5-of-5 proof is
18,955 bytes; all count at base weight. Each policy input conservatively costs
1,000 sigop units regardless of threshold. Normal weight, dust, transaction
and block limits continue to apply; wallet fee estimates reserve the exact
maximum proof size for the known threshold.

`createpqmultisig nrequired ["pqaddress",...]` works without a wallet and
returns the address, canonical policy hex, funding script, threshold and
sorted addresses. Only this network's single-key PQ addresses are accepted.
`addpqmultisigaddress nrequired ["pqaddress",...] [label]` registers the same
policy in a wallet when PQ rules are active. Register it on each cosigner
before receiving; adding it later requires an explicit `rescanblockchain`
to discover historical payments. Policy and downgrade protection are written
synchronously before publishing the wallet policy, and complete-wallet
backups retain it through encryption and restart. Make a new backup after
registration; a key-only export is not a complete multisig backup.

Wallets with a signing quorum classify the output as spendable. Wallets with
fewer keys classify it as watch-only and never claim unilateral control.
`getaddressinfo` reports `ispostquantum`, `ismultisig`, `pq_policy_id`, and,
for a known policy, `sigsrequired`, `addresses` and `policy`. Partial signers
can prepare a raw transaction with `fundrawtransaction` and
`{"includeWatching":true}`, sign with `signrawtransactionwithwallet`, pass
the returned hex to another cosigner, or merge independently signed versions
using `combinerawtransaction`. The first partial signature carries the
policy for subsequent cosigners. Partial proofs are never valid for relay;
broadcast only a fully signed transaction. Offline signing needs trustworthy
prevout script and amount data, just as other raw signing does.

```text
createpqmultisig 2 ["<PQ address A>","<PQ address B>","<PQ address C>"]
addpqmultisigaddress 2 ["<PQ address A>","<PQ address B>","<PQ address C>"] "Custody"
signrawtransactionwithwallet "<unsigned or partially signed transaction hex>"
combinerawtransaction ["<partial A hex>","<partial B hex>"]
sendrawtransaction "<fully signed transaction hex>"
```

This first policy is for ordinary coin custody. Bond producer, reward and
withdrawal credentials remain single-key v2 credentials; no multisig staking
credential or automatic conversion is implied. Existing legacy multisig
semantics and currently disabled mainnet PQ activation are unchanged.

- [x] Versioned spendable credentials, PQ owners, snapshot records and schema
  activation/downgrade refusal are integrated for fresh trial chains.
- [x] Mandatory hybrid verification covers producer authorization, votes,
  equivocation evidence and branch replay. Commitments exclude signature
  randomness. Fixed wire counts, preallocation bounds and block-weight
  reservation apply to the larger objects; unavailable crypto fails closed.
- [x] Wallet producer/voter keys use durable encrypted storage and the
  existing synchronous anti-equivocation journal.
- [x] Native PQ transfers, reward destinations, unbond authorization and
  withdrawal spending are integrated. Legacy owners can authorize transfers
  into PQ outputs; there is no arbitrary registration over existing coins.
- [ ] Independently review the implemented public eligibility selector, including
  entropy withholding, initial-PoW grinding and validator-set-change bounds.
  Randomized ML-DSA signatures are not a replacement VRF and must not provide
  extra lottery attempts.
- [x] Integrated lifecycle, activation, persistence, recovery and peer-sync
  regressions below pass on isolated Windows x64 nodes.
- [x] A bounded four-node hybrid/PQ soak passes partitions, restarts,
  convergence, finality and final reindex/state comparisons.
- [ ] Launch and monitor the fresh public trial; complete longer soak runs.
- [ ] Update external signers, explorers and migration tooling as needed;
  adapting Core does not automatically adapt those separate applications.
- [ ] Complete cross-platform/sanitizer/resource testing and independent
  consensus/cryptographic review before choosing production activation.
  Mainnet parameters remain unchanged. Protecting block signatures does not
  protect coins still authorized only by ECDSA/Schnorr.
- [ ] A future mainnet release must explicitly schedule both public eligibility
  and PQ authentication/spending, including its bond-registration window.
  Changing only the provisional mainnet fork height does not enable these
  two currently disabled mainnet flags.

### Fresh Trial Operator Checks

All participants must use the new trial build and fresh compatible chain data.
Public testnet retains its height-3000 PoS transition, height-1 bond rules,
720-block bond maturity and 20,160-block unbonding delay. Do not use a previous
classical trial database or downgrade a wallet after creating PQ keys.

Check `getposinfo`: `eligibility_scheme` is `public-stake-weighted-v1`,
`authentication_scheme` is `schnorr-mldsa44-v2`, and `pq_spending_active` is
true. `getbondinfo` reports `credential_version: 2`, the committed PQ signing
ID, and the real PQ reward/withdrawal addresses. Normal automatic staking
creates those credentials without a separate registration step.

```text
getnewpqaddress "PQ receive"
sendtoaddress "<returned PQ address>" 2000
setstaking true
listbonds
getbondinfo "<bond transaction ID>" <output index>
setstaking false
unbond "<bond transaction ID>" <output index>
listunbondings
backupwallet "<new complete backup path>"
```

`createbond 2000` remains available for an explicit bond. Supplied reward and
withdrawal addresses must be wallet-controlled PQ addresses on these trial
networks. Disable automatic staking or increase the reserve before unbonding
if released funds should not be bonded again. A withdrawal is spendable only
when its confirmation-relative lock has expired, not merely when its original
transaction has confirmations.

PQ receive, change and bond keys are independent of the classical HD seed.
Qt's Overview page separates owned **Legacy total** and **PQ total** balances;
these are a breakdown of the existing total, not additional funds. Each includes
its available, pending, immature and bonded/unbonding outputs, excluding
watch-only outputs. PQ ownership includes native outputs and PQ bond/withdrawal
wrappers. Pending bond transactions remain in the pending balance until mined.

On first initialization with PQ spending rules active for the next block, a
wallet with no existing keys starts with a durably stored PQ receiving key, not
a classical HD seed or a prefilled legacy keypool. This PQ-first policy persists
across restart, encryption and unlock. Existing wallets retain their keys and
legacy keypool behavior; opening them does not migrate their coins. Activation
uses the locally validated chain height, not a peer's advertised height or the
wall clock. A fresh node still synchronizing from below activation can therefore
initialize a legacy wallet; the Receive tab switches its default once activation
is locally known.

In PQ-first wallets, `getnewaddress` and `getrawchangeaddress` default to PQ while
PQ rules are active. Explicit `"legacy"` requests and the existing legacy
`-addresstype` / `-changetype` overrides remain available; explicit `"pq"` RPC
requests are rejected before activation. Classical HD keys are generated only
when needed for compatibility, hybrid staking or messaging, or when explicitly
requested with `keypoolrefill`. Startup and unlock never backfill a large legacy
keypool for these wallets. Ordinary transaction change already defaults to PQ
when its rules are active. PQ keys are independent of the classical HD seed:
use a current complete wallet backup, not a legacy seed-only backup.

The Qt Receive tab offers **Post-quantum (PQ)**, **Legacy** and **Stealth** address
types. PQ is the default when its rules become active, while explicit selections
remain unchanged on ordinary balance updates. On inactive networks the PQ option
is disabled and the wallet API independently refuses creation. PQ receive keys
are durably stored under the wallet's normal encryption, with an unlock prompt
when needed. A new full-wallet backup must succeed before displaying or recording
the payment request. Canceling unlock, backup confirmation or the file picker,
and failed key/backup operations never produce an empty or legacy fallback
request. A generated but unshared PQ key remains in the wallet after backup
cancellation. PQ addresses use the normal URI, QR and request-history flow.
Chatkey creation remains a legacy-address operation; PQ and stealth selections
disable and clear that option rather than claiming PQ secure messaging support.

**Move legacy balance to PQ** prepares an ordinary on-chain self-transfer to a
new, durably stored ML-DSA-44 address in the same wallet. It is available only
where PQ rules are active; the current mainnet configuration keeps it disabled.
Nothing is migrated merely by opening an old wallet. The user unlocks the wallet,
reviews the net amount, fee, destination and input count, confirms, and saves a
new complete wallet backup before broadcast. Canceling or a failed backup does
not send the transaction. The migration uses confirmed, safe, spendable legacy
outputs only, honoring manual coin locks and excluding PQ inputs, watch-only
coins, immature rewards, bonds and still-locked withdrawals. Each transaction
uses at most 100 inputs and a conservative standard-weight budget. Larger
wallets require explicitly confirmed additional batches; the dialog reports
the remaining confirmed/unlocked legacy amount. It never auto-unbonds coins.
Readiness and the exact selected inputs are checked again before commit, so
concurrent staking/spending cannot silently substitute other inputs. A submitted
migration still needs confirmation; normal reorganization and finality rules
apply before treating its legacy inputs as safely replaced by PQ outputs.
Backups taken here do not cover PQ keys generated later.

Take a new complete wallet backup after new PQ keys are created; a prior
backup does not contain later independently generated keys. Restore on a
separate node or with the original wallet offline: Berkeley DB deliberately
rejects loading the original and its copied internal file ID in one process.
Never run copies of active staking credentials simultaneously. The script
verification API checks spending signatures, not the entire bond lifecycle;
block/transaction consensus validation also enforces maturity and withdrawal
locks. External signer and explorer applications require their own updates.

### Verification Status

Integrated Windows x64 trial verification (2026-11-23) passes **144 unit cases
and 90,056 assertions**, including native PQ address/checksum/ownership tests,
canonical chunk bounds, network/amount/script/transaction hash binding, both
required hybrid signature halves, registered-key evidence authentication and
maximum-size hybrid attachment reservation. Classical-rule unit fixtures
remain explicit compatibility tests; the functional nodes below require the
actual new hybrid rules. The independent transaction-wrapper vector above
was also computed separately with Python's SHA-256 and struct serialization.

Migration coverage includes bounded input selection, exact self-transfer and fee
accounting, exclusion of PQ/watch-only/locked/immature/bond outputs, disabled
activation and locked-wallet rejection, pending/staked PQ balances and a real
mempool-validated legacy-to-PQ transaction. Offscreen Qt tests cover the separate
balances, activation/button states, refusal without a usable chain, and layout
at three widths and font sizes. All seven Qt test entries pass. Receive coverage
includes encryption/unlock, canceled unlock, inactive-network refusal, backup
confirmation/file-picker cancellation, successful encrypted-wallet backup,
relocking, request-history persistence, URI parsing and QR generation. A manual
user workflow and non-Windows GUI validation remain trial checks. The PQ lifecycle regression
keeps simulated chain time progressing during its PoS relay wait, without
changing production relay policy.

Native PQ multisig coverage includes thresholds 1, 2 and 5; five-cosigner
proof sizing; duplicate and noncanonical rejection; quorum ownership;
signature merging and carried-policy cosigning; network/amount/script replay
binding; sigop accounting; public verification API calls; wallet fee selection,
owned-balance accounting; encrypted policy persistence and backup/salvage.
The three-node lifecycle now exercises native 2-of-3 funding, partial signing,
independent signature merging, incomplete-proof relay refusal and confirmed
spends both before and after PoS. Two consecutive fresh runs pass, including
encrypted restart and backup restoration of the policy. An earlier run timed
out on pre-PoS single-key transaction relay before reaching multisig; it did
not reproduce in those unchanged reruns, and its cause is not established.
Activation, bounded prefetch and crash-recovery regressions also pass again
against the multisig-enabled daemon.

These nine rebuilt-daemon regressions pass: `feature_pos_post_quantum`,
`feature_pos_public_eligibility`, `feature_pos_activation`,
`feature_pos_crash_recovery`, `feature_pos_storage`, `feature_pos_vote_heads`,
`p2p_pos_prefetch`, `feature_pos_worker` and `wallet_pos_unbond`.
They cover protected transfers before/after PoS, maturing/eligible unbonding,
rejection before lock expiry, spending released principal, both mandatory
producer signatures, signature-only candidate retry, hybrid checkpoint votes
and finality, automatic production, independent slot selection, fresh sync,
reorganizations, clean/abrupt recovery, full/chainstate reindex, encrypted-key
reload, current-wallet backup restoration and clean shutdown. The full
withdrawal test uses the bounded 240-block regtest override, not a reduced
public-testnet lock. Backup restoration uses a separate fresh node, preserving
Berkeley DB's existing duplicate-file-ID guard.

The Windows daemon, CLI and Qt targets compile. A standalone C verification
API executable links and runs with only consensus, crypto, secp256k1 and OS
libraries, without wallet/server/common/util libraries. The compile-time
disabled-ML-DSA build passes its fail-closed case / 16 assertions. This does
not replace testing an actual older OpenSSL installation, the exported shared
library on every platform, a GUI runtime trial, or a public network run.

Before the multisig additions, the four-node hybrid/PQ soak with eight bonds per node passes 80 action rounds
in 122.704 seconds including final reindex/comparisons. It exercised four
partitions, one clean restart and one abrupt restart, produced 112 blocks
including competing branches, and converged at height 2104. Checkpoint epoch
10 finalized at height 2087. Final UTXO and state comparisons pass and the
full log scan finds no honest-peer penalties (maximum score zero).
Observed combined resident memory starts at 382,160,896 bytes and peaks at
396,242,944 bytes. These samples do not establish leak freedom. The isolated
artifact `build-msvc/pq-release-soak-20261123/pos-soak-results.json` is marked
`passed`. This is bounded local testing, not an overnight or production-scale
trial. All test nodes shut down successfully afterward.

All test directories and measurements are isolated build artifacts. No live
wallets or real chain data were used, and no public PQ testnet has launched.

The public selector passes five dedicated unit cases, including independent
weighted sampling vectors and malformed snapshots. At the public-selector
checkpoint, the local suite passed 122 cases / 88,700 assertions. A three-node isolated regtest checks 30
consecutive slots against an independent Python selector, fresh observer sync,
and chainstate reindex. Activation, vote-head and crash-recovery functional
regressions also pass with public eligibility enabled. These results precede
mandatory PQ signature and coin-spending integration.

Independent PQ seed persistence is implemented in the existing wallet
keystore/database, including encryption, synchronous key creation, reload,
backup and corruption tests. New `pqkey` / `cpqkey` records bind their public
key and independent seed using a domain-separated 256-bit key ID. Plain seeds
use secure-allocator memory; encrypted seeds use the existing wallet master
key and encryption path. The wallet minimum version is raised only when a PQ
key is first stored, not when an ordinary wallet is opened. Numeric version
27.0.1 (`.beta10`) is required so preceding 27.0.0 betas refuse these wallets
instead of silently ignoring unfamiliar key records. PQ seeds are independent
of the classical HD seed: a classical seed-only backup is not a PQ backup.
Use a current complete wallet backup; no secret export RPC has been added.
`dumpwallet` refuses PQ-bearing wallets rather than producing a text file
that silently omits their PQ secrets; `backupwallet` remains supported.
Key-only recovery retains both PQ key records and the minimum-version guard,
regardless of their scan order. Ordinary loading rejects PQ records without
that guard rather than opening a wallet that an older binary could misread.

At the wallet-codec checkpoint, the Windows x64 build passed 131 unit cases / 88,876 assertions,
including encrypted PQ reload and backup restoration, malformed/missing
version refusal, recovery-filter ordering, failed-write refusal, incomplete
text-export refusal, immutable bond-key binding, bounded v2 decoding, legacy
record byte compatibility and signature-independent v2 candidate commitments.
The rebuilt daemon passes the three-node independent public-selector/reindex
test, `wallet_pos_unbond` and `feature_pos_crash_recovery`. These runs use fresh
isolated regtest directories, not live wallets or a launched public testnet.

Initial foundation testing on Windows x64 with OpenSSL 4.0.0 passed 8 PQ unit cases / 94
assertions, including the pinned NIST vectors and an independently computed
hybrid transcript hash. That initial local PoS/wallet regression binary passed
117 cases / 58,617 assertions. A separately compiled
`OPENSSL_NO_ML_DSA` variant passes the unavailable-backend case / 16 assertions,
including rejection of an otherwise valid Schnorr half-signature. This is a
compile-time disabled-backend test, not an actual older-OpenSSL build.

Remaining verification includes the Linux/macOS/ARM build matrix, actual
older-backend compatibility, sanitizers, GUI runtime trials, overnight and
production-scale resource testing, fully authenticated maximum-capacity
producer/evidence scenarios, external ecosystem adaptation and independent
consensus/cryptographic review. The attachment-capacity unit fixture checks
assembly/weight reservation, not a network block with 192 independently
authenticated votes and 16 independently authenticated evidence items.
The integrated local results above support a fresh experimental trial, not
mainnet activation or a claim that all existing coins are quantum-resistant.

## Post-Quantum Secure Messaging (2026-11-25)

SMSG is a separate messaging protocol, not a PoS consensus object. The new
paid-message envelope is version 3.1; existing 3.0 history remains readable.
New RPC/GUI sends require a pinned PQ recipient bundle and never substitute
classical encryption. Upgraded relays advertise `NODE_SMSG_PQ` (bit 26), and
exchange messages only with peers advertising this capability. This does not
exclude older peers from blockchain synchronization.
Unanswered messaging handshakes retry at most once per 30 seconds after the
chain is current. An explicit peer disable suppresses retries until that peer
re-enables messaging. Repeated local chatkey lookups are idempotent and never
fall back to returning a classical-only messaging key.

Messaging keys are independent of staking and PQ coin-spending keys. Each
local messaging identity has 96 independently generated secret bytes: a
32-byte ML-DSA-44 seed and a 64-byte ML-KEM-768 seed. The existing classical
address is retained as a compatibility label and hybrid ECDH credential;
`smsgnewaddress` creates a dedicated address rather than repurposing a coin
receiving address. The public bundle is version 1, little-endian network ID,
33-byte classical public key, 1,184-byte ML-KEM public key and 1,312-byte
ML-DSA public key, totaling 2,534 bytes. Its full 256-bit domain-separated
fingerprint identifies the complete bundle, not only its classical address.
Contact bundles are immutable once pinned. First-contact authentication is
trust-on-first-use unless users compare full fingerprints through an
independently authenticated channel; migrating an old address alone is not
proof of the new PQ identity.

The shareable chatkey is `pq1` followed by canonical uppercase, padded Base32
of the bundle and a four-byte double-SHA256 checksum. Input is bounded to
4,200 characters before decoding. Uppercase Base32 permits QR alphanumeric
compaction. Sharing a public bundle does not reveal its private seeds.

Each message encapsulates a fresh ML-KEM-768 shared secret and creates a
fresh secp256k1 ECDH ephemeral key. HKDF-SHA256 extracts from the two fixed
32-byte secrets with an authenticated-envelope-derived salt and expands with
the `Verge/SMSG/HybridKey/v1` label. AES-256-GCM encrypts the body with a fresh
96-bit nonce. Associated data binds the complete recipient fingerprint,
network, message version, flags, timestamp, retention, ephemeral public key,
payload length and KEM ciphertext. Identified senders sign the complete
unsigned plaintext and associated data using ML-DSA-44 with a separate SMSG
context. Anonymous sends remain encrypted but do not claim sender identity.
All authentication must pass before plaintext is returned or a sender bundle
is learned. An ECDH-based 128-bit recipient filter precedes KEM decapsulation;
it is only a scanning optimization, never the PQ authentication boundary.

The outer header remains 104 bytes. Its first 16 MAC bytes hold the recipient
filter; the last 16 hold the GCM tag. The payload is a 1,088-byte KEM
ciphertext, encrypted body and appended 32-byte funding transaction ID. The
body is a one-byte anonymous/identified discriminator, an optional complete
sender bundle, a little-endian four-byte plaintext length, up to 5,120 message
bytes and, for identified messages, a 2,420-byte ML-DSA signature. No message
compression is used in the PQ envelope. Maximum payload size is 11,199 bytes.
Local identities are capped at 256 and contact pins at 4,096; existing relay
rate, batch and aggregate storage limits still apply.

Paid-message authorization is still mandatory after PoS/PQ activation. New
PQ funding uses a standard OP_RETURN output containing `SMSG4`, the existing
28-byte lookup ID and an additional 32-byte domain-separated envelope hash.
The data is 65 bytes, within the existing standard data-carrier limit. This
is not a new transaction type or consensus rule. The funding transaction is
validated under the chain's active spending rules. Relays require its payment
and full-envelope commitment to be confirmed on the active chain; unconfirmed
or reorganized-away funding does not authorize relay. Legacy `SMSG3` funding
remains readable. The short bucket/lookup identifiers are not PQ signatures
or funding-authorization proofs.

Secret records and immutable contact pins are synchronously persisted in
wallet.dat and included in full-wallet backups. Encrypted wallets encrypt the
messaging seeds, erase plaintext records during encryption, refuse private-key
access while locked, and verify derived public bundles after decryption. The
minimum wallet version is 27000101, so older binaries refuse these wallets
rather than silently ignoring messaging keys. The application build number
advances to 27.0.1.1 for this guard; wallets without the new records are not
automatically upgraded. GUI creation requires a new full backup before sharing.

This design does not provide a messaging ratchet or forward secrecy after
the recipient's long-term messaging secrets are stolen. It does not retrofit
quantum protection onto captured legacy ciphertext, make Tor quantum-safe,
or hide timing and routing metadata. Independent cryptographic protocol review
and cross-platform qualification remain release gates.

Implementation progress:
- [x] OpenSSL ML-KEM/ML-DSA wrappers and canonical public bundles.
- [x] Hybrid authenticated envelope, mandatory PQ sends and versioned relay.
- [x] Encrypted wallet persistence, immutable contact pins and downgrade guard.
- [x] RPC/GUI chatkeys, fingerprints, backup prompts and legacy/PQ history labels.
- [x] Unit and three-node functional regression fixtures added.
- [x] Native regression suite: 153 cases and 90,215 assertions passed, including
  stored-payload bounds and encrypted messaging-key backup restoration.
- [x] Three-node PoW/PoS messaging regression passed: paid funding, relay delivery,
  maximum payload, anonymous sends, encrypted restart/unlock, offline recovery,
  native PQ spending after activation, legacy peers and bounded handshake retry.
- [ ] Operator Qt builds and GUI/manual testnet verification.
- [ ] GUI runtime/QR validation and independent protocol review.

## Unbonding Transaction

A valid unbond transaction spends one `TX_STAKE_BOND_V1` output into one `TX_STAKE_UNBOND_V1` output carrying exactly the same principal and withdrawal credential. The wallet encodes a requested unlock height based on the next block plus 20,160 accepted blocks. Missing that next block does not expire the transaction: the effective withdrawal height is the greater of the encoded height and the actual confirmation height plus 20,160.
The version 1 unbond output uses a 35-byte script:

~~~text
PUSHDATA(8) || "VPU" || 0x01 ||
unlock_height_le32 ||
OP_DROP ||
OP_DUP OP_HASH160 withdrawal_key_id[20] OP_EQUALVERIFY OP_CHECKSIG
~~~

The requested unlock height is an absolute accepted-block height encoded as an unsigned little-endian 32-bit integer. Creation rejects heights no greater than the unbonding interval or later than the containing block height plus that interval. Delayed inclusion is allowed, but never shortens the confirmation-based lock. Zero heights, zero withdrawal identifiers, malformed pushes, altered magic/version bytes, and trailing opcodes are rejected. The transaction may use separate ordinary inputs to pay its fee; the bonded principal may not be reduced to pay fees.

The bond stops contributing eligibility and vote weight according to the approved delayed snapshot rules. Before the effective unlock height, spending the unbond output is invalid. At or after that height, the withdrawal credential may spend it normally or create a new bond. New bonding resets maturity and enters future snapshots normally.
The transaction-validation implementation enforces the lifecycle before and after PoS activation so stake may be prepared while the PoW history is still active:

- every bond output is at least 1,000 XVG;
- any coinbase input in a transaction that creates a bond is at least 720 accepted blocks deep;
- an unbond output cannot be created unless the transaction spends a live bond;
- a transaction spending a live bond has exactly one bond input and exactly one unbond output;
- that unbond output preserves the complete bonded principal and withdrawal credential;
- its requested unlock height is no later than the containing block height plus 20,160; and
- an unbond output cannot be spent before either its encoded height or its confirmation height plus 20,160.

This delayed-inclusion rule is a PoS consensus update and requires coordinated
testnet upgrades. Previously confirmed canonical unbonds retain the same
effective unlock height and replay unchanged. Existing unconfirmed requests
can be included without changing their signatures or transaction IDs, but the
full lock starts at their eventual confirmation. No serialization or wallet
database migration is involved.

Ordinary inputs and outputs may accompany an unbond transition to pay fees and return ordinary change. The bonded principal itself cannot pay the fee. Requiring every coinbase input in a bond-creation transaction to satisfy the 720-block rule is intentionally conservative: wallets should avoid mixing immature fee rewards into a bond transaction.

Wallets may begin unbonding either maturing or eligible bonds, provided no
consensus withdrawal lock applies and liquid funds are available for the fee.
The signer uses the withdrawal key and commits to the complete metadata-prefixed
script, both before and after PoS replay protection activates. Starting unbonding
does not release spendable funds immediately: the wallet excludes unbond outputs
from available balance and coin selection until the next candidate block reaches
their effective unlock height. This availability is recalculated across height
changes and reorganizations. Live bonds remain excluded from ordinary payments.

The wallet table includes pending and confirmed unbond outputs. Pending requests
show `Unbond pending` with zero confirmations and no running countdown. Confirmed
outputs show `Unbonding` and their blocks remaining, then `Unlocked` when
spendable. An outstanding request cannot be unbonded a second time. Confirmed
locked principal remains in the overview's bonded/unbonding balance until it
becomes available; pending outputs do not double-count the still-active bond.
`listunbondings` reports the encoded height separately from the effective
`unlock_height`, marks pending heights as estimates, and exposes the same
lifecycle statuses. Unconfirmed or abandoned outputs are never spendable merely
because an old encoded height has passed.

An actively locked bond cannot evade equivocation consequences by unbonding. Consensus rejects an unbond transition that conflicts with an applicable on-chain eligibility lockout.
## Rewards And Value Conservation

Let `fees` equal total ordinary transaction inputs minus total ordinary transaction outputs. The bonded principal is not an input or output of the block reward transaction. Consensus enforces:

```text
stake_reward >= 0
stake_reward <= fees
new_subsidy = 0
```

The producer may deliberately claim less than the available fees. Unclaimed fees are destroyed for that block; they are not carried forward. A zero-transaction block has zero fees and may still be valid.

Stake principal and rewards are accounted for separately. Block production does not spend or recreate the bonded principal, so its original maturity remains intact. Newly earned fee outputs retain the existing 120-block coinbase spend maturity and require 720 confirmations before they can be bonded. After unbonding completes, spending or rebonding creates ordinary new maturity state.

## Fork Choice

PoW chainwork is not assigned to PoS blocks. Before checkpoint finality exists, actual cumulative PoW work still ranks competing transition ancestors. The PoS fork-choice rule must be deterministic for the same authenticated candidate tree.

The fork-choice rule is:

1. Never reorganize below a finalized checkpoint.
2. Starting at the latest finalized checkpoint, project each fully validated candidate's branch-local latest votes onto the ancestry of their voted heads. Count at most one latest valid vote per bonded delegation in that branch view, weighted by its declared delayed snapshot.
3. For each child edge, use the maximum support assigned to that edge by any eligible descendant candidate's authenticated branch view. Never sum support from competing branch views: incompatible bond histories must not multiply the same principal.
4. Repeatedly choose the child with the greatest projected support. Break a support tie by the greatest fully validated descendant height, then by the lowest child block hash. Descend until a validated leaf is reached. This is a whole-tree rule, not a pairwise fold of candidate preferences; candidate arrival and iteration order do not select the winner.
5. Ignore votes that conflict with finalized history, are from an ineligible delegation, or target an unknown or invalid block. Off-chain relay votes do not replace the authenticated branch state.

Hash ties use lexicographic comparison of the 32-byte wire serialization,
matching `uint256::Compare`. This byte order is the reverse of the displayed
RPC hexadecimal string; it is not numeric comparison of that string. The same
ordering applies to the transition-ancestor hash tie below.

Receiving a structurally valid PoS header is not enough to add stake score or make it an active-chain candidate. A PoS branch becomes eligible only after every required full block has passed branch-local UTXO, stake proof, authorization, commitment, vote, evidence, and state-transition validation. Legacy `GetBlockProof` returns zero for `nBits` zero and is not replaced with synthetic work. Recent fully validated side-branch blocks remain servable after activation; the zero-work clock must not make them unavailable for convergence.

Before a supermajority certifies a checkpoint, select a transition ancestor at height `nPoSActivationHeight - 1` by greatest actual cumulative PoW work, then presence of an authenticated PoS descendant, then lowest transition block hash. Run tree descent only within that ancestry. The first local PoS block does not irreversibly pin that node's last-PoW block. Once checkpoint finality exists, its ancestry is fixed and the finalized barrier remains authoritative.

Legacy equivalent-work-time calculations return a saturated duration when
their reference tip has zero proof. This prevents division by zero after PoS
activation without assigning synthetic work to PoS blocks or influencing PoS
fork choice.

Historical PoS synchronization is validation-gated. A peer serves one PoS
header at a time, and the receiver requests the next header only after the
corresponding requested full block passes PoS validation. This prevents cheap
unvalidated header chains from consuming memory while allowing lagging nodes
to advance continuously through the post-activation chain.

Peers advertising the optional Verge `NODE_POS_SYNC` service bit (`1 << 25`)
can additionally prefetch full PoS blocks. `getposblocks` contains one 32-byte
hash of a fully validated active-chain anchor. `posblocks` echoes that anchor,
then a one-byte `more` flag, a CompactSize count, and the complete blocks in
parent-first order, without witness serialization. A response contains at most
eight blocks and at most 2 MiB including its envelope. This replaces repeated
header/body round trips with one bounded full-block response, not a trusted
header chain.

Only one batch may be outstanding globally, with a fixed 60-second deadline.
New requests must start at the current fully validated active tip, not an old
ancestor supplied by a peer. Blocks already validated through another peer are
skipped without reprocessing their payloads or secure messaging contents.
If an in-flight batch encounters a competing active-chain successor, it falls
back without penalizing its sender; branch exploration and selection continue
through normal legacy synchronization and the unchanged fork-choice rules.
Unsolicited replies and mismatched anchors never enter the block index. Count,
byte, framing, and linkage checks precede block processing; every accepted block
still undergoes the normal branch-local PoS, UTXO, signature, vote, evidence,
finality, and fork-choice checks before the next block is processed. No block
or header receives synthetic work or eligibility from being prefetched. A failed
block stops the batch; a future-slot block falls back to the bounded header
retry path without penalizing honest clock skew. Disconnect releases the global
request. Timeout, an empty response, a changed active-chain anchor, or a block
too large for the batch budget selects legacy download for that connection.
Serving respects pruning, historical upload limits, send-buffer backpressure,
and a per-peer token bucket (32 requests initially, eight replenished per second).
Old peers need no changes and never receive this optional request. This is a
transport optimization, not a consensus or transaction-format change. The GUI
shows validated PoS block counts instead of extrapolating a PoW header-sync
percentage from elapsed time.
The canonical extension encoding and independent producer signature are checked
against the header's `hashPoSData` commitment and authorization before
block-index admission, for both batch and legacy body transport. A
damaged extension is treated as a corrupt payload, so it cannot permanently
invalidate the committed block ID or prevent retrying the authentic body.

During initial block download an unprivileged node still serves connecting
headers. Pre-activation PoW uses the protocol's normal 2,000-header maximum so
the requester can unambiguously continue, while post-activation relay still
stops at one PoS locator header. This prevents two restarting nodes from
refusing all progress without creating an unbounded response.

Any legacy fixed-depth reorganization limiter must apply only before PoS
activation. After activation, stake-weighted tree descent chooses among fully validated branches
and the finalized-checkpoint barrier rejects reorganizations below finality.
Retaining the fixed-depth limiter after activation would prevent convergence
after a legitimate partition before those blocks become finalized.

## Vote And Checkpoint State Transitions

A synthetic bootstrap checkpoint is assigned to the final PoW block at height
`nPoSActivationHeight - 1`. It uses checkpoint epoch `UINT64_MAX` and is
initially justified, but not automatically finalized. It is not a stake snapshot
epoch and the shared numeric sentinel is interpreted according to the serialized
field's type. The first valid PoS block creates the epoch 0 checkpoint, and the
first supermajority link therefore votes from the final-PoW bootstrap checkpoint
to epoch 0. The bootstrap checkpoint carries no stake weight. It becomes
irreversible only through an authenticated supermajority link, not merely
because one node observed a particular final PoW block first. Before that link,
transition ancestry follows the deterministic rule above. Durable wallet signing
records permit changing this synthetic source root only at a strictly later
target epoch; same-target equivocation and regressing heads remain forbidden.

A checkpoint for epoch `e` is the first valid block in epoch `e`. If an epoch has no block, it creates no distinct checkpoint. Each signed vote contains:

```text
version
bond_outpoint
snapshot_epoch
source_epoch
source_checkpoint_root
target_epoch
target_checkpoint_root
head_slot
head_block_root
signature
```

The source is the voter's latest justified checkpoint. The target is a checkpoint descended from the source. The head is the voter's current fork-choice head descended from the target. A bond may update `head_slot` and `head_block_root` while retaining the same source and target link; only its latest valid message contributes to LMD-GHOST.

A supermajority link exists when votes for one exact source-target checkpoint pair represent at least two-thirds of eligible bonded value in the applicable delayed snapshot. A target becomes justified when its source is justified and the supermajority link is valid. A justified source becomes finalized when the checkpoint in the next non-empty epoch is justified by a supermajority link from that source. Empty epochs do not create checkpoints and do not permanently prevent later finalization.

A bond violates finality safety if it signs two different target checkpoint roots for one target epoch or signs surround votes as defined in the mandatory evidence rules. Merely updating the head on the same source-target link is not equivocation. Nodes retain only the latest valid head message per bond for fork choice while retaining sufficient signed history to prove finality violations.

Votes are relayed independently and may be included until stale. Inclusion order is canonical by bond outpoint, with at most one vote per bond in a block. A bond's accepted target epoch may never regress. For the same source-target link, `head_slot` must strictly increase; altered or duplicate messages at the same or an older slot are rejected. Vote age is bounded by the eight-epoch state window minus the configured snapshot delay, ensuring the required snapshot is always retained; latest-vote state is pruned on the same bound. Vote signatures are verified against the signing key and weight from the vote's declared snapshot epoch; peer-supplied weight is ignored.
## Finality And Long-Range Protection

PoS-only consensus requires protection against old keys constructing alternate history. The production design must include:

- justified and finalized checkpoints;
- conflicting-vote detection;
- an unbonding or eligibility-lock period;
- weak-subjectivity checkpoints for new or long-offline nodes; and
- explicit bootstrap behavior for assume-valid and snapshots.
Official releases must embed a recent finalized checkpoint for public networks as soon as post-activation finality exists, and subsequent releases must keep it current. A node can configure the exact trusted height and block hash with `-postrustedcheckpoint=<height>:<hash>`; startup rejects malformed values or heights before activation, verifies that a reached checkpoint is on the active chain and consistent with any local finality that has advanced past it, and warns when the chain advances one unbonding window without any trusted checkpoint. Consensus rejects a different block at that height and every descendant whose ancestor does not match it. A node that has been offline longer than the 20,160-block unbonding period must obtain a newer release or configure a current trusted checkpoint. Peer majority alone cannot establish a weak-subjectivity root.

After activation, the legacy fixed-depth reorganization limit does not select the chain. Reorganizations may affect any unfinalized block according to stake-weighted fork choice, but no ordinary fork may replace or precede the latest finalized checkpoint.

Checkpoint epochs contain 120 slots, approximately one hour. Empty slots remain part of the epoch and do not shift epoch boundaries. A checkpoint is finalized after valid votes represent at least two-thirds of all currently bonded, mature delegated stake. If that threshold is unavailable, valid block production continues but finality pauses.

Voting is automatic whenever staking is enabled and the delegated staking key is available. The wallet signs and relays checkpoint votes without user prompts. A vote does not spend XVG, change reward ownership, or grant general spending authority. Users make only the explicit lifecycle decisions to bond, enable or disable staking, and begin unbonding.

Unbonding takes 20,160 accepted blocks. Its wall-clock duration varies with occupied slots and is expected to exceed seven days under the one-eligible-producer-per-slot target. Stake remains slashable or lockable for protocol violations during that period and cannot evade an already-observed equivocation by beginning withdrawal.

Regtest must exercise finalization, but Phase 2 must not pretend that a single-node checkpoint implementation is production-ready.

## Equivocation

Signing two different blocks in the same slot is objectively detectable even
when the blocks name different parents. Equivocation evidence consists of both
conflicting, validly signed block authorization messages and is bounded to a
canonical fixed format.

Evidence affects consensus only after inclusion in a valid block. Withdrawal
locks begin immediately in the evidence-containing block. Production and voting
eligibility are removed at the first block of the next epoch, when the
20,160-block lockout duration begins. A withdrawal lock alone must not reject an
otherwise eligible producer in the evidence epoch. Producers and block
validation share the same maturity and eligibility check. Replaying the same
offense is idempotent, while a later offense renews an expired lockout or extends
an active one. Conflicting or malformed evidence is rejected before expensive
signature checks where possible. For surround-vote ordering, the final-PoW
checkpoint's serialized `UINT64_MAX` epoch is logically epoch -1, preceding
epoch 0; it is valid only as a source, never as a vote target.

Transactions spending a withdrawal-locked bond are rejected from the mempool. When newly included evidence activates a lock, any conflicting mempool spend and all of its descendants are evicted so stale withdrawals cannot poison subsequent PoS block templates.

The initial implementation uses an eligibility lockout rather than destruction of principal. Monetary slashing remains out of scope until it has been independently designed and audited.

## Mandatory Security Modernization

The PoS implementation adopts the following requirements as consensus and release gates.

### Schnorr Authorization

Delegated block signatures and checkpoint votes use canonical 64-byte BIP340 Schnorr signatures with 32-byte x-only secp256k1 public keys. Legacy DER ECDSA is not valid for new PoS authorization. The secp256k1 `extrakeys` and `schnorrsig` modules must be explicitly enabled and tested in every supported build workflow. P-256 remains isolated to RFC 9381 VRF operations.

### Tagged Consensus Hashes

All PoS hashes use tagged SHA-256 with distinct, versioned domains. Initial domains include:

```text
VergePoS/Bond/v1
VergePoS/Block/v1
VergePoS/Vote/v1
VergePoS/EpochSeed/v1
VergePoS/StakeRoot/v1
VergePoS/VoteRoot/v1
VergePoS/StakeProof/v1
VergePoS/Unbond/v1
VergePoS/Equivocation/v1
VergePoS/VRFKey/v1
```

A hash from one domain is never accepted in another domain. Exact tagged-hash construction and byte vectors are part of the consensus test suite.
The version 1 tagged hash is a single SHA-256 operation constructed as:

~~~text
tag_hash = SHA256(ascii(domain))
result   = SHA256(tag_hash || tag_hash || canonical_serialization(object))
~~~

It does not use the legacy double-SHA-256 object writer. As an initial cross-platform vector, hashing the one-byte payload 00 in the VergePoS/Bond/v1 domain produces digest bytes d6a4f440647636141f9427e8cdcd6a9bf53d222c5f8eead2743de31851eaddfc, represented by the node's uint256 hexadecimal display as fcddea5118e33d74d2ea8e5f2c223df59b6acdcde827941f1436766440f4a4d6.

### Canonical Serialization

Every PoS object has an explicit version, fixed field order, fixed-width integer encoding, canonical public-key encoding, and strict maximum serialized size. Parsers reject trailing bytes, duplicate fields, unknown versions, non-canonical signatures, non-minimal encodings, and unsorted or duplicate vote entries. Consensus objects do not use permissive extension parsing.
#### Version 1 Primitive Layouts

Phase 2 uses the existing network serializer's little-endian fixed-width integers and canonical 36-byte outpoints. Cryptographic byte strings have fixed sizes and are serialized without compact-size prefixes. The first byte of every primitive is version 1; every other version is rejected.

The version 1 stake proof is exactly 295 bytes in this order:

| Field | Size |
| --- | ---: |
| version | 1 byte |
| bond outpoint | 36 bytes |
| slot | 8 bytes |
| snapshot epoch | 8 bytes |
| snapshot root | 32 bytes |
| epoch seed | 32 bytes |
| compressed P-256 VRF public key | 33 bytes |
| VRF output | 32 bytes |
| VRF proof | 81 bytes |
| x-only secp256k1 signing public key | 32 bytes |

The block signature is deliberately excluded from the stake proof. Including it would make the signed block message commit to a stake-proof hash that contains the signature itself. Block authorization is therefore a separate, fixed 273-byte object:

| Field | Size |
| --- | ---: |
| version | 1 byte |
| network identifier | 4 bytes |
| parent block root | 32 bytes |
| candidate header hash | 32 bytes |
| slot | 8 bytes |
| bond outpoint | 36 bytes |
| unsigned stake-proof hash | 32 bytes |
| fee-reward transaction hash | 32 bytes |
| parent randomness | 32 bytes |
| block signature | 64 bytes |

The block signing hash uses the `VergePoS/Block/v1` domain over the first 209
bytes and excludes the signature. The candidate signing-header hash is
calculated with `hashPoSData` set to zero, avoiding a signature cycle.
`hashPoSData` is the `VergePoS/Data/v1` tagged hash of the extension with only
the producer authorization signature replaced by 64 zero bytes. The full
signature remains serialized in the body and must verify independently. The
final PoS block ID hashes the extended header. Signature randomness alone
therefore cannot create another block ID; unsigned authorization changes remain
committed. Invalid signature variants cannot permanently invalidate the ID.
The version 1 checkpoint vote is exactly 229 bytes in this order:

| Field | Size |
| --- | ---: |
| version | 1 byte |
| bond outpoint | 36 bytes |
| snapshot epoch | 8 bytes |
| source epoch | 8 bytes |
| source checkpoint root | 32 bytes |
| target epoch | 8 bytes |
| target checkpoint root | 32 bytes |
| head slot | 8 bytes |
| head block root | 32 bytes |
| vote signature | 64 bytes |

These primitives are initially implemented independently of block acceptance. A node must not accept a PoS block merely because these objects deserialize; semantic checks, cryptographic verification, canonical collection ordering, committed roots, and activation rules remain mandatory later in Phase 2.
Cheap structural validation rejects unsupported versions, null bond outpoints, slot zero, null snapshot or seed commitments, non-compressed VRF key prefixes, all-zero cryptographic values, reversed source/target epochs, and incomplete vote heads before cryptographic verification. Vote vectors are bounded before iteration, sorted by bond outpoint and then tagged vote hash, and reject duplicates. Passing these checks does not establish key validity, signature validity, VRF validity, eligibility, snapshot membership, or finality correctness.

### Finality Safety Evidence

Consensus detects and applies lockouts for all objectively provable violations:

- two valid block authorizations by one bond for the same slot, including competing parents;
- two checkpoint votes by one bond for different targets in the same target epoch;
- a later vote whose source and target epochs surround an earlier vote; and
- a later vote surrounded by an earlier vote.

Evidence contains both conflicting signed messages, is canonically ordered, and is applied only after inclusion in a valid block. Malformed evidence cannot trigger a lockout. Relay identity commits to both unsigned conflicting messages, so distinct offenses propagate independently while signature-byte variants cannot allocate additional identities. Consensus stores one monotonic high-water record per bond for block offenses and one for vote offenses. A later slot or target epoch renews the penalty, while an old offense cannot be replayed to renew an expired lockout. The fixed-size high-water records remain until the corresponding authenticated bond history is safely pruned.
Version 1 block equivocation evidence is exactly 547 bytes: one version byte followed by two 273-byte block authorizations. Both authorizations must have the same nonzero network identifier, bond outpoint, and slot but different candidate header signing hashes; the parents may differ because signing two branches in one slot is still equivocation. Version 1 vote equivocation evidence is exactly 459 bytes: one version byte followed by two 229-byte checkpoint votes from the same bond. It proves either different target roots for one target epoch or a strict surround relation. Each pair is sorted by its signature-independent signing hash and duplicates are rejected before signature verification. The final-PoW source epoch is ordered before ordinary epochs in surround comparisons.

### Committed State

Each PoS block commits through its coinbase-like transaction and header Merkle root to:

- the bonded-stake snapshot root used for eligibility and vote weight;
- the checkpoint-vote root;
- the epoch seed;
- the canonical stake-proof hash; and
- any applied equivocation-evidence root.

Nodes independently derive and verify every root. Peer-provided totals, weights, and roots are never authoritative.

### Crash-Safe State

Bonding, unbonding, eligibility lockouts, latest votes, justified and finalized checkpoints, epoch seeds, and stake snapshots are updated atomically with block connection. Every reversible state transition has deterministic disconnect undo data. Reindex and replay from blocks must reproduce byte-identical consensus state and roots. Database caches are never a source of consensus truth. Full validator snapshots, epoch seeds, and latest votes are retained for eight epochs; retired bond keys, evidence replay markers, and expired lockouts are removed only after the unbonding safety window makes them unnecessary. Skipped-epoch create-and-prune transitions are covered by reversible undo records. Pre-snapshot blocks do not store unnecessary PoS undo, and undo records at or below a newly finalized checkpoint are erased atomically because consensus forbids disconnecting finalized history. Verification still checks old block data but stops memory-only disconnects at finality.

### Layered Verification

Validation orders inexpensive checks before cryptographic work: activation and version, canonical lengths, count limits, parent availability, slot bounds, sorted uniqueness, known bond, maturity, and snapshot membership precede VRF and signature verification. Per-peer accounting limits repeated invalid proofs, votes, evidence, and duplicate announcements. Batch verification may optimize valid traffic but must fall back to individual verification and must never change acceptance results.

Consensus permits at most 16 bond outputs in one transaction. There is no
first-come global active-bond ceiling: such a ceiling could be filled to deny
later users access to staking and could make otherwise valid mempool
transactions poison block templates at the boundary. Transaction and block
size limits bound immediate parsing work, while minimum bond value and the
on-chain transaction fee make persistent registry growth economically costly.

After two distinct, fully processed block authorizations establish that one
bond signed conflicting candidates for the same slot, nodes retain and relay
the canonical evidence but ignore third and later candidates for that
bond-slot pair. The throttle applies equally to normal and reconstructed
compact blocks and is pruned with the authorization retention window.

### Security Test Gate

Phase 3 includes fixed cross-platform vectors, round-trip and differential serialization tests, fuzz targets for every PoS parser, malformed cryptographic proof tests, restart/reindex/disconnect tests, simulated partitions, concurrent slot winners, delayed and conflicting votes, finality stalls, long-range forks, and resource-exhaustion tests. A public testnet release is blocked while any consensus vector differs across supported platforms.
## Networking And Denial-of-Service Limits

### Independent Checkpoint Vote Relay

Checkpoint votes use inventory type `MSG_POS_VOTE` and the `posvote` payload
command. The payload is exactly the canonical 229-byte version 1 checkpoint
vote; nodes reject any other payload length or trailing bytes before signature
verification. Unknown inventory types remain harmless to pre-PoS peers, so this
relay extension does not alter pre-activation consensus or require old nodes to
understand votes.

Nodes admit independently relayed votes only after structural validation,
required-snapshot selection, bond membership and eligibility-lock checks,
known source and target checkpoint checks, a fully validated known head, and
Schnorr signature verification. A per-peer token bucket allows a burst of 256
votes and refills 64 votes per minute. Invalid structure, signatures, or wire
sizes increase peer misbehavior; stale or temporarily unknown chain-context
votes are ignored without punishment. Exhausting processing credits also drops
votes without increasing misbehavior: an honest peer may relay many validators'
votes at once. The CPU budget and malformed-message penalties remain in force.

The non-consensus vote pool stores at most 4,096 votes and 2 MiB of serialized
vote data, retains one latest vote per bond, and expires votes more than three
epochs behind. Duplicate and regressing votes are ignored. Conflicting votes
for one target epoch are not allowed to replace the pool entry. Block producers select
at most `MAX_VOTES_PER_BLOCK` (1,024) votes in canonical order and revalidate every
pooled vote against their current chain state before including it. Pool policy
does not change block validity.

#### Vote Heads and Partition Recovery

A committed vote authenticates two distinct claims: a source-to-target
checkpoint link and a `(head_slot, head_block_root)` fork-choice preference.
The checkpoint link is verified against the containing branch's authenticated
PoS state, delayed stake snapshot, eligibility locks, and Schnorr signature.
The head slot must not precede the target checkpoint or exceed the containing
block's slot. Naming the target itself requires its exact slot, and a different
head cannot claim that same slot.

Block validity must not depend on whether the receiver has seen a losing
branch's headers. A correctly signed checkpoint link can therefore remain valid
when its head names an unknown or orphaned block. Such a head carries **zero
fork-choice weight** for a candidate unless its hash and signed slot match an
ancestor of that candidate. Fork-choice head resolution searches only the
candidate's ancestry, not the global collection of previously received headers.
Every fully branch-validated PoS alternative is admitted to the candidate set,
including alternatives arriving later or at a lower height. The PoS selector,
not the old height/arrival-order comparator, decides between them. Finality and
branch-tip pruning still apply.
At the activation boundary, header relay defers the first PoS body until the
PoW anchor is script-validated, then requests the next header as that anchor
connects. A stored PoS body that was previously deferred can also continue
header download after successful revalidation.
Receiving an already-validated PoS header also continues from that header,
since initial/reconnect locators intentionally start one header back. A peer's
last continuation hash suppresses repeated identical requests.
External block-file imports
activate the available parent state before each PoS body instead of repeatedly
replaying all pre-activation history while leaving the active tip at genesis.
New block producers include only votes whose heads resolve on their parent
branch. Historical block validation does not require orphan-header downloads.
This is a correction to the experimental PoS validation/fork-choice rules;
all PoS testnet participants must upgrade together. It does not change PoW
validation, transaction serialization, or the PoS wire format.

Nodes that rejected a block under the old `bad-pos-vote-ancestry` check retain
that invalidity record across restart. After upgrading, an operator can use
`reconsiderblock <first-incorrectly-rejected-block-hash>`. Reconsideration fully
revalidates stored PoS blocks before restoring their validation marker and
candidate status; it does not bypass signatures, stake rules, or finalized
checkpoints. The October 22 testnet incident's first rejected block was
`a4770fd8a99ba52d8e4277fbea756bb5cd441df2f26c45c6edd5c662b71c3408`
at height 3203. Invalidity is not automatically cleared on startup, preserving
deliberate operator invalidations. No chain-data wipe is required for this
recovery when the needed block data remains available.

Regression coverage includes signed links with unavailable orphan heads,
signature and slot rejection, branch-local fork weight, independent producers
partitioning and reconnecting, a fresh observer crossing activation, bounded
vote-relay bursts without false bans, malformed-message penalties, explicit
reconsideration, restart, and chainstate reindex. Test fixtures use regtest bond
activation parameters and the current signed-extension header format.

Vote-equivocation evidence uses inventory type `MSG_POS_VOTE_EVIDENCE` and the
`posvevid` command with an exact 459-byte payload. Nodes stage at most 256
records and 256 KiB, admit only canonical conflicts with known snapshot bonds
and two valid signatures, and apply a separate burst/refill budget of 32 and 8
records per minute per peer. Existing valid evidence is not evicted to admit
new traffic. When two independently admitted votes form canonical double-target
or surround evidence, the node constructs and relays the evidence automatically.
Producers revalidate and include at most 16 records, removing them from the
local pool only after the containing block is accepted.

Block-equivocation evidence uses inventory type `MSG_POS_BLOCK_EVIDENCE` and
the `posbevid` command with an exact 547-byte payload. Nodes retain one bounded
authorization per bond and slot, automatically construct evidence when a
second valid authorization conflicts, verify both signatures against retained
bond history, relay the evidence, and stage at most 256 records or 256 KiB.
On restart, the authorization cache is reconstructed from up to 4,096 recent
fully validated active-chain PoS blocks so restarting during a partition does
not erase the local side of a later equivocation proof.
Producers revalidate staged evidence before inclusion and remove it only after
the containing block is accepted.

Before expensive signature or kernel checks, nodes must enforce:

- bounded proof and signature serialization;
- one canonical stake proof format;
- cheap activation-height and version checks;
- known-parent checks;
- slot and timestamp bounds;
- minimum encoded stake value checks; and
- per-peer limits for invalid or duplicate PoS announcements.

Nodes must validate the referenced UTXO against the parent state and never trust stake value, age, target, score, or finality weight supplied by a peer.

## Operational Participation Targets

Wallet count is not a consensus identity and cannot be enforced because one operator may control many keys. Release readiness is evaluated using observed independent operators and bonded-stake concentration.

- regtest development targets at least 3 to 5 staking wallets;
- private integration testing targets 10 to 20 wallets;
- public testnet targets at least 50 independently operated staking nodes; and
- mainnet readiness targets at least 100 independent operators, preferably several hundred, with more than 70 percent of bonded value reliably online.

Mainnet readiness must report the largest observed bond/operator concentration, the largest ten concentration, occupied-slot rate, competing-winner rate, vote participation, finality delay, and recovery from partitions. These are release gates, not claims that on-chain keys prove human identity.

The chain-level `getposinfo` RPC exposes activation and synchronization state,
slot and epoch parameters, tracked and eligible bond totals, concentration
indicators, snapshot roots, vote participation, justified and finalized
checkpoints, trusted-checkpoint configuration, local clock state, and aggregate
plus per-peer synchronization diagnostics. `getbondinfo` reports the public
consensus status of one bond outpoint. The wallet-scoped `listbonds` RPC reports
only bonds controlled by the selected wallet, including maturity, eligibility,
lockout, owner destinations, and latest-vote state. `listunbondings` reports
wallet-controlled unbond outputs and their remaining consensus lock duration.
These reporting RPCs do not alter consensus or wallet policy.
`getblockchaininfo`, `getposinfo`, and `getstakinginfo` expose the service
readiness state directly; the wallet report also distinguishes missing peers
from chain synchronization.

The one-way initial-block-download latch is not sufficient to authorize
network services after a node has synchronized once. Staking, automatic
bonding, secure messaging, and externally served mining templates additionally
require a fully selected active chain. Before PoS, the active chain must
contain the best known validated header. After PoS, readiness follows the
candidate selected by the existing PoS fork-choice rules and requires the
active UTXO-backed PoS state to match that candidate. A runtime-only readiness
marker is cleared when a newly validated candidate needs fork-choice review,
when blocks are invalidated or reconsidered, and when state is reloaded. It
becomes usable only when chain activation reaches the selected tip. Valid
losing branches, including equal-height branches received earlier, cannot
leave staking permanently paused as synchronizing. These checks do not alter
fork choice, finality, or block validity, and they perform no branch replay
on each staking readiness query.
Staking and automatic bonding also require at least one peer outside regtest. These
checks disable the affected service again if the node learns that its active
chain is behind, while avoiding a tip-age rule that could prevent an otherwise
valid stalled PoS network from recovering.

Local block production binds the selected stake proof, votes, and evidence to
the exact parent hash used during selection. Under the chain and mempool
locks, the assembler discards the attempt before transaction selection or
block signing if the active parent changed (including an equal-height tip
replacement), or if the selected timestamp no longer advances the parent.
The automatic worker treats this as an ordinary unsuccessful attempt and
retains its one-attempt-per-slot policy. It does not re-sign the same slot
against another parent. This is producer-side race handling only: block
validation, slot eligibility, fork choice, and finality rules are unchanged.

## Crash Recovery And Durability

PoS state and its transition undo are journaled atomically in the block-index
database. UTXOs are cached separately. Advancing finality must durably flush
block files, undo files, block-index entries, and the UTXO database before
deleting finalized PoS undo records. The final UTXO batch uses a synchronous
write, so a completed flush cannot be overtaken by later undo pruning.

When disconnecting the block named by the saved UTXO tip, its UTXO rollback
must be durably flushed before persisting the parent PoS state and erasing
the disconnected block's PoS undo. A crash before that step leaves the old
PoS state and the undo required to reconcile it with the saved UTXOs.
Ordinary connects and disconnects above the saved tip retain cached UTXO
updates; this does not force a disk flush for every block or competing winner.
Existing wallet and indexer notification ordering is preserved.

Interrupted multi-batch UTXO writes use the existing head markers and block
replay. Failed block/undo-file flushes stop the sequence before database
commits or undo pruning. Replay and normal UTXO flushes propagate write
failures instead of treating them as successful recovery. Recovery never
bypasses block validation or changes finality and fork-choice rules. No
database format change or routine
reindex is required by this hardening. A database already missing required
undo data from an older build may still require a one-time chainstate rebuild.

`feature_pos_crash_recovery.py` covers abrupt termination after finality
advances, recovery of an unflushed suffix, interrupted finality and disconnect
flushes, a repeated crash during startup replay, state/UTXO agreement with an
independent node, and continued block production without reindexing.
Unit coverage also checks that a failed block-file flush cannot advance the
durable UTXO tip and that a later successful flush can complete normally.

## Wallet Behavior

Staking is opt-in. Merely owning XVG or opening a wallet does not automatically expose keys or begin staking. The wallet must show:

- eligible and currently locked balances;
- staking enabled/disabled state;
- staking-key status;
- estimated selection probability without promising rewards;
- last attempted and accepted slots; and
- earned fee rewards and their maturity.

The wallet options expose the automatic-staking policy, liquid reserve, owned
bond totals, confirmations, eligibility state, and full outpoints. Users may
begin unbonding selected complete bonds to reduce their bonded balance or
disable automatic staking and unbond every currently withdrawable bond. A bond
cannot be partially spent; exact stake reductions therefore depend on bond
sizes, and released principal remains locked for the consensus unbonding
period. GUI unbond actions disable automatic staking before creating the
transactions so the wallet cannot immediately replace the selected bonds. The
GUI and RPC use the same wallet transaction implementation.

The bond table sorts numerically by amount, confirmations, or blocks to unlock
when its column headers are clicked. Refreshes preserve the chosen sort order
and selected outpoints. Transactions and recent overview entries show a green
Confirmed badge after inclusion in the active chain, or a red Unconfirmed badge
before inclusion. Conflicted, abandoned, and unaccepted transactions retain
distinct red labels. These badges do not indicate PoS finality or coinbase
maturity; the transaction details retain that additional information.

Wallet locking, backup, encryption, hardware-wallet behavior, and delegated staking require explicit tests.

## Required Test Coverage

Phase 3 requires fixed vectors and adversarial tests for:

- activation boundary and rejection of post-activation PoW;
- two-way transaction replay protection across the PoW/PoS boundary;
- activation-boundary mempool revalidation and transaction recreation;
- pre-activation compatibility;
- exact 1,000 XVG and 720-confirmation boundaries;
- kernel and target arithmetic;
- stake-weight linearity and split/merge equivalence;
- signature domain separation and malformed encodings;
- principal and fee conservation, including empty blocks;
- spent, immature, missing, and competing stake outpoints;
- duplicate slots, missed slots, clock drift, and timestamp grinding;
- short and long reorganizations;
- finality and conflicting votes;
- restart, reindex, pruning, and wallet recovery; and
- cross-platform deterministic vectors.

## Phase Gates

### Phase 2 Implementation Status

The regtest implementation now includes canonical PoS block-extension
serialization, bounded decoding, bond and unbond lifecycle scripts, tagged
consensus hashes, BIP340 authorization helpers, RFC 9381 P-256
ECVRF-P256-SHA256-SSWU proving and verification, stake snapshots, reversible
stake-state transitions, committed vote/evidence/stake roots, fee-reward
destination checks, and overflow-safe stake-weighted VRF eligibility.
The ECVRF implementation is checked byte-for-byte against the official RFC 9381
Examples 13 and 14. The initial seed commits to the network identifier,
activation height, genesis hash, and the ordered post-snapshot PoW block-hash
accumulator. Later seeds commit to the previous seed, next epoch, delayed
snapshot root, and the completed epoch's accepted VRF-output accumulator.
Validator membership is fixed before either entropy window completes.

Regtest PoS block acceptance now uses an explicit caller-owned state view.
`ConnectTip` atomically persists the resulting state and block undo record;
`VerifyDB` and `TestBlockValidity` use disposable copies and cannot mutate the
active PoS state. Restart loading requires the persisted state tip to match the
UTXO chainstate tip, except that an ahead state may be rolled back through
authenticated ancestry and stored undo records. Before the initial snapshot,
an upgraded node may initialize unspent bond records and their creation heights
from the UTXO set. Skipped epochs deterministically create each intermediate
snapshot and seed, and all such changes are covered by disconnect undo.

Full semantic block validation now checks the derived epoch seed, delayed
snapshot, bond credentials, RFC 9381 proof and output, weighted eligibility,
BIP340 block signature, vote signatures, historical evidence signatures,
committed roots, reward destination, and the fee-only reward ceiling. Regtest
production and deterministic restart, disconnect, and reconnect coverage are
implemented. The linked Windows binary has passed the complete clean-chain
functional path from block zero through bond creation, the height-1500
transition, three PoS epochs, restart, tip invalidation, and reconsideration.
Regtest honors its no-retarget setting throughout the pre-activation chain.
PoS-aware block-index loading, disk reads, and header RPC output are covered by
that restart path. The PoS header's signed-extension commitment is persisted in
the block index, and only fully branch-validated PoS blocks enter fork choice or
best-header state. Equivocation evidence is persisted by bounded canonical
evidence ID so replay is idempotent. Valid evidence immediately prevents withdrawal, then at
the first block of the next epoch removes the offender's latest vote and
excludes the bond from production, voting, and new snapshots for 20,160
accepted blocks. Lockout activation, expiry, evidence IDs, and vote removal
are all covered by disconnect undo records. LMD-GHOST candidate selection
is implemented over full-data block-index candidates replayed against their
own UTXO and PoS state. Each bond's latest valid vote contributes its
delayed-snapshot weight to the ancestry of its voted head within that branch
view. Each tree edge takes the maximum authenticated branch-view support,
then descendant height and child hash break ties. Candidate pruning retains
shorter PoS forks that may have greater stake support. Until a real
supermajority link establishes finality, competing last-PoW ancestors remain
eligible under the deterministic transition-ancestor rule.

Wallet production now has a shared single-attempt path used by both the
dedicated automatic staking worker and the `generatestake` regtest control.
The staking worker runs independently of the scheduler that delivers validation
callbacks: block activation may wait for that queue to drain. Shutdown signals
and joins the staking worker while the scheduler, script verification workers,
wallets, and networking are still available, before tearing those services down.
Restart restores each header's `hashPoSData` commitment from the block index,
preserving the block ID used for header announcements and block downloads.
During initial download, an empty headers reply releases that peer's initial
sync slot so a later connected peer can provide the chain. The empty peer stays
connected and can be retried after 60 seconds, avoiding a tight request loop.
Already-known valid headers still update per-peer block availability, so a
preferred outbound peer can supply block bodies first announced by another peer.
Each wallet is evaluated at most once per canonical slot. The producer derives
skipped-epoch seeds and snapshots through the same state transition used by block
connection, matches snapshot credentials to encrypted wallet keys, creates
and locally verifies the RFC 9381 proof, selects fee-paying mempool packages,
commits the PoS extension in the fee-only reward transaction, fixes the Merkle
root and candidate header hash, and signs only after all committed fields are
final. Automatic voting targets the latest already-known checkpoint and uses
the current parent as the LMD-GHOST head, avoiding a self-referential vote-root
and header-hash cycle.
Votes are transported both through validated PoS blocks and independently by
inventory-based relay. The bounded vote pool, per-peer admission budget,
standalone chain-context validator, wallet vote announcements, and block-template
revalidation are implemented. Conflict detection constructs canonical vote
evidence, while unit coverage verifies conflict identity, ordering, admission,
and removal. The two-node functional path verifies vote delivery, malformed
vote/evidence penalties, partition convergence, restart, chainstate reindex,
and disconnect/reconsider recovery.
The final PoW bootstrap checkpoint, latest-vote aggregation, monotonic vote
freshness, bounded historical state, configurable weak-subjectivity checkpoint,
reversible
justification/finalization, and finalized-reorganization barrier are consensus
state. Deserializing a PoS extension or passing structural checks alone never
establishes validity.

Phase 2 is complete for regtest. Fixed cryptographic and serialization vectors,
state transition and undo tests, deterministic fork weight and hash tie-break
tests, evidence-lock transaction tests, and the end-to-end functional path are
present. Phase 3 now includes independent vote and vote-evidence relay, bounded
pools, exact-size malformed-message checks, a two-node partition/convergence
scenario, restart plus chainstate-reindex coverage, signed-extension block
identity, full-branch prevalidation, stale-vote rejection, and deterministic
state pruning. Sustained resource-exhaustion testing and independent consensus
review remain mandatory before mainnet activation.

1. Phase 1 ends only after all open consensus decisions in this document are resolved and reviewed.
2. Phase 2 implements regtest-only activation and staking; mainnet behavior remains byte-for-byte unchanged.
3. Phase 3 adds deterministic and adversarial tests before public testing.
4. Phase 4 operates a public PoS testnet for months and records participation, forks, missed slots, and recovery events.
5. Phase 5 requires independent consensus and cryptographic review with findings addressed publicly.
6. Phase 6 begins only after readiness review, complete post-fork transaction
   replay protection, and independent verification of its fixed vectors;
   production activation height is selected last.
## Resource And Correctness Review (2026-11-03)

This is maintainer remediation and test coverage, not an independent audit.
The existing master-branch security report is not a PoS audit. Mainnet activation
continues to require independent consensus and cryptographic review.

### Implemented Hardening

- A block has one combined budget of **16** block/vote evidence objects and
  **1,024** votes. Local assembly selects vote evidence first, then uses only
  the remaining evidence capacity for block evidence. Previously, independently
  filling both pools could produce a locally invalid staking template. The
  authoritative limits are `pos::MAX_EVIDENCE_PER_BLOCK` and
  `pos::MAX_VOTES_PER_BLOCK`; the unused chain-parameter copies were removed.
  Wire decoding and accepted block limits are unchanged.
- Vote replacement checks capacity and allocates the new entry before deleting
  the previous one. New admission rolls back the vote if allocating its bond
  index fails. Tests cover a full pool and its accounting after replacement.
- Staking-slot and automatic-bond throttles belong to each wallet instance,
  instead of process-global maps keyed by wallet addresses. Unbond enumeration
  has an explicitly locked helper to avoid its redundant outer lock acquisition.
- PoS batch handlers check peer-state pointers before use. A non-authenticated
  result falls back to normal synchronization without treating the mere handler
  return value as misconduct. `ProcessMessages` returning false is not itself
  an automatic disconnect in this codebase; actual invalid-block scoring remains
  in force. Malformed framing, invalid signatures and abusive traffic still
  receive their existing penalties.
- PoS inventory hashes no longer trigger the wallet transaction-inventory
  callback. This was unnecessary accounting work, not demonstrated coin theft.
- Signing uses a randomized thread-local context, retaining per-signature
  auxiliary randomness and clearing temporary keypairs. Tests compare the
  fixed Schnorr vector across repeated operations in four threads.

### Fork-Choice And State Costs

Successful branch validation caches only authenticated `(head height, weight)`
summaries, never a full branch state or unauthenticated header claims. Summaries
preserve the bond iteration order and money-range rule. Fork selection projects
those branch-local summaries into a deterministic tree, using maximum edge
support rather than an order-dependent pairwise comparison. Off-chain relay
votes are not a substitute for branch state.

The process-local LRU cache is limited to 128 entries and 16 MiB of accounted
allocation usage, including vector capacity and container/shared allocation
overhead. Oversized entries are evaluated without retention. Eviction removes
only an optimization, never a chain candidate. Cache hits still check failed
ancestry and available branch data. Invalidation, reconsideration, state loading
and block-index teardown clear the cache. Existing finalized-ancestry checks
remain authoritative. A cache miss still replays that candidate's branch.

`BLOCK_VALID_POS` publication now belongs to admission/reconsideration after the
entire replay succeeds, not fork-choice evaluation or a partially failed replay.
Read-only network validation, chain RPCs and balance reporting use a state
reference held under `cs_main`. Block connection consumes the caller's disposable
state copy, avoiding a second complete copy while keeping active state isolated
from failed validation.

Incoming active-tip successors authenticate known bond credentials, maturity,
snapshot membership, authorization and VRF eligibility against a read-only parent
context before copying full state. Skipped empty epochs stream their seed and
delayed-root recurrence, rebuilding membership only at an eligibility change and
materializing full snapshots only inside the retained state window. This keeps
accepted roots and undo deterministic without allocating one validator vector
for every empty epoch.

Full-body PoS admission has per-peer and global work budgets, weighted by bond
count and alternate-branch replay distance. Active-tip and alternate-fork traffic,
and inbound and outbound connections, have separate global allowances. These
are local resource policies, not consensus validity rules: exhaustion retains a
bounded deferred header and retries later without banning the sender or marking
the block invalid. All body transports share this accounting.

`submitblock` belongs to the chain-level mining RPC interface. A fully signed PoS
block can be submitted with a locked wallet or `-disablewallet`; it never invokes
legacy wallet block signing. Already-signed PoW blocks also require no wallet.

Finality link tallies are derived runtime data. New/replaced votes update their
links incrementally; snapshot changes, vote pruning, lockout vote removal,
undo and deserialization invalidate them for reconstruction. The cache is not
serialized. Differential unit coverage compares warmed and freshly deserialized
states with 2,048 bonds through batched votes, replacements, skipped-epoch
pruning and undo. The partition test accepts `--reference-binary` to compare an
older daemon with the new one through competing production, convergence,
fresh sync, missed epochs, reconsideration, restart and chainstate reindex.
Benchmark logging records branch replay time, replayed blocks and retained
summary bytes (`-debug=bench`, or `logging ["bench"]` in the RPC console).

### Local Verification

The Windows MSVC-compatible build passed a full daemon/test-object rebuild.
All **79 selected PoS and wallet unit tests** passed, including fixed signing
vectors, full-pool replacement, all 17 permitted splits of the evidence budget,
cache eviction/oversize handling, wallet attempt lifetime, and tally rebuild
equivalence. The new fork-choice header also compiled as the first project
include in its test translation unit.

The final build passed these functional scenarios:

- `feature_pos_vote_heads.py`, with a pre-change reference daemon as the second
  producer, comparing chain/state hashes, initial snapshot, vote totals,
  justification and finality through partition, convergence and recovery.
- `feature_pos_crash_recovery.py`, including repeated abrupt exits and interrupted
  UTXO flushing/startup replay.
- `feature_pos_activation.py` and `feature_pos_worker.py`.
- `p2p_pos_prefetch.py`, including malformed/authentication failures, benign
  future-slot handling, bounded historical sync and reindex.
- `p2p_pos_header_time.py` and `wallet_pos_unbond.py`.

One local 2,048-bond measurement took 89 us for 100 warmed empty-vote updates
versus 82,698 us for tally rebuilding alone. Deserialization and byte comparison
are excluded from the latter measurement. These are focused test timings, not
mainnet throughput or a sustained DoS-resistance benchmark. Sanitizers, a
large-scale resource test, Pi-class measurements and overnight soak were not
performed in this local verification. No live wallet data was modified.

### Remaining Mainnet Gates

1. Incremental record persistence and local migration/recovery coverage are
   implemented below. Repeat this coverage on release platforms before
   deployment, including backups and operator downgrade/rebuild instructions.
   The recovery ordering established in the crash-recovery work must not be
   weakened to reduce writes.
2. The larger disk-state and summary-cache tests below are not a full-node
   100k-validator benchmark. Cold branch replay, retained eligible candidate
   count, state copies, long histories and many authenticated competing forks
   still require profiling under finalized and stalled-finality conditions.
   Record CPU, peak RSS, lock hold times, validation latency and physical
   database bytes written. Repeat on desktop and Pi-class hardware, with
   sanitizer runs and an overnight partition/recovery soak.
3. Give an independent reviewer a pinned commit/build manifest, this consensus
   specification, signature/VRF/replay-protection vectors and test commands.
   Scope includes weighted eligibility and bond-splitting invariance, entropy
   manipulation, branch-local vote accounting, finality safety under partition,
   evidence censorship/replay, lockouts and withdrawal recovery, historical
   synchronization, resource limits, and crash consistency. Publish findings,
   remediations and a reviewer retest; a maintainer test pass is not this gate.

No activation height, network identity, signature construction, accepted block
limit or serialized state/undo format changes in this remediation stage.

## Incremental Persistence And Soak Work

The incremental implementation changes the local LevelDB schema, not transaction,
block, signature, snapshot, or undo serialization. Legacy state under key `P`
is readable until the first new write migrates it in one batch. Schema version
1 uses metadata key `p` and record prefix `q`. A record key is the prefix byte,
field byte, 32-byte identifier and uint32 output index. Epoch keys encode the
uint64 epoch in the identifier's first eight little-endian bytes; the remaining
identifier bytes and output index must be zero.

Field numbers 1 through 11 identify bonds, historical bonds, snapshots, seeds,
epoch entropy, checkpoints, justified checkpoints, latest votes, lockouts,
applied evidence and retired bonds, respectively. Metadata contains version,
state tip, initial entropy, PoS flag, last epoch, finalized checkpoint and the
eleven map counts. Reads reject unknown versions/types, non-canonical keys,
missing metadata, mismatched record counts, and mixed legacy/new state.
Values use the corresponding existing state-map value serialization. A future
incompatible record/metadata change must use a new database schema version;
an unknown version is rejected before parsing its remaining metadata.

`StateChanges` collects affected keys from existing undo records. Connection
writes only those keys and metadata, plus the block undo, in one atomic batch.
The shared UTXO cache is updated only after that batch succeeds, so failure cannot
make shutdown persist UTXOs whose matching PoS state was never committed.
Snapshot creation still writes the new snapshot once per epoch. Disconnection
uses the same key set and resulting parent state. Startup recovery merges key
sets across its rollback suffix without retaining copies of every removed
snapshot, verifies undo continuity and atomically commits the recovered state
while deleting precisely that suffix's undo records. Full resets are reserved
for initialization, explicit rebuilding and pre-snapshot recovery without undo.

At and after creation of the initial snapshot, state/undo batches are synchronous.
The snapshot-creation block now retains undo too, closing the boundary where
the pre-snapshot reconstruction shortcut is no longer sufficient. Finality
still flushes blocks, indexes and UTXOs before pruning recovery undo. Earlier
state batches remain asynchronous because that state can be rebuilt from UTXOs.
Disconnecting the durable UTXO tip flushes its rollback before writing parent
PoS state, including legacy snapshot-creation blocks that have no PoS undo.
The block-index unload path now also clears in-memory PoS state; otherwise its
old tip can incorrectly survive rebuilding into a fresh database.
An empty-UTXO rebuild explicitly resets PoS records before replaying genesis;
the reset refuses an active chain or a nonempty UTXO tip. Ordinary writes never
discard their persisted-parent check to accommodate rebuilding.

Upgrade does not require wiping chain data. In-place downgrade is not supported:
an older binary cannot read record-format PoS state after the snapshot and must
not be used to write alongside it. Restore a matching pre-upgrade backup, or
perform the appropriate explicit rebuild with a compatible binary. Never remove
wallet files to change the chain database format.
In particular, `-reindex-chainstate` alone is not a downgrade procedure: it does
not clear the PoS records in the block-index database. Both reindex modes are
tested with the upgraded binary; after the initial snapshot, a normal
post-migration startup with the previous binary fails rather than silently
reinterpreting the data. Before that snapshot, an old binary can instead
reconstruct legacy state; a later upgrade rejects the resulting mixed formats.
Neither behavior is a supported in-place downgrade.

Verification includes record-versus-normal-state serialization hashes, all map
families across evidence/retention/undo, uncommitted-batch interruption, malformed
schema refusal, durable reopen, multi-block rollback and real binary migration.
Set `VERGE_POS_STORAGE_STRESS=1` to include disk-backed 1k/10k/100k-bond cases in
`pos_storage_tests`. Batch byte figures are encoded size estimates, not physical
disk write amplification or complete block-validation throughput.

`feature_pos_soak.py` is an extended four-node test with configurable rounds,
minimum duration, bond count and random seed. It exercises competing production
under repeated partitions, convergence, clean/abrupt restarts, delayed catch-up,
record-state recovery and final chainstate reindex. It compares consensus state,
UTXO hashes, readiness and honest-peer scores, and records observed resident
memory plus a JSON results artifact. A local bounded soak does not substitute
for an overnight run, hardware-specific profiling or independent review.

The first longer soak exposed an outbound announcement mismatch: queued tip
announcements could include multiple PoS headers, although the receiving path
permits only one terminal PoS header. Such outbound batches now fall back to
tip inventory; bounded full-block synchronization handles catch-up. The
receiver's single-header authentication bound and penalties for malformed
multi-header announcements remain unchanged.

### Larger Local Verification

All 87 selected PoS/wallet unit cases pass on the Windows MSVC-compatible
build, including failed-write UTXO isolation, snapshot-boundary undo and legacy
rollback, all record families, schema rejection, durable reopen, and ordered
64-block recovery. Extended disk-backed measurements were:

| Bonds | Full Encoded Batch (Bytes) | Maximum Delta With Undo (Bytes) | 64 Durable Writes (us) |
| --- | --- | --- | --- |
| 1,000 | 469,731 | 1,047 | 33,949 |
| 10,000 | 4,690,731 | 1,047 | 46,997 |
| 100,000 | 46,900,733 | 1,047 | 37,066 |

Each measured transition updates one vote plus entropy/metadata and its undo;
the combined 64 delta batches total 67,008 encoded bytes at each size. Bond
churn, large vote sets and new snapshots require proportionally larger writes.
These timings cover the durable write call, not state construction, signature
verification, total validation latency or physical disk amplification.

### Local Signing Safety

Before releasing a block or checkpoint-vote signature, the wallet commits a
`possigning` safety record with a synchronous database transaction. Records are
scoped to the network, genesis, activation height, signing key and bond outpoint.
They prevent conflicting same-slot blocks, backward slot signing, double-target
votes and surround votes by conservatively retaining monotonic signing history.
Repeated identical messages and safe later-head updates remain permitted. These
are local signer restrictions, not additional consensus acceptance rules.

A failed journal write stops further signing until restart. Corrupt or unknown
journal records are fatal wallet-load errors, not records that can be skipped.
Wallet recovery retains valid signing records. **Never run copies of an active
staking wallet simultaneously or restore an older backup while reusing active
bond signing keys.** A local journal cannot coordinate independent copies or
reconstruct signatures made after a backup was taken. Do not downgrade to a
staking build that ignores the journal. Preserve the latest wallet database and
its normal recovery files when migrating a staking node.
Protection starts when this signer first records a message; it cannot prove
what an older build previously signed. Coordinate upgrades on a single branch
before producing new votes, and use a clean trial for consensus-changing fixes.

The summary-cache stress inserts 256 synthetic branch summaries with 100,000
vote heads each into a 16 MiB accounted budget. It retains 10 summaries, peaks
at 16,001,920 accounted bytes, preserves reference weights, and evicts older
entries. The measured cache loop took 212,901 us. This tests cache accounting
and eviction, not replay/authentication of 256 real competing branches.

The final daemon build passes `feature_pos_activation`,
`feature_pos_crash_recovery`, `feature_pos_vote_heads`, `feature_pos_worker`,
`p2p_pos_prefetch`, `p2p_pos_header_time`, `wallet_pos_unbond`, and
`feature_pos_storage`. Persistence coverage runs both in default CI mode and
against the previous single-state-record daemon, including successful in-place
migration, explicit old-binary refusal after activation, retention pruning,
restart, full reindex and chainstate reindex with matching UTXO/state results.
The prefetch regression also confirms that malformed multiple-PoS-header
announcements still receive a penalty after the outbound announcement fix.

The four-node run with 32 real bonds per node passed 600.9 seconds of
steady-state soak and completed its final reindex/comparisons at 605.625 seconds.
It exercised 554 action rounds, 27 partitions, 8 clean restarts and 7 abrupt
restarts, producing 490 blocks including competing branches. All four nodes
converged at height 2442, with checkpoint epoch 82 finalized at height 2432.
Current peer scores and a complete log scan found no honest-peer penalties.
Consensus/snapshot comparisons and the final four-way UTXO hash comparison
passed, including the participant rebuilt with `-reindex-chainstate`.

Observed combined resident memory began at 387,448,832 bytes and peaked/ended
at 404,201,472 bytes (about 385.5 MiB) after final rebuilding. These are sampled
working-set totals, not private-memory or leak-freedom measurements. The local
artifact is `build-msvc/pos-records-final-soak/pos-soak-results.json`, marked
`passed`. This bounded desktop run does not complete the overnight, sanitizer,
Pi-class or full-scale authenticated-branch profiling gates above.

Run the disk/cache cases with `VERGE_POS_STORAGE_STRESS=1` and the PoS unit
binary. Run the network soak with:

```sh
python test/functional/feature_pos_soak.py --rounds=120 --duration=600 --bonds-per-node=32 --soak-seed=20261104
```

`--duration` is a minimum steady-state wall time in addition to `--rounds`;
setup and final reindex are extra. The activation override in this test is
regtest-only. For an overnight run, use `--duration=43200` in an isolated test
directory, not a live wallet directory. Results are written to
`pos-soak-results.json`; only a report marked `passed` includes the final
state/UTXO and reindex assertions. No live wallet data is modified.
These tests exercise process crashes and batch commit boundaries, not physical
power loss, torn device writes or storage hardware that fails to honor sync.

## Shared Runtime Memory Review (2026-11-05)

The daemon memory investigation and shared logging/scheduler hardening
add partial component counters to `getmemoryinfo stats` and a concurrent
mining-RPC resource regression, including walletless operation. A failed log
reopen no longer converts runtime logging to unbounded buffering, and callback
bursts no longer create redundant processing jobs. Callback ordering and
consensus rules are unchanged. The reported exhaustion on 16 GiB hosts still
requires affected-host evidence; bounded local tests are not a mainnet OOM
reproduction or an overnight leak audit.

## Documentation Discipline

This document is updated with each approved consensus decision. Implementation commits must reference the relevant section and must not introduce undocumented consensus behavior. Before public testnet, the draft must include exact byte serialization, domain-separation strings, arithmetic bounds, state-transition pseudocode, RPC behavior, and fixed vectors. Before Phase 6 it will be promoted from a research draft to a versioned consensus specification linked from the root README and release documentation.
