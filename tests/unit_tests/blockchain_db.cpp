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

#include <algorithm>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <cstdio>
#include <iostream>
#include <chrono>
#include <thread>

#include "gtest/gtest.h"

#include "string_tools.h"
#include "blockchain_db/blockchain_db.h"
#include "blockchain_db/lmdb/db_lmdb.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "ringct/rctOps.h"
#include "ringct/rctSigs.h"
#include "cryptonote_basic/account.h"
#include "cryptonote_core/cryptonote_tx_utils.h"

using namespace cryptonote;
using epee::string_tools::pod_to_hex;

#define ASSERT_HASH_EQ(a,b) ASSERT_EQ(pod_to_hex(a), pod_to_hex(b))

namespace {  // anonymous namespace

const std::vector<std::string> t_blocks =
  {
    "0100d5adc49a053b8818b2b6023cd2d532c6774e164a8fcacd603651cb3ea0cb7f9340b28ec016b4bc4ca301aa0101ff6e08acbb2702eab03067870349139bee7eab2ca2e030a6bb73d4f68ab6a3b6ca937214054cdac0843d028bbe23b57ea9bae53f12da93bb57bf8a2e40598d9fccd10c2921576e987d93cd80b4891302468738e391f07c4f2b356f7957160968e0bfef6e907c3cee2d8c23cbf04b089680c6868f01025a0f41f063e195a966051e3a29e17130a9ce97d48f55285b9bb04bdd55a09ae78088aca3cf0202d0f26169290450fe17e08974789c3458910b4db18361cdc564f8f2d0bdd2cf568090cad2c60e02d6f3483ec45505cc3be841046c7a12bf953ac973939bc7b727e54258e1881d4d80e08d84ddcb0102dae6dfb16d3e28aaaf43e00170b90606b36f35f38f8a3dceb5ee18199dd8f17c80c0caf384a30202385d7e57a4daba4cdd9e550a92dcc188838386e7581f13f09de796cbed4716a42101c052492a077abf41996b50c1b2e67fd7288bcd8c55cdc657b4e22d0804371f6901beb76a82ea17400cd6d7f595f70e1667d2018ed8f5a78d1ce07484222618c3cd"
  , "0100f9adc49a057d3113f562eac36f14afa08c22ae20bbbf8cffa31a4466d24850732cb96f80e9762365ee01ab0101ff6f08cc953502be76deb845c431f2ed9a4862457654b914003693b8cd672abc935f0d97b16380c08db7010291819f2873e3efbae65ecd5a736f5e8a26318b591c21e39a03fb536520ac63ba80dac40902439a10fde02e39e48e0b31e57cc084a07eedbefb8cbea0143aedd0442b189caa80c6868f010227b84449de4cd7a48cbdce8974baf0b6646e03384e32055e705c243a86bef8a58088aca3cf0202fa7bd15e4e7e884307ab130bb9d50e33c5fcea6546042a26f948efd5952459ee8090cad2c60e028695583dbb8f8faab87e3ef3f88fa827db097bbf51761d91924f5c5b74c6631780e08d84ddcb010279d2f247b54690e3b491e488acff16014a825fd740c23988a25df7c4670c1f2580c0caf384a302022599dfa3f8788b66295051d85937816e1c320cdb347a0fba5219e3fe60c83b2421010576509c5672025d28fd5d3f38efce24e1f9aaf65dd3056b2504e6e2b7f19f7800"
  };

const std::vector<size_t> t_sizes =
  {
    1122
  , 347
  };

const std::vector<difficulty_type> t_diffs =
  {
    4003674
  , 4051757
  };

const std::vector<uint64_t> t_coins =
  {
    1952630229575370
  , 1970220553446486
  };

const std::vector<std::vector<std::string>> t_transactions =
  {
    {
      "0100010280e08d84ddcb0106010401110701f254220bb50d901a5523eaed438af5d43f8c6d0e54ba0632eb539884f6b7c02008c0a8a50402f9c7cf807ae74e56f4ec84db2bd93cfb02c2249b38e306f5b54b6e05d00d543b8095f52a02b6abb84e00f47f0a72e37b6b29392d906a38468404c57db3dbc5e8dd306a27a880d293ad0302cfc40a86723e7d459e90e45d47818dc0e81a1f451ace5137a4af8110a89a35ea80b4c4c321026b19c796338607d5a2c1ba240a167134142d72d1640ef07902da64fed0b10cfc8088aca3cf02021f6f655254fee84161118b32e7b6f8c31de5eb88aa00c29a8f57c0d1f95a24dd80d0b8e1981a023321af593163cea2ae37168ab926efd87f195756e3b723e886bdb7e618f751c480a094a58d1d0295ed2b08d1cf44482ae0060a5dcc4b7d810a85dea8c62e274f73862f3d59f8ed80a0e5b9c2910102dc50f2f28d7ceecd9a1147f7106c8d5b4e08b2ec77150f52dd7130ee4f5f50d42101d34f90ac861d0ee9fe3891656a234ea86a8a93bf51a237db65baa00d3f4aa196a9e1d89bc06b40e94ea9a26059efc7ba5b2de7ef7c139831ca62f3fe0bb252008f8c7ee810d3e1e06313edf2db362fc39431755779466b635f12f9f32e44470a3e85e08a28fcd90633efc94aa4ae39153dfaf661089d045521343a3d63e8da08d7916753c66aaebd4eefcfe8e58e5b3d266b752c9ca110749fa33fce7c44270386fcf2bed4f03dd5dadb2dc1fd4c505419f8217b9eaec07521f0d8963e104603c926745039cf38d31de6ed95ace8e8a451f5a36f818c151f517546d55ac0f500e54d07b30ea7452f2e93fa4f60bdb30d71a0a97f97eb121e662006780fbf69002228224a96bff37893d47ec3707b17383906c0cd7d9e7412b3e6c8ccf1419b093c06c26f96e3453b424713cdc5c9575f81cda4e157052df11f4c40809edf420f88a3dd1f7909bbf77c8b184a933389094a88e480e900bcdbf6d1824742ee520fc0032e7d892a2b099b8c6edfd1123ce58a34458ee20cad676a7f7cfd80a28f0cb0888af88838310db372986bdcf9bfcae2324480ca7360d22bff21fb569a530e"
    }
  , {
    }
  };

// if the return type (blobdata for now) of block_to_blob ever changes
// from std::string, this might break.
bool compare_blocks(const block& a, const block& b)
{
  auto hash_a = pod_to_hex(get_block_hash(a));
  auto hash_b = pod_to_hex(get_block_hash(b));

  return hash_a == hash_b;
}

/*
void print_block(const block& blk, const std::string& prefix = "")
{
  std::cerr << prefix << ": " << std::endl
            << "\thash - " << pod_to_hex(get_block_hash(blk)) << std::endl
            << "\tparent - " << pod_to_hex(blk.prev_id) << std::endl
            << "\ttimestamp - " << blk.timestamp << std::endl
  ;
}

// if the return type (blobdata for now) of tx_to_blob ever changes
// from std::string, this might break.
bool compare_txs(const transaction& a, const transaction& b)
{
  auto ab = tx_to_blob(a);
  auto bb = tx_to_blob(b);

  return ab == bb;
}
*/

// convert hex string to string that has values based on that hex
// thankfully should automatically ignore null-terminator.
std::string h2b(const std::string& s)
{
  bool upper = true;
  std::string result;
  unsigned char val = 0;
  for (char c : s)
  {
    if (upper)
    {
      val = 0;
      if (c <= 'f' && c >= 'a')
      {
        val = ((c - 'a') + 10) << 4;
      }
      else
      {
        val = (c - '0') << 4;
      }
    }
    else
    {
      if (c <= 'f' && c >= 'a')
      {
        val |= (c - 'a') + 10;
      }
      else
      {
        val |= c - '0';
      }
      result += (char)val;
    }
    upper = !upper;
  }
  return result;
}

template <typename T>
class BlockchainDBTest : public testing::Test
{
protected:
  BlockchainDBTest() : m_db(new T()), m_hardfork(*m_db, 1, 0)
  {
    for (auto& i : t_blocks)
    {
      block bl;
      blobdata bd = h2b(i);
      CHECK_AND_ASSERT_THROW_MES(parse_and_validate_block_from_blob(bd, bl), "Invalid block");
      m_blocks.push_back(std::make_pair(bl, bd));
    }
    for (auto& i : t_transactions)
    {
      std::vector<std::pair<transaction, blobdata>> txs;
      for (auto& j : i)
      {
        transaction tx;
        blobdata bd = h2b(j);
        CHECK_AND_ASSERT_THROW_MES(parse_and_validate_tx_from_blob(bd, tx), "Invalid transaction");
        txs.push_back(std::make_pair(tx, bd));
      }
      m_txs.push_back(txs);
    }
  }

