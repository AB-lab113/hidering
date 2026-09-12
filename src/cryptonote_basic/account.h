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

#include "cryptonote_basic.h"
#include "subaddress_index.h"
#include "crypto/crypto.h"
#include "crypto/pqc.h"
#include "serialization/keyvalue_serialization.h"
#include <array>
#include <cstring>
#include <boost/optional/optional.hpp>

namespace cryptonote
{

  struct account_keys
  {
    account_public_address m_account_address;
    crypto::secret_key   m_spend_secret_key;
    crypto::secret_key   m_view_secret_key;
    std::vector<crypto::secret_key> m_multisig_keys;
    hw::device *m_device = &hw::get_device("default");
    crypto::chacha_iv m_encryption_iv;

    // HIDERING Phase 5 (HFv16): the wallet's ML-KEM-768 decapsulation keypair, present
    // only for accounts owning a BQ... address. Defaults to boost::none, so every
    // existing account is unaffected and the field is NOT serialized (absent from the
    // KV map below and from account_boost_serialization.h) — existing wallet files and
    // their on-disk layout are byte-for-byte unchanged.
    // audit M4: wrapped in epee::mlocked so the secret half (kyber_sk) is pinned in RAM
    // and never swapped to disk, mirroring crypto::secret_key (= mlocked<scrubbed<...>>).
    // mlocked<T> publicly derives from T, so every access here (!pq_keys, *pq_keys,
    // pq_keys->kyber_sk, pq_keys = plainT) and the blob (de)serialization below are
    // inheritance-transparent and the on-disk key_data bytes are unchanged.
    boost::optional<epee::mlocked<crypto::pqc::pq_stealth_keys>> pq_keys;

    // HIDERING Phase 5 (HFv16, audit C-1): the account's PERSISTENT ML-DSA-65 signing
    // keypair, used to authenticate transactions with a stable per-account key instead
    // of the former per-tx throwaway. Present only for BQ... accounts; boost::none for
    // every classic account, so it is absent from the serialized key_data (written only
    // when set, below) and existing wallet files stay byte-for-byte unchanged.
    // audit M4: mlocked for the same reason as pq_keys (pins dilithium_sk against swap).
    boost::optional<epee::mlocked<crypto::pqc::pq_dilithium_keys>> pq_dilithium;

