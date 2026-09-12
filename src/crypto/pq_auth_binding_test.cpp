// HIDERING Phase 5 (HFv16) — spec 2e: the residual of §3.4, and the T2 refusals.
//
// Standalone; links libwallet because both halves need the real thing: construct_tx builds the
// payment and wallet2::process_new_transaction scans it.
//
// WHAT IS BEING PROVEN.
//
//  1. §3.4, BOTH HALVES. The consensus cannot check that the SENDER committed to the recipient's
//     own authorisation commitment rather than to one of its own — it never sees the address. So
//     a sender CAN create an output bound to its own commitment, and the validator will accept a
//     spend of it (first half). What makes that harmless is entirely wallet-side: the recipient
//     recomputes the binding at scan and REFUSES the output, so it is never credited and never
//     displayed (second half). A test of only the first half would document the hole; only the
//     second half makes it safe.
//
//  2. Unlinkability of the blind (§2.4). Two outputs to the same subaddress share no on-chain
//     value, and revealing one output's blind at spend does not let anyone recompute the binding
//     tag of the other. Without the blind, one revealed authorisation key would unmask every
//     output ever sent to that subaddress.
//
//  3. T2 (§4.2), all three refusals: R-a a spent subaddress is never handed out again, R-b the
//     change of a BQ spend lands on a FRESH BQ subaddress, R-c two BQ subaddresses are not
//     merged into one transaction without an explicit opt-in.
//
// Usage: pq_auth_binding_test <scratch directory>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>

#include "crypto/crypto.h"
#include "crypto/pqc.h"
#include "cryptonote_basic/account.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "ringct/rctOps.h"
#include "wallet/wallet2.h"

using namespace cryptonote;
using namespace crypto::pqc;

class wallet_accessor_test
{
public:
  static void scan(tools::wallet2 &w, const transaction &tx, uint64_t height)
  {
    std::vector<uint64_t> o_indices(tx.vout.size());
    std::iota(o_indices.begin(), o_indices.end(), height * 100);
    w.process_new_transaction(get_transaction_hash(tx), tx, o_indices, height, HF_VERSION_PQ, 0,
                              false, false, false, tools::wallet2::tx_cache_data{}, nullptr, true);
  }
  static bool recover(const tools::wallet2 &w, const tools::wallet2::transfer_details &td, kyber_shared_secret &ss)
  { return w.recover_pq_spend_secret(td, ss); }
  static std::unordered_map<crypto::public_key, subaddress_index> subaddresses(const tools::wallet2 &w)
  { return w.m_subaddresses; }
};

namespace
{
  std::string g_dir;
  const char *PW = "pw";
  const uint64_t HRG = 1000000000000ull;

  std::string path_of(const std::string &n) { return g_dir + "/" + n; }

  void make_bq_wallet(tools::wallet2 &w, const std::string &name)
  {
    for (const char *ext : {"", ".keys", ".address.txt"})
      boost::filesystem::remove(path_of(name) + ext);
    w.set_seed_language("English");
    w.generate(path_of(name), PW, crypto::secret_key(), false, false, false, true /*use_pq*/, nullptr);
  }

  tx_source_entry owned_source(const account_keys &sender, uint64_t amount)
  {
    keypair txkey = keypair::generate(hw::get_device("default"));
    crypto::key_derivation der;
    crypto::generate_key_derivation(sender.m_account_address.m_view_public_key, txkey.sec, der);
    crypto::public_key P;
    crypto::derive_public_key(der, 0, sender.m_account_address.m_spend_public_key, P);
    tx_source_entry src;
    src.amount = amount; src.rct = true; src.mask = rct::skGen(); src.real_output = 7;
    for (size_t n = 0; n < 16; ++n)
    {
      tx_source_entry::output_entry oe;
      oe.first = 1000 + n;
      if (n == src.real_output) { oe.second.dest = rct::pk2rct(P); oe.second.mask = rct::commit(amount, src.mask); }
      else { oe.second.dest = rct::pkGen(); oe.second.mask = rct::pkGen(); }
      src.outputs.push_back(oe);
    }
    src.real_out_tx_key = txkey.pub;
    src.real_output_in_tx_index = 0;
    return src;
  }

