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
#include <stdexcept>
#include <string>
#include <boost/uuid/uuid.hpp>

#define CRYPTONOTE_DNS_TIMEOUT_MS                       20000

#define CRYPTONOTE_MAX_BLOCK_NUMBER                     500000000
#define CRYPTONOTE_MAX_TX_SIZE                          1000000
#define CRYPTONOTE_MAX_TX_PER_BLOCK                     0x10000000
#define CRYPTONOTE_PUBLIC_ADDRESS_TEXTBLOB_VER          0
#define CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW            60
#define CURRENT_TRANSACTION_VERSION                     2
#define CURRENT_BLOCK_MAJOR_VERSION                     1
#define CURRENT_BLOCK_MINOR_VERSION                     0
#define CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT              60*60*2
#define CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE             10

#define BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW               60

// MONEY_SUPPLY - total number coins to be generated
// 18M HRG cap (18M x 10^12 atomic units = 1.8e19, fits uint64 max 1.844e19, ~2.5% headroom).
// Adopted 2026-05-11 after the original 33M literal was found to overflow uint64_t.
#define MONEY_SUPPLY ((uint64_t)18000000000000000000ULL)
#define HALVING_INTERVAL                                210000  // Halving every 210k blocks (~2.66 years)
#define INITIAL_BLOCK_REWARD                            ((uint64_t)42857142857143ULL) // 42.857142... HRG fair launch (= 9e18 / 210000, so geometric series sums to 18M)
#define EMISSION_SPEED_FACTOR_PER_MINUTE                (20)
#define FINAL_SUBSIDY_PER_MINUTE                        ((uint64_t)0) // No tail emission

#define CRYPTONOTE_REWARD_BLOCKS_WINDOW                 100
#define CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2    60000 //size of block (bytes) after which reward for block calculated using block size
#define CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1    20000 //size of block (bytes) after which reward for block calculated using block size - before first fork
#define CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5    300000 //size of block (bytes) after which reward for block calculated using block size - second change, from v5
#define CRYPTONOTE_LONG_TERM_BLOCK_WEIGHT_WINDOW_SIZE   100000 // size in blocks of the long term block weight median window
#define CRYPTONOTE_SHORT_TERM_BLOCK_WEIGHT_SURGE_FACTOR 50
#define CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE          600
#define CRYPTONOTE_DISPLAY_DECIMAL_POINT 12
// COIN — atomic units per 1 HRG. Must equal 10^CRYPTONOTE_DISPLAY_DECIMAL_POINT so that
// amount parsing, display formatting, and wallet-side thresholds (e.g. DEFAULT_MIN_OUTPUT_VALUE)
// agree on the same denomination. Hidering uses 10^12 to match the decimal point, which also
// aligns with Monero's atomic-unit granularity (1 HRG = 10^12 piconero-equivalent).
#define COIN                                            ((uint64_t)1000000000000) // 10^12

// Hidering fee constants — Option B calibration: divided by 262 = HRG/XMR reward ratio
// (157.14 HRG initial / 0.6 XMR Monero tail) so fee value-in-HRG ≈ fee value-in-XMR.
// Active in HF15+: FEE_PER_BYTE (static-fee floor, wallet2.cpp:8489) and
// DYNAMIC_FEE_REFERENCE_TRANSACTION_WEIGHT (numerator of the dynamic fee formula
// blockchain.cpp:3690-3703). The other constants are unreachable at HF15 (legacy
// pre-HFv8 paths) but rescaled identically for ratio consistency.
#define FEE_PER_KB_OLD                                  ((uint64_t)38167939)    // legacy, unreachable in HF15
#define FEE_PER_KB                                      ((uint64_t)7633588)     // legacy, unreachable in HF15
#define FEE_PER_BYTE                                    ((uint64_t)1150)        // active in HF15
#define DYNAMIC_FEE_PER_KB_BASE_FEE                     ((uint64_t)7633588)     // legacy, unreachable in HF15
#define DYNAMIC_FEE_PER_KB_BASE_BLOCK_REWARD            ((uint64_t)38167938931) // legacy, unreachable in HF15
#define DYNAMIC_FEE_PER_KB_BASE_FEE_V5                  ((uint64_t)7633588 * (uint64_t)CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2 / CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5) // legacy, unreachable in HF15
#define DYNAMIC_FEE_REFERENCE_TRANSACTION_WEIGHT        ((uint64_t)12)          // active in HF15 (~3000/262)

