#!/usr/bin/env python3

file_path = '/home/shark/hidering/src/cryptonote_core/cryptonote_tx_utils.cpp'

with open(file_path, 'r') as f:
    content = f.read()

# Retirer le code du premine
old_code = '''    // Generate genesis miner transaction with premine
    account_public_address miner_address = AUTO_VAL_INIT(miner_address);
    // Premine address derived from genesis seed
    if (!epee::string_tools::hex_to_pod("7d996b0f2db6dbb5f2a086211f2399a4a7479b2c911af307fdc3f7f61a88cb0e", miner_address.m_spend_public_key))
      return false;
    if (!epee::string_tools::hex_to_pod("42ba20adb337e5eca797565be11c9adb0a8bef8c830bccc2df712535d3b8f608", miner_address.m_view_public_key))
      return false;

    blobdata extra_nonce;
    bool r = construct_miner_tx(0, 0, 0, 0, 0, miner_address, bl.miner_tx, extra_nonce, 1, 1);'''

new_code = '''    // Generate simple genesis block with no premine
    account_public_address miner_address = AUTO_VAL_INIT(miner_address);
    
    blobdata extra_nonce;
    bool r = construct_miner_tx(0, 0, 0, 0, 0, miner_address, bl.miner_tx, extra_nonce, 1, 0);'''

content = content.replace(old_code, new_code)

with open(file_path, 'w') as f:
    f.write(content)

print("✅ Genesis propre sans premine")
