// Copyright (c) 2026, The Hidering Project
//
// Phase 5 — Post-Quantum Cryptography key types and operations.
//
// HIDERING's Phase 5 additive hard fork (HFv16, scheduled at HF_HEIGHT_PQ) layers
// NIST-standardised post-quantum primitives on top of the existing CLSAG/Ed25519
// ring-signature machinery (which is left untouched):
//
//   * ML-DSA-65  (FIPS 204, formerly CRYSTALS-Dilithium3, NIST level 3) — an external
//     signature carried in the transaction `extra` field, alongside the classical
//     ring signature.
//   * ML-KEM-768 (FIPS 203, formerly CRYSTALS-Kyber768,   NIST level 3) — key
//     encapsulation, replacing the classical ECDH shared-secret derivation for the
//     new BQ... addresses.
//
// NB: struct members and wallet-file KV keys keep the historical "dilithium"/"kyber"
// naming (dilithium3_pk, kyber_sk, "pq_dilithium", "pq_keys", ...) — renaming them
// would break the experimental BQ wallet-file format for zero functional gain. The
// algorithms behind them are the final FIPS standards, NOT the Round-3 submissions
// (which are wire-incompatible: ML-DSA-65 sk/sig sizes differ, and ML-KEM's
// shared-secret derivation changed even though all ML-KEM-768 sizes match Kyber768).
//
// Backed by the Open Quantum Safe library (liboqs 0.15.0, imported `oqs` target).
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
  constexpr size_t ML_DSA_65_PUBLIC_KEY_BYTES = 1952;
  constexpr size_t ML_DSA_65_SECRET_KEY_BYTES = 4032;
  constexpr size_t ML_DSA_65_SIGNATURE_BYTES  = 3309;
  constexpr size_t ML_KEM_768_PUBLIC_KEY_BYTES   = 1184;
  constexpr size_t ML_KEM_768_SECRET_KEY_BYTES   = 2400;
  constexpr size_t ML_KEM_768_CIPHERTEXT_BYTES   = 1088;
  constexpr size_t ML_KEM_768_SHARED_SECRET_BYTES = 32;
  // Seed length for ML-KEM-768's derandomised keygen (FIPS 203: d||z, 32+32 bytes).
  constexpr size_t ML_KEM_768_KEYPAIR_SEED_BYTES = 64;
  // Seed length (ξ) ML-DSA-65 keygen consumes from the RNG (FIPS 204: 32 bytes).
  constexpr size_t ML_DSA_65_KEYGEN_SEED_BYTES = 32;

  // A combined post-quantum public key: a ML-DSA-65 verification key (for the
  // external signature) plus a ML-KEM-768 encapsulation key (for KEM-based address
  // derivation). The pair travels together with the new BQ... address format.
  struct pq_public_key
  {
    uint8_t dilithium3_pk[ML_DSA_65_PUBLIC_KEY_BYTES];
    uint8_t kyber768_pk[ML_KEM_768_PUBLIC_KEY_BYTES];
  };

  // The matching combined secret key (ML-DSA-65 signing key + ML-KEM-768 decaps key).
  struct pq_secret_key
  {
    uint8_t dilithium3_sk[ML_DSA_65_SECRET_KEY_BYTES];
    uint8_t kyber768_sk[ML_KEM_768_SECRET_KEY_BYTES];
  };

  // A ML-DSA-65 signature. ML-DSA signatures are fixed length, but pqc_sign
  // reports the produced length explicitly for forward-compatibility.
  struct pq_signature
  {
    uint8_t sig[ML_DSA_65_SIGNATURE_BYTES];
  };

  // A ML-KEM-768 KEM ciphertext (the encapsulation sent to the key holder).
  struct kyber_ciphertext
  {
    uint8_t ct[ML_KEM_768_CIPHERTEXT_BYTES];
  };

  // A ML-KEM-768 shared secret (derived identically by both encaps and decaps).
  struct kyber_shared_secret
  {
    uint8_t ss[ML_KEM_768_SHARED_SECRET_BYTES];
  };

  // Generate a fresh post-quantum keypair (ML-DSA-65 + ML-KEM-768).
  // Returns false if liboqs lacks either algorithm or a primitive fails.
  bool pqc_keygen(pq_public_key &pk, pq_secret_key &sk);

  // Phase 5 (HFv16, audit M-4) — DETERMINISTIC keygen from a wallet seed.
  //
  // Derives the ML-DSA-65 + ML-KEM-768 keypair deterministically from `seed`
  // (the wallet spend secret key), so a restore-from-mnemonic regenerates the
  // exact same BQ... keys instead of fresh random ones — fixing the M-4 loss of
  // BQ funds on seed restore. `seed`/`seed_len` is hashed with domain-separated
  // SHAKE256 into per-algorithm sub-seeds: ML-KEM-768 uses its derandomised
  // keygen (OQS_KEM_keypair_derand, 64-byte seed); ML-DSA-65 has no derandomised
  // keygen API, so OQS_SIG_keypair is driven by a deterministic SHAKE256 RNG
  // installed over liboqs' process-global randombytes hook for the duration of
  // the call (serialised by an internal mutex, default RNG restored on exit).
  // Returns false on any liboqs failure or if seed_len == 0.
  bool pqc_keygen_from_seed(const uint8_t *seed, size_t seed_len,
                            pq_public_key &pk, pq_secret_key &sk);

  // Sign `msg` (length `msg_len`) with the ML-DSA-65 secret key. On success
  // fills `sig` and sets `sig_len` to the produced signature length. Returns false
  // on failure (`sig_len` is set to 0).
  bool pqc_sign(const pq_secret_key &sk, const uint8_t *msg, size_t msg_len,
                pq_signature &sig, size_t &sig_len);

  // Verify a ML-DSA-65 signature of `msg` against the public key.
  // Returns true iff the signature is valid.
  bool pqc_verify(const pq_public_key &pk, const uint8_t *msg, size_t msg_len,
                  const pq_signature &sig, size_t sig_len);

  // ML-KEM-768 encapsulation: against the recipient's public key, produce a
  // ciphertext and the sender-side shared secret.
  bool pqc_kem_encaps(const pq_public_key &pk, kyber_ciphertext &ct,
                      kyber_shared_secret &ss);

  // ML-KEM-768 decapsulation: from the ciphertext and the recipient secret key,
  // recover the receiver-side shared secret (matches the sender's on success).
  bool pqc_kem_decaps(const pq_secret_key &sk, const kyber_ciphertext &ct,
                      kyber_shared_secret &ss);

  // Phase 5 (HFv16) — transaction-level ML-DSA-65 signature.
  //
  // The self-contained signature carried in a transaction's `extra` field: the
  // ML-DSA-65 verification key plus the signature over the tx prefix hash. Both
  // are fixed-length, so the on-wire layout (tag + pk + sig) is a simple blob and
  // a verifier needs nothing beyond the transaction itself to check it.
  struct pq_tx_sig
  {
    uint8_t pk[ML_DSA_65_PUBLIC_KEY_BYTES];
    uint8_t sig[ML_DSA_65_SIGNATURE_BYTES];
  };

  // Sign a transaction prefix hash with a raw ML-DSA-65 secret key, filling the
  // public key + signature into `out_sig`. `sk`/`sk_len` is the raw ML-DSA-65
  // signing key (sk_len must equal ML_DSA_65_SECRET_KEY_BYTES); the matching
  // public key must be supplied separately by the caller (out_sig.pk is filled
  // from `pk`). Returns false if liboqs lacks the algorithm, a size mismatches,
  // or signing fails.
  bool pqc_tx_sign(const uint8_t *tx_prefix_hash, size_t hash_len,
                   const uint8_t *sk, size_t sk_len,
                   const uint8_t *pk, size_t pk_len,
                   pq_tx_sig &out_sig);

  // Verify the ML-DSA-65 signature in `sig` (carrying its own public key) over
  // the given transaction prefix hash. Returns true iff valid.
  bool pqc_tx_verify(const uint8_t *tx_prefix_hash, size_t hash_len,
                     const pq_tx_sig &sig);

  // Phase 5 (HFv16) — ML-KEM-768 KEM for the new BQ... stealth addresses.
  //
  // A BQ... address publishes a ML-KEM-768 encapsulation key. The sender encapsulates
  // against it to obtain a shared secret (mixed into the one-time output key) plus a
  // ciphertext (carried in tx.extra under TX_EXTRA_TAG_KYBER_CT). The recipient, who
  // holds the matching decapsulation key, recovers the same shared secret from the
  // ciphertext. This replaces the classical ECDH shared secret for BQ... outputs;
  // standard B... addresses keep the Ed25519 ECDH path untouched.
  struct pq_stealth_keys
  {
    uint8_t kyber_pk[ML_KEM_768_PUBLIC_KEY_BYTES];
    uint8_t kyber_sk[ML_KEM_768_SECRET_KEY_BYTES];
  };

  // Phase 5 (HFv16, audit C-1) — the account's PERSISTENT ML-DSA-65 signing keypair.
  // The external tx signature (pq_tx_sig) must be produced by a stable per-account key,
  // not a throwaway generated per transaction: only a persistent key carries authority
  // that survives a quantum break of the Ed25519 ring (the entire point of Phase 5).
  // Held in account_keys (encrypted at rest like the Ed25519 secrets) and used by
  // construct_tx to sign the tx prefix hash.
  struct pq_dilithium_keys
  {
    uint8_t dilithium_pk[ML_DSA_65_PUBLIC_KEY_BYTES];
    uint8_t dilithium_sk[ML_DSA_65_SECRET_KEY_BYTES];
  };

  // Sender side: encapsulate against a recipient's raw ML-KEM-768 public key
  // (`pk_len` must equal ML_KEM_768_PUBLIC_KEY_BYTES), producing the ciphertext to put
  // on-chain and the sender-side shared secret. Returns false on any size mismatch
  // or liboqs failure.
  bool pqc_stealth_encaps(const uint8_t *recipient_kyber_pk, size_t pk_len,
                          kyber_ciphertext &ct, kyber_shared_secret &ss);

  // Recipient side: decapsulate the on-chain ciphertext with the BQ... address'
  // ML-KEM-768 keypair, recovering the shared secret (matches the sender's on success).
  bool pqc_stealth_decaps(const pq_stealth_keys &keys, const kyber_ciphertext &ct,
                          kyber_shared_secret &ss);

  // Phase 5 (HFv16, Option-2-transparent / A1) — compute the per-output post-quantum
  // BINDING tag committing a (revealed) BQ output key to its per-output ML-DSA-65 public key:
  //
  //   bind_tag = Keccak( "HRG_PQ_BIND_v1" || real_output_key || dsa_pk )
  //
  // using the Monero-variant Keccak-256 (crypto::cn_fast_hash), so it is consistent with
  // the rest of the codebase's "Keccak". Published at output creation in a
  // TX_EXTRA_TAG_PQ_BIND field and recomputed by the validator at spend time to verify a
  // transparent txin_to_key_pq carries the ML-DSA key that was actually bound to the output.
  // `out_tag` receives 32 bytes. Raw byte pointers keep this header free of cryptonote
  // type dependencies (the caller passes &public_key / dsa pk bytes + their lengths).
  void pqc_compute_bind_tag(const uint8_t *real_output_key, size_t rk_len,
                            const uint8_t *dsa_pk, size_t pk_len,
                            uint8_t out_tag[32]);

  // Phase 5 (HFv16) — derive the PER-OUTPUT ML-DSA-65 keypair for a BQ output,
  // deterministically from that output's KEM shared secret and its index:
  //   sub_seed = SHAKE256("HRG_PQ_OUT_DSA_v1" || ss || index_le8)  (via pqc_keygen_from_seed)
  // Both the sender (at output creation, to build bind_tag) and the receiver (at spend, to
  // sign) reproduce the same keypair from the same (ss, index). Returns false on failure.
  bool pqc_keygen_output_dsa(const kyber_shared_secret &ss, uint64_t output_index,
                             pq_public_key &pk, pq_secret_key &sk);
}
}
