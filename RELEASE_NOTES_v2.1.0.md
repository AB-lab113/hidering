HIDERING v2.1.0 is a maintenance release of the node (`hideringd`) and the command-line wallets (`hidering-wallet-cli`, `hidering-wallet-rpc`).

**This is not a hard fork.** v2.1.0 uses the same network and the same consensus rules as v2.0.3, and v2.0.3 and v2.1.0 nodes work together on the same chain. Upgrading is recommended, not required. A future release will be required for the post-quantum fork (HFv16), and its date will be announced in advance.

## Who should upgrade

- **Windows users of the command-line wallet.** `hidering-wallet-cli.exe` from the v2.0.3 zip does not start on a machine without MSYS2: the DLLs it needs (ICU, iconv and their dependencies) were not in the zip. The v2.1.0 zip contains them.
- **Everyone running a node.** v2.1.0 adds checkpoints at heights 80386, 90000 and 110000.

## Changes since v2.0.3

### Packaging
- **Windows:** the zip now contains the seven MSYS2 runtime DLLs that `hidering-wallet-cli.exe` loads (ICU, iconv, libstdc++, libgcc, libwinpthread). Keep the `.dll` files in the same folder as the `.exe` files. (`hideringd.exe` and `hidering-wallet-rpc.exe` only use Windows system DLLs.)
- **Windows:** a new `licenses/` folder contains the licence texts of these DLLs, with the name, version, licence and upstream URL of each MSYS2 package (`PACKAGE-INFO.txt`). The DLLs are unmodified MSYS2 binaries; their sources are available from MSYS2 (https://packages.msys2.org, https://github.com/msys2/MINGW-packages) and from the upstream URLs listed there.
- **Linux:** the binaries are now built for generic x86-64 (`-DARCH=default`) instead of the processor of the CI machine. Before this change, a CI build could contain AVX-512 instructions, depending on which machine built it, and crash with "illegal instruction" on processors without AVX-512. (The v2.0.3 Linux binaries were checked: they start on a processor without AVX-512.)
- **macOS:** built on macOS 15 (Apple silicon only, as before). The binaries now need **macOS 15 or newer** (v2.0.3 needed macOS 14) and Homebrew's **`openssl@4`** (v2.0.3 used `openssl@3`).

### Node
- Three new checkpoints: 80386, 90000 and 110000. The checkpoints help a new node to sync; consensus still rests on proof of work.
- Hardening: before HFv16, a transaction with a post-quantum input is now rejected at the input-type check. Every version, v2.0.3 included, already rejected such a transaction, so this changes no consensus result.

### Post-quantum code (inactive)
- liboqs updated from 0.15.0 to 0.16.0.
- The wallet CLI can now create a BQ... (post-quantum) wallet: `--bq-wallet`, `--bq-seed` and the `address bq` command. `hidering-wallet-rpc` and the GUI wallet cannot create BQ wallets.
- See the warning below before using any of this.

## Post-quantum: inactive, do not use BQ addresses on mainnet yet

- The post-quantum fork (HFv16) is **not scheduled**. The height in the code (2,000,000) is a placeholder; a future release will set the real height, and it will be announced before activation.
- **Before HFv16, a payment to a BQ... address is an ordinary classic output, with no post-quantum protection.**
- A BQ wallet has **two seeds**. Restoring it needs both (`--electrum-seed` and `--bq-seed`).
- After HFv16, spending from a BQ address will be **transparent**: amounts are visible and the post-quantum input has no ring signature. See §6.2 of the whitepaper.
- Use classic B... addresses for everyday use. Treat the BQ options as experimental.

## Compatibility

- **Network:** same network ID, same consensus rules. A node built from this source code synced the mainnet chain from the genesis block with `--fast-block-sync 0` and matched the production nodes (block hashes up to height 135000 and the RingCT output distribution).
- **Blockchain data:** the database format is unchanged. Replace the binaries and restart: no resync is needed.
- **Wallet files:** a wallet created with v2.0.3 opens in v2.1.0 with the same address, seed and view key. Opening it does not rewrite the `.keys` file, and v2.0.3 can still open the wallet after v2.1.0 has saved it. Restoring from the 25-word seed with v2.1.0 gives the same wallet. (Tested offline with the release candidate binaries.) As always, keep your seed safe before upgrading.
- **Desktop GUI wallet:** released separately (v2.0.2-gui, [hidering-gui](https://github.com/AB-lab113/hidering-gui/releases)); not changed by this release.

## Requirements

- **Windows x64:** Windows 10 or 11 (the executables use the Universal C Runtime). Unzip everything into one folder.
- **Linux x64:** built on Ubuntu 22.04, dynamically linked. Needs glibc 2.34 or newer, Boost 1.74 (chrono, filesystem, program_options, regex, serialization, thread), readline 8, libsodium 23, libunbound 8, OpenSSL 3 and, for `hideringd` only, libzmq 5. On Ubuntu 22.04:
  ```
  sudo apt install libboost-chrono1.74.0 libboost-filesystem1.74.0 \
    libboost-program-options1.74.0 libboost-regex1.74.0 libboost-serialization1.74.0 \
    libboost-thread1.74.0 libreadline8 libsodium23 libunbound8 libssl3 libzmq5
  ```
  Ubuntu 24.04 ships Boost 1.83 instead: install the 1.74 libraries separately or build from source.
- **macOS ARM64 (Apple silicon), macOS 15 or newer:** needs these [Homebrew](https://brew.sh) formulae: `boost`, `readline`, `libsodium`, `unbound`, `zeromq` and `openssl@4`. The archive contains no libraries. The binaries use the Homebrew versions that were current when they were built; if a later Homebrew update breaks them, build from source.

## Upgrading

1. Stop the node: type `exit` in the `hideringd` console, or run `./hideringd exit`, and wait until it has stopped.
2. Back up your wallet files and make sure you have your seed.
3. Verify the download (below), then replace the binaries and start `hideringd` again. Miners using the pool have nothing to change; solo miners restart their node with the new binary.

## Verify the download

Each archive comes with a `.sha256` file. **Check it before running anything.**

- Linux: `sha256sum -c hidering-linux-x64.tar.gz.sha256`
- macOS: `shasum -a 256 -c hidering-macos-arm64.tar.gz.sha256`
- Windows (PowerShell): `(Get-FileHash .\hidering-windows-x64.zip -Algorithm SHA256).Hash`, then compare with the first word of `hidering-windows-x64.zip.sha256`.

| File | SHA256 |
|---|---|
| `hidering-linux-x64.tar.gz` | `a7ed3465dde98b22b3a18b3b0ae7c71945536ca7c7f54a3eda6ee3e0ea3087bf` |
| `hidering-macos-arm64.tar.gz` | `95c2bd0b635759830c8df995e09d24dff8f83069070d8f7bda0300aea19fa88e` |
| `hidering-windows-x64.zip` | `5c5a81857de36bc27463d53394ad3a8ed15dc276f7ba8daa2f1d45bf0fadd7b1` |

`--version` prints `Hidering 'Privacy Enhanced' (v2.1.0-release)`.

## Testing done for this release

- Linux and Windows: all three programs start (`--version`) on a processor without AVX-512.
- Wallet compatibility v2.0.3 → v2.1.0 → v2.0.3, offline, as described above.
- macOS: built and checked by CI (including a post-quantum derivation test on arm64); their libraries and minimum macOS version were checked by reading the files, but the binaries were not run by the maintainer.

**Full Changelog**: https://github.com/AB-lab113/hidering/compare/v2.0.3...v2.1.0