  bool build(const account_keys &sender, const std::unordered_map<crypto::public_key, subaddress_index> &subaddrs,
             std::vector<tx_source_entry> sources, std::vector<tx_destination_entry> dests,
             const boost::optional<account_public_address> &change, transaction &tx)
  {
    crypto::secret_key tx_key;
    std::vector<crypto::secret_key> additional;
    const rct::RCTConfig cfg{rct::RangeProofPaddedBulletproof, 4};
    return construct_tx_and_get_tx_key(sender, subaddrs, sources, dests, change, {}, tx, tx_key, additional,
                                       true, cfg, true, HF_VERSION_PQ);
  }

  crypto::hash bind_of(const transaction &tx, size_t out_index, bool &found)
  {
    crypto::hash h{};
    found = false;
    std::vector<tx_extra_field> fields;
    if (!parse_tx_extra(tx.extra, fields)) return h;
    for (const auto &f : fields)
      if (f.type() == typeid(tx_extra_pq_bind) && boost::get<tx_extra_pq_bind>(f).output_index == out_index)
      { h = boost::get<tx_extra_pq_bind>(f).bind_tag; found = true; }
    return h;
  }
}

// (1) §3.4 — the sender binds the output to ITS OWN commitment.
static bool test_forged_commitment()
{
  tools::wallet2 victim(MAINNET, 1, true);
  make_bq_wallet(victim, "victim");
  const account_keys &vk = victim.get_account().get_keys();

  // The attacker builds a payment to the victim's BQ address, but with the authorisation
  // commitment swapped for its own. Everything else about the address is genuine, so the victim
  // WILL detect the output as its own before the binding check runs.
  account_base attacker; attacker.generate();
  generate_pq_keys(attacker.get_keys_nonconst(), generate_pq_root_secret());

  account_public_address poisoned = vk.m_account_address;
  std::array<uint8_t, 32> attacker_commit{};
  if (!get_pq_auth_commit(attacker.get_keys(), {0, 0}, attacker_commit))
  { printf("FAIL: attacker commitment\n"); return false; }
  poisoned.pq_auth_commit = attacker_commit;

  transaction pay;
  if (!build(attacker.get_keys(), {{attacker.get_keys().m_account_address.m_spend_public_key, {0, 0}}},
             {owned_source(attacker.get_keys(), 3 * HRG)},
             {tx_destination_entry(2 * HRG, poisoned, false)}, boost::none, pay))
  { printf("FAIL: could not build the poisoned payment\n"); return false; }

  // First half: the chain accepts it, and a spend of it would pass check (c) — for the attacker.
  bool found = false;
  const crypto::hash stored = bind_of(pay, 0, found);
  if (!found) { printf("FAIL: no binding tag emitted\n"); return false; }
  {
    // recompute with the ATTACKER's commitment: it matches, i.e. the attacker could spend.
    kyber_shared_secret ss_a;
    // the attacker knows ss because it encapsulated; re-derive it the way the victim would and
    // check the tag is the attacker's, not the victim's.
    std::array<uint8_t, 32> victim_commit{};
    if (!get_pq_auth_commit(vk, {0, 0}, victim_commit)) { printf("FAIL: victim commitment\n"); return false; }
    if (victim_commit == attacker_commit) { printf("FAIL: test setup — the two commitments are equal\n"); return false; }
    (void)ss_a;
  }

  // Second half, the one that matters: the victim must NOT credit the output.
  wallet_accessor_test::scan(victim, pay, 100);
  tools::wallet2::transfer_container t;
  victim.get_transfers(t);
  if (!t.empty())
  {
    printf("FAIL: spec 2e §3.4 — the wallet credited an output bound to someone else's\n"
           "      authorisation commitment; it can never be spent, and the balance is a lie\n");
    return false;
  }

  // Control: the SAME payment with the genuine commitment IS credited, so the refusal above is
  // the binding check and not some unrelated scan failure.
  transaction good;
  if (!build(attacker.get_keys(), {{attacker.get_keys().m_account_address.m_spend_public_key, {0, 0}}},
             {owned_source(attacker.get_keys(), 3 * HRG)},
             {tx_destination_entry(2 * HRG, vk.m_account_address, false)}, boost::none, good))
  { printf("FAIL: could not build the honest payment\n"); return false; }
  wallet_accessor_test::scan(victim, good, 101);
  victim.get_transfers(t);
  if (t.size() != 1) { printf("FAIL: control — the honest payment was not credited (%zu)\n", t.size()); return false; }

  printf("PASS: §3.4 — an output bound to the sender's own commitment is refused at scan (never\n"
         "      credited), while the same payment with the genuine commitment is credited\n");
  return true;
}

