// Copyright (c) 2026, The Hidering Project
//
// Phase 5 — Post-Quantum Cryptography operations (liboqs implementation).
//
// Thin, allocation-light wrappers over the Open Quantum Safe primitives:
//   * ML-DSA-65 (OQS_SIG_alg_ml_dsa_65) — signatures
//   * ML-KEM-768   (OQS_KEM_alg_ml_kem_768)   — key encapsulation
//
// Each call constructs and frees its OQS_SIG / OQS_KEM handle; these are cheap
// (no per-call key material) and keep the wrappers stateless and thread-safe.
// The compile-time sizes declared in pqc.h are cross-checked against the runtime
// liboqs object so a future liboqs bump that changed a size fails loudly here.

#include "pqc.h"
#include "hash.h" // crypto::cn_fast_hash (Monero-variant Keccak) for the PQ binding tag

#include <cstring>
#include <mutex>
#include <string>

#include <oqs/oqs.h>
#include <oqs/sha3.h> // incremental SHAKE256 API (not pulled in by oqs.h)

namespace
{
  // --- OQS entropy dispatch (deterministic keygen vs. real randomness) --------
  //
  // liboqs exposes no derandomised keygen for ML-DSA-65 — still true in 0.16.0,
  // whose OQS_SIG struct carries only `keypair` — so OQS_SIG_keypair pulls its
  // 32-byte seed through liboqs' PROCESS-GLOBAL `randombytes` hook. Deriving a BQ
  // account deterministically from the wallet spend key (M-4) therefore has to
  // drive that hook.
  //
  // audit MOYEN-4 (7 Sep 2026). The first implementation installed the
  // deterministic SHAKE256 stream ON the global hook for the duration of the
  // keygen and guarded it with a mutex. The mutex only serialised
  // pqc_keygen_from_seed against ITSELF; the hook stayed global for that window,
  // so any other thread that entered liboqs for real entropy meanwhile — a
  // concurrent pqc_keygen, a pqc_kem_encaps building an output, a second
  // wallet-rpc request — silently drew from the caller's deterministic stream.
  // Both halves fail silently: the concurrent operation gets key material
  // predictable from someone else's spend key, and the bytes it consumes
  // desynchronise the stream so the BQ address derived from a given seed stops
  // being reproducible (which is the whole point of M-4).
  //
  // The fix inverts ownership. The hook is installed EXACTLY ONCE per process and
  // never swapped again; it dispatches on a THREAD-LOCAL stream pointer. A thread
  // inside pqc_keygen_from_seed reads its own SHAKE256 stream; every other thread,
  // and that same thread outside the keygen, falls through to liboqs' own default
  // entropy. Threads cannot observe each other's state at all, so the mutex is
  // gone and two seeds can be derived concurrently.
  //
  // Regression test: src/crypto/pq_rng_isolation_test.cpp.

  // liboqs' default entropy sources. rand.c gives them external linkage but does
  // not declare them in <oqs/rand.h>; we mirror rand.c's own load-time selection
  // so that "not deterministic" means exactly what it meant before this hook
  // existed. A liboqs that renames them breaks the LINK, loudly — the same
  // fail-fast posture as the compile-time size cross-checks in this file.
  extern "C" void OQS_randombytes_system(uint8_t *random_array, size_t bytes_to_read);
#ifdef OQS_USE_OPENSSL
  extern "C" void OQS_randombytes_openssl(uint8_t *random_array, size_t bytes_to_read);
#endif

  void oqs_default_entropy(uint8_t *out, size_t len)
  {
#ifdef OQS_USE_OPENSSL
    OQS_randombytes_openssl(out, len); // OpenSSL RAND_bytes — liboqs' default in our build
#else
    OQS_randombytes_system(out, len);
#endif
  }

  // Non-null only on a thread currently inside pqc_keygen_from_seed.
  thread_local OQS_SHA3_shake256_inc_ctx *tl_det_rng_stream = nullptr;

