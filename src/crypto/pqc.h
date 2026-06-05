// Copyright (c) 2026, The Hidering Project
//
// Phase 5 — Post-Quantum Cryptography key types and operations.
//
// HIDERING's Phase 5 additive hard fork (HFv16, scheduled at HF_HEIGHT_PQ) layers
// NIST-standardised post-quantum primitives on top of the existing CLSAG/Ed25519
// ring-signature machinery (which is left untouched):
//
//   * Dilithium3 (CRYSTALS-Dilithium, NIST level 3) — an external signature carried
//     in the transaction `extra` field, alongside the classical ring signature.
//   * Kyber768   (CRYSTALS-Kyber,     NIST level 3) — key encapsulation, replacing
//     the classical ECDH shared-secret derivation for the new BQ... addresses.
//
// Backed by the Open Quantum Safe library (liboqs 0.10.1, imported `oqs` target).
// Key/signature/ciphertext sizes below are the fixed liboqs sizes for these
// algorithms and are asserted against the runtime liboqs values in pqc.cpp.

#pragma once

#include <cstddef>
#include <cstdint>

namespace crypto
{
namespace pqc
{
  // Fixed liboqs primitive sizes (bytes).
  constexpr size_t DILITHIUM3_PUBLIC_KEY_BYTES = 1952;
  constexpr size_t DILITHIUM3_SECRET_KEY_BYTES = 4000;
  constexpr size_t DILITHIUM3_SIGNATURE_BYTES  = 3293;
  constexpr size_t KYBER768_PUBLIC_KEY_BYTES   = 1184;
  constexpr size_t KYBER768_SECRET_KEY_BYTES   = 2400;
  constexpr size_t KYBER768_CIPHERTEXT_BYTES   = 1088;
  constexpr size_t KYBER768_SHARED_SECRET_BYTES = 32;

  // A combined post-quantum public key: a Dilithium3 verification key (for the
  // external signature) plus a Kyber768 encapsulation key (for KEM-based address
  // derivation). The pair travels together with the new BQ... address format.
  struct pq_public_key
  {
    uint8_t dilithium3_pk[DILITHIUM3_PUBLIC_KEY_BYTES];
    uint8_t kyber768_pk[KYBER768_PUBLIC_KEY_BYTES];
  };

  // The matching combined secret key (Dilithium3 signing key + Kyber768 decaps key).
  struct pq_secret_key
  {
    uint8_t dilithium3_sk[DILITHIUM3_SECRET_KEY_BYTES];
    uint8_t kyber768_sk[KYBER768_SECRET_KEY_BYTES];
  };

  // A Dilithium3 signature. Dilithium signatures are fixed length, but pqc_sign
  // reports the produced length explicitly for forward-compatibility.
  struct pq_signature
  {
    uint8_t sig[DILITHIUM3_SIGNATURE_BYTES];
  };

  // A Kyber768 KEM ciphertext (the encapsulation sent to the key holder).
  struct kyber_ciphertext
  {
    uint8_t ct[KYBER768_CIPHERTEXT_BYTES];
  };

  // A Kyber768 shared secret (derived identically by both encaps and decaps).
  struct kyber_shared_secret
  {
    uint8_t ss[KYBER768_SHARED_SECRET_BYTES];
  };

  // Generate a fresh post-quantum keypair (Dilithium3 + Kyber768).
  // Returns false if liboqs lacks either algorithm or a primitive fails.
  bool pqc_keygen(pq_public_key &pk, pq_secret_key &sk);

  // Sign `msg` (length `msg_len`) with the Dilithium3 secret key. On success
  // fills `sig` and sets `sig_len` to the produced signature length. Returns false
  // on failure (`sig_len` is set to 0).
  bool pqc_sign(const pq_secret_key &sk, const uint8_t *msg, size_t msg_len,
                pq_signature &sig, size_t &sig_len);

  // Verify a Dilithium3 signature of `msg` against the public key.
  // Returns true iff the signature is valid.
  bool pqc_verify(const pq_public_key &pk, const uint8_t *msg, size_t msg_len,
                  const pq_signature &sig, size_t sig_len);

  // Kyber768 encapsulation: against the recipient's public key, produce a
  // ciphertext and the sender-side shared secret.
  bool pqc_kem_encaps(const pq_public_key &pk, kyber_ciphertext &ct,
                      kyber_shared_secret &ss);

