// HIDERING Phase 5 — FROZEN post-quantum derivation vector (liboqs upgrade guard).
//
// Standalone (built like pqc_test.cpp / pq_keygen_test.cpp).
//
// WHY THIS EXISTS. liboqs exposes no derandomised keygen for ML-DSA-65, so
// pqc_keygen_from_seed derives it by driving liboqs' `randombytes` hook with a SHAKE256
// stream. That makes the derived key a function not only of our seed but of HOW MANY BYTES,
// AND IN WHAT ORDER, liboqs' ML-DSA implementation happens to read from that hook.
//
// liboqs 0.16.0 switches the ML-DSA backend to mldsa-native. If that backend reads entropy
// differently, THE SAME 25-WORD SEED WOULD DERIVE A DIFFERENT BQ ADDRESS after the upgrade —
// silently re-creating M-4, the bug where restore-from-seed could not reach your funds.
//
// The values below were generated on the currently pinned liboqs (submodule
// 97f6b86b1b6d109cfd43cf276ae39c2e776aed80 = tag 0.15.0) and are frozen here. Re-run this
// test after ANY liboqs bump, before shipping it. If it fails, the upgrade is NOT safe to
// take as-is: it would strand every existing BQ wallet, and needs a migration plan rather
// than a version bump.
//
// Note what pq_keygen_test does NOT cover: it checks that two derivations agree with each
// other inside ONE build, which stays true even if the whole derivation shifts. Only a
// frozen vector catches a cross-version change. See
// docs/audit/liboqs_0.16.0_upgrade_gate.md.
//
// The seed material here is a published constant chosen for this test. It is not, and must
// never be, a real wallet seed.
#include <cstdio>
#include <cstring>
#include <string>

#include "crypto/crypto.h"
#include "crypto/hash.h"
#include "crypto/pqc.h"
#include "cryptonote_basic/account.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"

using namespace cryptonote;
using namespace crypto::pqc;

// ---------------------------------------------------------------------------------------
// FROZEN VECTOR — liboqs 0.15.0 (submodule pin 97f6b86b1b6d109cfd43cf276ae39c2e776aed80)
// Generated 8 September 2026. Do not edit to make a failing test pass: a mismatch is the
// signal this file exists to produce.
// ---------------------------------------------------------------------------------------

// Test recovery key = the account's spend secret key (already reduced).
static const char *V_RECOVERY_KEY =
  "377a478a20a052d8ec70cc8620fb41e72032435465768798a9bacbdcedfe0f00";
static const char *V_VIEW_SK =
  "190f77729964bef50b83775a0ea0a20002cc918da993186d582d9e2276b39c04";

// The user-visible artifact: the BQ... address this seed must always produce.
static const char *V_BQ_ADDRESS_PREFIX = "BQSwT5LjQ7VGUiWWMh6uR9HoYNoXEd6gYPjYaD7QBKAPAwvmZRwavjkcCZjvg3Hii";
static const size_t V_BQ_ADDRESS_LEN = 1725;
// Keccak-256 of the whole address string, so the full 1725 chars are pinned, not just the head.
static const char *V_BQ_ADDRESS_KECCAK =
  "7f7debf90b40bd301bbc863fd0d6d19f29154672d6143fc6ac59b9ad6d27a5d0";

// Account-level keys derived from the spend key (Keccak-256 fingerprints).
static const char *V_MLKEM_PK_KECCAK = "db06ce05413ac917bcb2c31da1e24a7f1eeeea335e49c6145aa5eba4c280d2ff";
static const char *V_MLKEM_SK_KECCAK = "865e800064147287d5ba1d721072cb0388f55e4db21d19f70704c16691ff36a0";
static const char *V_MLDSA_PK_KECCAK = "a63e8353f3b100ba0624aaeddb1ef31b5b2988aded32fa55242a036a4c7919e8";
static const char *V_MLDSA_SK_KECCAK = "f1a4ce185fc85ef77f947622ff914bc9033a0b3072ae31d6d430ba217a83b206";
static const char *V_MLKEM_PK_HEAD16 = "90e2a3c4aa1e484a1a1f704f1d708d6b";
static const char *V_MLDSA_PK_HEAD16 = "62d97716904717a08861026a087ac946";

