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

The whitepaper commits HIDERING to a fair launch: emission is 100%
proof-of-work, and the code contains no premine and no allocation. Coins
mined by the operator's nodes before the posts of 8 June 2026 on X and
BitcoinTalk contradict that commitment. Rather than only disclosing them, the
project **burns** the corresponding amounts by sending them to this address.

The cut-off is **block 17567** (timestamp 2026-06-08 16:31:34 UTC), the last
block before the posts of 8 June 2026 on X and BitcoinTalk. The burn covers
the amounts listed below, in 3 lots and 20 transactions, and nothing else.

| Lot | Source | Amount | Status |
| --- | --- | --- | --- |
| 1 | `B5TLmi…` (mined up to block 16000, as quantified by the operator; blocks 16001 to 17567: lot 3) | 753.741467142937 HRG | **burned** — 2 transactions, below |
| 2 | `B6VkZP…` | 13 101 HRG | **burned** — 14 transactions, below |
| 3 | sent from `B6VkZP…`; corresponds in value to the block rewards mined to `B5TLmi…` in blocks 16001 to 17567 | 3 728.58 HRG | **burned** — 4 transactions, below |
| **Total** | | **17 583.321467142937 HRG** | **burned** — 20 transactions |

Lot 1 transactions:

| Amount | Block | Tx id |
| --- | --- | --- |
| 753.74 HRG | 132851 | `995dd0a8fe18886cd512bedc8399a115e79cc994dc1c02e94645f40aaba5f772` |
| 0.001467142937 HRG | 132853 | `1f919667aba3982f45afc4281e38c8d8cb40828f8cb823623f03c3c8449ab51f` |
| **753.741467142937 HRG** | | |

Lot 1 was sent in two transactions. The first send used an old wallet build
(before v2.0.3) that still truncated every destination amount to a multiple
of 0.01 HRG (the removed "amount normalisation", audit H5); it refused the
full amount of 753.741467142937 HRG. 753.74 HRG, a multiple of 0.01 HRG, was
sent first (block 132851). The remainder, 0.001467142937 HRG, is smaller than
0.01 HRG, so such a build truncates it to zero and refuses it; it was sent in
a separate transaction (block 132853).

Lot 2 transactions:

| # | Amount | Block | Tx id |
| --- | --- | --- | --- |
| 1 | 1000 HRG | 132879 | `9bb80f1e188cb966ba274e94b625bfcebcd061e9393ac0bda3411db20b88edbb` |
| 2 | 1000 HRG | 132883 | `67266e6c70f7b108c85e625c995ad97599cef112f48389b94fb70d92a977cbd8` |
| 3 | 1000 HRG | 132884 | `8a4a0033d3b0b98ad5545c1e1194cd7e5a883704b9be5e4b90b4dd53c5c9d4b5` |
| 4 | 1000 HRG | 132887 | `9ccaa3bcf4e9e7fd2c9bd9e6513a9b009ffb26dc7940ff913046632baa7f74c0` |
| 5 | 1000 HRG | 132887 | `704cfc0b605db2d79a7b4013366216c2bfcd067b71228ef44f1d6ef246b1151b` |
| 6 | 1000 HRG | 132887 | `fba11ff74d48aa71400f5d594ebb8c85f5c0039f08a9beaa7ce759d8bee2ba37` |
| 7 | 1000 HRG | 132887 | `56f867e3860707ec6e0c1e972c327899b38912e2a0254a430772f6fc31732684` |
| 8 | 1000 HRG | 132888 | `c403b8567d1058d1a5c82bff9854036e7e446abfcb43109efbeb81ff2956138e` |
| 9 | 1000 HRG | 132888 | `77bd806458df4c7a6549b3cc62586fe6aa9f30223b10f56dd945f6446b81a3e5` |
| 10 | 1000 HRG | 132888 | `f71e7ad0aa41bb607ee393466e6fecd195fbf23a0a26b496105833321cedd0f6` |
| 11 | 1000 HRG | 132894 | `fbd8d8d2003a93e1d7e43f552b5e99cbebee8f9b4ca9381968d93a4998944811` |
| 12 | 1000 HRG | 132895 | `e4dd0c76bbe2bca54e26f1c7e1102947f9b45082ebc47bf43b7a3b298643a342` |
| 13 | 1000 HRG | 132899 | `ae9e2e88f5da3c43de6cc671650f72bd388d2aebb1515a4ad2a13321aad6c777` |
| 14 | 101 HRG | 132899 | `a26fef552742373290be67a7a456631e6d54790269ac42a8708309eeb92f9f21` |
| | **13 101 HRG** | | |

