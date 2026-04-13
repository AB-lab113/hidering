# Hidering - Ring Size 32 Implementation

## Vue d'ensemble
- Projet : Hidering (fork Monero)
- Modification : Ring Size 15 -> 32
- Version : 0.18.1.0-ring32
- Date : 19 Janvier 2026

## Fichiers modifiés
1. src/cryptonote_config.h (HF_VERSION_MIN_MIXIN_31)
2. src/cryptonote_core/blockchain.cpp (5 lignes)
3. src/wallet/wallet2.cpp (2 lignes)

## Binaires installés
- /usr/local/bin/hideringd
- /usr/local/bin/hidering-wallet-cli
- /usr/local/bin/hidering-wallet-rpc

## Utilisation rapide
Testnet : ~/.hidering/start-testnet.sh --offline
Mainnet : ~/.hidering/start-mainnet.sh

## Documentation
- Changelog : ~/hidering/CHANGELOG-RING32.md
- Réseau : ~/.hidering/NETWORK-INFO.txt
- Logs : ~/.hidering/{mainnet,testnet}/hidering.log

Compilé le 19 Janvier 2026 par shark@AMD