// Raw-seed derivation, isolating liboqs from Monero's key derivation: seed[i] = 0xA0 + i.
static const char *V_RAW_MLKEM_PK_KECCAK = "105e923b335343d56c5c21a48b9d3dd37a1e981747bc63f50e3df6bd1de0a2f2";
static const char *V_RAW_MLDSA_PK_KECCAK = "6ac4fd6136d89cc8c822ce7dd854091f8b8530711ac5d1ca324c03424a60bb47";

// Per-output ML-DSA-65 key from (shared secret 0x5A^i, output index 7). This is the hot path
// of a BQ spend: if it moves, previously created BQ outputs become unspendable.
static const char *V_OUT7_MLDSA_PK_KECCAK = "98967f0d3919a2b9e1fea0938909daba41316942702ac25f868565de91f00339";

// Binding tag over output key 0x33+i and that per-output ML-DSA key (validator check c).
static const char *V_BIND_TAG = "9addd37d002e3073cb297a06a1ecb9e3b340ea2c683bfad35ca778ac4570cf3c";

// ---------------------------------------------------------------------------------------

static std::string hexs(const void *p, size_t n)
{
  static const char *H = "0123456789abcdef";
  std::string o; const uint8_t *b = (const uint8_t *)p;
  for (size_t i = 0; i < n; ++i) { o += H[b[i] >> 4]; o += H[b[i] & 15]; }
  return o;
}

static std::string keccak_hex(const void *p, size_t n)
{
  crypto::hash h;
  crypto::cn_fast_hash(p, n, h);
  return hexs(&h, 32);
}

static bool eq(const char *what, const std::string &got, const char *want)
{
  if (got == want)
    return true;
  printf("FAIL: %s changed\n      expected %s\n      got      %s\n", what, want, got.c_str());
  return false;
}

static void fill_recovery_key(crypto::secret_key &rec)
{
  for (int i = 0; i < 32; ++i)
    ((uint8_t *)rec.data)[i] = (uint8_t)(0x11 * (i + 1));
  sc_reduce32((uint8_t *)rec.data);
}

// The account-level vector: the seed a user would restore from must still reach the same
// BQ address and the same post-quantum keys.
static bool test_account_vector()
{
  crypto::secret_key rec;
  fill_recovery_key(rec);

  account_base acc;
  acc.generate(rec, true /*recover*/);
  if (!eq("test recovery key (test setup, not liboqs)", hexs(acc.get_keys().m_spend_secret_key.data, 32), V_RECOVERY_KEY))
    return false;
  if (!eq("view secret key (test setup, not liboqs)", hexs(acc.get_keys().m_view_secret_key.data, 32), V_VIEW_SK))
    return false;

  if (!generate_pq_keys(acc.get_keys_nonconst())) { printf("FAIL: generate_pq_keys\n"); return false; }
  const account_keys &k = acc.get_keys();

  bool ok = true;
  const std::string addr = get_pq_address_str(k, MAINNET);
  if (addr.size() != V_BQ_ADDRESS_LEN)
  { printf("FAIL: BQ address length changed: expected %zu, got %zu\n", V_BQ_ADDRESS_LEN, addr.size()); ok = false; }
  if (addr.compare(0, strlen(V_BQ_ADDRESS_PREFIX), V_BQ_ADDRESS_PREFIX) != 0)
  { printf("FAIL: BQ address head changed\n      expected %s...\n      got      %.65s...\n", V_BQ_ADDRESS_PREFIX, addr.c_str()); ok = false; }
  ok &= eq("BQ address (full string)", keccak_hex(addr.data(), addr.size()), V_BQ_ADDRESS_KECCAK);

  ok &= eq("ML-KEM-768 public key", keccak_hex(k.pq_keys->kyber_pk, ML_KEM_768_PUBLIC_KEY_BYTES), V_MLKEM_PK_KECCAK);
  ok &= eq("ML-KEM-768 secret key", keccak_hex(k.pq_keys->kyber_sk, ML_KEM_768_SECRET_KEY_BYTES), V_MLKEM_SK_KECCAK);
  ok &= eq("ML-DSA-65 public key",  keccak_hex(k.pq_dilithium->dilithium_pk, ML_DSA_65_PUBLIC_KEY_BYTES), V_MLDSA_PK_KECCAK);
  ok &= eq("ML-DSA-65 secret key",  keccak_hex(k.pq_dilithium->dilithium_sk, ML_DSA_65_SECRET_KEY_BYTES), V_MLDSA_SK_KECCAK);
  ok &= eq("ML-KEM-768 public key (first 16 bytes)", hexs(k.pq_keys->kyber_pk, 16), V_MLKEM_PK_HEAD16);
  ok &= eq("ML-DSA-65 public key (first 16 bytes)",  hexs(k.pq_dilithium->dilithium_pk, 16), V_MLDSA_PK_HEAD16);

  if (ok)
    printf("PASS: account vector — seed still derives the same BQ address and the same ML-KEM/ML-DSA keys\n");
  return ok;
}