#define ORPHANED_BLOCKS_MAX_COUNT                       100

// HIDERING Mixnet: minimum stem hops before fluff broadcast
#define HIDERING_MIXNET_MIN_HOPS                        3
// HIDERING Mixnet: random delay range per hop (milliseconds)
#define HIDERING_MIXNET_HOP_DELAY_MS                    2000

// HIDERING Amount normalization: round to 0.1 HRG (10^10 atomic units)
#define HIDERING_AMOUNT_QUANTUM                         ((uint64_t)10000000000) // 0.1 HRG


#define DIFFICULTY_TARGET_V2                            120  // seconds
#define DIFFICULTY_TARGET_V1                            60  // seconds - before first fork
#define DIFFICULTY_WINDOW                               720 // blocks
#define DIFFICULTY_LAG                                  15  // !!!
#define DIFFICULTY_CUT                                  60  // timestamps to cut after sorting
#define DIFFICULTY_BLOCKS_COUNT                         DIFFICULTY_WINDOW + DIFFICULTY_LAG


#define CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS_V1   DIFFICULTY_TARGET_V1 * CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS
#define CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS_V2   DIFFICULTY_TARGET_V2 * CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS
#define CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS       1


#define DIFFICULTY_BLOCKS_ESTIMATE_TIMESPAN             DIFFICULTY_TARGET_V1 //just alias; used by tests


#define BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT          10000  //by default, blocks ids count in synchronizing
#define BLOCKS_IDS_SYNCHRONIZING_MAX_COUNT              25000  //max blocks ids count in synchronizing
#define BLOCKS_SYNCHRONIZING_DEFAULT_COUNT_PRE_V4       100    //by default, blocks count in blocks downloading
#define BLOCKS_SYNCHRONIZING_DEFAULT_COUNT              20     //by default, blocks count in blocks downloading
#define BLOCKS_SYNCHRONIZING_MAX_COUNT                  2048   //must be a power of 2, greater than 128, equal to SEEDHASH_EPOCH_BLOCKS

#define CRYPTONOTE_MEMPOOL_TX_LIVETIME                    (86400*3) //seconds, three days
#define CRYPTONOTE_MEMPOOL_TX_FROM_ALT_BLOCK_LIVETIME     604800 //seconds, one week


#define CRYPTONOTE_DANDELIONPP_STEMS              2 // number of outgoing stem connections per epoch
#define CRYPTONOTE_DANDELIONPP_FLUFF_PROBABILITY 20 // out of 100
#define CRYPTONOTE_DANDELIONPP_MIN_EPOCH         10 // minutes
#define CRYPTONOTE_DANDELIONPP_EPOCH_RANGE       30 // seconds
#define CRYPTONOTE_DANDELIONPP_FLUSH_AVERAGE      5 // seconds average for poisson distributed fluff flush
#define CRYPTONOTE_DANDELIONPP_EMBARGO_AVERAGE   39 // seconds (see tx_pool.cpp for more info)

// see src/cryptonote_protocol/levin_notify.cpp
#define CRYPTONOTE_NOISE_MIN_EPOCH                      5      // minutes
#define CRYPTONOTE_NOISE_EPOCH_RANGE                    30     // seconds
#define CRYPTONOTE_NOISE_MIN_DELAY                      10     // seconds
#define CRYPTONOTE_NOISE_DELAY_RANGE                    5      // seconds
#define CRYPTONOTE_NOISE_BYTES                          3*1024 // 3 KiB
#define CRYPTONOTE_NOISE_CHANNELS                       2      // Max outgoing connections per zone used for noise/covert sending

