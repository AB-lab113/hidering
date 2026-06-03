// Copyright (c) 2026, The Hidering Project
//
// Phase 5 — Post-Quantum Cryptography operations (liboqs implementation).
//
// Thin, allocation-light wrappers over the Open Quantum Safe primitives:
//   * Dilithium3 (OQS_SIG_alg_dilithium_3) — signatures
//   * Kyber768   (OQS_KEM_alg_kyber_768)   — key encapsulation
//
// Each call constructs and frees its OQS_SIG / OQS_KEM handle; these are cheap
// (no per-call key material) and keep the wrappers stateless and thread-safe.
// The compile-time sizes declared in pqc.h are cross-checked against the runtime
// liboqs object so a future liboqs bump that changed a size fails loudly here.

#include "pqc.h"

#include <cstring>

#include <oqs/oqs.h>

namespace crypto
{
namespace pqc
{
  bool pqc_keygen(pq_public_key &pk, pq_secret_key &sk)
  {
    bool ok = false;
    OQS_SIG *sig = OQS_SIG_new(OQS_SIG_alg_dilithium_3);
    OQS_KEM *kem = OQS_KEM_new(OQS_KEM_alg_kyber_768);
    if (sig != nullptr && kem != nullptr
        && sig->length_public_key == DILITHIUM3_PUBLIC_KEY_BYTES
        && sig->length_secret_key == DILITHIUM3_SECRET_KEY_BYTES
        && kem->length_public_key == KYBER768_PUBLIC_KEY_BYTES
        && kem->length_secret_key == KYBER768_SECRET_KEY_BYTES)
    {
      ok = OQS_SIG_keypair(sig, pk.dilithium3_pk, sk.dilithium3_sk) == OQS_SUCCESS
        && OQS_KEM_keypair(kem, pk.kyber768_pk, sk.kyber768_sk) == OQS_SUCCESS;
    }
    if (sig != nullptr) OQS_SIG_free(sig);
    if (kem != nullptr) OQS_KEM_free(kem);
    return ok;
  }

  bool pqc_sign(const pq_secret_key &sk, const uint8_t *msg, size_t msg_len,
                pq_signature &sig_out, size_t &sig_len)
  {
    bool ok = false;
    sig_len = 0;
    OQS_SIG *sig = OQS_SIG_new(OQS_SIG_alg_dilithium_3);
    if (sig != nullptr && sig->length_signature == DILITHIUM3_SIGNATURE_BYTES)
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
    OQS_SIG *sig = OQS_SIG_new(OQS_SIG_alg_dilithium_3);
    if (sig != nullptr)
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
    OQS_KEM *kem = OQS_KEM_new(OQS_KEM_alg_kyber_768);
    if (kem != nullptr
        && kem->length_ciphertext == KYBER768_CIPHERTEXT_BYTES
        && kem->length_shared_secret == KYBER768_SHARED_SECRET_BYTES)
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
    OQS_KEM *kem = OQS_KEM_new(OQS_KEM_alg_kyber_768);
    if (kem != nullptr && kem->length_shared_secret == KYBER768_SHARED_SECRET_BYTES)
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
        || sk_len != DILITHIUM3_SECRET_KEY_BYTES
        || pk_len != DILITHIUM3_PUBLIC_KEY_BYTES)
      return false;

    bool ok = false;
    OQS_SIG *sig = OQS_SIG_new(OQS_SIG_alg_dilithium_3);
    if (sig != nullptr
        && sig->length_signature == DILITHIUM3_SIGNATURE_BYTES
        && sig->length_secret_key == DILITHIUM3_SECRET_KEY_BYTES
        && sig->length_public_key == DILITHIUM3_PUBLIC_KEY_BYTES)
    {
      size_t produced = 0;
      if (OQS_SIG_sign(sig, out_sig.sig, &produced, tx_prefix_hash, hash_len, sk) == OQS_SUCCESS
          && produced == DILITHIUM3_SIGNATURE_BYTES)
      {
        std::memcpy(out_sig.pk, pk, DILITHIUM3_PUBLIC_KEY_BYTES);
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
    OQS_SIG *sig = OQS_SIG_new(OQS_SIG_alg_dilithium_3);
    if (sig != nullptr)
    {
      ok = OQS_SIG_verify(sig, tx_prefix_hash, hash_len,
                          sig_in.sig, DILITHIUM3_SIGNATURE_BYTES, sig_in.pk) == OQS_SUCCESS;
    }
    if (sig != nullptr) OQS_SIG_free(sig);
    return ok;
  }
}
}