  void hrg_oqs_randombytes(uint8_t *out, size_t len)
  {
    if (len == 0)
      return;
    OQS_SHA3_shake256_inc_ctx *stream = tl_det_rng_stream;
    if (stream != nullptr)
      OQS_SHA3_shake256_inc_squeeze(out, len, stream);
    else
      oqs_default_entropy(out, len);
  }

  // Every pqc_* entry point calls this before touching liboqs, so the global hook
  // pointer is written once, ordered by call_once against every later OQS read of
  // it: no data race, and no window in which the hook is anything else.
  void ensure_oqs_rng_installed()
  {
    static std::once_flag once;
    std::call_once(once, [] { OQS_randombytes_custom_algorithm(hrg_oqs_randombytes); });
  }

  // RAII binding of a deterministic stream to the CURRENT thread only. Restores the
  // previous binding (nullptr in practice — nesting is not used) so an early return
  // or a throw can never leave a dangling stream visible to later calls.
  struct scoped_det_rng
  {
    OQS_SHA3_shake256_inc_ctx *prev;
    explicit scoped_det_rng(OQS_SHA3_shake256_inc_ctx *stream) : prev(tl_det_rng_stream) { tl_det_rng_stream = stream; }
    ~scoped_det_rng() { tl_det_rng_stream = prev; }
    scoped_det_rng(const scoped_det_rng &) = delete;
    scoped_det_rng &operator=(const scoped_det_rng &) = delete;
  };

  // Domain-separated SHAKE256 expansion of a master seed into a fixed-length sub-seed.
  void derive_subseed(const char *domain, const uint8_t *seed, size_t seed_len,
                      uint8_t *out, size_t out_len)
  {
    OQS_SHA3_shake256_inc_ctx st;
    OQS_SHA3_shake256_inc_init(&st);
    OQS_SHA3_shake256_inc_absorb(&st, reinterpret_cast<const uint8_t *>(domain), std::strlen(domain));
    OQS_SHA3_shake256_inc_absorb(&st, seed, seed_len);
    OQS_SHA3_shake256_inc_finalize(&st);
    OQS_SHA3_shake256_inc_squeeze(out, out_len, &st);
    OQS_SHA3_shake256_inc_ctx_release(&st);
  }
}

namespace crypto
{
namespace pqc
{
  bool pqc_keygen(pq_public_key &pk, pq_secret_key &sk)
  {
    ensure_oqs_rng_installed();
    bool ok = false;
    OQS_SIG *sig = OQS_SIG_new(OQS_SIG_alg_ml_dsa_65);
    OQS_KEM *kem = OQS_KEM_new(OQS_KEM_alg_ml_kem_768);
    if (sig != nullptr && kem != nullptr
        && sig->length_public_key == ML_DSA_65_PUBLIC_KEY_BYTES
        && sig->length_secret_key == ML_DSA_65_SECRET_KEY_BYTES
        && kem->length_public_key == ML_KEM_768_PUBLIC_KEY_BYTES
        && kem->length_secret_key == ML_KEM_768_SECRET_KEY_BYTES)
    {
      ok = OQS_SIG_keypair(sig, pk.dilithium3_pk, sk.dilithium3_sk) == OQS_SUCCESS
        && OQS_KEM_keypair(kem, pk.kyber768_pk, sk.kyber768_sk) == OQS_SUCCESS;
    }
    if (sig != nullptr) OQS_SIG_free(sig);
    if (kem != nullptr) OQS_KEM_free(kem);
    return ok;
  }

