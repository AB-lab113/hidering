// HIDERING Phase 5 Step 6 — smoke test for BQ... (post-quantum) account keygen,
// the mnemonic "BQ" address prefix, and the encode/parse round-trip.
//
// Standalone (built like pqc_test.cpp / pq_address_test.cpp). It exercises:
//   * generate_pq_keys() attaches a Kyber768 keypair → account_address.is_pq() == true,
//   * a classic account stays is_pq() == false (generate_pq_keys is purely opt-in),
//   * the rendered BQ... address actually begins with "BQ" (tag 62 + marker 0x33),
//   * encode → parse round-trips the spend/view Ed25519 keys and the 1184-byte Kyber key.
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

  // The published Kyber key on the address must match the keypair's public half.
  if (memcmp(acc.get_keys().m_account_address.pq_kyber_pk->data(),
             acc.get_keys().pq_keys->kyber_pk,
             crypto::pqc::KYBER768_PUBLIC_KEY_BYTES) != 0)
  { printf("FAIL: address Kyber pk != keypair Kyber pk\n"); return false; }

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
             crypto::pqc::KYBER768_PUBLIC_KEY_BYTES) != 0)
  { printf("FAIL: Kyber768 key mismatch after round-trip\n"); return false; }

  // A BQ... address must NOT parse via the classic path (shares prefix 62 with subaddress,
  // disambiguated by payload size + marker byte).
  address_parse_info info{};
  if (get_account_address_from_str(info, MAINNET, bq)) { printf("FAIL: BQ address wrongly parsed as classic/subaddress\n"); return false; }

  printf("PASS: BQ keygen + \"BQ\" prefix + encode/parse round-trip (spend/view + Kyber768 1184B)\n");
  return true;
}

int main()
{
  bool ok = true;
  ok &= test_classic_account_is_not_pq();
  ok &= test_bq_keygen_and_address();
  printf("\nRESULT: %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
