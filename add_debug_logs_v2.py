#!/usr/bin/env python3

with open('src/wallet/wallet2.cpp', 'r') as f:
    lines = f.readlines()

output_lines = []
modified = False

for i, line in enumerate(lines):
    if 'if (get_output_public_key(o, output_public_key))' in line and not modified:
        indent = len(line) - len(line.lstrip())
        spaces = ' ' * indent
        
        # Ajouter les logs AVANT le if
        output_lines.append(f'{spaces}MINFO("geniod: Processing output k=" << k << " of " << n_vouts << " in txidx=" << txidx);\n')
        output_lines.append(f'{spaces}MINFO("geniod: Output target type = " << o.target.type().name());\n')
        output_lines.append(f'{spaces}MINFO("geniod: Output amount = " << o.amount);\n')
        
        # Ajouter la ligne originale
        output_lines.append(line)
        modified = True
    else:
        output_lines.append(line)

with open('src/wallet/wallet2.cpp', 'w') as f:
    f.writelines(output_lines)

print("✅ Logs debug ajoutés (version simple sans try-catch)")
