# HIDERING (HRG) — WHITE PAPER OFFICIEL
**Enhanced Privacy Cryptocurrency avec Bitcoin-Style Emission**  
**FAIR LAUNCH 100% MINING — ZÉRO PREMINE**  
**Version 1.3**  
**Mai 2026**

---

## TABLE DES MATIÈRES
1. Abstract
2. Vision & Problématique
3. Architecture Technique
4. Tokenomics (HRG)
5. Émission Bitcoin-Style
6. Résistance Quantique
7. Distribution Fair Launch
8. Roadmap Complète
9. Comparaison Technique
10. Spécifications Techniques
11. Conformité & Légal
12. Ressources
13. Conclusion

---

## 1. ABSTRACT

HIDERING (HRG) est un protocole de cryptomonnaie axé sur la confidentialité maximale, dérivé de Monero v0.18.1, combinant les innovations d'anonymat avancées avec un modèle économique Bitcoin-style.

**FAIR LAUNCH 100% MINING** : Zéro premine, 100% distribution via PoW équitable. Supply cap irrévocable **18 millions HRG**.

**INNOVATIONS CLÉS** :
- Ring size dynamique 32–64 (vs Monero 16)
- Transaction padding fixe 2500 bytes
- Mixnet natif 3-hops obligatoire
- Normalisation montants (chunks 0.1 HRG)
- Stealth addresses V2 avec view keys temporelles
- Résistance quantique programmée (2027)

---

## 2. VISION & PROBLÉMATIQUE

### 2.1 Le problème de la vie privée financière

Les blockchains publiques traditionnelles (Bitcoin, Ethereum) exposent intégralement les flux de transactions. Même Monero, référence en matière de confidentialité, présente des limites : ring size fixe à 16, métadonnées réseau exploitables, timing attacks possibles.

### 2.2 La vision HIDERING

HIDERING vise la confidentialité maximale par construction :

- **Anonymat on-chain** : ring signatures 32–64, stealth addresses V2, RingCT
- **Anonymat réseau** : mixnet 3-hops natif, chiffrement bout-en-bout
- **Confidentialité des montants** : padding fixe 2500 bytes, normalisation en chunks 0.1 HRG
- **Résistance future** : migration post-quantique planifiée via hard fork (2027)

### 2.3 Modèle économique sain

HIDERING adopte le modèle Bitcoin (supply fixe, halving, zéro premine) pour garantir la confiance et l'équité dès le premier bloc miné.

---

## 3. ARCHITECTURE TECHNIQUE

### 3.1 Base

HIDERING est un fork de **Monero v0.18.1** (CryptoNote), bénéficiant de :
- RingCT (confidentialité des montants)
- Bulletproofs+ (taille réduite des preuves)
- RandomX (PoW CPU-only, résistant ASIC)
- Dandelion++ (diffusion des transactions)

### 3.2 Modifications principales

| Composant | Monero original | HIDERING |
|-----------|----------------|----------|
| Ring size | 16 | 32–64 (dynamique) |
| TX padding | Variable | 2500 bytes fixe |
| Réseau | P2P direct | Mixnet 3-hops |
| Montants | Variable | Normalisés 0.1 HRG |
| Stealth | V1 | V2 + view keys temporelles |
| PoW | RandomX | RandomX (identique) |
| Hard Fork | HFv15 | HFv15 dès bloc 1 |

---

## 4. TOKENOMICS (HRG)

### 4.1 Spécifications Économiques

| Paramètre | Valeur |
|-----------|--------|
| Ticker | HRG |
| Supply maximum | **18,000,000 HRG (hard cap irrévocable)** |
| Premine | **0 HRG (100% mining fair launch)** |
| Block time | 120 secondes |
| Halving interval | 210,000 blocs (~2.66 ans) |
| Difficulty adjustment | Chaque bloc (RandomX) |
| Algorithme PoW | RandomX (CPU-only) |
| Récompense initiale | **42.86 HRG/bloc** |
| Adresses | Commencent par "B" (préfixe 60) |

### 4.2 Schedule d'Émission (Bitcoin-Style)

| Période | Hauteur Blocs | Reward/Bloc | Total Émis (période) |
|---------|---------------|-------------|----------------------|
| Genesis | 0 | 157.14 HRG (verrouillé NUMS, unspendable) | 157.14 HRG (non circulant) |
| Période 1 | 1 – 210,000 | 42.86 HRG | 9,000,000 HRG |
| Période 2 | 210,001 – 420,000 | 21.43 HRG | 4,500,000 HRG |
| Période 3 | 420,001 – 630,000 | 10.71 HRG | 2,250,000 HRG |
| … | … | … | Cap 18M atteint |

