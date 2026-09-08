// HIDERING Phase 5 — cold-signing coverage for BQ (post-quantum) accounts.
//
// Standalone (built like pqc_test.cpp / pq_keygen_test.cpp).
//
// Two things are asserted here, and they are not the same thing:
//
//  (1) account_boost_serialization.h round-trips the BQ material. Archiving an
//      account_keys used to drop pq_keys and pq_dilithium on the floor, so a BQ
//      account restored from such an archive could neither scan nor spend. The
//      serializer is versioned: version 0 archives still stop after the Ed25519
//      keys. NB this serializer has no caller in-tree today (verified by symbol
//      inspection); the test pins the contract so it cannot rot before it does.
//
//  (2) THE REAL COLD-SIGNING GAP, demonstrated rather than asserted-away: the
//      unsigned-tx file drops the PQ fields of every input and output.
//      tx_source_entry::{is_pq,pq_ss} and tx_destination_entry::is_pq are not in
//      their BEGIN_SERIALIZE_OBJECT lists, and tx_destination_entry::addr goes
//      through account_public_address's binary serializer, which deliberately
//      omits pq_kyber_pk to keep B... addresses byte-identical on the wire. So a
//      BQ spend exported to an offline signer arrives looking like a classic ring
//      spend to a classic recipient. The test asserts this is STILL the current
//      behaviour, so that whoever fixes it gets a failing test telling them the
//      contract moved rather than silence.
#include <cstdio>
#include <cstring>
#include <sstream>

#include <boost/archive/portable_binary_iarchive.hpp>
#include <boost/archive/portable_binary_oarchive.hpp>

#include "crypto/crypto.h"
#include "crypto/pqc.h"
#include "cryptonote_basic/account.h"
#include "cryptonote_basic/account_boost_serialization.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "serialization/crypto.h"
#include "serialization/string.h"
#include "serialization/pair.h"
#include "serialization/tuple.h"
#include "serialization/containers.h"
#include "serialization/binary_utils.h"

using namespace cryptonote;

// (1) export -> import of a BQ account keeps every piece of PQ material.
static bool test_account_archive_round_trip()
{
  account_base acc;
  acc.generate();
  if (!generate_pq_keys(acc.get_keys_nonconst())) { printf("FAIL: generate_pq_keys\n"); return false; }
  const account_keys &orig = acc.get_keys();
  if (!orig.pq_keys || !orig.pq_dilithium) { printf("FAIL: BQ account missing PQ material (test setup)\n"); return false; }

  std::stringstream ss;
  {
    boost::archive::portable_binary_oarchive ar(ss);
    account_keys copy = orig;
    ar << copy;
  }

  account_keys restored{};
  {
    std::stringstream in(ss.str());
    boost::archive::portable_binary_iarchive ar(in);
    ar >> restored;
  }

  if (restored.m_spend_secret_key != orig.m_spend_secret_key ||
      restored.m_view_secret_key != orig.m_view_secret_key)
  { printf("FAIL: Ed25519 secrets lost in the archive round-trip\n"); return false; }

  if (!restored.pq_keys) { printf("FAIL: pq_keys (ML-KEM-768 decapsulation key) dropped by the archive\n"); return false; }
  if (memcmp(restored.pq_keys->kyber_sk, orig.pq_keys->kyber_sk, crypto::pqc::ML_KEM_768_SECRET_KEY_BYTES) != 0 ||
      memcmp(restored.pq_keys->kyber_pk, orig.pq_keys->kyber_pk, crypto::pqc::ML_KEM_768_PUBLIC_KEY_BYTES) != 0)
  { printf("FAIL: ML-KEM-768 keypair corrupted by the archive round-trip\n"); return false; }

  if (!restored.pq_dilithium) { printf("FAIL: pq_dilithium (ML-DSA-65 signing key) dropped by the archive\n"); return false; }
  if (memcmp(restored.pq_dilithium->dilithium_sk, orig.pq_dilithium->dilithium_sk, crypto::pqc::ML_DSA_65_SECRET_KEY_BYTES) != 0 ||
      memcmp(restored.pq_dilithium->dilithium_pk, orig.pq_dilithium->dilithium_pk, crypto::pqc::ML_DSA_65_PUBLIC_KEY_BYTES) != 0)
  { printf("FAIL: ML-DSA-65 keypair corrupted by the archive round-trip\n"); return false; }

  printf("PASS: account archive round-trips the BQ material (ML-KEM-768 + ML-DSA-65)\n");
  return true;
}

