// HIDERING Phase 5 Step 5 — smoke test: classic B... address parsing is unchanged,
// and the new BQ... (post-quantum, Kyber768-carrying) address round-trips.
#include <cstdio>
#include <cstring>
#include <array>
#include "crypto/crypto.h"
#include "crypto/pqc.h"
#include "cryptonote_basic/account.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"

using namespace cryptonote;

static bool test_classic_b_address()
{
  // Build a valid classic account and round-trip its B... address.
  account_base acc;
  acc.generate();
  const std::string b_addr = acc.get_public_address_str(MAINNET);
  printf("classic address: %s\n", b_addr.c_str());

  if (b_addr.empty() || b_addr[0] != 'B') { printf("FAIL: classic address not B...\n"); return false; }

  address_parse_info info{};
  if (!get_account_address_from_str(info, MAINNET, b_addr)) { printf("FAIL: classic parse failed\n"); return false; }
  if (info.address != acc.get_keys().m_account_address) { printf("FAIL: classic round-trip mismatch\n"); return false; }
  if (info.address.is_pq()) { printf("FAIL: classic address reports is_pq()=true\n"); return false; }

  // A classic address must NOT parse as a BQ... address.
  account_public_address pq_dummy{};
  if (get_account_address_from_str_pq(pq_dummy, b_addr)) { printf("FAIL: classic address wrongly parsed as BQ...\n"); return false; }

  printf("PASS: classic B... address parse + round-trip, is_pq()=false\n");
  return true;
}

static bool test_pq_bq_address()
{
  // Build a BQ... address: real Ed25519 spend/view keys + a Kyber768 public key.
  account_public_address addr{};
  crypto::secret_key sec;
  crypto::generate_keys(addr.m_spend_public_key, sec);
  crypto::generate_keys(addr.m_view_public_key, sec);

  crypto::pqc::pq_public_key pk; crypto::pqc::pq_secret_key sk;
  if (!crypto::pqc::pqc_keygen(pk, sk)) { printf("FAIL: pqc_keygen\n"); return false; }
  std::array<uint8_t, 1184> kpk;
  memcpy(kpk.data(), pk.kyber768_pk, crypto::pqc::KYBER768_PUBLIC_KEY_BYTES);
  addr.pq_kyber_pk = kpk;

  if (!addr.is_pq()) { printf("FAIL: built address is_pq()=false\n"); return false; }

  const std::string bq_addr = get_account_address_as_str_pq(MAINNET, addr);
  printf("BQ address (len %zu, leading %.4s...): %.24s...\n", bq_addr.size(), bq_addr.c_str(), bq_addr.c_str());
  // NB: the leading characters are determined by base58 encode_addr mechanics over the
  // full (tag||payload) blob, NOT just the numeric prefix; 0x3C11 over a 1184-byte
  // payload does not render as literal "BQ". The parser keys on the NUMERIC prefix,
  // which is what the round-trip below verifies. Cosmetic prefix tuning is part of the
  // remaining Step 5 address-format finalization.

  account_public_address parsed{};
  if (!get_account_address_from_str_pq(parsed, bq_addr)) { printf("FAIL: BQ parse failed\n"); return false; }
  if (!parsed.is_pq()) { printf("FAIL: parsed BQ is_pq()=false\n"); return false; }
  if (parsed.m_spend_public_key != addr.m_spend_public_key ||
      parsed.m_view_public_key  != addr.m_view_public_key) { printf("FAIL: BQ Ed25519 keys mismatch\n"); return false; }
  if (memcmp(parsed.pq_kyber_pk->data(), kpk.data(), crypto::pqc::KYBER768_PUBLIC_KEY_BYTES) != 0) { printf("FAIL: BQ Kyber768 key mismatch\n"); return false; }

  // A BQ... address must NOT parse via the classic path with the BQ prefix.
  address_parse_info info{};
  if (get_account_address_from_str(info, MAINNET, bq_addr)) { printf("FAIL: BQ address wrongly parsed as classic\n"); return false; }

  printf("PASS: BQ... address encode + parse round-trip (spend/view + Kyber768 1184B)\n");
  return true;
}

int main()
{
  bool ok = true;
  ok &= test_classic_b_address();
  ok &= test_pq_bq_address();
  printf("\nRESULT: %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
