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

#include <fstream>

#include "include_base_utils.h"
#include "account.h"
#include "warnings.h"
#include "crypto/crypto.h"
extern "C"
{
#include "crypto/keccak.h"
}
#include "cryptonote_basic_impl.h"
#include "cryptonote_format_utils.h"
#include "cryptonote_config.h"

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "account"

using namespace std;

DISABLE_VS_WARNINGS(4244 4345)

  namespace cryptonote
{

  //-----------------------------------------------------------------
  hw::device& account_keys::get_device() const  {
    return *m_device;
  }
  //-----------------------------------------------------------------
  void account_keys::set_device( hw::device &hwdev)  {
    m_device = &hwdev;
    MCDEBUG("device", "account_keys::set_device device type: "<<typeid(hwdev).name());
  }
  //-----------------------------------------------------------------
  static void derive_key(const crypto::chacha_key &base_key, crypto::chacha_key &key)
  {
    static_assert(sizeof(base_key) == sizeof(crypto::hash), "chacha key and hash should be the same size");
    epee::mlocked<tools::scrubbed_arr<char, sizeof(base_key)+1>> data;
    memcpy(data.data(), &base_key, sizeof(base_key));
    data[sizeof(base_key)] = config::HASH_KEY_MEMORY;
    crypto::generate_chacha_key(data.data(), sizeof(data), key, 1);
  }
  //-----------------------------------------------------------------
  static epee::wipeable_string get_key_stream(const crypto::chacha_key &base_key, const crypto::chacha_iv &iv, size_t bytes)
  {
    // derive a new key
    crypto::chacha_key key;
    derive_key(base_key, key);

    // chacha
    epee::wipeable_string buffer0(std::string(bytes, '\0'));
    epee::wipeable_string buffer1 = buffer0;
    crypto::chacha20(buffer0.data(), buffer0.size(), key, iv, buffer1.data());
    return buffer1;
  }
  //-----------------------------------------------------------------
  void account_keys::xor_with_key_stream(const crypto::chacha_key &key)
  {
    // HIDERING Phase 5 (HFv16): when a Kyber768 BQ keypair is present, its secret key
    // (kyber_sk, KYBER768_SECRET_KEY_BYTES = 2400 B) is encrypted alongside the Ed25519
    // secrets, appended after the multisig keys in the derived stream. The public half
    // (kyber_pk) is not secret and is left in the clear. Classic wallets have
    // pq_keys == boost::none, so the stream length and on-disk bytes are unchanged.
    const size_t pq_sk_bytes = pq_keys ? crypto::pqc::KYBER768_SECRET_KEY_BYTES : 0;
    // encrypt a large enough byte stream with chacha20
    epee::wipeable_string key_stream = get_key_stream(key, m_encryption_iv, sizeof(crypto::secret_key) * (2 + m_multisig_keys.size()) + pq_sk_bytes);
    const char *ptr = key_stream.data();
    for (size_t i = 0; i < sizeof(crypto::secret_key); ++i)
      m_spend_secret_key.data[i] ^= *ptr++;
    for (size_t i = 0; i < sizeof(crypto::secret_key); ++i)
      m_view_secret_key.data[i] ^= *ptr++;
    for (crypto::secret_key &k: m_multisig_keys)
    {
      for (size_t i = 0; i < sizeof(crypto::secret_key); ++i)
        k.data[i] ^= *ptr++;
    }
    if (pq_keys)
    {
      for (size_t i = 0; i < crypto::pqc::KYBER768_SECRET_KEY_BYTES; ++i)
        pq_keys->kyber_sk[i] ^= *ptr++;
    }
  }
  //-----------------------------------------------------------------
  void account_keys::encrypt(const crypto::chacha_key &key)
  {
    m_encryption_iv = crypto::rand<crypto::chacha_iv>();
    xor_with_key_stream(key);
  }
  //-----------------------------------------------------------------
  void account_keys::decrypt(const crypto::chacha_key &key)
  {
    xor_with_key_stream(key);
  }
  //-----------------------------------------------------------------
  void account_keys::encrypt_viewkey(const crypto::chacha_key &key)
  {
    // encrypt a large enough byte stream with chacha20
    epee::wipeable_string key_stream = get_key_stream(key, m_encryption_iv, sizeof(crypto::secret_key) * 2);
    const char *ptr = key_stream.data();
    ptr += sizeof(crypto::secret_key);
    for (size_t i = 0; i < sizeof(crypto::secret_key); ++i)
      m_view_secret_key.data[i] ^= *ptr++;
  }
  //-----------------------------------------------------------------
  void account_keys::decrypt_viewkey(const crypto::chacha_key &key)
  {
    encrypt_viewkey(key);
  }
  //-----------------------------------------------------------------
  account_base::account_base()
  {
    set_null();
  }
  //-----------------------------------------------------------------
  void account_base::set_null()
  {
    m_keys = account_keys();
    m_creation_timestamp = 0;
  }
  //-----------------------------------------------------------------
  void account_base::deinit()
  {
    try{
      m_keys.get_device().disconnect();
    } catch (const std::exception &e){
      MERROR("Device disconnect exception: " << e.what());
    }
  }
  //-----------------------------------------------------------------
  void account_base::forget_spend_key()
  {
    m_keys.m_spend_secret_key = crypto::secret_key();
    m_keys.m_multisig_keys.clear();
  }
  //-----------------------------------------------------------------
  void account_base::set_spend_key(const crypto::secret_key& spend_secret_key)
  {
    // make sure derived spend public key matches saved public spend key
    crypto::public_key spend_public_key;
    crypto::secret_key_to_public_key(spend_secret_key, spend_public_key);
    CHECK_AND_ASSERT_THROW_MES(m_keys.m_account_address.m_spend_public_key == spend_public_key,
        "Unexpected derived public spend key");

    m_keys.m_spend_secret_key = spend_secret_key;
  }
  //-----------------------------------------------------------------
  crypto::secret_key account_base::generate(const crypto::secret_key& recovery_key, bool recover, bool two_random)
  {
    crypto::secret_key first = generate_keys(m_keys.m_account_address.m_spend_public_key, m_keys.m_spend_secret_key, recovery_key, recover);

    // rng for generating second set of keys is hash of first rng.  means only one set of electrum-style words needed for recovery
    crypto::secret_key second;
    keccak((uint8_t *)&m_keys.m_spend_secret_key, sizeof(crypto::secret_key), (uint8_t *)&second, sizeof(crypto::secret_key));

    generate_keys(m_keys.m_account_address.m_view_public_key, m_keys.m_view_secret_key, second, two_random ? false : true);

    struct tm timestamp = {0};
    timestamp.tm_year = 2014 - 1900;  // year 2014
    timestamp.tm_mon = 6 - 1;  // month june
    timestamp.tm_mday = 8;  // 8th of june
    timestamp.tm_hour = 0;
    timestamp.tm_min = 0;
    timestamp.tm_sec = 0;

    if (recover)
    {
      m_creation_timestamp = mktime(&timestamp);
      if (m_creation_timestamp == (uint64_t)-1) // failure
        m_creation_timestamp = 0; // lowest value
    }
    else
    {
      m_creation_timestamp = time(NULL);
    }
    return first;
  }
  //-----------------------------------------------------------------
  void account_base::create_from_keys(const cryptonote::account_public_address& address, const crypto::secret_key& spendkey, const crypto::secret_key& viewkey)
  {
    m_keys.m_account_address = address;
    m_keys.m_spend_secret_key = spendkey;
    m_keys.m_view_secret_key = viewkey;

    struct tm timestamp = {0};
    timestamp.tm_year = 2014 - 1900;  // year 2014
    timestamp.tm_mon = 4 - 1;  // month april
    timestamp.tm_mday = 15;  // 15th of april
    timestamp.tm_hour = 0;
    timestamp.tm_min = 0;
    timestamp.tm_sec = 0;

    m_creation_timestamp = mktime(&timestamp);
    if (m_creation_timestamp == (uint64_t)-1) // failure
      m_creation_timestamp = 0; // lowest value
  }

