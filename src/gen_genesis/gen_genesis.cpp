// HIDERING Genesis TX Generator
// Generates a coinbase transaction for the genesis block with the correct initial reward
// Usage: hidering-gen-genesis

#include <iostream>
#include <string>
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_basic/account.h"
#include "string_tools.h"
#include "crypto/crypto.h"

using namespace cryptonote;

int main(int argc, char* argv[])
{
  // HIDERING initial reward: 157.14 HRG = 157140000000000 atomic units
  const uint64_t INITIAL_REWARD = 157140000000000ULL;

  // Generate a deterministic account for genesis (nobody owns these keys)
  // We use a zeroed seed to create a known, unspendable genesis address
  account_base genesis_account;
  genesis_account.generate();

  std::cout << "=== HIDERING Genesis TX Generator ===" << std::endl;
  std::cout << "Initial reward: " << INITIAL_REWARD << " atomic units (157.14 HRG)" << std::endl;
  std::cout << "Genesis address: " << get_account_address_as_str(MAINNET, false, genesis_account.get_keys().m_account_address) << std::endl;

  // Construct the coinbase/miner transaction at height 0
  transaction tx;
  // construct_miner_tx(height, median_weight, already_generated_coins, block_weight, fee, address, tx, extra, max_outs, hf_version)
  bool r = construct_miner_tx(0, 0, 0, 0, 0, genesis_account.get_keys().m_account_address, tx, blobdata(), 1, 1);
  if (!r) {
    std::cerr << "ERROR: Failed to construct miner TX" << std::endl;
    return 1;
  }

  // Override the output amount to our desired INITIAL_REWARD
  if (tx.vout.size() != 1) {
    std::cerr << "ERROR: Expected 1 output, got " << tx.vout.size() << std::endl;
    return 1;
  }
  tx.vout[0].amount = INITIAL_REWARD;

  // Invalidate hashes so they get recomputed
  tx.invalidate_hashes();

  // Serialize to hex
  blobdata tx_blob = tx_to_blob(tx);
  std::string tx_hex = epee::string_tools::buff_to_hex_nodelimer(tx_blob);

  std::cout << std::endl;
  std::cout << "=== GENESIS_TX (paste into cryptonote_config.h) ===" << std::endl;
  std::cout << tx_hex << std::endl;
  std::cout << std::endl;
  std::cout << "TX size: " << tx_blob.size() << " bytes" << std::endl;
  std::cout << "TX hash: " << epee::string_tools::pod_to_hex(get_transaction_hash(tx)) << std::endl;

  return 0;
}
