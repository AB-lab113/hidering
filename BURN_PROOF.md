# Pre-mine burn address — verifiable construction

This document proves that the HIDERING burn address below is **unspendable**:
nobody, including the project authors, holds or can classically compute its
private spend key. It uses **the same NUMS construction as the genesis
output** (see [GENESIS_PROOF.md](GENESIS_PROOF.md)). Anyone can reproduce it
in under a second with the Python 3 standard library.

```
B9p95wVgmNkVtfLYdGKZPFhj8V1BvUu4c43udYRW1EgbFjkX4s78cF9aTwc8ka5yW9dZvLRCWqEVVVFBg4bmCM1xBf3HARs
```

## Why this address exists

The whitepaper describes HIDERING as a fair launch with no pre-mine. Coins
mined by the founders before the public phase contradict that claim. Rather
than only disclosing them, the project **burns** them by sending them to this
address.

| Lot | Source | Amount | Status |
| --- | --- | --- | --- |
| 1 | `B5TLmi…` (chain-verified portion, mined before the block 16000 cut-off) | 753.741467142937 HRG | pending — tx id to be added |
| 2 | `B6VkZP…` | 13 101 HRG | later, separately |

Each burn transaction will be listed here with its tx id and tx key once it
is broadcast. Network fees are paid by the sender and are not part of the
burned amount.

## Construction

### Spend public key — NUMS point (nobody knows the private key)

Exactly the genesis method:

```
H_i = SHA-256( domain || i_be32 )      # 32 bytes, big-endian 4-byte counter
```

Decode `H_i` as a compressed Ed25519 point (clear bit 7 of byte 31 as the sign
bit, read the rest as little-endian `y`, recover `x` from
`x² = (y² − 1) / (d·y² + 1) mod p`). The first `i` that yields a valid point
is kept, and `H_i` itself is the public key. **No scalar is ever involved**:
the key comes straight out of SHA-256, so no private key was generated,
stored or discarded.

| Field | Value |
| --- | --- |
| Domain (ASCII) | `HIDERING premine burn - fair launch correction 2026 spend key` |
| Counter | `0` |
| Spend public key | `b1cb3843b454b5acb9eb7705865c0af37cacc62429abe9123c4b399ac4855c58` |

### View key — public on purpose

The view key cannot spend; it can only see incoming outputs. It is therefore
**made public**, so that anyone can audit everything this address receives.
Since outputs can only be decrypted by whoever holds the **private** view key,
that key has to be a known scalar (a NUMS point here would make the address
impossible to audit for everybody):

```
a = int_le( SHA-256( view_domain || i_be32 ) ) mod l     # first i with a ≠ 0
A = a·G
```

`l = 2²⁵² + 27742317777372353535851937790883648493` is the order of the
Ed25519 prime-order subgroup.

| Field | Value |
| --- | --- |
| Domain (ASCII) | `HIDERING premine burn - fair launch correction 2026 view key` |
| Counter | `0` |
| **Private view key (public)** | `4d32ad4830f0ffc09902080f484896d2faa38a69ab93cb4ce07c0f763b002a0c` |
| View public key | `1b7232eec9a298c8123ba4ba0bf94ada9f8e89d9f463f8a8dcf70527a7f6ff5e` |

### Address

Standard CryptoNote encoding: `varint(60) || spend_pub || view_pub ||
Keccak-256(…)[0..4]`, base58 in 8-byte blocks. Prefix 60 is the HIDERING
mainnet standard address prefix (`B…`).

The domains are plain ASCII on purpose (a hyphen, not a dash), so that typing
them by hand gives the same bytes.

## Independent verification

### 1. Recompute everything

```bash
python3 -I utils/burn/gen_burn_address.py
```

The script (`utils/burn/gen_burn_address.py`, standard library only, writes no
file) first checks itself: it reproduces the genesis NUMS point `bb0e85ed…`
(counter 2) with the same `hash_to_point` function as `GENESIS_PROOF.md`,
checks a Keccak-256 test vector, then derives the keys and the address and
decodes the address back. Expected output:

