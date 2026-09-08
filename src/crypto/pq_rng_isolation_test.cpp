// HIDERING Phase 5 — regression test for audit finding MOYEN-4 (7 Sep 2026):
// the deterministic SHAKE256 RNG that backs pqc_keygen_from_seed must be visible
// to the deriving thread and to NOBODY else.
//
// Standalone (built like pqc_test.cpp / pq_keygen_test.cpp).
//
// THE BUG THIS CATCHES. pqc_keygen_from_seed has to drive liboqs' randombytes
// hook, because liboqs offers no derandomised keygen for ML-DSA-65. The original
// implementation installed the deterministic stream on that hook — which is
// PROCESS-GLOBAL — and guarded it with a mutex that only serialised
// keygen_from_seed against itself. Any other thread inside liboqs during that
// window (a concurrent pqc_keygen, a pqc_kem_encaps, a second wallet-rpc request)
// drew its "random" bytes out of the caller's deterministic stream. Both sides
// break, silently:
//
//   * the concurrent operation gets key material derived from someone else's
//     wallet spend key instead of real entropy, and
//   * the bytes it steals desynchronise the stream, so the BQ address derived
//     from a given seed is no longer reproducible — which destroys M-4
//     (restore-from-seed must reproduce the BQ address).
//
// Both symptoms are asserted below, under real concurrency. Run against the
// pre-fix pqc.cpp this test fails on both counts; against the thread-local
// dispatch it passes.
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "crypto/pqc.h"

using namespace crypto::pqc;

namespace
{
  // How long the deterministic threads hammer keygen_from_seed. Each iteration is
  // one full ML-DSA-65 + ML-KEM-768 keygen (~sub-millisecond), and the racy window
  // is the whole OQS_SIG_keypair call, so a few hundred rounds against four
  // competing threads makes an interleaving essentially certain.
  constexpr int DET_ITERATIONS = 250;
  constexpr int DET_THREADS = 2;
  constexpr int RAND_THREADS = 4;

  std::string pk_fingerprint(const pq_public_key &pk)
  {
    // The ML-DSA half is the one derived through the randombytes hook, so it is
    // the half that shows the corruption.
    return std::string(reinterpret_cast<const char *>(pk.dilithium3_pk), 32);
  }

  void fill_seed(uint8_t *seed, size_t len, uint8_t tag)
  {
    for (size_t i = 0; i < len; ++i)
      seed[i] = static_cast<uint8_t>(tag + i);
  }
}

