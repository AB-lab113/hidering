// Copyright (c) 2026, The Hidering Project
//
// Phase 5 — Post-Quantum Cryptography integration smoke test.
//
// Exercises the Open Quantum Safe (liboqs) primitives that HIDERING will adopt
// in the Phase 5 additive hard fork:
//   * Dilithium3 (CRYSTALS-Dilithium, NIST level 3) — digital signatures
//   * Kyber768   (CRYSTALS-Kyber,     NIST level 3) — key encapsulation
//
// Build (standalone, against the statically-built liboqs in external/liboqs):
//   g++ -std=c++17 -I external/liboqs/build/include \
//       src/crypto/pqc_test.cpp external/liboqs/build/lib/liboqs.a \
//       -lcrypto -o pqc_test
//
// Or, once wired into CMake, link the `oqs` target.

#include <oqs/oqs.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

static int test_dilithium3()
{
    static const char MESSAGE[] = "HIDERING_PQC_TEST";
    const size_t message_len = std::strlen(MESSAGE);

    OQS_SIG *sig = OQS_SIG_new(OQS_SIG_alg_dilithium_3);
    if (sig == nullptr) {
        std::fprintf(stderr, "ERROR: Dilithium3 is not enabled in this liboqs build\n");
        return 1;
    }

    std::vector<uint8_t> public_key(sig->length_public_key);
    std::vector<uint8_t> secret_key(sig->length_secret_key);
    std::vector<uint8_t> signature(sig->length_signature);
    size_t signature_len = 0;

    if (OQS_SIG_keypair(sig, public_key.data(), secret_key.data()) != OQS_SUCCESS) {
        std::fprintf(stderr, "ERROR: Dilithium3 keypair generation failed\n");
        OQS_SIG_free(sig);
        return 1;
    }

    if (OQS_SIG_sign(sig, signature.data(), &signature_len,
                     reinterpret_cast<const uint8_t *>(MESSAGE), message_len,
                     secret_key.data()) != OQS_SUCCESS) {
        std::fprintf(stderr, "ERROR: Dilithium3 signing failed\n");
        OQS_SIG_free(sig);
        return 1;
    }

    const OQS_STATUS verified =
        OQS_SIG_verify(sig, reinterpret_cast<const uint8_t *>(MESSAGE), message_len,
                       signature.data(), signature_len, public_key.data());

    // Negative control: a flipped bit must make verification fail.
    signature[0] ^= 0x01;
    const OQS_STATUS tampered =
        OQS_SIG_verify(sig, reinterpret_cast<const uint8_t *>(MESSAGE), message_len,
                       signature.data(), signature_len, public_key.data());

    std::printf("[Dilithium3]  (%s)\n", sig->method_name);
    std::printf("  message              : \"%s\" (%zu bytes)\n", MESSAGE, message_len);
    std::printf("  public key size      : %zu bytes\n", sig->length_public_key);
    std::printf("  secret key size      : %zu bytes\n", sig->length_secret_key);
    std::printf("  signature (max) size : %zu bytes\n", sig->length_signature);
    std::printf("  signature size       : %zu bytes\n", signature_len);
    std::printf("  verify valid sig     : %s\n", verified == OQS_SUCCESS ? "[ OK ]" : "[FAIL]");
    std::printf("  reject tampered sig  : %s\n", tampered != OQS_SUCCESS ? "[ OK ]" : "[FAIL]");

    OQS_SIG_free(sig);
    return (verified == OQS_SUCCESS && tampered != OQS_SUCCESS) ? 0 : 1;
}

static int test_kyber768()
{
    OQS_KEM *kem = OQS_KEM_new(OQS_KEM_alg_kyber_768);
    if (kem == nullptr) {
        std::fprintf(stderr, "ERROR: Kyber768 is not enabled in this liboqs build\n");
        return 1;
    }

    std::vector<uint8_t> public_key(kem->length_public_key);
    std::vector<uint8_t> secret_key(kem->length_secret_key);
    std::vector<uint8_t> ciphertext(kem->length_ciphertext);
    std::vector<uint8_t> shared_secret_e(kem->length_shared_secret);
    std::vector<uint8_t> shared_secret_d(kem->length_shared_secret);

    int rc = 0;
    rc |= (OQS_KEM_keypair(kem, public_key.data(), secret_key.data()) != OQS_SUCCESS);
    rc |= (OQS_KEM_encaps(kem, ciphertext.data(), shared_secret_e.data(), public_key.data()) != OQS_SUCCESS);
    rc |= (OQS_KEM_decaps(kem, shared_secret_d.data(), ciphertext.data(), secret_key.data()) != OQS_SUCCESS);

    const bool match = (rc == 0) &&
        std::memcmp(shared_secret_e.data(), shared_secret_d.data(), kem->length_shared_secret) == 0;

    std::printf("[Kyber768]    (%s)\n", kem->method_name);
    std::printf("  public key size      : %zu bytes\n", kem->length_public_key);
    std::printf("  secret key size      : %zu bytes\n", kem->length_secret_key);
    std::printf("  ciphertext size      : %zu bytes\n", kem->length_ciphertext);
    std::printf("  shared secret size   : %zu bytes\n", kem->length_shared_secret);
    std::printf("  encaps/decaps match  : %s\n", match ? "[ OK ]" : "[FAIL]");

    OQS_KEM_free(kem);
    return match ? 0 : 1;
}

int main()
{
    OQS_init();
    std::printf("HIDERING Phase 5 - Post-Quantum self-test (liboqs %s)\n\n", OQS_version());

    const int dilithium_rc = test_dilithium3();
    std::printf("\n");
    const int kyber_rc = test_kyber768();

    const bool pass = (dilithium_rc == 0 && kyber_rc == 0);
    std::printf("\nRESULT: %s\n", pass ? "PASS" : "FAIL");

    OQS_destroy();
    return pass ? 0 : 1;
}