  ~BlockchainDBTest() {
    delete m_db;
    remove_files();
  }

  BlockchainDB* m_db;
  HardFork m_hardfork;
  std::string m_prefix;
  std::vector<std::pair<block, blobdata>> m_blocks;
  std::vector<std::vector<std::pair<transaction, blobdata>>> m_txs;
  std::vector<std::string> m_filenames;

  void init_hard_fork()
  {
    m_hardfork.init();
    m_db->set_hard_fork(&m_hardfork);
  }

  void get_filenames()
  {
    m_filenames = m_db->get_filenames();
    for (auto& f : m_filenames)
    {
      std::cerr << "File created by test: " << f << std::endl;
    }
  }

  void remove_files()
  {
    // remove each file the db created, making sure it starts with fname.
    for (auto& f : m_filenames)
    {
      if (boost::starts_with(f, m_prefix))
      {
        boost::filesystem::remove(f);
      }
      else
      {
        std::cerr << "File created by test not to be removed (for safety): " << f << std::endl;
      }
    }

    // remove directory if it still exists
    boost::filesystem::remove_all(m_prefix);
  }

  void set_prefix(const std::string& prefix)
  {
    m_prefix = prefix;
  }
};

using testing::Types;

typedef Types<BlockchainLMDB> implementations;

TYPED_TEST_CASE(BlockchainDBTest, implementations);

TYPED_TEST(BlockchainDBTest, OpenAndClose)
{
  boost::filesystem::path tempPath = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
  std::string dirPath = tempPath.string();

  this->set_prefix(dirPath);

  // make sure open does not throw
  ASSERT_NO_THROW(this->m_db->open(dirPath));
  this->get_filenames();

  // make sure open when already open DOES throw
  ASSERT_THROW(this->m_db->open(dirPath), DB_OPEN_FAILURE);

  ASSERT_NO_THROW(this->m_db->close());
}

TYPED_TEST(BlockchainDBTest, AddBlock)
{

  boost::filesystem::path tempPath = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
  std::string dirPath = tempPath.string();

  this->set_prefix(dirPath);

  // make sure open does not throw
  ASSERT_NO_THROW(this->m_db->open(dirPath));
  this->get_filenames();
  this->init_hard_fork();

  db_wtxn_guard guard(this->m_db);

  // adding a block with no parent in the blockchain should throw.
  // note: this shouldn't be possible, but is a good (and cheap) failsafe.
  //
  // TODO: need at least one more block to make this reasonable, as the
  // BlockchainDB implementation should not check for parent if
  // no blocks have been added yet (because genesis has no parent).
  //ASSERT_THROW(this->m_db->add_block(this->m_blocks[1], t_sizes[1], t_sizes[1], t_diffs[1], t_coins[1], this->m_txs[1]), BLOCK_PARENT_DNE);

  ASSERT_NO_THROW(this->m_db->add_block(this->m_blocks[0], t_sizes[0], t_sizes[0], t_diffs[0], t_coins[0], this->m_txs[0]));
  ASSERT_NO_THROW(this->m_db->add_block(this->m_blocks[1], t_sizes[1], t_sizes[1], t_diffs[1], t_coins[1], this->m_txs[1]));

  block b;
  ASSERT_TRUE(this->m_db->block_exists(get_block_hash(this->m_blocks[0].first)));
  ASSERT_NO_THROW(b = this->m_db->get_block(get_block_hash(this->m_blocks[0].first)));

  ASSERT_TRUE(compare_blocks(this->m_blocks[0].first, b));

  ASSERT_NO_THROW(b = this->m_db->get_block_from_height(0));

  ASSERT_TRUE(compare_blocks(this->m_blocks[0].first, b));

  // assert that we can't add the same block twice
  ASSERT_THROW(this->m_db->add_block(this->m_blocks[0], t_sizes[0], t_sizes[0], t_diffs[0], t_coins[0], this->m_txs[0]), TX_EXISTS);

  for (auto& h : this->m_blocks[0].first.tx_hashes)
  {
    transaction tx;
    ASSERT_TRUE(this->m_db->tx_exists(h));
    ASSERT_NO_THROW(tx = this->m_db->get_tx(h));

    ASSERT_HASH_EQ(h, get_transaction_hash(tx));
  }
}

TYPED_TEST(BlockchainDBTest, RetrieveBlockData)
{
  boost::filesystem::path tempPath = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
  std::string dirPath = tempPath.string();

  this->set_prefix(dirPath);

  // make sure open does not throw
  ASSERT_NO_THROW(this->m_db->open(dirPath));
  this->get_filenames();
  this->init_hard_fork();

  db_wtxn_guard guard(this->m_db);

  ASSERT_NO_THROW(this->m_db->add_block(this->m_blocks[0], t_sizes[0], t_sizes[0],  t_diffs[0], t_coins[0], this->m_txs[0]));

  ASSERT_EQ(t_sizes[0], this->m_db->get_block_weight(0));
  ASSERT_EQ(t_diffs[0], this->m_db->get_block_cumulative_difficulty(0));
  ASSERT_EQ(t_diffs[0], this->m_db->get_block_difficulty(0));
  ASSERT_EQ(t_coins[0], this->m_db->get_block_already_generated_coins(0));

  ASSERT_NO_THROW(this->m_db->add_block(this->m_blocks[1], t_sizes[1], t_sizes[1], t_diffs[1], t_coins[1], this->m_txs[1]));
  ASSERT_EQ(t_diffs[1] - t_diffs[0], this->m_db->get_block_difficulty(1));

  ASSERT_HASH_EQ(get_block_hash(this->m_blocks[0].first), this->m_db->get_block_hash_from_height(0));

  std::vector<block> blks;
  ASSERT_NO_THROW(blks = this->m_db->get_blocks_range(0, 1));
  ASSERT_EQ(2, blks.size());
  
  ASSERT_HASH_EQ(get_block_hash(this->m_blocks[0].first), get_block_hash(blks[0]));
  ASSERT_HASH_EQ(get_block_hash(this->m_blocks[1].first), get_block_hash(blks[1]));

  std::vector<crypto::hash> hashes;
  ASSERT_NO_THROW(hashes = this->m_db->get_hashes_range(0, 1));
  ASSERT_EQ(2, hashes.size());

  ASSERT_HASH_EQ(get_block_hash(this->m_blocks[0].first), hashes[0]);
  ASSERT_HASH_EQ(get_block_hash(this->m_blocks[1].first), hashes[1]);
}

}  // anonymous namespace

