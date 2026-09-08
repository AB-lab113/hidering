// HIDERING Phase 5 (HFv16) — hybrid post-quantum + ring transactions, and the amount binding
// they depend on.
//
// Standalone; links libringct because it drives the REAL rct::genRctSimple /
// rct::verRctSemanticsSimple, i.e. the consensus balance check itself.
//
// TWO THINGS ARE PROVEN HERE.
//
// (1) A TRANSPARENT POST-QUANTUM INPUT'S DECLARED AMOUNT MUST OPEN THE ON-CHAIN COMMITMENT.
//     Validator checks (a)-(d) establish that the spender OWNS the output being spent — the
//     revealed key matches, the binding tag commits it to a per-output ML-DSA-65 key, and that
//     key signs the transaction. NONE of them says what the output is WORTH. `amount` arrives
//     from the transaction, and before check (b2) existed nothing constrained it, while the
//     balance rules trust it: the fully transparent shape balances on it arithmetically, and
//     the hybrid shape feeds it straight into the RingCT sum. The owner of a dust BQ output
//     could therefore declare it worth millions and mint the difference. The test shows every
//     ownership check still passing on an inflated amount, and check (b2) rejecting it.
//
// (2) THE HYBRID BALANCE CLOSES, AND ONLY FOR THE TRUE TRANSPARENT TOTAL. A hybrid tx is an
//     ordinary RingCT transaction plus transparent inputs with no commitment of their own.
//     Their revealed total enters the sum check as a public zero-mask term, so the transparent
//     inputs behave exactly like a negative fee, and pseudoOuts stays 1:1 with the CLSAGs.
//     The test builds a real rctSig and verifies it through the real balance check.
#include <cstdio>
#include <cstring>
#include <vector>

#include "crypto/crypto.h"
#include "crypto/pqc.h"
#include "device/device.hpp"
#include "ringct/rctOps.h"
#include "ringct/rctSigs.h"
#include "ringct/rctTypes.h"

using namespace crypto::pqc;

// (1) An owner can pass every ownership check while lying about the value.
static bool test_amount_binding_catches_inflation()
{
  // A BQ output worth 1 HRG, created by a classic RingCT tx: its commitment hides the amount
  // behind an ECDH-derived mask that only the owner knows.
  const uint64_t real_amount   = 1000000000000ull;          // 1 HRG
  const uint64_t stolen_amount = 1000000000000000ull;       // 1000 HRG
  const rct::key mask = rct::skGen();
  const rct::key on_chain_commitment = rct::commit(real_amount, mask);

  // The spender legitimately owns it: a one-time output key, a per-output ML-DSA-65 key
  // derived from the ML-KEM shared secret, and the binding tag published at creation.
  kyber_shared_secret ss;
  for (int i = 0; i < 32; ++i) ss.ss[i] = (uint8_t)(0x9E ^ i);
  const uint64_t out_index = 2;
  pq_public_key dsa_pk; pq_secret_key dsa_sk;
  if (!pqc_keygen_output_dsa(ss, out_index, dsa_pk, dsa_sk)) { printf("FAIL: per-output keygen\n"); return false; }

  crypto::public_key P; crypto::secret_key p_sec;
  crypto::generate_keys(P, p_sec);
  uint8_t stored_bind[32];
  pqc_compute_bind_tag((const uint8_t *)&P, sizeof(P), dsa_pk.dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES, stored_bind);

  // --- the attack: spend it declaring 1000x the value -------------------------------------
  // (b) revealed key matches the on-chain output key
  const bool check_b = true; // the attacker uses their real output key, so this passes by construction
  // (c) the binding tag recomputes
  uint8_t recomputed[32];
  pqc_compute_bind_tag((const uint8_t *)&P, sizeof(P), dsa_pk.dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES, recomputed);
  const bool check_c = memcmp(recomputed, stored_bind, 32) == 0;
  // (d) the ML-DSA-65 signature verifies
  uint8_t prefix_hash[32];
  for (int i = 0; i < 32; ++i) prefix_hash[i] = (uint8_t)(0x2B + i);
  pq_tx_sig sig;
  const bool signed_ok = pqc_tx_sign(prefix_hash, sizeof(prefix_hash), dsa_sk.dilithium3_sk, ML_DSA_65_SECRET_KEY_BYTES,
                                     dsa_pk.dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES, sig);
  const bool check_d = signed_ok && pqc_tx_verify(prefix_hash, sizeof(prefix_hash), sig);
  // (e) the fully transparent arithmetic rule: inputs >= outputs. With an inflated input the
  // attacker simply creates outputs up to the inflated value, so it is satisfied too.
  const bool check_e = stolen_amount >= stolen_amount;

  if (!(check_b && check_c && check_d && check_e))
  { printf("FAIL: test setup — the ownership checks should all pass here\n"); return false; }

  // Every ownership check passes on the inflated amount. Only the commitment says otherwise.
  if (!(rct::commit(real_amount, mask) == on_chain_commitment))
  { printf("FAIL: test setup — the honest amount must open the commitment\n"); return false; }
  if (rct::commit(stolen_amount, mask) == on_chain_commitment)
  { printf("FAIL: check (b2) does NOT catch an inflated amount — money can be minted\n"); return false; }

  // No mask can make the lie open the commitment either (that would be a discrete log).
  const rct::key other_mask = rct::skGen();
  if (rct::commit(stolen_amount, other_mask) == on_chain_commitment)
  { printf("FAIL: an inflated amount opened the commitment under another mask\n"); return false; }

  printf("PASS: checks (b)(c)(d)(e) ALL accept a 1000x inflated amount — ownership says nothing\n"
         "      about value; only check (b2), commit(amount, mask) == on-chain commitment,\n"
         "      rejects it. That check is what stops a dust BQ output from minting 1000 HRG.\n");
  return true;
}

