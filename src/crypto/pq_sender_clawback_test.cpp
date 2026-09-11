// HIDERING Phase 5 (HFv16) — audit CRIT-3 (11 Sep 2026): the SENDER of a BQ output could spend it.
//
// Asserts the property the design needs — "only the recipient can spend a BQ output" — against
// the real code: construct_tx_and_get_tx_key builds every transaction, wallet2 scans the payment,
// and each spend is checked with the validator's per-input checks reproduced line for line from
// Blockchain::check_tx_inputs: (b), (b2), (c), (d), (d2), the account-level signature, (e).
// Doc: docs/audit/CRIT-3_sender_can_reclaim_bq_output_2026-09-11.md
//
// THE BUG. A transparent BQ spend (txin_to_key_pq) was authorised by:
//   * the per-output ML-DSA-65 key, derived from (ss, output index)          (checks c, d)
//   * the output's amount and blinding mask                                  (check b2)
//   * an account-level ML-DSA-65 signature by ANY key (C-1: not bound to the owner)
// with Keccak(P') — a public value — as double-spend marker. ss is the ML-KEM shared secret, which
// the SENDER obtains from pqc_stealth_encaps when it builds the output (cryptonote_tx_utils.cpp:
// `kss`); honest code merely wipes it. The amount and the mask are the sender's too: it chose one
// and derives the other from its own tx key. Nothing asked for the one thing only the recipient
// has — the one-time secret x with P' = x*G. Before the fix this test was red: a spend built from
// sender-side knowledge alone passed every check above, with the victim's own key image.
//
// THE FIX, check (d2): every txin_to_key_pq carries an Ed25519 signature by x over the same
// message as its ML-DSA signature. x = H_s(d, i) + b (+ m) + t needs the recipient's spend key b.
//
// HOW THE SENDER'S ss IS OBTAINED HERE. Honest construct_tx wipes its copy, so the test reads the
// secret through the recipient's decapsulation. By ML-KEM correctness that is bit for bit the
// value pqc_stealth_encaps returned inside the sender's process. Everything else the attacker uses
// is computed from sender-side data only (the tx key construct_tx returns to its caller) and
// checked against what the recipient recorded.
#include <cstdio>
#include <cstring>
#include <numeric>
#include <string>
#include <vector>

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
  {
    return w.recover_pq_spend_secret(td, ss);
  }
  static const std::unordered_map<crypto::public_key, subaddress_index> &subaddresses(const tools::wallet2 &w)
  {
    return w.m_subaddresses;
  }
};

namespace
{
  const uint64_t HRG = 1000000000000ull;

  tx_source_entry owned_source(const account_keys &sender, uint64_t amount)
  {
    keypair txkey = keypair::generate(hw::get_device("default"));
    crypto::key_derivation der;
    crypto::generate_key_derivation(sender.m_account_address.m_view_public_key, txkey.sec, der);
    crypto::public_key P;
    crypto::derive_public_key(der, 0, sender.m_account_address.m_spend_public_key, P);
    tx_source_entry src;
    src.amount = amount; src.rct = true; src.mask = rct::skGen(); src.real_output = 5;
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
             transaction &tx, crypto::secret_key &tx_key)
  {
    std::vector<crypto::secret_key> additional;
    const rct::RCTConfig cfg{rct::RangeProofPaddedBulletproof, 4};
    return construct_tx_and_get_tx_key(sender, subaddrs, sources, dests, boost::none, {}, tx, tx_key, additional,
                                       true, cfg, true, HF_VERSION_PQ);
  }

  std::unordered_map<crypto::public_key, subaddress_index> own_map(const account_keys &k)
  {
    return {{k.m_account_address.m_spend_public_key, {0, 0}}};
  }

  const size_t PQ_SIG_FIELD = 1 + ML_DSA_65_PUBLIC_KEY_BYTES + ML_DSA_65_SIGNATURE_BYTES;

