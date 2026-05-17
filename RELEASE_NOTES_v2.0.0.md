# HIDERING v2.0.0 — Coordinated Hard Fork

**Ticker:** HRG · **Network:** mainnet (post-fork) · **Hard fork** (wire-break, not backwards-compatible)

> ⚠️ **This is a coordinated hard fork.** v2.0.0 nodes will **not** peer with v1.x nodes. The v1.x chain (height ~3577 at fork time) is abandoned by explicit project decision. Operators upgrading from v1.0.x must wipe their LMDB data directory and re-sync from scratch — the on-disk database from v1.x is orphan for a v2.0.0 daemon. There is no in-place migration path.

## TL;DR

| | |
| --- | --- |
| Type | Hard fork (P2P handshake separates the networks) |
| Wire-compat | **None.** v1.x peers rejected at handshake |
| DB-compat | **None.** v1.x LMDB is orphan to v2.0.0 |
| Consensus rules | Unchanged (same RandomX, ring sizes, mixnet, supply curve) |
| Genesis block | Unchanged (same hash, same NUMS 157.14 HRG locked) |
| Upgrade path | Stop daemon → wipe `~/.hidering` → install v2.0.0 → re-sync |

## Why this hard fork

A 51% attack hit the v1.x mainnet at height ~3577 on 2026-05-16. Rather than attempt to reorg or coordinate a softer rollback on a chain that was demonstrably exposed at its post-launch hash rate, the project elected a clean cut: bump the network identifier so v1.x and v2.0.0 nodes cannot accidentally peer, restart the public chain from genesis, and reopen mining publicly after a private bootstrap phase that lets honest hash rate aggregate before the network is exposed again.

## What changed at the protocol layer

The change is intentionally minimal — three bytes, three constants. The cryptographic primitives, consensus rules, transaction format, and genesis block are all unchanged.

### Network identifier — `src/cryptonote_config.h:255`

```diff
- 0x48, 0x52, 0x47, 0x01, 0x48, 0x49, 0x44, 0x45, 0x52, 0x49, 0x4E, 0x47, 0x4D, 0x41, 0x49, 0x4E
+ 0x48, 0x52, 0x47, 0x02, 0x48, 0x49, 0x44, 0x45, 0x52, 0x49, 0x4E, 0x47, 0x4D, 0x41, 0x49, 0x4E
```

UUID changes from `HRG\x01HIDERINGMAIN` to `HRG\x02HIDERINGMAIN`. The P2P handshake compares the full 16-byte UUID, so v1.x and v2.0.0 nodes mutually reject each other on connection.

### Magic bytes — `src/cryptonote_config.h`

```diff
- 0x48524701
+ 0x48524702
```

First four bytes of `NETWORK_ID`. Used to disambiguate any tooling that keys off magic bytes.

### Version string — `src/version.cpp.in`

```diff
- #define DEF_MONERO_VERSION "1.0.2"
+ #define DEF_MONERO_VERSION "2.0.0"
```

`hideringd --version` now reports `Hidering 'Privacy Enhanced' (v2.0.0-release)`.

## What did not change

| Property | Value (unchanged) |
| --- | --- |
| PoW | RandomX (CPU-only, ASIC-resistant) |
| Block time | 120 s |
| Halving interval | 210,000 blocks (~2.66 y) |
| Initial reward | **42.857142857143 HRG / block** |
| Supply hard cap | **18,000,000 HRG** |
| Ring size | 32–64 (dynamic) at HF15+ |
| Tx padding | 2,500 bytes fixed |
| Mixnet | 3-hop, mandatory |
| Amount quantum | 0.1 HRG |
| Address prefix | 60 (addresses start with `B`) |
| P2P port | 19740 |
| RPC port | 19741 |
| Genesis block | identical hash (NUMS 157.14 HRG locked, unspendable) |

The genesis block intentionally stays bit-identical with v1.x: `NETWORK_ID` is not part of the block hash, only the P2P handshake compares it. See `GENESIS_PROOF.md` for the NUMS construction.

## User-facing rebrand sweep

Two cleanup commits in this release purge ~23 residual "Monero" prose strings that surfaced in user-visible places (wallet `welcome`, refresh / mining messages, MMS errors, daemon prune warning, legacy `blockchain.bin` migration warning, etc.):