Lot 2 was sent in 14 transactions because the sending wallet refused larger
single sends (a limit observed around 1000 HRG per transaction). All amounts
are multiples of 0.01 HRG, so the old-build truncation did not apply: the
decrypted amounts are exact.

Lot 3 transactions (sent from `B6VkZP…`):

| # | Amount | Block | Tx id |
| --- | --- | --- | --- |
| 1 | 1000 HRG | 132955 | `6008b56bb3eb17f4ea6f300ef32d4a2f82436f55a6202c1be396abd672307f9d` |
| 2 | 1000 HRG | 132957 | `34df5533e7637d8e952a202c0740c228445628ccdc931faee296e8bb94297a46` |
| 3 | 1000 HRG | 132961 | `86af76e72cd835e5f5224e220eb9e9980ded16ecaaa8271334b9cde8ed4bc32a` |
| 4 | 728.58 HRG | 132964 | `212c9a601bacd6695f0eb57a4235505740702b80ba812524352fc3221c5db73c` |
| | **3 728.58 HRG** | | |

Lot 3 is sent from `B6VkZP…`. Its amount corresponds in value to the block rewards
mined to `B5TLmi…` in blocks 16001 to 17567; it does not move those coinbase
outputs themselves.

- **Verifiable on the chain:** blocks 16001 to 17567 are contiguous (1567
  blocks); the 4 transactions above, their blocks and their amounts (decrypted
  with the public view key); the total received by the burn address,
  17 583.321467142937 HRG.
- **Declared by the operator:** 87 coinbase outputs of 42.857142857143 HRG
  each, identified by the operator in the wallet `B5TLmi…` in these blocks
  (first at block 16019, last at block 17535), i.e. 3 728.571428571441 HRG.
  All 87 amounts are exactly 42.857142857143 HRG, so none of these blocks
  included transaction fees. The sending wallet of each lot is also declared
  by the operator. This attribution cannot be verified with the public view
  key of the burn address alone.

3 728.571428571441 HRG was rounded up to the next 0.01 HRG (the 0.01 HRG
quantum of older wallet builds, audit H5), giving 3 728.58 HRG: 0.008571428559
HRG more than the declared rewards was burned.

Each burn transaction is listed here with its tx id. Network fees are paid by
the sender and are not part of the burned amount.

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
hidering-wallet-cli --generate-from-view-key burn-audit --restore-height 132850
# Standard address : B9p95wVgmNkVtfLYdGKZPFhj8V1BvUu4c43udYRW1EgbFjkX4s78cF9aTwc8ka5yW9dZvLRCWqEVVVFBg4bmCM1xBf3HARs
# Secret view key  : 4d32ad4830f0ffc09902080f484896d2faa38a69ab93cb4ce07c0f763b002a0c
```

The wallet rejects the pair (« view key does not match standard address ») if
the view key does not belong to the address. Once synced, it shows every
amount received by the burn address. Checked on 2026-11-19 with
`hidering-wallet-cli` v2.0.3: the wallet accepted the pair and, synced from
height 132850, listed 20 incoming transfers totalling 17 583.321467142937 HRG,
with the 20 tx ids above. Also checked with `hidering-wallet-rpc`
`validate_address` (`valid: true`, `nettype: mainnet`; the same address with
its last character changed: `valid: false`).

Every burn transaction listed above can be verified this way, read-only, with
the public view key alone: once synced from height 132850, the view-only
wallet lists each of them as an incoming transfer to the burn address, with
its tx id and amount (`show_transfers in`).

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
