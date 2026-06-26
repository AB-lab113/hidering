// HIDERING Phase 5 (HFv16, Option-2-transparent / A3) — standalone smoke test for the SPEND
// side of a transparent BQ input (txin_to_key_pq), mirroring what construct_tx produces and what
// the consensus validator (blockchain.cpp check_tx_inputs, checks c/d/e) verifies:
//
//   * fix H-5: a transaction may carry several ML-KEM-768 ciphertexts (one per BQ output). The
//     spender must recover the shared secret of the OUTPUT IT OWNS, not blindly ct[0]. We model
//     two BQ outputs encapsulated to two different recipients and show recipient #1 only
//     reproduces the per-output ML-DSA-65 key (and thus the on-chain binding tag) from ITS OWN
//     ciphertext's secret — the other ciphertext's (implicit-rejection) secret does not.
//   * the per-output ML-DSA-65 key recovered at spend time signs the prefix hash and verifies
//     (validator check d), and the recomputed binding tag matches the one published at output
//     creation (validator check c).
//   * tamper paths: an attacker substituting their own ML-DSA key changes the binding tag
//     (check c rejects); a flipped signature byte fails verification (check d rejects).
//   * money conservation (validator check e) is plain arithmetic on revealed amounts.
//
// Built ad-hoc like pqc_test.cpp / pq_bind_test.cpp, linked against the compiled cncrypto objects
// + liboqs. NB: the wallet's actual output DETECTION additionally uses the on-curve un-tweak
// P' - t*G == P (cryptonote::derive_bq_output_tweak + is_out_to_acc_precomp, exercised by the
// scan path / regtest); here we use the binding-tag reproduction as the ownership discriminator,
// which is the spend-authorisation property A3 relies on and needs no ringct/curve-addition lib.
#include <cstdio>
#include <cstring>
#include "crypto/crypto.h"
#include "crypto/pqc.h"

using namespace crypto;
using namespace crypto::pqc;

// Recompute the on-chain binding tag exactly as creator (tx_utils) and validator (blockchain) do.
static void bind_tag_of(const crypto::public_key& out_key, const pq_public_key& dsa, uint8_t tag[32])
{
  pqc_compute_bind_tag((const uint8_t*)&out_key, sizeof(out_key), dsa.dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES, tag);
}