    // HIDERING Phase 5 (HFv16, audit CRIT-4, decision R2a) — the POST-QUANTUM ROOT SECRET.
    //
    // Every BQ key of the account (the primary ML-KEM/ML-DSA pair and every subaddress
    // ML-KEM pair) is derived from this and from nothing else. It used to be
    // m_spend_secret_key, which is the discrete log of the spend public key the BQ address
    // publishes: Shor on that point handed an adversary the root and, through it, every BQ
    // key the account would ever own. It is now 32 bytes of INDEPENDENT entropy, backed up
    // by its own 25-word mnemonic (see wallet2::get_pq_seed) — nothing published on chain
    // is a function of it, so no quantum attack on an Ed25519 point reaches it.
    //
    // It is raw entropy, NOT an Ed25519 scalar: it is never reduced mod l and never
    // multiplied by a base point. crypto::secret_key is used purely as the carrier, for its
    // mlocked+scrubbed storage and for ElectrumWords::{bytes_to,words_to}_bytes, which
    // handle the 32 bytes verbatim (no sc_reduce32 on that path — crypto.cpp:159-166 is the
    // only place reduction happens, and the root never goes through it).
    //
    // Present only for BQ accounts; boost::none for every classic account, which therefore
    // emits no "pq_root" field and keeps its key_data byte-for-byte unchanged.
    boost::optional<crypto::secret_key> pq_root;

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(m_account_address)
      KV_SERIALIZE_VAL_POD_AS_BLOB_FORCE(m_spend_secret_key)
      KV_SERIALIZE_VAL_POD_AS_BLOB_FORCE(m_view_secret_key)
      KV_SERIALIZE_CONTAINER_POD_AS_BLOB(m_multisig_keys)
      const crypto::chacha_iv default_iv{{0, 0, 0, 0, 0, 0, 0, 0}};
      KV_SERIALIZE_VAL_POD_AS_BLOB_OPT(m_encryption_iv, default_iv)
      // HIDERING Phase 5 (HFv16): persist the optional ML-KEM-768 BQ keypair (pq_keys).
      // It is written as a single fixed-size blob (pq_stealth_keys = kyber_pk||kyber_sk,
      // 3584 B) ONLY for BQ... accounts; classic wallets have pq_keys == boost::none,
      // emit nothing here, and their key_data stays byte-for-byte unchanged. The secret
      // half (kyber_sk) is already chacha20-encrypted in place by encrypt()/decrypt()
      // before this map runs (see xor_with_key_stream), exactly like the Ed25519 secrets;
      // the public half (kyber_pk) is not secret and stays in the clear.
      //
      // We deliberately do NOT add pq_kyber_pk to account_public_address's own
      // serialization (Step 5 design: keeping B... wire/base58/file bytes identical).
      // Instead, on load we rehydrate m_account_address.pq_kyber_pk from the (plaintext)
      // kyber_pk inside the blob, so is_pq() becomes true again for a reloaded BQ wallet.
      if (is_store)
      {
        if (this_ref.pq_keys)
        {
          // audit M4: pq_blob is a plain (un-mlocked) stack copy of the blob. The secret
          // half is chacha20-encrypted at this point (encrypt_keys runs before this map),
          // but scrub it anyway as defense-in-depth — the mlocked member itself stays pinned.
          crypto::pqc::pq_stealth_keys pq_blob = *this_ref.pq_keys;
          epee::serialization::selector<is_store>::serialize_t_val_as_blob(pq_blob, stg, hparent_section, "pq_keys");
          memwipe(&pq_blob, sizeof(pq_blob));
        }
      }
      else
      {
        crypto::pqc::pq_stealth_keys pq_blob{};
        if (epee::serialization::selector<is_store>::serialize_t_val_as_blob(pq_blob, stg, hparent_section, "pq_keys"))
        {
          this_ref.pq_keys = pq_blob; // copies into the mlocked member (implicit mlocked(const T&))
          std::array<uint8_t, crypto::pqc::ML_KEM_768_PUBLIC_KEY_BYTES> kpk{};
          memcpy(kpk.data(), pq_blob.kyber_pk, crypto::pqc::ML_KEM_768_PUBLIC_KEY_BYTES);
          this_ref.m_account_address.pq_kyber_pk = kpk;
          memwipe(&pq_blob, sizeof(pq_blob)); // audit M4: scrub the transient un-mlocked copy
        }
        else
        {
          this_ref.pq_keys = boost::none;
        }
      }
      // HIDERING Phase 5 (HFv16, audit C-1): persist the optional persistent ML-DSA-65
      // signing keypair (pq_dilithium) as a single fixed-size blob, encrypted exactly
      // like pq_keys (dilithium_sk is chacha20-encrypted in place by encrypt()/decrypt()
      // before this map runs; dilithium_pk stays in the clear). Written ONLY when set, so
      // classic wallets emit no "pq_dilithium" field and their key_data is unchanged.
      if (is_store)
      {
        if (this_ref.pq_dilithium)
        {
          crypto::pqc::pq_dilithium_keys d_blob = *this_ref.pq_dilithium;
          epee::serialization::selector<is_store>::serialize_t_val_as_blob(d_blob, stg, hparent_section, "pq_dilithium");
          memwipe(&d_blob, sizeof(d_blob)); // audit M4: scrub the transient un-mlocked copy
        }
      }
      else
      {
        crypto::pqc::pq_dilithium_keys d_blob{};
        if (epee::serialization::selector<is_store>::serialize_t_val_as_blob(d_blob, stg, hparent_section, "pq_dilithium"))
        {
          this_ref.pq_dilithium = d_blob;
          memwipe(&d_blob, sizeof(d_blob)); // audit M4: scrub the transient un-mlocked copy
        }
        else
          this_ref.pq_dilithium = boost::none;
      }
      // HIDERING Phase 5 (HFv16, audit CRIT-4 / decision R2a): persist the post-quantum
      // root secret. Same shape as pq_keys/pq_dilithium: a named optional field written
      // ONLY when set, so a classic wallet emits nothing and its key_data is unchanged.
      // The 32 bytes are chacha20-encrypted in place by encrypt()/decrypt() before this
      // map runs (xor_with_key_stream), exactly like m_spend_secret_key.
      if (is_store)
      {
        if (this_ref.pq_root)
        {
          crypto::secret_key r_blob = *this_ref.pq_root; // mlocked+scrubbed carrier
          epee::serialization::selector<is_store>::serialize_t_val_as_blob(r_blob, stg, hparent_section, "pq_root");
        }
      }
      else
      {
        crypto::secret_key r_blob{};
        if (epee::serialization::selector<is_store>::serialize_t_val_as_blob(r_blob, stg, hparent_section, "pq_root"))
          this_ref.pq_root = r_blob;
        else
          this_ref.pq_root = boost::none;
      }
    END_KV_SERIALIZE_MAP()