  //-----------------------------------------------------------------
  void account_base::create_from_device(const std::string &device_name)
  {
    hw::device &hwdev =  hw::get_device(device_name);
    hwdev.set_name(device_name);
    create_from_device(hwdev);
  }

  void account_base::create_from_device(hw::device &hwdev)
  {
    m_keys.set_device(hwdev);
    MCDEBUG("device", "device type: "<<typeid(hwdev).name());
    CHECK_AND_ASSERT_THROW_MES(hwdev.init(), "Device init failed");
    CHECK_AND_ASSERT_THROW_MES(hwdev.connect(), "Device connect failed");
    try {
      CHECK_AND_ASSERT_THROW_MES(hwdev.get_public_address(m_keys.m_account_address), "Cannot get a device address");
      CHECK_AND_ASSERT_THROW_MES(hwdev.get_secret_keys(m_keys.m_view_secret_key, m_keys.m_spend_secret_key), "Cannot get device secret");
    } catch (const std::exception &e){
      hwdev.disconnect();
      throw;
    }
    struct tm timestamp = {0};
    timestamp.tm_year = 2014 - 1900;  // year 2014
    timestamp.tm_mon = 4 - 1;  // month april
    timestamp.tm_mday = 15;  // 15th of april
    timestamp.tm_hour = 0;
    timestamp.tm_min = 0;
    timestamp.tm_sec = 0;

    m_creation_timestamp = mktime(&timestamp);
    if (m_creation_timestamp == (uint64_t)-1) // failure
      m_creation_timestamp = 0; // lowest value
  }

