#!/bin/bash
# Supprimer les seed nodes Monero du mainnet Hidering

echo "🧹 Suppression des seed nodes Monero..."

# Backup
cp src/p2p/net_node.inl src/p2p/net_node.inl.backup_beforeSeedRemoval
echo "✅ Backup : src/p2p/net_node.inl.backup_beforeSeedRemoval"

# Trouver et remplacer la section mainnet seed nodes
# On garde la structure mais on vide les insertions
sed -i '/else$/,/^    }$/ {
  /full_addrs.insert.*18080/d
}' src/p2p/net_node.inl

echo "✅ Seed nodes Monero supprimés"
echo ""
echo "🔍 Vérification - cette commande ne doit rien afficher :"
grep "18080" src/p2p/net_node.inl || echo "   ✅ Aucun port 18080 trouvé (bon!)"
echo ""
echo "✅ Hidering est maintenant un réseau indépendant !"