// ---------------------------------------------------------------------------------------------
// HIDERING Phase 5 (HFv16, A3) — regression test for CRIT-1 (audit 7 Sep 2026).
//
// THE BUG. A transparent BQ spend (a tx carrying a txin_to_key_pq input) publishes its outputs
// with REVEALED amounts, because its balance is plain arithmetic instead of RingCT. Stored
// naively those outputs landed in the per-amount bucket tx.vout[i].amount, while the consensus
// validator resolves a PQ input in the RingCT bucket 0:
//     Blockchain::check_tx_inputs, check (a): m_db->get_output_key((uint64_t)0, idx, true)
//     Blockchain::check_tx_inputs, check (c): m_db->get_output_tx_and_index((uint64_t)0, idx)
// The lookup therefore missed (or resolved a completely unrelated output, failing check (b) on
// the pubkey), so EVERY BQ output created by a BQ spend — the CHANGE above all — was permanently
// unspendable. That is guaranteed fund loss on the very first BQ->BQ chain: spend once, and the
// change is gone forever.
//
// It was invisible to the A4 end-to-end run because the only BQ output exercised there came from
// a classic RingCT B...->BQ... tx, whose output amounts ARE zeroed, hence bucket 0 — the one case
// that happened to work.
//
// THE FIX (mirroring what Monero already does for v2 coinbase outputs, which are likewise v2
// outputs with a revealed amount): normalise the DB representation, not the wire format. Store
// transparent-BQ outputs in bucket 0 as rct outputs with an identity-mask commitment. The tx on
// the wire keeps its revealed amounts (needed by money-conservation check (e) and by the wallet);
// only the amount bucket is normalised, so all BQ outputs share one uniform index space
// regardless of how they were produced.
//
// WHAT THIS TEST DOES. It drives a real BlockchainLMDB and asserts the exact primitive pair the
// validator uses. Before the fix, get_output_key(0, idx) throws OUTPUT_DNE (the output sits in
// bucket 42'000'000'000'000) and the test fails on the first EXPECT — reproducing the defect
// precisely. After the fix both lookups resolve and agree with the on-chain output key.
// It also covers the reorg direction: remove_tx_outputs must pick the SAME bucket, or popping the
// block would delete the wrong output / throw and corrupt the DB.
TYPED_TEST(BlockchainDBTest, PqTransparentOutputsLiveInRctBucketZero)
{
  boost::filesystem::path tempPath = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
  std::string dirPath = tempPath.string();
  this->set_prefix(dirPath);

  ASSERT_NO_THROW(this->m_db->open(dirPath));
  this->get_filenames();
  this->init_hard_fork();

  uint64_t rct_outs_before = 0;
  {
  // scoped: pop_block() opens its own write txn, so the guard must be closed before the reorg leg
  db_wtxn_guard guard(this->m_db);

  // a base block so the chain is non-empty and bucket 0 already holds real rct outputs
  ASSERT_NO_THROW(this->m_db->add_block(this->m_blocks[0], t_sizes[0], t_sizes[0], t_diffs[0], t_coins[0], this->m_txs[0]));
  rct_outs_before = this->m_db->get_num_outputs(0);

  // ---- build a transparent BQ spend: v2 + RCTTypeNull + txin_to_key_pq + revealed amounts ----
  // This is the shape construct_tx produces on the pq_transparent_tx branch, and the shape the
  // change output of any BQ->BQ spend has.
  const uint64_t BQ_CHANGE_AMOUNT = 42000000000000ull; // revealed, NOT a decomposed amount
  transaction pq_tx{};
  pq_tx.version = 2;
  pq_tx.rct_signatures.type = rct::RCTTypeNull;       // no commitments: outPk is empty
  txin_to_key_pq pq_in{};
  pq_in.amount = BQ_CHANGE_AMOUNT;
  pq_in.spent_output_index = 0;
  for (size_t i = 0; i < sizeof(pq_in.real_output_key); ++i)
    ((uint8_t*)&pq_in.real_output_key)[i] = (uint8_t)(0x40 + i);
  pq_tx.vin.push_back(pq_in);

  crypto::public_key bq_out_key;
  for (size_t i = 0; i < sizeof(bq_out_key); ++i)
    ((uint8_t*)&bq_out_key)[i] = (uint8_t)(0xA0 + i);
  tx_out bq_out{};
  bq_out.amount = BQ_CHANGE_AMOUNT;                   // REVEALED (this is the crux of the bug)
  txout_to_tagged_key tagged{};
  tagged.key = bq_out_key;
  tagged.view_tag = crypto::view_tag{};
  bq_out.target = tagged;
  pq_tx.vout.push_back(bq_out);

  // splice it into the next block
  std::pair<block, blobdata> blk = this->m_blocks[1];
  const crypto::hash pq_txid = get_transaction_hash(pq_tx);
  blk.first.tx_hashes.clear();
  blk.first.tx_hashes.push_back(pq_txid);
  std::vector<std::pair<transaction, blobdata>> blk_txs;
  blk_txs.push_back(std::make_pair(pq_tx, tx_to_blob(pq_tx)));

  ASSERT_NO_THROW(this->m_db->add_block(blk, t_sizes[1], t_sizes[1], t_diffs[1], t_coins[1], blk_txs));

  // ---- the assertions that failed before the fix ----

  // the BQ output must have been indexed in the RingCT bucket, not in bucket BQ_CHANGE_AMOUNT
  EXPECT_EQ(rct_outs_before + 1, this->m_db->get_num_outputs(0))
      << "transparent BQ output was not indexed in the rct bucket 0";
  EXPECT_EQ(0u, this->m_db->get_num_outputs(BQ_CHANGE_AMOUNT))
      << "transparent BQ output leaked into a per-amount bucket; check_tx_inputs looks in bucket 0 only";

  // the amount output index the daemon hands wallets must address bucket 0
  const std::vector<std::vector<uint64_t>> amount_indices =
      this->m_db->get_tx_amount_output_indices(this->m_db->get_tx_count() - 1, 1);
  ASSERT_EQ(1u, amount_indices.size());
  ASSERT_EQ(1u, amount_indices.front().size());
  const uint64_t bq_global_index = amount_indices.front()[0];

  // validator check (a)+(b): resolve in bucket 0 and match the revealed output key.
  // BEFORE THE FIX this throws OUTPUT_DNE -> the BQ change is unspendable forever.
  output_data_t od{};
  ASSERT_NO_THROW(od = this->m_db->get_output_key((uint64_t)0, bq_global_index, true))
      << "get_output_key(0, idx) missed the BQ output => validator check (a) fails => funds lost";
  EXPECT_EQ(bq_out_key, od.pubkey)
      << "bucket-0 lookup resolved a DIFFERENT output => validator check (b) fails => funds lost";

  // validator check (c): the creating tx must be reachable through the same bucket
  tx_out_index toi;
  ASSERT_NO_THROW(toi = this->m_db->get_output_tx_and_index((uint64_t)0, bq_global_index))
      << "get_output_tx_and_index(0, idx) missed the BQ output => validator check (c) fails";
  ASSERT_HASH_EQ(pq_txid, toi.first);
  EXPECT_EQ(0u, toi.second);
  } // close the write txn before the reorg leg

  // reorg direction: removal must target the same bucket, otherwise the DB is corrupted
  block popped_blk;
  std::vector<transaction> popped_txs;
  ASSERT_NO_THROW(this->m_db->pop_block(popped_blk, popped_txs))
      << "popping a block holding a transparent BQ output failed => remove_tx_outputs used the wrong bucket";
  EXPECT_EQ(rct_outs_before, this->m_db->get_num_outputs(0))
      << "the BQ output was not removed from bucket 0 on reorg";
}

