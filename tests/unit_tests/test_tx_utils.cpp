// Copyright (c) 2014-2024, The Monero Project
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
// 
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#include "gtest/gtest.h"

#include <vector>

#include "common/util.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/tx_extra.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "cryptonote_core/blockchain.h"
#include "cryptonote_basic/verification_context.h"
#include "cryptonote_config.h"
#include "crypto/pqc.h"
#include "string_tools.h"
#include "rapidjson/document.h"

namespace
{
  uint64_t const TEST_FEE = 5000000000; // 5 * 10^9
}

TEST(parse_tx_extra, handles_empty_extra)
{
  std::vector<uint8_t> extra;
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
  ASSERT_TRUE(tx_extra_fields.empty());
}

TEST(parse_tx_extra, handles_padding_only_size_1)
{
  const uint8_t extra_arr[] = {0};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
  ASSERT_EQ(1, tx_extra_fields.size());
  ASSERT_EQ(typeid(cryptonote::tx_extra_padding), tx_extra_fields[0].type());
  ASSERT_EQ(1, boost::get<cryptonote::tx_extra_padding>(tx_extra_fields[0]).size);
}

TEST(parse_tx_extra, handles_padding_only_size_2)
{
  const uint8_t extra_arr[] = {0, 0};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
  ASSERT_EQ(1, tx_extra_fields.size());
  ASSERT_EQ(typeid(cryptonote::tx_extra_padding), tx_extra_fields[0].type());
  ASSERT_EQ(2, boost::get<cryptonote::tx_extra_padding>(tx_extra_fields[0]).size);
}

TEST(parse_tx_extra, handles_padding_only_max_size)
{
  // HIDERING: padding cap is TX_EXTRA_PADDING_MAX_COUNT (2500), raised from Monero's 255 for
  // the internal 2500-byte tx_extra padding privacy feature — not TX_EXTRA_NONCE_MAX_COUNT.
  std::vector<uint8_t> extra(TX_EXTRA_PADDING_MAX_COUNT, 0);
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
  ASSERT_EQ(1, tx_extra_fields.size());
  ASSERT_EQ(typeid(cryptonote::tx_extra_padding), tx_extra_fields[0].type());
  ASSERT_EQ(TX_EXTRA_PADDING_MAX_COUNT, boost::get<cryptonote::tx_extra_padding>(tx_extra_fields[0]).size);
}

TEST(parse_tx_extra, handles_padding_only_exceed_max_size)
{
  std::vector<uint8_t> extra(TX_EXTRA_PADDING_MAX_COUNT + 1, 0);
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_FALSE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
}

TEST(parse_tx_extra, handles_invalid_padding_only)
{
  std::vector<uint8_t> extra(2, 0);
  extra[1] = 42;
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_FALSE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
}

TEST(parse_tx_extra, handles_pub_key_only)
{
  const uint8_t extra_arr[] = {1, 30, 208, 98, 162, 133, 64, 85, 83, 112, 91, 188, 89, 211, 24, 131, 39, 154, 22, 228,
    80, 63, 198, 141, 173, 111, 244, 183, 4, 149, 186, 140, 230};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
  ASSERT_EQ(1, tx_extra_fields.size());
  ASSERT_EQ(typeid(cryptonote::tx_extra_pub_key), tx_extra_fields[0].type());
}

TEST(parse_tx_extra, handles_extra_nonce_only)
{
  const uint8_t extra_arr[] = {2, 1, 42};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
  ASSERT_EQ(1, tx_extra_fields.size());
  ASSERT_EQ(typeid(cryptonote::tx_extra_nonce), tx_extra_fields[0].type());
  cryptonote::tx_extra_nonce extra_nonce = boost::get<cryptonote::tx_extra_nonce>(tx_extra_fields[0]);
  ASSERT_EQ(1, extra_nonce.nonce.size());
  ASSERT_EQ(42, extra_nonce.nonce[0]);
}

