# HIDERING (HRG) — WHITE PAPER OFFICIEL
**Enhanced Privacy Cryptocurrency avec Bitcoin-Style Emission**  
**FAIR LAUNCH · 100% PoW MINING · RandomX CPU**  
**Version 1.3 (révisée 1 Juin 2026 — release v2.0.2)**  
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

**FAIR LAUNCH** : émission 100% via PoW RandomX, supply cap irrévocable **18 millions HRG**. v2.0.0 introduit une courte phase de pré-minage en réseau privé (réponse à l'attaque 51% du 16 mai 2026 sur la chaîne v1.x à h≈3577) — voir §7 pour la justification complète.

**INNOVATIONS CLÉS** :
- Ring size dynamique 32–64 (vs Monero 16)
- Transaction padding fixe 2500 bytes
- Routage stem multi-hops (biais best-effort vers ≥3 hops, par-dessus Dandelion++)
- Normalisation montants (chunks 0.1 HRG)
- Stealth addresses V2
- Résistance quantique programmée (2027)

---

## 2. VISION & PROBLÉMATIQUE

### 2.1 Le problème de la vie privée financière

Les blockchains publiques traditionnelles (Bitcoin, Ethereum) exposent intégralement les flux de transactions. Même Monero, référence en matière de confidentialité, présente des limites : ring size fixe à 16, métadonnées réseau exploitables, timing attacks possibles.

### 2.2 La vision HIDERING

HIDERING vise la confidentialité maximale par construction :

- **Anonymat on-chain** : ring signatures 32–64, stealth addresses V2, RingCT
- **Anonymat réseau** : routage stem multi-hops (best-effort par-dessus Dandelion++), chiffrement bout-en-bout
- **Confidentialité des montants** : padding fixe 2500 bytes, normalisation en chunks 0.1 HRG
- **Résistance future** : migration post-quantique planifiée via hard fork (2027)

### 2.3 Modèle économique sain

HIDERING adopte le modèle Bitcoin (supply fixe, halving, émission PoW pure) pour garantir la prédictibilité monétaire et l'équité à long terme.

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
| Réseau | P2P direct | Routage stem multi-hops |
| Montants | Variable | Normalisés 0.1 HRG |
| Stealth | V1 | V2 |
| PoW | RandomX | RandomX (identique) |
| Hard Fork | HFv15 | HFv15 dès bloc 1 |

---

## 4. TOKENOMICS (HRG)

### 4.1 Spécifications Économiques

| Paramètre | Valeur |
|-----------|--------|
| Ticker | HRG |
| Supply maximum | **18,000,000 HRG (hard cap irrévocable)** |
| Émission | **100% PoW** (pas de seed round, pas de vente privée) — phase pré-publique v2.0.0 voir §7 |
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

La série géométrique converge vers le cap de **18M HRG**. Émission 100% PoW, zéro inflation, cap irrévocable.

### 4.4 Note technique — Choix du cap 18M

Le cap original de 33M HRG provoquait un **overflow uint64** (33 × 10¹⁹ > UINT64\_MAX = 1.844 × 10¹⁹) avec 12 décimales atomiques héritées de Monero. Comportement observé sur les binaires v1.0.0 : wrap silencieux sous Linux gcc (cap effectif ~14.55M), erreur fatale de compilation sous macOS clang. Le cap de **18M HRG** (1.8 × 10¹⁹, marge uint64 ~2.5%) a été adopté le 11 mai 2026 et implémenté dans la release v1.0.1.

---

## 5. ÉMISSION BITCOIN-STYLE

### 5.1 Philosophie

| Aspect | Bitcoin | **HIDERING** |
|--------|---------|--------------|
| Supply cap | 21M BTC | **18M HRG** |
| Émission | 100% PoW (fair launch) | **100% PoW (fair launch)** |
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

**Principe** : émission 100% via PoW RandomX, aucune vente privée, aucun seed round, pas de token allocation à l'équipe. Bloc 0 est un genesis avec 157.14 HRG verrouillés sous clé NUMS (cryptographiquement unspendable, héritage chaîne — voir `GENESIS_PROOF.md`). Bloc 1 onwards : récompense distribuée par compétition PoW pure.

### 7.1 Phase pré-publique v2.0.0 (post-attaque 16 mai 2026)

Peu après le lancement v1.x, le réseau HIDERING a subi une attaque 51% (16 mai 2026, chaîne v1.x à h≈3577) : un mineur externe disposant d'une puissance supérieure à l'ensemble du réseau a dominé la chaîne et causé une réorganisation majeure, rendant orphelins tous les blocs minés. La réponse v2.0.0 est duale :

- **Hard fork réseau** : le Network ID passe de `HRG\x01HIDERINGMAIN` à `HRG\x02HIDERINGMAIN` (magic `0x48524701` → `0x48524702`), isolant immédiatement v2.0.0 du mineur malveillant et permettant la consolidation du hashrate sur réseau privé avant ré-ouverture publique.
- **Phase de pré-minage** : avant l'ouverture publique, les nœuds fondateurs minent en réseau privé pour trois raisons :
  1. **Durcir le réseau dès le premier jour public** — accumuler suffisamment de hashrate pour rendre une nouvelle attaque 51% économiquement prohibitive dès le launch.
  2. **Financer le développement** — les tests du wallet GUI et l'implémentation des signatures post-quantiques (Dilithium3 + Kyber768, prévues 2027) requièrent des transactions on-chain réelles.
  3. **Constituer une réserve communautaire** — qui sera soit redistribuée aux mineurs et DEX participants au lancement public, soit partiellement brûlée pour réduire la supply en circulation.

Le pré-minage v2.0.0 n'est **pas** une capture de valeur par l'équipe : il s'agit d'un bouclier défensif et d'un levier de financement développement. Une fois le réseau ouvert publiquement, tous les mineurs concourent sous les mêmes règles RandomX, sans avantage insider sur des coins inaccessibles au public.

### 7.2 Engagements

- ✓ Algorithme PoW RandomX (CPU-only, ASIC-resistant)
- ✓ Aucune vente privée, aucun seed round, aucun token allocation équipe
- ✓ Genesis vérifiable on-chain (`GENESIS_PROOF.md`)
- ✓ Transparence sur la phase pré-publique v2.0.0 : durée, hashrate, destination des coins minés (communiqués au launch public)

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
- ✅ Releases v1.0.0 → v1.0.2 Linux (rebrand, fix MONEY_SUPPLY 18M, fix bad_alloc boot daemon)
- ✅ **Hard fork v2.0.0 (16 mai 2026)** — NETWORK_ID bumpé `HRG\x01HIDERINGMAIN` → `HRG\x02HIDERINGMAIN`, magic `0x48524701` → `0x48524702`, version 1.0.2 → 2.0.0. Chaîne v1.x (pré-launch, ~h≈3577) retirée au profit du nouveau réseau v2.0.0.
- ✅ **Release v2.0.2 (1 juin 2026)** — dernière publique, bundles complets (daemon + wallet) Linux x64, macOS ARM64 et Windows x64, publiés automatiquement par CI
- ✅ Binaires multi-plateformes v2.0.2 (Linux / macOS ARM64 / Windows) — chaque asset accompagné de son sidecar SHA256
- ✅ Pool mining RandomX/HRG en production (`pool.hidering.org:3333`)
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
| Mixnet | Non | Routage stem multi-hops |
| Montants | Variables | Normalisés 0.1 HRG |
| Stealth | V1 | V2 |
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
| Network ID | `HRG\x02HIDERINGMAIN` (v2.0.0) |
| Magic bytes | `0x48524702` (v2.0.0) |
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
| Émission | 100% PoW (voir §7) |
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

HIDERING (HRG) représente une évolution significative de la confidentialité financière on-chain. En combinant les meilleures innovations de Monero avec un modèle économique Bitcoin-style (supply fixe 18M, halving, émission 100% PoW) et des améliorations privacy substantielles (ring size 32–64, padding fixe, routage stem multi-hops), HIDERING offre une confidentialité maximale par construction.

Le fair launch garantit une distribution équitable par compétition PoW pure. La phase pré-publique v2.0.0 (réponse à l'attaque 51% du 16 mai 2026, §7.1) durcit le réseau et finance le développement avant la ré-ouverture publique. La roadmap post-quantique (2027) assure la pérennité du protocole face aux menaces futures.

**HIDERING — Privacy by design. Fair by launch.** 🚀

---

*Whitepaper v1.3 — Mai 2026 (révisé 1 juin 2026 pour release v2.0.2)*  
*SHA256 release v2.0.2 (dernière publique) :*  
*  Linux x64 : `844b0cac1cb3192c9d616dffa50da538399447c05e8086a44447adccf5bd3f30`*  
*  macOS ARM64 : `5215a59ec18d444f7565d3351276049b39565e253b98875f733068d2e7e48a22`*  
*  Windows x64 : `0f687dc8bd84a0cdd3a7b0f631c79201ba0a8f0a0c869a15f6a1e8950b884cf3`*  
*SHA256 release v1.0.2 (historique, pré-HF) : `da92781f5e0d085a08a656b48ea49be3d54201b3db32b736204266ea89fd2b8b`*
