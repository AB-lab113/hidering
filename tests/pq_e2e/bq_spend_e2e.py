#!/usr/bin/env python3
"""HIDERING Phase 5 — regtest e2e of a transparent BQ spend. Launched by run.sh, never directly.

Cases, each run once towards a classic B... destination and once towards a BQ... destination,
each from its OWN throwaway BQ wallet holding exactly one output (so a case cannot disturb the
next one):

  a  non-zero change   transfer 5 HRG; the tx must be accepted and mined, the change must land
                       on a FRESH BQ subaddress (minor != 0) and that subaddress, like the spent
                       primary, must be refused by `address bq <i>` (R-a). The change is then
                       spent again: that second tx must carry a `key_pq` input, which proves on
                       chain that the change was a BQ output (R-b normal path).
  b1 zero change       transfer (balance - fee) to a single destination.
  b2 zero change       sweep_all to a single destination.

Balances of sender and destination are checked before and after every case.

No seed or secret key is ever printed: the wallet-creation output is parsed for the two public
addresses and then discarded; later transcripts only ever show command output.
"""
import json, os, pty, re, select, sys, time, urllib.request

WORK = os.environ["WORK"]
WALLET_BIN = os.environ["WALLET_BIN"]
RPC = "http://127.0.0.1:%s" % os.environ["RPC_PORT"]
XFAIL = os.environ.get("ZERO_CHANGE_XFAIL", "0") == "1"
# change subaddress indices skipped (4, then 8) while every fee-loop attempt allocated one
INDEX_XFAIL = os.environ.get("CHANGE_INDEX_XFAIL", "0") == "1"
ATOMIC = 10 ** 12

PROMPT_RE = re.compile(r"\[wallet [^\]\n]*\]:\s*$")
PASSWORD_RE = re.compile(r"(password|Password)[^\n]*:\s*$")
YESNO_RE = re.compile(r"(Is this okay\?|Rescan anyway ?\?|\(Y/Yes/N/No\)|Spend them now anyway\?)[^\n]*$")
BGMINE_RE = re.compile(r"Do you want to do it now\?")
ANSI_RE = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]|\r")


def log(*a):
    print(*a, flush=True)


def strip(s):
    return ANSI_RE.sub("", s)


def money(s):
    whole, _, frac = s.partition(".")
    return int(whole) * ATOMIC + int((frac + "0" * 12)[:12])


