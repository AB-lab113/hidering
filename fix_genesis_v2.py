#!/usr/bin/env python3

file_path = '/home/shark/hidering/src/cryptonote_core/cryptonote_tx_utils.cpp'

# Restaurer le backup
import shutil
shutil.copy(file_path + '.backup_genesis', file_path)

with open(file_path, 'r') as f:
    lines = f.readlines()

new_code = '''    //genesis block
    bl = {};
    
    // Generate genesis miner transaction with premine
    account_public_address miner_address = AUTO_VAL_INIT(miner_address);
    // Use a burn address (all zeros) for genesis block
    miner_address.m_spend_public_key = crypto::null_pkey;
    miner_address.m_view_public_key = crypto::null_pkey;
    
    blobdata extra_nonce;
    bool r = construct_miner_tx(0, 0, 0, 0, 0, miner_address, bl.miner_tx, extra_nonce, 1, 1);
    CHECK_AND_ASSERT_MES(r, false, "Failed to construct genesis miner tx");
    
'''

for i, line in enumerate(lines):
    if i >= 652 and '//genesis block' in line:
        lines[i:i+10] = [new_code]
        break

with open(file_path, 'w') as f:
    f.writelines(lines)

print("✅ Fichier corrigé avec crypto::null_pkey")