// Both below are in seconds. The idea is to delay forwarding from i2p/tor
// to ipv4/6, such that 2+ incoming connections _could_ have sent the tx
#define CRYPTONOTE_FORWARD_DELAY_BASE (CRYPTONOTE_NOISE_MIN_DELAY + CRYPTONOTE_NOISE_DELAY_RANGE)
#define CRYPTONOTE_FORWARD_DELAY_AVERAGE (CRYPTONOTE_FORWARD_DELAY_BASE + (CRYPTONOTE_FORWARD_DELAY_BASE / 2))

#define CRYPTONOTE_MAX_FRAGMENTS                        20 // ~20 * NOISE_BYTES max payload size for covert/noise send

#define COMMAND_RPC_GET_BLOCKS_FAST_MAX_BLOCK_COUNT     1000
#define COMMAND_RPC_GET_BLOCKS_FAST_MAX_TX_COUNT        20000
#define DEFAULT_RPC_MAX_CONNECTIONS_PER_PUBLIC_IP       3
#define DEFAULT_RPC_MAX_CONNECTIONS_PER_PRIVATE_IP      25
#define DEFAULT_RPC_MAX_CONNECTIONS                     100
#define DEFAULT_RPC_SOFT_LIMIT_SIZE                     25 * 1024 * 1024 // 25 MiB
#define MAX_RPC_CONTENT_LENGTH                          1048576 // 1 MB

#define P2P_LOCAL_WHITE_PEERLIST_LIMIT                  1000
#define P2P_LOCAL_GRAY_PEERLIST_LIMIT                   5000

#define P2P_DEFAULT_CONNECTIONS_COUNT                   12
#define P2P_DEFAULT_HANDSHAKE_INTERVAL                  60           //secondes
#define P2P_DEFAULT_PACKET_MAX_SIZE                     50000000     //50000000 bytes maximum packet size
#define P2P_DEFAULT_PEERS_IN_HANDSHAKE                  250
#define P2P_MAX_PEERS_IN_HANDSHAKE                      250
#define P2P_DEFAULT_CONNECTION_TIMEOUT                  5000       //5 seconds
#define P2P_DEFAULT_SOCKS_CONNECT_TIMEOUT               45         // seconds
#define P2P_DEFAULT_PING_CONNECTION_TIMEOUT             2000       //2 seconds
#define P2P_DEFAULT_INVOKE_TIMEOUT                      60*2*1000  //2 minutes
#define P2P_DEFAULT_HANDSHAKE_INVOKE_TIMEOUT            5000       //5 seconds
#define P2P_DEFAULT_WHITELIST_CONNECTIONS_PERCENT       70
#define P2P_DEFAULT_ANCHOR_CONNECTIONS_COUNT            2
#define P2P_DEFAULT_SYNC_SEARCH_CONNECTIONS_COUNT       2
#define P2P_DEFAULT_LIMIT_RATE_UP                       8192       // kB/s
#define P2P_DEFAULT_LIMIT_RATE_DOWN                     32768       // kB/s

#define P2P_FAILED_ADDR_FORGET_SECONDS                  (60*60)     //1 hour
#define P2P_IP_BLOCKTIME                                (60*60*24)  //24 hour
#define P2P_IP_FAILS_BEFORE_BLOCK                       10
#define P2P_IDLE_CONNECTION_KILL_INTERVAL               (5*60) //5 minutes

#define P2P_SUPPORT_FLAG_FLUFFY_BLOCKS                  0x01
#define P2P_SUPPORT_FLAGS                               P2P_SUPPORT_FLAG_FLUFFY_BLOCKS

#define RPC_IP_FAILS_BEFORE_BLOCK                       3

#define CRYPTONOTE_NAME                         "hidering"
#define CRYPTONOTE_BLOCKCHAINDATA_FILENAME      "data.mdb"
#define CRYPTONOTE_BLOCKCHAINDATA_LOCK_FILENAME "lock.mdb"
#define P2P_NET_DATA_FILENAME                   "p2pstate.bin"
#define RPC_PAYMENTS_DATA_FILENAME              "rpcpayments.bin"
#define MINER_CONFIG_FILE_NAME                  "miner_conf.json"

