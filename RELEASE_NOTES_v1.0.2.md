# HIDERING v1.0.2 — Clean Daemon Boot

**Ticker:** HRG · **Network:** mainnet · **Fair launch** (0 premine)

Maintenance release on top of [v1.0.1](https://github.com/AB-lab113/hidering/releases/tag/v1.0.1). One consensus-orthogonal fix that silences a confusing exception log on every daemon startup. Network parameters and on-disk format are unchanged — v1.0.2 is a drop-in replacement for v1.0.1.

## Changes since v1.0.1

### Fix — `std::bad_alloc` no longer logged at daemon startup (`6b9cf60de`)

On every fresh boot, `hideringd` would emit a `std::bad_alloc` exception with a stack trace shortly after the genesis block log:

```
Exception: std::bad_alloc
  [1] __cxa_throw
  [2] randomx::generateSuperscalar [clone .cold]
  [3] randomx_alloc_cache
  [4] rx_alloc_cache
  [5] rx_set_main_seedhash_thread
```

The exception was caught internally and the daemon kept running fine — RPC bound, P2P loop ran, sync worked — but the trace made every boot look broken.

Root cause: RandomX's `LargePageAllocator` calls `mmap(MAP_HUGETLB)`, which fails on every host with `vm.nr_hugepages = 0` (the default everywhere). The allocator throws `std::bad_alloc` and the library catches it internally to fall back to the default allocator, but our `__cxa_throw` interposer (`src/common/stack_trace.cpp`) logs every throw before any catch handler runs.

Fix: probe `/proc/sys/vm/nr_hugepages` once at first use and skip the `RANDOMX_FLAG_LARGE_PAGES` flag when no huge pages are reserved. Behavior on properly tuned hosts (`echo N > /proc/sys/vm/nr_hugepages`) and on non-Linux platforms is unchanged — the huge-pages fast path is still used where it works. Patch confined to `src/crypto/rx-slow-hash.c`; no consensus or wire-format change.

### Chore — version bump

`DEF_MONERO_VERSION` `1.0.1` → `1.0.2` in `src/version.cpp.in`.

## What's in this release

Linux x86_64 binaries (built from branch `v2-privacy`, this tag):

| Binary | Purpose |
| --- | --- |
| `hideringd` | Full node daemon (P2P + RPC) |
| `hidering-wallet-cli` | Interactive wallet (send / receive / mine) |
| `hidering-wallet-rpc` | Headless wallet for integrations and services |

Windows and macOS builds will be added once the `build-release.yml` workflow can run (GitHub Actions billing has been gating multi-OS CI since v1.0.1 — same situation as the previous release).

## Verification

```
sha256sum -c hidering-v1.0.2-linux-x64.tar.gz.sha256
```

The Linux binaries are stripped, link against system deps (`libssl`, `libsodium`, `libboost`, `libzmq`, `libunbound`, `libhidapi`), and built with `ARCH=default` so they run on any modern x86_64 host (no AVX-512 required).

## Compatibility

- **Wire / consensus:** identical to v1.0.1. v1.0.2 nodes peer with v1.0.1 nodes with no protocol changes.
- **Database:** identical LMDB schema. No wipe needed when upgrading from v1.0.1.
- **Wallets:** no wallet-format changes.

## Network specs (unchanged since v1.0.1)

| Setting | Value |
| --- | --- |
| Supply max | **18,000,000 HRG** (hard cap) |
| Block time | 120 s |
| Initial reward | **42.86 HRG / block** |
| Halving | every 210,000 blocks (~2.66 y) |
| PoW | RandomX (CPU-only, ASIC-resistant) |
| P2P port | 19740 |
| RPC port | 19741 |
| Address prefix | 60 (addresses start with `B`) |
| Network ID | `HRG\x01HIDERINGMAIN` |
| Magic bytes | `0x48524701` |

The genesis-block NUMS output remains `157.14 HRG` locked (legacy from v1.0.0, unspendable, cryptographically inert) so that the existing chain hash is preserved — see `GENESIS_PROOF.md`.

## Privacy enhancements (unchanged)

1. **Ring size 32–64** (dynamic) — up from upstream's fixed 16.
2. **Transaction padding** to a fixed 2,500 bytes.
3. **Native 3-hop mixnet**, mandatory.
4. **Amount normalization** at 0.1 HRG granularity.
5. **Stealth addresses V2** with time-bounded view keys.

## Upgrading from v1.0.1

```bash
# stop your existing node
pkill hideringd

# replace the binaries
tar -xzf hidering-v1.0.2-linux-x64.tar.gz
cd hidering-v1.0.2-linux-x64
./hideringd                 # picks up your existing ~/.hidering data dir
```

No data-dir migration is needed.

## Known limitations (carryover from v1.0.1)

- No DNS seeds yet — bootstrap relies on the hard-coded Flux seed and any peers supplied via `--add-peer`.
- No public block explorer yet (planned, Phase 4).
- macOS / Windows assets land once the GitHub Actions billing gate is cleared (tracked as v1.0.2 punch list item #2 in `CLAUDE.md`).
- The whitepaper and `GENESIS_PROOF.md` still cite the historical 33M / 157.14 figures and are slated for a doc revision (v1.0.2 punch list item #3).

## Reproducibility

Source corresponds exactly to the `v1.0.2` git tag on branch `v2-privacy`. The `.github/workflows/build-release.yml` workflow reproduces these binaries on any fork that re-tags.

---

License: see `LICENSE` (BSD-3-Clause, inherited from Monero).
