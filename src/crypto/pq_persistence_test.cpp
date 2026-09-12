// HIDERING Phase 5 Step 7 — smoke test for BQ... (post-quantum) key persistence.
//
// Standalone (built like pqc_test.cpp / pq_keygen_test.cpp), but it additionally
// exercises the wallet-file serialization path used by wallet2::store_keys /
// load_keys: account_keys is (de)serialized via its KV map (epee store_t_to_binary /
// load_t_from_binary), with the ML-KEM-768 secret key chacha20-encrypted in place by
// account_keys::encrypt()/decrypt() — exactly as a .keys file is written/read.
//
// It checks:
//   * a BQ account round-trips through encrypt -> serialize -> deserialize -> decrypt:
//       pq_keys restored, is_pq()==true, kyber_pk/kyber_sk and the Ed25519 secrets
//       all recovered bit-for-bit;
//   * the encrypted blob does NOT contain the plaintext kyber_sk (it is encrypted);
//   * audit CRIT-4 / decision R2a: the post-quantum ROOT secret round-trips the same way,
//       encrypted at rest (it is appended LAST in the chacha20 key stream, so this also
//       pins that the stream stays in sync with the other secrets), and it is what every
//       BQ key of the account derives from — a reloaded BQ wallet without it is unusable;
//   * a classic (B...) account round-trips unchanged and stays is_pq()==false, with
//       no "pq_keys" and no "pq_root" field present (no format change / no regression).
#include <cstdio>
#include <cstring>
#include <string>
#include "crypto/crypto.h"
#include "crypto/chacha.h"
#include "crypto/pqc.h"
#include "cryptonote_basic/account.h"
#include "storages/portable_storage_template_helper.h"

using namespace cryptonote;

static crypto::chacha_key test_key()
{
  crypto::chacha_key k;
  crypto::generate_chacha_key("hidering-phase5-step7-test-password", 35, k, 1);
  return k;
}

static bool test_bq_key_persistence()
{
  const crypto::chacha_key key = test_key();

  account_base acc;
  acc.generate();
  const crypto::secret_key root_plain = generate_pq_root_secret();
  if (!generate_pq_keys(acc.get_keys_nonconst(), root_plain)) { printf("FAIL: generate_pq_keys\n"); return false; }
  if (!acc.get_keys().m_account_address.is_pq()) { printf("FAIL: is_pq()=false after keygen\n"); return false; }
  if (!acc.get_keys().pq_root) { printf("FAIL: pq_root not set after keygen\n"); return false; }

  // Keep plaintext copies of the secrets we expect to recover.
  const crypto::secret_key spend_plain = acc.get_keys().m_spend_secret_key;
  const crypto::secret_key view_plain  = acc.get_keys().m_view_secret_key;
  crypto::pqc::pq_stealth_keys pq_plain = *acc.get_keys().pq_keys;

  // Encrypt the keys in place (as get_keys_file_data does), then serialize.
  acc.encrypt_keys(key);
  epee::byte_slice slice;
  if (!epee::serialization::store_t_to_binary(acc, slice)) { printf("FAIL: store_t_to_binary\n"); return false; }
  std::string blob(reinterpret_cast<const char*>(slice.data()), slice.size());

  // The kyber_sk must be encrypted on disk, never the plaintext secret bytes.
  std::string needle(reinterpret_cast<const char*>(pq_plain.kyber_sk), crypto::pqc::ML_KEM_768_SECRET_KEY_BYTES);
  if (blob.find(needle) != std::string::npos) { printf("FAIL: plaintext kyber_sk found in serialized blob\n"); return false; }
  // ... but the (public) kyber_pk is in the clear.
  std::string pk_needle(reinterpret_cast<const char*>(pq_plain.kyber_pk), crypto::pqc::ML_KEM_768_PUBLIC_KEY_BYTES);
  if (blob.find(pk_needle) == std::string::npos) { printf("FAIL: kyber_pk not present in serialized blob\n"); return false; }
  // audit CRIT-4: the root is the most valuable PQ secret of all (it regenerates every BQ
  // key, present and future) — it must never appear in the clear either.
  std::string root_needle(reinterpret_cast<const char*>(root_plain.data), sizeof(crypto::secret_key));
  if (blob.find(root_needle) != std::string::npos) { printf("FAIL: plaintext pq_root found in serialized blob\n"); return false; }
  if (blob.find("pq_root") == std::string::npos) { printf("FAIL: no pq_root field written for a BQ account\n"); return false; }

  // Deserialize into a fresh account and decrypt (as load_keys_buf does).
  account_base acc2;
  if (!epee::serialization::load_t_from_binary(acc2, blob)) { printf("FAIL: load_t_from_binary\n"); return false; }
  if (!acc2.get_keys().pq_keys) { printf("FAIL: pq_keys not restored on load\n"); return false; }
  if (!acc2.get_keys().m_account_address.is_pq()) { printf("FAIL: is_pq()=false after reload (address pk not rehydrated)\n"); return false; }
  acc2.decrypt_keys(key);

  const account_keys& k2 = acc2.get_keys();
  if (memcmp(&k2.m_spend_secret_key, &spend_plain, sizeof(crypto::secret_key)) != 0) { printf("FAIL: spend key mismatch\n"); return false; }
  if (memcmp(&k2.m_view_secret_key,  &view_plain,  sizeof(crypto::secret_key)) != 0) { printf("FAIL: view key mismatch\n"); return false; }
  if (memcmp(k2.pq_keys->kyber_sk, pq_plain.kyber_sk, crypto::pqc::ML_KEM_768_SECRET_KEY_BYTES) != 0) { printf("FAIL: kyber_sk mismatch after decrypt\n"); return false; }
  if (memcmp(k2.pq_keys->kyber_pk, pq_plain.kyber_pk, crypto::pqc::ML_KEM_768_PUBLIC_KEY_BYTES) != 0) { printf("FAIL: kyber_pk mismatch\n"); return false; }
  if (memcmp(k2.m_account_address.pq_kyber_pk->data(), pq_plain.kyber_pk, crypto::pqc::ML_KEM_768_PUBLIC_KEY_BYTES) != 0) { printf("FAIL: rehydrated address kyber_pk mismatch\n"); return false; }
  if (!k2.pq_root) { printf("FAIL: pq_root not restored on load\n"); return false; }
  if (memcmp(&*k2.pq_root, &root_plain, sizeof(crypto::secret_key)) != 0) { printf("FAIL: pq_root mismatch after decrypt\n"); return false; }
  if (get_pq_root_secret(k2) == nullptr) { printf("FAIL: get_pq_root_secret returns null on a reloaded BQ account\n"); return false; }
  // The whole point of persisting the root: subaddress keys are redrived from it, so the
  // reloaded account must produce the same ones as the original. `acc` is still encrypted
  // here (that is what the blob checks above needed), so decrypt it back first — deriving
  // from an encrypted root would silently give different keys, which is exactly the class of
  // bug that made ordinary wallets miss every BQ output before decision 4 (design_2b §8.5).
  acc.decrypt_keys(key);
  crypto::pqc::pq_stealth_keys s1{}, s2{};
  const subaddress_index idx{0, 5};
  if (!generate_pq_subaddress_keys(acc.get_keys(), idx, s1) ||
      !generate_pq_subaddress_keys(k2, idx, s2)) { printf("FAIL: subaddress derivation\n"); return false; }
  if (memcmp(s1.kyber_sk, s2.kyber_sk, crypto::pqc::ML_KEM_768_SECRET_KEY_BYTES) != 0)
  { printf("FAIL: reloaded account derives a different BQ subaddress key\n"); return false; }
  if (memcmp(&*acc.get_keys().pq_root, &root_plain, sizeof(crypto::secret_key)) != 0)
  { printf("FAIL: the root did not survive encrypt/decrypt in place\n"); return false; }

  printf("PASS: BQ keypair + PQ root persist through encrypt -> serialize -> reload -> decrypt (both encrypted on disk)\n");
  return true;
}