def fmt(v):
    return "%d.%012d" % (v // ATOMIC, v % ATOMIC)


def rpc(method, params=None):
    body = json.dumps({"jsonrpc": "2.0", "id": 0, "method": method, "params": params or {}}).encode()
    req = urllib.request.Request(RPC + "/json_rpc", body, {"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=600) as r:
        res = json.load(r)
    if "error" in res:
        raise RuntimeError("%s: %s" % (method, res["error"]))
    return res["result"]


def other_rpc(path, params):
    req = urllib.request.Request(RPC + path, json.dumps(params).encode(), {"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=120) as r:
        return json.load(r)


def height():
    return rpc("get_info")["height"]


class Wallet:
    """hidering-wallet-cli in a pty. Commands end when the REPL prompt comes back; interactive
    questions are answered on the way (password: empty, background mining: no, confirmations:
    per call)."""

    def __init__(self, name, bq):
        self.name = name
        self.path = "%s/wallets/%s" % (WORK, name)
        self.logfile = "%s/logs/%s.log" % (WORK, name)
        argv = [WALLET_BIN, "--generate-new-wallet", self.path, "--password", "",
                "--mnemonic-language", "English",
                "--daemon-address", RPC[len("http://"):], "--trusted-daemon",
                "--allow-mismatched-daemon-version", "--log-file", self.logfile,
                "--log-level", os.environ.get("E2E_WALLET_LOGLEVEL", "0,serialization:ERROR")]
        if bq:
            argv.append("--bq-wallet")
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            os.execv(argv[0], argv)
        created = self._converse(None, confirm="n", timeout=300)
        # creation output carries the seeds and the view key: keep the public addresses, drop the rest
        m = re.search(r"Generated new wallet: (B[1-9A-HJ-NP-Za-km-z]{90,110})", created)
        if not m:
            raise RuntimeError("%s: wallet creation failed (output withheld: it may contain a seed)" % name)
        self.addr = m.group(1)
        mq = re.search(r"Post-quantum BQ\.\.\. address: (BQ[1-9A-HJ-NP-Za-km-z]{1500,2200})", created)
        self.bq_addr = mq.group(1) if mq else None
        if bq and not self.bq_addr:
            raise RuntimeError("%s: --bq-wallet did not produce a BQ... address" % name)
        del created
        for c in ("set inactivity-lock-timeout 0", "set ask-password 0", "set auto-refresh 0", "set setup-background-mining 0",
                  "set refresh-from-block-height 1"):
            self.cmd(c)
        self.cmd("rescan_bc hard", confirm="y", timeout=600, until=r"Balance: |Error")

    def _read(self, timeout):
        r, _, _ = select.select([self.fd], [], [], timeout)
        if not r:
            return ""
        try:
            return os.read(self.fd, 65536).decode("utf-8", "replace")
        except OSError:
            return ""

    def _converse(self, line, confirm="n", timeout=600, until=None):
        if line is not None:
            os.write(self.fd, (line + "\n").encode())
        out, start, prompt_at, scan = "", time.time(), None, 0
        while time.time() - start < timeout:
            chunk = self._read(0.5)
            if not chunk:
                # the REPL prompt was seen and nothing followed it for 2 s: the command is done
                if prompt_at is not None and time.time() - prompt_at > 2.0 \
                        and (until is None or re.search(until, out)):
                    return out
                continue
            out += strip(chunk)
            # only look at what arrived since the last answer, so a question is answered once
            tail = out[max(scan, len(out) - 600):]
            if PROMPT_RE.search(tail):
                prompt_at = time.time()
                continue
            prompt_at = None
            ans = None
            if BGMINE_RE.search(tail):
                ans = "n"
            elif YESNO_RE.search(tail):
                ans = confirm(out) if callable(confirm) else confirm
            elif PASSWORD_RE.search(tail):
                ans = ""
            if ans is not None:
                os.write(self.fd, (ans + "\n").encode())
                scan = len(out)
        raise RuntimeError("%s: timeout on %r; tail:\n%s" % (self.name, line, out[-1500:]))

    def cmd(self, line, confirm="n", timeout=600, until=None):
        return self._converse(line, confirm=confirm, timeout=timeout, until=until)

    def refresh(self):
        return self.cmd("refresh", timeout=600, until=r"Balance: |Error")

    def balance(self):
        out = self.cmd("balance")
        m = re.findall(r"Balance: ([0-9.]+), unlocked balance: ([0-9.]+)", out)
        if not m:
            raise RuntimeError("%s: cannot parse balance:\n%s" % (self.name, out[-800:]))
        return money(m[-1][0]), money(m[-1][1])

    def unspent(self):
        """[(amount, minor)] of unspent outputs."""
        out = self.cmd("incoming_transfers available")
        rows = re.findall(r"^\s*([0-9]+\.[0-9]{12})\s+F\s+\S+\s+\S+\s+\d+\s+<?[0-9a-f]{64}>?\s+(\d+)", out, re.M)
        return [(money(a), int(i)) for a, i in rows]

    def num_subaddresses(self):
        """Number of subaddresses of account 0 (highest listed index + 1)."""
        idx = [int(i) for i in re.findall(r"(?:^|\s)(\d+)\s+B[1-9A-HJ-NP-Za-km-z]{90,110}\b", self.cmd("address all"), re.M)]
        return max(idx) + 1 if idx else 0

    def log_lines(self, needle_re):
        try:
            with open(self.logfile, errors="replace") as f:
                return [l.rstrip() for l in f if re.search(needle_re, l)]
        except OSError:
            return []

    def close(self):
        try:
            os.write(self.fd, b"exit\n")
        except OSError:
            pass
        deadline = time.time() + 20
        while time.time() < deadline:
            self._read(0.5)  # drain, so the child is never blocked writing to the pty
            try:
                if os.waitpid(self.pid, os.WNOHANG)[0] == self.pid:
                    break
            except ChildProcessError:
                break
        else:
            for sig in (15, 9):
                try:
                    os.kill(self.pid, sig); time.sleep(1)
                    os.waitpid(self.pid, os.WNOHANG)
                except (ProcessLookupError, ChildProcessError):
                    break
        try:
            os.close(self.fd)
        except OSError:
            pass


def mine(n, addr):
    rpc("generateblocks", {"amount_of_blocks": n, "wallet_address": addr, "starting_nonce": 0})


def txid_of(out):
    m = re.search(r"Transaction successfully submitted, transaction <?([0-9a-f]{64})>?", out)
    return m.group(1) if m else None


def tx_json(txid):
    res = other_rpc("/get_transactions", {"txs_hashes": [txid], "decode_as_json": True})
    txs = res.get("txs") or []
    if not txs:
        return None
    return json.loads(txs[0]["as_json"])


def input_kinds(txid):
    j = tx_json(txid)
    return None if j is None else [list(v.keys())[0] for v in j["vin"]]


FAILS, XFAILS, XPASSES = [], [], []


def rct_invariant(when):
    """Daemon invariant: the RingCT output distribution handed to wallets (decoy selection and the
    wallet2 get_outs sanity check) must count every output of amount bucket 0. Transparent BQ
    outputs live in bucket 0 since the CRIT-1 fix, so they must be counted too."""
    dist = rpc("get_output_distribution", {"amounts": [0], "cumulative": True, "from_height": 0, "binary": False, "compress": False})
    total_dist = dist["distributions"][0]["distribution"][-1]
    hist = rpc("get_output_histogram", {"amounts": [0]})
    total_bucket0 = hist["histogram"][0]["total_instances"]
    check(total_dist == total_bucket0, "daemon, %s: rct distribution total %d == bucket-0 outputs %d (gap %d)"
          % (when, total_dist, total_bucket0, total_bucket0 - total_dist))


def short(out):
    """Command output on one line, BQ addresses abbreviated (they are ~1770 characters)."""
    out = re.sub(r"(BQ[1-9A-HJ-NP-Za-km-z]{12})[1-9A-HJ-NP-Za-km-z]{1000,}", r"\1...", out)
    out = PROMPT_RE.sub("", re.sub(r"\[wallet [^\]\n]*\]:\s*", " ", out))
    return " ".join(out.split())[:600]


def check(cond, what):
    log(("  ok    " if cond else "  FAIL  ") + what)
    if not cond:
        FAILS.append(what)
    return cond


def expect(cond, what, xfail):
    """check() for a behaviour with a known defect: failure is an XFAIL while `xfail` is set,
    success while it is set is an XPASS (the flag must then be cleared)."""
    if cond and xfail:
        XPASSES.append(what); log("  XPASS " + what); return
    if not cond and xfail:
        XFAILS.append(what); log("  XFAIL " + what); return
    check(cond, what)


def error_lines(w, out):
    """Wallet-side error text: the CLI message plus the log lines that locate it (file:line)."""
    cli = [l.strip() for l in out.splitlines() if re.search(r"Error:|was not constructed|Failed to|refused|must not", l)]
    loc = w.log_lines(r"\tERROR\t|HIDERING:|not enough money|Exception: ")
    return cli, loc[-12:]


# ---------------------------------------------------------------------------------------------
def case_a(sender, dest, dest_addr, miner, label):
    sender.refresh(); dest.refresh()
    log("\n== case a (%s): non-zero change, R-b normal path + R-a ==" % label)
    s0, d0 = sender.balance(), dest.balance()
    log("  before: sender %s / dest %s" % (fmt(s0[1]), fmt(d0[0])))
    amount = 5 * ATOMIC
    fee = {}

    def conf(out):
        m = re.findall(r"The transaction fee is ([0-9.]+)", out)
        if m:
            fee["v"] = money(m[-1])
        return "y"
    n0 = sender.num_subaddresses()
    out = sender.cmd("transfer %s %s" % (dest_addr, fmt(amount)), confirm=conf)
    txid = txid_of(out)
    n1 = sender.num_subaddresses()
    expect(n1 == n0 + 1, "a/%s: exactly one subaddress allocated by the transfer (%d -> %d)" % (label, n0, n1), INDEX_XFAIL)
    if not check(txid is not None, "a/%s: transfer built and submitted" % label):
        cli, loc = error_lines(sender, out)
        log("  CLI: %s\n  log: %s" % (cli, "\n       ".join(loc)))
        return
    log("  tx %s, fee %s" % (txid, fmt(fee.get("v", 0))))
    if os.environ.get("E2E_DEBUG"):
        # one "constructing tx" per transfer_selected_rct call (needs wallet.wallet2 at DEBUG)
        log("  DEBUG transfer_selected_rct calls for this transfer: %d" % len(sender.log_lines(r"constructing tx")))
    check(input_kinds(txid) == ["key_pq"], "a/%s: spend input is txin_to_key_pq (vin=%s)" % (label, input_kinds(txid)))
    mine(12, miner.addr)
    sender.refresh(); dest.refresh()
    j = other_rpc("/get_transactions", {"txs_hashes": [txid]})
    check(not j["txs"][0].get("in_pool", True), "a/%s: tx mined (block_height=%s)" % (label, j["txs"][0].get("block_height")))
    s1, d1 = sender.balance(), dest.balance()
    log("  after:  sender %s / dest %s" % (fmt(s1[0]), fmt(d1[0])))
    change = s0[1] - amount - fee.get("v", 0)
    check(s1[0] == change, "a/%s: sender balance = before - amount - fee (%s)" % (label, fmt(change)))
    check(d1[0] == d0[0] + amount, "a/%s: destination received exactly %s" % (label, fmt(amount)))
    rows = sender.unspent()
    log("  sender unspent outputs (amount, minor): %s" % [(fmt(a), i) for a, i in rows])
    ch = [r for r in rows if r[0] == change]
    if not check(len(ch) == 1 and ch[0][1] != 0, "a/%s: change on a fresh BQ subaddress, not the primary index" % label):
        return
    k = ch[0][1]
    expect(k == n0, "a/%s: change on the next free subaddress index %d (got %d)" % (label, n0, k), INDEX_XFAIL)
    n_ref = sender.num_subaddresses()
    r_k = sender.cmd("address bq %d" % k)
    if not check("already been used in a spend" in r_k, "a/%s: R-a refuses change subaddress %d (just allocated, marked spent)" % (label, k)):
        log("  `address bq %d` printed: %s" % (k, short(r_k)))
    r_0 = sender.cmd("address bq 0")
    if not check("already been used in a spend" in r_0, "a/%s: R-a refuses the spent primary via `address bq 0`" % label):
        log("  `address bq 0` printed: %s" % short(r_0))
    bare = sender.cmd("address bq")
    check(not re.search(r"BQ[1-9A-HJ-NP-Za-km-z]{1500,}", bare) and "already been used" in bare and "address new" in bare,
          "a/%s: `address bq` refuses the spent primary and names the command for a fresh one" % label)
    plain = sender.cmd("address")
    check(not re.search(r"BQ[1-9A-HJ-NP-Za-km-z]{1500,}", plain), "a/%s: plain `address` no longer prints the spent primary BQ address" % label)
    check(sender.num_subaddresses() == n_ref, "a/%s: the refusals allocate nothing" % label)
    # second hop: spend the change itself; a key_pq input proves the change was a BQ output
    n2 = sender.num_subaddresses()
    out2 = sender.cmd("transfer %s 1" % dest_addr, confirm="y")
    txid2 = txid_of(out2)
    if check(txid2 is not None, "a/%s: the change itself is spendable" % label):
        check(input_kinds(txid2) == ["key_pq"], "a/%s: spend of the change is a BQ spend (vin=%s)" % (label, input_kinds(txid2)))
        mine(12, miner.addr)
        sender.refresh()
        rows2 = sender.unspent()
        k2 = [i for _, i in rows2]
        check(len(k2) == 1 and k2[0] not in (0, k), "a/%s: second change on yet another fresh subaddress (%s, previous %d)" % (label, k2, k))
        if len(k2) == 1:
            expect(k2[0] == n2, "a/%s: second change on the next free index %d (got %d)" % (label, n2, k2[0]), INDEX_XFAIL)
    else:
        cli, loc = error_lines(sender, out2)
        log("  CLI: %s\n  log: %s" % (cli, "\n       ".join(loc)))


def zero_change_result(case, label, sender, dest, s0, d0, out, txid, miner, sent=None):
    """Common tail of b1/b2: expected failure while the defect stands."""
    if txid:
        mine(12, miner.addr)
        sender.refresh(); dest.refresh()
        s1, d1 = sender.balance(), dest.balance()
        log("  after:  sender %s / dest %s" % (fmt(s1[0]), fmt(d1[0])))
        ok = s1[0] == 0 and (sent is None or d1[0] == d0[0] + sent) and d1[0] > d0[0]
        check(ok, "%s/%s: sender emptied, destination credited" % (case, label))
        if XFAIL and ok:
            XPASSES.append("%s/%s" % (case, label))
            log("  XPASS %s/%s: the zero-change BQ spend now works — set ZERO_CHANGE_XFAIL=0" % (case, label))
        return
    cli, loc = error_lines(sender, out)
    log("  construction failed")
    log("  CLI : %s" % " | ".join(cli))
    for l in loc:
        log("  log : %s" % l)
    sender.refresh(); dest.refresh()
    s1, d1 = sender.balance(), dest.balance()
    log("  after:  sender %s (unlocked %s) / dest %s" % (fmt(s1[0]), fmt(s1[1]), fmt(d1[0])))
    check(s1 == s0 and d1[0] == d0[0], "%s/%s: nothing moved, balances unchanged" % (case, label))
    what = "%s/%s: zero-change BQ spend could not be constructed" % (case, label)
    if XFAIL and any("spec 2e §4.3" in l for l in loc):
        XFAILS.append(what)
        log("  XFAIL %s — known defect (dummy classic change refused by construct_tx §4.3)" % what)
    else:
        FAILS.append(what)
        log("  FAIL  %s" % what)


def case_b1(sender, dest, dest_addr, miner, label):
    sender.refresh(); dest.refresh()
    log("\n== case b1 (%s): zero change — transfer (balance - fee) to one destination ==" % label)
    s0, d0 = sender.balance(), dest.balance()
    log("  before: sender %s / dest %s" % (fmt(s0[1]), fmt(d0[0])))
    bal, out, txid, amt = s0[1], "", None, None
    probe = {}

    def learn(o):
        m = re.findall(r"The transaction fee is ([0-9.]+)", o)
        probe["fee"] = money(m[-1]) if m else None
        return "n"
    sender.cmd("transfer %s %s" % (dest_addr, fmt(bal - ATOMIC)), confirm=learn)
    if probe.get("fee") is None:
        FAILS.append("b1/%s: bench could not learn the fee" % label)
        log("  FAIL  b1/%s: probe transfer of balance - 1 HRG did not reach the confirmation" % label)
        return
    fee_guess = probe["fee"]
    log("  probe: fee of a 1-input BQ spend with change = %s" % fmt(fee_guess))
    for attempt in range(6):
        amt = bal - fee_guess
        seen = {}

        def conf(o):
            m = re.findall(r"The transaction fee is ([0-9.]+)", o)
            seen["fee"] = money(m[-1]) if m else None
            # accept only if this really is a zero-change transaction
            return "y" if seen["fee"] is not None and amt + seen["fee"] == bal else "n"
        out = sender.cmd("transfer %s %s" % (dest_addr, fmt(amt)), confirm=conf)
        txid = txid_of(out)
        if txid:
            log("  attempt %d: amount %s + fee %s = balance -> built with ZERO change, submitted %s" % (attempt, fmt(amt), fmt(seen["fee"]), txid))
            break
        if seen.get("fee") is not None:
            log("  attempt %d: amount %s built with fee %s -> change %s != 0, declined, retrying" % (attempt, fmt(amt), fmt(seen["fee"]), fmt(bal - amt - seen["fee"])))
            fee_guess = seen["fee"]
            continue
        if "was not constructed" in out:
            log("  attempt %d: amount %s (fee estimate %s) -> construction refused" % (attempt, fmt(amt), fmt(fee_guess)))
            break
        m = [l for l in sender.log_lines(r"transaction amount [0-9.]+ = [0-9.]+ \+ [0-9.]+ \(fee\)")]
        f = re.findall(r"\+ ([0-9.]+) \(fee\)", m[-1]) if m else []
        if f and money(f[-1]) != fee_guess:
            fee_guess = money(f[-1])
            log("  attempt %d: amount %s too large, wallet fee estimate %s, retrying" % (attempt, fmt(amt), fmt(fee_guess)))
            continue
        break
    zero_change_result("b1", label, sender, dest, s0, d0, out, txid, miner, sent=amt if txid else None)


def case_b2(sender, dest, dest_addr, miner, label):
    sender.refresh(); dest.refresh()
    log("\n== case b2 (%s): zero change — sweep_all to one destination ==" % label)
    s0, d0 = sender.balance(), dest.balance()
    log("  before: sender %s / dest %s" % (fmt(s0[1]), fmt(d0[0])))
    n0 = sender.num_subaddresses()
    out = sender.cmd("sweep_all %s" % dest_addr, confirm="y")
    txid = txid_of(out)
    if txid:
        log("  sweep submitted %s (vin=%s)" % (txid, input_kinds(txid)))
        n1 = sender.num_subaddresses()
        expect(n1 == n0 + 1, "b2/%s: sweep_all allocated exactly one change subaddress (%d -> %d)" % (label, n0, n1), INDEX_XFAIL)
    zero_change_result("b2", label, sender, dest, s0, d0, out, txid, miner)


def sweep(sender, dest_addr):
    """sweep_all; returns (txid, fee) — fee parsed from the confirmation prompt."""
    fee = {}

    def conf(o):
        m = re.findall(r"for a total fee of ([0-9.]+)", o)
        if m:
            fee["v"] = money(m[-1])
        return "y"
    out = sender.cmd("sweep_all %s" % dest_addr, confirm=conf)
    return out, txid_of(out), fee.get("v")


def case_c(sender, recv, dest, miner):
    log("\n== case c: migration — classic wallet sweep_all to a fresh BQ address, then the BQ wallet spends ==")
    for w in (sender, recv, dest):
        w.refresh()
    s0, r0 = sender.balance(), recv.balance()
    log("  before: classic sender %s / BQ receiver %s" % (fmt(s0[1]), fmt(r0[0])))
    out, txid, fee = sweep(sender, recv.bq_addr)
    if not check(txid is not None and fee is not None, "c: classic -> BQ sweep_all built and submitted"):
        cli, loc = error_lines(sender, out)
        log("  CLI: %s\n  log: %s" % (cli, "\n       ".join(loc)))
        return
    check(input_kinds(txid) == ["key"], "c: the migration tx spends a classic ring input (vin=%s)" % input_kinds(txid))
    mine(12, miner.addr)
    sender.refresh(); recv.refresh()
    s1, r1 = sender.balance(), recv.balance()
    log("  after:  classic sender %s / BQ receiver %s (fee %s)" % (fmt(s1[0]), fmt(r1[0]), fmt(fee)))
    check(s1[0] == 0, "c: classic sender emptied")
    check(r1[0] == r0[0] + s0[1] - fee, "c: BQ receiver credited with balance - fee")
    d0 = dest.balance()
    fee2 = {}

    def conf(o):
        m = re.findall(r"The transaction fee is ([0-9.]+)", o)
        if m:
            fee2["v"] = money(m[-1])
        return "y"
    out2 = recv.cmd("transfer %s 5" % dest.addr, confirm=conf)
    txid2 = txid_of(out2)
    if not check(txid2 is not None, "c: the BQ receiver re-spends the migrated funds"):
        cli, loc = error_lines(recv, out2)
        log("  CLI: %s\n  log: %s" % (cli, "\n       ".join(loc)))
        return
    check(input_kinds(txid2) == ["key_pq"], "c: the re-spend is a BQ spend (vin=%s)" % input_kinds(txid2))
    mine(12, miner.addr)
    recv.refresh(); dest.refresh()
    r2, d1 = recv.balance(), dest.balance()
    check(r2[0] == r1[0] - 5 * ATOMIC - fee2.get("v", 0), "c: BQ receiver balance = before - 5 - fee (%s)" % fmt(r2[0]))
    check(d1[0] == d0[0] + 5 * ATOMIC, "c: destination received exactly 5")


def case_d(s_sweep, s_xfer, dest, miner):
    log("\n== case d: classic control — sweep_all, then a transfer with change, to a classic address ==")
    for w in (s_sweep, s_xfer, dest):
        w.refresh()
    s0, d0 = s_sweep.balance(), dest.balance()
    out, txid, fee = sweep(s_sweep, dest.addr)
    if check(txid is not None and fee is not None, "d: classic sweep_all built and submitted"):
        check(input_kinds(txid) == ["key"], "d: ring input (vin=%s)" % input_kinds(txid))
        mine(12, miner.addr)
        s_sweep.refresh(); dest.refresh()
        s1, d1 = s_sweep.balance(), dest.balance()
        log("  sweep: sender %s -> %s, dest %s -> %s (fee %s)" % (fmt(s0[1]), fmt(s1[0]), fmt(d0[0]), fmt(d1[0]), fmt(fee)))
        check(s1[0] == 0 and d1[0] == d0[0] + s0[1] - fee, "d: sweep balances (sender 0, dest + balance - fee)")
    else:
        cli, loc = error_lines(s_sweep, out)
        log("  CLI: %s\n  log: %s" % (cli, "\n       ".join(loc)))
    # the sweep above mined 12 blocks: bring the second wallet to the tip first, or simplewallet
    # rightly refuses a decoy from a block it has not seen (simplewallet.cpp, process_ring_members)
    s_xfer.refresh()
    x0, d0 = s_xfer.balance(), dest.balance()
    fee2 = {}

    def conf(o):
        m = re.findall(r"The transaction fee is ([0-9.]+)", o)
        if m:
            fee2["v"] = money(m[-1])
        return "y"
    out2 = s_xfer.cmd("transfer %s 5" % dest.addr, confirm=conf)
    txid2 = txid_of(out2)
    if not check(txid2 is not None, "d: classic transfer with change built and submitted"):
        cli, loc = error_lines(s_xfer, out2)
        log("  CLI: %s\n  log: %s" % (cli, "\n       ".join(loc)))
        return
    mine(12, miner.addr)
    s_xfer.refresh(); dest.refresh()
    x1, d1 = s_xfer.balance(), dest.balance()
    check(x1[0] == x0[1] - 5 * ATOMIC - fee2.get("v", 0), "d: sender balance = before - 5 - fee (%s)" % fmt(x1[0]))
    check(d1[0] == d0[0] + 5 * ATOMIC, "d: destination received exactly 5")
    rows = s_xfer.unspent()
    check([i for _, i in rows] == [0], "d: classic change back on the primary index (%s)" % rows)


def daemon_log_lines(needle_re):
    try:
        with open(WORK + "/daemon.log", errors="replace") as f:
            return [l.rstrip() for l in f if re.search(needle_re, l)]
    except OSError:
        return []


def case_e(sender, recv, dest, miner):
    """HYBRID: a BQ wallet holding one classic output (on its B... address) and one BQ output
    pays an amount only both together cover -> one tx with a ring input AND a txin_to_key_pq
    input, i.e. a real RingCT tx. Then the RECIPIENT spends the output it received from that
    hybrid tx (a ring spend whose real ring member is that output). Question under test: is the
    commitment the daemon stored for a hybrid output the real outPk mask (the recipient's spend
    verifies) or zeroCommit(0) (it cannot)? The sender's change (a BQ output created by the hybrid)
    is re-spent too, as a second probe of the same stored commitment (check b2)."""
    log("\n== case e: hybrid tx (ring + BQ inputs), then the recipient spends what it received ==")
    for w in (sender, recv, dest):
        w.refresh()
    s0, r0 = sender.balance(), recv.balance()
    log("  before: hybrid sender %s (outputs %s) / recipient %s" % (fmt(s0[1]), [(fmt(a), i) for a, i in sender.unspent()], fmt(r0[0])))
    amount = 30 * ATOMIC
    fee = {}

    def conf(o):
        m = re.findall(r"The transaction fee is ([0-9.]+)", o)
        if m:
            fee["v"] = money(m[-1])
        return "y"
    out = sender.cmd("transfer %s %s" % (recv.addr, fmt(amount)), confirm=conf)
    txid = txid_of(out)
    if not check(txid is not None, "e: hybrid transfer of 30 HRG (needs both outputs) built and submitted"):
        cli, loc = error_lines(sender, out)
        log("  CLI: %s\n  log: %s" % (cli, "\n       ".join(loc)))
        log("  daemon: %s" % "\n          ".join(daemon_log_lines(r"ERROR|mismatched|Sum check|PQ input")[-8:]))
        return
    kinds = input_kinds(txid)
    j = tx_json(txid)
    rct_type = j["rct_signatures"]["type"] if j else None
    log("  tx %s, vin=%s, rct type %s, fee %s" % (txid, kinds, rct_type, fmt(fee.get("v", 0))))
    if not check(kinds is not None and "key" in kinds and "key_pq" in kinds and rct_type not in (None, 0),
                 "e: the tx is HYBRID (ring + key_pq inputs, rct type != Null)"):
        return
    mine(12, miner.addr)
    sender.refresh(); recv.refresh()
    tj = other_rpc("/get_transactions", {"txs_hashes": [txid]})
    check(not tj["txs"][0].get("in_pool", True), "e: hybrid tx mined (block_height=%s)" % tj["txs"][0].get("block_height"))
    s1, r1 = sender.balance(), recv.balance()
    log("  after:  sender %s / recipient %s" % (fmt(s1[0]), fmt(r1[0])))
    check(r1[0] == r0[0] + amount, "e: recipient credited with exactly 30")
    rct_invariant("after the hybrid tx")

    # the daemon's view of the recipient's output: the commitment it will put in any ring
    outs_json = j["rct_signatures"]["outPk"] if j else []
    log("  on-wire outPk of the hybrid tx: %s" % outs_json)
    oi = other_rpc("/get_transactions", {"txs_hashes": [txid]})["txs"][0].get("output_indices", [])
    if oi:
        got = other_rpc("/get_outs", {"outputs": [{"amount": 0, "index": i} for i in oi], "get_txid": False})
        stored = [o["mask"] for o in got.get("outs", [])]
        log("  daemon get_outs commitments for those outputs: %s" % stored)
        G = "5866666666666666666666666666666666666666666666666666666666666666"
        check(stored == outs_json, "e: daemon stores each hybrid output with its outPk mask (stored %s)"
              % ["G=zeroCommit(0)" if m == G else m[:16] for m in stored])

    # second hop: the recipient spends the output it received from the hybrid tx
    d0 = dest.balance()
    out2 = recv.cmd("transfer %s 5" % dest.addr, confirm="y")
    txid2 = txid_of(out2)
    if check(txid2 is not None, "e: recipient's spend of the hybrid output built and accepted by the daemon"):
        mine(12, miner.addr)
        recv.refresh(); dest.refresh()
        check(dest.balance()[0] == d0[0] + 5 * ATOMIC, "e: destination received exactly 5 from the recipient")
    else:
        cli, loc = error_lines(recv, out2)
        log("  CLI: %s\n  log: %s" % (cli, "\n       ".join(loc)))
        log("  daemon: %s" % "\n          ".join(daemon_log_lines(r"ERROR|mismatched|Sum check|PQ input|Failed to check")[-8:]))

    # third probe: the sender's change from the hybrid (a BQ output) spent through check b2
    rows = sender.unspent()
    log("  sender unspent after the hybrid (amount, minor): %s" % [(fmt(a), i) for a, i in rows])
    if rows:
        out3 = sender.cmd("transfer %s 1" % dest.addr, confirm="y")
        txid3 = txid_of(out3)
        if check(txid3 is not None, "e: sender's change from the hybrid tx is spendable"):
            log("  change spend %s vin=%s" % (txid3, input_kinds(txid3)))
        else:
            cli, loc = error_lines(sender, out3)
            log("  CLI: %s\n  log: %s" % (cli, "\n       ".join(loc)))
            log("  daemon: %s" % "\n          ".join(daemon_log_lines(r"ERROR|mismatched|Sum check|PQ input|Failed to check")[-8:]))


# ---------------------------------------------------------------------------------------------
def main():
    os.makedirs(WORK + "/wallets"); os.makedirs(WORK + "/logs")
    log("regtest height %d" % height())
    wallets = []
    try:
        log("creating throwaway wallets (creation output discarded)")
        miner = Wallet("miner", bq=False); wallets.append(miner)
        dst_c = Wallet("dest_classic", bq=False); wallets.append(dst_c)
        dst_q = Wallet("dest_bq", bq=True); wallets.append(dst_q)
        cases = os.environ.get("CASES", "a b1 b2 c d e").split()
        senders, funded = {}, []   # funded: (wallet, address to fund)
        for case in [c for c in cases if c in ("a", "b1", "b2")]:
            for label in ("classic", "bq"):
                w = Wallet("s_%s_%s" % (case, label), bq=True); wallets.append(w)
                senders[(case, label)] = w; funded.append((w, w.bq_addr))
        if "c" in cases:
            w = Wallet("s_c_classic", bq=False); wallets.append(w)
            senders[("c", "sender")] = w; funded.append((w, w.addr))
            r = Wallet("r_c_bq", bq=True); wallets.append(r)
            senders[("c", "recv")] = r
        if "d" in cases:
            for label in ("sweep", "xfer"):
                w = Wallet("s_d_%s" % label, bq=False); wallets.append(w)
                senders[("d", label)] = w; funded.append((w, w.addr))
        if "e" in cases:
            # funded separately below: one 20 HRG output on its B... AND one on its BQ... address
            senders[("e", "sender")] = Wallet("s_e_hybrid", bq=True); wallets.append(senders[("e", "sender")])
            senders[("e", "recv")] = Wallet("r_e_classic", bq=False); wallets.append(senders[("e", "recv")])
        log("mining 150 blocks to the miner (coinbase unlock 60, ring 32)")
        mine(150, miner.addr)
        r = miner.refresh()
        if os.environ.get("E2E_DEBUG"):
            log("DEBUG height %d, refresh output:\n%s" % (height(), r[-1500:]))
        log("miner balance %s" % fmt(miner.balance()[1]))
        log("funding each sender with ONE 20 HRG output on its primary (BQ or classic) address")
        for w, a in funded:
            out = miner.cmd("transfer %s 20" % a, confirm="y")
            if not txid_of(out):
                log("BENCH: funding %s failed:\n%s" % (w.name, out[-800:]))
                return 2
            mine(1, miner.addr); miner.refresh()
        mine(12, miner.addr)
        for w in list(senders.values()) + [dst_c, dst_q]:
            w.refresh()
        for w, _ in funded:
            b = w.balance()
            if b != (20 * ATOMIC, 20 * ATOMIC) or [i for _, i in w.unspent()] != [0]:
                log("BENCH: %s not funded as expected: balance %s" % (w.name, (fmt(b[0]), fmt(b[1]))))
                return 2
        log("all %d senders hold one unlocked 20 HRG output at index 0 (height %d)" % (len(funded), height()))
        if ("e", "sender") in senders:
            w = senders[("e", "sender")]
            for a in (w.addr, w.bq_addr):
                out = miner.cmd("transfer %s 20" % a, confirm="y")
                if not txid_of(out):
                    log("BENCH: funding %s failed:\n%s" % (w.name, out[-800:]))
                    return 2
                mine(1, miner.addr); miner.refresh()
            mine(12, miner.addr)
            w.refresh()
            if w.balance() != (40 * ATOMIC, 40 * ATOMIC) or len(w.unspent()) != 2:
                log("BENCH: %s not funded as expected: %s" % (w.name, w.unspent()))
                return 2
            log("hybrid sender holds 20 HRG classic (B...) + 20 HRG BQ, both unlocked")

        rct_invariant("after funding")
        dests = {"classic": (dst_c, dst_c.addr), "bq": (dst_q, dst_q.bq_addr)}
        for label in ("classic", "bq"):
            if ("a", label) in senders: case_a(senders[("a", label)], dests[label][0], dests[label][1], miner, label)
        for label in ("classic", "bq"):
            if ("b1", label) in senders: case_b1(senders[("b1", label)], dests[label][0], dests[label][1], miner, label)
        for label in ("classic", "bq"):
            if ("b2", label) in senders: case_b2(senders[("b2", label)], dests[label][0], dests[label][1], miner, label)
        rct_invariant("after cases a/b")
        if ("c", "sender") in senders:
            case_c(senders[("c", "sender")], senders[("c", "recv")], dst_c, miner)
        rct_invariant("after case c")
        if ("d", "sweep") in senders:
            case_d(senders[("d", "sweep")], senders[("d", "xfer")], dst_c, miner)
        if ("e", "sender") in senders:
            case_e(senders[("e", "sender")], senders[("e", "recv")], dst_c, miner)
    finally:
        for w in wallets:
            w.close()

    log("\n== summary ==")
    for x in XFAILS:
        log("  XFAIL  " + x)
    for x in XPASSES:
        log("  XPASS  " + x)
    for x in FAILS:
        log("  FAIL   " + x)
    if FAILS:
        return 1
    if XPASSES:
        return 3
    log("  all cases as expected%s" % (" (with expected failures of known defects)" if XFAILS else ""))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as e:  # bench problem, not a verdict
        log("BENCH ERROR: %s" % e)
        sys.exit(2)