    void encrypt(const crypto::chacha_key &key);
    void decrypt(const crypto::chacha_key &key);
    void encrypt_viewkey(const crypto::chacha_key &key);
    void decrypt_viewkey(const crypto::chacha_key &key);

    hw::device& get_device()  const ;
    void set_device( hw::device &hwdev) ;

  private:
    void xor_with_key_stream(const crypto::chacha_key &key);
  };

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  class account_base
  {
  public:
    account_base();
    crypto::secret_key generate(const crypto::secret_key& recovery_key = crypto::secret_key(), bool recover = false, bool two_random = false);
    void create_from_device(const std::string &device_name);
    void create_from_device(hw::device &hwdev);
    void create_from_keys(const cryptonote::account_public_address& address, const crypto::secret_key& spendkey, const crypto::secret_key& viewkey);
    void create_from_viewkey(const cryptonote::account_public_address& address, const crypto::secret_key& viewkey);
    bool make_multisig(const crypto::secret_key &view_secret_key, const crypto::secret_key &spend_secret_key, const crypto::public_key &spend_public_key, const std::vector<crypto::secret_key> &multisig_keys);
    const account_keys& get_keys() const;
    // HIDERING Phase 5 (HFv16): mutable access to the keys, used to attach the optional
    // ML-KEM-768 BQ... keypair (generate_pq_keys). Classic flows never touch pq_keys.
    account_keys& get_keys_nonconst();
    std::string get_public_address_str(network_type nettype) const;
    std::string get_public_integrated_address_str(const crypto::hash8 &payment_id, network_type nettype) const;

    hw::device& get_device() const  {return m_keys.get_device();}
    void set_device( hw::device &hwdev) {m_keys.set_device(hwdev);}
    void deinit();

    uint64_t get_createtime() const { return m_creation_timestamp; }
    void set_createtime(uint64_t val) { m_creation_timestamp = val; }

    bool load(const std::string& file_path);
    bool store(const std::string& file_path);

    void forget_spend_key();
    void set_spend_key(const crypto::secret_key& spend_secret_key);
    const std::vector<crypto::secret_key> &get_multisig_keys() const { return m_keys.m_multisig_keys; }

    void encrypt_keys(const crypto::chacha_key &key) { m_keys.encrypt(key); }
    void decrypt_keys(const crypto::chacha_key &key) { m_keys.decrypt(key); }
    void encrypt_viewkey(const crypto::chacha_key &key) { m_keys.encrypt_viewkey(key); }
    void decrypt_viewkey(const crypto::chacha_key &key) { m_keys.decrypt_viewkey(key); }

    template <class t_archive>
    inline void serialize(t_archive &a, const unsigned int /*ver*/)
    {
      a & m_keys;
      a & m_creation_timestamp;
    }

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(m_keys)
      KV_SERIALIZE(m_creation_timestamp)
    END_KV_SERIALIZE_MAP()

  private:
    void set_null();
    account_keys m_keys;
    uint64_t m_creation_timestamp;
  };

