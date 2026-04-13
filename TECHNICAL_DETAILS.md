# HIDERING - DÉTAILS TECHNIQUES - 12 FÉVRIER 2026

## CONFIGURATION VALIDÉE

Supply: 33M HRG
Halving: 210,000 blocs
Reward: 157.14 HRG/bloc
Genesis: 17.59 HRG (legacy Monero, accepté)

## BLOCKCHAIN

Height: 1408 blocs
Data: ~/.hidering/lmdb/ (2.7 MB)
Genesis Hash: dbf4c85b7eaaa2efbcdfcc9b14cdfe0e590ce386a26ebd428fd5b344983eb9dc

## WALLET

Fichier: ~/.hidering/test-tx-padding
Balance: 221,095.98 HRG
Adresse: B3CFTxpyJTcW7QGENdBSPjcabLQPCgd7CgC57Mn745zURUPByHto4DFhsw8iNYznhzSypqzrta6NKh4ep6T1mJpoNBciLDY

## TESTS RING SIZE

Ring 32: 4785 bytes, 0.030 HRG - OPTIMAL
Ring 64: 11142 bytes, 0.066 HRG - OK
Ring 48: 12222 bytes, 0.072 HRG - Éviter

Rapport complet: RING_SIZE_TEST_RESULTS_11FEB2026.md

## PROCHAINES ÉTAPES

1. Implémenter Ring 32 par défaut
2. Développer Mixnet 3-hops
3. Stealth V2 avec view keys temporelles

Statut: Phase 2 en cours (85%)