TEST(parse_tx_extra, handles_pub_key_and_padding)
{
  const uint8_t extra_arr[] = {1, 30, 208, 98, 162, 133, 64, 85, 83, 112, 91, 188, 89, 211, 24, 131, 39, 154, 22, 228,
    80, 63, 198, 141, 173, 111, 244, 183, 4, 149, 186, 140, 230, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
  ASSERT_EQ(2, tx_extra_fields.size());
  ASSERT_EQ(typeid(cryptonote::tx_extra_pub_key), tx_extra_fields[0].type());
  ASSERT_EQ(typeid(cryptonote::tx_extra_padding), tx_extra_fields[1].type());
}

// HIDERING Phase 5 (audit H2): the greedy 0x00 padding field and the trailing ML-DSA-65
// signature (0x06) are mutually exclusive as tx_extra fields — both must be the LAST field,
// and the padding parser rejects any non-zero byte after it. construct_tx_with_tx_key
// therefore omits the 0x00 padding on the PQ path. These two tests pin that invariant at the
// parse layer: padding-then-pq_sig must NOT parse; pq_sig-as-terminal must parse cleanly.
namespace
{
  // tx_extra_pq_sig is a BLOB_SERIALIZER: on the wire it is [0x06][pk||sig].
  // HIDERING Phase 5 (C2): reference the canonical ML-DSA-65 (FIPS 204) sizes from crypto::pqc
  // rather than literals, so a future liboqs/size change can never silently desync this test.
  const size_t PQ_SIG_BODY = crypto::pqc::ML_DSA_65_PUBLIC_KEY_BYTES + crypto::pqc::ML_DSA_65_SIGNATURE_BYTES;
}

// HIDERING Phase 5 (C2): pin the FIPS-final ML-DSA-65 / ML-KEM-768 sizes. The Step-9 migration
// (liboqs 0.10.1→0.15.0) moved ML-DSA-65 sk 4000→4032 and sig 3293→3309; these are the values the
// tx_extra PQ fields, the wallet BQ blobs and the validator all assume. If liboqs ever changes a
// size, this test fails loudly instead of letting a wire/format mismatch slip through.
TEST(pqc_sizes, ml_dsa_65_and_ml_kem_768_are_fips_final)
{
  EXPECT_EQ(1952u, crypto::pqc::ML_DSA_65_PUBLIC_KEY_BYTES);
  EXPECT_EQ(4032u, crypto::pqc::ML_DSA_65_SECRET_KEY_BYTES);   // FIPS 204 (was 4000 in the draft)
  EXPECT_EQ(3309u, crypto::pqc::ML_DSA_65_SIGNATURE_BYTES);    // FIPS 204 (was 3293 in the draft)
  EXPECT_EQ(1184u, crypto::pqc::ML_KEM_768_PUBLIC_KEY_BYTES);
  EXPECT_EQ(2400u, crypto::pqc::ML_KEM_768_SECRET_KEY_BYTES);
  EXPECT_EQ(1088u, crypto::pqc::ML_KEM_768_CIPHERTEXT_BYTES);
  EXPECT_EQ(32u,   crypto::pqc::ML_KEM_768_SHARED_SECRET_BYTES);
  // on-wire tx_extra_pq_sig body = pk||sig
  EXPECT_EQ(5261u, PQ_SIG_BODY);
}

TEST(parse_tx_extra, rejects_padding_before_pq_sig)
{
  // [pubkey][padding 0x00 + zeros][pq_sig 0x06 + 5261 bytes]: the greedy padding parser
  // consumes the 0x06 tag byte as "padding", sees it is non-zero, and rejects the whole
  // tx_extra. This is exactly the chain-halting layout H2 prevents from ever being emitted.
  std::vector<uint8_t> extra;
  extra.push_back(TX_EXTRA_TAG_PUBKEY);
  extra.insert(extra.end(), 32, 0x11);              // pubkey
  extra.push_back(TX_EXTRA_TAG_PADDING);            // 0x00
  extra.insert(extra.end(), 99, 0x00);              // padding zeros
  extra.push_back(0x06);                            // TX_EXTRA_TAG_PQ_SIG
  extra.insert(extra.end(), PQ_SIG_BODY, 0x00);     // ML-DSA-65 pk||sig body
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_FALSE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
}

TEST(parse_tx_extra, accepts_pq_sig_as_terminal_field)
{
  // [pubkey][pq_sig 0x06 + 5261 bytes] with NO 0x00 padding: parses canonically, pq_sig last.
  std::vector<uint8_t> extra;
  extra.push_back(TX_EXTRA_TAG_PUBKEY);
  extra.insert(extra.end(), 32, 0x11);              // pubkey
  extra.push_back(0x06);                            // TX_EXTRA_TAG_PQ_SIG
  extra.insert(extra.end(), PQ_SIG_BODY, 0x00);     // ML-DSA-65 pk||sig body
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
  ASSERT_EQ(2, tx_extra_fields.size());
  ASSERT_EQ(typeid(cryptonote::tx_extra_pub_key), tx_extra_fields[0].type());
  ASSERT_EQ(typeid(cryptonote::tx_extra_pq_sig), tx_extra_fields.back().type());
}

// HIDERING Phase 5 (C3, negative consensus — double-spend basis). A transparent BQ input
// (txin_to_key_pq) carries no Ed25519 key image; its double-spend marker is the SYNTHETIC key
// image get_pq_input_key_image(real_output_key) = Keccak("HRG_PQ_KI_v1" || P'). For the consensus
// double-spend check to work, this map must be DETERMINISTIC (same output → same KI, so a second
// spend collides and is rejected) and COLLISION-FREE across distinct outputs (different output →
// different KI, so unrelated outputs are not falsely flagged).
TEST(pq_consensus, synthetic_key_image_is_deterministic_and_distinct)
{
  crypto::public_key p1, p2;
  for (size_t i = 0; i < sizeof(p1); ++i) { ((uint8_t*)&p1)[i] = (uint8_t)(0x10 + i); ((uint8_t*)&p2)[i] = (uint8_t)(0x90 + i); }

  const crypto::key_image ki1a = cryptonote::get_pq_input_key_image(p1);
  const crypto::key_image ki1b = cryptonote::get_pq_input_key_image(p1);
  const crypto::key_image ki2  = cryptonote::get_pq_input_key_image(p2);

  // same output → same synthetic key image (a re-spend of the same BQ output collides → rejected)
  ASSERT_EQ(0, memcmp(&ki1a, &ki1b, sizeof(crypto::key_image)));
  // distinct outputs → distinct key images (independent BQ outputs never falsely double-spend)
  ASSERT_NE(0, memcmp(&ki1a, &ki2, sizeof(crypto::key_image)));
}

// HIDERING Phase 5 (C3, negative consensus — invalid ML-DSA-65 signature). The validator's check
// (d) verifies the per-input ML-DSA-65 signature over the tx prefix hash; a flipped byte must fail.
TEST(pq_consensus, invalid_ml_dsa_signature_is_rejected)
{
  crypto::pqc::pq_public_key pk; crypto::pqc::pq_secret_key sk;
  ASSERT_TRUE(crypto::pqc::pqc_keygen(pk, sk));
  crypto::hash msg; for (size_t i = 0; i < sizeof(msg); ++i) ((uint8_t*)&msg)[i] = (uint8_t)(i * 5 + 3);
  crypto::pqc::pq_tx_sig sig;
  memcpy(sig.pk, pk.dilithium3_pk, crypto::pqc::ML_DSA_65_PUBLIC_KEY_BYTES);
  ASSERT_TRUE(crypto::pqc::pqc_tx_sign((const uint8_t*)&msg, sizeof(msg),
      sk.dilithium3_sk, crypto::pqc::ML_DSA_65_SECRET_KEY_BYTES,
      pk.dilithium3_pk, crypto::pqc::ML_DSA_65_PUBLIC_KEY_BYTES, sig));
  ASSERT_TRUE(crypto::pqc::pqc_tx_verify((const uint8_t*)&msg, sizeof(msg), sig));   // honest verifies
  crypto::pqc::pq_tx_sig bad = sig; bad.sig[0] ^= 0xFF;
  ASSERT_FALSE(crypto::pqc::pqc_tx_verify((const uint8_t*)&msg, sizeof(msg), bad));  // tampered rejected
  crypto::pqc::pq_tx_sig wrongkey = sig; wrongkey.pk[0] ^= 0xFF;                     // key/sig mismatch
  ASSERT_FALSE(crypto::pqc::pqc_tx_verify((const uint8_t*)&msg, sizeof(msg), wrongkey));
}

// HIDERING Phase 5 (C3, negative consensus — malformed ML-KEM-768 ciphertext at parse). A
// tx_extra_kyber_ct is a fixed-size 1088-byte blob; a truncated one must fail the canonical TLV
// parser (the validator parses tx_extra canonically — audit E-3 — and rejects malformed extra).
TEST(pq_consensus, malformed_kyber_ct_fails_parse)
{
  // well-formed: [pubkey][kyber_ct 0x07 + 1088 bytes] parses
  {
    std::vector<uint8_t> extra;
    extra.push_back(TX_EXTRA_TAG_PUBKEY);
    extra.insert(extra.end(), 32, 0x11);
    extra.push_back(0x07); // TX_EXTRA_TAG_KYBER_CT
    extra.insert(extra.end(), crypto::pqc::ML_KEM_768_CIPHERTEXT_BYTES, 0x00);
    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(extra, fields));
    bool has_ct = false; for (const auto &f : fields) if (f.type() == typeid(cryptonote::tx_extra_kyber_ct)) has_ct = true;
    ASSERT_TRUE(has_ct);
  }
  // malformed: kyber_ct truncated (body shorter than 1088) → canonical parse fails
  {
    std::vector<uint8_t> extra;
    extra.push_back(TX_EXTRA_TAG_PUBKEY);
    extra.insert(extra.end(), 32, 0x11);
    extra.push_back(0x07);
    extra.insert(extra.end(), crypto::pqc::ML_KEM_768_CIPHERTEXT_BYTES - 50, 0x00); // truncated
    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_FALSE(cryptonote::parse_tx_extra(extra, fields));
  }
}

