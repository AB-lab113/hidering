// HIDERING Phase 5 (HFv16) — cold signing a BQ spend: unsigned_tx_set v4 side table.
//
// Standalone; links libwallet because it drives the REAL wallet2 code paths
// (dump_tx_to_str / parse_unsigned_tx_from_str / restore_pq_construction_data) rather than
// a reimplementation of them.
//
// WHAT IS BEING PROVEN.
//
//  1. A BQ spend survives the trip to an offline signer. tx_source_entry and
//     tx_destination_entry do not serialise their PQ fields — addr goes through
//     account_public_address's binary serialiser, which omits pq_kyber_pk so classic B...
//     addresses stay byte-identical on the wire — so before v4 a BQ spend arrived at the
//     cold machine looking like a classic ring spend to a classic recipient.
//
//  2. THE FILE CARRIES NO SPEND AUTHORITY. What travels is the ML-KEM-768 CIPHERTEXT, never
//     the shared secret it decapsulates to. The cold machine recomputes the secret with its
//     own key. The test greps the serialised bytes for pq_ss and for the wallet's private
//     keys and requires them absent — the property that makes offline signing worth doing.
//
//  3. Classic cold signing is untouched: a set with no PQ material still serialises with
//     version varint 3 and the \005 file prefix, i.e. the same bytes as before v4 existed.
//
//  4. An older wallet cannot silently mis-sign a BQ set. It would read the struct version
//     varint, handle the fields it knows and ignore the trailing pq_data — so the guard is
//     the FILE version byte, which such a wallet rejects outright. We check that the parser
//     rejects a file version it does not know, which is that mechanism.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "crypto/crypto.h"
#include "crypto/pqc.h"
#include "cryptonote_basic/account.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "wallet/wallet2.h"

using namespace cryptonote;
using namespace crypto::pqc;

namespace
{
  // An in-memory wallet (empty path => never written to disk) with a fixed seed, so the
  // "hot" exporter and the "cold" signer are the same account on two simulated machines.
  void make_wallet(tools::wallet2 &w, uint8_t seed_byte, bool with_bq)
  {
    crypto::secret_key rec;
    for (int i = 0; i < 32; ++i) ((uint8_t *)rec.data)[i] = (uint8_t)(seed_byte + i);
    sc_reduce32((uint8_t *)rec.data);
    w.generate("", "", rec, true /*recover*/, false /*two_random*/, false /*create_address_file*/);
    if (with_bq)
      generate_pq_keys(w.get_account().get_keys_nonconst());
  }

  bool contains(const std::string &haystack, const void *needle, size_t n)
  {
    if (n == 0 || haystack.size() < n) return false;
    return haystack.find(std::string((const char *)needle, n)) != std::string::npos;
  }

  // Build a destination paying `addr`. A BQ address keeps its ML-KEM key in memory here; the
  // whole point of the test is that the serialiser drops it and the side table restores it.
  tx_destination_entry dst_for(uint64_t amount, const account_public_address &addr)
  {
    tx_destination_entry d(amount, addr, false);
    d.is_pq = addr.is_pq();
    return d;
  }
}

