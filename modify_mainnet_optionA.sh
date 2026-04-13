#!/bin/bash
# OPTION A : Nouveau réseau Hidering avec préfixe "H"

echo "🔧 Modification du mainnet Hidering - Option A"
echo ""

# Backup du fichier original
cp src/cryptonote_config.h src/cryptonote_config.h.backup_beforeMainnetA
echo "✅ Backup créé : src/cryptonote_config.h.backup_beforeMainnetA"

# Modifications dans cryptonote_config.h
sed -i '229s/= 18/= 60/' src/cryptonote_config.h  # Préfixe principal (adresses "H")
sed -i '230s/= 19/= 61/' src/cryptonote_config.h  # Préfixe integrated
sed -i '231s/= 42/= 62/' src/cryptonote_config.h  # Préfixe subaddress
sed -i '232s/= 18080/= 28080/' src/cryptonote_config.h  # Port P2P
sed -i '233s/= 18081/= 28081/' src/cryptonote_config.h  # Port RPC
sed -i '234s/= 18082/= 28082/' src/cryptonote_config.h  # Port ZMQ

echo "✅ Modifications effectuées :"
echo "   - Préfixe adresses : 18 → 60 (adresses commencent par 'H')"
echo "   - Ports : 18080/18081/18082 → 28080/28081/28082"
echo ""
echo "🔍 Vérification..."
grep -n "CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX\|P2P_DEFAULT_PORT\|RPC_DEFAULT_PORT" src/cryptonote_config.h | head -6
echo ""
echo "✅ Script terminé ! Prêt pour compilation."