#define THREAD_STACK_SIZE                       5 * 1024 * 1024

#define HF_VERSION_DYNAMIC_FEE                  4
#define HF_VERSION_MIN_MIXIN_4                  6
#define HF_VERSION_MIN_MIXIN_6                  7
#define HF_VERSION_MIN_MIXIN_10                 8
#define HF_VERSION_MIN_MIXIN_31                 15
#define HF_VERSION_ENFORCE_RCT                  6
#define HF_VERSION_PER_BYTE_FEE                 8
#define HF_VERSION_SMALLER_BP                   10
#define HF_VERSION_LONG_TERM_BLOCK_WEIGHT       10
#define HF_VERSION_MIN_2_OUTPUTS                12
#define HF_VERSION_MIN_V2_COINBASE_TX           12
#define HF_VERSION_SAME_MIXIN                   12
#define HF_VERSION_REJECT_SIGS_IN_COINBASE      12
#define HF_VERSION_ENFORCE_MIN_AGE              12
#define HF_VERSION_EFFECTIVE_SHORT_TERM_MEDIAN_IN_PENALTY 12
#define HF_VERSION_EXACT_COINBASE               13
#define HF_VERSION_CLSAG                        13
#define HF_VERSION_DETERMINISTIC_UNLOCK_TIME    13
#define HF_VERSION_BULLETPROOF_PLUS             15
#define HF_VERSION_VIEW_TAGS                    15
#define HF_VERSION_2021_SCALING                 15

// HIDERING Phase 5 — Post-Quantum additive hard fork (Dilithium3 + Kyber768).
// HFv16 introduces the external Dilithium3 signature carried in tx `extra` and
// the Kyber768-based BQ... addresses. HF_HEIGHT_PQ is a placeholder height: the
// fork is registered in the schedule but inactive until mainnet reaches it.
//
// SECURITY (audit M-6): a hard fork registered with threshold 0 at a fixed height
// is a REAL consensus rule — at HF_HEIGHT_PQ every block MUST be major_version 16.
// If the PQ consensus rules (and the audit findings C-1/E-*) are not finalised and
// audited before the tip approaches this height, the chain would halt (no valid v16
// block can be produced). The placeholder is therefore pushed far above any realistic
// near-term tip. À AJUSTER QUAND LA SPEC PQC SERA FINALISÉE ET AUDITÉE (cible T2 2027).
#define HF_VERSION_PQ                           16
#define HF_HEIGHT_PQ                            2000000ULL

// Phase 5 (HFv16): tx_extra tag carrying the external Dilithium3 signature
// (pk + sig). Distinct from the classic tx_extra tags in tx_extra.h
// (0x00..0x04, 0xDE) — 0x06 is unused there. Only emitted/validated once
// hf_version >= HF_VERSION_PQ; pre-fork transactions never carry it.
static const uint8_t TX_EXTRA_TAG_PQ_SIG = 0x06;

// Phase 5 (HFv16): tx_extra tag carrying a Kyber768 KEM ciphertext (1088 bytes)
// for a BQ... post-quantum stealth output. One such field is emitted per BQ...
// destination, before the trailing Dilithium3 signature field. Tag 0x07 is unused
// by the classic tx_extra tags. Only emitted once hf_version >= HF_VERSION_PQ;
// pre-fork transactions never carry it.
static const uint8_t TX_EXTRA_TAG_KYBER_CT = 0x07;

#define PER_KB_FEE_QUANTIZATION_DECIMALS        8
#define CRYPTONOTE_SCALING_2021_FEE_ROUNDING_PLACES 2

#define HASH_OF_HASHES_STEP                     512

#define DEFAULT_TXPOOL_MAX_WEIGHT               648000000ull // 3 days at 300000, in bytes

#define BULLETPROOF_MAX_OUTPUTS                 16
#define BULLETPROOF_PLUS_MAX_OUTPUTS            16