  // The message both per-input signatures sign: the prefix hash with every PQ input signature —
  // dsa.sig AND owner_sig — zeroed, and the trailing account-level pq_sig field dropped. Exactly
  // what Blockchain::check_tx_inputs reconstructs.
  crypto::hash pq_input_message(transaction tx)
  {
    for (auto &in : tx.vin)
      if (in.type() == typeid(txin_to_key_pq))
      {
        txin_to_key_pq &p = boost::get<txin_to_key_pq>(in);
        memset(p.dsa.sig, 0, ML_DSA_65_SIGNATURE_BYTES);
        memset(&p.owner_sig, 0, sizeof(p.owner_sig));
      }
    tx.extra.resize(tx.extra.size() - PQ_SIG_FIELD);
    return get_transaction_prefix_hash(tx);
  }

  // What the chain knows about a BQ output being spent.
  struct chain_output
  {
    crypto::public_key P;
    rct::key C;
    crypto::hash bind_tag;
  };

  chain_output chain_view(const transaction &pay, size_t i)
  {
    chain_output o{};
    get_output_public_key(pay.vout[i], o.P);
    o.C = pay.rct_signatures.outPk[i].mask;
    std::vector<tx_extra_field> fields;
    parse_tx_extra(pay.extra, fields);
    for (const auto &f : fields)
      if (f.type() == typeid(tx_extra_pq_bind) && boost::get<tx_extra_pq_bind>(f).output_index == i)
        o.bind_tag = boost::get<tx_extra_pq_bind>(f).bind_tag;
    return o;
  }

  struct verdict
  {
    bool b = false, b2 = false, c = false, d = false, d2 = false, ext = false, e = false;
    bool all() const { return b && b2 && c && d && d2 && ext && e; }
    bool all_but_d2() const { return b && b2 && c && d && ext && e; }
  };

  // Validator checks for one input of `spend`, which spends `out`.
  verdict validate(const transaction &spend, size_t vin_index, const chain_output &out)
  {
    verdict v;
    std::vector<tx_extra_field> fields;
    if (!parse_tx_extra(spend.extra, fields) || fields.empty() || fields.back().type() != typeid(tx_extra_pq_sig))
      return v;
    const txin_to_key_pq &in = boost::get<txin_to_key_pq>(spend.vin[vin_index]);
    v.b = in.real_output_key == out.P;
    v.b2 = out.C == rct::commit(in.amount, in.mask);
    uint8_t expect[32];
    pqc_compute_bind_tag((const uint8_t *)&in.real_output_key, 32, in.dsa.pk, ML_DSA_65_PUBLIC_KEY_BYTES, expect);
    v.c = !memcmp(expect, &out.bind_tag, 32);
    const crypto::hash msg = pq_input_message(spend);
    v.d = pqc_tx_verify((const uint8_t *)&msg, sizeof(msg), in.dsa);
    v.d2 = crypto::check_signature(msg, in.real_output_key, in.owner_sig);
    transaction stripped = spend;
    stripped.extra.resize(stripped.extra.size() - PQ_SIG_FIELD);
    const crypto::hash h = get_transaction_prefix_hash(stripped);
    v.ext = pqc_tx_verify((const uint8_t *)&h, sizeof(h), boost::get<tx_extra_pq_sig>(fields.back()).sig);
    if (spend.rct_signatures.type != rct::RCTTypeNull)
    {
      // A hybrid is a RingCT tx: its balance is closed by verRctSemanticsSimple with the
      // transparent term (pq_hybrid_test covers it), not by arithmetic on revealed amounts.
      v.e = true;
      return v;
    }
    uint64_t in_sum = 0, out_sum = 0;
    for (const auto &x : spend.vin) in_sum += boost::get<txin_to_key_pq>(x).amount;
    for (const auto &o : spend.vout) out_sum += o.amount;
    v.e = in_sum >= out_sum;
    return v;
  }

  void print(const char *who, const verdict &v)
  {
    auto a = [](bool x) { return x ? "accepted" : "REJECTED"; };
    printf("  %s:\n", who);
    printf("    (b) %s  (b2) %s  (c) %s  (d) %s  (d2) %s  account sig %s  (e) %s\n",
           a(v.b), a(v.b2), a(v.c), a(v.d), a(v.d2), a(v.ext), a(v.e));
  }
}

