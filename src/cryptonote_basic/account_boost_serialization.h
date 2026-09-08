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

#include "account.h"
#include "cryptonote_boost_serialization.h"

#include <boost/serialization/binary_object.hpp>
#include <boost/serialization/version.hpp>

//namespace cryptonote {
namespace boost
{
  namespace serialization
  {
    // HIDERING Phase 5 (HFv16): raw-byte serializers for the two post-quantum keypairs.
    // Both are plain fixed-size uint8_t arrays, so a binary_object is the faithful
    // representation and the archive stays a fixed width.
    template <class Archive>
    inline void serialize(Archive &a, crypto::pqc::pq_stealth_keys &x, const boost::serialization::version_type ver)
    {
      a & boost::serialization::make_binary_object(x.kyber_pk, sizeof(x.kyber_pk));
      a & boost::serialization::make_binary_object(x.kyber_sk, sizeof(x.kyber_sk));
    }

    template <class Archive>
    inline void serialize(Archive &a, crypto::pqc::pq_dilithium_keys &x, const boost::serialization::version_type ver)
    {
      a & boost::serialization::make_binary_object(x.dilithium_pk, sizeof(x.dilithium_pk));
      a & boost::serialization::make_binary_object(x.dilithium_sk, sizeof(x.dilithium_sk));
    }

    template <class Archive>
    inline void serialize(Archive &a, cryptonote::account_keys &x, const boost::serialization::version_type ver)
    {
      a & x.m_account_address;
      a & x.m_spend_secret_key;
      a & x.m_view_secret_key;

      // ---- HIDERING Phase 5 (HFv16): post-quantum key material -----------------
      //
      // Version 0 archives stop here, exactly as before, so anything ever written
      // by an older build still reads back identically.
      //
      // SCOPE, stated plainly: at the time of writing NOTHING in this tree archives
      // an account_keys — verified by symbol inspection of the built libraries
      // (serialize(account_keys) has zero template instantiations, while the
      // neighbouring serialize(account_public_address) has several, because the
      // wallet CACHE archives m_account_public_address). So this block is
      // insurance, not a fix: it makes the serializer complete should a future
      // caller archive an account, instead of silently dropping the BQ material.
      //
      // In particular it is NOT what makes cold-signing a BQ spend work. The cold
      // signer loads its own .keys file, where pq_keys and pq_dilithium are already
      // persisted (Step 7 and audit C-1). The real cold-signing gap is that
      // tx_source_entry and tx_destination_entry drop their PQ fields on the way
      // into the unsigned-tx file — see docs/audit/, that one needs a transfer-file
      // format decision.
      //
      // NB deliberately NOT extended: serialize(account_public_address) below. It IS
      // live in the wallet cache, so adding a field there would change the cache
      // format and break every existing wallet. The BQ ML-KEM key rides inside
      // pq_keys and account.cpp rehydrates m_account_address.pq_kyber_pk from it on
      // load — the same design decision taken in Step 7.
      if (ver < 1)
        return;

      bool has_pq_keys = static_cast<bool>(x.pq_keys);
      a & has_pq_keys;
      if (has_pq_keys)
      {
        // mlocked<T> IS-A T, but the archive needs a plain lvalue of the POD.
        crypto::pqc::pq_stealth_keys pq_blob{};
        if (!Archive::is_loading::value)
          pq_blob = *x.pq_keys;
        a & pq_blob;
        if (Archive::is_loading::value)
          x.pq_keys = pq_blob; // copies into the mlocked member
        memwipe(&pq_blob, sizeof(pq_blob)); // audit M4: scrub the transient plaintext copy
      }
      else if (Archive::is_loading::value)
      {
        x.pq_keys = boost::none;
      }

      bool has_pq_dilithium = static_cast<bool>(x.pq_dilithium);
      a & has_pq_dilithium;
      if (has_pq_dilithium)
      {
        crypto::pqc::pq_dilithium_keys d_blob{};
        if (!Archive::is_loading::value)
          d_blob = *x.pq_dilithium;
        a & d_blob;
        if (Archive::is_loading::value)
          x.pq_dilithium = d_blob;
        memwipe(&d_blob, sizeof(d_blob));
      }
      else if (Archive::is_loading::value)
      {
        x.pq_dilithium = boost::none;
      }
    }

    template <class Archive>
    inline void serialize(Archive &a, cryptonote::account_public_address &x, const boost::serialization::version_type ver)
    {
      a & x.m_spend_public_key;
      a & x.m_view_public_key;
    }

  }
}

// HIDERING Phase 5: version 1 adds the optional post-quantum keypairs. Version 0
// archives (anything written before HFv16 work) still load through the early return.
BOOST_CLASS_VERSION(cryptonote::account_keys, 1)
