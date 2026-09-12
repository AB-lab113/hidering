// HIDERING Phase 5 Step 5 — smoke test: classic B... address parsing is unchanged,
// and the new BQ... (post-quantum, ML-KEM-768-carrying) address round-trips.
#include <cstdio>
#include <cstring>
#include <array>
#include "crypto/crypto.h"
#include "crypto/pqc.h"
#include "cryptonote_basic/account.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "common/base58.h"
#include <string>

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
  // Build a BQ... address: real Ed25519 spend/view keys + a ML-KEM-768 public key.
  account_public_address addr{};
  crypto::secret_key sec;
  crypto::generate_keys(addr.m_spend_public_key, sec);
  crypto::generate_keys(addr.m_view_public_key, sec);

  crypto::pqc::pq_public_key pk; crypto::pqc::pq_secret_key sk;
  if (!crypto::pqc::pqc_keygen(pk, sk)) { printf("FAIL: pqc_keygen\n"); return false; }
  std::array<uint8_t, 1184> kpk;
  memcpy(kpk.data(), pk.kyber768_pk, crypto::pqc::ML_KEM_768_PUBLIC_KEY_BYTES);
  addr.pq_kyber_pk = kpk;
  // Spec 2e: a BQ address also publishes the commitment to its authorisation key.
  std::array<uint8_t, 32> commit{};
  for (size_t i = 0; i < commit.size(); ++i) commit[i] = (uint8_t)(0x5C + i);
  addr.pq_auth_commit = commit;
  addr.pq_auth_ver = ::config::CRYPTONOTE_PQ_ADDRESS_AUTH_VER;

  if (!addr.is_pq()) { printf("FAIL: built address is_pq()=false\n"); return false; }

  const std::string bq_addr = get_account_address_as_str_pq(MAINNET, addr);
  printf("BQ address (len %zu, leading %.4s...): %.24s...\n", bq_addr.size(), bq_addr.c_str(), bq_addr.c_str());
  // Step 6: the rendered address now begins with the mnemonic "BQ" — tag 62 plus a fixed
  // leading marker byte in the payload pin the first two base58 chars (the tag alone
  // cannot, see cryptonote_config.h). The parser keys on the NUMERIC prefix + payload
  // size + marker byte, which is what the round-trip below verifies.

  account_public_address parsed{};
  if (!get_account_address_from_str_pq(parsed, bq_addr)) { printf("FAIL: BQ parse failed\n"); return false; }
  if (!parsed.is_pq()) { printf("FAIL: parsed BQ is_pq()=false\n"); return false; }
  if (parsed.m_spend_public_key != addr.m_spend_public_key ||
      parsed.m_view_public_key  != addr.m_view_public_key) { printf("FAIL: BQ Ed25519 keys mismatch\n"); return false; }
  if (memcmp(parsed.pq_kyber_pk->data(), kpk.data(), crypto::pqc::ML_KEM_768_PUBLIC_KEY_BYTES) != 0) { printf("FAIL: BQ ML-KEM-768 key mismatch\n"); return false; }
  if (!parsed.pq_auth_commit || *parsed.pq_auth_commit != commit) { printf("FAIL: BQ authorisation commitment mismatch\n"); return false; }
  if (parsed.pq_auth_ver != ::config::CRYPTONOTE_PQ_ADDRESS_AUTH_VER) { printf("FAIL: BQ auth version mismatch\n"); return false; }

  // Spec 2e (T4): an address whose version byte we do not know must be REFUSED, not parsed
  // under assumptions. The version sits at payload offset 1, right after the marker, so a
  // parser sees it before interpreting any key.
  {
    std::string blob;
    uint64_t prefix = 0;
    if (!tools::base58::decode_addr(bq_addr, prefix, blob)) { printf("FAIL: cannot decode for the version test\n"); return false; }
    blob[1] = (char)0x7E; // an unallocated authorisation version
    const std::string bad = tools::base58::encode_addr(prefix, blob);
    account_public_address dummy{};
    if (get_account_address_from_str_pq(dummy, bad)) { printf("FAIL: an unknown auth version was accepted\n"); return false; }
    address_parse_info binfo{};
    if (get_account_address_from_str(binfo, MAINNET, bad)) { printf("FAIL: generic parser accepted an unknown auth version\n"); return false; }
  }

  // Since Phase 5 A4 the generic entry point ROUTES a BQ... address to the BQ parser
  // (that is what lets `transfer BQ...` work); it must come back flagged is_pq and
  // NOT as a subaddress, even though prefix 62 is shared with subaddresses.
  address_parse_info info{};
  if (!get_account_address_from_str(info, MAINNET, bq_addr))
  { printf("FAIL: BQ address rejected by the generic parser\n"); return false; }
  if (!info.address.is_pq() || info.is_subaddress)
  { printf("FAIL: BQ address misclassified by the generic parser (is_pq=%d, is_subaddress=%d)\n",
           (int)info.address.is_pq(), (int)info.is_subaddress); return false; }

  printf("PASS: BQ... address encode + parse round-trip (spend/view + ML-KEM-768 1184B)\n");
  return true;
}

// Spec 2e §1.2 — the auth_ver byte lands in the FIRST 8-byte base58 block
// (varint(62) | marker | auth_ver | spend[0..4]), so it can move the rendered prefix. Decision 4
// had to verify the subaddress marker 0x35 the same way before allocating it; the same proof is
// required for every (marker, auth_ver) pair, and it belongs in the test suite rather than in a
// one-off script. Both markers, many random payloads.
static bool test_bq_prefix_is_stable()
{
  const uint8_t markers[2] = { ::config::CRYPTONOTE_PQ_ADDRESS_MARKER, ::config::CRYPTONOTE_PQ_SUBADDRESS_MARKER };
  const size_t N = 2000;
  for (int m = 0; m < 2; ++m)
  {
    for (size_t n = 0; n < N; ++n)
    {
      account_public_address a{};
      crypto::secret_key s1, s2;
      crypto::generate_keys(a.m_spend_public_key, s1);
      crypto::generate_keys(a.m_view_public_key, s2);
      std::array<uint8_t, crypto::pqc::ML_KEM_768_PUBLIC_KEY_BYTES> k{};
      crypto::rand(k.size(), k.data());
      a.pq_kyber_pk = k;
      std::array<uint8_t, 32> c{};
      crypto::rand(c.size(), c.data());
      a.pq_auth_commit = c;
      a.pq_auth_ver = ::config::CRYPTONOTE_PQ_ADDRESS_AUTH_VER;
      const std::string str = get_account_address_as_str_pq(MAINNET, a,
          markers[m] == ::config::CRYPTONOTE_PQ_SUBADDRESS_MARKER);
      if (str.size() < 2 || str[0] != 'B' || str[1] != 'Q')
      {
        printf("FAIL: marker 0x%02x + auth_ver 0x%02x rendered \"%.4s\" instead of \"BQ\" (payload %zu)\n",
               (unsigned)markers[m], (unsigned)::config::CRYPTONOTE_PQ_ADDRESS_AUTH_VER, str.c_str(), n);
        return false;
      }
    }
  }
  printf("PASS: marker 0x%02x/0x%02x + auth_ver 0x%02x render a stable \"BQ\" prefix over %zu random payloads each\n",
         (unsigned)markers[0], (unsigned)markers[1], (unsigned)::config::CRYPTONOTE_PQ_ADDRESS_AUTH_VER, N);
  return true;
}

int main()
{
  bool ok = true;
  ok &= test_classic_b_address();
  ok &= test_pq_bq_address();
  ok &= test_bq_prefix_is_stable();
  printf("\nRESULT: %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
