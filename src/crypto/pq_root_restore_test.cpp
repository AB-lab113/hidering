// HIDERING Phase 5 (HFv16) — audit CRIT-4, decision R2a: the post-quantum root secret has
// its own seed.
//
// Standalone; links libwallet because it drives the REAL code on both sides: wallet2::generate
// creates and restores the wallets, construct_tx_and_get_tx_key builds the payment, and
// wallet2::process_new_transaction scans it. Nothing here reimplements a derivation.
//
// WHAT IS BEING PROVEN.
//
//  1. Two seeds, one wallet. A BQ wallet issues a classic 25-word seed AND a second,
//     post-quantum one. Restoring from BOTH reproduces the classic address, the BQ address,
//     the ML-KEM/ML-DSA secret keys, and every BQ subaddress.
//
//  2. CRIT-4 itself. The same classic seed with a DIFFERENT post-quantum seed gives the same
//     B... address and a DIFFERENT BQ address. Before the fix that was not expressible: the
//     root WAS m_spend_secret_key, the discrete log of the spend public key published in the
//     BQ address, so Shor on that point yielded every BQ key of the account. This assertion
//     is the property "the post-quantum material is not a function of the Ed25519 half".
//
//  3. Fail-closed restore. Restoring a BQ wallet with only the classic seed is REFUSED.
//     Minting a fresh root there would hand the user a well-formed BQ address that simply is
//     not the one holding the funds — M-4 again, and silent. (The report leaves the UX of
//     "attach a brand new BQ root to an existing classic seed" open; today that flow does not
//     exist, and a refusal is what makes the absence safe.)
//
//  4. It is the funds that move, not just a string. A classic wallet pays the ORIGINAL
//     wallet's primary BQ address and one BQ subaddress. The wallet RESTORED from the two
//     seeds — which never saw the original's key file — finds both outputs on the right
//     subaddresses, and the per-output secret it recovers reproduces the on-chain binding tag
//     (i.e. it can actually spend them). A wallet restored with the WRONG post-quantum seed
//     scans the same transaction and finds nothing.
//
//  5. On-disk round trip. Storing and reloading the restored wallet keeps the root (it is
//     encrypted at rest, appended last in the key stream) and the BQ address.
//
//  6. The seed cannot be shown from scrambled bytes. With the keys encrypted in memory (the
//     default), the root is scrambled; rendering it anyway would print 25 perfectly valid
//     words that restore a DIFFERENT wallet — the worst failure a backup can have. get_pq_seed
//     refuses, and under an unlocker returns words that really do restore the same BQ address.
//     (Found by driving the CLI: the first cut of this change printed the scrambled root.)
//
// Usage: pq_root_restore_test <scratch directory>   (wallet files are written there)
#include <cstdio>
#include <cstring>
#include <map>
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
#include "mnemonics/electrum-words.h"
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
};

namespace
{
  std::string g_dir;

  struct pw_callback : tools::i_wallet2_callback
  {
    epee::wipeable_string pw;
    explicit pw_callback(const char *p) : pw(p) {}
    boost::optional<epee::wipeable_string> on_get_password(const char *) override { return pw; }
  };

  const char *PW = "pw";
  const uint64_t HRG = 1000000000000ull;

  std::string path_of(const std::string &name) { return g_dir + "/" + name; }

  void wipe_wallet_files(const std::string &name)
  {
    for (const char *ext : {"", ".keys", ".address.txt"})
      boost::filesystem::remove(path_of(name) + ext);
  }

  // Create a BQ wallet the way `--bq-wallet` does: a fresh Ed25519 keypair AND a fresh
  // post-quantum root. Note recover=false — this is the CREATE path. On the restore path
  // (recover=true) wallet2 refuses a null root, which test (3) below exercises.
  bool create_bq_wallet(tools::wallet2 &w, const std::string &name)
  {
    wipe_wallet_files(name);
    w.set_seed_language("English");
    w.generate(path_of(name), PW, crypto::secret_key(), false /*create*/, false, false,
               true /*use_pq*/, nullptr /*draw a fresh root*/);
    return w.get_account().get_keys().m_account_address.is_pq() && w.is_deterministic();
  }

  // Restore from two word lists, exactly as `--electrum-seed ... --bq-seed ...` does.
  bool restore_bq_wallet(tools::wallet2 &w, const std::string &name,
                         const epee::wipeable_string &ed_words, const epee::wipeable_string &pq_words)
  {
    wipe_wallet_files(name);
    crypto::secret_key ed_key, pq_root;
    std::string lang;
    if (!crypto::ElectrumWords::words_to_bytes(ed_words, ed_key, lang)) { printf("FAIL: classic seed does not verify\n"); return false; }
    if (!crypto::ElectrumWords::words_to_bytes(pq_words, pq_root, lang)) { printf("FAIL: post-quantum seed does not verify\n"); return false; }
    w.set_seed_language("English");
    w.generate(path_of(name), PW, ed_key, true /*recover*/, false, false, true /*use_pq*/, &pq_root);
    return true;
  }

