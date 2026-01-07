#!/usr/bin/env python3
import sys

file_path = '/home/shark/hidering/src/cryptonote_core/cryptonote_tx_utils.cpp'

with open(file_path, 'r') as f:
    lines = f.readlines()

# Trouver et remplacer les lignes 654-662
new_code = '''    //genesis block
    bl = {};
    
    // Generate genesis miner transaction with premine
    account_public_address miner_address = AUTO_VAL_INIT(miner_address);
    // Use a burn address (all zeros) for genesis block
    miner_address.m_spend_public_key = null_pkey;
    miner_address.m_view_public_key = null_pkey;
    
    blobdata extra_nonce;
    bool r = construct_miner_tx(0, 0, 0, 0, 0, miner_address, bl.miner_tx, extra_nonce, 1, 1);
    CHECK_AND_ASSERT_MES(r, false, "Failed to construct genesis miner tx");
    
'''

# Chercher la ligne avec "//genesis block"
for i, line in enumerate(lines):
    if i >= 652 and '//genesis block' in line:
        # Remplacer les 10 lignes suivantes
        lines[i:i+10] = [new_code]
        break

with open(file_path, 'w') as f:
    f.writelines(lines)

print("✅ Fichier modifié avec succès")