// The same check one level down, on the raw liboqs-facing entry point, so a failure can be
// attributed to liboqs rather than to Monero's key derivation.
static bool test_raw_seed_vector()
{
  uint8_t seed[32];
  for (int i = 0; i < 32; ++i) seed[i] = (uint8_t)(0xA0 + i);

  pq_public_key pk; pq_secret_key sk;
  if (!pqc_keygen_from_seed(seed, sizeof(seed), pk, sk)) { printf("FAIL: pqc_keygen_from_seed\n"); return false; }

  bool ok = true;
  ok &= eq("raw-seed ML-KEM-768 public key", keccak_hex(pk.kyber768_pk, ML_KEM_768_PUBLIC_KEY_BYTES), V_RAW_MLKEM_PK_KECCAK);
  ok &= eq("raw-seed ML-DSA-65 public key",  keccak_hex(pk.dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES), V_RAW_MLDSA_PK_KECCAK);
  if (ok)
    printf("PASS: raw-seed vector — pqc_keygen_from_seed is unchanged (ML-KEM derand + ML-DSA over the SHAKE256 hook)\n");
  return ok;
}

// Per-output derivation and the binding tag: what a BQ output's spendability depends on.
static bool test_output_vector()
{
  kyber_shared_secret ss;
  for (int i = 0; i < 32; ++i) ss.ss[i] = (uint8_t)(0x5A ^ i);

  pq_public_key opk; pq_secret_key osk;
  if (!pqc_keygen_output_dsa(ss, 7, opk, osk)) { printf("FAIL: pqc_keygen_output_dsa\n"); return false; }

  bool ok = eq("per-output ML-DSA-65 public key (index 7)",
               keccak_hex(opk.dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES), V_OUT7_MLDSA_PK_KECCAK);

  crypto::public_key P;
  for (int i = 0; i < 32; ++i) ((uint8_t *)P.data)[i] = (uint8_t)(0x33 + i);
  uint8_t tag[32];
  pqc_compute_bind_tag((const uint8_t *)&P, 32, opk.dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES, tag);
  ok &= eq("binding tag (validator check c)", hexs(tag, 32), V_BIND_TAG);

  // The signature is randomised (hedged), so it cannot be pinned — but the key it comes from
  // is pinned above, and it must still verify.
  pq_tx_sig sig;
  const uint8_t msg[32] = {0};
  if (!pqc_tx_sign(msg, sizeof(msg), osk.dilithium3_sk, ML_DSA_65_SECRET_KEY_BYTES,
                   opk.dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES, sig)
      || !pqc_tx_verify(msg, sizeof(msg), sig))
  { printf("FAIL: the pinned per-output key no longer signs/verifies\n"); ok = false; }

  if (ok)
    printf("PASS: output vector — per-output ML-DSA-65 key and binding tag unchanged\n");
  return ok;
}

int main()
{
  printf("Frozen against liboqs 0.15.0 (pin 97f6b86b1b6d109cfd43cf276ae39c2e776aed80).\n"
         "A failure here means a dependency change moved a derivation that user funds depend on.\n\n");
  bool ok = true;
  ok &= test_raw_seed_vector();
  ok &= test_account_vector();
  ok &= test_output_vector();
  printf("\nRESULT: %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
