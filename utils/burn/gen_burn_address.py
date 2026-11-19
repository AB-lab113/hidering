#!/usr/bin/env python3
"""HIDERING burn address generator (one-shot, offline, out of consensus).

Builds a standard mainnet "B..." address whose SPEND public key is a NUMS
point (nothing-up-my-sleeve): nobody knows, or can compute classically, the
matching private spend key. The method is exactly the one used for the
genesis output (GENESIS_PROOF.md):

    H_i = SHA-256(domain || i_be32), first i that decodes as an Ed25519 point

The VIEW key is deliberately PUBLIC: the private view key is a scalar
derived from a public phrase, so that anybody can open a view-only wallet
on the burn address and audit everything it receives. A view key cannot
spend; only the spend key can, and that one does not exist.

This script never creates, prints or stores a private spend key. The only
scalar it ever computes is the (public) view key.

Usage:  python3 -I utils/burn/gen_burn_address.py
No dependency outside the Python 3 standard library. Touches no file.
"""
import hashlib
import sys

# ---------------------------------------------------------------- parameters
SPEND_DOMAIN = b"HIDERING premine burn - fair launch correction 2026 spend key"
VIEW_DOMAIN = b"HIDERING premine burn - fair launch correction 2026 view key"
MAINNET_ADDRESS_PREFIX = 60  # CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX (mainnet)

# ---------------------------------------------------------------- Ed25519
p = 2**255 - 19
d = -121665 * pow(121666, p - 2, p) % p
L = 2**252 + 27742317777372353535851937790883648493  # prime subgroup order