  // Kyber768 decapsulation: from the ciphertext and the recipient secret key,
  // recover the receiver-side shared secret (matches the sender's on success).
  bool pqc_kem_decaps(const pq_secret_key &sk, const kyber_ciphertext &ct,
                      kyber_shared_secret &ss);

  // Phase 5 (HFv16) — transaction-level Dilithium3 signature.
  //
  // The self-contained signature carried in a transaction's `extra` field: the
  // Dilithium3 verification key plus the signature over the tx prefix hash. Both
  // are fixed-length, so the on-wire layout (tag + pk + sig) is a simple blob and
  // a verifier needs nothing beyond the transaction itself to check it.
  struct pq_tx_sig
  {
    uint8_t pk[DILITHIUM3_PUBLIC_KEY_BYTES];
    uint8_t sig[DILITHIUM3_SIGNATURE_BYTES];
  };

  // Sign a transaction prefix hash with a raw Dilithium3 secret key, filling the
  // public key + signature into `out_sig`. `sk`/`sk_len` is the raw Dilithium3
  // signing key (sk_len must equal DILITHIUM3_SECRET_KEY_BYTES); the matching
  // public key must be supplied separately by the caller (out_sig.pk is filled
  // from `pk`). Returns false if liboqs lacks the algorithm, a size mismatches,
  // or signing fails.
  bool pqc_tx_sign(const uint8_t *tx_prefix_hash, size_t hash_len,
                   const uint8_t *sk, size_t sk_len,
                   const uint8_t *pk, size_t pk_len,
                   pq_tx_sig &out_sig);

  // Verify the Dilithium3 signature in `sig` (carrying its own public key) over
  // the given transaction prefix hash. Returns true iff valid.
  bool pqc_tx_verify(const uint8_t *tx_prefix_hash, size_t hash_len,
                     const pq_tx_sig &sig);

  // Phase 5 (HFv16) — Kyber768 KEM for the new BQ... stealth addresses.
  //
  // A BQ... address publishes a Kyber768 encapsulation key. The sender encapsulates
  // against it to obtain a shared secret (mixed into the one-time output key) plus a
  // ciphertext (carried in tx.extra under TX_EXTRA_TAG_KYBER_CT). The recipient, who
  // holds the matching decapsulation key, recovers the same shared secret from the
  // ciphertext. This replaces the classical ECDH shared secret for BQ... outputs;
  // standard B... addresses keep the Ed25519 ECDH path untouched.
  struct pq_stealth_keys
  {
    uint8_t kyber_pk[KYBER768_PUBLIC_KEY_BYTES];
    uint8_t kyber_sk[KYBER768_SECRET_KEY_BYTES];
  };

  // Phase 5 (HFv16, audit C-1) — the account's PERSISTENT Dilithium3 signing keypair.
  // The external tx signature (pq_tx_sig) must be produced by a stable per-account key,
  // not a throwaway generated per transaction: only a persistent key carries authority
  // that survives a quantum break of the Ed25519 ring (the entire point of Phase 5).
  // Held in account_keys (encrypted at rest like the Ed25519 secrets) and used by
  // construct_tx to sign the tx prefix hash.
  struct pq_dilithium_keys
  {
    uint8_t dilithium_pk[DILITHIUM3_PUBLIC_KEY_BYTES];
    uint8_t dilithium_sk[DILITHIUM3_SECRET_KEY_BYTES];
  };

  // Sender side: encapsulate against a recipient's raw Kyber768 public key
  // (`pk_len` must equal KYBER768_PUBLIC_KEY_BYTES), producing the ciphertext to put
  // on-chain and the sender-side shared secret. Returns false on any size mismatch
  // or liboqs failure.
  bool pqc_stealth_encaps(const uint8_t *recipient_kyber_pk, size_t pk_len,
                          kyber_ciphertext &ct, kyber_shared_secret &ss);

  // Recipient side: decapsulate the on-chain ciphertext with the BQ... address'
  // Kyber768 keypair, recovering the shared secret (matches the sender's on success).
  bool pqc_stealth_decaps(const pq_stealth_keys &keys, const kyber_ciphertext &ct,
                          kyber_shared_secret &ss);
}
}
