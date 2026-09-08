# HIDERING (HRG)

An enhanced-privacy, ASIC-resistant cryptocurrency with a Bitcoin-style emission curve
and a post-quantum migration path.

HIDERING is a **fork of [Monero](https://github.com/monero-project/monero)** (CryptoNote
lineage). The privacy core — ring signatures, stealth addresses, RingCT — is Monero's work,
and the debt is acknowledged throughout this file. What HIDERING changes is documented in
[Differences from Monero](#differences-from-monero).

Copyright (c) 2026, The HIDERING Project
Portions Copyright (c) 2014-2024, The Monero Project
Portions Copyright (c) 2012-2013 The Cryptonote developers

## Table of Contents

  - [Project resources](#project-resources)
  - [Vulnerability response](#vulnerability-response)
  - [Introduction](#introduction)
  - [Differences from Monero](#differences-from-monero)
  - [Network parameters](#network-parameters)
  - [Post-Quantum Cryptography](#post-quantum-cryptography)
  - [Releases](#releases)
  - [Network upgrade schedule](#network-upgrade-schedule)
  - [About this project](#about-this-project)
  - [License](#license)
  - [Contributing](#contributing)
  - [Compiling HIDERING from source](#compiling-hidering-from-source)
    - [Dependencies](#dependencies)
  - [Installing from a package](#installing-from-a-package)
  - [Running hideringd](#running-hideringd)
  - [Internationalization](#internationalization)
  - [Using Tor](#using-tor)
  - [Pruning](#pruning)
  - [Debugging](#debugging)
  - [Known issues](#known-issues)

## Project resources

- Web: [hidering.org](https://hidering.org)
- Source: [github.com/AB-lab113/hidering](https://github.com/AB-lab113/hidering)
- GUI wallet: [github.com/AB-lab113/hidering-gui](https://github.com/AB-lab113/hidering-gui)
- Block explorer: [explorer.hidering.org](https://explorer.hidering.org)
- Mining pool: `pool.hidering.org:3333`
- Whitepaper: [docs/whitepaper_v1.4.md](docs/whitepaper_v1.4.md) ([web version](https://hidering.org/whitepaper.html))
- Genesis proof: [GENESIS_PROOF.md](GENESIS_PROOF.md)

HIDERING is a small project. There is no IRC channel, no mailing list and no forum thread —
use GitHub issues. Anything claiming to be an official HIDERING channel elsewhere is not.

## Vulnerability response

Report security issues **privately**, through
[GitHub security advisories](https://github.com/AB-lab113/hidering/security/advisories/new)
on this repository. Please do not open a public issue for a vulnerability, and please give us
a reasonable window to ship a fix before disclosing.

HIDERING has no bug bounty programme and is not on HackerOne. Since HIDERING tracks Monero,
vulnerabilities in inherited code should also be reported to the
[Monero Vulnerability Response Process](https://github.com/monero-project/meta/blob/master/VULNERABILITY_RESPONSE_PROCESS.md).

## Introduction

HIDERING is a private, secure, decentralised digital currency. You are your own bank, you
control your funds, and nobody can trace your transfers unless you allow them to.

**Privacy.** Transactions hide the sender in a ring of decoys, the recipient behind a
one-time stealth address, and the amount behind a Pedersen commitment (RingCT). HIDERING
raises the ring size to a dynamic 32–64, above Monero's fixed 16.

**Security.** Every transaction is cryptographically secured by a distributed peer-to-peer
consensus network. Wallets have a 25-word mnemonic seed, displayed once, which is the only
thing you need to back up. Wallet files should be encrypted with a strong passphrase.

**Decentralisation.** Proof of work is RandomX — CPU-friendly and ASIC-hostile — so ordinary
hardware can compete. Emission is 100% PoW: no ICO, no private sale, no seed round. See
[§7 of the whitepaper](docs/whitepaper_v1.4.md) for the pre-public mining phase and what it
means, stated plainly.

## Differences from Monero

| | Monero | HIDERING |
|---|---|---|
| Ticker | XMR | HRG |
| Supply cap | tail emission, no cap | **18,000,000 HRG** hard cap |
| Initial block reward | — | 42.86 HRG |
| Halving | none (tail emission) | every 210,000 blocks (~2.66 years) |
| Block time | 120 s | 120 s |
| Ring size | fixed 16 | **dynamic 32–64** |
| Address prefix | `4...` | `B...` (and `BQ...` post-quantum, see below) |
| PoW | RandomX | RandomX (unchanged) |
| Post-quantum | research | ML-DSA-65 + ML-KEM-768 implemented, inert until HFv16 |

Everything else — the CryptoNote transaction model, CLSAG, bulletproofs+, view tags, the LMDB
blockchain, the wallet architecture — is Monero's, and upstream security fixes are backported.

## Network parameters

| | Mainnet |
|---|---|
| Network ID | `HRG\x02HIDERINGMAIN` |
| Magic bytes | `0x48524702` |
| P2P port | 19740 |
| RPC port | 19741 |
| Address prefix | 60 (`B...`) |
| Atomic units | 10^12 per HRG |
| Coinbase unlock window | 60 blocks |
| Seed nodes | `seed1.hidering.org` … `seed4.hidering.org` |

The chain embeds checkpoint anchors at heights 2939, 5000, 11000, 16000, 20000, 25000 and
80386. They speed up initial sync and pin the chain against deep reorganisation; consensus
security still rests on cumulative RandomX proof of work.

## Post-Quantum Cryptography

HIDERING implements a post-quantum spend path, **inert until the HFv16 hard fork**
(`HF_HEIGHT_PQ` = 2,000,000, mainnet target Q2 2027). Nothing below affects the live chain.

- **Signatures:** ML-DSA-65 (FIPS 204), via [liboqs](https://github.com/open-quantum-safe/liboqs)
- **Key encapsulation:** ML-KEM-768 (FIPS 203)
- **Addresses:** a `BQ...` format carrying an ML-KEM-768 encapsulation key. The classic `B...`
  format is unchanged, on the wire and on disk.
- **Status:** the nominal path — keygen, key persistence, the C-1 binding, validator checks,
  the wallet spend side — is implemented, and the full `B...` → `BQ...` → spend flow is
  validated end-to-end on regtest.

**Read this before using a BQ address.** Spending a BQ output is **transparent**: a
post-quantum input carries no Pedersen commitment, so the transaction is built with revealed
amounts and **no ring signature** on that input. Receiving to a BQ address is as private as
receiving to a classic one; *spending* that output is as transparent as a Bitcoin
transaction. BQ addresses trade spend privacy for quantum resistance. Use `B...` for
everyday use. See [§6.2 of the whitepaper](docs/whitepaper_v1.4.md).

**Activation is not imminent, and not fast.** liboqs still carries its authors' own warning
against production use and holds no FIPS 140-3 validation; cold-signing a BQ spend, mixing
`B...` and `BQ...` funds in one transaction, and BQ subaddresses are all still open. Beyond
that, a hard fork needs binaries published for every platform and weeks of lead time for
miners, pools and holders to upgrade. Earlier versions of this file claimed activation was
possible "in 24–48h by changing a constant" — that was false and has been withdrawn.
See [§6.1 of the whitepaper](docs/whitepaper_v1.4.md) and `docs/audit/`.

## Releases

Binaries for Linux x64, macOS ARM64 and Windows x64 are published on the
[releases page](https://github.com/AB-lab113/hidering/releases), built by CI, each with a
`.sha256` sidecar. **Verify the checksum before running anything.**

- **Node + wallet CLI:** v2.0.3 — `hideringd`, `hidering-wallet-cli`, `hidering-wallet-rpc`
- **Desktop GUI wallet:** v2.0.2-gui, from the
  [hidering-gui](https://github.com/AB-lab113/hidering-gui/releases) repository (Linux
  AppImage, macOS `.app`, Windows portable)

Note that the v1.x chain was abandoned after a 51% attack in May 2026; the current chain
starts at v2.0.0 and **v1.x binaries cannot connect to it** (the network ID differs).

## Network upgrade schedule

HIDERING uses a scheduled network upgrade (hard fork) mechanism. Run a current version and
upgrade when new releases are published: a node left behind ends up on a dead chain.

Dates are YYYY-MM-DD. "Minimum" is the version that follows the new consensus rules.

| Fork height | Date | Fork version | Minimum version | Details |
| ----------- | ---- | ------------ | --------------- | ------- |
| 0 | chain launch | v1 | v1.0.0 | Genesis block. Its coinbase is locked under a NUMS key and is cryptographically unspendable — see [GENESIS_PROOF.md](GENESIS_PROOF.md). The v1 row exists only so the genesis coinbase validates. |
| 1 | chain launch | v15 | v1.0.0 | The chain runs at fork version 15 **from block 1**: CLSAG, bulletproofs+, view tags, dynamic ring 32–64. |
| — | 2026-05-16 | — | v2.0.0 | Not a consensus fork: a **network ID change** (`HRG\x01` → `HRG\x02`) following the 51% attack on the v1.x chain, which was abandoned. v1.x binaries are cut off at the P2P handshake. |
| 2000000 | not scheduled (target Q2 2027) | v16 | TBD | **Planned, not scheduled.** Post-quantum: ML-DSA-65 signatures, ML-KEM-768 KEM, `BQ...` addresses. The height is a placeholder and the activation date is not fixed — see [Post-Quantum Cryptography](#post-quantum-cryptography). |

HIDERING did **not** inherit Monero's upgrade history: the chain launched directly at fork
version 15, so Monero's v2–v14 heights and dates do not apply here.

## About this project

This is the reference implementation of HIDERING. It is open source and free to use, subject
only to the licence below. Anyone is welcome to write an alternative implementation that
speaks the same protocol.

The `master` branch tracks releases; day-to-day development happens on `v2-privacy`. As with
most projects, prefer a tagged release over a branch tip for anything that matters.

Contributions are welcome — open a pull request. Small, self-contained changes are easiest to
review; larger or consensus-affecting changes should be discussed in an issue first.

Design notes, security reviews and open architectural questions live in
[`docs/audit/`](docs/audit/) and in `CLAUDE.md`, which is the project's working log.

## License

BSD 3-Clause. See [LICENSE](LICENSE).

## Contributing

If you want to help out, see [CONTRIBUTING](docs/CONTRIBUTING.md) for a set of guidelines.

## Compiling HIDERING from source

### Dependencies

The following table summarizes the tools and libraries required to build. A
few of the libraries are also included in this repository (marked as
"Vendored"). By default, the build uses the library installed on the system
and ignores the vendored sources. However, if no library is found installed on
the system, then the vendored source will be built and used. The vendored
sources are also used for statically-linked builds because distribution
packages often include only shared library binaries (`.so`) but not static
library archives (`.a`).

| Dep          | Min. version  | Vendored | Debian/Ubuntu pkg    | Arch pkg     | Void pkg           | Fedora pkg          | Optional | Purpose         |
| ------------ | ------------- | -------- | -------------------- | ------------ | ------------------ | ------------------- | -------- | --------------- |
| GCC          | 7             | NO       | `build-essential`    | `base-devel` | `base-devel`       | `gcc`               | NO       |                 |
| CMake        | 3.10          | NO       | `cmake`              | `cmake`      | `cmake`            | `cmake`             | NO       |                 |
| pkg-config   | any           | NO       | `pkg-config`         | `base-devel` | `base-devel`       | `pkgconf`           | NO       |                 |
| Boost        | 1.66          | NO       | `libboost-all-dev`   | `boost`      | `boost-devel`      | `boost-devel`       | NO       | C++ libraries   |
| OpenSSL      | basically any | NO       | `libssl-dev`         | `openssl`    | `openssl-devel`    | `openssl-devel`     | NO       | sha256 sum      |
| libzmq       | 4.2.0         | NO       | `libzmq3-dev`        | `zeromq`     | `zeromq-devel`     | `zeromq-devel`      | NO       | ZeroMQ library  |
| libunbound   | 1.4.16        | NO       | `libunbound-dev`     | `unbound`    | `unbound-devel`    | `unbound-devel`     | NO       | DNS resolver    |
| libsodium    | ?             | NO       | `libsodium-dev`      | `libsodium`  | `libsodium-devel`  | `libsodium-devel`   | NO       | cryptography    |
| libunwind    | any           | NO       | `libunwind8-dev`     | `libunwind`  | `libunwind-devel`  | `libunwind-devel`   | YES      | Stack traces    |
| liblzma      | any           | NO       | `liblzma-dev`        | `xz`         | `liblzma-devel`    | `xz-devel`          | YES      | For libunwind   |
| libreadline  | 6.3.0         | NO       | `libreadline6-dev`   | `readline`   | `readline-devel`   | `readline-devel`    | YES      | Input editing   |
| expat        | 1.1           | NO       | `libexpat1-dev`      | `expat`      | `expat-devel`      | `expat-devel`       | YES      | XML parsing     |
| GTest        | 1.5           | YES      | `libgtest-dev`       | `gtest`      | `gtest-devel`      | `gtest-devel`       | YES      | Test suite      |
| ccache       | any           | NO       | `ccache`             | `ccache`     | `ccache`           | `ccache`            | YES      | Compil. cache   |
| Doxygen      | any           | NO       | `doxygen`            | `doxygen`    | `doxygen`          | `doxygen`           | YES      | Documentation   |
| Graphviz     | any           | NO       | `graphviz`           | `graphviz`   | `graphviz`         | `graphviz`          | YES      | Documentation   |
| lrelease     | ?             | NO       | `qttools5-dev-tools` | `qt5-tools`  | `qt5-tools`        | `qt5-linguist`      | YES      | Translations    |
| libhidapi    | ?             | NO       | `libhidapi-dev`      | `hidapi`     | `hidapi-devel`     | `hidapi-devel`      | YES      | Hardware wallet |
| libusb       | ?             | NO       | `libusb-1.0-0-dev`   | `libusb`     | `libusb-devel`     | `libusbx-devel`     | YES      | Hardware wallet |
| libprotobuf  | ?             | NO       | `libprotobuf-dev`    | `protobuf`   | `protobuf-devel`   | `protobuf-devel`    | YES      | Hardware wallet |
| protoc       | ?             | NO       | `protobuf-compiler`  | `protobuf`   | `protobuf`         | `protobuf-compiler` | YES      | Hardware wallet |
| libudev      | ?             | NO       | `libudev-dev`        | `systemd`    | `eudev-libudev-devel` | `systemd-devel`  | YES      | Hardware wallet |

Install all dependencies at once on Debian/Ubuntu:

```
sudo apt update && sudo apt install build-essential cmake pkg-config libssl-dev libzmq3-dev libunbound-dev libsodium-dev libunwind8-dev liblzma-dev libreadline6-dev libexpat1-dev qttools5-dev-tools libhidapi-dev libusb-1.0-0-dev libprotobuf-dev protobuf-compiler libudev-dev libboost-chrono-dev libboost-date-time-dev libboost-filesystem-dev libboost-locale-dev libboost-program-options-dev libboost-regex-dev libboost-serialization-dev libboost-system-dev libboost-thread-dev python3 ccache doxygen graphviz git curl autoconf libtool gperf
```

Install all dependencies at once on Arch:
```
sudo pacman -Syu --needed base-devel cmake boost openssl zeromq unbound libsodium libunwind xz readline expat python3 ccache doxygen graphviz qt5-tools hidapi libusb protobuf systemd
```

Install all dependencies at once on Fedora:
```
sudo dnf install gcc gcc-c++ cmake pkgconf boost-devel openssl-devel zeromq-devel unbound-devel libsodium-devel libunwind-devel xz-devel readline-devel expat-devel ccache doxygen graphviz qt5-linguist hidapi-devel libusbx-devel protobuf-devel protobuf-compiler systemd-devel
```

Install all dependencies at once on openSUSE:

```
sudo zypper ref && sudo zypper in cppzmq-devel libboost_chrono-devel libboost_date_time-devel libboost_filesystem-devel libboost_locale-devel libboost_program_options-devel libboost_regex-devel libboost_serialization-devel libboost_system-devel libboost_thread-devel libexpat-devel libminiupnpc-devel libsodium-devel libunwind-devel unbound-devel cmake doxygen ccache fdupes gcc-c++ libevent-devel libopenssl-devel pkgconf-pkg-config readline-devel xz-devel libqt5-qttools-devel patterns-devel-C-C++-devel_C_C++
```

Install all dependencies at once on macOS with the provided Brewfile:

```
brew update && brew bundle --file=contrib/brew/Brewfile
```

FreeBSD 12.1 one-liner required to build dependencies:

```
pkg install git gmake cmake pkgconf boost-libs libzmq4 libsodium unbound
```

### Cloning the repository

Clone recursively to pull-in needed submodule(s):

```
git clone --recursive https://github.com/AB-lab113/hidering
```

If you already have a repo cloned, initialize and update:

```
cd hidering && git submodule init && git submodule update
```

*Note*: If there are submodule differences between branches, you may need 
to use `git submodule sync && git submodule update` after changing branches
to build successfully.

### Build instructions

HIDERING uses the CMake build system and a top-level [Makefile](Makefile) that
invokes cmake commands as needed.

#### HIDERING-specific build notes

Two things differ from an ordinary Monero build and will bite you otherwise:

* **Build liboqs first.** The post-quantum sources include `<oqs/oqs.h>`
  unconditionally, so the submodule has to be built before the main tree:

    ```bash
    cmake -S external/liboqs -B external/liboqs/build \
          -DBUILD_SHARED_LIBS=OFF -DOQS_USE_OPENSSL=ON -DOQS_BUILD_ONLY_LIB=ON
    cmake --build external/liboqs/build -j$(nproc)
    ```

  Before building it, confirm the pin — the submodule SHA *is* the integrity anchor,
  since OQS publishes neither signatures nor checksums for releases:

    ```bash
    git -C external/liboqs rev-parse HEAD          # 97f6b86b1b6d109cfd43cf276ae39c2e776aed80
    git -C external/liboqs tag --points-at HEAD    # 0.15.0
    ```

* **Do not link statically.** `-DSTATIC=ON` breaks libunbound. The official binaries are
  dynamically linked; `unbound` and `zeromq` are required.

* **Watch memory on the wallet.** `wallet2.cpp` is a very large translation unit and
  routinely gets OOM-killed at high parallelism on machines with less than ~8 GB free.
  Build the wallet targets with `make -j2` (or `-j1`) even when the rest of the tree
  builds fine at `-j$(nproc)`.

#### On Linux and macOS

* Install the dependencies
* Change to the root of the source code directory, change to the most recent release branch, and build:

    ```bash
    cd hidering
    git checkout release-v0.18
    make
    ```

    *Optional*: If your machine has several cores and enough memory, enable
    parallel build by running `make -j<number of threads>` instead of `make`. For
    this to be worthwhile, the machine should have one core and about 2GB of RAM
    available per thread.

    *Note*: The instructions above compile the most stable release. If you want to
    use and test the most recent code, use `git checkout v2-privacy` — that is where
    HIDERING development happens. It may contain updates that are unstable or
    incompatible with released software, though testing is always encouraged.

* The resulting executables can be found in `build/release/bin`

* Add `PATH="$PATH:$HOME/hidering/build/release/bin"` to `.profile`

* Run HIDERING with `hideringd --detach`

* **Optional**: build and run the test suite to verify the binaries:

    ```bash
    make release-test
    ```

    *NOTE*: `core_tests` test may take a few hours to complete.

* **Optional**: to build binaries suitable for debugging:

    ```bash
    make debug
    ```

* **Optional**: build documentation in `doc/html` (omit `HAVE_DOT=YES` if `graphviz` is not installed):

    ```bash
    HAVE_DOT=YES doxygen Doxyfile
    ```

* **Optional**: use ccache not to rebuild translation units, that haven't really changed. HIDERING's CMakeLists.txt file automatically handles it

    ```bash
    sudo apt install ccache
    ```

#### On the Raspberry Pi

Tested on a Raspberry Pi 5B with a clean installation of Raspberry Pi OS (64-bit) with Debian 12 from https://www.raspberrypi.com/software/operating-systems/.

* `apt-get update && apt-get upgrade` to install the latest software

* Install the dependencies for HIDERING from the 'Debian' column in the table above.

* **Optional**: increase the system swap size:

    ```bash
    sudo /etc/init.d/dphys-swapfile stop  
    sudo nano /etc/dphys-swapfile  
    CONF_SWAPSIZE=2048
    sudo /etc/init.d/dphys-swapfile start
    ```

* If using an external hard disk without an external power supply, ensure it gets enough power to avoid hardware issues when syncing, by adding the line "max_usb_current=1" to /boot/config.txt

* Clone HIDERING and checkout the most recent release version:

    ```bash
    git clone --recursive https://github.com/AB-lab113/hidering.git
    cd hidering
    git checkout v0.18.4.1
    ```

* Build:

    ```bash
    USE_SINGLE_BUILDDIR=1 make release
    ```

* Wait a few hours

* The resulting executables can be found in `build/release/bin`

* Add `export PATH="$PATH:$HOME/hidering/build/release/bin"` to `$HOME/.profile`

* Run `source $HOME/.profile`

* Run HIDERING with `hideringd --detach`

* You may wish to reduce the size of the swap file after the build has finished, and delete the boost directory from your home directory

#### On Windows:

Binaries for Windows can be built on Windows using the MinGW toolchain within
[MSYS2 environment](https://www.msys2.org). The MSYS2 environment emulates a
POSIX system. The toolchain runs within the environment and *cross-compiles*
binaries that can run outside of the environment as a regular Windows
application.

**Preparing the build environment**

* Download and install the [MSYS2 installer](https://www.msys2.org). Installing MSYS2 requires 64-bit Windows 10 or newer.
* Open the MSYS shell via the `MSYS2 MSYS` shortcut
* Update packages using pacman:

    ```bash
    pacman -Syu
    ```

* Install dependencies:

    ```bash
    pacman -S mingw-w64-x86_64-toolchain make mingw-w64-x86_64-cmake mingw-w64-x86_64-boost mingw-w64-x86_64-openssl mingw-w64-x86_64-zeromq mingw-w64-x86_64-libsodium mingw-w64-x86_64-hidapi mingw-w64-x86_64-unbound
    ```

* Open the MingW shell via `MSYS2 MINGW64` shortcut.

**Cloning**

* To git clone, run:

    ```bash
    git clone --recursive https://github.com/AB-lab113/hidering.git
    ```

**Building**

* Change to the cloned directory, run:

    ```bash
    cd hidering
    ```

* If you would like a specific [version/tag](https://github.com/AB-lab113/hidering/tags), do a git checkout for that version. eg. 'v2.0.3'. If you don't care about the version and just want binaries from master, skip this step:

    ```bash
    git checkout v0.18.4.1
    ```

* To build HIDERING, run:

    ```bash
    make release-static -j $(nproc)
    ```

   The resulting executables can be found in `build/release/bin`


* **Optional**: to build Windows binaries suitable for debugging, run:

    ```bash
    make debug -j $(nproc)
    ```

   The resulting executables can be found in `build/debug/bin`

### On FreeBSD:

The project can be built from scratch by following instructions for Linux above(but use `gmake` instead of `make`). 
If you are running HIDERING in a jail, you need to add `sysvsem="new"` to your jail configuration, otherwise lmdb will throw the error message: `Failed to open lmdb environment: Function not implemented`.

There is no FreeBSD port or package for HIDERING; build from source as above.

### On OpenBSD:

You will need to add a few packages to your system. `pkg_add cmake gmake zeromq libiconv boost libunbound`.

The `doxygen` and `graphviz` packages are optional and require the xbase set.
Running the test suite also requires `py3-requests` package.

Build HIDERING: `gmake`

Note: you may encounter the following error when compiling the latest version of HIDERING as a normal user:

```
LLVM ERROR: out of memory
c++: error: unable to execute command: Abort trap (core dumped)
```

Then you need to increase the data ulimit size to 2GB and try again: `ulimit -d 2000000`

### On NetBSD:

Check that the dependencies are present: `pkg_info -c libexecinfo boost-headers boost-libs protobuf readline libusb1 zeromq git-base pkgconf gmake cmake | more`, and install any that are reported missing, using `pkg_add` or from your pkgsrc tree.  Readline is optional but worth having.

Third-party dependencies are usually under `/usr/pkg/`, but if you have a custom setup, adjust the "/usr/pkg" (below) accordingly.

Clone the HIDERING repository recursively and checkout the most recent release as described above. Then build HIDERING: `gmake BOOST_ROOT=/usr/pkg LDFLAGS="-Wl,-R/usr/pkg/lib" release`.  The resulting executables can be found in `build/NetBSD/[Release version]/Release/bin/`.

### On Solaris:

The default Solaris linker can't be used, you have to install GNU ld, then run cmake manually with the path to your copy of GNU ld:

```bash
mkdir -p build/release
cd build/release
cmake -DCMAKE_LINKER=/path/to/ld -D CMAKE_BUILD_TYPE=Release ../..
cd ../..
```

Then you can run make as usual.

### Cross Compiling

You can also cross-compile static binaries on Linux for Windows and macOS with the `depends` system.

* ```make depends target=x86_64-linux-gnu``` for 64-bit linux binaries.
* ```make depends target=x86_64-w64-mingw32``` for 64-bit windows binaries.
  * Requires: `g++-mingw-w64-x86-64`
  * You also need to run:
    ```shell
    update-alternatives --set x86_64-w64-mingw32-g++ $(which x86_64-w64-mingw32-g++-posix) && \
    update-alternatives --set x86_64-w64-mingw32-gcc $(which x86_64-w64-mingw32-gcc-posix)
    ```
* ```make depends target=x86_64-apple-darwin``` for Intel macOS binaries.
  * Requires: `clang-18 lld-18`
* ```make depends target=arm64-apple-darwin``` for Apple Silicon macOS binaries.
  * Requires: `clang-18 lld-18`
  * You also need to run:
    ```shell
    export PATH="/usr/lib/llvm-18/bin/:$PATH"
    ```
* ```make depends target=i686-linux-gnu``` for 32-bit linux binaries.
  * Requires: `g++-multilib bc`
* ```make depends target=i686-w64-mingw32``` for 32-bit windows binaries.
  * Requires: `python3 g++-mingw-w64-i686`
* ```make depends target=arm-linux-gnueabihf``` for armv7 binaries.
  * Requires: `g++-arm-linux-gnueabihf`
* ```make depends target=aarch64-linux-gnu``` for armv8 binaries.
  * Requires: `g++-aarch64-linux-gnu`
* ```make depends target=riscv64-linux-gnu``` for RISC V 64 bit binaries.
  * Requires: `g++-riscv64-linux-gnu`
* ```make depends target=x86_64-unknown-freebsd``` for freebsd binaries.
  * Requires: `clang-8`
* ```make depends target=arm-linux-android``` for 32bit android binaries
* ```make depends target=aarch64-linux-android``` for 64bit android binaries


The required packages are the names for each toolchain on apt. Depending on your distro, they may have different names. The `depends` system has been tested on Ubuntu 18.04 and 20.04.

Using `depends` might also be easier to compile HIDERING on Windows than using MSYS. Activate Windows Subsystem for Linux (WSL) with a distro (for example Ubuntu), install the apt build-essentials and follow the `depends` steps as depicted above.

The produced binaries still link libc dynamically. If the binary is compiled on a current distribution, it might not run on an older distribution with an older installation of libc.

### Trezor hardware wallet support

If you have an issue with building HIDERING with Trezor support, you can disable it by setting `USE_DEVICE_TREZOR=OFF`, e.g., 

```bash
USE_DEVICE_TREZOR=OFF make release
```

For more information, please check out Trezor [src/device_trezor/README.md](src/device_trezor/README.md).

**Note:** Trezor support is inherited from Monero and is **not supported for HRG** — Trezor
firmware does not know about the HIDERING network. The code builds; it has not been tested
against a device on this chain. Building with `USE_DEVICE_TREZOR=OFF` is the safe default.

### Guix builds

Inherited from Monero and **not exercised by HIDERING** — the manifests still describe
Monero's build. See [contrib/guix/README.md](contrib/guix/README.md) if you want to adapt it.
Official HIDERING binaries are produced by the GitHub Actions workflows in
[.github/workflows](.github/workflows).

## Installing from a package

**No distribution packages HIDERING.** `apt install monero`, `brew install monero` and
friends install *Monero*, not HIDERING — do not follow Monero's packaging instructions and
expect an HRG node.

Use one of:

* the [official release binaries](https://github.com/AB-lab113/hidering/releases) (verify the
  `.sha256` sidecar), or
* a build from source, as described above.

A [Dockerfile](Dockerfile) is included and builds a node image:

```bash
# Build using all available cores (needs ~3 GB of disk, and a while)
docker build -t hidering .

# or with a fixed number of cores, to cap RAM use
docker build --build-arg NPROC=1 -t hidering .

# run, exposing P2P 19740 (add -p 19741:19741 to expose RPC)
docker run -it -v /hidering/chain:/home/hidering/.hidering -v /hidering/wallet:/wallet \
  -p 19740:19740 hidering
```

Packaging HIDERING for your favourite distribution would be a welcome contribution.

## Running hideringd

The build places the binary in `bin/` sub-directory within the build directory
from which cmake was invoked (repository root by default). To run in the
foreground:

```bash
./bin/hideringd
```

To list all available options, run `./bin/hideringd --help`.  Options can be
specified either on the command line or in a configuration file passed by the
`--config-file` argument.  To specify an option in the configuration file, add
a line with the syntax `argumentname=value`, where `argumentname` is the name
of the argument without the leading dashes, for example, `log-level=1`.

To run in background:

```bash
./bin/hideringd --log-file hideringd.log --detach
```

To run as a systemd service, copy
[monerod.service](utils/systemd/monerod.service) to `/etc/systemd/system/` and
[monerod.conf](utils/conf/monerod.conf) to `/etc/`, then edit both to point at the
`hideringd` binary and an HRG data directory. (These example files still carry their
upstream names and defaults — they are inherited from Monero and have not been rebranded, so
read them before use. The HIDERING seed nodes run a unit of this shape with
`--p2p-bind-port 19740 --rpc-bind-port 19741 --restricted-rpc`.)

If you're on Mac, you may need to add the `--max-concurrency 1` option to
`hidering-wallet-cli`, and possibly `hideringd`, if you get crashes refreshing.

## Internationalization

See [README.i18n.md](docs/README.i18n.md).

## Using Tor

> There is a new, still experimental, [integration with Tor](docs/ANONYMITY_NETWORKS.md). The
> feature allows connecting over IPv4 and Tor simultaneously - IPv4 is used for
> relaying blocks and relaying transactions received by peers whereas Tor is
> used solely for relaying transactions received over local RPC. This provides
> privacy and better protection against surrounding node (sybil) attacks.

While HIDERING isn't made to integrate with Tor, it can be used wrapped with torsocks, by
setting the following configuration parameters and environment variables:

* `--p2p-bind-ip 127.0.0.1` on the command line or `p2p-bind-ip=127.0.0.1` in
  hideringd.conf to disable listening for connections on external interfaces.
* `--no-igd` on the command line or `no-igd=1` in hideringd.conf to disable IGD
  (UPnP port forwarding negotiation), which is pointless with Tor.
* If you use the wallet with a Tor daemon via the loopback IP (eg, 127.0.0.1:9050),
  then use `--untrusted-daemon` unless it is your own hidden service.

Example command line to start hideringd through Tor:

```bash
hideringd --proxy 127.0.0.1:9050 --p2p-bind-ip 127.0.0.1 --no-igd
```

A helper script is in contrib/tor/monero-over-tor.sh (inherited name). It assumes Tor is
installed already; edit the binary name inside it before use.

### Using Tor on Tails

TAILS ships with a very restrictive set of firewall rules. Therefore, you need
to add a rule to allow this connection too, in addition to telling torsocks to
allow inbound connections. Full example:

```bash
sudo iptables -I OUTPUT 2 -p tcp -d 127.0.0.1 -m tcp --dport 18081 -j ACCEPT
DNS_PUBLIC=tcp torsocks ./hideringd --p2p-bind-ip 127.0.0.1 --no-igd --rpc-bind-ip 127.0.0.1 \
    --data-dir /home/amnesia/Persistent/your/directory/to/the/blockchain
```

## Pruning

The HIDERING blockchain is young and small — a few hundred MB as of September 2026 — so pruning is not needed yet. The mechanism below is inherited from Monero and works the same way; the sizes quoted in Monero's documentation do not apply to HRG.
A pruned blockchain can only serve part of the historical chain data to other peers, but is otherwise identical in
functionality to the full blockchain.
To use a pruned blockchain, it is best to start the initial sync with `--prune-blockchain`. However, it is also possible
to prune an existing blockchain using the `hidering-blockchain-prune` tool or using the `--prune-blockchain` `hideringd` option
with an existing chain. If an existing chain exists, pruning will temporarily require disk space to store both the full
and pruned blockchains.

For more detailed background on how pruning works, see the ['Pruning' entry in the Moneropedia](https://www.getmonero.org/resources/moneropedia/pruning.html) (Monero documentation; the mechanism is the same)

## Debugging

This section contains general instructions for debugging failed installs or problems encountered with HIDERING. First, ensure you are running the latest version built from the GitHub repo.

### Obtaining stack traces and core dumps on Unix systems

We generally use the tool `gdb` (GNU debugger) to provide stack trace functionality, and `ulimit` to provide core dumps in builds which crash or segfault.

* To use `gdb` in order to obtain a stack trace for a build that has stalled:

Run the build.

Once it stalls, enter the following command:

```bash
gdb /path/to/hideringd `pidof hideringd`
```

Type `thread apply all bt` within gdb in order to obtain the stack trace

* If however the core dumps or segfaults:

Enter `ulimit -c unlimited` on the command line to enable unlimited filesizes for core dumps

Enter `echo core | sudo tee /proc/sys/kernel/core_pattern` to stop cores from being hijacked by other tools

Run the build.

When it terminates with an output along the lines of "Segmentation fault (core dumped)", there should be a core dump file in the same directory as hideringd. It may be named just `core`, or `core.xxxx` with numbers appended.

You can now analyse this core dump with `gdb` as follows:

```bash
gdb /path/to/hideringd /path/to/dumpfile`
```

Print the stack trace with `bt`

 * If a program crashed and cores are managed by systemd, the following can also get a stack trace for that crash:

```bash
coredumpctl -1 gdb
```

#### To run HIDERING within gdb:

Type `gdb /path/to/hideringd`

Pass command-line options with `--args` followed by the relevant arguments

Type `run` to run hideringd

### Analysing memory corruption

There are two tools available:

#### ASAN

Configure HIDERING with the -D SANITIZE=ON cmake flag, eg:

```bash
cd build/debug && cmake -D SANITIZE=ON -D CMAKE_BUILD_TYPE=Debug ../..
```

You can then run the HIDERING tools normally. Performance will typically halve.

#### valgrind

Install valgrind and run as `valgrind /path/to/hideringd`. It will be very slow.

### LMDB

Instructions for debugging suspected blockchain corruption as per @HYC

There is an `mdb_stat` command in the LMDB source that can print statistics about the database but it's not routinely built. This can be built with the following command:

```bash
cd ~/hidering/external/db_drivers/liblmdb && make
```

The output of `mdb_stat -ea <path to blockchain dir>` will indicate inconsistencies in the blocks, block_heights and block_info table.

The output of `mdb_dump -s blocks <path to blockchain dir>` and `mdb_dump -s block_info <path to blockchain dir>` is useful for indicating whether blocks and block_info contain the same keys.

These records are dumped as hex data, where the first line is the key and the second line is the data.

# Known Issues

## Protocols

### Socket-based

Because of the nature of the socket-based protocols that drive HIDERING, certain protocol weaknesses are somewhat unavoidable at this time. While these weaknesses can theoretically be fully mitigated, the effort required (the means) may not justify the ends. As such, please consider taking the following precautions if you are a HIDERING node operator:

- Run `hideringd` on a "secured" machine. If operational security is not your forte, at a very minimum, have a dedicated a computer running `hideringd` and **do not** browse the web, use email clients, or use any other potentially harmful apps on your `hideringd` machine. **Do not click links or load URL/MUA content on the same machine**. Doing so may potentially exploit weaknesses in commands which accept "localhost" and "127.0.0.1".
- If you plan on hosting a public "remote" node, start `hideringd` with `--restricted-rpc`. This is a must.

### Blockchain-based

Certain blockchain "features" can be considered "bugs" if misused correctly. Consequently, please consider the following:

- When receiving HRG, be aware that it may be locked for an arbitrary time if the sender elected to, preventing you from spending those coins until the lock time expires. You may want to hold off acting upon such a transaction until the unlock time lapses. To get a sense of that time, you can consider the remaining blocktime until unlock as seen in the `show_transfers` command.

---

## Ring Size Analysis

A comprehensive analysis of ring sizes has been conducted to determine optimal parameters for privacy and efficiency.

📊 **[View Full Ring Size Analysis](RING_SIZE_ANALYSIS.md)**

### Key Finding

**Ring Size 48 is recommended** as the optimal choice, providing:
- Best privacy/efficiency balance
- Lowest transaction fees (0.024 HRG)
- Smallest transaction size (3,436 bytes)
- No transaction splitting required

For detailed test results and methodology, see [RING_SIZE_ANALYSIS.md](RING_SIZE_ANALYSIS.md).

