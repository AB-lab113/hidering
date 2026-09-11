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

#pragma once

#include <cstdint>
#include <cstddef>

#include <boost/variant/variant.hpp>

#include "serialization/serialization.h"
#include "serialization/binary_archive.h"
#include "serialization/variant.h"
#include "crypto/crypto.h"
#include "crypto/pqc.h" // HIDERING Phase 5 (HFv16): post-quantum tx_extra field payload sizes

// HIDERING (audit F-4 support): HIDERING normalises every tx_extra to a fixed
// privacy padding (2500 bytes, see cryptonote_tx_utils.cpp). That single padding run
// exceeds the stock Monero cap of 255, which made HIDERING tx_extra fail canonical
// parse_tx_extra() — historically tolerated only because tx_extra is opaque at
// consensus. Raising the parse cap (load-side only; more permissive; emits no new
// bytes) lets parse_tx_extra() accept HIDERING padding, which the HFv16 post-quantum
// validator now relies on (audit E-3). No consensus rule keys on this value.
#define TX_EXTRA_PADDING_MAX_COUNT          2500
#define TX_EXTRA_NONCE_MAX_COUNT            255

#define TX_EXTRA_TAG_PADDING                0x00
#define TX_EXTRA_TAG_PUBKEY                 0x01
#define TX_EXTRA_NONCE                      0x02
#define TX_EXTRA_MERGE_MINING_TAG           0x03
#define TX_EXTRA_TAG_ADDITIONAL_PUBKEYS     0x04
#define TX_EXTRA_MYSTERIOUS_MINERGATE_TAG   0xDE

#define TX_EXTRA_NONCE_PAYMENT_ID           0x00
#define TX_EXTRA_NONCE_ENCRYPTED_PAYMENT_ID 0x01

namespace cryptonote
{
  struct tx_extra_padding
  {
    size_t size = 2500;

    // load
    template <template <bool> class Archive>
    bool member_do_serialize(Archive<false>& ar)
    {
      // size - 1 - because of variant tag
      for (size = 1; size <= TX_EXTRA_PADDING_MAX_COUNT; ++size)
      {
        if (ar.eof())
          break;

        uint8_t zero;
        if (!::do_serialize(ar, zero))
          return false;

        if (0 != zero)
          return false;
      }

      return size <= TX_EXTRA_PADDING_MAX_COUNT;
    }

    // store
    template <template <bool> class Archive>
    bool member_do_serialize(Archive<true>& ar)
    {
      if(TX_EXTRA_PADDING_MAX_COUNT < size)
        return false;

      // i = 1 - because of variant tag
      for (size_t i = 1; i < size; ++i)
      {
        uint8_t zero = 0;
        if (!::do_serialize(ar, zero))
          return false;
      }
      return true;
    }
  };

  struct tx_extra_pub_key
  {
    crypto::public_key pub_key;

    BEGIN_SERIALIZE()
      FIELD(pub_key)
    END_SERIALIZE()
  };

  struct tx_extra_nonce
  {
    std::string nonce;

    BEGIN_SERIALIZE()
      FIELD(nonce)
      if(TX_EXTRA_NONCE_MAX_COUNT < nonce.size()) return false;
    END_SERIALIZE()
  };

  struct tx_extra_merge_mining_tag
  {
    struct serialize_helper
    {
      tx_extra_merge_mining_tag& mm_tag;

      serialize_helper(tx_extra_merge_mining_tag& mm_tag_) : mm_tag(mm_tag_)
      {
      }

      BEGIN_SERIALIZE()
        VARINT_FIELD_N("depth", mm_tag.depth)
        FIELD_N("merkle_root", mm_tag.merkle_root)
      END_SERIALIZE()
    };

    uint64_t depth;
    crypto::hash merkle_root;

    // load
    template <template <bool> class Archive>
    bool member_do_serialize(Archive<false>& ar)
    {
      std::string field;
      if(!::do_serialize(ar, field))
        return false;

      binary_archive<false> iar{epee::strspan<std::uint8_t>(field)};
      serialize_helper helper(*this);
      return ::serialization::serialize(iar, helper);
    }

    // store
    template <template <bool> class Archive>
    bool member_do_serialize(Archive<true>& ar)
    {
      std::ostringstream oss;
      binary_archive<true> oar(oss);
      serialize_helper helper(*this);
      if(!::do_serialize(oar, helper))
        return false;

      std::string field = oss.str();
      return ::serialization::serialize(ar, field);
    }
  };

  // per-output additional tx pubkey for multi-destination transfers involving at least one subaddress
  struct tx_extra_additional_pub_keys
  {
    std::vector<crypto::public_key> data;

    BEGIN_SERIALIZE()
      FIELD(data)
    END_SERIALIZE()
  };

  struct tx_extra_mysterious_minergate
  {
    std::string data;

    BEGIN_SERIALIZE()
      FIELD(data)
    END_SERIALIZE()
  };

