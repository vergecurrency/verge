<!-- Copyright (c) 2026 The Verge Core developers -->

# Verge PoS-Only Consensus Draft

Status: Phase 1 research draft for the `regtest` branch. This document is not a production activation proposal.

## Scope

Verge remains proof of work before a network-specific activation height. Beginning with the block at `nPoSActivationHeight`, proof-of-work blocks are invalid and every new block must satisfy the proof-of-stake rules below. Initial implementation and activation are restricted to regtest.

This transition does not mint new XVG. A valid PoS block may claim at most the transaction fees in that block. An empty block therefore pays zero.

## Confirmed Parameters

| Parameter | Initial value |
| --- | ---: |
| Target slot duration | 30 seconds |
| Minimum staking output | 1,000 XVG |
| Stake maturity | 720 confirmations |
| PoS subsidy | 0 XVG |
| Producer reward | Current block transaction fees only |
| Future-time tolerance | One 30-second slot |
| Staking authorization | Delegated staking keys |
| Principal maturity after staking | Preserved |
| Fee reward staking eligibility | 720 confirmations |
| Expected eligible producers | 1 per slot |
| Bonding | Explicit on-chain transaction |
| Delegation granularity | Entire bonded UTXO |
| Unbonding lock | 20,160 accepted blocks |
| Initial seed window | 120 pre-activation PoW blocks |
| Post-activation PoW fallback | None |
| Mainnet activation height | 15,000,000 |
| Testnet activation height | 15,000,000 |
| Regtest activation | Disabled unless -posactivationheight=<height> is set |

All amounts are evaluated in base units. A staking output exactly equal to 1,000 XVG is eligible.

## Activation

Each network has an explicit `nPoSActivationHeight`. Mainnet and testnet use height 15,000,000 in this development branch so the parameters are visible together and easy to review. These are real consensus activation heights, not comments or placeholders; this branch must not be merged into a release branch before its applicable phase gates are complete.

Regtest activation is disabled by default and requires `-posactivationheight=<height>`. The override is rejected on testnet and mainnet. Automated tests use small explicit heights, while manual regtest networks may choose an upcoming height.

## Deployment Portability

The same PoS consensus code path must be used on regtest, public testnet, and mainnet. Network classes provide parameters such as activation height, slot duration, maturity, minimum stake, target limits, checkpoint intervals, and deployment state; they do not select separate consensus algorithms.

Regtest may expose deterministic test controls and an activation-height override, but those controls must be unavailable on public networks. Promotion follows regtest to public testnet to mainnet by changing reviewed network parameters, not by copying or rewriting the implementation. Mainnet activation remains disabled by default until Phase 6.

Database records, block serialization, wallet staking records, RPC schemas, and P2P messages must be forward-compatible across those environments so testnet exercises the actual production format. A node must log the configured activation height and reject ambiguous or conflicting activation settings at startup.

Wallet staking is explicitly opt-in and disabled by default. `setstaking true`
persists the preference in that wallet, while locking an encrypted wallet pauses
signing without changing the preference. `createbond` generates a
wallet-encrypted delegated secp256k1 key, deterministically derives the separate
P-256 VRF key under `VergePoS/VRFKey/v1`, and requires wallet-controlled reward
and withdrawal destinations. `unbond` preserves the complete principal in the
consensus unbond script and funds its transaction fee from separate wallet
inputs.
At activation:

1. The block at height `nPoSActivationHeight - 1` is the final PoW block.
2. The block at `nPoSActivationHeight` must be PoS.
3. Bond and unbond transactions are valid before activation, allowing delegated stake to mature and enter the delayed snapshots while PoW still secures the chain.
4. A bond must satisfy the 720-confirmation maturity rule and appear in the stake snapshot selected two epochs before its first eligible PoS epoch. Merely bonding immediately before activation does not bypass either delay.
5. The initial epoch seed is the domain-separated SHA-256 hash of the network identifier, activation height, and ordered block IDs of the 120 PoW blocks immediately preceding activation. Networks without 120 predecessor blocks cannot activate.
6. Existing unbonded UTXOs remain spendable but are not eligible stake until explicitly bonded.
7. PoW difficulty and algorithm-frequency rules no longer determine validity after activation.
8. No emergency PoW fallback exists after activation. If no valid PoS producer is available, the chain pauses until an eligible producer returns.
9. Old nodes remain on incompatible PoW consensus. This is intentionally a hard fork.