```
spend domain        : HIDERING premine burn - fair launch correction 2026 spend key
spend counter       : 0
spend public key    : b1cb3843b454b5acb9eb7705865c0af37cacc62429abe9123c4b399ac4855c58
spend in prime-order subgroup (L*B == O): False
view domain         : HIDERING premine burn - fair launch correction 2026 view key
view counter        : 0
view SECRET key (public on purpose): 4d32ad4830f0ffc09902080f484896d2faa38a69ab93cb4ce07c0f763b002a0c
view public key     : 1b7232eec9a298c8123ba4ba0bf94ada9f8e89d9f463f8a8dcf70527a7f6ff5e
address (mainnet)   : B9p95wVgmNkVtfLYdGKZPFhj8V1BvUu4c43udYRW1EgbFjkX4s78cF9aTwc8ka5yW9dZvLRCWqEVVVFBg4bmCM1xBf3HARs
```

To check the spend key without trusting the script, the `hash_to_point`
function printed in `GENESIS_PROOF.md` is enough:

```python
assert hash_to_point(b'HIDERING premine burn - fair launch correction 2026 spend key') == (
    bytes.fromhex('b1cb3843b454b5acb9eb7705865c0af37cacc62429abe9123c4b399ac4855c58'), 0)
```

### 2. Check it with the official wallet, and audit the burn

Create a view-only wallet from the address and the public view key:

```bash
hidering-wallet-cli --generate-from-view-key burn-audit --restore-height <height>
# Standard address : B9p95wVgmNkVtfLYdGKZPFhj8V1BvUu4c43udYRW1EgbFjkX4s78cF9aTwc8ka5yW9dZvLRCWqEVVVFBg4bmCM1xBf3HARs
# Secret view key  : 4d32ad4830f0ffc09902080f484896d2faa38a69ab93cb4ce07c0f763b002a0c
```

The wallet rejects the pair (« view key does not match standard address ») if
the view key does not belong to the address. Once synced, it shows every
amount received by the burn address. Checked on 2026-11-19 with
`hidering-wallet-cli` v2.0.3 (wallet accepted the pair, offline) and with
`hidering-wallet-rpc` `validate_address` (`valid: true`, `nettype: mainnet`;
the same address with its last character changed: `valid: false`).

For one specific burn transaction, the sender's published tx key also works:
`check_tx_key <txid> <txkey> B9p95w…HARs`.

## Why the funds cannot be spent

Spending an output sent to this address requires the private spend key `b`
behind `B`. Two avenues exist, as for the genesis output:

1. **Choose the phrase so that the hash lands on a key you control.** That
   would require a SHA-256 preimage for a point whose discrete log is known —
   about 2¹²⁸ work or more.
2. **Compute the discrete log of `B` directly** on Curve25519: about 2¹²⁶
   group operations with Pollard rho.

Knowing the private view key changes nothing: in CryptoNote the one-time
output key is `P = H_s(a·R, n)·G + B`, so `x = H_s(…) + b` and `b` is
still required. (A view-only wallet can't detect spends either, since key
images need `b` too. That is moot here, since nothing can be spent.)

**Torsion.** `B` has a small-order component (order 8, like the genesis
output key which has order 4): `l·B ≠ O`. Strictly speaking, no scalar `b`
then satisfies `b·G = B`. **This is NOT claimed as extra security**: CryptoNote
signatures do not reject torsioned keys, and anyone who knew the discrete
log of the prime-order part of `B` could probably still sign by trial and
error over the small component. The guarantee is the one above: the
discrete log on Curve25519, and nothing more.

## Post-quantum limitation

The guarantee is **classical**. A large quantum computer running Shor's
algorithm would compute discrete logs on Curve25519, so it could recover a
key for this address — and for the genesis output too. That is the same
limitation as `GENESIS_PROOF.md`.

**Coins cannot be "swept" out of this address later**, even to a
post-quantum burn address: moving them would require the very spend key that
does not exist. A post-quantum guarantee therefore has to come from a
**consensus rule**: declared intention to add, at the post-quantum hard fork
(HFv16) or later, a rule that makes the outputs received by this address (and
the genesis output) **permanently unusable as inputs and as ring members**,
listed by their one-time keys, once such a rule has been designed and
audited. Until then, these coins are burned against any classical attacker
only.

## Scope

This address is generated offline and touches no consensus, genesis or
network code. Nothing about it is deployed. The genesis `GENESIS_TX` and
its proof are unchanged.