  bool pqc_keygen_from_seed(const uint8_t *seed, size_t seed_len,
                            pq_public_key &pk, pq_secret_key &sk)
  {
    if (seed == nullptr || seed_len == 0)
      return false;

    ensure_oqs_rng_installed();

    // Domain-separated sub-seeds derived from the master seed (the wallet spend key).
    uint8_t kem_seed[ML_KEM_768_KEYPAIR_SEED_BYTES];
    uint8_t dsa_seed[ML_DSA_65_KEYGEN_SEED_BYTES];
    derive_subseed("HRG_PQ_KEM_v1", seed, seed_len, kem_seed, sizeof(kem_seed));
    derive_subseed("HRG_PQ_DSA_v1", seed, seed_len, dsa_seed, sizeof(dsa_seed));

    bool ok = false;
    OQS_SIG *sig = OQS_SIG_new(OQS_SIG_alg_ml_dsa_65);
    OQS_KEM *kem = OQS_KEM_new(OQS_KEM_alg_ml_kem_768);
    if (sig != nullptr && kem != nullptr
        && sig->length_public_key == ML_DSA_65_PUBLIC_KEY_BYTES
        && sig->length_secret_key == ML_DSA_65_SECRET_KEY_BYTES
        && kem->length_public_key == ML_KEM_768_PUBLIC_KEY_BYTES
        && kem->length_secret_key == ML_KEM_768_SECRET_KEY_BYTES
        && kem->length_keypair_seed == ML_KEM_768_KEYPAIR_SEED_BYTES
        && kem->keypair_derand != nullptr)
    {
      // ML-KEM-768: clean derandomised keygen straight from the 64-byte sub-seed.
      bool kem_ok = OQS_KEM_keypair_derand(kem, pk.kyber768_pk, sk.kyber768_sk, kem_seed) == OQS_SUCCESS;

      // ML-DSA-65: no derandomised keygen API → drive OQS_SIG_keypair with a
      // deterministic SHAKE256 RNG. The stream is bound to THIS THREAD ONLY
      // (audit MOYEN-4): concurrent OQS users on other threads keep drawing real
      // entropy, and nothing they do can consume bytes out of this stream.
      bool sig_ok = false;
      {
        OQS_SHA3_shake256_inc_ctx stream;
        OQS_SHA3_shake256_inc_init(&stream);
        OQS_SHA3_shake256_inc_absorb(&stream, dsa_seed, sizeof(dsa_seed));
        OQS_SHA3_shake256_inc_finalize(&stream);

        {
          // Unbind before releasing the context, so no later OQS_randombytes on
          // this thread can dereference a freed stream.
          scoped_det_rng bind(&stream);
          sig_ok = OQS_SIG_keypair(sig, pk.dilithium3_pk, sk.dilithium3_sk) == OQS_SUCCESS;
        }
        OQS_SHA3_shake256_inc_ctx_release(&stream);
      }
      ok = kem_ok && sig_ok;
    }
    if (sig != nullptr) OQS_SIG_free(sig);
    if (kem != nullptr) OQS_KEM_free(kem);

    // Scrub the derived sub-seeds off the stack.
    OQS_MEM_cleanse(kem_seed, sizeof(kem_seed));
    OQS_MEM_cleanse(dsa_seed, sizeof(dsa_seed));
    return ok;
  }

  bool pqc_sign(const pq_secret_key &sk, const uint8_t *msg, size_t msg_len,
                pq_signature &sig_out, size_t &sig_len)
  {
    ensure_oqs_rng_installed(); // ML-DSA signing is hedged → it reads randombytes
    bool ok = false;
    sig_len = 0;
    OQS_SIG *sig = OQS_SIG_new(OQS_SIG_alg_ml_dsa_65);
    if (sig != nullptr && sig->length_signature == ML_DSA_65_SIGNATURE_BYTES)
    {
      size_t produced = 0;
      if (OQS_SIG_sign(sig, sig_out.sig, &produced, msg, msg_len, sk.dilithium3_sk) == OQS_SUCCESS)
      {
        sig_len = produced;
        ok = true;
      }
    }
    if (sig != nullptr) OQS_SIG_free(sig);
    return ok;
  }

  bool pqc_verify(const pq_public_key &pk, const uint8_t *msg, size_t msg_len,
                  const pq_signature &sig_in, size_t sig_len)
  {
    bool ok = false;
    OQS_SIG *sig = OQS_SIG_new(OQS_SIG_alg_ml_dsa_65);
    // audit M-5: validate the runtime liboqs sizes and the caller-supplied signature
    // length before verifying, so a mismatched/truncated length can never reach
    // OQS_SIG_verify with an inconsistent buffer view.
    if (sig != nullptr
        && sig->length_signature == ML_DSA_65_SIGNATURE_BYTES
        && sig->length_public_key == ML_DSA_65_PUBLIC_KEY_BYTES
        && sig_len == ML_DSA_65_SIGNATURE_BYTES)
    {
      ok = OQS_SIG_verify(sig, msg, msg_len, sig_in.sig, sig_len, pk.dilithium3_pk) == OQS_SUCCESS;
    }
    if (sig != nullptr) OQS_SIG_free(sig);
    return ok;
  }

