<!-- Copyright (c) 2026 Verge -->

# Verge Stealth Addresses

Verge stealth addresses provide reusable payment addresses without reusing the
same destination on the blockchain. A stealth address contains two compressed
secp256k1 public keys: a scan public key and a spend public key. A sender can use
these public keys to derive a unique one-time destination for every payment, but
cannot derive the corresponding private key. Only the recipient, who owns the
scan and spend secrets, can detect and spend the payment.

Stealth addresses improve address-level privacy. They do not conceal transaction
amounts, timing, network metadata, or other information visible through normal
blockchain and network analysis.

## Address Format

The encoded address is a Base58 string containing the following 75-byte payload:

| Field | Size | Value |
| --- | ---: | --- |
| Version | 1 byte | `0x28` |
| Options | 1 byte | `0` |
| Scan public key | 33 bytes | Compressed secp256k1 key |
| Spend-key count | 1 byte | `1` |
| Spend public key | 33 bytes | Compressed secp256k1 key |
| Signature count | 1 byte | `0` |
| Prefix length | 1 byte | `0` |
| Checksum | 4 bytes | First four bytes of double-SHA256 |

The wallet validates the complete format, checksum, and both curve points before
accepting an address.

## Payment Derivation

Let the recipient scan keypair be `(d, Q)` and spend keypair be `(f, R)`, where
`Q = dG` and `R = fG`.

For each payment, the sender generates a fresh ephemeral keypair `(e, P)`:

```text
P  = eG
c  = SHA256(compressed(eQ))
R' = R + cG
```

The transaction pays a standard P2PKH output derived from `R'`. It also includes
a zero-value `OP_RETURN` output containing the compressed ephemeral public key
`P`. Output ordering may be randomized by the wallet.

The recipient scans transaction metadata and independently calculates:

```text
c  = SHA256(compressed(dP))
R' = R + cG
f' = (f + c) mod n
```

Because `eQ = dP`, both parties derive the same destination public key. The
recipient compares the derived destination against transaction outputs and, on a
match, imports `f'` as the one-time spending key.

An encrypted wallet can identify incoming stealth payments while locked because
it retains the scan capability. It stores the ephemeral-key metadata and expands
the one-time spending key after the wallet is unlocked. The spend secret remains
encrypted while the wallet is locked.

## Wallet Backup and Recovery

Stealth scan and spend secrets are randomly generated and are not recreated from
the wallet's HD seed. Back up the wallet after creating stealth addresses.

The following backup methods preserve stealth ownership:

- A normal `wallet.dat` backup.
- `dumpwallet`, which includes owned stealth scan and spend secrets.
- `exportstealthaddress`, used separately for each address.

Treat wallet dumps and exported stealth secrets like private keys. Anyone with
both secrets can identify and spend payments sent to that stealth address.

`importwallet` restores stealth entries found in a wallet dump and performs the
required blockchain rescan. After using `importstealthaddress`, run
`rescanblockchain` so the wallet can discover historical payments.

## RPC Commands

The examples below use `verge-cli`. Add network and data-directory arguments such
as `-testnet` or `-datadir=<path>` when required by the running node.

### `getnewstealthaddress`

Creates and stores a new stealth address. An encrypted wallet must be unlocked.

```bash
verge-cli getnewstealthaddress "Private donations"
```

The result is the encoded stealth address that can be shared with senders.

### `sendtostealthaddress`

Sends XVG to a stealth address using a newly generated one-time destination.
The wallet must be unlocked and synchronized.

```bash
verge-cli sendtostealthaddress "<stealth-address>" 25
```

The command returns the transaction ID. The payment uses the normal transaction
fee policy; the ephemeral-key metadata output itself has zero value.

### `liststealthaddresses`

Lists stealth addresses owned by the wallet without revealing secrets:

```bash
verge-cli liststealthaddresses
```

To include scan and spend secrets, unlock the wallet and explicitly request them:

```bash
verge-cli liststealthaddresses true
```

Avoid recording or sharing output produced with `show_secrets=true`.

### `exportstealthaddress`

Exports the scan and spend secrets for one owned stealth address. The argument
may be its label or encoded address, and the wallet must be unlocked.

```bash
verge-cli exportstealthaddress "Private donations"
```

Example result structure:

```json
{
  "label": "Private donations",
  "scan_secret": "<64-hex-characters>",
  "spend_secret": "<64-hex-characters>"
}
```

### `importstealthaddress`

Imports a scan secret, spend secret, and optional label. Each secret may be a
32-byte hexadecimal value or a supported Base58-encoded value. The wallet must
be unlocked.

```bash
verge-cli importstealthaddress \
  "<scan-secret>" \
  "<spend-secret>" \
  "Recovered stealth address"
```

Discover historical payments after importing:

```bash
verge-cli rescanblockchain
```

### Full Wallet Export and Import

Export all regular keys, scripts, and stealth secrets:

```bash
verge-cli dumpwallet "/secure/path/verge-wallet-backup.txt"
```

Restore the dump into an unlocked wallet:

```bash
verge-cli importwallet "/secure/path/verge-wallet-backup.txt"
```

The import command rescans the blockchain and may take substantial time.

## Operational Notes

- Keep the node synchronized so new transactions are scanned promptly.
- Unlock encrypted wallets periodically if received one-time spending keys still
  need to be expanded.
- Retain more than an HD seed when stealth addresses are in use.
- A sender needs only the encoded stealth address and never needs either secret.
- Receiving does not require the sender or recipient to coordinate outside the
  normal transaction broadcast process.
