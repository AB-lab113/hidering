#!/usr/bin/env python3

file_path = '/home/shark/hidering/src/cryptonote_core/cryptonote_tx_utils.cpp'

with open(file_path, 'r') as f:
    content = f.read()

# Trouver et remplacer la section complète
old_section = '''    blobdata extra_nonce;
    bool r = construct_miner_tx(0, 0, 0, 0, 0, miner_address, bl.miner_tx, extra_nonce, 1, 1);
    CHECK_AND_ASSERT_MES(r, false, "Failed to construct genesis miner tx");

    bl.timestamp = 0;'''

new_section = '''    blobdata extra_nonce;
    bool r = construct_miner_tx(0, 0, 0, 0, 0, miner_address, bl.miner_tx, extra_nonce, 1, 1);
    CHECK_AND_ASSERT_MES(r, false, "Failed to construct genesis miner tx");
    
    bl.major_version = CURRENT_BLOCK_MAJOR_VERSION;
    bl.minor_version = CURRENT_BLOCK_MINOR_VERSION;
    bl.timestamp = 0;'''

content = content.replace(old_section, new_section)

with open(file_path, 'w') as f:
    f.write(content)

print("✅ Versions du bloc ajoutées")