  bool pqc_kem_encaps(const pq_public_key &pk, kyber_ciphertext &ct,
                      kyber_shared_secret &ss)
  {
    ensure_oqs_rng_installed();
    bool ok = false;
    OQS_KEM *kem = OQS_KEM_new(OQS_KEM_alg_ml_kem_768);
    if (kem != nullptr
        && kem->length_ciphertext == ML_KEM_768_CIPHERTEXT_BYTES
        && kem->length_shared_secret == ML_KEM_768_SHARED_SECRET_BYTES)
    {
      ok = OQS_KEM_encaps(kem, ct.ct, ss.ss, pk.kyber768_pk) == OQS_SUCCESS;
    }
    if (kem != nullptr) OQS_KEM_free(kem);
    return ok;
  }

  bool pqc_kem_decaps(const pq_secret_key &sk, const kyber_ciphertext &ct,
                      kyber_shared_secret &ss)
  {
    bool ok = false;
    OQS_KEM *kem = OQS_KEM_new(OQS_KEM_alg_ml_kem_768);
    if (kem != nullptr && kem->length_shared_secret == ML_KEM_768_SHARED_SECRET_BYTES)
    {
      ok = OQS_KEM_decaps(kem, ss.ss, ct.ct, sk.kyber768_sk) == OQS_SUCCESS;
    }
    if (kem != nullptr) OQS_KEM_free(kem);
    return ok;
  }

  bool pqc_tx_sign(const uint8_t *tx_prefix_hash, size_t hash_len,
                   const uint8_t *sk, size_t sk_len,
                   const uint8_t *pk, size_t pk_len,
                   pq_tx_sig &out_sig)
  {
    ensure_oqs_rng_installed(); // hedged ML-DSA signing reads randombytes
    if (tx_prefix_hash == nullptr || sk == nullptr || pk == nullptr
        || sk_len != ML_DSA_65_SECRET_KEY_BYTES
        || pk_len != ML_DSA_65_PUBLIC_KEY_BYTES)
      return false;

    bool ok = false;
    OQS_SIG *sig = OQS_SIG_new(OQS_SIG_alg_ml_dsa_65);
    if (sig != nullptr
        && sig->length_signature == ML_DSA_65_SIGNATURE_BYTES
        && sig->length_secret_key == ML_DSA_65_SECRET_KEY_BYTES
        && sig->length_public_key == ML_DSA_65_PUBLIC_KEY_BYTES)
    {
      size_t produced = 0;
      if (OQS_SIG_sign(sig, out_sig.sig, &produced, tx_prefix_hash, hash_len, sk) == OQS_SUCCESS
          && produced == ML_DSA_65_SIGNATURE_BYTES)
      {
        std::memcpy(out_sig.pk, pk, ML_DSA_65_PUBLIC_KEY_BYTES);
        ok = true;
      }
    }
    if (sig != nullptr) OQS_SIG_free(sig);
    return ok;
  }

  bool pqc_tx_verify(const uint8_t *tx_prefix_hash, size_t hash_len,
                     const pq_tx_sig &sig_in)
  {
    if (tx_prefix_hash == nullptr)
      return false;

    bool ok = false;
    OQS_SIG *sig = OQS_SIG_new(OQS_SIG_alg_ml_dsa_65);
    // audit M-5: confirm the runtime liboqs signature/public-key sizes match the
    // compile-time constants before verifying the fixed-length blob.
    if (sig != nullptr
        && sig->length_signature == ML_DSA_65_SIGNATURE_BYTES
        && sig->length_public_key == ML_DSA_65_PUBLIC_KEY_BYTES)
    {
      ok = OQS_SIG_verify(sig, tx_prefix_hash, hash_len,
                          sig_in.sig, ML_DSA_65_SIGNATURE_BYTES, sig_in.pk) == OQS_SUCCESS;
    }
    if (sig != nullptr) OQS_SIG_free(sig);
    return ok;
  }