> **Genesis** : Le bloc 0 contient 157.14 HRG verrouillés sous une clé NUMS (Nothing-Up-My-Sleeve), cryptographiquement unspendable. Ce montant héritage est conservé pour la compatibilité de la chaîne déployée et ne fait pas partie du supply circulant. Voir `GENESIS_PROOF.md`.

### 4.3 Preuve Mathématique Supply

```
Émission totale = 42.86 × 210,000 × (1 + 1/2 + 1/4 + ...) 
               = 9,000,000 × 2 
               = 18,000,000 HRG exactement
```

La série géométrique converge vers le cap de **18M HRG**. Zéro premine, zéro inflation, cap irrévocable.

### 4.4 Note technique — Choix du cap 18M

Le cap original de 33M HRG provoquait un **overflow uint64** (33 × 10¹⁹ > UINT64\_MAX = 1.844 × 10¹⁹) avec 12 décimales atomiques héritées de Monero. Comportement observé sur les binaires v1.0.0 : wrap silencieux sous Linux gcc (cap effectif ~14.55M), erreur fatale de compilation sous macOS clang. Le cap de **18M HRG** (1.8 × 10¹⁹, marge uint64 ~2.5%) a été adopté le 11 mai 2026 et implémenté dans la release v1.0.1.

---

## 5. ÉMISSION BITCOIN-STYLE

### 5.1 Philosophie

| Aspect | Bitcoin | **HIDERING** |
|--------|---------|--------------|
| Supply cap | 21M BTC | **18M HRG** |
| Premine | 0% | **0% (fair launch)** |
| Mécanisme | Halving q/4 ans | Halving q/2.66 ans |
| PoW | SHA-256 (ASIC) | RandomX (CPU-only) |

### 5.2 Code Halving C++

```cpp
bool get_block_reward(uint64_t already_generated_coins, uint64_t &reward) {
    const uint64_t MONEY_SUPPLY = 18000000000000000000ULL; // 18M HRG (uint64-safe)
    const uint64_t HALVING_INTERVAL = 210000;
    const uint64_t INITIAL_REWARD = 42857142857143ULL;     // 42.857142... HRG

    if (already_generated_coins >= MONEY_SUPPLY) {
        reward = 0;
        return true;
    }

    uint64_t halvings = already_generated_coins / (INITIAL_REWARD * HALVING_INTERVAL);
    uint64_t base_reward = INITIAL_REWARD >> halvings; // / 2^halvings

    reward = std::min(base_reward, MONEY_SUPPLY - already_generated_coins);
    return true;
}
```

---

## 6. RÉSISTANCE QUANTIQUE

La cryptographie post-quantique sera introduite via un **hard fork planifié en 2027** :

- **Signatures** : Dilithium3 (NIST PQC standard)
- **Échange de clés** : Kyber768 (NIST PQC standard)
- **Impact** : les transactions futures (post-fork) utilisent les nouveaux schémas. La blockchain historique n'est pas affectée.
- **Compatibilité** : mise à jour obligatoire des binaires au moment du fork.

---

## 7. DISTRIBUTION FAIR LAUNCH

**100% MINING PUBLIC** :
- Aucun premine, aucune vente privée, aucun seed round.
- Bloc 0 : genesis avec 157.14 HRG verrouillés sous clé NUMS (unspendable, héritage chaîne).
- Bloc 1 : **premier miner public** reçoit 42.86 HRG.
- Équipe mine publiquement comme tous les participants.

**Avantages** :
- ✓ Distribution pure PoW (RandomX CPU-friendly)
- ✓ Zéro avantage équipe (fair depuis jour 1)
- ✓ Transparence absolue (genesis vérifiable on-chain)
- ✓ Confiance maximale (pas de « dev wallet »)

---

## 8. ROADMAP COMPLÈTE

**PHASE 0 : SETUP ✅**
- Fork Monero v0.18.1
- Compilation Ubuntu AMD
- Dependencies OK

**PHASE 1 : CORE TOKENOMICS ✅**
- MONEY_SUPPLY = 18M HRG (uint64-safe)
- HALVING_INTERVAL = 210,000 blocs
- INITIAL_REWARD = 42.86 HRG/bloc
- Genesis fair (NUMS, unspendable)