## Block Representation

A PoS block retains the existing block header for compatibility with storage and relay infrastructure. A dedicated version bit identifies PoS. After activation, the five PoW algorithm version encodings are invalid for new blocks.
Historical blocks through activation height minus one retain their original header interpretation and proof-of-work validation. At and after activation, the PoS version bit is mandatory, proof-of-work blocks are invalid, and both nBits and nNonce must be zero. The in-memory null-header sentinel uses nVersion equal to zero so a valid PoS header with zero nBits is not mistaken for an empty object. PoS timestamps must be exact 30-second slot boundaries and must map to a slot strictly later than the parent block's slot; skipped slots remain valid.

The first transaction remains structurally coinbase-like so existing block indexing can be adapted incrementally. It creates no subsidy and may pay at most the block's transaction fees to the owner-controlled reward destination committed by the selected bond.

A consensus-defined stake proof references exactly one bonded UTXO without spending or recreating it. The proof commits to the selected outpoint, slot, stake snapshot, VRF output and proof, delegated staking public key; block authorization and checkpoint votes are serialized separately. Principal therefore remains locked in its original bonded output while producing blocks.

The exact serialization must be domain-separated and covered by fixed test vectors before Phase 2 is considered complete. Witness serialization is not used.

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

The coinbase-like transaction commits to tagged roots for the stake proof, vote vector, evidence vector, stake snapshot, and epoch seed. The extension is invalid if any recomputed root differs. Full-block validation also requires the authorization network identifier (mainnet 1, testnet 2, regtest 3), parent root, canonical global slot, candidate header hash, bond outpoint, tagged stake-proof hash, and fee-reward transaction hash to match the block and stake proof exactly. These checks occur when the full block is available because the compatibility header does not serialize the extension.

## Stake Eligibility

An outpoint is eligible for a candidate block when all of the following are true at the parent block:

- it exists and is unspent;
- its value is at least 1,000 XVG;
- its creating transaction is at least 720 blocks deep;
- its script type is supported by the staking verifier;
- it is not subject to an active staking lock or reward-maturity rule; and
- its ownership proof is valid for the candidate block and slot.

Selection weight is linear in value. Each eligible base unit has equal probability. Splitting or combining outputs therefore does not change expected aggregate selection weight, apart from integer rounding and the minimum-output rule. Coin age beyond 720 confirmations adds no weight.

Bonding is an explicit on-chain action. A bonded output delegates its entire value to one staking key; users who want separate delegations must split funds before bonding. Block production never spends the bonded principal. Beginning unbonding disables new eligibility and locks withdrawal for 20,160 blocks.
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

The parser requires the exact field sizes, a nonzero signing key, a compressed VRF-key prefix of 0x02 or 0x03, nonzero reward and withdrawal key identifiers, minimal push encoding produced by the script builder, and no trailing opcodes. The script remains cryptographically spendable by the withdrawal key, but post-activation consensus rejects spending a live bond except through the explicit equal-principal unbond transition. Script validity alone never authorizes an ordinary bond spend.

## Slots And Timestamps

Time is divided into 30-second slots. A block commits to one slot derived from its timestamp. Nodes calculate slots using consensus arithmetic, not local scheduling state.

A valid block must:

- use a slot strictly later than its parent slot;
- use the canonical timestamp for that slot;
- not exceed the permitted future-slot tolerance; and
- satisfy transaction timestamp rules.