// (1)+(2): a BQ set round-trips its PQ material, and carries no secret.
static bool test_bq_roundtrip_and_no_secrets()
{
  tools::wallet2 hot(MAINNET), cold(MAINNET);
  make_wallet(hot, 0x21, true);
  make_wallet(cold, 0x21, true);   // same seed => same account, on the offline machine
  const account_keys &ck = cold.get_account().get_keys();
  if (!ck.pq_keys) { printf("FAIL: cold wallet has no ML-KEM-768 key (test setup)\n"); return false; }

  // A separate BQ recipient we are paying.
  account_base recip; recip.generate();
  if (!generate_pq_keys(recip.get_keys_nonconst())) { printf("FAIL: recipient keygen\n"); return false; }

  // The hot wallet found a BQ output of ours: encapsulate to our own key to get the
  // (ciphertext, secret) pair the scanner would have recovered.
  kyber_ciphertext ct; kyber_shared_secret ss_hot;
  if (!pqc_stealth_encaps(ck.pq_keys->kyber_pk, ML_KEM_768_PUBLIC_KEY_BYTES, ct, ss_hot))
  { printf("FAIL: encaps\n"); return false; }

  // Assemble the pending tx exactly as the spend path would.
  tools::wallet2::pending_tx ptx{};
  tools::wallet2::tx_construction_data &cd = ptx.construction_data;
  cryptonote::tx_source_entry src{};
  src.amount = 5000;
  src.rct = true;
  src.real_output = 0;
  src.real_output_in_tx_index = 3;
  src.push_output(0, crypto::public_key{}, src.amount);
  src.is_pq = true;
  src.pq_ss = ss_hot;   // present in memory...
  src.pq_ct = ct;       // ...but only THIS is allowed to travel
  cd.sources.push_back(src);

  cd.splitted_dsts.push_back(dst_for(3000, recip.get_keys().m_account_address));            // BQ payee
  cd.splitted_dsts.push_back(dst_for(1500, cold.get_account().get_keys().m_account_address)); // BQ change
  cd.dests.push_back(cd.splitted_dsts[0]);
  cd.change_dts = cd.splitted_dsts[1];
  cd.use_rct = true;
  ptx.change_dts = cd.change_dts;

  std::vector<tools::wallet2::pending_tx> ptxs{ptx};
  const std::string blob = hot.dump_tx_to_str(ptxs);
  if (blob.empty()) { printf("FAIL: dump_tx_to_str produced nothing\n"); return false; }

  // ---- (2) NO SECRET MAY APPEAR IN THE FILE -------------------------------------------
  // The blob is encrypted with the view key, so search the PLAINTEXT the exporter built:
  // parse it back and re-serialise, then scan those bytes. (Searching the ciphertext would
  // pass trivially and prove nothing.)
  tools::wallet2::unsigned_tx_set parsed{};
  if (!cold.parse_unsigned_tx_from_str(blob, parsed))
  { printf("FAIL: the cold wallet could not parse the exported set\n"); return false; }

  std::ostringstream oss;
  binary_archive<true> ar(oss);
  if (!::serialization::serialize(ar, parsed)) { printf("FAIL: re-serialize\n"); return false; }
  const std::string plain = oss.str();

  bool ok = true;
  if (contains(plain, ss_hot.ss, sizeof(ss_hot.ss)))
  { printf("FAIL: the ML-KEM-768 SHARED SECRET (pq_ss) is present in the unsigned tx set —\n"
           "      the transfer file would carry spend authority for the BQ output\n"); ok = false; }
  if (contains(plain, ck.pq_keys->kyber_sk, ML_KEM_768_SECRET_KEY_BYTES))
  { printf("FAIL: the ML-KEM-768 SECRET KEY is present in the unsigned tx set\n"); ok = false; }
  if (ck.pq_dilithium && contains(plain, ck.pq_dilithium->dilithium_sk, ML_DSA_65_SECRET_KEY_BYTES))
  { printf("FAIL: the ML-DSA-65 SECRET KEY is present in the unsigned tx set\n"); ok = false; }
  if (contains(plain, ck.m_spend_secret_key.data, sizeof(ck.m_spend_secret_key)))
  { printf("FAIL: the Ed25519 SPEND SECRET KEY is present in the unsigned tx set\n"); ok = false; }
  if (contains(plain, ck.m_view_secret_key.data, sizeof(ck.m_view_secret_key)))
  { printf("FAIL: the Ed25519 VIEW SECRET KEY is present in the unsigned tx set\n"); ok = false; }
  // ...and the ciphertext, which IS meant to travel, must actually be there.
  if (!contains(plain, ct.ct, ML_KEM_768_CIPHERTEXT_BYTES))
  { printf("FAIL: the ML-KEM-768 ciphertext did not make it into the unsigned tx set\n"); ok = false; }
  if (!ok) return false;

  // POSITIVE CONTROL for the check above. "Absent" is only meaningful if the search would
  // have found the secret had it been there, so deliberately leak pq_ss into a set, serialise
  // it, and require the same search to catch it. Without this, the five checks above could be
  // passing because the search is broken rather than because the file is clean.
  {
    tools::wallet2::unsigned_tx_set leaky = parsed;
    leaky.pq_data[0].change_kem_pk.assign((const char *)ss_hot.ss, sizeof(ss_hot.ss));
    std::ostringstream loss; binary_archive<true> lar(loss);
    if (!::serialization::serialize(lar, leaky)) { printf("FAIL: control re-serialize\n"); return false; }
    if (!contains(loss.str(), ss_hot.ss, sizeof(ss_hot.ss)))
    { printf("FAIL: control — a deliberately leaked pq_ss was NOT detected, so the\n"
             "      \"no secrets\" result above proves nothing\n"); return false; }
  }

  printf("PASS: the unsigned tx set carries the ML-KEM-768 ciphertext and NO secret\n"
         "      (checked: pq_ss, ML-KEM sk, ML-DSA sk, spend sk, view sk; a deliberately\n"
         "      leaked pq_ss IS caught by the same search, so the absence is meaningful)\n");

  // ---- the format really did strip the in-struct PQ fields ----------------------------
  if (parsed.txes.size() != 1) { printf("FAIL: expected 1 tx\n"); return false; }
  if (parsed.txes[0].sources[0].is_pq || parsed.txes[0].sources[0].pq_ss)
  { printf("FAIL: tx_source_entry unexpectedly serialised its PQ fields\n"); return false; }
  if (parsed.txes[0].splitted_dsts[0].addr.is_pq())
  { printf("FAIL: tx_destination_entry unexpectedly serialised the ML-KEM key\n"); return false; }
  if (parsed.pq_data.size() != 1 || parsed.pq_data[0].source_cts.size() != 1
      || parsed.pq_data[0].dest_kem_pks.size() != 2 || parsed.pq_data[0].change_kem_pk.empty())
  { printf("FAIL: the v4 side table did not survive the round-trip\n"); return false; }

  // ---- (1) the cold machine rebuilds everything from public data + its own keys --------
  cold.restore_pq_construction_data(parsed);
  const auto &rsrc = parsed.txes[0].sources[0];
  if (!rsrc.is_pq) { printf("FAIL: the restored source is not flagged is_pq\n"); return false; }
  if (!rsrc.pq_ss) { printf("FAIL: the cold machine did not recompute the shared secret\n"); return false; }
  if (memcmp(rsrc.pq_ss->ss, ss_hot.ss, ML_KEM_768_SHARED_SECRET_BYTES) != 0)
  { printf("FAIL: the cold machine derived a DIFFERENT shared secret — the spend would be invalid\n"); return false; }

  if (!parsed.txes[0].splitted_dsts[0].addr.is_pq() || !parsed.txes[0].splitted_dsts[1].addr.is_pq())
  { printf("FAIL: BQ destinations were not restored\n"); return false; }
  if (memcmp(parsed.txes[0].splitted_dsts[0].addr.pq_kyber_pk->data(),
             recip.get_keys().m_account_address.pq_kyber_pk->data(), ML_KEM_768_PUBLIC_KEY_BYTES) != 0)
  { printf("FAIL: the restored payee ML-KEM key is wrong\n"); return false; }
  if (!parsed.txes[0].change_dts.addr.is_pq())
  { printf("FAIL: the BQ change address was not restored\n"); return false; }
  printf("PASS: the offline signer recomputes the shared secret from the ciphertext with its own\n"
         "      key, and both BQ destinations plus the BQ change address come back intact\n");

  // ---- the restored material actually authorises the spend (validator checks c and d) ---
  pq_public_key opk; pq_secret_key osk;
  if (!pqc_keygen_output_dsa(*rsrc.pq_ss, rsrc.real_output_in_tx_index, opk, osk))
  { printf("FAIL: per-output ML-DSA-65 derivation from the restored secret\n"); return false; }
  pq_public_key opk_hot; pq_secret_key osk_hot;
  if (!pqc_keygen_output_dsa(ss_hot, src.real_output_in_tx_index, opk_hot, osk_hot))
  { printf("FAIL: per-output ML-DSA-65 derivation (reference)\n"); return false; }
  if (memcmp(opk.dilithium3_pk, opk_hot.dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES) != 0)
  { printf("FAIL: the cold machine derived a different per-output ML-DSA key (check c would reject)\n"); return false; }

  crypto::public_key P;
  for (int i = 0; i < 32; ++i) ((uint8_t *)P.data)[i] = (uint8_t)(0x44 + i);
  uint8_t tag_cold[32], tag_chain[32];
  pqc_compute_bind_tag((const uint8_t *)&P, 32, opk.dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES, tag_cold);
  pqc_compute_bind_tag((const uint8_t *)&P, 32, opk_hot.dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES, tag_chain);
  if (memcmp(tag_cold, tag_chain, 32) != 0)
  { printf("FAIL: binding tag mismatch (validator check c)\n"); return false; }

  uint8_t prefix_hash[32];
  for (int i = 0; i < 32; ++i) prefix_hash[i] = (uint8_t)(0x77 ^ i);
  pq_tx_sig sig;
  if (!pqc_tx_sign(prefix_hash, sizeof(prefix_hash), osk.dilithium3_sk, ML_DSA_65_SECRET_KEY_BYTES,
                   opk.dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES, sig))
  { printf("FAIL: the cold machine could not sign with the restored per-output key\n"); return false; }
  if (!pqc_tx_verify(prefix_hash, sizeof(prefix_hash), sig))
  { printf("FAIL: the cold-produced signature does not verify (validator check d)\n"); return false; }
  printf("PASS: the cold-restored material reproduces the on-chain binding tag (check c) and\n"
         "      produces a signature that verifies (check d) — the spend is authorised offline\n");
  return true;
}