// (1) determinism must survive concurrency, and (2) threads asking liboqs for real
// randomness must never receive stream bytes (which would show up as repeats).
static bool test_det_rng_is_thread_isolated()
{
  // Reference keys, derived single-threaded with nothing else running.
  uint8_t seeds[DET_THREADS][32];
  pq_public_key ref_pk[DET_THREADS];
  pq_secret_key ref_sk[DET_THREADS];
  for (int t = 0; t < DET_THREADS; ++t)
  {
    fill_seed(seeds[t], sizeof(seeds[t]), static_cast<uint8_t>(0x11 * (t + 1)));
    if (!pqc_keygen_from_seed(seeds[t], sizeof(seeds[t]), ref_pk[t], ref_sk[t]))
    { printf("FAIL: reference pqc_keygen_from_seed failed (test setup)\n"); return false; }
  }

  std::atomic<bool> det_running{true};
  std::atomic<int> det_mismatches{0};
  std::atomic<int> keygen_failures{0};

  std::vector<std::thread> threads;

  // Deterministic threads: same seed, over and over, while the process is busy.
  for (int t = 0; t < DET_THREADS; ++t)
  {
    threads.emplace_back([&, t]() {
      for (int i = 0; i < DET_ITERATIONS; ++i)
      {
        pq_public_key pk; pq_secret_key sk;
        if (!pqc_keygen_from_seed(seeds[t], sizeof(seeds[t]), pk, sk)) { ++keygen_failures; continue; }
        if (memcmp(pk.dilithium3_pk, ref_pk[t].dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES) != 0
            || memcmp(sk.dilithium3_sk, ref_sk[t].dilithium3_sk, ML_DSA_65_SECRET_KEY_BYTES) != 0
            || memcmp(pk.kyber768_pk, ref_pk[t].kyber768_pk, ML_KEM_768_PUBLIC_KEY_BYTES) != 0)
          ++det_mismatches;
      }
    });
  }

  // Randomness threads: everything else in the codebase that pulls entropy out of
  // liboqs — fresh keygens and KEM encapsulations — running at the same time.
  std::mutex seen_mu;
  std::set<std::string> seen;
  std::atomic<int> rand_total{0};
  std::atomic<int> rand_repeats{0};
  std::atomic<int> rand_matched_reference{0};

  for (int t = 0; t < RAND_THREADS; ++t)
  {
    threads.emplace_back([&]() {
      while (det_running.load(std::memory_order_relaxed))
      {
        pq_public_key pk; pq_secret_key sk;
        if (!pqc_keygen(pk, sk)) { ++keygen_failures; continue; }

        // A fresh keypair must never reproduce a seed-derived one.
        for (int r = 0; r < DET_THREADS; ++r)
          if (memcmp(pk.dilithium3_pk, ref_pk[r].dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES) == 0)
            ++rand_matched_reference;

        const std::string fp = pk_fingerprint(pk);
        {
          std::lock_guard<std::mutex> lock(seen_mu);
          if (!seen.insert(fp).second)
            ++rand_repeats;
        }
        ++rand_total;

        // Exercise the encapsulation path too: it is the one a wallet hits per BQ
        // output, i.e. the realistic concurrent consumer.
        kyber_ciphertext ct; kyber_shared_secret ss;
        if (!pqc_kem_encaps(pk, ct, ss)) ++keygen_failures;
      }
    });
  }

  for (int t = 0; t < DET_THREADS; ++t) threads[t].join();
  det_running.store(false, std::memory_order_relaxed);
  for (size_t t = DET_THREADS; t < threads.size(); ++t) threads[t].join();

  bool ok = true;
  if (keygen_failures.load() != 0)
  { printf("FAIL: %d liboqs operation(s) failed outright\n", keygen_failures.load()); ok = false; }
  if (det_mismatches.load() != 0)
  { printf("FAIL: %d/%d seed-derived keypairs differed from the reference — the deterministic\n"
           "      stream was consumed by another thread (M-4 broken under concurrency)\n",
           det_mismatches.load(), DET_THREADS * DET_ITERATIONS); ok = false; }
  if (rand_repeats.load() != 0)
  { printf("FAIL: %d/%d 'random' keypairs repeated — entropy came from a deterministic stream\n",
           rand_repeats.load(), rand_total.load()); ok = false; }
  if (rand_matched_reference.load() != 0)
  { printf("FAIL: %d 'random' keypairs equalled a seed-derived reference key — a concurrent\n"
           "      caller was handed key material derived from another wallet's spend key\n",
           rand_matched_reference.load()); ok = false; }
  if (rand_total.load() < 16)
  { printf("FAIL: only %d concurrent random keygens ran — the race window was never exercised\n",
           rand_total.load()); ok = false; }

  if (ok)
    printf("PASS: deterministic RNG is thread-isolated (%d seed derivations all reproducible, "
           "%d concurrent random keygens all distinct)\n",
           DET_THREADS * DET_ITERATIONS, rand_total.load());
  return ok;
}

// The hook must also be inert once the derivation returns: a keygen issued right
// after a seed-derived one, on the SAME thread, has to be random again.
static bool test_stream_unbound_after_return()
{
  uint8_t seed[32];
  fill_seed(seed, sizeof(seed), 0x5A);
  pq_public_key pk0; pq_secret_key sk0;
  if (!pqc_keygen_from_seed(seed, sizeof(seed), pk0, sk0))
  { printf("FAIL: pqc_keygen_from_seed failed (test setup)\n"); return false; }

  pq_public_key a, b; pq_secret_key sa, sb;
  if (!pqc_keygen(a, sa) || !pqc_keygen(b, sb))
  { printf("FAIL: pqc_keygen failed (test setup)\n"); return false; }

  if (memcmp(a.dilithium3_pk, b.dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES) == 0)
  { printf("FAIL: two consecutive random keygens produced the same key — the deterministic\n"
           "      stream stayed bound after pqc_keygen_from_seed returned\n"); return false; }
  if (memcmp(a.dilithium3_pk, pk0.dilithium3_pk, ML_DSA_65_PUBLIC_KEY_BYTES) == 0)
  { printf("FAIL: a random keygen reproduced the seed-derived key\n"); return false; }

  printf("PASS: the deterministic stream is unbound as soon as the derivation returns\n");
  return true;
}

int main()
{
  bool ok = true;
  ok &= test_stream_unbound_after_return();
  ok &= test_det_rng_is_thread_isolated();
  printf("\nRESULT: %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