// HIDERING Phase 5 (C4, hf transition invariants). The PQ hard fork activates at exactly
// HF_VERSION_PQ (16) / HF_HEIGHT_PQ (2,000,000). Every BQ/PQ code path is gated on
// hf_version >= HF_VERSION_PQ, so these constants ARE the hf15→hf16 transition point: below 16 all
// PQ structures are rejected/inert, at/after 16 they are validated. Pin them so the activation
// height/version can't drift silently.
TEST(pq_consensus, hf_transition_parameters)
{
  EXPECT_EQ(16, (int)HF_VERSION_PQ);
  EXPECT_EQ(2000000ull, (unsigned long long)HF_HEIGHT_PQ);
  // the live chain runs hf 15 (< HF_VERSION_PQ): PQ is inert there by construction.
  EXPECT_LT(15, (int)HF_VERSION_PQ);
}

// HIDERING Phase 5 (C4, hf15->hf16 transition at the consensus output rules). A transparent BQ
// spend is a version-2 RCTTypeNull tx that REVEALS its output amounts and carries a txin_to_key_pq.
// Blockchain::check_tx_outputs (static, pure on tx+hf_version) is exactly the gate that flips at the
// fork: BEFORE HFv16 such a tx is rejected (a v2 tx must have 0-amount outputs / only modern rct
// types), and AT/AFTER HFv16 it is accepted (early-accept for txin_to_key_pq). This pins the
// transition boundary deterministically without needing a chain. (On regtest the daemon activates
// v16 at height 1, so the live hf16 acceptance is covered by the functional e2e; this test covers
// the pre-fork REJECTION that a regtest cannot produce, since it has no hf15 blocks.)
TEST(pq_consensus, transparent_pq_tx_outputs_gated_at_hf16)
{
  cryptonote::transaction tx{};
  tx.version = 2;
  tx.rct_signatures.type = rct::RCTTypeNull;          // transparent: no RingCT commitments
  cryptonote::txin_to_key_pq in{};
  in.amount = 100;                                    // revealed input amount
  in.spent_output_index = 0;
  tx.vin.push_back(in);                               // marks the tx as a transparent BQ spend
  cryptonote::tx_out o{};
  o.amount = 100;                                     // REVEALED output amount (non-zero)
  o.target = cryptonote::txout_to_tagged_key{};       // hf16-era output type (view-tagged)
  tx.vout.push_back(o);

  // before the fork (hf 15): a v2 tx with non-zero output amounts is invalid -> rejected
  cryptonote::tx_verification_context tvc_pre{};
  EXPECT_FALSE(cryptonote::Blockchain::check_tx_outputs(tx, tvc_pre, HF_VERSION_PQ - 1));
  EXPECT_TRUE(tvc_pre.m_invalid_output);

  // at the fork (hf 16): the txin_to_key_pq early-accept allows the revealed-amount outputs
  cryptonote::tx_verification_context tvc_post{};
  EXPECT_TRUE(cryptonote::Blockchain::check_tx_outputs(tx, tvc_post, HF_VERSION_PQ));
  EXPECT_FALSE(tvc_post.m_invalid_output);
}