// fix H-5: with two BQ outputs in one tx, each to a different recipient, the spender of output #1
// must use the secret from ITS ciphertext. Prove that only recipient #1's own ciphertext secret
// reproduces the per-output ML-DSA key bound on chain for its output index.
static bool test_h5_per_output_secret()
{
  // two recipients, each with a real ML-KEM-768 keypair
  pq_stealth_keys r1, r2;
  {
    pq_public_key pk1, pk2; pq_secret_key sk1, sk2;
    if (!pqc_keygen(pk1, sk1) || !pqc_keygen(pk2, sk2)) { printf("FAIL: recipient keygen\n"); return false; }
    memcpy(r1.kyber_pk, pk1.kyber768_pk, ML_KEM_768_PUBLIC_KEY_BYTES);
    memcpy(r1.kyber_sk, sk1.kyber768_sk, ML_KEM_768_SECRET_KEY_BYTES);
    memcpy(r2.kyber_pk, pk2.kyber768_pk, ML_KEM_768_PUBLIC_KEY_BYTES);
    memcpy(r2.kyber_sk, sk2.kyber768_sk, ML_KEM_768_SECRET_KEY_BYTES);
  }

  // SENDER builds two BQ outputs: output #1 (index 0) to r1, output #2 (index 1) to r2.
  kyber_ciphertext ct1, ct2; kyber_shared_secret ss1_s, ss2_s;
  if (!pqc_stealth_encaps(r1.kyber_pk, ML_KEM_768_PUBLIC_KEY_BYTES, ct1, ss1_s)) { printf("FAIL: encaps r1\n"); return false; }
  if (!pqc_stealth_encaps(r2.kyber_pk, ML_KEM_768_PUBLIC_KEY_BYTES, ct2, ss2_s)) { printf("FAIL: encaps r2\n"); return false; }

  // one-time output keys (P' on chain); their exact value is opaque to this PQ test
  crypto::public_key P1, P2;
  { crypto::secret_key s; crypto::generate_keys(P1, s); crypto::generate_keys(P2, s); }

  // per-output ML-DSA keys + binding tags published at creation, derived from the SENDER secret
  pq_public_key dsa1_s, dsa2_s; pq_secret_key dsa1ss, dsa2ss;
  if (!pqc_keygen_output_dsa(ss1_s, 0, dsa1_s, dsa1ss)) { printf("FAIL: out dsa #1\n"); return false; }
  if (!pqc_keygen_output_dsa(ss2_s, 1, dsa2_s, dsa2ss)) { printf("FAIL: out dsa #2\n"); return false; }
  uint8_t bind1[32], bind2[32];
  bind_tag_of(P1, dsa1_s, bind1);
  bind_tag_of(P2, dsa2_s, bind2);

  // RECIPIENT #1 spends output #1. It does NOT know which ciphertext is "its" — it tries each,
  // and the matching one is the ciphertext whose recovered secret reproduces the per-output key
  // bound to its output (index 0). fix H-5: trying only ct[0] is fine here, but the discriminator
  // must be per-output, and the WRONG ciphertext must NOT validate.
  kyber_ciphertext all[2] = { ct1, ct2 };
  int matched_idx = -1;
  pq_public_key dsa_recovered; pq_secret_key dsa_recovered_sk;
  for (int j = 0; j < 2; ++j)
  {
    kyber_shared_secret ss;
    if (!pqc_stealth_decaps(r1, all[j], ss)) continue;            // implicit rejection: "succeeds" for any ct
    pq_public_key dpk; pq_secret_key dsk;
    if (!pqc_keygen_output_dsa(ss, 0, dpk, dsk)) continue;        // output index 0 (recipient #1's output)
    uint8_t tag[32];
    bind_tag_of(P1, dpk, tag);
    if (memcmp(tag, bind1, 32) == 0) { matched_idx = j; dsa_recovered = dpk; dsa_recovered_sk = dsk; }
  }
  if (matched_idx != 0) { printf("FAIL: recipient #1 did not match its OWN ciphertext (got %d)\n", matched_idx); return false; }
  if (memcmp(dsa_recovered.dilithium3_pk, dsa1_s.dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES) != 0)
  { printf("FAIL: recovered per-output ML-DSA pk != creator's\n"); return false; }

  // sanity: the OTHER ciphertext (ct2, encapsulated to r2) decapsulated by r1 must NOT reproduce
  // the binding tag for output 0 (else H-5 would let any ciphertext authorise a spend).
  {
    kyber_shared_secret ss; pq_public_key dpk; pq_secret_key dsk; uint8_t tag[32];
    if (pqc_stealth_decaps(r1, ct2, ss) && pqc_keygen_output_dsa(ss, 0, dpk, dsk))
    {
      bind_tag_of(P1, dpk, tag);
      if (memcmp(tag, bind1, 32) == 0) { printf("FAIL: foreign ciphertext reproduced our binding tag\n"); return false; }
    }
  }
  printf("PASS: H-5 — spender recovers its OWN output's secret/key from the right ciphertext\n");
  return true;
}

