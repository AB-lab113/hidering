#!/usr/bin/env bash
# HIDERING Phase 5 — end-to-end regtest test of a transparent BQ spend (spec 2e §4.2 R-a / R-b)
# and of the zero-change BQ spend (single destination, whole balance; sweep_all).
#
# Method reused from the A4 / CRIT-1 e2e runs (commit 03e34a2ad message; session scratchpad
# e2e/walletdrv.py of 7 Sep 2026): ONE regtest daemon session (regtest activates HFv16 at h=1 and
# its LMDB does not survive a restart), hidering-wallet-cli driven through a pty,
# --allow-mismatched-daemon-version (regtest v16@h1 vs mainnet table v16@2M),
# refresh-from-block-height 1 + rescan_bc hard on each fresh wallet, blocks mined through the
# daemon's regtest-only `generateblocks` RPC.
#
# Everything lives under /tmp/pq-e2e-<timestamp>; the EXIT trap stops the daemon and every wallet
# process started from that directory, then deletes it — on success and on failure alike.
# Throwaway wallets only, empty passwords, no seed is ever printed or kept.
#
# Usage:  tests/pq_e2e/run.sh            (binaries default to build/release/bin)
#         BIN_DIR=... tests/pq_e2e/run.sh
# Exit:   0 = every case as expected (known defects may be EXPECTED FAILURES, see below)
#         1 = a case failed unexpectedly     2 = test bench problem
#         3 = a flagged known defect now PASSES: clear its flag
#
# Known-defect flags (both fixed, default 0): ZERO_CHANGE_XFAIL (zero-change BQ spend refused), CHANGE_INDEX_XFAIL
# (change subaddress indices skipped). Set to 1 to turn those failures into "expected failures" when
# testing an unfixed build. Optional: CASES="a b1 b2 c d" (subset), E2E_WALLET_LOGLEVEL (wallet log level).
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BIN_DIR="${BIN_DIR:-$REPO/build/release/bin}"
DAEMON="$BIN_DIR/hideringd"
WALLET="$BIN_DIR/hidering-wallet-cli"
P2P_PORT="${P2P_PORT:-47740}"
RPC_PORT="${RPC_PORT:-47741}"

for b in "$DAEMON" "$WALLET"; do
  [ -x "$b" ] || { echo "BENCH: missing binary $b" >&2; exit 2; }
done
if ss -tlnH 2>/dev/null | awk '{print $4}' | grep -Eq ":(${P2P_PORT}|${RPC_PORT})\$"; then
  echo "BENCH: port ${P2P_PORT} or ${RPC_PORT} already in use" >&2; exit 2
fi

WORK="/tmp/pq-e2e-$(date +%Y%m%d-%H%M%S)-$$"
mkdir -m 700 "$WORK"
DAEMON_PID=""

cleanup() {
  local rc=$?
  set +e
  # every wallet-cli we started carries $WORK in its argv
  pkill -TERM -f -- "$WORK/" 2>/dev/null
  if [ -n "$DAEMON_PID" ] && kill -0 "$DAEMON_PID" 2>/dev/null; then
    kill -TERM "$DAEMON_PID"
    for _ in $(seq 1 30); do kill -0 "$DAEMON_PID" 2>/dev/null || break; sleep 1; done
    kill -KILL "$DAEMON_PID" 2>/dev/null
  fi
  pkill -KILL -f -- "$WORK/" 2>/dev/null
  rm -rf -- "$WORK"
  echo "cleanup: daemon stopped, $WORK removed (exit $rc)"
  exit $rc
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

echo "bench: $WORK  (daemon $DAEMON, rpc 127.0.0.1:$RPC_PORT, offline, no peers)"
"$DAEMON" --regtest --offline --fixed-difficulty 1 --non-interactive \
  --data-dir "$WORK/data" --log-file "$WORK/daemon.log" --log-level 0 \
  --p2p-bind-ip 127.0.0.1 --p2p-bind-port "$P2P_PORT" \
  --rpc-bind-ip 127.0.0.1 --rpc-bind-port "$RPC_PORT" \
  --no-zmq --no-igd --out-peers 0 --in-peers 0 --disable-dns-checkpoints \
  >"$WORK/daemon.stdout" 2>&1 &
DAEMON_PID=$!

for _ in $(seq 1 60); do
  if curl -fsS "http://127.0.0.1:$RPC_PORT/get_info" >/dev/null 2>&1; then break; fi
  kill -0 "$DAEMON_PID" 2>/dev/null || { echo "BENCH: daemon died"; tail -20 "$WORK/daemon.stdout"; exit 2; }
  sleep 1
done
curl -fsS "http://127.0.0.1:$RPC_PORT/get_info" >/dev/null || { echo "BENCH: daemon RPC never came up"; exit 2; }

set +e
WORK="$WORK" WALLET_BIN="$WALLET" RPC_PORT="$RPC_PORT" ZERO_CHANGE_XFAIL="${ZERO_CHANGE_XFAIL:-0}" \
  CHANGE_INDEX_XFAIL="${CHANGE_INDEX_XFAIL:-0}" \
  python3 -u "$REPO/tests/pq_e2e/bq_spend_e2e.py"
rc=$?
set -e
exit $rc