// HIDERING Phase 5 — the per-block RingCT output count must follow the SAME rule as the bucket-0
// storage above. The count (bi_cum_rct) is what get_output_distribution hands wallets: gamma decoy
// selection draws from it, and wallet2's get_outs sanity check refuses to build any transaction
// once it lags the real bucket-0 index space ("Daemon reports suspicious number of rct outputs").
// It used to count `vout.amount == 0` only, so every transparent BQ output — stored in bucket 0,
// but with its amount revealed — was missing from the distribution: one output lost per output.
// Found by tests/pq_e2e (the distribution fell 12 behind bucket 0 after a few BQ spends, and a
// later BQ spend failed with "failed to get output distribution").
TYPED_TEST(BlockchainDBTest, PqTransparentOutputsCountedInRctDistribution)
{
  boost::filesystem::path tempPath = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
  std::string dirPath = tempPath.string();
  this->set_prefix(dirPath);

  ASSERT_NO_THROW(this->m_db->open(dirPath));
  this->get_filenames();
  this->init_hard_fork();

  db_wtxn_guard guard(this->m_db);

  ASSERT_NO_THROW(this->m_db->add_block(this->m_blocks[0], t_sizes[0], t_sizes[0], t_diffs[0], t_coins[0], this->m_txs[0]));
  const uint64_t bucket0_before = this->m_db->get_num_outputs(0);
  const uint64_t cum_before = this->m_db->get_block_cumulative_rct_outputs({0}).front();

  // a transparent BQ spend with two REVEALED, non-zero outputs (payment + change) and a zero one
  transaction pq_tx{};
  pq_tx.version = 2;
  pq_tx.rct_signatures.type = rct::RCTTypeNull;
  txin_to_key_pq pq_in{};
  pq_in.amount = 30000000000000ull;
  for (size_t i = 0; i < sizeof(pq_in.real_output_key); ++i)
    ((uint8_t*)&pq_in.real_output_key)[i] = (uint8_t)(0x50 + i);
  pq_tx.vin.push_back(pq_in);
  const uint64_t amounts[] = {20000000000000ull, 9990000000000ull, 0};
  for (size_t o = 0; o < 3; ++o)
  {
    tx_out out{};
    out.amount = amounts[o];
    txout_to_tagged_key tagged{};
    for (size_t i = 0; i < sizeof(tagged.key); ++i)
      ((uint8_t*)&tagged.key)[i] = (uint8_t)(0xB0 + 16 * o + i);
    out.target = tagged;
    pq_tx.vout.push_back(out);
  }

  std::pair<block, blobdata> blk = this->m_blocks[1];
  blk.first.tx_hashes.clear();
  blk.first.tx_hashes.push_back(get_transaction_hash(pq_tx));
  std::vector<std::pair<transaction, blobdata>> blk_txs;
  blk_txs.push_back(std::make_pair(pq_tx, tx_to_blob(pq_tx)));
  ASSERT_NO_THROW(this->m_db->add_block(blk, t_sizes[1], t_sizes[1], t_diffs[1], t_coins[1], blk_txs));

  const uint64_t bucket0_added = this->m_db->get_num_outputs(0) - bucket0_before;
  const uint64_t cum_after = this->m_db->get_block_cumulative_rct_outputs({1}).front();
  EXPECT_EQ(bucket0_added, cum_after - cum_before)
      << "the block's RingCT output count differs from what bucket 0 received";
  EXPECT_EQ(this->m_db->get_num_outputs(0), cum_after)
      << "the cumulative RingCT count (get_output_distribution) lags the bucket-0 index space";
  // all three transparent outputs, the revealed non-zero ones included, are in bucket 0
  EXPECT_LE(3u, bucket0_added);
}

