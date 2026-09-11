// HIDERING Phase 5 (HFv16) — decision 4 / design 2b option B3: BQ subaddresses.
//
// Standalone; links libwallet because it drives the REAL code on both sides of a payment:
// construct_tx_and_get_tx_key builds the transaction, and wallet2::process_new_transaction scans
// it — no reimplementation of either.
//
// WHAT IS BEING PROVEN.
//
//  1. Derivation. Each BQ subaddress has its OWN ML-KEM-768 key, derived deterministically: the
//     same seed restores the same subaddress addresses, two subaddresses never share a key (the
//     B2 linkability is absent), and (0,0) is exactly the primary BQ address.
//
//  2. Address format. A BQ subaddress renders "BQ", parses back with is_subaddress = true, and
//     the primary still parses with is_subaddress = false. The sender needs that bit.
//
//  3. The selection tag is blinded. Two payments to the same subaddress carry unrelated tags;
//     only the holder of the derivation can unblind them.
//
//  4. End to end. One transaction pays the primary BQ address and two BQ subaddresses of one
//     wallet (forcing additional tx keys) plus a classic output. The recipient's wallet finds
//     all three outputs, on the right subaddresses, with the right amounts — with its keys
//     encrypted in memory, as by default, asking the password exactly once. The per-output
//     spend secret is recovered and reproduces the on-chain binding tag. Another BQ wallet
//     scanning the same transaction finds nothing and is never asked for its password.
//
//  5. Consensus index rule and the tx_extra budget, measured on real transactions: how many
//     BQ outputs fit under MAX_TX_EXTRA_SIZE_PQ, for a B...→BQ payment and for a BQ spend.
//
// Usage: pq_subaddress_test <scratch directory>   (wallet files are written there)
#include <cstdio>
#include <cstring>
#include <map>
#include <numeric>
#include <set>
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

// wallet2 grants friendship to ::wallet_accessor_test (the unit tests use the same hook).
class wallet_accessor_test
{
public:
  static void scan(tools::wallet2 &w, const transaction &tx, uint64_t height)
  {
    std::vector<uint64_t> o_indices(tx.vout.size());
    std::iota(o_indices.begin(), o_indices.end(), height * 100);
    w.process_new_transaction(get_transaction_hash(tx), tx, o_indices, height, HF_VERSION_PQ, 0,
                              false /*miner*/, false /*pool*/, false, tools::wallet2::tx_cache_data{}, nullptr, true);
  }
  static bool recover(const tools::wallet2 &w, const tools::wallet2::transfer_details &td, kyber_shared_secret &ss)
  {
    return w.recover_pq_spend_secret(td, ss);
  }
  static size_t pq_table(const tools::wallet2 &w) { return w.m_pq_subaddress_indices.size(); }
  static size_t subaddr_table(const tools::wallet2 &w) { return w.m_subaddresses.size(); }
};

namespace
{
  std::string g_dir;

  struct pw_callback : tools::i_wallet2_callback
  {
    int calls = 0;
    epee::wipeable_string pw;
    explicit pw_callback(const char *p) : pw(p) {}
    boost::optional<epee::wipeable_string> on_get_password(const char *) override { ++calls; return pw; }
  };

  crypto::secret_key seed_key(uint8_t b)
  {
    crypto::secret_key k;
    for (int i = 0; i < 32; ++i) ((uint8_t *)k.data)[i] = (uint8_t)(b + 3 * i);
    sc_reduce32((uint8_t *)k.data);
    return k;
  }

  // A BQ wallet on disk (verify_password reads the keys file), keys encrypted in memory as by
  // default. generate(use_pq) derives the BQ keys before setup_keys encrypts them.
  void make_bq_wallet(tools::wallet2 &w, const std::string &name, uint8_t seed, const char *pw)
  {
    const std::string path = g_dir + "/" + name;
    for (const char *ext : {"", ".keys", ".address.txt"})
      boost::filesystem::remove(path + ext);
    w.generate(path, pw, seed_key(seed), true /*recover*/, false, false, true /*use_pq*/);
  }