Empty slots are allowed. The next eligible producer may build on the latest valid block using a later slot; no empty placeholder blocks are inserted. This prevents a single offline winner from halting the chain.

A node accepts a candidate no more than one 30-second slot ahead of network-adjusted time. A farther-future block is not accepted early or retained as valid pending work.

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

The initial stake snapshot is taken from chain state at height `nPoSActivationHeight - 240`. A bond in that snapshot is initially eligible only if it also has 720 confirmations at activation. Subsequent epoch `e` uses the canonical bonded-stake snapshot from the end of epoch `e - 2`. Epochs 0 and 1 use the initial snapshot. Snapshot membership, values, credentials, and total eligible bonded value are committed by a canonical Merkle root.
## Randomness And Fork Choice

The consensus design uses:

1. Use the RFC 9381 `ECVRF-P256-SHA256-SSWU` ciphersuite for private slot eligibility and publicly verifiable proofs. P-256 support is already available through the required OpenSSL dependency, but the ECVRF protocol wrapper remains new consensus code and must pass RFC vectors and independent audit.
2. Give each bonded delegation a separate VRF public key. The online delegated credential produces VRF proofs; the unrestricted XVG spending key is not used for slot evaluation.
3. Snapshot bonded stake two epochs before it is used for leader selection and voting, and commit the canonical snapshot root in every applicable PoS block. This prevents a producer from moving stake after seeing the randomness for the target epoch.
4. Derive each epoch seed from the previous seed and canonical VRF outputs committed during the prior epoch, with a cutoff before the end of that epoch. Late outputs cannot influence the next seed. The contribution cutoff is the first 80 slots of each 120-slot epoch. If no eligible contribution is included before the cutoff, the next seed is the domain-separated hash of the previous seed and epoch number. Both paths require deterministic vectors.
5. Use latest-message-driven, stake-weighted GHOST from the latest finalized checkpoint for temporary fork choice. Each bonded delegation has one latest vote, weighted by its snapshotted bonded value. Finalized history is never reconsidered by ordinary fork choice.

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

The kernel must not include mutable transaction ordering, reward destination, signature bytes, or arbitrary nonce fields. This limits grinding by a candidate producer. `parent_randomness` is the epoch seed committed by the finalized epoch transition. It is derived from the previous seed and canonical accepted VRF outputs before the contribution cutoff, never from signature bytes.

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

## Unbonding Transaction

A valid unbond transaction spends one `TX_STAKE_BOND_V1` output into one `TX_STAKE_UNBOND_V1` output carrying exactly the same principal and withdrawal credential. It sets the consensus unlock height to the containing block height plus 20,160 accepted blocks.
The version 1 unbond output uses a 35-byte script:

~~~text
PUSHDATA(8) || "VPU" || 0x01 ||
unlock_height_le32 ||
OP_DROP ||
OP_DUP OP_HASH160 withdrawal_key_id[20] OP_EQUALVERIFY OP_CHECKSIG
~~~

The unlock height is an absolute accepted-block height encoded as an unsigned little-endian 32-bit integer. Zero heights, zero withdrawal identifiers, malformed pushes, altered magic/version bytes, and trailing opcodes are rejected. The transaction may use separate ordinary inputs to pay its fee; the bonded principal may not be reduced to pay fees.

The bond stops contributing eligibility and vote weight according to the approved delayed snapshot rules. Before the unlock height, spending the unbond output is invalid. At or after the unlock height, the withdrawal credential may spend it normally or create a new bond. New bonding resets maturity and enters future snapshots normally.
The transaction-validation implementation enforces the lifecycle before and after PoS activation so stake may be prepared while the PoW history is still active:

- every bond output is at least 1,000 XVG;
- any coinbase input in a transaction that creates a bond is at least 720 accepted blocks deep;
- an unbond output cannot be created unless the transaction spends a live bond;
- a transaction spending a live bond has exactly one bond input and exactly one unbond output;
- that unbond output preserves the complete bonded principal and withdrawal credential;
- its unlock height equals the containing block height plus 20,160 exactly; and
- an unbond output cannot be spent before its encoded unlock height.