// HIDERING Phase 5 — HYBRID transactions (ring inputs + transparent BQ inputs) and the commitment
// their outputs are stored with.
//
// A hybrid tx is a genuine RingCT transaction: its outputs have amount 0 on the wire, and their
// real Pedersen commitments sit in rct_signatures.outPk[i].mask (the transparent PQ inputs enter
// the balance as a public term, see construct_tx's "structure H2"). The outputs_stored_as_pseudo_rct
// rule, however, fires on has_transparent_pq_input(tx) alone — true for a hybrid too — and then
// stores every output with zeroCommit(vout.amount) = zeroCommit(0) = G instead of outPk[i].mask.
//
// The wallet keeps the real mask (vout.amount == 0 -> td.m_mask from the ECDH decode), and the
// verifier reads the stored commitment (mixRing from get_output_key, check b2 for a BQ output), so
// any output of a hybrid tx would become unspendable at its real value. This test pins the storage
// primitive: each output of a hybrid tx must be stored with exactly outPk[i].mask.
//
// The synthetic tx only has to be stored, not verified: the CLSAG / range proof contents are
// placeholders, sized so the tx serialises (the DB needs a blob to split pruned/prunable).
TYPED_TEST(BlockchainDBTest, HybridTxOutputsKeepTheirRctCommitments)
{
  boost::filesystem::path tempPath = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
  std::string dirPath = tempPath.string();
  this->set_prefix(dirPath);

  ASSERT_NO_THROW(this->m_db->open(dirPath));
  this->get_filenames();
  this->init_hard_fork();

  db_wtxn_guard guard(this->m_db);

  ASSERT_NO_THROW(this->m_db->add_block(this->m_blocks[0], t_sizes[0], t_sizes[0], t_diffs[0], t_coins[0], this->m_txs[0]));
  const uint64_t bucket0_before = this->m_db->get_num_outputs(0);

  // ---- a hybrid tx: one ring input first, then one transparent BQ input (consensus order) ----
  const size_t RING_SIZE = 32;
  transaction hy{};
  hy.version = 2;

  txin_to_key ring_in{};
  ring_in.amount = 0;
  for (size_t m = 0; m < RING_SIZE; ++m)
    ring_in.key_offsets.push_back(1);
  ring_in.k_image = rct::rct2ki(rct::pkGen());
  hy.vin.push_back(ring_in);

  txin_to_key_pq pq_in{};
  pq_in.amount = 15000000000000ull;
  pq_in.spent_output_index = 0;
  pq_in.real_output_key = rct::rct2pk(rct::pkGen());
  hy.vin.push_back(pq_in);
  ASSERT_TRUE(has_transparent_pq_input(hy)) << "precondition: a hybrid tx has a transparent PQ input";

  // two RingCT outputs: amount 0 on the wire, the value is in the commitment
  const size_t N_OUT = 2;
  const uint64_t out_amounts[N_OUT] = {12000000000000ull, 7990000000000ull};
  std::vector<crypto::public_key> out_keys;
  for (size_t o = 0; o < N_OUT; ++o)
  {
    tx_out out{};
    out.amount = 0;
    txout_to_tagged_key tagged{};
    tagged.key = rct::rct2pk(rct::pkGen());
    tagged.view_tag = crypto::view_tag{};
    out.target = tagged;
    out_keys.push_back(tagged.key);
    hy.vout.push_back(out);
  }

  rct::rctSig &rv = hy.rct_signatures;
  rv.type = rct::RCTTypeBulletproofPlus;
  rv.txnFee = 10000000000ull;
  for (size_t o = 0; o < N_OUT; ++o)
  {
    rct::ctkey pk;
    pk.dest = rct::pk2rct(out_keys[o]);
    pk.mask = rct::commit(out_amounts[o], rct::skGen());   // a real, non-trivial commitment
    ASSERT_FALSE(pk.mask == rct::zeroCommit(0)) << "precondition: outPk mask differs from zeroCommit(0)";
    ASSERT_FALSE(pk.mask == rct::zeroCommit(out_amounts[o]));
    rv.outPk.push_back(pk);
    rct::ecdhTuple e{};
    rv.ecdhInfo.push_back(e);
  }
  // placeholder prunable data, sized to serialise (one BP+ covering 2 outputs: L,R of 7 keys);
  // one CLSAG and one pseudoOut for the one RING input, as genRctSimple produces for a hybrid
  rct::BulletproofPlus bpp{};
  bpp.L.assign(7, rct::identity());
  bpp.R.assign(7, rct::identity());
  rv.p.bulletproofs_plus.push_back(bpp);
  rct::clsag c{};
  c.s.assign(RING_SIZE, rct::identity());
  rv.p.CLSAGs.push_back(c);
  rv.p.pseudoOuts.push_back(rct::identity());

  const blobdata hy_blob = tx_to_blob(hy);
  ASSERT_FALSE(hy_blob.empty()) << "precondition: the synthetic hybrid tx serialises";

  std::pair<block, blobdata> blk = this->m_blocks[1];
  blk.first.tx_hashes.clear();
  blk.first.tx_hashes.push_back(get_transaction_hash(hy));
  std::vector<std::pair<transaction, blobdata>> blk_txs;
  blk_txs.push_back(std::make_pair(hy, hy_blob));
  ASSERT_NO_THROW(this->m_db->add_block(blk, t_sizes[1], t_sizes[1], t_diffs[1], t_coins[1], blk_txs));

  // both outputs are RingCT outputs: bucket 0 either way (this part is not in question)
  ASSERT_EQ(bucket0_before + N_OUT, this->m_db->get_num_outputs(0));
  const std::vector<std::vector<uint64_t>> amount_indices =
      this->m_db->get_tx_amount_output_indices(this->m_db->get_tx_count() - 1, 1);
  ASSERT_EQ(1u, amount_indices.size());
  ASSERT_EQ(N_OUT, amount_indices.front().size());

  // ---- the assertion under test: stored commitment == outPk[i].mask ----
  for (size_t o = 0; o < N_OUT; ++o)
  {
    output_data_t od{};
    ASSERT_NO_THROW(od = this->m_db->get_output_key((uint64_t)0, amount_indices.front()[o], true));
    EXPECT_EQ(out_keys[o], od.pubkey);
    EXPECT_TRUE(od.commitment == rv.outPk[o].mask)
        << "output " << o << " of a hybrid tx stored with commitment " << epee::string_tools::pod_to_hex(od.commitment)
        << " instead of outPk.mask " << epee::string_tools::pod_to_hex(rv.outPk[o].mask)
        << (od.commitment == rct::zeroCommit(0) ? " (== zeroCommit(0) = G: the pseudo-rct rule fired on a real RingCT tx)" : "");
  }
}

// HIDERING Phase 5 — found by tests/pq_e2e case e: a hybrid tx built by the wallet is rejected by
// the daemon with "Failed to parse transaction from blob". construct_tx feeds genRctSimple the RING
// sources only, so CLSAGs and p.pseudoOuts hold one entry per RING input ("structure H2"), while the
// transaction serialiser passes inputs = vin.size() (ring + PQ) to serialize_rctsig_prunable, which
// requires CLSAGs.size() == inputs and pseudoOuts.size() == inputs. This pins the round trip of that
// exact shape (1 ring input + 1 txin_to_key_pq, 1 CLSAG, 1 pseudoOut). Not a DB test strictly, but it
// is the reason no hybrid output can reach the DB today.
TYPED_TEST(BlockchainDBTest, HybridTxShapedLikeConstructTxRoundTrips)
{
  const size_t RING_SIZE = 32;
  transaction hy{};
  hy.version = 2;
  txin_to_key ring_in{};
  for (size_t m = 0; m < RING_SIZE; ++m)
    ring_in.key_offsets.push_back(1);
  ring_in.k_image = rct::rct2ki(rct::pkGen());
  hy.vin.push_back(ring_in);
  txin_to_key_pq pq_in{};
  pq_in.amount = 15000000000000ull;
  pq_in.real_output_key = rct::rct2pk(rct::pkGen());
  hy.vin.push_back(pq_in);
  for (size_t o = 0; o < 2; ++o)
  {
    tx_out out{};
    txout_to_tagged_key tagged{};
    tagged.key = rct::rct2pk(rct::pkGen());
    out.target = tagged;
    hy.vout.push_back(out);
    hy.rct_signatures.outPk.push_back({rct::pk2rct(tagged.key), rct::commit(1000, rct::skGen())});
    hy.rct_signatures.ecdhInfo.push_back(rct::ecdhTuple{});
  }
  rct::rctSig &rv = hy.rct_signatures;
  rv.type = rct::RCTTypeBulletproofPlus;
  rv.txnFee = 10000000000ull;
  rct::BulletproofPlus bpp{};
  bpp.L.assign(7, rct::identity());
  bpp.R.assign(7, rct::identity());
  rv.p.bulletproofs_plus.push_back(bpp);
  // as genRctSimple returns it for a hybrid: ONE CLSAG and ONE pseudoOut, for the one ring input
  rct::clsag c{};
  c.s.assign(RING_SIZE, rct::identity());
  rv.p.CLSAGs.push_back(c);
  rv.p.pseudoOuts.push_back(rct::identity());

  blobdata blob;
  const bool serialised = tx_to_blob(hy, blob);
  EXPECT_TRUE(serialised) << "a hybrid tx with one CLSAG per RING input does not serialise (vin.size()="
                          << hy.vin.size() << ", CLSAGs=" << rv.p.CLSAGs.size() << ")";
  transaction back;
  EXPECT_TRUE(serialised && parse_and_validate_tx_from_blob(blob, back))
      << "the daemon cannot parse a hybrid tx of the shape construct_tx builds (blob " << blob.size() << " bytes)";
}