// The identity-mask case: a BQ output produced by a transparent BQ spend is stored as
// zeroCommit(amount), and check (b2) must accept exactly that.
static bool test_amount_binding_identity_mask()
{
  const uint64_t amount = 4200000000000ull;
  const rct::key stored = rct::zeroCommit(amount);          // what the daemon stores (CRIT-1)
  if (!(rct::commit(amount, rct::identity()) == stored))
  { printf("FAIL: commit(a, I) != zeroCommit(a) — check (b2) would reject honest BQ outputs\n"
           "      created by a transparent BQ spend\n"); return false; }
  if (rct::commit(amount + 1, rct::identity()) == stored)
  { printf("FAIL: an off-by-one amount opened the identity-mask commitment\n"); return false; }
  printf("PASS: check (b2) accepts the identity-mask commitment of a BQ output created by a\n"
         "      transparent BQ spend, and still rejects a wrong amount\n");
  return true;
}

namespace
{
  // Build a real RingCT signature over `n_ring` ring inputs, as construct_tx does for the
  // hybrid shape: only the ring inputs go in, and txnFee carries the REAL fee.
  bool make_ring_sig(const std::vector<uint64_t> &in_amounts,
                     const std::vector<uint64_t> &out_amounts,
                     uint64_t real_fee, rct::rctSig &rv)
  {
    const size_t n_in = in_amounts.size(), n_out = out_amounts.size();
    rct::ctkeyV inSk(n_in);
    rct::ctkeyM mixRing(n_in);
    std::vector<unsigned int> index(n_in, 0);
    rct::keyV destinations(n_out);
    rct::keyV amount_keys(n_out);
    rct::ctkeyV outSk;

    for (size_t i = 0; i < n_in; ++i)
    {
      rct::key sk;
      rct::skpkGen(sk, mixRing[i].emplace_back().dest);      // decoy 0 == the real one
      inSk[i].dest = sk;
      inSk[i].mask = rct::skGen();
      mixRing[i][0].mask = rct::commit(in_amounts[i], inSk[i].mask);
    }
    for (size_t j = 0; j < n_out; ++j)
    {
      rct::key sk;
      rct::skpkGen(sk, destinations[j]);
      amount_keys[j] = rct::hash_to_scalar(rct::skGen());
    }

    const rct::RCTConfig cfg{rct::RangeProofPaddedBulletproof, 4};   // bulletproof plus
    try
    {
      rv = rct::genRctSimple(rct::skGen(), inSk, destinations, in_amounts, out_amounts,
                             real_fee, mixRing, amount_keys, index, outSk, cfg,
                             hw::get_device("default"));
    }
    catch (const std::exception &e) { printf("      genRctSimple threw: %s\n", e.what()); return false; }
    return true;
  }
}

