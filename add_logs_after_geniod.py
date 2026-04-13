#!/usr/bin/env python3

with open('src/wallet/wallet2.cpp', 'r') as f:
    lines = f.readlines()

output_lines = []

for i, line in enumerate(lines):
    output_lines.append(line)
    
    # Ajouter log après "THROW_WALLET_EXCEPTION_IF(!waiter.wait()"
    if 'THROW_WALLET_EXCEPTION_IF(!waiter.wait(), error::wallet_internal_error, "Exception in thread pool")' in line:
        indent = len(line) - len(line.lstrip())
        spaces = ' ' * indent
        output_lines.append(f'{spaces}MINFO("process_parsed_blocks: waiter.wait() completed successfully");\n')
    
    # Ajouter log après "hwdev.set_mode(hw::device::NONE)"
    if 'hwdev.set_mode(hw::device::NONE)' in line:
        indent = len(line) - len(line.lstrip())
        spaces = ' ' * indent
        output_lines.append(f'{spaces}MINFO("process_parsed_blocks: Starting block processing loop");\n')

with open('src/wallet/wallet2.cpp', 'w') as f:
    f.writelines(output_lines)

print("✅ Logs ajoutés après geniod")