// HIDERING Phase 5 — regression test for HAUT-2 (audit 7 Sep 2026).
// check_inputs_types_supported used to whitelist txin_to_key_pq unconditionally, and the main
// input loop of Blockchain::check_tx_inputs then skipped it with a bare `continue`. Since the
// dedicated PQ validation pass IS gated on hf_version >= HF_VERSION_PQ, a PQ input appearing
// before the fork went through BOTH passes untouched: neither validated nor rejected. It was
// safe only by accident, through unrelated invariants (RCTTypeNull refused off-coinbase, empty
// CLSAG ring, min_tx_version == 2). The type is now rejected explicitly at the semantic gate.
TEST(pq_consensus, transparent_pq_input_type_rejected_before_hf16)
{
  cryptonote::transaction tx{};
  tx.version = 2;
  tx.rct_signatures.type = rct::RCTTypeNull;
  cryptonote::txin_to_key_pq in{};
  in.amount = 100;
  in.spent_output_index = 0;
  tx.vin.push_back(in);

  // before the fork the input type itself is refused, whatever the rest of the tx looks like
  EXPECT_FALSE(cryptonote::check_inputs_types_supported(tx, HF_VERSION_PQ - 1));
  EXPECT_FALSE(cryptonote::check_inputs_types_supported(tx, 0));
  // at/after the fork it is a legitimate input type
  EXPECT_TRUE(cryptonote::check_inputs_types_supported(tx, HF_VERSION_PQ));

  // a classic ring input is accepted at every hard-fork version (no behaviour change for B...)
  cryptonote::transaction classic{};
  classic.version = 2;
  cryptonote::txin_to_key cin{};
  cin.amount = 0;
  cin.key_offsets.push_back(0);
  classic.vin.push_back(cin);
  EXPECT_TRUE(cryptonote::check_inputs_types_supported(classic, 0));
  EXPECT_TRUE(cryptonote::check_inputs_types_supported(classic, HF_VERSION_PQ - 1));
  EXPECT_TRUE(cryptonote::check_inputs_types_supported(classic, HF_VERSION_PQ));
}