int main()
{
  mlog_configure("", false);
  mlog_set_log_level(0);

  // The victim: a BQ wallet, unattended so its keys are not encrypted in memory.
  tools::wallet2 victim(MAINNET, 1, true);
  victim.generate("", "", rct::rct2sk(rct::skGen()), true, false, false, true /*use_pq*/);
  const account_keys &vk = victim.get_account().get_keys();
  address_parse_info sub;
  if (!get_account_address_from_str(sub, MAINNET, victim.get_pq_subaddress_as_str({0, 3})))
  { printf("SETUP FAIL: victim BQ subaddress\n"); return 2; }

  // The sender pays the victim's primary BQ address and one BQ subaddress, with the real builder,
  // and keeps the tx key construct_tx hands back to it.
  account_base sender; sender.generate();
  transaction pay; crypto::secret_key sender_tx_key;
  if (!build(sender.get_keys(), own_map(sender.get_keys()), {owned_source(sender.get_keys(), 8 * HRG)},
             {tx_destination_entry(5 * HRG, vk.m_account_address, false),
              tx_destination_entry(2 * HRG, sub.address, true)}, pay, sender_tx_key))
  { printf("SETUP FAIL: could not build the B...->BQ payment\n"); return 2; }

  wallet_accessor_test::scan(victim, pay, 100);
  tools::wallet2::transfer_container t;
  victim.get_transfers(t);
  if (t.size() != 2) { printf("SETUP FAIL: the victim detected %zu of its 2 BQ outputs\n", t.size()); return 2; }
  const tools::wallet2::transfer_details &td_primary = t[0].m_subaddr_index.is_zero() ? t[0] : t[1];
  const size_t i = td_primary.m_internal_output_index;

  bool ok = true;

  // --- 1. Positive control: the RECIPIENT spends both outputs in one tx; all checks pass ------
  {
    std::vector<tx_source_entry> srcs;
    for (const auto &td : t)
    {
      kyber_shared_secret ss;
      if (!wallet_accessor_test::recover(victim, td, ss)) { printf("SETUP FAIL: victim cannot recover\n"); return 2; }
      tx_source_entry s;
      s.amount = td.amount(); s.rct = true; s.real_output = 0; s.real_output_in_tx_index = td.m_internal_output_index;
      s.mask = td.m_mask;
      s.real_out_tx_key = get_tx_pub_key_from_extra(pay, td.m_pk_index);
      s.real_out_additional_tx_keys = get_additional_tx_pub_keys_from_extra(pay);
      tx_source_entry::output_entry oe; oe.first = 4242 + td.m_internal_output_index;
      oe.second.dest = rct::pk2rct(td.get_public_key()); oe.second.mask = rct::commit(td.amount(), td.m_mask);
      s.outputs.push_back(oe);
      s.is_pq = true; s.pq_ss = ss;
      srcs.push_back(s);
    }
    account_base payee; payee.generate();
    transaction honest; crypto::secret_key unused;
    if (!build(vk, wallet_accessor_test::subaddresses(victim), srcs,
               {tx_destination_entry(7 * HRG - HRG / 100, payee.get_keys().m_account_address, false)}, honest, unused))
    { printf("FAIL: the recipient could not build a spend of its own BQ outputs\n"); ok = false; }
    else
    {
      printf("The recipient spends its two BQ outputs (primary + subaddress):\n");
      for (size_t n = 0; n < honest.vin.size(); ++n)
      {
        const txin_to_key_pq &in = boost::get<txin_to_key_pq>(honest.vin[n]);
        const size_t oi = in.spent_output_index - 4242;
        const verdict v = validate(honest, n, chain_view(pay, oi));
        print(oi == i ? "input spending the primary-address output" : "input spending the subaddress output", v);
        if (!v.all()) { printf("FAIL: a legitimate BQ spend is rejected\n"); ok = false; }
      }
    }
  }

  // --- 1b. Positive control, HYBRID shape: a ring input plus the BQ input, handed to the builder
  // PQ-first. The builder sorts ring inputs before PQ ones, so this also checks that each PQ
  // input is signed with ITS OWN material after the sort (it used to be matched by pre-sort
  // position, which put a PQ key on the wrong input — or on a ring input — in a hybrid).
  {
    kyber_shared_secret ss_p;
    wallet_accessor_test::recover(victim, td_primary, ss_p);
    tx_source_entry pq;
    pq.amount = td_primary.amount(); pq.rct = true; pq.real_output = 0; pq.real_output_in_tx_index = i;
    pq.mask = td_primary.m_mask;
    pq.real_out_tx_key = get_tx_pub_key_from_extra(pay, td_primary.m_pk_index);
    pq.real_out_additional_tx_keys = get_additional_tx_pub_keys_from_extra(pay);
    tx_source_entry::output_entry oe; oe.first = 4242 + i;
    oe.second.dest = rct::pk2rct(td_primary.get_public_key()); oe.second.mask = rct::commit(pq.amount, pq.mask);
    pq.outputs.push_back(oe);
    pq.is_pq = true; pq.pq_ss = ss_p;
    account_base payee; payee.generate();
    transaction hybrid; crypto::secret_key unused;
    if (!build(vk, wallet_accessor_test::subaddresses(victim), {pq, owned_source(vk, 3 * HRG)},
               {tx_destination_entry(8 * HRG - HRG / 100, payee.get_keys().m_account_address, false)}, hybrid, unused))
    { printf("FAIL: the recipient could not build a hybrid (ring + BQ) spend\n"); ok = false; }
    else
    {
      size_t pq_at = hybrid.vin.size();
      for (size_t n = 0; n < hybrid.vin.size(); ++n)
        if (hybrid.vin[n].type() == typeid(txin_to_key_pq)) pq_at = n;
      if (pq_at != 1 || hybrid.rct_signatures.type == rct::RCTTypeNull)
      { printf("FAIL: the hybrid is not a RingCT tx with the ring input first\n"); ok = false; }
      else
      {
        const verdict v = validate(hybrid, pq_at, chain_view(pay, i));
        print("hybrid spend, BQ input (sorted after the ring input)", v);
        if (!v.all()) { printf("FAIL: the BQ input of a legitimate hybrid spend is rejected\n"); ok = false; }
      }
    }
  }

  // --- What the SENDER knows, computed from sender-side data only ---------------------------
  kyber_shared_secret ss;           // == the kss its pqc_stealth_encaps returned (see header)
  wallet_accessor_test::recover(victim, td_primary, ss);
  crypto::key_derivation d_sender;
  crypto::generate_key_derivation(vk.m_account_address.m_view_public_key, sender_tx_key, d_sender);
  crypto::secret_key amount_key;
  crypto::derivation_to_scalar(d_sender, i, amount_key);
  const rct::key sender_mask = rct::genCommitmentMask(rct::sk2rct(amount_key));
  if (!(sender_mask == td_primary.m_mask))
  { printf("SETUP FAIL: the sender-side mask differs from the one the victim recorded\n"); return 2; }
  const chain_output out = chain_view(pay, i);

  // --- 2. The sender, through the real builder ------------------------------------------------
  account_base attacker; attacker.generate();
  generate_pq_keys(attacker.get_keys_nonconst());
  tx_source_entry src;
  src.amount = 5 * HRG; src.rct = true; src.real_output = 0; src.real_output_in_tx_index = i;
  src.mask = sender_mask;
  src.real_out_tx_key = get_tx_pub_key_from_extra(pay);
  src.real_out_additional_tx_keys = get_additional_tx_pub_keys_from_extra(pay);
  tx_source_entry::output_entry oe; oe.first = 4242; oe.second.dest = rct::pk2rct(out.P); oe.second.mask = out.C;
  src.outputs.push_back(oe);
  src.is_pq = true;
  src.pq_ss = ss;
  transaction via_builder; crypto::secret_key unused;
  const bool built = build(attacker.get_keys(), own_map(attacker.get_keys()), {src},
                           {tx_destination_entry(5 * HRG - HRG / 100, attacker.get_keys().m_account_address, false)}, via_builder, unused);
  printf("The sender, through the real construct_tx: %s\n", built ? "BUILT a spend" : "refused (it cannot derive the one-time secret)");
  if (built)
  {
    const verdict v = validate(via_builder, 0, out);
    print("builder-made spend by the sender", v);
    if (v.all()) { printf("FAIL: CRIT-3 — the sender's spend passes every validator check\n"); ok = false; }
  }

  // --- 3. The sender, by hand: everything it knows, and its best guess at the owner key -------
  // It knows H_s(d, i) (its own derivation) and t (from ss). The recipient's spend key b is the
  // only missing part of x' = H_s(d, i) + b + t, and it is what (d2) demands.
  {
    transaction forged;
    forged.version = 2;
    forged.unlock_time = 0;
    txin_to_key_pq in{};
    in.amount = 5 * HRG;
    in.spent_output_index = 4242;
    in.real_output_key = out.P;
    in.mask = sender_mask;
    pq_public_key dpk; pq_secret_key dsk;
    pqc_keygen_output_dsa(ss, i, dpk, dsk);
    memcpy(in.dsa.pk, dpk.dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES);
    forged.vin.push_back(in);
    tx_out o{};
    o.amount = 5 * HRG - HRG / 100;
    txout_to_tagged_key tk{};
    tk.key = attacker.get_keys().m_account_address.m_spend_public_key;
    o.target = tk;
    forged.vout.push_back(o);
    add_tx_pub_key_to_extra(forged, rct::rct2pk(rct::pkGen()));
    forged.rct_signatures.type = rct::RCTTypeNull;

    const crypto::hash msg = get_transaction_prefix_hash(forged);   // sigs zero, no pq_sig yet
    pq_tx_sig s;
    pqc_tx_sign((const uint8_t *)&msg, sizeof(msg), dsk.dilithium3_sk, ML_DSA_65_SECRET_KEY_BYTES,
                dpk.dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES, s);
    crypto::secret_key hs, t_tweak, x_guess;
    crypto::derivation_to_scalar(d_sender, i, hs);
    derive_bq_output_tweak(ss, i, t_tweak);
    sc_add((unsigned char *)&x_guess, (const unsigned char *)&hs, (const unsigned char *)&t_tweak);
    crypto::public_key x_guess_pub;
    crypto::secret_key_to_public_key(x_guess, x_guess_pub);
    crypto::signature owner;
    crypto::generate_signature(msg, x_guess_pub, x_guess, owner);
    txin_to_key_pq &fin = boost::get<txin_to_key_pq>(forged.vin[0]);
    memcpy(fin.dsa.sig, s.sig, ML_DSA_65_SIGNATURE_BYTES);
    fin.owner_sig = owner;

    const crypto::hash acct = get_transaction_prefix_hash(forged);
    pq_tx_sig ext;
    const auto &ad = *attacker.get_keys().pq_dilithium;
    pqc_tx_sign((const uint8_t *)&acct, sizeof(acct), ad.dilithium_sk, ML_DSA_65_SECRET_KEY_BYTES,
                ad.dilithium_pk, ML_DSA_65_PUBLIC_KEY_BYTES, ext);
    forged.extra.push_back(TX_EXTRA_TAG_PQ_SIG);
    forged.extra.insert(forged.extra.end(), ext.pk, ext.pk + ML_DSA_65_PUBLIC_KEY_BYTES);
    forged.extra.insert(forged.extra.end(), ext.sig, ext.sig + ML_DSA_65_SIGNATURE_BYTES);

    const verdict v = validate(forged, 0, out);
    print("hand-forged spend by the sender (all it knows + best-guess owner key)", v);
    if (get_pq_input_key_image(fin.real_output_key) != get_pq_input_key_image(out.P))
    { printf("FAIL: test setup — the forgery does not target the victim's output\n"); ok = false; }
    if (v.all())
    { printf("FAIL: CRIT-3 — the sender's hand-made spend passes every validator check\n"); ok = false; }
    else if (!v.all_but_d2())
    { printf("FAIL: test setup — the forgery should pass every check except (d2), so that the\n"
             "      test shows (d2) is what stops it\n"); ok = false; }
    else
      printf("PASS: the sender's spend passes (b)(b2)(c)(d), the account signature and (e) —\n"
             "      everything it could compute — and is rejected by (d2) alone\n");
  }

  printf(ok ? "RESULT: PASS (only the recipient can spend a BQ output)\n"
            : "RESULT: FAIL (CRIT-3 — see docs/audit/CRIT-3_sender_can_reclaim_bq_output_2026-09-11.md)\n");
  return ok ? 0 : 1;
}
