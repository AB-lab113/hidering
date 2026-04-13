#!/usr/bin/env python3
import sys

# Lire le fichier
with open('src/wallet/wallet2.cpp', 'r') as f:
    lines = f.readlines()

# Trouver la ligne avec get_output_public_key
modified = False
output_lines = []

for i, line in enumerate(lines):
    # Si on trouve la ligne avec get_output_public_key
    if 'if (get_output_public_key(o, output_public_key))' in line and not modified:
        # Calculer l'indentation
        indent = len(line) - len(line.lstrip())
        spaces = ' ' * indent
        
        # Ajouter les logs AVANT
        output_lines.append(f'{spaces}MINFO("geniod: Processing output k=" << k << " of " << n_vouts << " in txidx=" << txidx);\n')
        output_lines.append(f'{spaces}MINFO("geniod: Output target type = " << o.target.type().name());\n')
        output_lines.append(f'{spaces}MINFO("geniod: Output amount = " << o.amount);\n')
        output_lines.append(f'{spaces}try {{\n')
        
        # Ajouter la ligne originale avec indentation supplémentaire
        output_lines.append(f'{spaces}  {line.lstrip()}')
        modified = True
        
    # Si on trouve le bloc qui suit get_output_public_key
    elif modified and i > 0 and '{' in lines[i-1] and 'std::vector<crypto::key_derivation> additional_derivations' in line:
        # On est dans le bloc if, ajouter le code normalement mais wrapper avec try-catch
        output_lines.append(line)
        
    # Si on trouve la fin du bloc if (ligne avec } qui ferme)
    elif modified and line.strip() == '}' and 'additional_derivations.clear()' in lines[i-1]:
        # Ajouter la fermeture du try
        indent = len(line) - len(line.lstrip())
        spaces = ' ' * indent
        output_lines.append(f'{spaces}}} catch (const std::exception &e) {{\n')
        output_lines.append(f'{spaces}  MERROR("geniod: EXCEPTION at k=" << k << "/" << n_vouts << " txidx=" << txidx << " error: " << e.what());\n')
        output_lines.append(f'{spaces}  throw;\n')
        output_lines.append(f'{spaces}}}\n')
        output_lines.append(line)  # Ajouter le } original
        modified = False  # Reset pour éviter de modifier plusieurs fois
        
    else:
        output_lines.append(line)

# Sauvegarder
with open('src/wallet/wallet2.cpp', 'w') as f:
    f.writelines(output_lines)

print("✅ Logs debug ajoutés dans wallet2.cpp")
print("   - Ligne ~3316 : Logs avant get_output_public_key()")
print("   - Try-catch autour du traitement des outputs")