  bool parse(const std::string &s, address_parse_info &info)
  {
    return get_account_address_from_str(info, MAINNET, s);
  }

  // A previous output owned by `sender`, with random ring members around it. construct_tx does
  // not look the ring up on a chain, so this is enough to build a real, fully signed transaction.
  tx_source_entry owned_source(const account_keys &sender, uint64_t amount)
  {
    keypair txkey = keypair::generate(hw::get_device("default"));
    crypto::key_derivation der;
    crypto::generate_key_derivation(sender.m_account_address.m_view_public_key, txkey.sec, der);
    crypto::public_key P;
    crypto::derive_public_key(der, 0, sender.m_account_address.m_spend_public_key, P);
    tx_source_entry src;
    src.amount = amount;
    src.rct = true;
    src.mask = rct::skGen();
    src.real_output = 7;
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

  bool build(const account_keys &sender, std::vector<tx_source_entry> sources, std::vector<tx_destination_entry> dests,
             transaction &tx)
  {
    std::unordered_map<crypto::public_key, subaddress_index> subaddrs;
    subaddrs[sender.m_account_address.m_spend_public_key] = {0, 0};
    crypto::secret_key tx_key;
    std::vector<crypto::secret_key> additional;
    const rct::RCTConfig cfg{rct::RangeProofPaddedBulletproof, 4};
    return construct_tx_and_get_tx_key(sender, subaddrs, sources, dests, boost::none, {}, tx, tx_key, additional,
                                       true, cfg, true /*view tags*/, HF_VERSION_PQ);
  }
}

// (1) + (2)
static bool test_derivation_and_format()
{
  tools::wallet2 w(MAINNET), w2(MAINNET), other(MAINNET);
  make_bq_wallet(w, "deriv", 0x11, "pw");
  make_bq_wallet(w2, "deriv_restore", 0x11, "pw");   // same seed: a restore
  make_bq_wallet(other, "deriv_other", 0x12, "pw");

  // Rendering a subaddress needs the PQ root, which is encrypted in memory: must be refused,
  // never silently derived from the scrambled bytes (the address would be unspendable).
  bool refused = false;
  try { (void)w.get_pq_subaddress_as_str({0, 1}); }
  catch (const tools::error::password_needed &) { refused = true; }
  if (!refused) { printf("FAIL: a BQ subaddress was rendered from keys still encrypted in memory\n"); return false; }

  epee::wipeable_string pw("pw");
  tools::wallet_keys_unlocker u1(w, &pw), u2(w2, &pw), u3(other, &pw);

  const std::string primary = get_pq_address_str(w.get_account().get_keys(), MAINNET);
  if (w.get_pq_subaddress_as_str({0, 0}) != primary)
  { printf("FAIL: BQ subaddress (0,0) is not the primary BQ address\n"); return false; }

  std::vector<std::string> subs;
  for (uint32_t major = 0; major < 3; ++major)
    for (uint32_t minor = (major == 0 ? 1 : 0); minor < 4; ++minor)
    {
      const std::string s = w.get_pq_subaddress_as_str({major, minor});
      if (s.compare(0, 2, "BQ") != 0) { printf("FAIL: BQ subaddress (%u,%u) does not render \"BQ\"\n", major, minor); return false; }
      if (s != w2.get_pq_subaddress_as_str({major, minor}))
      { printf("FAIL: restoring from the same seed gave a different BQ subaddress (%u,%u)\n", major, minor); return false; }
      if (s == other.get_pq_subaddress_as_str({major, minor}))
      { printf("FAIL: two different seeds gave the same BQ subaddress\n"); return false; }
      address_parse_info info;
      if (!parse(s, info) || !info.is_subaddress || !info.address.is_pq())
      { printf("FAIL: BQ subaddress (%u,%u) does not parse back as a BQ subaddress\n", major, minor); return false; }
      if (info.address.m_spend_public_key != w.get_subaddress({major, minor}).m_spend_public_key)
      { printf("FAIL: the Ed25519 half of BQ subaddress (%u,%u) is not the classic subaddress\n", major, minor); return false; }
      subs.push_back(s);
    }
  address_parse_info pinfo;
  if (!parse(primary, pinfo) || pinfo.is_subaddress || !pinfo.address.is_pq())
  { printf("FAIL: the primary BQ address no longer parses as a primary address\n"); return false; }

  // B2's defect is exactly what must be absent: no two subaddresses share an ML-KEM key.
  std::set<std::string> kems;
  for (const std::string &s : subs)
  {
    address_parse_info info; parse(s, info);
    kems.insert(std::string((const char *)info.address.pq_kyber_pk->data(), ML_KEM_768_PUBLIC_KEY_BYTES));
  }
  kems.insert(std::string((const char *)pinfo.address.pq_kyber_pk->data(), ML_KEM_768_PUBLIC_KEY_BYTES));
  if (kems.size() != subs.size() + 1) { printf("FAIL: two BQ subaddresses share an ML-KEM key (linkable)\n"); return false; }

  // The marker must render "BQ" whatever the keys: 2000 random payloads.
  for (int n = 0; n < 2000; ++n)
  {
    account_public_address a;
    a.m_spend_public_key = rct::rct2pk(rct::pkGen());
    a.m_view_public_key = rct::rct2pk(rct::pkGen());
    std::array<uint8_t, ML_KEM_768_PUBLIC_KEY_BYTES> k;
    for (auto &b : k) b = (uint8_t)crypto::rand<uint8_t>();
    a.pq_kyber_pk = k;
    if (get_account_address_as_str_pq(MAINNET, a, true).compare(0, 2, "BQ") != 0)
    { printf("FAIL: the BQ subaddress marker does not always render \"BQ\"\n"); return false; }
  }

  printf("PASS: each BQ subaddress has its own ML-KEM key, restores from the seed, renders \"BQ\",\n"
         "      and parses back as a subaddress; (0,0) is the primary; locked keys are refused\n");
  return true;
}

// (3)
static bool test_tag_is_blinded()
{
  std::array<uint8_t, ML_KEM_768_PUBLIC_KEY_BYTES> kem;
  for (auto &b : kem) b = (uint8_t)crypto::rand<uint8_t>();
  crypto::key_derivation d1, d2;
  crypto::generate_key_derivation(rct::rct2pk(rct::pkGen()), rct::rct2sk(rct::skGen()), d1);
  crypto::generate_key_derivation(rct::rct2pk(rct::pkGen()), rct::rct2sk(rct::skGen()), d2);
  bq_sel_tag t1, t2, t3, fp;
  pqc_compute_sel_tag((const uint8_t *)&d1, 32, 0, kem.data(), kem.size(), t1);
  pqc_compute_sel_tag((const uint8_t *)&d2, 32, 0, kem.data(), kem.size(), t2);
  pqc_compute_sel_tag((const uint8_t *)&d1, 32, 1, kem.data(), kem.size(), t3);
  pqc_kem_pk_fingerprint(kem.data(), kem.size(), fp);
  if (!memcmp(t1.data, t2.data, 8) || !memcmp(t1.data, t3.data, 8))
  { printf("FAIL: two payments to the same subaddress carry the same selection tag (linkable)\n"); return false; }
  if (!memcmp(t1.data, fp.data, 8))
  { printf("FAIL: the selection tag is the bare key fingerprint\n"); return false; }
  bq_sel_tag pad;
  pqc_sel_pad((const uint8_t *)&d1, 32, 0, pad);
  if (memcmp(bq_sel_tag_xor(t1, pad).data, fp.data, 8) != 0)
  { printf("FAIL: the derivation holder cannot unblind the tag\n"); return false; }
  printf("PASS: the selection tag differs per derivation and per output index, and only the\n"
         "      derivation unblinds it to the subaddress fingerprint\n");
  return true;
}

// (4) + consensus index rule
static bool test_end_to_end_scan()
{
  pw_callback cb_r("pw-r"), cb_o("pw-o");
  tools::wallet2 recv(MAINNET), other(MAINNET);
  make_bq_wallet(recv, "recv", 0x21, "pw-r");
  make_bq_wallet(other, "other", 0x22, "pw-o");
  recv.callback(&cb_r);
  other.callback(&cb_o);

  if (wallet_accessor_test::pq_table(recv) != wallet_accessor_test::subaddr_table(recv))
  { printf("FAIL: the BQ fingerprint table was not built at wallet creation (%zu of %zu)\n",
           wallet_accessor_test::pq_table(recv), wallet_accessor_test::subaddr_table(recv)); return false; }

  std::string s_primary, s_sub1, s_sub2;
  {
    epee::wipeable_string pw("pw-r");
    tools::wallet_keys_unlocker u(recv, &pw);
    s_primary = recv.get_pq_subaddress_as_str({0, 0});
    s_sub1 = recv.get_pq_subaddress_as_str({0, 5});
    s_sub2 = recv.get_pq_subaddress_as_str({1, 3});
  }
  address_parse_info a0, a1, a2;
  if (!parse(s_primary, a0) || !parse(s_sub1, a1) || !parse(s_sub2, a2)) { printf("FAIL: address parse\n"); return false; }

  account_base sender; sender.generate();
  account_base third; third.generate();
  const uint64_t HRG = 1000000000000ull;
  std::vector<tx_destination_entry> dests = {
    tx_destination_entry(1 * HRG, a0.address, a0.is_subaddress),
    tx_destination_entry(2 * HRG, a1.address, a1.is_subaddress),
    tx_destination_entry(3 * HRG, a2.address, a2.is_subaddress),
    tx_destination_entry(4 * HRG, third.get_keys().m_account_address, false),
  };
  transaction tx;
  if (!build(sender.get_keys(), {owned_source(sender.get_keys(), 11 * HRG)}, dests, tx))
  { printf("FAIL: construct_tx refused a payment to BQ subaddresses\n"); return false; }

  std::vector<tx_extra_field> fields;
  if (!parse_tx_extra(tx.extra, fields)) { printf("FAIL: tx_extra does not parse canonically\n"); return false; }
  size_t n_ct = 0;
  for (const auto &f : fields) if (f.type() == typeid(tx_extra_kyber_ct)) ++n_ct;
  if (n_ct != 3 || !check_pq_output_field_indices(fields, tx.vout.size()))
  { printf("FAIL: expected 3 indexed ML-KEM fields matching the binding fields (got %zu)\n", n_ct); return false; }
  if (get_additional_tx_pub_keys_from_extra(tx).size() != tx.vout.size())
  { printf("FAIL: paying two subaddresses should use additional tx keys\n"); return false; }

  // The other BQ wallet: its tags never match, so it must neither find anything nor ask.
  wallet_accessor_test::scan(other, tx, 100);
  tools::wallet2::transfer_container other_t;
  other.get_transfers(other_t);
  if (!other_t.empty()) { printf("FAIL: an unrelated BQ wallet detected outputs that are not its own\n"); return false; }
  if (cb_o.calls != 0) { printf("FAIL: an unrelated BQ wallet was asked for its password %d time(s)\n", cb_o.calls); return false; }

  // The recipient, keys encrypted in memory.
  wallet_accessor_test::scan(recv, tx, 100);
  tools::wallet2::transfer_container t;
  recv.get_transfers(t);
  if (t.size() != 3) { printf("FAIL: the recipient found %zu of its 3 BQ outputs\n", t.size()); return false; }
  if (cb_r.calls != 1) { printf("FAIL: the recipient was asked for its password %d times, expected once\n", cb_r.calls); return false; }
  std::map<std::pair<uint32_t, uint32_t>, uint64_t> got;
  for (const auto &td : t) got[{td.m_subaddr_index.major, td.m_subaddr_index.minor}] = td.amount();
  const std::map<std::pair<uint32_t, uint32_t>, uint64_t> want = {{{0, 0}, 1 * HRG}, {{0, 5}, 2 * HRG}, {{1, 3}, 3 * HRG}};
  if (got != want) { printf("FAIL: outputs landed on the wrong subaddresses or with wrong amounts\n"); return false; }

  // Spend authority: the recovered per-output secret must reproduce the on-chain binding tag.
  for (const auto &td : t)
  {
    kyber_shared_secret ss;
    if (!wallet_accessor_test::recover(recv, td, ss))
    { printf("FAIL: cannot recover the spend secret of the output on (%u,%u)\n", td.m_subaddr_index.major, td.m_subaddr_index.minor); return false; }
    pq_public_key dpk; pq_secret_key dsk;
    pqc_keygen_output_dsa(ss, td.m_internal_output_index, dpk, dsk);
    crypto::public_key P;
    get_output_public_key(tx.vout[td.m_internal_output_index], P);
    uint8_t expect[32];
    pqc_compute_bind_tag((const uint8_t *)&P, 32, dpk.dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES, expect);
    bool found = false;
    for (const auto &f : fields)
      if (f.type() == typeid(tx_extra_pq_bind) && boost::get<tx_extra_pq_bind>(f).output_index == td.m_internal_output_index)
        found = !memcmp(&boost::get<tx_extra_pq_bind>(f).bind_tag, expect, 32);
    if (!found) { printf("FAIL: the recovered secret does not reproduce the binding tag\n"); return false; }
  }

  // The consensus rule rejects the malformations it exists for.
  {
    std::vector<tx_extra_field> bad = fields;
    for (auto &f : bad) if (f.type() == typeid(tx_extra_kyber_ct)) { boost::get<tx_extra_kyber_ct>(f).output_index = tx.vout.size(); break; }
    if (check_pq_output_field_indices(bad, tx.vout.size())) { printf("FAIL: an out-of-range ciphertext index was accepted\n"); return false; }
    bad = fields;
    std::vector<size_t> ct_pos;
    for (size_t n = 0; n < bad.size(); ++n) if (bad[n].type() == typeid(tx_extra_kyber_ct)) ct_pos.push_back(n);
    boost::get<tx_extra_kyber_ct>(bad[ct_pos[1]]).output_index = boost::get<tx_extra_kyber_ct>(bad[ct_pos[0]]).output_index;
    if (check_pq_output_field_indices(bad, tx.vout.size())) { printf("FAIL: a duplicate ciphertext index was accepted\n"); return false; }
    bad = fields;
    bad.erase(bad.begin() + ct_pos[2]);
    if (check_pq_output_field_indices(bad, tx.vout.size())) { printf("FAIL: a binding tag without its ciphertext was accepted\n"); return false; }
  }

  printf("PASS: one tx pays the primary BQ address and two BQ subaddresses; the recipient finds\n"
         "      all three on the right subaddresses (password asked once, keys encrypted in memory),\n"
         "      recovers each spend secret; an unrelated BQ wallet finds nothing and is never asked;\n"
         "      the consensus index rule rejects out-of-range, duplicate and unpaired fields\n");
  return true;
}

// (5) the budget, on real transactions
static bool test_extra_budget()
{
  tools::wallet2 recv(MAINNET);
  make_bq_wallet(recv, "budget", 0x31, "pw");
  const account_public_address bq = recv.get_account().get_keys().m_account_address; // primary BQ
  const uint64_t HRG = 1000000000000ull;

  account_base classic; classic.generate();
  size_t max_b_to_bq = 0, extra_at_max = 0;
  for (size_t n = 6; n <= 8; ++n)
  {
    std::vector<tx_destination_entry> dests(n, tx_destination_entry(1 * HRG, bq, false));
    transaction tx;
    if (build(classic.get_keys(), {owned_source(classic.get_keys(), (n + 1) * HRG)}, dests, tx))
    { max_b_to_bq = n; extra_at_max = tx.extra.size(); }
  }

  // A transparent BQ spend: the input is a BQ output (its per-output ML-DSA key lives in vin,
  // not in extra), and the account-level ML-DSA-65 signature is appended to extra.
  account_base bqs; bqs.generate();
  if (!generate_pq_keys(bqs.get_keys_nonconst())) { printf("FAIL: BQ sender keygen\n"); return false; }
  size_t max_spend = 0, spend_extra_at_max = 0;
  for (size_t n = 1; n <= 3; ++n)
  {
    // A genuine BQ output of the spender (CRIT-3: construct_tx must be able to derive its
    // one-time secret): P' = H_s(r*A, 0)*G + B + t*G, t from the output's ML-KEM secret.
    const account_keys &k = bqs.get_keys();
    kyber_shared_secret ss; for (auto &b : ss.ss) b = crypto::rand<uint8_t>();
    const crypto::secret_key r = rct::rct2sk(rct::skGen());
    crypto::key_derivation der;
    crypto::generate_key_derivation(k.m_account_address.m_view_public_key, r, der);
    crypto::public_key P;
    crypto::derive_public_key(der, 0, k.m_account_address.m_spend_public_key, P);
    crypto::secret_key t;
    derive_bq_output_tweak(ss, 0, t);
    crypto::public_key tG;
    crypto::secret_key_to_public_key(t, tG);
    tx_source_entry src;
    src.amount = (n + 1) * HRG; src.rct = true; src.real_output = 0; src.real_output_in_tx_index = 0;
    src.mask = rct::identity();
    src.real_out_tx_key = rct::rct2pk(rct::scalarmultBase(rct::sk2rct(r)));
    tx_source_entry::output_entry oe; oe.first = 42;
    oe.second.dest = rct::addKeys(rct::pk2rct(P), rct::pk2rct(tG)); oe.second.mask = rct::zeroCommit(src.amount);
    src.outputs.push_back(oe);
    src.is_pq = true;
    src.pq_ss = ss;
    std::vector<tx_destination_entry> dests(n, tx_destination_entry(1 * HRG, bq, false));
    transaction tx;
    if (build(bqs.get_keys(), {src}, dests, tx))
    { max_spend = n; spend_extra_at_max = tx.extra.size(); }
  }

  printf("INFO: MAX_TX_EXTRA_SIZE_PQ = %u; a BQ output costs %u bytes of tx_extra\n"
         "      (ML-KEM field 1+1+8+1088 = 1098, binding field 1+1+32 = 34)\n",
         (unsigned)MAX_TX_EXTRA_SIZE_PQ, 1098u + 34u);
  printf("INFO: B...->BQ payment: at most %zu BQ outputs (tx_extra %zu bytes at the maximum)\n", max_b_to_bq, extra_at_max);
  printf("INFO: transparent BQ spend: at most %zu BQ outputs (tx_extra %zu bytes at the maximum)\n", max_spend, spend_extra_at_max);
  if (max_b_to_bq != 7 || max_spend != 2)
  { printf("FAIL: budget differs from the analysis (expected 7 and 2)\n"); return false; }
  printf("PASS: tx_extra budget measured on real transactions matches the analysis: 7 BQ outputs\n"
         "      for a B...->BQ payment, 2 for a transparent BQ spend (payment + BQ change)\n");
  return true;
}

int main(int argc, char **argv)
{
  if (argc < 2) { printf("usage: %s <scratch dir>\n", argv[0]); return 2; }
  g_dir = argv[1];
  mlog_configure("", false);
  mlog_set_log_level(0);
  bool ok = true;
  ok &= test_tag_is_blinded();
  ok &= test_derivation_and_format();
  ok &= test_end_to_end_scan();
  ok &= test_extra_budget();
  printf(ok ? "RESULT: PASS\n" : "RESULT: FAIL\n");
  return ok ? 0 : 1;
}