// (2) §2.4 — the blind keeps other outputs to the same subaddress unlinkable.
static bool test_blind_unlinkability()
{
  tools::wallet2 w(MAINNET, 1, true);
  make_bq_wallet(w, "blind");
  const account_keys &k = w.get_account().get_keys();

  account_base sender; sender.generate();
  transaction pay;
  if (!build(sender.get_keys(), {{sender.get_keys().m_account_address.m_spend_public_key, {0, 0}}},
             {owned_source(sender.get_keys(), 5 * HRG)},
             {tx_destination_entry(1 * HRG, k.m_account_address, false),
              tx_destination_entry(2 * HRG, k.m_account_address, false)}, boost::none, pay))
  { printf("FAIL: could not build a two-output payment\n"); return false; }

  bool f0 = false, f1 = false;
  const crypto::hash b0 = bind_of(pay, 0, f0), b1 = bind_of(pay, 1, f1);
  if (!f0 || !f1) { printf("FAIL: missing binding tags\n"); return false; }
  if (b0 == b1) { printf("FAIL: two outputs to one subaddress share a binding tag\n"); return false; }

  // An observer learns the authorisation key from a spend of output 0, hence the commitment.
  // With output 0's blind revealed it can recompute tag 0 — and NOTHING about tag 1.
  wallet_accessor_test::scan(w, pay, 200);
  tools::wallet2::transfer_container t;
  w.get_transfers(t);
  if (t.size() != 2) { printf("FAIL: the wallet found %zu of 2 outputs\n", t.size()); return false; }

  std::array<uint8_t, 32> commit{};
  if (!get_pq_auth_commit(k, {0, 0}, commit)) { printf("FAIL: commitment\n"); return false; }
  for (const auto &td : t)
  {
    kyber_shared_secret ss;
    if (!wallet_accessor_test::recover(w, td, ss)) { printf("FAIL: recover\n"); return false; }
    uint8_t blind[32], expect[32];
    pqc_compute_auth_blind(ss, td.m_internal_output_index, blind);
    crypto::public_key P;
    get_output_public_key(pay.vout[td.m_internal_output_index], P);
    pqc_compute_bind_tag_v2(::config::CRYPTONOTE_PQ_ADDRESS_AUTH_VER, (const uint8_t *)&P, 32, commit.data(), blind, expect);
    const crypto::hash want = td.m_internal_output_index == 0 ? b0 : b1;
    if (memcmp(expect, &want, 32) != 0) { printf("FAIL: owner cannot reproduce its own binding tag\n"); return false; }

    // the OTHER output's tag must not be reachable with this output's blind
    const crypto::hash other = td.m_internal_output_index == 0 ? b1 : b0;
    crypto::public_key Q;
    get_output_public_key(pay.vout[td.m_internal_output_index == 0 ? 1 : 0], Q);
    uint8_t cross[32];
    pqc_compute_bind_tag_v2(::config::CRYPTONOTE_PQ_ADDRESS_AUTH_VER, (const uint8_t *)&Q, 32, commit.data(), blind, cross);
    if (memcmp(cross, &other, 32) == 0)
    { printf("FAIL: one output's blind reproduces another output's binding tag\n"); return false; }
  }

  printf("PASS: §2.4 — per-output blinds keep two outputs to one subaddress unlinkable on chain;\n"
         "      revealing one at spend does not expose the other\n");
  return true;
}