  bool pqc_stealth_encaps(const uint8_t *recipient_kyber_pk, size_t pk_len,
                          kyber_ciphertext &ct, kyber_shared_secret &ss)
  {
    if (recipient_kyber_pk == nullptr || pk_len != ML_KEM_768_PUBLIC_KEY_BYTES)
      return false;

    ensure_oqs_rng_installed();

    bool ok = false;
    OQS_KEM *kem = OQS_KEM_new(OQS_KEM_alg_ml_kem_768);
    if (kem != nullptr
        && kem->length_public_key == ML_KEM_768_PUBLIC_KEY_BYTES
        && kem->length_ciphertext == ML_KEM_768_CIPHERTEXT_BYTES
        && kem->length_shared_secret == ML_KEM_768_SHARED_SECRET_BYTES)
    {
      ok = OQS_KEM_encaps(kem, ct.ct, ss.ss, recipient_kyber_pk) == OQS_SUCCESS;
    }
    if (kem != nullptr) OQS_KEM_free(kem);
    return ok;
  }

  bool pqc_stealth_decaps(const pq_stealth_keys &keys, const kyber_ciphertext &ct,
                          kyber_shared_secret &ss)
  {
    bool ok = false;
    OQS_KEM *kem = OQS_KEM_new(OQS_KEM_alg_ml_kem_768);
    if (kem != nullptr
        && kem->length_secret_key == ML_KEM_768_SECRET_KEY_BYTES
        && kem->length_shared_secret == ML_KEM_768_SHARED_SECRET_BYTES)
    {
      ok = OQS_KEM_decaps(kem, ss.ss, ct.ct, keys.kyber_sk) == OQS_SUCCESS;
    }
    if (kem != nullptr) OQS_KEM_free(kem);
    return ok;
  }

  void pqc_compute_bind_tag(const uint8_t *real_output_key, size_t rk_len,
                            const uint8_t *dsa_pk, size_t pk_len,
                            uint8_t out_tag[32])
  {
    static const char domain[] = "HRG_PQ_BIND_v1";
    std::string buf;
    buf.reserve((sizeof(domain) - 1) + rk_len + pk_len);
    buf.append(domain, sizeof(domain) - 1);
    buf.append(reinterpret_cast<const char*>(real_output_key), rk_len);
    buf.append(reinterpret_cast<const char*>(dsa_pk), pk_len);
    crypto::hash h;
    crypto::cn_fast_hash(buf.data(), buf.size(), h); // Monero-variant Keccak-256
    std::memcpy(out_tag, &h, 32);
  }

  bool pqc_keygen_output_dsa(const kyber_shared_secret &ss, uint64_t output_index,
                             pq_public_key &pk, pq_secret_key &sk)
  {
    // sub-seed = "HRG_PQ_OUT_DSA_v1" || ss || index_le8 → fed to the deterministic keygen,
    // so sender and receiver derive the identical per-output ML-DSA-65 (and ML-KEM-768) pair.
    static const char domain[] = "HRG_PQ_OUT_DSA_v1";
    std::string seed;
    seed.reserve((sizeof(domain) - 1) + sizeof(ss.ss) + 8);
    seed.append(domain, sizeof(domain) - 1);
    seed.append(reinterpret_cast<const char*>(ss.ss), sizeof(ss.ss));
    for (int i = 0; i < 8; ++i)
      seed.push_back(static_cast<char>((output_index >> (8 * i)) & 0xff));
    const bool r = pqc_keygen_from_seed(reinterpret_cast<const uint8_t*>(seed.data()), seed.size(), pk, sk);
    if (!seed.empty()) std::memset(&seed[0], 0, seed.size());
    return r;
  }

