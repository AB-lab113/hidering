# Genesis output unspendability — verifiable construction

This document proves that the **157.14 HRG output emitted by the HIDERING
genesis transaction is permanently unspendable**: nobody, including the
project authors, holds the spend key for it. Anyone can reproduce the
construction below in under a minute.

## TL;DR

- Genesis amount is unchanged (157.14 HRG) — a legacy value preserved to
  keep the deployed chain hash valid. The current maximum supply is
  **18,000,000 HRG** (cap revised 2026-05-11; see whitepaper §4.4 and
  `CLAUDE.md` BUG CRITIQUE MONEY_SUPPLY). The genesis output simply
  burns 157.14 HRG forever, so the effective circulating max is
  **17,999,842.86 HRG**.
- The output one-time public key `P` and the transaction public key `R` were
  derived from a **public domain string + integer counter** via SHA-256
  try-and-increment until the result decodes as a valid Ed25519 point.
- Recovering a spend key for `P` would require either (a) inverting SHA-256
  or (b) computing a discrete logarithm over Curve25519 — both are
  cryptographically infeasible.

## Pre-history (why this rebuild was needed)

The previous `GENESIS_TX` blob hard-coded a one-time pubkey beginning with
the bytes `48 65 65 0f` (ASCII `Hee`). Such a vanity prefix can only be
produced by repeatedly choosing the transaction private key `r` while
holding the recipient address `(A, B)` fixed — meaning the dev who
generated the original genesis necessarily held the wallet that received
it. Local files (`premine-restored.keys`, `cake-import.keys`) corroborated
this. The current rebuild removes that capability.

## NUMS construction

```
Ed25519 prime:  p = 2^255 - 19
Curve eq.:      -x² + y² = 1 + d·x²·y²,  d ≡ -121665 / 121666  (mod p)
```

For each NUMS point we pick a domain string and compute, for `i = 0, 1, …`:

```
H_i = SHA-256( domain || i_be32 )       # 32 bytes, big-endian counter
```

Decode `H_i` as an Ed25519 compressed point: clear bit 7 of byte 31
(sign bit), interpret the result as little-endian `y`, recover `x` from
`x² = (y² − 1) / (d·y² + 1) (mod p)`. The first `i` for which a valid `x`
exists is recorded; `H_i` is the published point.

### One-time output public key `P`

| Field | Value |
| --- | --- |
| Domain | `HIDERING fair launch 2026 genesis block` |
| Counter | `2` |
| `P` (hex) | `bb0e85edf7e295a84d172c26a759978dc1febea3e2608a240495368a74cdb2d2` |

### Transaction public key `R` (in tx_extra)

| Field | Value |
| --- | --- |
| Domain | `HIDERING fair launch 2026 genesis tx pubkey` |
| Counter | `2` |
| `R` (hex) | `f8ae2e22119ef501e05a9f8f347632f0ada2a69cee150d156c41d10df98fbe9f` |

## Resulting `GENESIS_TX`

```
013c01ff00018090858fb0dd2302
bb0e85edf7e295a84d172c26a759978dc1febea3e2608a240495368a74cdb2d2
2101
f8ae2e22119ef501e05a9f8f347632f0ada2a69cee150d156c41d10df98fbe9f
```

(80 bytes; layout: version=1, unlock=60, vin_count=1, txin_gen, height=0,
vout_count=1, amount=157.14 HRG varint, target=txout_to_key, `P`,
extra_size=33, tag=TX_EXTRA_TAG_PUBKEY, `R`.)

## Independent verification

Save the script below as `verify_genesis.py` and run it with `python3`:

```python
import hashlib

p = 2**255 - 19
d = -121665 * pow(121666, p-2, p) % p

def recover_x(y, sign):
    if y >= p: return None
    num = (y*y - 1) % p
    den = (d*y*y + 1) % p
    if den == 0: return None
    x2 = num * pow(den, p-2, p) % p
    if x2 == 0: return 0 if sign == 0 else None
    x = pow(x2, (p+3)//8, p)
    if (x*x - x2) % p != 0:
        x = x * pow(2, (p-1)//4, p) % p
    if (x*x - x2) % p != 0:
        return None
    if (x & 1) != sign: x = p - x
    return x

def hash_to_point(domain):
    for i in range(256):
        h = hashlib.sha256(domain + i.to_bytes(4, 'big')).digest()
        b = bytearray(h)
        sign = (b[31] >> 7) & 1
        b[31] &= 0x7f
        y = int.from_bytes(b, 'little')
        if recover_x(y, sign) is not None:
            return h, i
    raise RuntimeError("no valid point in 256 iterations")

P, P_ctr = hash_to_point(b'HIDERING fair launch 2026 genesis block')
R, R_ctr = hash_to_point(b'HIDERING fair launch 2026 genesis tx pubkey')

assert P.hex()  == 'bb0e85edf7e295a84d172c26a759978dc1febea3e2608a240495368a74cdb2d2'
assert P_ctr    == 2
assert R.hex()  == 'f8ae2e22119ef501e05a9f8f347632f0ada2a69cee150d156c41d10df98fbe9f'
assert R_ctr    == 2

print('OK — NUMS keys reproduced from public domain strings')
```

The script has no external dependencies and runs in under a second.

## Why this guarantees unspendability

To spend a CryptoNote-style output one would need the secret scalar `x`
such that `x · G = P`. Two attack avenues exist:

1. **Brute-force the seed.** Find a `(domain', counter')` pair that
   reproduces `P` with a key the attacker controls. SHA-256 collision
   resistance (~2¹²⁸ work) makes this infeasible.
2. **Solve the discrete log directly.** Compute `x = log_G(P)` on
   Curve25519. The best known attack (Pollard rho) costs ~2¹²⁶ group
   operations.

No third path exists in current public knowledge. The 157.14 HRG sitting
at this output are therefore permanently locked, reducing the effective
maximum circulating supply to **17 999 842.86 HRG** (against the
18,000,000 HRG hard cap set in `cryptonote_config.h` since v1.0.1).

## Network-wide identity

The same NUMS `GENESIS_TX` blob is used for `mainnet`, `testnet`, and
`stagenet` (cf. `src/cryptonote_config.h`). Per-network identity is
preserved through distinct `NETWORK_ID`, `GENESIS_NONCE`, and base58
address prefixes.
