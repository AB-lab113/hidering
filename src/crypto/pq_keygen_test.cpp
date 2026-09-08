// HIDERING Phase 5 Step 6 — smoke test for BQ... (post-quantum) account keygen,
// the mnemonic "BQ" address prefix, and the encode/parse round-trip.
//
// Standalone (built like pqc_test.cpp / pq_address_test.cpp). It exercises:
//   * generate_pq_keys() attaches a ML-KEM-768 keypair → account_address.is_pq() == true,
//   * a classic account stays is_pq() == false (generate_pq_keys is purely opt-in),
//   * the rendered BQ... address actually begins with "BQ" (tag 62 + marker 0x33),
//   * encode → parse round-trips the spend/view Ed25519 keys and the 1184-byte ML-KEM key.
#include <cstdio>
#include <cstring>
#include "crypto/crypto.h"
#include "crypto/pqc.h"
#include "cryptonote_basic/account.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"

using namespace cryptonote;

static bool test_classic_account_is_not_pq()
{
  account_base acc;
  acc.generate();
  if (acc.get_keys().m_account_address.is_pq()) { printf("FAIL: classic account is_pq()=true\n"); return false; }
  if (!get_pq_address_str(acc.get_keys(), MAINNET).empty()) { printf("FAIL: classic account yielded a BQ... address\n"); return false; }
  const std::string b = acc.get_public_address_str(MAINNET);
  if (b.empty() || b[0] != 'B') { printf("FAIL: classic address not B...\n"); return false; }
  printf("PASS: classic account stays is_pq()=false, classic B... address intact (%.6s...)\n", b.c_str());
  return true;
}

static bool test_bq_keygen_and_address()
{
  account_base acc;
  acc.generate();

  // Opt into a BQ... address.
  if (!generate_pq_keys(acc.get_keys_nonconst())) { printf("FAIL: generate_pq_keys\n"); return false; }
  if (!acc.get_keys().m_account_address.is_pq()) { printf("FAIL: is_pq()=false after generate_pq_keys\n"); return false; }
  if (!acc.get_keys().pq_keys) { printf("FAIL: pq_keys (decaps key) not set\n"); return false; }

  // The published ML-KEM key on the address must match the keypair's public half.
  if (memcmp(acc.get_keys().m_account_address.pq_kyber_pk->data(),
             acc.get_keys().pq_keys->kyber_pk,
             crypto::pqc::ML_KEM_768_PUBLIC_KEY_BYTES) != 0)
  { printf("FAIL: address ML-KEM pk != keypair ML-KEM pk\n"); return false; }

  // Render the BQ... address and check the mnemonic prefix.
  const std::string bq = get_pq_address_str(acc.get_keys(), MAINNET);
  printf("BQ address (len %zu): %.10s...\n", bq.size(), bq.c_str());
  if (bq.size() < 2 || bq[0] != 'B' || bq[1] != 'Q') { printf("FAIL: address does not begin with \"BQ\"\n"); return false; }

  // Encode → parse round-trip.
  account_public_address parsed{};
  if (!get_account_address_from_str_pq(parsed, bq)) { printf("FAIL: BQ parse failed\n"); return false; }
  if (!parsed.is_pq()) { printf("FAIL: parsed BQ is_pq()=false\n"); return false; }
  if (parsed.m_spend_public_key != acc.get_keys().m_account_address.m_spend_public_key ||
      parsed.m_view_public_key  != acc.get_keys().m_account_address.m_view_public_key)
  { printf("FAIL: Ed25519 keys mismatch after round-trip\n"); return false; }
  if (memcmp(parsed.pq_kyber_pk->data(), acc.get_keys().m_account_address.pq_kyber_pk->data(),
             crypto::pqc::ML_KEM_768_PUBLIC_KEY_BYTES) != 0)
  { printf("FAIL: ML-KEM-768 key mismatch after round-trip\n"); return false; }

  // The generic entry point must ROUTE a BQ... address to the BQ parser rather than
  // mistake it for a subaddress: prefix 62 is shared with subaddresses and is
  // disambiguated by payload size (64 vs 1249) plus the marker byte. This is what
  // makes `transfer BQ...` work (Phase 5 A4); before A4 the generic path rejected
  // BQ outright, and this test asserted that older contract.
  address_parse_info info{};
  if (!get_account_address_from_str(info, MAINNET, bq))
  { printf("FAIL: BQ address rejected by the generic parser (transfer to BQ... would fail)\n"); return false; }
  if (!info.address.is_pq()) { printf("FAIL: generic parser returned a non-PQ address for a BQ... string\n"); return false; }
  if (info.is_subaddress) { printf("FAIL: BQ address parsed as a subaddress\n"); return false; }
  if (info.address.m_spend_public_key != acc.get_keys().m_account_address.m_spend_public_key ||
      info.address.m_view_public_key  != acc.get_keys().m_account_address.m_view_public_key)
  { printf("FAIL: generic parser lost the Ed25519 keys of the BQ address\n"); return false; }
  if (memcmp(info.address.pq_kyber_pk->data(), acc.get_keys().m_account_address.pq_kyber_pk->data(),
             crypto::pqc::ML_KEM_768_PUBLIC_KEY_BYTES) != 0)
  { printf("FAIL: generic parser lost the ML-KEM-768 key of the BQ address\n"); return false; }

  // A classic B... address must still come back non-PQ through the same entry point.
  account_base classic;
  classic.generate();
  address_parse_info cinfo{};
  if (!get_account_address_from_str(cinfo, MAINNET, classic.get_public_address_str(MAINNET)))
  { printf("FAIL: classic B... address no longer parses\n"); return false; }
  if (cinfo.address.is_pq()) { printf("FAIL: classic B... address came back is_pq()=true\n"); return false; }

  printf("PASS: BQ keygen + \"BQ\" prefix + encode/parse round-trip (spend/view + ML-KEM-768 1184B)\n");
  return true;
}