  bool pqc_kem_keygen_subaddress(const uint8_t *root, size_t root_len,
                                 uint32_t major, uint32_t minor, pq_stealth_keys &out)
  {
    if (root == nullptr || root_len == 0)
      return false;

    ensure_oqs_rng_installed();

    // root || major_le4 || minor_le4, expanded under its own domain into the 64-byte d||z seed.
    std::string buf;
    buf.reserve(root_len + 8);
    buf.append(reinterpret_cast<const char*>(root), root_len);
    for (int n = 0; n < 4; ++n) buf.push_back(static_cast<char>((major >> (8 * n)) & 0xff));
    for (int n = 0; n < 4; ++n) buf.push_back(static_cast<char>((minor >> (8 * n)) & 0xff));
    uint8_t kem_seed[ML_KEM_768_KEYPAIR_SEED_BYTES];
    derive_subseed("HRG_BQ_SUBADDR_KEM_v1", reinterpret_cast<const uint8_t*>(buf.data()), buf.size(),
                   kem_seed, sizeof(kem_seed));
    OQS_MEM_cleanse(&buf[0], buf.size());

    bool ok = false;
    OQS_KEM *kem = OQS_KEM_new(OQS_KEM_alg_ml_kem_768);
    if (kem != nullptr
        && kem->length_public_key == ML_KEM_768_PUBLIC_KEY_BYTES
        && kem->length_secret_key == ML_KEM_768_SECRET_KEY_BYTES
        && kem->length_keypair_seed == ML_KEM_768_KEYPAIR_SEED_BYTES
        && kem->keypair_derand != nullptr)
    {
      // Pure function of the seed: no randombytes involved, so none of the MOYEN-4 hook
      // machinery is needed here (unlike the ML-DSA half of pqc_keygen_from_seed).
      ok = OQS_KEM_keypair_derand(kem, out.kyber_pk, out.kyber_sk, kem_seed) == OQS_SUCCESS;
    }
    if (kem != nullptr) OQS_KEM_free(kem);
    OQS_MEM_cleanse(kem_seed, sizeof(kem_seed));
    return ok;
  }

  void pqc_kem_pk_fingerprint(const uint8_t *kem_pk, size_t pk_len, bq_sel_tag &out)
  {
    static const char domain[] = "HRG_BQ_KEMPK_v1";
    std::string buf;
    buf.reserve((sizeof(domain) - 1) + pk_len);
    buf.append(domain, sizeof(domain) - 1);
    buf.append(reinterpret_cast<const char*>(kem_pk), pk_len);
    crypto::hash h;
    crypto::cn_fast_hash(buf.data(), buf.size(), h);
    std::memcpy(out.data, &h, BQ_SEL_TAG_BYTES);
  }

  bool pqc_sel_pad(const uint8_t *derivation, size_t derivation_len, uint64_t output_index, bq_sel_tag &out)
  {
    // 13 + 32 + 8 = 53 bytes: a single Keccak-f permutation, cheap enough for the scan path.
    static const char domain[] = "HRG_BQ_SEL_v1";
    uint8_t buf[(sizeof(domain) - 1) + 32 + 8];
    if (derivation == nullptr || derivation_len != 32)
      return false;
    size_t off = 0;
    std::memcpy(buf + off, domain, sizeof(domain) - 1); off += sizeof(domain) - 1;
    std::memcpy(buf + off, derivation, 32);            off += 32;
    for (int n = 0; n < 8; ++n) buf[off++] = static_cast<uint8_t>((output_index >> (8 * n)) & 0xff);
    crypto::hash h;
    crypto::cn_fast_hash(buf, off, h);
    std::memcpy(out.data, &h, BQ_SEL_TAG_BYTES);
    // the derivation is a shared secret: scrub the copy
    OQS_MEM_cleanse(buf, sizeof(buf));
    OQS_MEM_cleanse(&h, sizeof(h));
    return true;
  }

  bool pqc_compute_sel_tag(const uint8_t *derivation, size_t derivation_len, uint64_t output_index,
                           const uint8_t *kem_pk, size_t pk_len, bq_sel_tag &out)
  {
    bq_sel_tag fp, pad;
    if (!pqc_sel_pad(derivation, derivation_len, output_index, pad))
      return false;
    pqc_kem_pk_fingerprint(kem_pk, pk_len, fp);
    out = bq_sel_tag_xor(fp, pad);
    return true;
  }
}
}