- `8b6fa8ddf` — 17 strings in `simplewallet.cpp` + `command_parser_executor.cpp`
- `b4356298c` — 6 strings in `wallet2.cpp`, `message_store.cpp`, `cryptonote_core.cpp`, `db_lmdb.cpp`

Interop-bound references (Trezor protobuf message names, Ledger app references, `monero:` URI scheme, cold-wallet file format magic headers, `MONERO_RANDOMX_*` env vars) are intentionally left as-is — changing them would break hardware wallet signing and existing tooling.

## Pre-public network phase

Following the 51% attack on v1.x, the v2.0.0 mainnet starts as a **private network phase**: founding nodes mine on the new chain before public peering is enabled, so that honest hash rate has time to build up. Total volume mined during this phase and the exact public-opening date will be disclosed at launch. The "zero premine" claim that was attached to v1.0.x is no longer accurate for v2.0.0; please refer to the whitepaper and project docs for the current framing.

## What's in this release

Linux x86_64 binaries (built from branch `v2-privacy`, this tag):

| Binary | Purpose |
| --- | --- |
| `hideringd` | Full node daemon (P2P + RPC), hard-fork-ready |
| `hidering-wallet-cli` | Interactive wallet (send / receive / mine) |
| `hidering-wallet-rpc` | Headless wallet for integrations and services |

macOS and Windows builds will follow once the `build-release.yml` workflow can run again — the GitHub Actions billing gate that blocked multi-OS CI on v1.0.1 and v1.0.2 is still active at the time of this release.

## Verification

```
sha256sum -c hidering-v2.0.0-linux-x64.tar.gz.sha256
```

The Linux binaries are stripped and built with `ARCH=default` so they run on any modern x86_64 host (no AVX-512 required). Dependencies linked dynamically: `libssl`, `libsodium`, `libboost`, `libzmq`, `libunbound`, `libhidapi`.

## Upgrade from v1.0.x

```bash
# stop your existing v1.x node
pkill hideringd

# IMPORTANT — wipe the v1.x LMDB (it is orphan to v2.0.0)
rm -rf ~/.hidering/lmdb

# install v2.0.0
tar -xzf hidering-v2.0.0-linux-x64.tar.gz
cd hidering-v2.0.0-linux-x64
./hideringd
```

Wallets are unaffected by the hard fork — your seed phrase, view key, and spend key remain valid. Address format (`B…`, prefix 60) is unchanged. Wallet files in `~/.hidering/` other than the blockchain DB do not need to be touched.

## Compatibility matrix

| Combination | Result |
| --- | --- |
| v2.0.0 daemon ↔ v2.0.0 peer | ✅ peers (handshake matches) |
| v2.0.0 daemon ↔ v1.x peer | ❌ rejected at handshake (NETWORK_ID mismatch) |
| v1.x daemon ↔ v2.0.0 peer | ❌ rejected (mirror case) |
| v2.0.0 daemon + v1.x LMDB | ❌ daemon refuses to load the orphan DB |
| v2.0.0 wallet ↔ v1.x daemon | ⚠️ RPC may connect but the chain it sees is the abandoned v1.x chain — do not use |

## Known limitations (carried over from v1.0.2)

- **macOS / Windows assets** land once GitHub Actions billing is restored (v2.0.0 punch list item #4 in `CLAUDE.md`).
- **Block explorer** is invalid — the indexed chain is the abandoned v1.x. A reset or fresh instance is part of the post-fork punch list.
- **Pool mining** (Phase 4E) stack (`monero-pool` by jtgrassie) must be relinked against the v2.0.0 libraries and pointed at a v2.0.0 daemon before it can be used post-fork.
- **DNS seeds** — bootstrap currently relies on the hard-coded Flux seed (`hideringseed1`) and any peers supplied via `--add-priority-node`. The Flux seed is being redeployed on a fresh v2.0.0 image with a volume wipe.

## Reproducibility

Source corresponds exactly to the `v2.0.0` git tag on branch `v2-privacy`. The `.github/workflows/build-release.yml` workflow reproduces these binaries on any fork that re-tags.

---

License: see `LICENSE` (BSD-3-Clause, inherited from Monero).