  //-----------------------------------------------------------------
  void account_base::create_from_viewkey(const cryptonote::account_public_address& address, const crypto::secret_key& viewkey)
  {
    crypto::secret_key fake;
    memset(&unwrap(unwrap(fake)), 0, sizeof(fake));
    create_from_keys(address, fake, viewkey);
  }
  //-----------------------------------------------------------------
  bool account_base::make_multisig(const crypto::secret_key &view_secret_key, const crypto::secret_key &spend_secret_key, const crypto::public_key &spend_public_key, const std::vector<crypto::secret_key> &multisig_keys)
  {
    m_keys.m_account_address.m_spend_public_key = spend_public_key;
    m_keys.m_view_secret_key = view_secret_key;
    m_keys.m_spend_secret_key = spend_secret_key;
    m_keys.m_multisig_keys = multisig_keys;
    return crypto::secret_key_to_public_key(view_secret_key, m_keys.m_account_address.m_view_public_key);
  }
  //-----------------------------------------------------------------
  const account_keys& account_base::get_keys() const
  {
    return m_keys;
  }
  //-----------------------------------------------------------------
  account_keys& account_base::get_keys_nonconst()
  {
    return m_keys;
  }
  //-----------------------------------------------------------------
  // HIDERING Phase 5 (HFv16): attach a fresh Kyber768 keypair to `keys`, turning its
  // account_public_address into a BQ... (post-quantum) address. Additive and opt-in.
  bool generate_pq_keys(account_keys& keys)
  {
    crypto::pqc::pq_public_key pq_pk;
    crypto::pqc::pq_secret_key pq_sk;
    if (!crypto::pqc::pqc_keygen(pq_pk, pq_sk))
    {
      MERROR("generate_pq_keys: liboqs Kyber768/Dilithium3 keygen failed");
      return false;
    }

    // Stealth (KEM) keypair: only the Kyber768 half is needed for BQ... address
    // derivation. The Dilithium3 half (tx signing) is plumbed separately (Step 3 caveat 1).
    crypto::pqc::pq_stealth_keys sk{};
    memcpy(sk.kyber_pk, pq_pk.kyber768_pk, crypto::pqc::KYBER768_PUBLIC_KEY_BYTES);
    memcpy(sk.kyber_sk, pq_sk.kyber768_sk, crypto::pqc::KYBER768_SECRET_KEY_BYTES);
    keys.pq_keys = sk;

    // Publish the Kyber768 public key on the address so is_pq() == true.
    std::array<uint8_t, crypto::pqc::KYBER768_PUBLIC_KEY_BYTES> kpk{};
    memcpy(kpk.data(), pq_pk.kyber768_pk, crypto::pqc::KYBER768_PUBLIC_KEY_BYTES);
    keys.m_account_address.pq_kyber_pk = kpk;
    return true;
  }
  //-----------------------------------------------------------------
  std::string get_pq_address_str(const account_keys& keys, network_type nettype)
  {
    if (!keys.m_account_address.is_pq())
    {
      MWARNING("get_pq_address_str: account has no Kyber768 key (not a BQ... address)");
      return std::string();
    }
    return get_account_address_as_str_pq(nettype, keys.m_account_address);
  }
  //-----------------------------------------------------------------
  std::string account_base::get_public_address_str(network_type nettype) const
  {
    //TODO: change this code into base 58
    return get_account_address_as_str(nettype, false, m_keys.m_account_address);
  }
  //-----------------------------------------------------------------
  std::string account_base::get_public_integrated_address_str(const crypto::hash8 &payment_id, network_type nettype) const
  {
    //TODO: change this code into base 58
    return get_account_integrated_address_as_str(nettype, m_keys.m_account_address, payment_id);
  }
  //-----------------------------------------------------------------
}