def recover_x(y, sign):
    """Verbatim from GENESIS_PROOF.md."""
    if y >= p:
        return None
    num = (y * y - 1) % p
    den = (d * y * y + 1) % p
    if den == 0:
        return None
    x2 = num * pow(den, p - 2, p) % p
    if x2 == 0:
        return 0 if sign == 0 else None
    x = pow(x2, (p + 3) // 8, p)
    if (x * x - x2) % p != 0:
        x = x * pow(2, (p - 1) // 4, p) % p
    if (x * x - x2) % p != 0:
        return None
    if (x & 1) != sign:
        x = p - x
    return x


def hash_to_point(domain):
    """Verbatim from GENESIS_PROOF.md (same primitive as the genesis NUMS)."""
    for i in range(256):
        h = hashlib.sha256(domain + i.to_bytes(4, "big")).digest()
        b = bytearray(h)
        sign = (b[31] >> 7) & 1
        b[31] &= 0x7F
        y = int.from_bytes(b, "little")
        if recover_x(y, sign) is not None:
            return h, i
    raise RuntimeError("no valid point in 256 iterations")


def decode_point(s):
    b = bytearray(s)
    sign = (b[31] >> 7) & 1
    b[31] &= 0x7F
    y = int.from_bytes(b, "little")
    x = recover_x(y, sign)
    if x is None:
        raise ValueError("not a point")
    return (x, y, 1, x * y % p)


def encode_point(P):
    X, Y, Z, _ = P
    zi = pow(Z, p - 2, p)
    x, y = X * zi % p, Y * zi % p
    return (y | ((x & 1) << 255)).to_bytes(32, "little")


def point_add(P, Q):  # extended twisted Edwards coordinates, a = -1
    X1, Y1, Z1, T1 = P
    X2, Y2, Z2, T2 = Q
    A = (Y1 - X1) * (Y2 - X2) % p
    B = (Y1 + X1) * (Y2 + X2) % p
    C = 2 * d * T1 * T2 % p
    D = 2 * Z1 * Z2 % p
    E, F, G_, H = B - A, D - C, D + C, B + A
    return (E * F % p, G_ * H % p, F * G_ % p, E * H % p)


IDENTITY = (0, 1, 1, 0)


def scalar_mult(k, P):
    R = IDENTITY
    while k:
        if k & 1:
            R = point_add(R, P)
        P = point_add(P, P)
        k >>= 1
    return R


def is_identity(P):
    X, Y, Z, _ = P
    return X % p == 0 and (Y - Z) % p == 0


Gy = 4 * pow(5, p - 2, p) % p
G = decode_point(Gy.to_bytes(32, "little"))

# ---------------------------------------------------------------- Keccak-256
# Original Keccak (pad 0x01), as used by CryptoNote -- NOT hashlib.sha3_256.
_RC = [
    0x0000000000000001, 0x0000000000008082, 0x800000000000808A, 0x8000000080008000,
    0x000000000000808B, 0x0000000080000001, 0x8000000080008081, 0x8000000000008009,
    0x000000000000008A, 0x0000000000000088, 0x0000000080008009, 0x000000008000000A,
    0x000000008000808B, 0x800000000000008B, 0x8000000000008089, 0x8000000000008003,
    0x8000000000008002, 0x8000000000000080, 0x000000000000800A, 0x800000008000000A,
    0x8000000080008081, 0x8000000000008080, 0x0000000080000001, 0x8000000080008008,
]
_ROT = [[0, 36, 3, 41, 18], [1, 44, 10, 45, 2], [62, 6, 43, 15, 61],
        [28, 55, 25, 21, 56], [27, 20, 39, 8, 14]]
_M = (1 << 64) - 1


def _rol(v, n):
    return ((v << n) | (v >> (64 - n))) & _M if n else v


def _keccak_f(A):
    for rc in _RC:
        C = [A[x][0] ^ A[x][1] ^ A[x][2] ^ A[x][3] ^ A[x][4] for x in range(5)]
        D = [C[(x - 1) % 5] ^ _rol(C[(x + 1) % 5], 1) for x in range(5)]
        A = [[A[x][y] ^ D[x] for y in range(5)] for x in range(5)]
        B = [[0] * 5 for _ in range(5)]
        for x in range(5):
            for y in range(5):
                B[y][(2 * x + 3 * y) % 5] = _rol(A[x][y], _ROT[x][y])
        A = [[B[x][y] ^ ((~B[(x + 1) % 5][y]) & B[(x + 2) % 5][y]) for y in range(5)]
             for x in range(5)]
        A[0][0] ^= rc
    return A


def keccak256(data):
    rate = 136
    msg = bytearray(data) + b"\x01"
    msg += b"\x00" * ((-len(msg)) % rate)
    msg[-1] |= 0x80
    A = [[0] * 5 for _ in range(5)]
    for off in range(0, len(msg), rate):
        block = msg[off:off + rate]
        for i in range(rate // 8):
            x, y = i % 5, i // 5
            A[x][y] ^= int.from_bytes(block[8 * i:8 * i + 8], "little")
        A = _keccak_f(A)
    out = b"".join(A[i % 5][i // 5].to_bytes(8, "little") for i in range(4))
    return out


# ---------------------------------------------------------------- CryptoNote base58
_ALPHABET = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"
_ENC_BLOCK = [0, 2, 3, 5, 6, 7, 9, 10, 11]


def _enc_block(block):
    n = int.from_bytes(block, "big")
    out = []
    for _ in range(_ENC_BLOCK[len(block)]):
        n, r = divmod(n, 58)
        out.append(_ALPHABET[r])
    return "".join(reversed(out))


def _dec_block(s, size):
    n = 0
    for c in s:
        n = n * 58 + _ALPHABET.index(c)
    return n.to_bytes(size, "big")


def base58_encode(data):
    return "".join(_enc_block(data[i:i + 8]) for i in range(0, len(data), 8))


def base58_decode(s):
    out = b""
    for i in range(0, len(s), 11):
        chunk = s[i:i + 11]
        out += _dec_block(chunk, _ENC_BLOCK.index(len(chunk)))
    return out


def varint(n):
    out = bytearray()
    while n >= 0x80:
        out.append((n & 0x7F) | 0x80)
        n >>= 7
    out.append(n)
    return bytes(out)


def make_address(prefix, spend_pub, view_pub):
    body = varint(prefix) + spend_pub + view_pub
    return base58_encode(body + keccak256(body)[:4])


# ---------------------------------------------------------------- main
def main():
    # Self-tests of the primitives (fail closed).
    assert keccak256(b"").hex() == \
        "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470"
    gP, gctr = hash_to_point(b"HIDERING fair launch 2026 genesis block")
    assert gP.hex() == "bb0e85edf7e295a84d172c26a759978dc1febea3e2608a240495368a74cdb2d2" and gctr == 2, \
        "hash_to_point does not reproduce the genesis NUMS point"
    assert encode_point(scalar_mult(1, G)) == Gy.to_bytes(32, "little")

    # 1. Spend public key: NUMS point, straight out of SHA-256. No scalar involved.
    spend_pub, spend_ctr = hash_to_point(SPEND_DOMAIN)
    B = decode_point(spend_pub)
    assert encode_point(B) == spend_pub  # canonical encoding
    spend_torsion_free = is_identity(scalar_mult(L, B))

    # 2. View key: public scalar from a public phrase -> anybody can scan.
    view_ctr = None
    for i in range(256):
        a = int.from_bytes(hashlib.sha256(VIEW_DOMAIN + i.to_bytes(4, "big")).digest(), "little") % L
        if a != 0:
            view_ctr = i
            break
    view_sec = a.to_bytes(32, "little")
    view_pub = encode_point(scalar_mult(a, G))

    # Guard: the spend key must not be the image of any scalar this script knows.
    assert spend_pub != view_pub and spend_pub != encode_point(G)

    # 3. Address.
    addr = make_address(MAINNET_ADDRESS_PREFIX, spend_pub, view_pub)
    raw = base58_decode(addr)
    assert raw[:1] == varint(MAINNET_ADDRESS_PREFIX) and raw[1:33] == spend_pub \
        and raw[33:65] == view_pub and raw[65:] == keccak256(raw[:65])[:4]

    print("spend domain        :", SPEND_DOMAIN.decode())
    print("spend counter       :", spend_ctr)
    print("spend public key    :", spend_pub.hex())
    print("spend in prime-order subgroup (L*B == O):", spend_torsion_free)
    print("view domain         :", VIEW_DOMAIN.decode())
    print("view counter        :", view_ctr)
    print("view SECRET key (public on purpose):", view_sec.hex())
    print("view public key     :", view_pub.hex())
    print("address (mainnet)   :", addr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
