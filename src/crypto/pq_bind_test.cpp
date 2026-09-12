// HIDERING Phase 5 (HFv16) — standalone smoke test for the post-quantum BINDING primitives
// that authorise a transparent BQ spend, in their SPEC 2e form:
//   * pqc_compute_auth_commit(): the address-level commitment to the authorisation key, with
//     the type inside the preimage so two types can never collide on one commitment,
//   * pqc_compute_auth_blind(): the per-output blinding factor, a ONE-WAY image of the KEM
//     shared secret — this is what stops a single revealed auth key from unmasking every
//     output ever sent to that subaddress (spec 2e §2.4),
//   * pqc_compute_bind_tag_v2(): the tag published at output creation is a pure function of
//     (auth_type, P', auth_commit, auth_blind), so the validator recomputes exactly what the
//     creator published — and substituting another authorisation key yields a different tag,
//   * the authorisation key actually signs/verifies (the (d) check of check_tx_inputs).
//
// pqc_keygen_output_dsa (the pre-2e per-output key, derived from the shared secret alone —
// CRIT-3) is no longer used by consensus. Its determinism is still exercised below because
// pq_vector_test pins it as a liboqs-drift canary; see spec 2e §5.4.
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

  // Spec 2e: the creator commits to the address-level key, blinded per output.
  uint8_t commit[32], blind[32];
  pqc_compute_auth_commit(PQ_AUTH_TYPE_MLDSA65, pk.dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES, commit);
  if (!pqc_compute_auth_blind(ss, 1, blind)) { printf("FAIL: auth blind\n"); return false; }

  // creator side (cryptonote_tx_utils.cpp) vs validator side (blockchain.cpp): same inputs → same tag
  uint8_t tag_creator[32], tag_validator[32];
  pqc_compute_bind_tag_v2(PQ_AUTH_TYPE_MLDSA65, (const uint8_t*)&out_key, sizeof(out_key), commit, blind, tag_creator);
  pqc_compute_bind_tag_v2(PQ_AUTH_TYPE_MLDSA65, (const uint8_t*)&out_key, sizeof(out_key), commit, blind, tag_validator);
  if (memcmp(tag_creator, tag_validator, 32) != 0) { printf("FAIL: bind tag not reproducible\n"); return false; }

  // The blind must actually bind: the same key and output key under another output index is a
  // different tag. Without that, two outputs to one subaddress would share a tag.
  uint8_t blind_other[32], tag_other_idx[32];
  if (!pqc_compute_auth_blind(ss, 2, blind_other)) { printf("FAIL: auth blind #2\n"); return false; }
  if (memcmp(blind, blind_other, 32) == 0) { printf("FAIL: blind ignores the output index\n"); return false; }
  pqc_compute_bind_tag_v2(PQ_AUTH_TYPE_MLDSA65, (const uint8_t*)&out_key, sizeof(out_key), commit, blind_other, tag_other_idx);
  if (memcmp(tag_creator, tag_other_idx, 32) == 0) { printf("FAIL: bind tag ignores the blind\n"); return false; }

  // The type is inside the commitment preimage: a future type cannot reuse this commitment.
  uint8_t commit_type2[32];
  pqc_compute_auth_commit(0x02, pk.dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES, commit_type2);
  if (memcmp(commit, commit_type2, 32) == 0) { printf("FAIL: auth commit ignores the type\n"); return false; }

  // an attacker substituting their own ML-DSA key (to spend the transparent output) yields a
  // DIFFERENT tag → validator check (c) rejects.
  pq_public_key pk_attacker; pq_secret_key sk_attacker;
  pqc_keygen(pk_attacker, sk_attacker);
  uint8_t commit_attacker[32], tag_attacker[32];
  pqc_compute_auth_commit(PQ_AUTH_TYPE_MLDSA65, pk_attacker.dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES, commit_attacker);
  pqc_compute_bind_tag_v2(PQ_AUTH_TYPE_MLDSA65, (const uint8_t*)&out_key, sizeof(out_key), commit_attacker, blind, tag_attacker);
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
