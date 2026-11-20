> **Historical release — v1.x chain.** This release belongs to the v1.x chain, which was abandoned at the v2.0.0 hard fork after a 51% attack (16 May 2026, height ≈ 3577). v1.x and v2.0.x nodes do not peer with each other: do not use this release on the current network. Current release: https://github.com/AB-lab113/hidering/releases/tag/v2.0.3 (downloads: https://www.hidering.org/download.html).
>
> The statements below, including "Fair launch (0 premine)", were written for the v1.x launch and are kept as published. They are not statements about the current v2.0.x network: see whitepaper §7.1 and §7.2 (https://www.hidering.org/whitepaper.html) and `BURN_PROOF.md`. The v2.0.0 release notes record that the "zero premine" claim attached to v1.0.x is no longer accurate for v2.0.0.

# HIDERING v1.0.0 — First Public Binaries

**Ticker:** HRG · **Network:** mainnet · **Fair launch** (0 premine)

This is the first public release of **HIDERING**, a privacy-focused proof-of-work cryptocurrency forked from Monero v0.18.1 and hardened with five privacy patches on top of the upstream design.

## What's in this release

Linux x86_64 binaries (built from branch `v2-privacy`, this tag):

| Binary | Purpose |
| --- | --- |
| `hideringd` | Full node daemon (P2P + RPC) |
| `hidering-wallet-cli` | Interactive wallet (send / receive / mine) |
| `hidering-wallet-rpc` | Headless wallet for integrations and services |

Windows and macOS builds will be added to this release once the `build-release.yml` workflow finishes — track the release page for updates.

## Verification

Every asset ships with a `.sha256` sidecar.

```
sha256sum -c hidering-v1.0.0-linux-x64.tar.gz.sha256
```

The Linux binaries are stripped, statically link against ccache-cached system deps (`libssl`, `libsodium`, `libboost`, `libzmq`, `libunbound`, `libhidapi`), and are built with `ARCH=default` so they run on any modern x86_64 host (no AVX-512 required).

## Network specs

| Setting | Value |
| --- | --- |
| Supply max | 33,000,000 HRG (hard cap) |
| Block time | 120 s |
| Initial reward | 157.14 HRG / block |
| Halving | every 210,000 blocks (~2.66 y) |
| PoW | RandomX (CPU-only, ASIC-resistant) |
| P2P port | 19740 |
| RPC port | 19741 |
| Address prefix | 60 (addresses start with `B`) |
| Network ID | `HRG\x01HIDERINGMAIN` |
| Magic bytes | `0x48524701` |

## Privacy enhancements over upstream Monero

1. **Ring size 32–64** (dynamic) — up from upstream's fixed 16.
2. **Transaction padding** to a fixed 2,500 bytes — defeats size-based heuristics. *(Clarification: in the current v2.0.x code the padding applies to the `tx_extra` field only and is applied by the HIDERING wallet when it builds a transaction; it is not enforced by consensus and does not by itself make transaction sizes uniform. See the whitepaper, section 3.)*
3. **Native 3-hop mixnet**, mandatory — no plaintext P2P metadata. *(Corrected: this routing bias was removed in the June 2026 internal review; transaction propagation uses Dandelion++ as inherited from Monero, and P2P traffic is not encrypted. See the whitepaper, sections 2 and 3 (https://www.hidering.org/whitepaper.html).)*
4. **Amount normalization** at 0.01 HRG granularity (corrected: earlier text said 0.1 HRG; feature removed in v2.0.3, audit H5) — reduces fingerprinting.
5. **Stealth addresses V2** with time-bounded view keys. *(Corrected: no time-bounded view-key mechanism exists in the code.)*

## Genesis

- Height 0 reward: **157.14 HRG locked as NUMS** (cryptographically unspendable).
- Unlock: 60 blocks.
- Premine: **0 HRG**. The genesis NUMS output is not part of circulating supply.
- Full cryptographic proof: see `GENESIS_PROOF.md` in the repo.

## Quick start

```bash
tar -xzf hidering-v1.0.0-linux-x64.tar.gz
cd hidering-v1.0.0-linux-x64
./hideringd                 # starts a full node on mainnet
./hidering-wallet-cli       # in a second shell, create / open a wallet
```

The daemon will sync the chain from the public seed node `hideringseed1` (Flux-hosted; address resolved via `https://api.runonflux.io/apps/location/hideringseed1`).

## Reproducibility

Source corresponds exactly to the `v1.0.0` git tag on branch `v2-privacy`. A multi-OS build workflow (`.github/workflows/build-release.yml`) is included so anyone can reproduce these binaries by tagging their own fork.

## Known limitations

- No DNS seeds yet — bootstrap relies on the hard-coded Flux seed and any peers you supply via `--add-peer`.
- No public block explorer yet (planned, see roadmap Phase 4 in `CLAUDE.md`).
- macOS / Windows assets land later in this release once the cross-platform CI run completes.

## Roadmap

- Phase 4 (T2 2026): Mainnet public launch, DNS seeds, block explorer, mining pools.
- Phase 5 (T2 2027): Post-quantum hard fork (Dilithium3 signatures + Kyber768 KEX).

---

License: see `LICENSE` (BSD-3-Clause, inherited from Monero). The HIDERING name, logo, and ticker (HRG) are project marks; the protocol is fully open-source.

> Correction (30/09/2026): the halving interval is about 292 days, not 2.66 years. / Correction du 30/09/2026 : l'intervalle de halving est d'environ 292 jours, pas 2,66 ans.