Ordinary inputs and outputs may accompany an unbond transition to pay fees and return ordinary change. The bonded principal itself cannot pay the fee. Requiring every coinbase input in a bond-creation transaction to satisfy the 720-block rule is intentionally conservative: wallets should avoid mixing immature fee rewards into a bond transaction.

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

PoW chainwork is not meaningful after activation. The PoS fork-choice rule must be deterministic and must not reward production of private empty histories.

The fork-choice rule is:

1. Never reorganize below a finalized checkpoint.
2. Starting at the latest finalized checkpoint, apply latest-message-driven, stake-weighted GHOST using the bonded-stake snapshot for the voting epoch.
3. Count at most one latest valid vote per bonded delegation, weighted by its snapshotted bonded value.
4. Ignore votes that conflict with finalized history, are from an ineligible delegation, or target an unknown or invalid block.
5. Resolve equal-weight child branches by the lowest block hash.

PoW chainwork and raw block count do not participate in post-activation fork choice. Vote updates, snapshot selection, tie-breaking, and restart reconstruction require deterministic tests before Phase 2 is complete. Because the compatibility header does not contain the PoS extension, receiving a structurally valid PoS header is not enough to add stake score or make it an active-chain candidate. Header-only descendants remain staged. A PoS branch becomes eligible for fork choice only after every required full block and its stake proof, authorization, commitments, and state transition have been validated. Legacy GetBlockProof returns zero for nBits zero and must not be replaced with header count or synthetic work.

## Vote And Checkpoint State Transitions

A synthetic bootstrap checkpoint is assigned to the final PoW block at height
`nPoSActivationHeight - 1`. It uses checkpoint epoch `UINT64_MAX` and is
justified and finalized by consensus at activation. It is not a stake snapshot
epoch and the shared numeric sentinel is interpreted according to the serialized
field's type. The first valid PoS block creates the epoch 0 checkpoint, and the
first supermajority link therefore votes from the final-PoW bootstrap checkpoint
to epoch 0. The bootstrap checkpoint carries no stake weight and cannot be
recreated or replaced by an ordinary fork.

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

Votes are relayed independently and may be included by any later block until stale. Inclusion order is canonical by bond outpoint and vote hash. Vote signatures are verified against the signing key and weight from the vote's declared snapshot epoch; peer-supplied weight is ignored.
## Finality And Long-Range Protection

PoS-only consensus requires protection against old keys constructing alternate history. The production design must include:

- justified and finalized checkpoints;
- conflicting-vote detection;
- an unbonding or eligibility-lock period;
- weak-subjectivity checkpoints for new or long-offline nodes; and
- explicit bootstrap behavior for assume-valid and snapshots.
Official releases embed a recent finalized checkpoint for public networks. A node that has been offline longer than the 20,160-block unbonding period must obtain a newer release or explicitly configure a trusted finalized checkpoint. Peer majority alone cannot establish a weak-subjectivity root. Regtest permits deterministic local checkpoint configuration for testing.

After activation, the legacy fixed-depth reorganization limit does not select the chain. Reorganizations may affect any unfinalized block according to stake-weighted fork choice, but no ordinary fork may replace or precede the latest finalized checkpoint.

Checkpoint epochs contain 120 slots, approximately one hour. Empty slots remain part of the epoch and do not shift epoch boundaries. A checkpoint is finalized after valid votes represent at least two-thirds of all currently bonded, mature delegated stake. If that threshold is unavailable, valid block production continues but finality pauses.

Voting is automatic whenever staking is enabled and the delegated staking key is available. The wallet signs and relays checkpoint votes without user prompts. A vote does not spend XVG, change reward ownership, or grant general spending authority. Users make only the explicit lifecycle decisions to bond, enable or disable staking, and begin unbonding.