  bool seeds_of(const tools::wallet2 &w, epee::wipeable_string &ed_words, epee::wipeable_string &pq_words)
  {
    if (!w.get_seed(ed_words)) { printf("FAIL: no classic seed\n"); return false; }
    if (!w.get_pq_seed(pq_words)) { printf("FAIL: no post-quantum seed\n"); return false; }
    return true;
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

  bool build(const account_keys &sender, std::vector<tx_source_entry> sources,
             std::vector<tx_destination_entry> dests, transaction &tx)
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

// (1) Two seeds restore the whole wallet.
static bool test_restore_from_both_seeds()
{
  tools::wallet2 orig(MAINNET, 1, true);
  if (!create_bq_wallet(orig, "orig")) { printf("FAIL: BQ wallet creation\n"); return false; }

  epee::wipeable_string ed_words, pq_words;
  if (!seeds_of(orig, ed_words, pq_words)) return false;

  // The two seeds must be different word lists — the whole point is independent entropy.
  if (std::string(ed_words.data(), ed_words.size()) == std::string(pq_words.data(), pq_words.size()))
  { printf("FAIL: the post-quantum seed is the classic seed\n"); return false; }
  {
    std::vector<epee::wipeable_string> w;
    pq_words.split(w);
    if (w.size() != 25) { printf("FAIL: post-quantum seed is %zu words, expected 25\n", w.size()); return false; }
  }

  tools::wallet2 rest(MAINNET, 1, true);
  if (!restore_bq_wallet(rest, "rest", ed_words, pq_words)) return false;

  const account_keys &a = orig.get_account().get_keys();
  const account_keys &b = rest.get_account().get_keys();

  if (orig.get_address_as_str() != rest.get_address_as_str())
  { printf("FAIL: the classic B... address did not survive the restore\n"); return false; }
  const std::string bq_a = get_pq_address_str(a, MAINNET), bq_b = get_pq_address_str(b, MAINNET);
  if (bq_a.empty() || bq_a != bq_b)
  { printf("FAIL: the BQ address did not survive the restore\n"); return false; }
  if (!b.pq_root || memcmp(&*a.pq_root, &*b.pq_root, sizeof(crypto::secret_key)) != 0)
  { printf("FAIL: the restored post-quantum root differs\n"); return false; }
  if (memcmp(a.pq_keys->kyber_sk, b.pq_keys->kyber_sk, ML_KEM_768_SECRET_KEY_BYTES) != 0)
  { printf("FAIL: the restored ML-KEM-768 secret key differs\n"); return false; }
  if (memcmp(a.pq_dilithium->dilithium_sk, b.pq_dilithium->dilithium_sk, ML_DSA_65_SECRET_KEY_BYTES) != 0)
  { printf("FAIL: the restored ML-DSA-65 secret key differs\n"); return false; }
  // Subaddresses follow, because they are redrived from the root and from nothing else.
  for (const subaddress_index idx : {subaddress_index{0, 3}, subaddress_index{2, 11}})
  {
    pq_stealth_keys s1{}, s2{};
    if (!generate_pq_subaddress_keys(a, idx, s1) || !generate_pq_subaddress_keys(b, idx, s2))
    { printf("FAIL: BQ subaddress derivation (%u,%u)\n", idx.major, idx.minor); return false; }
    if (memcmp(s1.kyber_sk, s2.kyber_sk, ML_KEM_768_SECRET_KEY_BYTES) != 0)
    { printf("FAIL: BQ subaddress (%u,%u) differs after restore\n", idx.major, idx.minor); return false; }
  }

  printf("PASS: a BQ wallet issues two independent 25-word seeds; restoring from both reproduces\n"
         "      the B... address, the BQ address, the ML-KEM/ML-DSA keys and every BQ subaddress\n");
  return true;
}

// (2) audit CRIT-4 + (3) fail-closed restore.
static bool test_root_is_independent_and_required()
{
  tools::wallet2 orig(MAINNET, 1, true);
  if (!create_bq_wallet(orig, "indep")) { printf("FAIL: BQ wallet creation\n"); return false; }
  epee::wipeable_string ed_words, pq_words;
  if (!seeds_of(orig, ed_words, pq_words)) return false;

  // (2) Same classic seed, different post-quantum seed.
  tools::wallet2 other_root(MAINNET, 1, true);
  epee::wipeable_string other_pq_words;
  {
    const crypto::secret_key r = generate_pq_root_secret();
    if (!crypto::ElectrumWords::bytes_to_words(r, other_pq_words, "English"))
    { printf("FAIL: cannot render a second post-quantum seed\n"); return false; }
  }
  if (!restore_bq_wallet(other_root, "indep2", ed_words, other_pq_words)) return false;

  if (orig.get_address_as_str() != other_root.get_address_as_str())
  { printf("FAIL: the post-quantum seed changed the classic B... address\n"); return false; }
  if (get_pq_address_str(orig.get_account().get_keys(), MAINNET) ==
      get_pq_address_str(other_root.get_account().get_keys(), MAINNET))
  { printf("FAIL: the BQ address is unchanged by a different post-quantum root — it is still\n"
           "      derived from the Ed25519 spend key (CRIT-4 not fixed)\n"); return false; }

  // (3) Only the classic seed: refused, not silently downgraded and not re-rooted.
  {
    tools::wallet2 half(MAINNET, 1, true);
    wipe_wallet_files("half");
    crypto::secret_key ed_key; std::string lang;
    if (!crypto::ElectrumWords::words_to_bytes(ed_words, ed_key, lang)) { printf("FAIL: seed parse\n"); return false; }
    bool threw = false;
    try
    {
      half.generate(path_of("half"), PW, ed_key, true /*recover*/, false, false, true /*use_pq*/, nullptr);
    }
    catch (const std::exception &) { threw = true; }
    if (!threw)
    { printf("FAIL: restoring a BQ wallet without its post-quantum seed was allowed — it would have\n"
             "      minted a fresh root and stranded the funds behind a valid-looking BQ address\n"); return false; }
  }

  printf("PASS: the post-quantum root is independent of the classic seed (same B..., different BQ),\n"
         "      and a BQ restore without its post-quantum seed is refused rather than re-rooted\n");
  return true;
}

// (4) The restored wallet can actually see and spend the money. (5) and it survives a reload.
static bool test_restored_wallet_scans_and_spends()
{
  tools::wallet2 orig(MAINNET, 1, true);
  if (!create_bq_wallet(orig, "e2e")) { printf("FAIL: BQ wallet creation\n"); return false; }
  epee::wipeable_string ed_words, pq_words;
  if (!seeds_of(orig, ed_words, pq_words)) return false;

  const std::string s_primary = orig.get_pq_subaddress_as_str({0, 0});
  const std::string s_sub     = orig.get_pq_subaddress_as_str({0, 7});
  address_parse_info a0, a1;
  if (!parse(s_primary, a0) || !parse(s_sub, a1)) { printf("FAIL: BQ address parse\n"); return false; }

  // A classic wallet pays the BQ address and one BQ subaddress, with the real builder.
  account_base sender; sender.generate();
  std::vector<tx_destination_entry> dests = {
    tx_destination_entry(1 * HRG, a0.address, a0.is_subaddress),
    tx_destination_entry(2 * HRG, a1.address, a1.is_subaddress),
  };
  transaction tx;
  if (!build(sender.get_keys(), {owned_source(sender.get_keys(), 5 * HRG)}, dests, tx))
  { printf("FAIL: construct_tx refused the payment\n"); return false; }

  std::vector<tx_extra_field> fields;
  if (!parse_tx_extra(tx.extra, fields)) { printf("FAIL: tx_extra does not parse canonically\n"); return false; }

  // The wallet restored from BOTH seeds — it has never seen the original's key file.
  tools::wallet2 rest(MAINNET, 1, true);
  if (!restore_bq_wallet(rest, "e2e_rest", ed_words, pq_words)) return false;
  wallet_accessor_test::scan(rest, tx, 100);
  tools::wallet2::transfer_container t;
  rest.get_transfers(t);
  if (t.size() != 2) { printf("FAIL: the restored wallet found %zu of its 2 BQ outputs\n", t.size()); return false; }
  std::map<std::pair<uint32_t, uint32_t>, uint64_t> got;
  for (const auto &td : t) got[{td.m_subaddr_index.major, td.m_subaddr_index.minor}] = td.amount();
  const std::map<std::pair<uint32_t, uint32_t>, uint64_t> want = {{{0, 0}, 1 * HRG}, {{0, 7}, 2 * HRG}};
  if (got != want) { printf("FAIL: outputs landed on the wrong subaddresses or with wrong amounts\n"); return false; }

  // Spend authority: the recovered per-output secret must reproduce the on-chain binding tag.
  for (const auto &td : t)
  {
    kyber_shared_secret ss;
    if (!wallet_accessor_test::recover(rest, td, ss))
    { printf("FAIL: the restored wallet cannot recover the spend secret on (%u,%u)\n",
             td.m_subaddr_index.major, td.m_subaddr_index.minor); return false; }
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

  // The same classic seed with the wrong post-quantum seed sees nothing: the money really does
  // hang off the second seed, not off a label.
  {
    tools::wallet2 wrong(MAINNET, 1, true);
    epee::wipeable_string wrong_pq;
    const crypto::secret_key r = generate_pq_root_secret();
    if (!crypto::ElectrumWords::bytes_to_words(r, wrong_pq, "English")) { printf("FAIL: seed render\n"); return false; }
    if (!restore_bq_wallet(wrong, "e2e_wrong", ed_words, wrong_pq)) return false;
    wallet_accessor_test::scan(wrong, tx, 100);
    tools::wallet2::transfer_container wt;
    wrong.get_transfers(wt);
    if (!wt.empty())
    { printf("FAIL: a wallet restored with the wrong post-quantum seed found %zu output(s)\n", wt.size()); return false; }
  }

  // (5) store + reload: the root is persisted (encrypted) and the BQ address is stable.
  const std::string bq_before = get_pq_address_str(rest.get_account().get_keys(), MAINNET);
  rest.store();
  {
    tools::wallet2 reloaded(MAINNET, 1, true);
    reloaded.load(path_of("e2e_rest"), PW);
    const account_keys &k = reloaded.get_account().get_keys();
    if (!k.pq_root) { printf("FAIL: the post-quantum root was not persisted\n"); return false; }
    if (memcmp(&*k.pq_root, &*rest.get_account().get_keys().pq_root, sizeof(crypto::secret_key)) != 0)
    { printf("FAIL: the reloaded post-quantum root differs\n"); return false; }
    if (get_pq_address_str(k, MAINNET) != bq_before)
    { printf("FAIL: the reloaded BQ address differs\n"); return false; }
  }

  printf("PASS: a wallet restored from the two seeds finds the real payment on both its BQ\n"
         "      addresses and recovers the spend secret of each; the wrong post-quantum seed\n"
         "      finds nothing; the root survives store/reload\n");
  return true;
}

// (6) The seed must never be rendered from an encrypted root.
static bool test_seed_refused_while_locked()
{
  // Not "unattended": this wallet encrypts its keys in memory, as a real one does by default.
  pw_callback cb(PW);
  tools::wallet2 w(MAINNET);
  w.callback(&cb);
  wipe_wallet_files("locked");
  w.set_seed_language("English");
  w.generate(path_of("locked"), PW, crypto::secret_key(), false, false, false, true /*use_pq*/, nullptr);
  const std::string bq = w.get_pq_subaddress_as_str({0, 0});

  // setup_keys has already encrypted the keys in memory (AskPasswordToDecrypt is the
  // default), so even the classic seed needs an unlocker here.
  epee::wipeable_string ed_words, pq_words;
  {
    epee::wipeable_string pw(PW);
    tools::wallet_keys_unlocker u(w, &pw);
    if (!w.get_seed(ed_words)) { printf("FAIL: no classic seed\n"); return false; }
  }

  // Locked (the state the wallet is in right now): refuse, do not render.
  bool threw = false;
  try { w.get_pq_seed(pq_words); }
  catch (const tools::error::password_needed &) { threw = true; }
  if (!threw)
  { printf("FAIL: get_pq_seed rendered a seed from the encrypted root — those words restore\n"
           "      a different wallet, silently\n"); return false; }

  // Unlocked: render, and the words must really restore the same BQ address.
  {
    epee::wipeable_string pw(PW);
    tools::wallet_keys_unlocker u(w, &pw);
    if (!w.get_pq_seed(pq_words)) { printf("FAIL: get_pq_seed under an unlocker\n"); return false; }
  }
  tools::wallet2 back(MAINNET, 1, true);
  if (!restore_bq_wallet(back, "locked_back", ed_words, pq_words)) return false;
  if (get_pq_address_str(back.get_account().get_keys(), MAINNET) != bq)
  { printf("FAIL: the seed shown by an unlocked wallet does not restore its BQ address\n"); return false; }

  printf("PASS: get_pq_seed refuses while the keys are encrypted in memory, and the words it\n"
         "      returns once unlocked really restore the same BQ address\n");
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
  ok &= test_restore_from_both_seeds();
  ok &= test_root_is_independent_and_required();
  ok &= test_restored_wallet_scans_and_spends();
  ok &= test_seed_refused_while_locked();
  printf("\nRESULT: %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