#define CRYPTONOTE_PRUNING_STRIPE_SIZE          4096 // the smaller, the smoother the increase
#define CRYPTONOTE_PRUNING_LOG_STRIPES          3 // the higher, the more space saved
#define CRYPTONOTE_PRUNING_TIP_BLOCKS           5500 // the smaller, the more space saved

#define RPC_CREDITS_PER_HASH_SCALE ((float)(1<<24))

#define DNS_BLOCKLIST_LIFETIME (86400 * 8)

//The limit is enough for the mandatory transaction content with 16 outputs (547 bytes),
//a custom tag (1 byte) and up to 32 bytes of custom data for each recipient.
// (1+32) + (1+1+16*32) + (1+16*32) = 1060
#define MAX_TX_EXTRA_SIZE                       3000

// Phase 5 caveat 3 — CONSENSUS CHANGE, ACTIVATES WITH HFv16 ONLY.
// A post-quantum BQ... transaction must fit the Dilithium3 signature field
// (1 tag + pk 1952 + sig 3293 = 5246 bytes) plus one Kyber768 ciphertext per BQ
// output (1 tag + 1088 bytes), which blows past the classic 3000-byte limit.
// MAX_TX_EXTRA_SIZE stays 3000 for the live chain (hf < HF_VERSION_PQ) — the
// tx_pool relay check and non-PQ construction are unchanged. This larger ceiling
// is consulted ONLY on paths gated by hf_version >= HF_VERSION_PQ, so the current
// chain is untouched. When HFv16 activates, the gated check below becomes the
// effective limit for PQ transactions.
#define MAX_TX_EXTRA_SIZE_PQ                    8192

// New constants are intended to go here
namespace config
{
  uint64_t const DEFAULT_FEE_ATOMIC_XMR_PER_KB = 500; // Just a placeholder!  Change me!
  uint8_t const FEE_CALCULATION_MAX_RETRIES = 10;
  uint64_t const DEFAULT_DUST_THRESHOLD = ((uint64_t)2000000000); // 2 * pow(10, 9)
  uint64_t const BASE_REWARD_CLAMP_THRESHOLD = ((uint64_t)100000000); // pow(10, 8)

  uint64_t const CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX = 60;
  uint64_t const CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX = 61;
  uint64_t const CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX = 62;
  // Phase 5 (HFv16): post-quantum address tag carrying a Kyber768 public key.
  // get_account_address_{as,from}_str_pq() key on this NUMERIC value.
  //
  // Step 6 mnemonic-prefix tuning: the human-readable BQ... address now actually
  // begins "BQ". The leading base58 chars are a property of encode_addr over the full
  // (varint(tag)||payload||checksum) blob — they cannot be set by the tag alone, because
  // a leading 'B' forces a 1-byte tag in [60,65], leaving random spend-key bytes in the
  // first 8-byte base58 block (so the 2nd char would vary). The fix has two parts:
  //   1. tag = 62 (the only [60,65] value whose base 2nd-char digit is reachable to 'Q'
  //      by a positive marker byte; 60->'3',61->'D',62->'N'=21->+marker->'Q'=23, 63..65
  //      overshoot), and
  //   2. a fixed CRYPTONOTE_PQ_ADDRESS_MARKER byte prepended to the address payload, which
  //      pins the 2nd base58 char to 'Q' regardless of the (random) Ed25519/Kyber keys.
  // Empirically (see Step 6 search) tag 62 + marker in [0x29,0x41] yields a stable "BQ..."
  // across thousands of random payloads; 0x33 is used.
  //
  // COLLISION NOTE: 62 also equals CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX above. It is
  // the *only* tag that can render literal "BQ", so the prefix is intentionally shared.
  // This is unambiguous because the two address types differ in decoded payload SIZE
  // (subaddress = 2*32 = 64 bytes; BQ = marker + 2*32 + 1184 = 1249 bytes): each parser
  // size-checks before accepting, so a subaddress never parses as BQ and vice-versa.
  uint64_t const CRYPTONOTE_PQ_ADDRESS_PREFIX = 62;
  // Fixed leading byte of the BQ... address payload (before spend|view|kyber). Its sole
  // purpose is to pin the rendered base58 prefix to "BQ"; the parser validates it.
  uint8_t const CRYPTONOTE_PQ_ADDRESS_MARKER = 0x33;
  uint16_t const P2P_DEFAULT_PORT = 19740;
  uint16_t const RPC_DEFAULT_PORT = 19741;
  uint16_t const ZMQ_RPC_DEFAULT_PORT = 19742;
  boost::uuids::uuid const NETWORK_ID = { {
      0x48, 0x52, 0x47, 0x02, 0x48, 0x49, 0x44, 0x45, 0x52, 0x49, 0x4E, 0x47, 0x4D, 0x41, 0x49, 0x4E
    } }; // HIDERING mainnet v2.0.0 hard fork
  std::string const GENESIS_TX = "013c01ff00018090858fb0dd2302bb0e85edf7e295a84d172c26a759978dc1febea3e2608a240495368a74cdb2d22101f8ae2e22119ef501e05a9f8f347632f0ada2a69cee150d156c41d10df98fbe9f";
  uint32_t const GENESIS_NONCE = 10000; // HIDERING genesis: 157.14 HRG initial reward