// HIDERING Phase 5 — regression test for the txin_to_key_pq JSON bug (found during the
// BQ->BQ e2e, 7 Sep 2026). The serialization macro for txin_to_key_pq called
// ar.serialize_blob(&dsa, sizeof(dsa)) WITHOUT a preceding ar.tag("dsa"), so the JSON
// archive emitted the 5261-byte dsa blob as a bare value with no key and no comma:
//     "real_output_key": "6313...e8""6906..."
// making obj_to_json_str() output (served by the daemon as get_transactions.as_json)
// unparseable for every transparent BQ spend. binary_archive::tag() is a no-op, so the fix
// is wire-neutral — which the pinned blob below enforces.
TEST(pq_consensus, txin_to_key_pq_json_valid_and_wire_unchanged)
{
  cryptonote::transaction tx{};
  tx.version = 2;
  tx.unlock_time = 0;
  tx.rct_signatures.type = rct::RCTTypeNull;

  cryptonote::txin_to_key_pq in{};
  in.amount = 500000000000000ull;
  in.spent_output_index = 281;
  for (size_t i = 0; i < sizeof(in.real_output_key); ++i)
    ((uint8_t*)&in.real_output_key)[i] = (uint8_t)(0x40 + i);
  for (size_t i = 0; i < crypto::pqc::ML_DSA_65_PUBLIC_KEY_BYTES; ++i)
    in.dsa.pk[i] = (uint8_t)(i & 0xFF);
  for (size_t i = 0; i < crypto::pqc::ML_DSA_65_SIGNATURE_BYTES; ++i)
    in.dsa.sig[i] = (uint8_t)((i * 7 + 13) & 0xFF);
  tx.vin.push_back(in);

  cryptonote::tx_out o{};
  o.amount = 299999931430000ull;
  cryptonote::txout_to_tagged_key tagged{};
  for (size_t i = 0; i < sizeof(tagged.key); ++i)
    ((uint8_t*)&tagged.key)[i] = (uint8_t)(0xA0 + i);
  tagged.view_tag = crypto::view_tag{};
  o.target = tagged;
  tx.vout.push_back(o);

  tx.extra.push_back(TX_EXTRA_TAG_PUBKEY);
  tx.extra.insert(tx.extra.end(), 32, 0x11);

  // (1) WIRE FORMAT IS UNCHANGED. Pinned against the blob produced before the tag("dsa")
  // fix; binary_archive::tag() is a no-op so these must never move. A change here means
  // the consensus-visible encoding of a PQ input moved — never acceptable.
  const cryptonote::blobdata blob = cryptonote::tx_to_blob(tx);
  EXPECT_EQ(5383u, blob.size());
  crypto::hash blob_hash;
  crypto::cn_fast_hash(blob.data(), blob.size(), blob_hash);
  EXPECT_EQ(std::string("aa202bc0fbe69e42d64438f7e3845457efeb7ad7568d4cbda89d0e8a7e101879"), epee::string_tools::pod_to_hex(blob_hash));

  // (2) the blob still round-trips
  cryptonote::transaction tx2{};
  ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(blob, tx2));
  ASSERT_EQ(1u, tx2.vin.size());
  ASSERT_EQ(typeid(cryptonote::txin_to_key_pq), tx2.vin[0].type());
  const cryptonote::txin_to_key_pq &in2 = boost::get<cryptonote::txin_to_key_pq>(tx2.vin[0]);
  EXPECT_EQ(in.amount, in2.amount);
  EXPECT_EQ(in.spent_output_index, in2.spent_output_index);
  EXPECT_EQ(in.real_output_key, in2.real_output_key);
  EXPECT_EQ(0, memcmp(in.dsa.pk,  in2.dsa.pk,  crypto::pqc::ML_DSA_65_PUBLIC_KEY_BYTES));
  EXPECT_EQ(0, memcmp(in.dsa.sig, in2.dsa.sig, crypto::pqc::ML_DSA_65_SIGNATURE_BYTES));

  // (3) THE BUG: the JSON the daemon serves must name the dsa field and must PARSE.
  const std::string js = cryptonote::obj_to_json_str(tx);
  EXPECT_NE(std::string::npos, js.find("\"dsa\""))
      << "dsa blob emitted without its key -> invalid JSON";
  rapidjson::Document doc;
  doc.Parse(js.c_str());
  EXPECT_FALSE(doc.HasParseError())
      << "obj_to_json_str produced invalid JSON at offset " << doc.GetErrorOffset();
}