// HIDERING Phase 5 — the real round trip: construct_tx builds a HYBRID tx (one ring input + one
// transparent BQ input) exactly as the wallet does, the tx goes to a blob, the blob is parsed back,
// and the parsed tx is (1) byte- and hash-identical, (2) still balanced (verRctSemanticsSimple with
// the transparent term, which also verifies the BP+ range proof), and (3) stored by the DB with each
// output's real commitment outPk[i].mask. Before the serialisation fix step (1) failed — the blob was
// truncated — which is what the daemon reported as "Failed to parse transaction from blob".
TYPED_TEST(BlockchainDBTest, HybridTxFromConstructTxRoundTripsAndIsStoredWithItsCommitments)
{
  // sender: a BQ account (ML-KEM + ML-DSA keys from an independent PQ root, as --bq-wallet makes it)
  account_base sender_acc;
  sender_acc.generate();
  account_keys keys = sender_acc.get_keys();
  ASSERT_TRUE(generate_pq_keys(keys, generate_pq_root_secret()));
  std::unordered_map<crypto::public_key, subaddress_index> subaddresses;
  subaddresses[keys.m_account_address.m_spend_public_key] = {0, 0};

  // an output of the sender's primary address, as a tx with secret key r would have created it
  auto owned_output = [&](crypto::public_key &tx_pub, crypto::public_key &out_key)
  {
    const crypto::secret_key r = rct::rct2sk(rct::skGen());
    ASSERT_TRUE(crypto::secret_key_to_public_key(r, tx_pub));
    crypto::key_derivation d;
    ASSERT_TRUE(crypto::generate_key_derivation(keys.m_account_address.m_view_public_key, r, d));
    ASSERT_TRUE(crypto::derive_public_key(d, 0, keys.m_account_address.m_spend_public_key, out_key));
  };

  const uint64_t RING_AMOUNT = 20000000000000ull, PQ_AMOUNT = 20000000000000ull, FEE = 60000000000ull;
  std::vector<tx_source_entry> sources(2);

  // ring source: a classic RingCT output of ours at position 7 in a ring of 16
  {
    tx_source_entry &src = sources[0];
    crypto::public_key out_key;
    owned_output(src.real_out_tx_key, out_key);
    src.amount = RING_AMOUNT;
    src.rct = true;
    src.mask = rct::skGen();
    src.real_output = 7;
    src.real_output_in_tx_index = 0;
    for (uint64_t n = 0; n < 16; ++n)
    {
      rct::ctkey ck;
      ck.dest = n == 7 ? rct::pk2rct(out_key) : rct::pkGen();
      ck.mask = n == 7 ? rct::commit(RING_AMOUNT, src.mask) : rct::pkGen();
      src.outputs.push_back({100 + 3 * n, ck});
    }
  }
  // BQ source: our output P, tweaked to P' = P + t*G by the ML-KEM shared secret (spent transparently)
  {
    tx_source_entry &src = sources[1];
    crypto::public_key untweaked;
    owned_output(src.real_out_tx_key, untweaked);
    crypto::pqc::kyber_shared_secret ss;
    const rct::key ss_bytes = rct::skGen();
    memcpy(ss.ss, ss_bytes.bytes, sizeof(ss.ss));
    crypto::secret_key tweak;
    ASSERT_TRUE(derive_bq_output_tweak(ss, 0, tweak));
    const rct::key tweaked = rct::addKeys(rct::pk2rct(untweaked), rct::scalarmultBase(rct::sk2rct(tweak)));
    src.amount = PQ_AMOUNT;
    src.rct = true;
    src.mask = rct::skGen();
    src.real_output = 0;
    src.real_output_in_tx_index = 0;
    src.outputs.push_back({500, {tweaked, rct::commit(PQ_AMOUNT, src.mask)}});
    src.is_pq = true;
    src.pq_ss = ss;
    src.pq_subaddr = subaddress_index{0, 0};
  }

  // two classic destinations, no change: every atomic unit is accounted for
  account_base d1, d2;
  d1.generate(); d2.generate();
  std::vector<tx_destination_entry> dests;
  dests.push_back(tx_destination_entry(30000000000000ull, d1.get_keys().m_account_address, false));
  dests.push_back(tx_destination_entry(RING_AMOUNT + PQ_AMOUNT - FEE - 30000000000000ull, d2.get_keys().m_account_address, false));

  transaction tx;
  crypto::secret_key tx_key;
  std::vector<crypto::secret_key> additional_tx_keys;
  ASSERT_TRUE(construct_tx_and_get_tx_key(keys, subaddresses, sources, dests, boost::none, {}, tx, tx_key, additional_tx_keys,
                                          true, {rct::RangeProofPaddedBulletproof, 4}, true, HF_VERSION_PQ))
      << "construct_tx refused the hybrid";

  // it is a hybrid, with RingCT parts sized on the ring input
  ASSERT_EQ(2u, tx.vin.size());
  EXPECT_EQ(typeid(txin_to_key), tx.vin[0].type()) << "ring inputs come first";
  EXPECT_EQ(typeid(txin_to_key_pq), tx.vin[1].type());
  ASSERT_EQ(rct::RCTTypeBulletproofPlus, tx.rct_signatures.type);
  EXPECT_EQ(1u, tx.rct_signatures.p.CLSAGs.size());
  EXPECT_EQ(1u, tx.rct_signatures.p.pseudoOuts.size());
  ASSERT_EQ(tx.vout.size(), tx.rct_signatures.outPk.size());
  EXPECT_EQ(FEE, tx.rct_signatures.txnFee);

  // (1) blob round trip
  blobdata blob;
  ASSERT_TRUE(tx_to_blob(tx, blob)) << "construct_tx's hybrid does not serialise";
  transaction parsed;
  ASSERT_TRUE(parse_and_validate_tx_from_blob(blob, parsed)) << "the daemon cannot parse construct_tx's hybrid";
  EXPECT_EQ(blob, tx_to_blob(parsed)) << "re-serialising the parsed hybrid does not give the same bytes";
  EXPECT_EQ(get_transaction_hash(tx), get_transaction_hash(parsed));
  ASSERT_EQ(2u, parsed.vin.size());
  EXPECT_EQ(1u, parsed.rct_signatures.p.CLSAGs.size());
  EXPECT_EQ(1u, parsed.rct_signatures.p.pseudoOuts.size());
  for (size_t o = 0; o < tx.vout.size(); ++o)
    EXPECT_TRUE(parsed.rct_signatures.outPk[o].mask == tx.rct_signatures.outPk[o].mask);

  // (2) the parsed tx still balances with the transparent term the node recomputes from vin
  uint64_t pq_in = 0;
  ASSERT_TRUE(get_pq_transparent_input_sum(parsed, pq_in));
  EXPECT_EQ(PQ_AMOUNT, pq_in);
  parsed.rct_signatures.pq_transparent_in = pq_in;
  EXPECT_TRUE(rct::verRctSemanticsSimple(parsed.rct_signatures)) << "balance / range proof of the parsed hybrid";
  parsed.rct_signatures.pq_transparent_in = 0;
  EXPECT_FALSE(rct::verRctSemanticsSimple(parsed.rct_signatures)) << "control: without the transparent term it must not balance";
  EXPECT_FALSE(outputs_stored_as_pseudo_rct(parsed)) << "a hybrid is a real RingCT tx, not pseudo-rct";

  // (3) stored by the DB with each output's real commitment
  boost::filesystem::path tempPath = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
  std::string dirPath = tempPath.string();
  this->set_prefix(dirPath);
  ASSERT_NO_THROW(this->m_db->open(dirPath));
  this->get_filenames();
  this->init_hard_fork();
  db_wtxn_guard guard(this->m_db);
  ASSERT_NO_THROW(this->m_db->add_block(this->m_blocks[0], t_sizes[0], t_sizes[0], t_diffs[0], t_coins[0], this->m_txs[0]));
  std::pair<block, blobdata> blk = this->m_blocks[1];
  blk.first.tx_hashes.clear();
  blk.first.tx_hashes.push_back(get_transaction_hash(parsed));
  std::vector<std::pair<transaction, blobdata>> blk_txs;
  blk_txs.push_back(std::make_pair(parsed, blob));
  ASSERT_NO_THROW(this->m_db->add_block(blk, t_sizes[1], t_sizes[1], t_diffs[1], t_coins[1], blk_txs));
  const std::vector<std::vector<uint64_t>> idx = this->m_db->get_tx_amount_output_indices(this->m_db->get_tx_count() - 1, 1);
  ASSERT_EQ(1u, idx.size());
  ASSERT_EQ(tx.vout.size(), idx.front().size());
  for (size_t o = 0; o < tx.vout.size(); ++o)
  {
    output_data_t od{};
    ASSERT_NO_THROW(od = this->m_db->get_output_key((uint64_t)0, idx.front()[o], true));
    EXPECT_TRUE(od.commitment == tx.rct_signatures.outPk[o].mask)
        << "output " << o << " of construct_tx's hybrid stored with " << epee::string_tools::pod_to_hex(od.commitment);
  }
}