Unbonding takes 20,160 accepted blocks. Its wall-clock duration varies with occupied slots and is expected to exceed seven days under the one-eligible-producer-per-slot target. Stake remains slashable or lockable for protocol violations during that period and cannot evade an already-observed equivocation by beginning withdrawal.

Regtest must exercise finalization, but Phase 2 must not pretend that a single-node checkpoint implementation is production-ready.

## Equivocation

Signing two different blocks for the same parent and slot is objectively detectable. Equivocation evidence consists of both conflicting, validly signed block authorization messages and is bounded to a canonical fixed format.

Evidence affects consensus only after inclusion in a valid block. The offending bond becomes ineligible at the first block of the next epoch and remains locked against withdrawal and staking for 20,160 blocks. Evidence already applied for a bond and offense is idempotent. Conflicting or malformed evidence is rejected before expensive signature checks where possible.

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

The block signing hash uses the VergePoS/Block/v1 domain over the first 209 bytes and excludes the signature. The candidate header hash covers the existing header, which does not serialize the PoS extension, so it does not create a cycle. A complete signed authorization may separately be hashed in the VergePoS/Equivocation/v1 domain for canonical evidence ordering.
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

- two valid block authorizations by one bond for the same parent and slot;
- two checkpoint votes by one bond for different targets in the same target epoch;
- a later vote whose source and target epochs surround an earlier vote; and
- a later vote surrounded by an earlier vote.

Evidence contains both conflicting signed messages, is canonically ordered, and is applied only after inclusion in a valid block. Malformed evidence cannot trigger a lockout.
Version 1 block equivocation evidence is exactly 547 bytes: one version byte followed by two 273-byte block authorizations. Both authorizations must have the same nonzero network identifier, bond outpoint, parent block root, and slot, but different candidate header hashes. Version 1 vote equivocation evidence is exactly 459 bytes: one version byte followed by two 229-byte checkpoint votes from the same bond. It proves either different target roots for one target epoch or a strict surround relation. Each pair is sorted by its full signed-object hash and duplicates are rejected before signature verification.

### Committed State

Each PoS block commits through its coinbase-like transaction and header Merkle root to:

- the bonded-stake snapshot root used for eligibility and vote weight;
- the checkpoint-vote root;
- the epoch seed;
- the canonical stake-proof hash; and
- any applied equivocation-evidence root.

Nodes independently derive and verify every root. Peer-provided totals, weights, and roots are never authoritative.

### Crash-Safe State

Bonding, unbonding, eligibility lockouts, latest votes, justified and finalized checkpoints, epoch seeds, and stake snapshots are updated atomically with block connection. Every state transition has deterministic disconnect undo data. Reindex and replay from blocks must reproduce byte-identical consensus state and roots. Database caches are never a source of consensus truth.

### Layered Verification

Validation orders inexpensive checks before cryptographic work: activation and version, canonical lengths, count limits, parent availability, slot bounds, sorted uniqueness, known bond, maturity, and snapshot membership precede VRF and signature verification. Per-peer accounting limits repeated invalid proofs, votes, evidence, and duplicate announcements. Batch verification may optimize valid traffic but must fall back to individual verification and must never change acceptance results.

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
votes are ignored without punishment.

The non-consensus vote pool stores at most 4,096 votes and 2 MiB of serialized
vote data, retains one latest vote per bond, and expires votes more than three
epochs behind. Duplicate and regressing votes are ignored. Conflicting votes
for one target epoch are not allowed to replace the pool entry. Block producers select
at most `nPoSMaxVotesPerBlock` votes in canonical order and revalidate every
pooled vote against their current chain state before including it. Pool policy
does not change block validity.