TEST(parse_and_validate_tx_extra, is_valid_tx_extra_parsed)
{
  cryptonote::transaction tx = AUTO_VAL_INIT(tx);
  cryptonote::account_base acc;
  acc.generate();
  cryptonote::blobdata b = "dsdsdfsdfsf";
  ASSERT_TRUE(cryptonote::construct_miner_tx(0, 0, 10000000000000, 1000, TEST_FEE, acc.get_keys().m_account_address, tx, b, 1));
  crypto::public_key tx_pub_key = cryptonote::get_tx_pub_key_from_extra(tx);
  ASSERT_NE(tx_pub_key, crypto::null_pkey);
}
TEST(parse_and_validate_tx_extra, fails_on_big_extra_nonce)
{
  cryptonote::transaction tx = AUTO_VAL_INIT(tx);
  cryptonote::account_base acc;
  acc.generate();
  cryptonote::blobdata b(TX_EXTRA_NONCE_MAX_COUNT + 1, 0);
  ASSERT_FALSE(cryptonote::construct_miner_tx(0, 0, 10000000000000, 1000, TEST_FEE, acc.get_keys().m_account_address, tx, b, 1));
}
TEST(parse_and_validate_tx_extra, fails_on_wrong_size_in_extra_nonce)
{
  cryptonote::transaction tx = AUTO_VAL_INIT(tx);
  tx.extra.resize(20, 0);
  tx.extra[0] = TX_EXTRA_NONCE;
  tx.extra[1] = 255;
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_FALSE(cryptonote::parse_tx_extra(tx.extra, tx_extra_fields));
}
TEST(validate_parse_amount_case, validate_parse_amount)
{
  uint64_t res = 0;
  bool r = cryptonote::parse_amount(res, "0.0001");
  ASSERT_TRUE(r);
  ASSERT_EQ(res, 100000000);

  r = cryptonote::parse_amount(res, "100.0001");
  ASSERT_TRUE(r);
  ASSERT_EQ(res, 100000100000000);

  r = cryptonote::parse_amount(res, "000.0000");
  ASSERT_TRUE(r);
  ASSERT_EQ(res, 0);

  r = cryptonote::parse_amount(res, "0");
  ASSERT_TRUE(r);
  ASSERT_EQ(res, 0);


  r = cryptonote::parse_amount(res, "   100.0001    ");
  ASSERT_TRUE(r);
  ASSERT_EQ(res, 100000100000000);

  r = cryptonote::parse_amount(res, "   100.0000    ");
  ASSERT_TRUE(r);
  ASSERT_EQ(res, 100000000000000);

  r = cryptonote::parse_amount(res, "   100. 0000    ");
  ASSERT_FALSE(r);

  r = cryptonote::parse_amount(res, "100. 0000");
  ASSERT_FALSE(r);

  r = cryptonote::parse_amount(res, "100 . 0000");
  ASSERT_FALSE(r);

  r = cryptonote::parse_amount(res, "100.00 00");
  ASSERT_FALSE(r);

  r = cryptonote::parse_amount(res, "1 00.00 00");
  ASSERT_FALSE(r);
}