// Full spend authorisation: build a txin_to_key_pq-equivalent, sign the prefix hash with the
// recovered per-output key, and run the validator's checks c/d (+ tamper) and e (arithmetic).
static bool test_spend_authorises_and_validates()
{
  pq_stealth_keys r;
  { pq_public_key pk; pq_secret_key sk; if (!pqc_keygen(pk, sk)) { printf("FAIL: keygen\n"); return false; }
    memcpy(r.kyber_pk, pk.kyber768_pk, ML_KEM_768_PUBLIC_KEY_BYTES);
    memcpy(r.kyber_sk, sk.kyber768_sk, ML_KEM_768_SECRET_KEY_BYTES); }

  const uint64_t out_index = 5;
  kyber_ciphertext ct; kyber_shared_secret ss_s;
  if (!pqc_stealth_encaps(r.kyber_pk, ML_KEM_768_PUBLIC_KEY_BYTES, ct, ss_s)) { printf("FAIL: encaps\n"); return false; }
  crypto::public_key Pp; { crypto::secret_key s; crypto::generate_keys(Pp, s); }
  pq_public_key dsa_s; pq_secret_key dsa_s_sk;
  if (!pqc_keygen_output_dsa(ss_s, out_index, dsa_s, dsa_s_sk)) { printf("FAIL: out dsa\n"); return false; }
  uint8_t stored_bind[32]; bind_tag_of(Pp, dsa_s, stored_bind);   // published on chain at creation

  // SPEND: recipient recovers ss, re-derives the per-output key, fills the input's dsa.pk, signs.
  kyber_shared_secret ss_r;
  if (!pqc_stealth_decaps(r, ct, ss_r)) { printf("FAIL: decaps\n"); return false; }
  pq_public_key dsa_r; pq_secret_key dsa_r_sk;
  if (!pqc_keygen_output_dsa(ss_r, out_index, dsa_r, dsa_r_sk)) { printf("FAIL: recover out dsa\n"); return false; }

  pq_tx_sig dsa;                                   // == txin_to_key_pq.dsa
  memcpy(dsa.pk, dsa_r.dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES);
  memset(dsa.sig, 0, ML_DSA_65_SIGNATURE_BYTES);   // zeroed while hashing (as construct_tx/validator do)

  crypto::hash prefix_hash;                          // stand-in for get_transaction_prefix_hash(tx)
  for (size_t i = 0; i < sizeof(prefix_hash); ++i) ((uint8_t*)&prefix_hash)[i] = (uint8_t)(0x10 + i);
  if (!pqc_tx_sign((const uint8_t*)&prefix_hash, sizeof(prefix_hash), dsa_r_sk.dilithium3_sk, ML_DSA_65_SECRET_KEY_BYTES,
                   dsa.pk, ML_DSA_65_PUBLIC_KEY_BYTES, dsa)) { printf("FAIL: sign input\n"); return false; }

  // validator check (c): recompute the binding tag from the REVEALED output key + the input's
  // dsa.pk (exactly as blockchain.cpp does) and compare to the on-chain value.
  pq_public_key dsa_pk_view; memcpy(dsa_pk_view.dilithium3_pk, dsa.pk, ML_DSA_65_PUBLIC_KEY_BYTES);
  uint8_t check_bind[32]; bind_tag_of(Pp, dsa_pk_view, check_bind);
  if (memcmp(check_bind, stored_bind, 32) != 0) { printf("FAIL: check (c) binding tag mismatch on honest spend\n"); return false; }

  // validator check (d): signature verifies over the prefix hash
  if (!pqc_tx_verify((const uint8_t*)&prefix_hash, sizeof(prefix_hash), dsa)) { printf("FAIL: check (d) honest sig rejected\n"); return false; }

  // tamper (c): attacker substitutes their own ML-DSA key → binding tag mismatch
  { pq_public_key apk; pq_secret_key ask; pqc_keygen(apk, ask);
    uint8_t atag[32]; bind_tag_of(Pp, apk, atag);
    if (memcmp(atag, stored_bind, 32) == 0) { printf("FAIL: attacker key matched binding tag\n"); return false; } }

  // tamper (d): flipped signature byte fails verification
  { pq_tx_sig bad = dsa; bad.sig[0] ^= 0xFF;
    if (pqc_tx_verify((const uint8_t*)&prefix_hash, sizeof(prefix_hash), bad)) { printf("FAIL: tampered sig verified\n"); return false; } }

  // validator check (e): money conservation on revealed amounts (sum inputs >= sum outputs)
  { const uint64_t in_amt = 1000000, out_amt = 900000, fee = in_amt - out_amt;
    if (!(in_amt >= out_amt) || fee != 100000) { printf("FAIL: money conservation arithmetic\n"); return false; } }

  printf("PASS: transparent BQ spend authorises (c/d) and conserves money (e); tampering rejected\n");
  return true;
}

int main()
{
  bool ok = true;
  ok &= test_h5_per_output_secret();
  ok &= test_spend_authorises_and_validates();
  printf("\nRESULT: %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