static bool test_classic_no_regression()
{
  const crypto::chacha_key key = test_key();

  account_base acc;
  acc.generate();
  if (acc.get_keys().m_account_address.is_pq()) { printf("FAIL: classic account is_pq()=true\n"); return false; }

  const crypto::secret_key spend_plain = acc.get_keys().m_spend_secret_key;

  acc.encrypt_keys(key);
  epee::byte_slice slice;
  if (!epee::serialization::store_t_to_binary(acc, slice)) { printf("FAIL: store_t_to_binary (classic)\n"); return false; }
  std::string blob(reinterpret_cast<const char*>(slice.data()), slice.size());

  // No "pq_keys" field for a classic wallet => key_data layout unchanged.
  if (blob.find("pq_keys") != std::string::npos) { printf("FAIL: classic wallet emitted a pq_keys field\n"); return false; }
  if (blob.find("pq_root") != std::string::npos) { printf("FAIL: classic wallet emitted a pq_root field\n"); return false; }

  account_base acc2;
  if (!epee::serialization::load_t_from_binary(acc2, blob)) { printf("FAIL: load_t_from_binary (classic)\n"); return false; }
  if (acc2.get_keys().pq_keys) { printf("FAIL: classic reload conjured a pq_keys\n"); return false; }
  if (acc2.get_keys().pq_root) { printf("FAIL: classic reload conjured a pq_root\n"); return false; }
  if (get_pq_root_secret(acc2.get_keys()) != nullptr) { printf("FAIL: classic account reports a PQ root\n"); return false; }
  if (acc2.get_keys().m_account_address.is_pq()) { printf("FAIL: classic reload is_pq()=true\n"); return false; }
  acc2.decrypt_keys(key);
  if (memcmp(&acc2.get_keys().m_spend_secret_key, &spend_plain, sizeof(crypto::secret_key)) != 0) { printf("FAIL: classic spend key mismatch\n"); return false; }

  printf("PASS: classic wallet round-trips unchanged, no pq_keys/pq_root field, is_pq()=false\n");
  return true;
}

int main()
{
  bool ok = true;
  ok &= test_bq_key_persistence();
  ok &= test_classic_no_regression();
  printf("\nRESULT: %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