// HIDERING — the pseudo-rct predicate on each transaction shape (one rule for DB, count, reorg,
// wallet). Only a v2 coinbase and a FULLY transparent BQ spend (key_pq + RCTTypeNull) qualify.
TYPED_TEST(BlockchainDBTest, PseudoRctPredicateByShape)
{
  transaction coinbase{};
  coinbase.version = 2;
  coinbase.vin.push_back(txin_gen{});
  EXPECT_TRUE(outputs_stored_as_pseudo_rct(coinbase));
  coinbase.version = 1;
  EXPECT_FALSE(outputs_stored_as_pseudo_rct(coinbase)) << "a v1 coinbase keeps its per-amount buckets";

  transaction classic{};
  classic.version = 2;
  classic.vin.push_back(txin_to_key{});
  classic.rct_signatures.type = rct::RCTTypeBulletproofPlus;
  EXPECT_FALSE(outputs_stored_as_pseudo_rct(classic));

  transaction bq{};
  bq.version = 2;
  bq.vin.push_back(txin_to_key_pq{});
  bq.rct_signatures.type = rct::RCTTypeNull;
  EXPECT_TRUE(outputs_stored_as_pseudo_rct(bq));

  transaction hybrid = classic;
  hybrid.vin.push_back(txin_to_key_pq{});
  EXPECT_FALSE(outputs_stored_as_pseudo_rct(hybrid)) << "a hybrid is RingCT: outputs keep outPk";
}