  // HIDERING Phase 5 (HFv16) — post-quantum tx_extra fields, registered as proper variant
  // types so parse_tx_extra and sort_tx_extra handle them CANONICALLY (audit E-3). The pq_sig
  // field is byte-identical to the raw [tag | pk | sig] blob cryptonote_tx_utils.cpp appends
  // (the validator strips it by its fixed length). The kyber_ct and pq_bind fields lead with
  // a varint output index and are written through this serialiser. Tags 0x06/0x07/0x08
  // mirror TX_EXTRA_TAG_PQ_SIG / TX_EXTRA_TAG_KYBER_CT / TX_EXTRA_TAG_PQ_BIND in
  // cryptonote_config.h. These fields only ever appear once hf_version >= HF_VERSION_PQ.
  struct tx_extra_pq_sig
  {
    crypto::pqc::pq_tx_sig sig; // pk(1952) || sig(3309) = 5261 bytes

    BEGIN_SERIALIZE()
      FIELD(sig)
    END_SERIALIZE()
  };

  // One per BQ... output. Layout: [ 0x07 | output_index:varint | sel_tag:8 | ct:1088 ].
  //
  // Decision 4 (design 2b, option B3) added output_index and sel_tag to what was a bare
  // ciphertext blob. sel_tag is the blinded selection tag (pqc.h, pqc_compute_sel_tag) that
  // tells the recipient which of its subaddress ML-KEM keys to decapsulate with, instead of
  // trying every one. output_index says which output the tag and ciphertext belong to: the
  // tag's blinding is bound to that index, and before this change the association between a
  // ciphertext and its output was only the implicit emission order. Consensus requires the
  // indices to be in range, unique, and to match the tx_extra_pq_bind indices one for one.
  struct tx_extra_kyber_ct
  {
    uint64_t output_index;
    crypto::pqc::bq_sel_tag sel_tag;  // 8 bytes
    crypto::pqc::kyber_ciphertext ct; // 1088 bytes

    BEGIN_SERIALIZE()
      VARINT_FIELD(output_index)
      FIELD(sel_tag)
      FIELD(ct)
    END_SERIALIZE()
  };

  // HIDERING Phase 5 (HFv16, Option-2-transparent / A1) — per-output post-quantum
  // binding tag. For a BQ... output at local index `output_index` in this tx, bind_tag
  // commits the one-time output key P'_i to the per-output ML-DSA-65 public key that
  // will authorise its (transparent) spend: bind_tag = Keccak("HRG_PQ_BIND_v1" || P'_i
  // || dsa_pk_i). Stored at output CREATION, looked up by the validator at SPEND time
  // (see blockchain.cpp check_tx_inputs, check c). Variable-length leading varint, so it
  // takes the generic [tag|size|data] tx_extra encoding (unlike the fixed-size pq_sig /
  // kyber_ct blobs). Only present once hf_version >= HF_VERSION_PQ.
  struct tx_extra_pq_bind
  {
    uint64_t output_index;
    crypto::hash bind_tag;

    BEGIN_SERIALIZE()
      VARINT_FIELD(output_index)
      FIELD(bind_tag)
    END_SERIALIZE()
  };

  // tx_extra_field format, except tx_extra_padding and tx_extra_pub_key:
  //   varint tag;
  //   varint size;
  //   varint data[];
  typedef boost::variant<tx_extra_padding, tx_extra_pub_key, tx_extra_nonce, tx_extra_merge_mining_tag, tx_extra_additional_pub_keys, tx_extra_mysterious_minergate, tx_extra_pq_sig, tx_extra_kyber_ct, tx_extra_pq_bind> tx_extra_field;
}

BLOB_SERIALIZER(crypto::pqc::pq_tx_sig);
BLOB_SERIALIZER(crypto::pqc::kyber_ciphertext);
BLOB_SERIALIZER(crypto::pqc::bq_sel_tag);

VARIANT_TAG(binary_archive, cryptonote::tx_extra_padding, TX_EXTRA_TAG_PADDING);
VARIANT_TAG(binary_archive, cryptonote::tx_extra_pub_key, TX_EXTRA_TAG_PUBKEY);
VARIANT_TAG(binary_archive, cryptonote::tx_extra_nonce, TX_EXTRA_NONCE);
VARIANT_TAG(binary_archive, cryptonote::tx_extra_merge_mining_tag, TX_EXTRA_MERGE_MINING_TAG);
VARIANT_TAG(binary_archive, cryptonote::tx_extra_additional_pub_keys, TX_EXTRA_TAG_ADDITIONAL_PUBKEYS);
VARIANT_TAG(binary_archive, cryptonote::tx_extra_mysterious_minergate, TX_EXTRA_MYSTERIOUS_MINERGATE_TAG);
// HIDERING Phase 5 (HFv16): tags 0x06 / 0x07 (free in the classic tx_extra tag space).
VARIANT_TAG(binary_archive, cryptonote::tx_extra_pq_sig, 0x06);
VARIANT_TAG(binary_archive, cryptonote::tx_extra_kyber_ct, 0x07);
VARIANT_TAG(binary_archive, cryptonote::tx_extra_pq_bind, 0x08); // ::config::TX_EXTRA_TAG_PQ_BIND