  // HIDERING Phase 5 (HFv16) — BQ... (post-quantum) account helpers.
  //
  // generate_pq_keys() derives the account's ML-KEM-768 + ML-DSA-65 keypairs from
  // `pq_root` and attaches them to `keys`:
  //   - keys.pq_root                       <- the root itself (persisted, encrypted)
  //   - keys.pq_keys                       <- the ML-KEM-768 {pk, sk} (decapsulation key)
  //   - keys.pq_dilithium                  <- the persistent ML-DSA-65 {pk, sk} (audit C-1)
  //   - keys.m_account_address.pq_kyber_pk <- the ML-KEM-768 public key (1184 bytes)
  // so that keys.m_account_address.is_pq() becomes true and a BQ... address can be
  // rendered/spent. Returns false if liboqs keygen fails. This is purely additive: it
  // is only ever called for accounts that opt into a BQ... address; classic accounts
  // leave pq_keys == boost::none and are byte-for-byte unchanged.
  //
  // audit CRIT-4 / decision R2a: `pq_root` is an EXPLICIT input, and the caller owns where
  // it comes from — generate_pq_root_secret() when creating, the user's BQ mnemonic when
  // restoring. It is deliberately not derivable from anything else the account holds, so
  // there is no default and no fallback: a restore that cannot supply it must fail loudly
  // rather than mint a fresh root and strand the funds (that is M-4 all over again).
  bool generate_pq_keys(account_keys& keys, const crypto::secret_key& pq_root);

  // Draw a FRESH post-quantum root secret: 32 bytes straight from the system CSPRNG
  // (audit CRIT-4 / decision R2a). It is deliberately NOT derived from the account's
  // Ed25519 material — that independence is the whole point, see account_keys::pq_root.
  // The caller is expected to show the user its 25-word backup (wallet2::get_pq_seed)
  // and to feed it back to generate_pq_keys on a restore.
  crypto::secret_key generate_pq_root_secret();

  // Render the BQ... address string for an account that owns a ML-KEM-768 key. Returns an
  // empty string if keys.m_account_address.is_pq() is false.
  std::string get_pq_address_str(const account_keys& keys, network_type nettype);

  // HIDERING Phase 5 (HFv16, decision 4 / design 2b option B3) — BQ subaddresses.
  //
  // The secret every BQ key of the account is derived from: the primary keypair
  // (generate_pq_keys) and every subaddress ML-KEM keypair. ONE definition, on purpose.
  //
  // audit CRIT-4 (11 Sep 2026), fixed by decision R2a (12 Sep 2026): this used to return
  // keys.m_spend_secret_key, the discrete log of the spend public key published in the BQ
  // address — a quantum adversary recovered it with Shor and, through it, every BQ key of
  // the account. It now returns keys.pq_root, 32 bytes of independent entropy with their
  // own mnemonic. Because the derivation was already funnelled through this one function,
  // the subaddress half followed with no change of its own.
  // See docs/audit/design_2c_pq_root_shor_resistance.md.
  // Returns nullptr when the account has no root — a classic account, or a BQ account
  // created before CRIT-4 (whose keys hung off the spend key). Callers MUST treat nullptr
  // as "no BQ derivation possible" and fail; falling back to the spend key would put the
  // vulnerability straight back.
  const crypto::secret_key* get_pq_root_secret(const account_keys& keys);

  // The ML-KEM-768 keypair of BQ subaddress `index`. (0,0) is the primary BQ address and
  // returns keys.pq_keys as is; any other index is derived from get_pq_root_secret with
  // crypto::pqc::pqc_kem_keygen_subaddress. Nothing is stored: a subaddress keypair is
  // recomputed on demand (≈17 µs), so the .keys file only ever carries the account keys.
  // Returns false for an account without pq_keys, or on a liboqs failure.
  bool generate_pq_subaddress_keys(const account_keys& keys, const subaddress_index& index,
                                   crypto::pqc::pq_stealth_keys& out);

  // The public BQ address of subaddress `index`: the Ed25519 half of the classic subaddress
  // (D, C) plus that subaddress' own ML-KEM-768 key. For (0,0) it is the primary BQ address.
  bool get_pq_subaddress(const account_keys& keys, const subaddress_index& index,
                         account_public_address& out);
}