TEST(sort_tx_extra, empty)
{
  std::vector<uint8_t> extra, sorted;
  ASSERT_TRUE(cryptonote::sort_tx_extra(extra, sorted));
  ASSERT_EQ(extra, sorted);
}

TEST(sort_tx_extra, pubkey)
{
  std::vector<uint8_t> sorted;
  const uint8_t extra_arr[] = {1, 30, 208, 98, 162, 133, 64, 85, 83, 112, 91, 188, 89, 211, 24, 131, 39, 154, 22, 228,
    80, 63, 198, 141, 173, 111, 244, 183, 4, 149, 186, 140, 230};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));
  ASSERT_TRUE(cryptonote::sort_tx_extra(extra, sorted));
  ASSERT_EQ(extra, sorted);
}

TEST(sort_tx_extra, two_pubkeys)
{
  std::vector<uint8_t> sorted;
  const uint8_t extra_arr[] = {1, 30, 208, 98, 162, 133, 64, 85, 83, 112, 91, 188, 89, 211, 24, 131, 39, 154, 22, 228,
    80, 63, 198, 141, 173, 111, 244, 183, 4, 149, 186, 140, 230,
    1, 30, 208, 98, 162, 133, 64, 85, 83, 112, 91, 188, 89, 211, 24, 131, 39, 154, 22, 228,
    80, 63, 198, 141, 173, 111, 244, 183, 4, 149, 186, 140, 230};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));
  ASSERT_TRUE(cryptonote::sort_tx_extra(extra, sorted));
  ASSERT_EQ(extra, sorted);
}

TEST(sort_tx_extra, keep_order)
{
  std::vector<uint8_t> sorted;
  const uint8_t extra_arr[] = {1, 30, 208, 98, 162, 133, 64, 85, 83, 112, 91, 188, 89, 211, 24, 131, 39, 154, 22, 228,
    80, 63, 198, 141, 173, 111, 244, 183, 4, 149, 186, 140, 230,
    2, 9, 1, 0, 0, 0, 0, 0, 0, 0, 0};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));
  ASSERT_TRUE(cryptonote::sort_tx_extra(extra, sorted));
  ASSERT_EQ(extra, sorted);
}

TEST(sort_tx_extra, switch_order)
{
  std::vector<uint8_t> sorted;
  const uint8_t extra_arr[] = {2, 9, 1, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 30, 208, 98, 162, 133, 64, 85, 83, 112, 91, 188, 89, 211, 24, 131, 39, 154, 22, 228,
    80, 63, 198, 141, 173, 111, 244, 183, 4, 149, 186, 140, 230};
  const uint8_t expected_arr[] = {1, 30, 208, 98, 162, 133, 64, 85, 83, 112, 91, 188, 89, 211, 24, 131, 39, 154, 22, 228,
    80, 63, 198, 141, 173, 111, 244, 183, 4, 149, 186, 140, 230,
    2, 9, 1, 0, 0, 0, 0, 0, 0, 0, 0};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));
  ASSERT_TRUE(cryptonote::sort_tx_extra(extra, sorted));
  std::vector<uint8_t> expected(&expected_arr[0], &expected_arr[0] + sizeof(expected_arr));
  ASSERT_EQ(expected, sorted);
}

TEST(sort_tx_extra, invalid)
{
  std::vector<uint8_t> sorted;
  const uint8_t extra_arr[] = {1};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));
  ASSERT_FALSE(cryptonote::sort_tx_extra(extra, sorted));
}

TEST(sort_tx_extra, invalid_suffix_strict)
{
  std::vector<uint8_t> sorted;
  const uint8_t extra_arr[] = {2, 9, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));
  ASSERT_FALSE(cryptonote::sort_tx_extra(extra, sorted));
}

TEST(sort_tx_extra, invalid_suffix_partial)
{
  std::vector<uint8_t> sorted;
  const uint8_t extra_arr[] = {2, 9, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1};
  const uint8_t expected_arr[] = {2, 9, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));
  ASSERT_TRUE(cryptonote::sort_tx_extra(extra, sorted, true));
  std::vector<uint8_t> expected(&expected_arr[0], &expected_arr[0] + sizeof(expected_arr));
  ASSERT_EQ(sorted, expected);
}