// audit M-4: the BQ keypair must be DETERMINISTIC in the spend key, so a wallet
// restored from its 25-word seed regenerates the exact same BQ... address (and can
// therefore spend BQ funds again). Two accounts created from the same recovery key
// must yield identical BQ keys; a different recovery key must yield a different one.
static bool test_bq_keygen_is_deterministic()
{
  // Account `a`: a fresh (random) spend key, then derive its BQ keypair.
  account_base a;
  const crypto::secret_key recovery = a.generate();
  if (!generate_pq_keys(a.get_keys_nonconst())) { printf("FAIL: generate_pq_keys (a)\n"); return false; }
  const std::string addr_a = get_pq_address_str(a.get_keys(), MAINNET);

  // Account `b`: RESTORED from a's recovery key (the M-4 scenario). Its spend key
  // is identical, so the seed-derived BQ keypair must be identical too.
  account_base b;
  b.generate(recovery, true /*recover*/);
  if (memcmp(&b.get_keys().m_spend_secret_key, &a.get_keys().m_spend_secret_key, sizeof(crypto::secret_key)) != 0)
  { printf("FAIL: restored account has a different spend key (test setup)\n"); return false; }
  if (!generate_pq_keys(b.get_keys_nonconst())) { printf("FAIL: generate_pq_keys (b)\n"); return false; }

  const std::string addr_b = get_pq_address_str(b.get_keys(), MAINNET);
  if (addr_a.empty() || addr_a != addr_b)
  { printf("FAIL: restore produced a different BQ address (M-4 not fixed)\n"); return false; }
  if (memcmp(a.get_keys().pq_keys->kyber_sk, b.get_keys().pq_keys->kyber_sk,
             crypto::pqc::ML_KEM_768_SECRET_KEY_BYTES) != 0)
  { printf("FAIL: restore produced a different ML-KEM-768 secret key\n"); return false; }
  if (memcmp(a.get_keys().pq_dilithium->dilithium_sk, b.get_keys().pq_dilithium->dilithium_sk,
             crypto::pqc::ML_DSA_65_SECRET_KEY_BYTES) != 0)
  { printf("FAIL: restore produced a different ML-DSA-65 secret key\n"); return false; }

  // A different spend key must yield a different BQ address.
  account_base c;
  c.generate();
  if (!generate_pq_keys(c.get_keys_nonconst())) { printf("FAIL: generate_pq_keys (c)\n"); return false; }
  if (get_pq_address_str(c.get_keys(), MAINNET) == addr_a)
  { printf("FAIL: a different spend key collided to the same BQ address\n"); return false; }

  printf("PASS: BQ keygen deterministic in spend key (restore reproduces BQ, distinct seed → distinct BQ)\n");
  return true;
}

int main()
{
  bool ok = true;
  ok &= test_classic_account_is_not_pq();
  ok &= test_bq_keygen_and_address();
  ok &= test_bq_keygen_is_deterministic();
  printf("\nRESULT: %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
