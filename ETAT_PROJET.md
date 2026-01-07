# HIDERING - ÉTAT DU PROJET
Date: 06/01/2026

## ✅ ACCOMPLI

### 1. Fork Monero créé
- Nom: Hidering (HRG)
- Répertoire: `/home/shark/hidering`
- Binaires: `/home/shark/hidering/build/release/bin/`

### 2. Configuration
- Ticker: HRG
- Préfixe adresse: 0x5a (commence par "4")
- Ports: 18080 (P2P), 18081 (RPC)
- Premine: 3,300,000 HRG (bloc 1)
- Supply max: 18.4M HRG
- Temps bloc: 120s
- Difficulté: Ajustée (mining facile en mode --fixed-difficulty 1)

### 3. Wallet Premine
**SEED (25 mots) - ULTRA CONFIDENTIEL:**
uncle haggled swept owner pivot bacon knee technical snake bluntly igloo tirade sifting solved biweekly bested textbook dewdrop goodbye puddle omnibus each tipsy together swept

**Adresse:**
44tEEx19VVCXRfzdRk7cM5R2e6G7hSAS1Nk91DQwbHxU4kXdJcvu4EFEJZVfpwQawx3h1jNatB9yBZtZuch5pFhs6mcJy9G

**Fichiers wallet:**
- `/home/shark/hidering/premine`
- `/home/shark/hidering/premine.keys`

**Balance:** 3,300,000 HRG (vérifié bloc 1)

### 4. Blockchain
- Hauteur actuelle: 784 blocs
- Data dir: `~/hidering_data`
- Mode: Offline (--offline --fixed-difficulty 1)

### 5. Modifications code source

**Fichier: `src/cryptonote_config.h`**
- Ticker changé en HRG
- Ports modifiés
- Préfixe adresse modifié

**Fichier: `src/cryptonote_basic/cryptonote_basic_impl.cpp`**
- Premine ajouté dans `get_block_reward()`:
\`\`\`cpp
if (already_generated_coins == 0) {
  reward = 3300000000000000000ULL;  // 3.3M HRG premine
  return true;
}
\`\`\`

## ⚠️ PROBLÈMES CONNUS

### BUG CRITIQUE: Wallet crash au refresh
- **Symptôme:** Segmentation fault lors de refresh
- **Fichiers concernés:** wallet2.cpp, wallet_rpc_server.cpp
- **Impact:** Impossible d'utiliser wallet-cli et wallet-rpc normalement
- **Workaround:** Calculer balance manuellement via RPC daemon
- **Logs crash:** dmesg montre segfault at 0x8

### Solution temporaire
\`\`\`bash
# Vérifier balance via daemon RPC
curl -s http://127.0.0.1:18081/json_rpc -d '{"jsonrpc":"2.0","id":"0","method":"get_block","params":{"height":1}}' -H 'Content-Type: application/json' | jq '.result.block_header.reward'
\`\`\`

## 📁 COMMANDES UTILES

### Démarrer daemon
\`\`\`bash
cd ~/hidering/build/release/bin
./hideringd --data-dir ~/hidering_data --offline --fixed-difficulty 1 --detach
\`\`\`

### Miner des blocs
\`\`\`bash
ADDR="44tEEx19VVCXRfzdRk7cM5R2e6G7hSAS1Nk91DQwbHxU4kXdJcvu4EFEJZVfpwQawx3h1jNatB9yBZtZuch5pFhs6mcJy9G"
./hideringd start_mining \$ADDR 4
# Attendre...
./hideringd stop_mining
\`\`\`

### Vérifier état
\`\`\`bash
./hideringd status
\`\`\`

### Arrêter daemon
\`\`\`bash
killall hideringd
\`\`\`

## 🎯 PROCHAINES ÉTAPES

### Priorité 1: Corriger bug wallet
- Debugger wallet2.cpp (ligne ~6493)
- Isoler la cause du segfault au refresh
- Tester avec gdb

### Priorité 2: Configuration réseau
- Configurer seed nodes
- Ouvrir ports publics
- Configurer DNS seeds
- Créer nodes bootstrap

### Priorité 3: Production
- Script démarrage automatique
- Sauvegarder genesis block
- Documentation utilisateur
- Site web / whitepaper

## 🔧 ENVIRONNEMENT
- **OS:** Ubuntu WSL2
- **CPU:** AMD (8 cores utilisables)
- **Compilateur:** GCC (build/release)
- **Dépendances:** Installées et fonctionnelles

## 📝 NOTES IMPORTANTES
- **TOUJOURS** en mode offline pour éviter sync avec réseau Monero
- `--fixed-difficulty 1` nécessaire pour mining rapide
- SEED sauvegardée = seul moyen de récupérer les 3.3M HRG
- Wallet files dans `/home/shark/hidering/` (PAS dans build/)
- Blockchain dans `~/hidering_data/` (séparé du code source)