// (3): classic cold signing must be byte-for-byte what it was before v4 existed.
static bool test_classic_set_is_unchanged()
{
  tools::wallet2 hot(MAINNET);
  make_wallet(hot, 0x33, false);   // classic B... wallet, no PQ material anywhere

  account_base payee; payee.generate();

  tools::wallet2::pending_tx ptx{};
  tools::wallet2::tx_construction_data &cd = ptx.construction_data;
  cryptonote::tx_source_entry src{};
  src.amount = 900; src.rct = true; src.real_output = 0; src.real_output_in_tx_index = 0;
  src.push_output(0, crypto::public_key{}, src.amount);
  cd.sources.push_back(src);
  cd.splitted_dsts.push_back(dst_for(900, payee.get_keys().m_account_address));
  cd.dests.push_back(cd.splitted_dsts[0]);
  cd.change_dts = cd.splitted_dsts[0];
  cd.use_rct = true;

  std::vector<tools::wallet2::pending_tx> ptxs{ptx};
  const std::string blob = hot.dump_tx_to_str(ptxs);
  if (blob.empty()) { printf("FAIL: classic dump_tx_to_str produced nothing\n"); return false; }

  // File version byte must still be \005: that is what lets wallets built before v4 keep
  // reading classic sets.
  const std::string magic = "Monero unsigned tx set";
  if (blob.compare(0, magic.size(), magic) != 0)
  { printf("FAIL: classic set lost its magic\n"); return false; }
  if (blob[magic.size()] != '\005')
  { printf("FAIL: classic set file version changed to \\%03o — older wallets would refuse it\n", blob[magic.size()]); return false; }

  tools::wallet2::unsigned_tx_set parsed{};
  if (!hot.parse_unsigned_tx_from_str(blob, parsed)) { printf("FAIL: classic set does not parse\n"); return false; }
  if (parsed.has_pq_data()) { printf("FAIL: classic set reported post-quantum data\n"); return false; }
  if (!parsed.pq_data.empty() && !parsed.pq_data[0].empty())
  { printf("FAIL: classic set carries a non-empty side table\n"); return false; }

  // The version varint the classic set serialises with must still be 3 — a bump to 4 would
  // rewrite the bytes of every classic transfer.
  std::ostringstream oss; binary_archive<true> ar(oss);
  if (!::serialization::serialize(ar, parsed)) { printf("FAIL: classic re-serialize\n"); return false; }
  if (oss.str().empty() || (uint8_t)oss.str()[0] != 3)
  { printf("FAIL: classic set serialises with version %u, expected 3\n", oss.str().empty() ? 0u : (unsigned)(uint8_t)oss.str()[0]); return false; }

  printf("PASS: a classic transfer still serialises as v3 under the \\005 prefix — B... cold\n"
         "      signing is byte-identical and older wallets keep reading it\n");
  return true;
}

