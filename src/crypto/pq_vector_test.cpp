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
// TWO DELIBERATE REGENERATIONS HAVE HAPPENED.
//
// (2) 12 September 2026 — spec 2e (docs/audit/spec_2e_bq_address_auth_binding.md). The BQ
// address payload gained an auth_ver byte and a 32-byte authorisation commitment, so the
// ADDRESS vector moved (length 1770 -> 1770, new head, new digest). Nothing else did: the
// ML-KEM/ML-DSA account key digests below are byte-identical across that change, which is the
// evidence that only the address ENCODING moved and not any derivation. New vectors pin the
// per-subaddress ML-DSA identity key (a new hot path over the randombytes hook), the
// authorisation commitment, the binding blind and the v2 binding tag.
//
// (1) 12 September 2026 — audit CRIT-4, decision R2a
// (docs/audit/design_2c_pq_root_shor_resistance.md). The post-quantum keys used to be derived
// from the Ed25519 spend key; they are now derived from an independent root secret with its
// own mnemonic. That is a change of INPUT, not of derivation, so only the ACCOUNT-LEVEL
// vectors below were regenerated (they are now a function of V_PQ_ROOT, and V_PQ_ROOT is
// deliberately NOT equal to V_RECOVERY_KEY — otherwise this test would still pass against
// the vulnerable code). The raw-seed vectors (V_RAW_*) and the per-output vectors
// (V_OUT7_*, V_BIND_TAG) were NOT touched: they sit below the change and are exactly what
// isolates a liboqs drift from one of our own. If one of THOSE moves, it is not this fix.
//
// The rule the header states stands: never edit an expected value to make a failing test
// pass. Regenerate only against a written decision, and say which values and why, here.
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
// audit CRIT-4 / decision R2a: the post-quantum root, the SECOND seed. Raw entropy, never
// reduced mod l, and chosen distinct from V_RECOVERY_KEY on purpose — with the two equal,
// this vector could not tell the fixed derivation apart from the vulnerable one.
static const char *V_PQ_ROOT =
  "b7104c2e5d1f83a6c904e27b3af85160d2c73e94a1580bf6372ed0c845b9e719";

// The user-visible artifact: the BQ... address this seed must always produce.
static const char *V_BQ_ADDRESS_PREFIX = "BQQpMRu8sXnLTrRAFBheoxESucC4ikKr8dPKhWzJ1rgyfqwmQpnmgrFVjtZpAeq8y";
static const size_t V_BQ_ADDRESS_LEN = 1770;
// Keccak-256 of the whole address string, so the full 1770 chars are pinned, not just the head.
static const char *V_BQ_ADDRESS_KECCAK =
  "ad941a949bd4fee0ce8d51f87082abda5d44c2aa70f1d14e23d0e4e9a70ba8ae";

// Account-level keys derived from V_PQ_ROOT (Keccak-256 fingerprints). REGENERATED
// 12 Sep 2026 for CRIT-4 / R2a — see the header. Everything below the account level was
// re-measured at the same time and came back byte-identical, which is what says the
// derivation itself did not move.
static const char *V_MLKEM_PK_KECCAK = "1b5b6df96296f511fdfec2f6d38fcd5e828b549b1f2db315faf4ec28d090a0d9";
static const char *V_MLKEM_SK_KECCAK = "f221c7b375810e25da31ea36e6b829a418f1233e67d9ed539f2a62773da81040";
static const char *V_MLDSA_PK_KECCAK = "da1bb5b11ceacdf16271f7b8535e857f838c3e7b7ebf9daccb5db624dc5398c2";
static const char *V_MLDSA_SK_KECCAK = "ac722285cb9f364627e4251c75cd493807c1dac80c9274e855ee978d9293a6a6";
static const char *V_MLKEM_PK_HEAD16 = "b7f46892723fda76523bbb749bc20498";
static const char *V_MLDSA_PK_HEAD16 = "a692062fe4bd6ebdddfd9b91c4acdb4f";

// Raw-seed derivation, isolating liboqs from Monero's key derivation: seed[i] = 0xA0 + i.
static const char *V_RAW_MLKEM_PK_KECCAK = "105e923b335343d56c5c21a48b9d3dd37a1e981747bc63f50e3df6bd1de0a2f2";
static const char *V_RAW_MLDSA_PK_KECCAK = "6ac4fd6136d89cc8c822ce7dd854091f8b8530711ac5d1ca324c03424a60bb47";

// Per-output ML-DSA-65 key from (shared secret 0x5A^i, output index 7). This is the hot path
// of a BQ spend: if it moves, previously created BQ outputs become unspendable.
static const char *V_OUT7_MLDSA_PK_KECCAK = "98967f0d3919a2b9e1fea0938909daba41316942702ac25f868565de91f00339";

// Binding tag over output key 0x33+i and that per-output ML-DSA key (the pre-2e check c).
static const char *V_BIND_TAG = "9addd37d002e3073cb297a06a1ecb9e3b340ea2c683bfad35ca778ac4570cf3c";

// ---- spec 2e ---------------------------------------------------------------------------
// The ML-DSA-65 IDENTITY key of BQ subaddress (3,9). This is the hot path of a BQ spend under
// spec 2e, and it drives liboqs' randombytes hook exactly as the account key does: if the
// backend's read pattern moves, every BQ output already created becomes unspendable.
static const char *V_SUB39_IDPK_KECCAK = "3a1cd062bfa3f6c91f0f4ed5f9d3e4c9cfeb8838ae723b45823a6f5059221dad";
static const char *V_SUB39_IDSK_KECCAK = "68a286d9f05ae11e594518e0c7250d00d40dfca5c2ad9673a2b02a99e40475e0";
// The address-level commitment to that identity key (pure hash).
static const char *V_SUB39_AUTH_COMMIT = "4c6ce22f2343729e031899dab7d88d8775c301f8701613c249fc082dd416deec";
// The per-output binding blind for (shared secret 0x5A^i, output index 7) (pure hash).
static const char *V_AUTH_BLIND = "aced0ef294e54233dcffb460367d6e4778923e03e7fca999cc8725ace1d6e1db";
// The v2 binding tag over output key 0x33+i, that commitment and that blind (validator check c).
static const char *V_BIND_TAG_V2 = "448a97dfbaa06fe735f1d6b9bfa9227b2136f206149a66c8b868da94f6c5cc48";

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