// HIDERING Phase 5 — same real round trip with TWO ring inputs + one BQ input, so n_ring (2) differs
// from both 1 and vin.size() (3): CLSAGs and pseudoOuts must count exactly the ring inputs, the two
// ring inputs must precede the BQ one, and the blob / balance / DB commitments must hold as above.
TYPED_TEST(BlockchainDBTest, HybridTxTwoRingInputsFromConstructTxRoundTripsAndIsStoredWithItsCommitments)
{
  account_base sender_acc;
  sender_acc.generate();
  account_keys keys = sender_acc.get_keys();
  ASSERT_TRUE(generate_pq_keys(keys, generate_pq_root_secret()));
  std::unordered_map<crypto::public_key, subaddress_index> subaddresses;
  subaddresses[keys.m_account_address.m_spend_public_key] = {0, 0};

  auto owned_output = [&](crypto::public_key &tx_pub, crypto::public_key &out_key)
  {
    const crypto::secret_key r = rct::rct2sk(rct::skGen());
    ASSERT_TRUE(crypto::secret_key_to_public_key(r, tx_pub));
    crypto::key_derivation d;
    ASSERT_TRUE(crypto::generate_key_derivation(keys.m_account_address.m_view_public_key, r, d));
    ASSERT_TRUE(crypto::derive_public_key(d, 0, keys.m_account_address.m_spend_public_key, out_key));
  };

  const size_t N_RING = 2;
  const uint64_t RING_AMOUNTS[N_RING] = {12000000000000ull, 9000000000000ull};
  const uint64_t PQ_AMOUNT = 20000000000000ull, FEE = 70000000000ull;
  std::vector<tx_source_entry> sources(N_RING + 1);

  // two ring sources, each a RingCT output of ours in its own ring of 16 (real at 7, then at 11)
  for (size_t r = 0; r < N_RING; ++r)
  {
    tx_source_entry &src = sources[r];
    crypto::public_key out_key;
    owned_output(src.real_out_tx_key, out_key);
    src.amount = RING_AMOUNTS[r];
    src.rct = true;
    src.mask = rct::skGen();
    src.real_output = r == 0 ? 7 : 11;
    src.real_output_in_tx_index = 0;
    for (uint64_t n = 0; n < 16; ++n)
    {
      rct::ctkey ck;
      const bool real = n == src.real_output;
      ck.dest = real ? rct::pk2rct(out_key) : rct::pkGen();
      ck.mask = real ? rct::commit(src.amount, src.mask) : rct::pkGen();
      src.outputs.push_back({100 + 1000 * r + 3 * n, ck});
    }
  }
  // the BQ source, LAST in the sources list here; construct_tx must still order ring inputs first
  {
    tx_source_entry &src = sources[N_RING];
    crypto::public_key untweaked;
    owned_output(src.real_out_tx_key, untweaked);
    crypto::pqc::kyber_shared_secret ss;
    const rct::key ss_bytes = rct::skGen();
    memcpy(ss.ss, ss_bytes.bytes, sizeof(ss.ss));
    crypto::secret_key tweak;
    ASSERT_TRUE(derive_bq_output_tweak(ss, 0, tweak));
    const rct::key tweaked = rct::addKeys(rct::pk2rct(untweaked), rct::scalarmultBase(rct::sk2rct(tweak)));
    src.amount = PQ_AMOUNT;
    src.rct = true;
    src.mask = rct::skGen();
    src.real_output = 0;
    src.real_output_in_tx_index = 0;
    src.outputs.push_back({5000, {tweaked, rct::commit(PQ_AMOUNT, src.mask)}});
    src.is_pq = true;
    src.pq_ss = ss;
    src.pq_subaddr = subaddress_index{0, 0};
  }
  // put the BQ source FIRST in the list handed to construct_tx, to exercise its reordering
  std::rotate(sources.begin(), sources.begin() + N_RING, sources.end());

  const uint64_t total_in = RING_AMOUNTS[0] + RING_AMOUNTS[1] + PQ_AMOUNT;
  account_base d1, d2;
  d1.generate(); d2.generate();
  std::vector<tx_destination_entry> dests;
  dests.push_back(tx_destination_entry(25000000000000ull, d1.get_keys().m_account_address, false));
  dests.push_back(tx_destination_entry(total_in - FEE - 25000000000000ull, d2.get_keys().m_account_address, false));

  transaction tx;
  crypto::secret_key tx_key;
  std::vector<crypto::secret_key> additional_tx_keys;
  ASSERT_TRUE(construct_tx_and_get_tx_key(keys, subaddresses, sources, dests, boost::none, {}, tx, tx_key, additional_tx_keys,
                                          true, {rct::RangeProofPaddedBulletproof, 4}, true, HF_VERSION_PQ))
      << "construct_tx refused the 2-ring + 1-BQ hybrid";

  // n_ring = 2: two ring inputs first, then the BQ input; RingCT parts sized on the ring inputs
  ASSERT_EQ(N_RING + 1, tx.vin.size());
  EXPECT_EQ(typeid(txin_to_key), tx.vin[0].type()) << "ring inputs come first";
  EXPECT_EQ(typeid(txin_to_key), tx.vin[1].type()) << "ring inputs come first";
  EXPECT_EQ(typeid(txin_to_key_pq), tx.vin[2].type());
  ASSERT_EQ(rct::RCTTypeBulletproofPlus, tx.rct_signatures.type);
  EXPECT_EQ(N_RING, tx.rct_signatures.p.CLSAGs.size());
  EXPECT_EQ(N_RING, tx.rct_signatures.p.pseudoOuts.size());
  ASSERT_EQ(tx.vout.size(), tx.rct_signatures.outPk.size());
  EXPECT_EQ(FEE, tx.rct_signatures.txnFee);

  // (1) blob round trip
  blobdata blob;
  ASSERT_TRUE(tx_to_blob(tx, blob)) << "construct_tx's 2-ring hybrid does not serialise";
  transaction parsed;
  ASSERT_TRUE(parse_and_validate_tx_from_blob(blob, parsed)) << "the daemon cannot parse construct_tx's 2-ring hybrid";
  EXPECT_EQ(blob, tx_to_blob(parsed)) << "re-serialising the parsed hybrid does not give the same bytes";
  EXPECT_EQ(get_transaction_hash(tx), get_transaction_hash(parsed));
  ASSERT_EQ(N_RING + 1, parsed.vin.size());
  EXPECT_EQ(N_RING, parsed.rct_signatures.p.CLSAGs.size());
  EXPECT_EQ(N_RING, parsed.rct_signatures.p.pseudoOuts.size());
  for (size_t i = 0; i < N_RING; ++i)
  {
    EXPECT_TRUE(parsed.rct_signatures.p.pseudoOuts[i] == tx.rct_signatures.p.pseudoOuts[i]);
    EXPECT_TRUE(parsed.rct_signatures.p.CLSAGs[i].c1 == tx.rct_signatures.p.CLSAGs[i].c1);
    EXPECT_EQ(16u, parsed.rct_signatures.p.CLSAGs[i].s.size());
  }
  for (size_t o = 0; o < tx.vout.size(); ++o)
    EXPECT_TRUE(parsed.rct_signatures.outPk[o].mask == tx.rct_signatures.outPk[o].mask);

  // (2) balance with the transparent term (and the BP+ proof), negative control without it
  uint64_t pq_in = 0;
  ASSERT_TRUE(get_pq_transparent_input_sum(parsed, pq_in));
  EXPECT_EQ(PQ_AMOUNT, pq_in);
  parsed.rct_signatures.pq_transparent_in = pq_in;
  EXPECT_TRUE(rct::verRctSemanticsSimple(parsed.rct_signatures)) << "balance / range proof of the parsed 2-ring hybrid";
  parsed.rct_signatures.pq_transparent_in = 0;
  EXPECT_FALSE(rct::verRctSemanticsSimple(parsed.rct_signatures)) << "control: without the transparent term it must not balance";
  EXPECT_FALSE(outputs_stored_as_pseudo_rct(parsed)) << "a hybrid is a real RingCT tx, not pseudo-rct";

  // (3) stored by the DB with each output's real commitment
  boost::filesystem::path tempPath = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
  std::string dirPath = tempPath.string();
  this->set_prefix(dirPath);
  ASSERT_NO_THROW(this->m_db->open(dirPath));
  this->get_filenames();
  this->init_hard_fork();
  db_wtxn_guard guard(this->m_db);
  ASSERT_NO_THROW(this->m_db->add_block(this->m_blocks[0], t_sizes[0], t_sizes[0], t_diffs[0], t_coins[0], this->m_txs[0]));
  std::pair<block, blobdata> blk = this->m_blocks[1];
  blk.first.tx_hashes.clear();
  blk.first.tx_hashes.push_back(get_transaction_hash(parsed));
  std::vector<std::pair<transaction, blobdata>> blk_txs;
  blk_txs.push_back(std::make_pair(parsed, blob));
  ASSERT_NO_THROW(this->m_db->add_block(blk, t_sizes[1], t_sizes[1], t_diffs[1], t_coins[1], blk_txs));
  const std::vector<std::vector<uint64_t>> idx = this->m_db->get_tx_amount_output_indices(this->m_db->get_tx_count() - 1, 1);
  ASSERT_EQ(1u, idx.size());
  ASSERT_EQ(tx.vout.size(), idx.front().size());
  for (size_t o = 0; o < tx.vout.size(); ++o)
  {
    output_data_t od{};
    ASSERT_NO_THROW(od = this->m_db->get_output_key((uint64_t)0, idx.front()[o], true));
    EXPECT_TRUE(od.commitment == tx.rct_signatures.outPk[o].mask)
        << "output " << o << " of the 2-ring hybrid stored with " << epee::string_tools::pod_to_hex(od.commitment);
  }
}