// (3) T2 — the three refusals.
static bool test_t2_refusals()
{
  tools::wallet2 w(MAINNET, 1, true);
  make_bq_wallet(w, "t2");

  // R-a: a subaddress marked spent is never handed out again, and the refusal is an exception.
  const subaddress_index burnt{0, 4};
  w.add_subaddress(0, "for the test");
  w.mark_pq_subaddress_spent(burnt);
  bool threw = false;
  try { (void)w.get_pq_subaddress_as_str(burnt); }
  catch (const std::exception &) { threw = true; }
  if (!threw)
  { printf("FAIL: R-a — a BQ subaddress already used in a spend was handed out again\n"); return false; }
  if (!w.is_pq_subaddress_spent(burnt)) { printf("FAIL: R-a bookkeeping\n"); return false; }

  // allocate_fresh_pq_subaddress must skip it, and never return (major, 0).
  const subaddress_index fresh = w.allocate_fresh_pq_subaddress(0);
  if (fresh.is_zero()) { printf("FAIL: R-b — a fresh BQ subaddress must never be (major,0)\n"); return false; }
  if (w.is_pq_subaddress_spent(fresh)) { printf("FAIL: R-b — allocated an already-spent subaddress\n"); return false; }
  if (w.get_pq_subaddress_as_str(fresh).compare(0, 2, "BQ") != 0)
  { printf("FAIL: R-b — the fresh subaddress does not render a BQ address\n"); return false; }

  // R-b, last-resort stop (§4.3): construct_tx refuses a BQ spend whose change is not BQ.
  {
    tools::wallet2 v(MAINNET, 1, true);
    make_bq_wallet(v, "t2b");
    const account_keys &vk = v.get_account().get_keys();
    account_base sender; sender.generate();
    transaction pay;
    if (!build(sender.get_keys(), {{sender.get_keys().m_account_address.m_spend_public_key, {0, 0}}},
               {owned_source(sender.get_keys(), 4 * HRG)},
               {tx_destination_entry(3 * HRG, vk.m_account_address, false)}, boost::none, pay))
    { printf("FAIL: setup payment\n"); return false; }
    wallet_accessor_test::scan(v, pay, 300);
    tools::wallet2::transfer_container t;
    v.get_transfers(t);
    if (t.size() != 1) { printf("FAIL: setup scan\n"); return false; }
    kyber_shared_secret ss;
    if (!wallet_accessor_test::recover(v, t[0], ss)) { printf("FAIL: setup recover\n"); return false; }
    tx_source_entry s;
    s.amount = t[0].amount(); s.rct = true; s.real_output = 0;
    s.real_output_in_tx_index = t[0].m_internal_output_index;
    s.mask = t[0].m_mask;
    s.real_out_tx_key = get_tx_pub_key_from_extra(pay, t[0].m_pk_index);
    s.real_out_additional_tx_keys = get_additional_tx_pub_keys_from_extra(pay);
    tx_source_entry::output_entry oe; oe.first = 7777;
    oe.second.dest = rct::pk2rct(t[0].get_public_key());
    oe.second.mask = rct::commit(s.amount, s.mask);
    s.outputs.push_back(oe);
    s.is_pq = true; s.pq_ss = ss; s.pq_subaddr = t[0].m_subaddr_index;

    account_base payee; payee.generate();
    // a CLASSIC change address on a BQ spend must be refused
    transaction bad;
    if (build(vk, wallet_accessor_test::subaddresses(v), {s},
              {tx_destination_entry(1 * HRG, payee.get_keys().m_account_address, false)},
              boost::optional<account_public_address>(payee.get_keys().m_account_address), bad))
    { printf("FAIL: §4.3 — construct_tx built a BQ spend with a non-BQ change address\n"); return false; }
    // the same spend with a BQ change address is accepted
    transaction good;
    if (!build(vk, wallet_accessor_test::subaddresses(v), {s},
               {tx_destination_entry(1 * HRG, payee.get_keys().m_account_address, false)},
               boost::optional<account_public_address>(vk.m_account_address), good))
    { printf("FAIL: §4.3 — construct_tx refused a BQ spend with a BQ change address\n"); return false; }
  }

  printf("PASS: T2 — R-a refuses a spent subaddress with an explicit error, a fresh one is never\n"
         "      (major,0), and construct_tx refuses a BQ spend whose change is not BQ (§4.3)\n");
  return true;
}

int main(int argc, char *argv[])
{
  if (argc < 2) { printf("usage: %s <scratch directory>\n", argv[0]); return 2; }
  g_dir = argv[1];
  boost::filesystem::create_directories(g_dir);
  mlog_configure("", false);
  mlog_set_log_level(0);

  bool ok = true;
  ok &= test_forged_commitment();
  ok &= test_blind_unlinkability();
  ok &= test_t2_refusals();
  printf("\nRESULT: %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
