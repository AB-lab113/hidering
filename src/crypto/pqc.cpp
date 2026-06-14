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

#include <cstring>
#include <mutex>

#include <oqs/oqs.h>
#include <oqs/sha3.h> // incremental SHAKE256 API (not pulled in by oqs.h)

namespace
{
  // --- Deterministic SHAKE256 RNG backing pqc_keygen_from_seed (ML-DSA path) ---
  //
  // liboqs exposes no derandomised keygen for ML-DSA-65; OQS_SIG_keypair pulls its
  // 32-byte ξ through the process-global `randombytes` hook. We install a SHAKE256
  // byte-stream as that hook so the keygen becomes a pure function of the seed.
  // The hook is a global function pointer with no context argument, so the stream
  // state must be static and is guarded by g_det_rng_mutex (held across the whole
  // OQS_SIG_keypair call by the caller). Using a squeezable SHAKE stream rather than
  // a fixed 32-byte buffer keeps it correct no matter how many bytes liboqs reads.
  std::mutex g_det_rng_mutex;
  OQS_SHA3_shake256_inc_ctx *g_det_rng_stream = nullptr;

  void det_rng_fill(uint8_t *out, size_t len)
  {
    if (g_det_rng_stream != nullptr)
      OQS_SHA3_shake256_inc_squeeze(out, len, g_det_rng_stream);
    else if (len > 0)
      std::memset(out, 0, len); // unreachable: stream is always set under the lock
  }

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
      // deterministic SHAKE256 RNG over liboqs' global randombytes hook. The hook
      // is process-global, so serialise and always restore the default RNG.
      bool sig_ok = false;
      {
        std::lock_guard<std::mutex> lock(g_det_rng_mutex);
        OQS_SHA3_shake256_inc_ctx stream;
        OQS_SHA3_shake256_inc_init(&stream);
        OQS_SHA3_shake256_inc_absorb(&stream, dsa_seed, sizeof(dsa_seed));
        OQS_SHA3_shake256_inc_finalize(&stream);
        g_det_rng_stream = &stream;
        OQS_randombytes_custom_algorithm(det_rng_fill);

        sig_ok = OQS_SIG_keypair(sig, pk.dilithium3_pk, sk.dilithium3_sk) == OQS_SUCCESS;

        // Restore the default system RNG BEFORE releasing the stream, so no later
        // OQS_randombytes call can dereference a freed context.
        OQS_randombytes_switch_algorithm(OQS_RAND_alg_system);
        g_det_rng_stream = nullptr;
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
}
}