// audit CRIT-4 / decision R2a — the fixed post-quantum root behind the account vectors.
static bool fill_pq_root(crypto::secret_key &root)
{
  for (int i = 0; i < 32; ++i)
  {
    unsigned byte = 0;
    if (sscanf(V_PQ_ROOT + 2 * i, "%2x", &byte) != 1) return false;
    ((uint8_t *)root.data)[i] = (uint8_t)byte;
  }
  return true;
}

// The account-level vector: the seeds a user would restore from must still reach the same
// BQ address and the same post-quantum keys. Since CRIT-4 that is TWO seeds — the classic
// one for the Ed25519 half, the post-quantum root for everything BQ.
static bool test_account_vector()
{
  crypto::secret_key rec, root;
  fill_recovery_key(rec);
  if (!fill_pq_root(root)) { printf("FAIL: cannot parse V_PQ_ROOT\n"); return false; }

  account_base acc;
  acc.generate(rec, true /*recover*/);
  if (!eq("test recovery key (test setup, not liboqs)", hexs(acc.get_keys().m_spend_secret_key.data, 32), V_RECOVERY_KEY))
    return false;
  if (!eq("view secret key (test setup, not liboqs)", hexs(acc.get_keys().m_view_secret_key.data, 32), V_VIEW_SK))
    return false;

  if (memcmp(&root, &acc.get_keys().m_spend_secret_key, sizeof(crypto::secret_key)) == 0)
  { printf("FAIL: V_PQ_ROOT equals the spend key — this vector could not detect CRIT-4\n"); return false; }
  if (!generate_pq_keys(acc.get_keys_nonconst(), root)) { printf("FAIL: generate_pq_keys\n"); return false; }
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
    printf("PASS: account vector — the two seeds still derive the same BQ address and the same ML-KEM/ML-DSA keys\n");
  return ok;
}

// Spec 2e — the recipient-held authorisation factor: the per-subaddress ML-DSA-65 identity key
// (a second derandomised path over liboqs' randombytes hook, so a second drift canary), and the
// three pure hashes the consensus binding is made of.
static bool test_auth_vector()
{
  crypto::secret_key rec, root;
  fill_recovery_key(rec);
  if (!fill_pq_root(root)) { printf("FAIL: cannot parse V_PQ_ROOT\n"); return false; }
  account_base acc;
  acc.generate(rec, true /*recover*/);
  if (!generate_pq_keys(acc.get_keys_nonconst(), root)) { printf("FAIL: generate_pq_keys\n"); return false; }

  bool ok = true;
  pq_dilithium_keys id{};
  const subaddress_index idx{3, 9};
  if (!generate_pq_identity_keys(acc.get_keys(), idx, id)) { printf("FAIL: generate_pq_identity_keys\n"); return false; }
  ok &= eq("BQ subaddress (3,9) ML-DSA-65 identity public key",
           keccak_hex(id.dilithium_pk, ML_DSA_65_PUBLIC_KEY_BYTES), V_SUB39_IDPK_KECCAK);
  ok &= eq("BQ subaddress (3,9) ML-DSA-65 identity secret key",
           keccak_hex(id.dilithium_sk, ML_DSA_65_SECRET_KEY_BYTES), V_SUB39_IDSK_KECCAK);

  uint8_t commit[32];
  pqc_compute_auth_commit(PQ_AUTH_TYPE_MLDSA65, id.dilithium_pk, ML_DSA_65_PUBLIC_KEY_BYTES, commit);
  ok &= eq("authorisation commitment of BQ subaddress (3,9)", hexs(commit, 32), V_SUB39_AUTH_COMMIT);
  // and the account helper must agree with the primitive
  std::array<uint8_t, 32> commit2{};
  if (!get_pq_auth_commit(acc.get_keys(), idx, commit2) || memcmp(commit, commit2.data(), 32) != 0)
  { printf("FAIL: get_pq_auth_commit disagrees with pqc_compute_auth_commit\n"); ok = false; }

  kyber_shared_secret ss;
  for (int i = 0; i < 32; ++i) ss.ss[i] = (uint8_t)(0x5A ^ i);
  uint8_t blind[32];
  if (!pqc_compute_auth_blind(ss, 7, blind)) { printf("FAIL: pqc_compute_auth_blind\n"); return false; }
  ok &= eq("binding blind (ss 0x5A^i, index 7)", hexs(blind, 32), V_AUTH_BLIND);

  crypto::public_key P;
  for (int i = 0; i < 32; ++i) ((uint8_t *)P.data)[i] = (uint8_t)(0x33 + i);
  uint8_t tag[32];
  pqc_compute_bind_tag_v2(PQ_AUTH_TYPE_MLDSA65, (const uint8_t *)&P, 32, commit, blind, tag);
  ok &= eq("binding tag v2 (validator check c)", hexs(tag, 32), V_BIND_TAG_V2);

  if (ok)
    printf("PASS: spec 2e vector — per-subaddress identity key, commitment, blind and v2 binding tag unchanged\n");
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
  ok &= test_auth_vector();
  ok &= test_output_vector();
  printf("\nRESULT: %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
