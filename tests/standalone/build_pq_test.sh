#!/usr/bin/env bash
#
# Build one of the standalone post-quantum tests (src/crypto/pq*_test.cpp, pqc_test.cpp).
#
# WHY THIS FILE EXISTS
# These tests deliberately have no CMake target: they link against the already-built static
# libraries and are compiled by hand, which keeps them out of the daemon/wallet build and lets
# them poke at internals (see the wallet_accessor_test friend hook in wallet2.h). The recipe
# used to live only in throwaway scratch directories and had to be reconstructed from memory
# more than once. Anything CI depends on has to be in the repository, so here it is.
#
# USAGE
#   tests/standalone/build_pq_test.sh <test-name> [output-dir]
#   tests/standalone/build_pq_test.sh --list
#
#   <test-name>   basename without .cpp, e.g. pq_vector_test
#   [output-dir]  where to put the binary (default: ./build/standalone)
#
# PREREQUISITES
#   1. liboqs built:  cmake -S external/liboqs -B external/liboqs/build \
#                       -DBUILD_SHARED_LIBS=OFF -DOQS_USE_OPENSSL=ON -DOQS_BUILD_ONLY_LIB=ON
#                     cmake --build external/liboqs/build -j
#   2. the project's static libs built, at least `make cncrypto` — and `make wallet` for the
#      wallet-linked tests. The build directory is auto-detected (build/release, then build).
#
# LINKING
# Two library sets. Most tests need only the crypto set; those that drive wallet2 (cold-sign,
# subaddress, clawback, root-restore, auth-binding) need libwallet and its dependency closure.
# The script picks by test name and falls back to the full set if the crypto link fails, which
# is what the hand recipe did in practice.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO"

WALLET_LINKED="pq_coldsign_v4_test pq_subaddress_test pq_sender_clawback_test pq_root_restore_test pq_auth_binding_test"

if [ "${1:-}" = "--list" ]; then
  for f in src/crypto/pq*_test.cpp; do
    [ -e "$f" ] || continue
    n="$(basename "$f" .cpp)"
    case " $WALLET_LINKED " in *" $n "*) echo "$n (wallet-linked)";; *) echo "$n";; esac
  done
  exit 0
fi

TEST="${1:?usage: $0 <test-name> [output-dir]   (try --list)}"
OUT_DIR="${2:-$REPO/build/standalone}"
SRC="$REPO/src/crypto/${TEST}.cpp"
[ -f "$SRC" ] || { echo "no such test: $SRC" >&2; exit 2; }
mkdir -p "$OUT_DIR"

# --- locate the project build directory -------------------------------------------------
B=""
for cand in build/release build; do
  if [ -f "$REPO/$cand/src/crypto/libcncrypto.a" ]; then B="$REPO/$cand"; break; fi
done
[ -n "$B" ] || { echo "no build dir with libcncrypto.a (looked in build/release, build)" >&2
                 echo "build the project first, e.g.: make -C build/release cncrypto" >&2; exit 2; }

OQS_LIB="$REPO/external/liboqs/build/lib/liboqs.a"
[ -f "$OQS_LIB" ] || { echo "liboqs not built: $OQS_LIB missing (see PREREQUISITES above)" >&2; exit 2; }

INCLUDES=(-I "$REPO/src" -I "$REPO/contrib/epee/include" -I "$REPO/external"
          -I "$REPO/external/easylogging++" -I "$REPO/external/rapidjson/include"
          -I "$REPO/external/liboqs/build/include" -I "$B/src" -I "$B/translations"
          -I "$REPO/external/supercop/include")

CRYPTO_LIBS=(
  "$B/src/cryptonote_basic/libcryptonote_basic.a"
  "$B/src/cryptonote_basic/libcryptonote_format_utils_basic.a"
  "$B/src/ringct/libringct.a" "$B/src/ringct/libringct_basic.a"
  "$B/src/device/libdevice.a" "$B/src/common/libcommon.a"
  "$B/src/crypto/libcncrypto.a" "$B/src/crypto/wallet/libwallet-crypto.a"
  "$B/contrib/epee/src/libepee.a" "$B/external/easylogging++/libeasylogging.a"
  "$B/src/checkpoints/libcheckpoints.a" "$B/external/randomx/librandomx.a"
  "$B/src/libversion.a" "$OQS_LIB"
)

WALLET_LIBS=(
  "$B/lib/libwallet.a" "$B/src/multisig/libmultisig.a"
  "$B/src/cryptonote_core/libcryptonote_core.a"
  "$B/src/cryptonote_basic/libcryptonote_basic.a"
  "$B/src/cryptonote_basic/libcryptonote_format_utils_basic.a"
  "$B/src/blockchain_db/libblockchain_db.a"
  "$B/src/ringct/libringct.a" "$B/src/ringct/libringct_basic.a"
  "$B/src/device/libdevice.a" "$B/src/device_trezor/libdevice_trezor.a"
  "$B/src/mnemonics/libmnemonics.a"
  "$B/src/rpc/librpc_base.a" "$B/src/rpc/libdaemon_messages.a" "$B/src/rpc/librpc.a"
  "$B/src/net/libnet.a" "$B/src/common/libcommon.a"
  "$B/src/checkpoints/libcheckpoints.a" "$B/src/hardforks/libhardforks.a"
  "$B/src/crypto/libcncrypto.a" "$B/src/crypto/wallet/libwallet-crypto.a"
  "$B/contrib/epee/src/libepee.a" "$B/external/easylogging++/libeasylogging.a"
  "$B/src/libversion.a" "$B/external/randomx/librandomx.a"
  "$B/src/blocks/libblocks.a" "$B/src/serialization/libserialization.a"
  "$OQS_LIB" "$B/external/db_drivers/liblmdb/liblmdb.a" "$B/src/lmdb/liblmdb_lib.a"
)