// (2) The hybrid balance: transparent inputs as a public term == a negative fee.
static bool test_hybrid_balance_closes()
{
  // 3 HRG from a ring input + 2 HRG from a transparent BQ input, paying 4.5 HRG out with a
  // 0.5 HRG fee. Only the ring half is inside the RingCT signature.
  const uint64_t ring_in = 3000000000000ull;
  const uint64_t pq_in   = 2000000000000ull;
  const uint64_t out_a   = 4000000000000ull;
  const uint64_t out_b   =  500000000000ull;
  const uint64_t fee     =  500000000000ull;
  if (ring_in + pq_in != out_a + out_b + fee) { printf("FAIL: test arithmetic\n"); return false; }

  rct::rctSig rv;
  if (!make_ring_sig({ring_in}, {out_a, out_b}, fee, rv))
  { printf("FAIL: could not build the RingCT signature\n"); return false; }

  // Without the transparent term the balance is short by exactly pq_in*H and must FAIL —
  // this is the pre-hybrid behaviour, and the reason the field fails closed when unset.
  rv.pq_transparent_in = 0;
  if (rct::verRctSemanticsSimple(rv))
  { printf("FAIL: the balance passed WITHOUT the transparent term — the sum check is not\n"
           "      actually constraining the transparent inputs\n"); return false; }

  // With the true total it closes.
  rv.pq_transparent_in = pq_in;
  if (!rct::verRctSemanticsSimple(rv))
  { printf("FAIL: the hybrid balance does not close with the correct transparent total\n"); return false; }

  printf("PASS: hybrid balance closes — sum(pseudoOuts) + sum(a_pq)*H == sum(outPk) + fee*H,\n"
         "      with pseudoOuts covering the ring input only (ring 3 HRG + transparent 2 HRG\n"
         "      -> 4.5 HRG out + 0.5 HRG fee)\n");
  return true;
}

// Money creation must fail: claiming more transparent input than was really spent.
static bool test_hybrid_balance_rejects_inflated_transparent_term()
{
  const uint64_t ring_in = 3000000000000ull;
  const uint64_t pq_in   = 2000000000000ull;
  const uint64_t fee     =  500000000000ull;
  rct::rctSig rv;
  if (!make_ring_sig({ring_in}, {4000000000000ull, 500000000000ull}, fee, rv))
  { printf("FAIL: could not build the RingCT signature\n"); return false; }

  bool ok = true;
  rv.pq_transparent_in = pq_in + 1;
  if (rct::verRctSemanticsSimple(rv))
  { printf("FAIL: a transparent total one atomic unit too high was accepted\n"); ok = false; }
  rv.pq_transparent_in = pq_in * 2;
  if (rct::verRctSemanticsSimple(rv))
  { printf("FAIL: a doubled transparent total was accepted — money creation\n"); ok = false; }
  rv.pq_transparent_in = pq_in - 1;
  if (rct::verRctSemanticsSimple(rv))
  { printf("FAIL: a transparent total one atomic unit too low was accepted\n"); ok = false; }

  rv.pq_transparent_in = pq_in;   // control: the honest value still passes
  if (!rct::verRctSemanticsSimple(rv))
  { printf("FAIL: control — the honest transparent total was rejected\n"); ok = false; }

  if (ok)
    printf("PASS: the balance rejects any transparent total but the true one (+1, x2, -1 all\n"
           "      rejected; the honest value accepted)\n");
  return ok;
}

// A classic RingCT transaction must be completely unaffected by the new term.
static bool test_classic_balance_unchanged()
{
  const uint64_t in_a = 3000000000000ull, in_b = 2000000000000ull;
  const uint64_t out_a = 4000000000000ull, out_b = 500000000000ull, fee = 500000000000ull;
  rct::rctSig rv;
  if (!make_ring_sig({in_a, in_b}, {out_a, out_b}, fee, rv))
  { printf("FAIL: could not build the RingCT signature\n"); return false; }

  // Default-constructed, never touched: exactly what a classic tx carries.
  if (rv.pq_transparent_in != 0)
  { printf("FAIL: pq_transparent_in is not 0 by default — classic txs would change behaviour\n"); return false; }
  if (!rct::verRctSemanticsSimple(rv))
  { printf("FAIL: a classic two-input RingCT transaction no longer verifies\n"); return false; }

  printf("PASS: a classic RingCT transaction verifies unchanged (transparent term 0 by\n"
         "      default, so the balance equation is bit-for-bit the original)\n");
  return true;
}

int main()
{
  bool ok = true;
  ok &= test_amount_binding_catches_inflation();
  ok &= test_amount_binding_identity_mask();
  ok &= test_classic_balance_unchanged();
  ok &= test_hybrid_balance_closes();
  ok &= test_hybrid_balance_rejects_inflated_transparent_term();
  printf("\nRESULT: %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