// (4): the mechanism that stops an older wallet mis-signing a BQ set — an unknown file
// version byte is refused rather than parsed leniently.
static bool test_unknown_file_version_is_refused()
{
  tools::wallet2 w(MAINNET);
  make_wallet(w, 0x55, true);

  tools::wallet2::pending_tx ptx{};
  ptx.construction_data.use_rct = true;
  cryptonote::tx_source_entry src{};
  src.amount = 10; src.rct = true; src.real_output = 0;
  src.push_output(0, crypto::public_key{}, src.amount);
  ptx.construction_data.sources.push_back(src);
  account_base payee; payee.generate();
  ptx.construction_data.splitted_dsts.push_back(dst_for(10, payee.get_keys().m_account_address));
  ptx.construction_data.change_dts = ptx.construction_data.splitted_dsts[0];

  std::vector<tools::wallet2::pending_tx> ptxs{ptx};
  std::string blob = w.dump_tx_to_str(ptxs);
  if (blob.empty()) { printf("FAIL: dump\n"); return false; }

  const size_t vpos = strlen("Monero unsigned tx set");
  blob[vpos] = '\007';   // a version no released wallet knows
  tools::wallet2::unsigned_tx_set parsed{};
  if (w.parse_unsigned_tx_from_str(blob, parsed))
  { printf("FAIL: a file with an unknown version byte was accepted — an older wallet could\n"
           "      likewise accept a v4 set and sign a BQ spend as a classic one\n"); return false; }

  printf("PASS: an unknown file version byte is refused — that is the guard that makes a\n"
         "      pre-v4 wallet reject a \\006 BQ set instead of mis-signing it\n");
  return true;
}

int main()
{
  bool ok = true;
  ok &= test_classic_set_is_unchanged();
  ok &= test_bq_roundtrip_and_no_secrets();
  ok &= test_unknown_file_version_is_refused();
  printf("\nRESULT: %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