// The restored account must actually be USABLE for the two operations a cold
// signer performs on a BQ spend: decapsulating its own output, and signing.
static bool test_restored_account_can_sign_a_bq_spend()
{
  account_base acc;
  acc.generate();
  if (!generate_pq_keys(acc.get_keys_nonconst())) { printf("FAIL: generate_pq_keys\n"); return false; }

  std::stringstream ss;
  {
    boost::archive::portable_binary_oarchive ar(ss);
    account_keys copy = acc.get_keys();
    ar << copy;
  }
  account_keys cold{};
  {
    std::stringstream in(ss.str());
    boost::archive::portable_binary_iarchive ar(in);
    ar >> cold;
  }

  // A sender encapsulates to the account's published ML-KEM-768 address key...
  crypto::pqc::kyber_ciphertext ct{};
  crypto::pqc::kyber_shared_secret ss_sender{};
  if (!crypto::pqc::pqc_stealth_encaps(acc.get_keys().pq_keys->kyber_pk,
                                       crypto::pqc::ML_KEM_768_PUBLIC_KEY_BYTES, ct, ss_sender))
  { printf("FAIL: pqc_stealth_encaps\n"); return false; }

  // ...and the RESTORED (cold) account must decapsulate it with the key it got
  // out of the archive. This is the step a cold signer needs to rebuild the
  // per-output ML-DSA key of the output it is spending.
  crypto::pqc::kyber_shared_secret ss_cold{};
  if (!cold.pq_keys) { printf("FAIL: restored account has no decapsulation key\n"); return false; }
  if (!crypto::pqc::pqc_stealth_decaps(*cold.pq_keys, ct, ss_cold))
  { printf("FAIL: restored account failed to decapsulate\n"); return false; }
  if (memcmp(ss_cold.ss, ss_sender.ss, crypto::pqc::ML_KEM_768_SHARED_SECRET_BYTES) != 0)
  { printf("FAIL: restored account decapsulated the WRONG shared secret\n"); return false; }

  // And the account's persistent ML-DSA-65 key (audit C-1) must still sign, and
  // verify against the public half the archive carried.
  uint8_t msg[32];
  memset(msg, 0x5C, sizeof(msg));
  crypto::pqc::pq_tx_sig sig{};
  if (!crypto::pqc::pqc_tx_sign(msg, sizeof(msg),
                                cold.pq_dilithium->dilithium_sk, crypto::pqc::ML_DSA_65_SECRET_KEY_BYTES,
                                cold.pq_dilithium->dilithium_pk, crypto::pqc::ML_DSA_65_PUBLIC_KEY_BYTES,
                                sig))
  { printf("FAIL: restored account could not sign with its ML-DSA-65 key\n"); return false; }
  if (!crypto::pqc::pqc_tx_verify(msg, sizeof(msg), sig))
  { printf("FAIL: signature from the restored account does not verify\n"); return false; }

  printf("PASS: restored account decapsulates its own BQ output and signs with its ML-DSA-65 key\n");
  return true;
}

// (2) The unsigned-tx transfer format still loses the PQ fields. Documented as a
// live assertion so the day it is fixed, this test says so.
static bool test_transfer_format_still_drops_pq_fields()
{
  // --- input side ---
  tx_source_entry src{};
  src.amount = 42;
  src.real_output = 0;
  src.real_output_in_tx_index = 0;
  src.rct = false;
  src.outputs.push_back(std::make_pair(static_cast<uint64_t>(0), rct::ctkey{}));
  src.is_pq = true;
  crypto::pqc::kyber_shared_secret ss{};
  memset(ss.ss, 0xA5, sizeof(ss.ss));
  src.pq_ss = ss;

  std::string blob;
  if (!::serialization::dump_binary(src, blob)) { printf("FAIL: could not serialize tx_source_entry\n"); return false; }
  tx_source_entry src2{};
  if (!::serialization::parse_binary(blob, src2)) { printf("FAIL: could not parse tx_source_entry\n"); return false; }

  if (src2.is_pq || src2.pq_ss)
  {
    printf("NOTE: tx_source_entry now carries its PQ fields — cold-signing a BQ input may be\n"
           "      fixed; update this test and the cold-sign documentation.\n");
    return false;
  }

  // --- output side ---
  account_base recipient;
  recipient.generate();
  if (!generate_pq_keys(recipient.get_keys_nonconst())) { printf("FAIL: generate_pq_keys (recipient)\n"); return false; }
  tx_destination_entry dst(7, recipient.get_keys().m_account_address, false);
  dst.is_pq = true;
  if (!dst.addr.is_pq()) { printf("FAIL: destination address is not BQ (test setup)\n"); return false; }

  std::string dblob;
  if (!::serialization::dump_binary(dst, dblob)) { printf("FAIL: could not serialize tx_destination_entry\n"); return false; }
  tx_destination_entry dst2{};
  if (!::serialization::parse_binary(dblob, dst2)) { printf("FAIL: could not parse tx_destination_entry\n"); return false; }

  if (dst2.is_pq || dst2.addr.is_pq())
  {
    printf("NOTE: tx_destination_entry now carries its PQ fields — cold-signing to a BQ\n"
           "      recipient may be fixed; update this test and the cold-sign documentation.\n");
    return false;
  }

  printf("PASS (documents an OPEN gap): the transfer format drops is_pq/pq_ss on inputs and\n"
         "      is_pq/ML-KEM key on outputs — an offline signer cannot build a BQ spend from\n"
         "      an exported unsigned tx. Needs a transfer-file format decision.\n");
  return true;
}

int main()
{
  bool ok = true;
  ok &= test_account_archive_round_trip();
  ok &= test_restored_account_can_sign_a_bq_spend();
  ok &= test_transfer_format_still_drops_pq_fields();
  printf("\nRESULT: %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
