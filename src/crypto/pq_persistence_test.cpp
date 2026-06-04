// HIDERING Phase 5 Step 7 — smoke test for BQ... (post-quantum) key persistence.
//
// Standalone (built like pqc_test.cpp / pq_keygen_test.cpp), but it additionally
// exercises the wallet-file serialization path used by wallet2::store_keys /
// load_keys: account_keys is (de)serialized via its KV map (epee store_t_to_binary /
// load_t_from_binary), with the Kyber768 secret key chacha20-encrypted in place by
// account_keys::encrypt()/decrypt() — exactly as a .keys file is written/read.
//
// It checks:
//   * a BQ account round-trips through encrypt -> serialize -> deserialize -> decrypt:
//       pq_keys restored, is_pq()==true, kyber_pk/kyber_sk and the Ed25519 secrets
//       all recovered bit-for-bit;
//   * the encrypted blob does NOT contain the plaintext kyber_sk (it is encrypted);
//   * a classic (B...) account round-trips unchanged and stays is_pq()==false, with
//       no "pq_keys" field present (no format change / no regression).
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
  if (!generate_pq_keys(acc.get_keys_nonconst())) { printf("FAIL: generate_pq_keys\n"); return false; }
  if (!acc.get_keys().m_account_address.is_pq()) { printf("FAIL: is_pq()=false after keygen\n"); return false; }

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
  std::string needle(reinterpret_cast<const char*>(pq_plain.kyber_sk), crypto::pqc::KYBER768_SECRET_KEY_BYTES);
  if (blob.find(needle) != std::string::npos) { printf("FAIL: plaintext kyber_sk found in serialized blob\n"); return false; }
  // ... but the (public) kyber_pk is in the clear.
  std::string pk_needle(reinterpret_cast<const char*>(pq_plain.kyber_pk), crypto::pqc::KYBER768_PUBLIC_KEY_BYTES);
  if (blob.find(pk_needle) == std::string::npos) { printf("FAIL: kyber_pk not present in serialized blob\n"); return false; }

  // Deserialize into a fresh account and decrypt (as load_keys_buf does).
  account_base acc2;
  if (!epee::serialization::load_t_from_binary(acc2, blob)) { printf("FAIL: load_t_from_binary\n"); return false; }
  if (!acc2.get_keys().pq_keys) { printf("FAIL: pq_keys not restored on load\n"); return false; }
  if (!acc2.get_keys().m_account_address.is_pq()) { printf("FAIL: is_pq()=false after reload (address pk not rehydrated)\n"); return false; }
  acc2.decrypt_keys(key);

  const account_keys& k2 = acc2.get_keys();
  if (memcmp(&k2.m_spend_secret_key, &spend_plain, sizeof(crypto::secret_key)) != 0) { printf("FAIL: spend key mismatch\n"); return false; }
  if (memcmp(&k2.m_view_secret_key,  &view_plain,  sizeof(crypto::secret_key)) != 0) { printf("FAIL: view key mismatch\n"); return false; }
  if (memcmp(k2.pq_keys->kyber_sk, pq_plain.kyber_sk, crypto::pqc::KYBER768_SECRET_KEY_BYTES) != 0) { printf("FAIL: kyber_sk mismatch after decrypt\n"); return false; }
  if (memcmp(k2.pq_keys->kyber_pk, pq_plain.kyber_pk, crypto::pqc::KYBER768_PUBLIC_KEY_BYTES) != 0) { printf("FAIL: kyber_pk mismatch\n"); return false; }
  if (memcmp(k2.m_account_address.pq_kyber_pk->data(), pq_plain.kyber_pk, crypto::pqc::KYBER768_PUBLIC_KEY_BYTES) != 0) { printf("FAIL: rehydrated address kyber_pk mismatch\n"); return false; }

  printf("PASS: BQ keypair persists through encrypt -> serialize -> reload -> decrypt (kyber_sk encrypted on disk)\n");
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

  account_base acc2;
  if (!epee::serialization::load_t_from_binary(acc2, blob)) { printf("FAIL: load_t_from_binary (classic)\n"); return false; }
  if (acc2.get_keys().pq_keys) { printf("FAIL: classic reload conjured a pq_keys\n"); return false; }
  if (acc2.get_keys().m_account_address.is_pq()) { printf("FAIL: classic reload is_pq()=true\n"); return false; }
  acc2.decrypt_keys(key);
  if (memcmp(&acc2.get_keys().m_spend_secret_key, &spend_plain, sizeof(crypto::secret_key)) != 0) { printf("FAIL: classic spend key mismatch\n"); return false; }

  printf("PASS: classic wallet round-trips unchanged, no pq_keys field, is_pq()=false\n");
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