  // Hash domain separators
  const char HASH_KEY_BULLETPROOF_EXPONENT[] = "bulletproof";
  const char HASH_KEY_BULLETPROOF_PLUS_EXPONENT[] = "bulletproof_plus";
  const char HASH_KEY_BULLETPROOF_PLUS_TRANSCRIPT[] = "bulletproof_plus_transcript";
  const char HASH_KEY_RINGDB[] = "ringdsb";
  const char HASH_KEY_SUBADDRESS[] = "SubAddr";
  const unsigned char HASH_KEY_ENCRYPTED_PAYMENT_ID = 0x8d;
  const unsigned char HASH_KEY_WALLET = 0x8c;
  const unsigned char HASH_KEY_WALLET_CACHE = 0x8d;
  const unsigned char HASH_KEY_BACKGROUND_CACHE = 0x8e;
  const unsigned char HASH_KEY_BACKGROUND_KEYS_FILE = 0x8f;
  const unsigned char HASH_KEY_RPC_PAYMENT_NONCE = 0x58;
  const unsigned char HASH_KEY_MEMORY = 'k';
  const unsigned char HASH_KEY_MULTISIG[] = {'M', 'u', 'l', 't' , 'i', 's', 'i', 'g', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  const unsigned char HASH_KEY_MULTISIG_KEY_AGGREGATION[] = "Multisig_key_agg";
  const unsigned char HASH_KEY_CLSAG_ROUND_MULTISIG[] = "CLSAG_round_ms_merge_factor";
  const unsigned char HASH_KEY_TXPROOF_V2[] = "TXPROOF_V2";
  const unsigned char HASH_KEY_CLSAG_ROUND[] = "CLSAG_round";
  const unsigned char HASH_KEY_CLSAG_AGG_0[] = "CLSAG_agg_0";
  const unsigned char HASH_KEY_CLSAG_AGG_1[] = "CLSAG_agg_1";
  const char HASH_KEY_MESSAGE_SIGNING[] = "HideringMessageSignature";
  const unsigned char HASH_KEY_MM_SLOT = 'm';
  const constexpr char HASH_KEY_MULTISIG_TX_PRIVKEYS_SEED[] = "multisig_tx_privkeys_seed";
  const constexpr char HASH_KEY_MULTISIG_TX_PRIVKEYS[] = "multisig_tx_privkeys";
  const constexpr char HASH_KEY_TXHASH_AND_MIXRING[] = "txhash_and_mixring";

  // Multisig
  const uint32_t MULTISIG_MAX_SIGNERS{16};

  namespace testnet
  {
    uint64_t const CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX = 53;
    uint64_t const CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX = 54;
    uint64_t const CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX = 63;
    uint16_t const P2P_DEFAULT_PORT = 29740;
    uint16_t const RPC_DEFAULT_PORT = 29741;
    uint16_t const ZMQ_RPC_DEFAULT_PORT = 29742;
    boost::uuids::uuid const NETWORK_ID = { {
        0x48, 0x52, 0x47, 0x02, 0x48, 0x49, 0x44, 0x45, 0x52, 0x49, 0x4E, 0x47, 0x54, 0x45, 0x53, 0x54
      } }; // HIDERING testnet
    std::string const GENESIS_TX = "013c01ff00018090858fb0dd2302bb0e85edf7e295a84d172c26a759978dc1febea3e2608a240495368a74cdb2d22101f8ae2e22119ef501e05a9f8f347632f0ada2a69cee150d156c41d10df98fbe9f";
    uint32_t const GENESIS_NONCE = 10001;
  }

  namespace stagenet
  {
    uint64_t const CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX = 24;
    uint64_t const CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX = 25;
    uint64_t const CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX = 36;
    uint16_t const P2P_DEFAULT_PORT = 39740;
    uint16_t const RPC_DEFAULT_PORT = 39741;
    uint16_t const ZMQ_RPC_DEFAULT_PORT = 39742;
    boost::uuids::uuid const NETWORK_ID = { {
        0x48, 0x52, 0x47, 0x03, 0x48, 0x49, 0x44, 0x45, 0x52, 0x49, 0x4E, 0x47, 0x53, 0x54, 0x41, 0x47
      } }; // HIDERING stagenet
    std::string const GENESIS_TX = "013c01ff00018090858fb0dd2302bb0e85edf7e295a84d172c26a759978dc1febea3e2608a240495368a74cdb2d22101f8ae2e22119ef501e05a9f8f347632f0ada2a69cee150d156c41d10df98fbe9f";
    uint32_t const GENESIS_NONCE = 10002;
  }
}

namespace cryptonote
{
  enum network_type : uint8_t
  {
    MAINNET = 0,
    TESTNET,
    STAGENET,
    FAKECHAIN,
    UNDEFINED = 255
  };
  struct config_t
  {
    uint64_t const CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX;
    uint64_t const CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX;
    uint64_t const CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX;
    uint16_t const P2P_DEFAULT_PORT;
    uint16_t const RPC_DEFAULT_PORT;
    uint16_t const ZMQ_RPC_DEFAULT_PORT;
    boost::uuids::uuid const NETWORK_ID;
    std::string const GENESIS_TX;
    uint32_t const GENESIS_NONCE;
  };
  inline const config_t& get_config(network_type nettype)
  {
    static const config_t mainnet = {
      ::config::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
      ::config::CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX,
      ::config::CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX,
      ::config::P2P_DEFAULT_PORT,
      ::config::RPC_DEFAULT_PORT,
      ::config::ZMQ_RPC_DEFAULT_PORT,
      ::config::NETWORK_ID,
      ::config::GENESIS_TX,
      ::config::GENESIS_NONCE
    };
    static const config_t testnet = {
      ::config::testnet::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
      ::config::testnet::CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX,
      ::config::testnet::CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX,
      ::config::testnet::P2P_DEFAULT_PORT,
      ::config::testnet::RPC_DEFAULT_PORT,
      ::config::testnet::ZMQ_RPC_DEFAULT_PORT,
      ::config::testnet::NETWORK_ID,
      ::config::testnet::GENESIS_TX,
      ::config::testnet::GENESIS_NONCE
    };
    static const config_t stagenet = {
      ::config::stagenet::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
      ::config::stagenet::CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX,
      ::config::stagenet::CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX,
      ::config::stagenet::P2P_DEFAULT_PORT,
      ::config::stagenet::RPC_DEFAULT_PORT,
      ::config::stagenet::ZMQ_RPC_DEFAULT_PORT,
      ::config::stagenet::NETWORK_ID,
      ::config::stagenet::GENESIS_TX,
      ::config::stagenet::GENESIS_NONCE
    };
    switch (nettype)
    {
      case MAINNET: return mainnet;
      case TESTNET: return testnet;
      case STAGENET: return stagenet;
      case FAKECHAIN: return mainnet;
      default: throw std::runtime_error("Invalid network type");
    }
  };
}