BOOST_NAMES=(system filesystem thread serialization program_options chrono regex date_time)
BOOST=()
SYS=(-lsodium -lssl -lcrypto -lunbound -lpthread)

# --- platform differences ---------------------------------------------------------------
# macOS: Apple ld has no --start-group (archives are searched repeatedly anyway), no -ldl,
# and Homebrew keeps its libraries outside the default search path.
EXTRA_LDFLAGS=()
GROUP_BEGIN=(); GROUP_END=()
case "$(uname -s)" in
  Darwin)
    BOOST_LIBDIR=""
    for f in openssl unbound libsodium boost; do
      p="$(brew --prefix "$f" 2>/dev/null || true)"
      [ -n "$p" ] && [ -d "$p/lib" ] && EXTRA_LDFLAGS+=("-L$p/lib")
      [ -n "$p" ] && [ -d "$p/include" ] && INCLUDES+=(-I "$p/include")
      [ "$f" = boost ] && [ -n "$p" ] && BOOST_LIBDIR="$p/lib"
    done
    # Boost.System has been header-only since 1.69 and Homebrew no longer ships a stub for it,
    # while Linux distributions still do. Asking for a -l that has no library is a hard link
    # error, so only request the ones that are actually on disk.
    for n in "${BOOST_NAMES[@]}"; do
      if [ -z "$BOOST_LIBDIR" ] \
         || [ -e "$BOOST_LIBDIR/libboost_$n.dylib" ] || [ -e "$BOOST_LIBDIR/libboost_$n.a" ] \
         || [ -e "$BOOST_LIBDIR/libboost_$n-mt.dylib" ]; then
        BOOST+=("-lboost_$n")
      else
        echo "note: skipping -lboost_$n (not present in $BOOST_LIBDIR)" >&2
      fi
    done
    ;;
  *)
    for n in "${BOOST_NAMES[@]}"; do BOOST+=("-lboost_$n"); done
    SYS+=(-ldl)
    # hidapi is only linked into libdevice on Linux builds that found it
    if [ -e /usr/lib/x86_64-linux-gnu/libhidapi-libusb.so ] || ldconfig -p 2>/dev/null | grep -q hidapi-libusb; then
      SYS+=(-lhidapi-libusb)
    fi
    GROUP_BEGIN=(-Wl,--start-group); GROUP_END=(-Wl,--end-group)
    ;;
esac

# Keep only the archives that actually exist.
#
# The set is CONFIGURATION-dependent, not just platform-dependent, so a hardcoded list is wrong.
# Two real examples, both found on the first macOS CI runs:
#   * wallet-crypto is an ALIAS for cncrypto when the crypto autodetect falls back to the
#     internal "cn" backend (src/crypto/wallet/CMakeLists.txt), so no libwallet-crypto.a is
#     produced at all — while a machine whose autodetect picks another backend does have one;
#   * liblmdb_lib.a only exists if a target that needs it was built, and CI builds just
#     daemon/simplewallet/wallet_rpc_server.
# Dropping a genuinely required archive still fails, just as "undefined symbol" instead of
# "no such file" — and the note below names what was skipped, so the cause is visible.
existing() {
  local out=() f
  for f in "$@"; do
    if [ -f "$f" ]; then out+=("$f"); else echo "note: skipping $(basename "$f") (not built in this configuration)" >&2; fi
  done
  printf '%s\n' ${out[@]+"${out[@]}"}
}

link() { # $1 = "crypto" | "wallet"
  local libs=()
  if [ "$1" = "wallet" ]; then
    libs=("${WALLET_LIBS[@]}")
    case " ${BOOST[*]-} " in *" -lboost_locale "*) :;; *) BOOST+=(-lboost_locale);; esac
    SYS+=(-lzmq -lprotobuf -lusb-1.0 -lreadline)
  else
    libs=("${CRYPTO_LIBS[@]}")
  fi
  local present=()
  while IFS= read -r f; do [ -n "$f" ] && present+=("$f"); done < <(existing ${libs[@]+"${libs[@]}"})
  libs=(${present[@]+"${present[@]}"})
  # ${arr[@]+"${arr[@]}"} rather than "${arr[@]}": macOS ships bash 3.2, where expanding an
  # EMPTY array under `set -u` is an "unbound variable" error. bash >= 4.4 allows it, which is
  # why this only showed up on the first real macOS CI run and never on Linux.
  g++ -std=c++17 -O1 -o "$OUT_DIR/$TEST" "$SRC" \
    ${INCLUDES[@]+"${INCLUDES[@]}"} \
    ${GROUP_BEGIN[@]+"${GROUP_BEGIN[@]}"} "${libs[@]}" ${GROUP_END[@]+"${GROUP_END[@]}"} \
    ${EXTRA_LDFLAGS[@]+"${EXTRA_LDFLAGS[@]}"} ${BOOST[@]+"${BOOST[@]}"} ${SYS[@]+"${SYS[@]}"}
}

case " $WALLET_LINKED " in
  *" $TEST "*) SET=wallet ;;
  *)           SET=crypto ;;
esac

echo "building $TEST  [${SET} link set, build dir $B, $(uname -s)]"
if ! link "$SET"; then
  if [ "$SET" = "crypto" ]; then
    echo "crypto-only link failed; retrying with the full wallet set" >&2
    link wallet
  else
    exit 1
  fi
fi
echo "built: $OUT_DIR/$TEST"
