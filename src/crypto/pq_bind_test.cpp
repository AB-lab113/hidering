// HIDERING Phase 5 (HFv16, Option-2-transparent / A1 / A2) — standalone smoke test for the
// post-quantum BINDING primitives that authorise a transparent BQ spend:
//   * pqc_keygen_output_dsa(): per-output ML-DSA-65 key derived from (ss, output_index) is
//     DETERMINISTIC (sender at output-creation and receiver at spend derive the same pair),
//   * pqc_compute_bind_tag(): the binding tag is a pure function of (real_output_key, dsa_pk),
//     so the validator (which recomputes it from the revealed key + supplied pk) matches the
//     value the creator published — and a tampered dsa_pk yields a different tag (theft fails),
//   * the per-output ML-DSA key actually signs/verifies (the (d) check of check_tx_inputs).
//
// Built ad-hoc like pqc_test.cpp, linked against the already-compiled cncrypto objects + liboqs.
#include <cstdio>
#include <cstring>
#include "crypto/crypto.h"
#include "crypto/pqc.h"

using namespace crypto;
using namespace crypto::pqc;

static kyber_shared_secret make_ss(uint8_t seed)
{
  kyber_shared_secret ss;
  for (size_t i = 0; i < sizeof(ss.ss); ++i) ss.ss[i] = (uint8_t)(seed + i);
  return ss;
}

static bool test_output_dsa_deterministic()
{
  kyber_shared_secret ss = make_ss(7);
  pq_public_key pk1, pk2, pk_other; pq_secret_key sk1, sk2, sk_other;
  if (!pqc_keygen_output_dsa(ss, 3, pk1, sk1)) { printf("FAIL: keygen_output_dsa #1\n"); return false; }
  if (!pqc_keygen_output_dsa(ss, 3, pk2, sk2)) { printf("FAIL: keygen_output_dsa #2\n"); return false; }
  if (memcmp(pk1.dilithium3_pk, pk2.dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES) != 0) { printf("FAIL: same (ss,idx) gave different pk\n"); return false; }
  // a different output index must give a different key (per-output binding)
  if (!pqc_keygen_output_dsa(ss, 4, pk_other, sk_other)) { printf("FAIL: keygen_output_dsa #3\n"); return false; }
  if (memcmp(pk1.dilithium3_pk, pk_other.dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES) == 0) { printf("FAIL: distinct index gave identical pk\n"); return false; }
  printf("PASS: per-output ML-DSA-65 key is deterministic in (ss,index) and index-separated\n");
  return true;
}

static bool test_bind_tag_matches_and_detects_tamper()
{
  kyber_shared_secret ss = make_ss(42);
  pq_public_key pk; pq_secret_key sk;
  if (!pqc_keygen_output_dsa(ss, 1, pk, sk)) { printf("FAIL: keygen\n"); return false; }

  crypto::public_key out_key;
  for (size_t i = 0; i < sizeof(out_key); ++i) ((uint8_t*)&out_key)[i] = (uint8_t)(0xA0 + i);

  // creator side (cryptonote_tx_utils.cpp) vs validator side (blockchain.cpp): same inputs → same tag
  uint8_t tag_creator[32], tag_validator[32];
  pqc_compute_bind_tag((const uint8_t*)&out_key, sizeof(out_key), pk.dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES, tag_creator);
  pqc_compute_bind_tag((const uint8_t*)&out_key, sizeof(out_key), pk.dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES, tag_validator);
  if (memcmp(tag_creator, tag_validator, 32) != 0) { printf("FAIL: bind tag not reproducible\n"); return false; }

  // an attacker substituting their own ML-DSA key (to spend the transparent output) yields a
  // DIFFERENT tag → validator check (c) rejects.
  pq_public_key pk_attacker; pq_secret_key sk_attacker;
  pqc_keygen(pk_attacker, sk_attacker);
  uint8_t tag_attacker[32];
  pqc_compute_bind_tag((const uint8_t*)&out_key, sizeof(out_key), pk_attacker.dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES, tag_attacker);
  if (memcmp(tag_creator, tag_attacker, 32) == 0) { printf("FAIL: attacker key produced same bind tag\n"); return false; }
  printf("PASS: bind tag reproducible by validator; substituted ML-DSA key changes the tag\n");
  return true;
}

static bool test_output_dsa_signs()
{
  kyber_shared_secret ss = make_ss(99);
  pq_public_key pk; pq_secret_key sk;
  if (!pqc_keygen_output_dsa(ss, 2, pk, sk)) { printf("FAIL: keygen\n"); return false; }
  crypto::hash msg; for (size_t i = 0; i < sizeof(msg); ++i) ((uint8_t*)&msg)[i] = (uint8_t)(i*7+1);
  pq_tx_sig s;
  memcpy(s.pk, pk.dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES);
  if (!pqc_tx_sign((const uint8_t*)&msg, sizeof(msg), sk.dilithium3_sk, ML_DSA_65_SECRET_KEY_BYTES, pk.dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES, s))
  { printf("FAIL: pqc_tx_sign\n"); return false; }
  if (!pqc_tx_verify((const uint8_t*)&msg, sizeof(msg), s)) { printf("FAIL: pqc_tx_verify (good sig)\n"); return false; }
  s.sig[0] ^= 0xFF;
  if (pqc_tx_verify((const uint8_t*)&msg, sizeof(msg), s)) { printf("FAIL: tampered sig verified\n"); return false; }
  printf("PASS: per-output ML-DSA-65 key signs and verifies the prefix hash (check d)\n");
  return true;
}

int main()
{
  bool ok = true;
  ok &= test_output_dsa_deterministic();
  ok &= test_bind_tag_matches_and_detects_tamper();
  ok &= test_output_dsa_signs();
  printf("\nRESULT: %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