Vote-equivocation evidence uses inventory type `MSG_POS_VOTE_EVIDENCE` and the
`posvevid` command with an exact 459-byte payload. Nodes stage at most 256
records and 256 KiB, admit only canonical conflicts with known snapshot bonds
and two valid signatures, and apply a separate burst/refill budget of 32 and 8
records per minute per peer. Existing valid evidence is not evicted to admit
new traffic. When two independently admitted votes form canonical double-target
or surround evidence, the node constructs and relays the evidence automatically.
Producers revalidate and include at most 16 records, removing them from the
local pool only after the containing block is accepted.

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
## Wallet Behavior

Staking is opt-in. Merely owning XVG or opening a wallet does not automatically expose keys or begin staking. The wallet must show:

- eligible and currently locked balances;
- staking enabled/disabled state;
- staking-key status;
- estimated selection probability without promising rewards;
- last attempted and accepted slots; and
- earned fee rewards and their maturity.

Wallet locking, backup, encryption, hardware-wallet behavior, and delegated staking require explicit tests.

## Required Test Coverage

Phase 3 requires fixed vectors and adversarial tests for:

- activation boundary and rejection of post-activation PoW;
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
Examples 13 and 14. Randomness helpers commit to exactly 120 ordered
pre-activation block IDs for the initial seed and to ordered cutoff VRF outputs
for subsequent epoch seeds.

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
that restart path. Equivocation evidence is persisted by canonical evidence ID so
replay is idempotent. Valid evidence immediately prevents withdrawal, then at
the first block of the next epoch removes the offender's latest vote and
excludes the bond from production, voting, and new snapshots for 20,160
accepted blocks. Lockout activation, expiry, evidence IDs, and vote removal
are all covered by disconnect undo records. LMD-GHOST candidate selection
is implemented over full-data block-index candidates: each bond's latest valid
vote contributes its delayed-snapshot weight to the complete descendant
subtree containing its voted head. The heavier subtree wins, equal subtree
weights select the lower child block hash, and candidate pruning retains
shorter PoS forks that may have greater stake support. Before the first PoS
block establishes the finalized PoW anchor, eligible PoS candidates must
descend directly from the active block at activation height minus one.

Wallet production now has a shared single-attempt path used by both the
automatic scheduler and the `generatestake` regtest control. Each wallet is
evaluated at most once per canonical slot. The producer derives skipped-epoch
seeds and snapshots through the same state transition used by block
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
The final PoW bootstrap checkpoint, latest-vote aggregation, reversible
justification/finalization, and finalized-reorganization barrier are consensus
state. Deserializing a PoS extension or passing structural checks alone never
establishes validity.

Phase 2 is complete for regtest. Fixed cryptographic and serialization vectors,
state transition and undo tests, deterministic fork weight and hash tie-break
tests, evidence-lock transaction tests, and the end-to-end functional path are
present. Phase 3 now includes independent vote and vote-evidence relay, bounded
pools, exact-size malformed-message checks, a two-node partition/convergence
scenario, and restart plus chainstate-reindex coverage. Broader multi-producer
partitions, sustained resource-exhaustion tests, and full reindex-from-block-file
recovery remain mandatory before public testing.

1. Phase 1 ends only after all open consensus decisions in this document are resolved and reviewed.
2. Phase 2 implements regtest-only activation and staking; mainnet behavior remains byte-for-byte unchanged.
3. Phase 3 adds deterministic and adversarial tests before public testing.
4. Phase 4 operates a public PoS testnet for months and records participation, forks, missed slots, and recovery events.
5. Phase 5 requires independent consensus and cryptographic review with findings addressed publicly.
6. Phase 6 begins only after readiness review; production activation height is selected last.
## Documentation Discipline

This document is updated with each approved consensus decision. Implementation commits must reference the relevant section and must not introduce undocumented consensus behavior. Before public testnet, the draft must include exact byte serialization, domain-separation strings, arithmetic bounds, state-transition pseudocode, RPC behavior, and fixed vectors. Before Phase 6 it will be promoted from a research draft to a versioned consensus specification linked from the root README and release documentation.