TEST(remove_field_from_tx_extra, remove_first)
{
  const uint8_t extra_arr[] = {1, 30, 208, 98, 162, 133, 64, 85, 83, 112, 91, 188, 89, 211, 24, 131, 39, 154, 22, 228,
    80, 63, 198, 141, 173, 111, 244, 183, 4, 149, 186, 140, 230, 2, 1, 42};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));

  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
  ASSERT_EQ(2, tx_extra_fields.size());
  ASSERT_EQ(typeid(cryptonote::tx_extra_pub_key), tx_extra_fields[0].type());
  ASSERT_EQ(typeid(cryptonote::tx_extra_nonce), tx_extra_fields[1].type());

  tx_extra_fields.clear();
  ASSERT_TRUE(cryptonote::remove_field_from_tx_extra(extra, typeid(cryptonote::tx_extra_pub_key)));
  ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
  ASSERT_EQ(1, tx_extra_fields.size());
  ASSERT_EQ(typeid(cryptonote::tx_extra_nonce), tx_extra_fields[0].type());
}

TEST(remove_field_from_tx_extra, remove_last)
{
  const uint8_t extra_arr[] = {1, 30, 208, 98, 162, 133, 64, 85, 83, 112, 91, 188, 89, 211, 24, 131, 39, 154, 22, 228,
    80, 63, 198, 141, 173, 111, 244, 183, 4, 149, 186, 140, 230, 2, 1, 42};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));

  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
  ASSERT_EQ(2, tx_extra_fields.size());
  ASSERT_EQ(typeid(cryptonote::tx_extra_pub_key), tx_extra_fields[0].type());
  ASSERT_EQ(typeid(cryptonote::tx_extra_nonce), tx_extra_fields[1].type());

  tx_extra_fields.clear();
  ASSERT_TRUE(cryptonote::remove_field_from_tx_extra(extra, typeid(cryptonote::tx_extra_nonce)));
  ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
  ASSERT_EQ(1, tx_extra_fields.size());
  ASSERT_EQ(typeid(cryptonote::tx_extra_pub_key), tx_extra_fields[0].type());
}

TEST(remove_field_from_tx_extra, remove_middle)
{
  const uint8_t extra_arr[] = {1, 30, 208, 98, 162, 133, 64, 85, 83, 112, 91, 188, 89, 211, 24, 131, 39, 154, 22, 228,
    80, 63, 198, 141, 173, 111, 244, 183, 4, 149, 186, 140, 230, 2, 1, 42, 1, 30, 208, 98, 162, 133, 64, 85, 83, 112,
    91, 188, 89, 211, 24, 131, 39, 154, 22, 228, 80, 63, 198, 141, 173, 111, 244, 183, 4, 149, 186, 140, 230};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));

  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
  ASSERT_EQ(3, tx_extra_fields.size());
  ASSERT_EQ(typeid(cryptonote::tx_extra_pub_key), tx_extra_fields[0].type());
  ASSERT_EQ(typeid(cryptonote::tx_extra_nonce), tx_extra_fields[1].type());
  ASSERT_EQ(typeid(cryptonote::tx_extra_pub_key), tx_extra_fields[2].type());

  tx_extra_fields.clear();
  ASSERT_TRUE(cryptonote::remove_field_from_tx_extra(extra, typeid(cryptonote::tx_extra_nonce)));
  ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
  ASSERT_EQ(2, tx_extra_fields.size());
  ASSERT_EQ(typeid(cryptonote::tx_extra_pub_key), tx_extra_fields[0].type());
  ASSERT_EQ(typeid(cryptonote::tx_extra_pub_key), tx_extra_fields[0].type());
}

TEST(remove_field_from_tx_extra, invalid_varint)
{
  const uint8_t extra_arr[] = {1, 30, 208, 98, 162, 133, 64, 85, 83, 112, 91, 188, 89, 211, 24, 131, 39, 154, 22, 228,
                               80, 63, 198, 141, 173, 111, 244, 183, 4, 149, 186, 140, 230, 2, 0x80, 0};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));

  ASSERT_FALSE(cryptonote::remove_field_from_tx_extra(extra, typeid(cryptonote::tx_extra_nonce)));
  ASSERT_EQ(sizeof(extra_arr), extra.size());
}