**PHASE 2 : PRIVACY ENHANCEMENTS ✅**
- P1 ✅ Ring dynamique 32–64
- P2 ✅ TX padding 2500 bytes
- P3 ✅ Mixnet 3-hops
- P4 ✅ Normalisation montants 0.1 HRG
- P5 ✅ Stealth V2

**PHASE 3 : INFRASTRUCTURE ✅**
- Rebranding complet HIDERING
- Seed nodes Flux (hideringseed1, hideringseed2)
- DNS seed1/seed2.hidering.org
- Block explorer (hrgexplorer.app.runonflux.io)
- Site web hidering.org

**PHASE 4 : MAINNET LAUNCH (En cours)**
- ✅ Mainnet live (189,000+ HRG minés)
- ✅ Release v1.0.0 Linux
- ✅ Release v1.0.1 Linux (fix MONEY_SUPPLY 18M)
- ✅ Release v1.0.2 Linux (fix bad_alloc boot daemon)
- ⏳ Binaires macOS/Windows (juin 2026)
- ⏳ Whitepaper v1.3 harmonisé
- ⏳ Pool mining RandomX/HRG
- ⏳ Launch public (Twitter, Reddit, BitcoinTalk)

**PHASE 5 : POST-QUANTIQUE (2027)**
- Dilithium3 + Kyber768 signatures
- Hard fork PQC

---

## 9. COMPARAISON TECHNIQUE

| Fonctionnalité | Monero | **HIDERING** |
|---------------|--------|--------------|
| Ring size | 16 (fixe) | 32–64 (dynamique) |
| TX padding | Non | 2500 bytes fixe |
| Mixnet | Non | 3-hops natif |
| Montants | Variables | Normalisés 0.1 HRG |
| Stealth | V1 | V2 + view keys temporelles |
| Supply | Tail emission | 18M hard cap |
| Halving | Non | Tous les 210,000 blocs |
| PoW | RandomX | RandomX (identique) |
| Post-quantique | Non planifié | 2027 (Dilithium3+Kyber768) |

---

## 10. SPÉCIFICATIONS TECHNIQUES

| Paramètre | Valeur |
|-----------|--------|
| Ticker | HRG |
| Fork base | Monero v0.18.1 |
| Network ID | `HRG\x01HIDERINGMAIN` |
| Magic bytes | `0x48524701` |
| Port P2P | 19740 |
| Port RPC | 19741 |
| Préfixe adresses | 60 → "B" |
| Décimales | 12 (10¹²) |
| Block time | 120 secondes |
| PoW | RandomX CPU-only |
| HFv15 | Dès bloc 1 |
| Ring size | 32–64 |
| Supply | 18,000,000 HRG |
| Reward initial | 42.857142857143 HRG/bloc |
| Halving | 210,000 blocs (~2.66 ans) |
| Premine | 0% |
| Repo | github.com/AB-lab113/hidering |

---

## 11. CONFORMITÉ & LÉGAL

HIDERING est un logiciel open-source distribué sous licence identique à Monero (BSD 3-Clause). L'équipe ne donne aucun conseil financier. HRG est un actif expérimental. Les utilisateurs sont responsables du respect des lois locales relatives aux cryptomonnaies.

---

## 12. RESSOURCES

- **Site web** : https://hidering.org
- **GitHub** : https://github.com/AB-lab113/hidering
- **Releases** : https://github.com/AB-lab113/hidering/releases
- **Block Explorer** : https://hrgexplorer.app.runonflux.io
- **DNS Seed 1** : seed1.hidering.org
- **DNS Seed 2** : seed2.hidering.org

---

## 13. CONCLUSION

HIDERING (HRG) représente une évolution significative de la confidentialité financière on-chain. En combinant les meilleures innovations de Monero avec un modèle économique Bitcoin-style (supply fixe 18M, halving, zéro premine) et des améliorations privacy substantielles (ring size 32–64, padding fixe, mixnet 3-hops), HIDERING offre une confidentialité maximale par construction.

Le fair launch garantit une distribution équitable depuis le bloc 1. La roadmap post-quantique (2027) assure la pérennité du protocole face aux menaces futures.

**HIDERING — Privacy by design. Fair by launch.** 🚀

---

*Whitepaper v1.3 — Mai 2026*  
*SHA256 release v1.0.2 : `da92781f5e0d085a08a656b48ea49be3d54201b3db32b736204266ea89fd2b8b`*
