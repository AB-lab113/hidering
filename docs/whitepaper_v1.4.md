# HIDERING (HRG) — WHITE PAPER OFFICIEL
**Enhanced Privacy Cryptocurrency avec Bitcoin-Style Emission**  
**FAIR LAUNCH · 100% PoW MINING · RandomX CPU**  
**Version 1.4 (révisée 6 Juin 2026 — post-audit sécurité juin 2026)**  
**Juin 2026**

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
9. Pool Mining
10. Comparaison Technique
11. Spécifications Techniques
12. Conformité & Légal
13. Ressources
14. Conclusion

---

## 1. ABSTRACT

HIDERING (HRG) est un protocole de cryptomonnaie axé sur la confidentialité maximale, dérivé de Monero v0.18.1, combinant les innovations d'anonymat avancées avec un modèle économique Bitcoin-style.

**FAIR LAUNCH** : émission 100% via PoW RandomX, supply cap irrévocable **18 millions HRG**. v2.0.0 introduit une courte phase de pré-minage en réseau privé (réponse à l'attaque 51% du 16 mai 2026 sur la chaîne v1.x à h≈3577) constituant une réserve minée — voir §7 pour la justification complète et l'usage des fonds.

**INNOVATIONS CLÉS** :
- Ring size dynamique 32–64 (vs Monero 16)
- Padding réseau hérité de Monero (granularité 1024 bytes)
- TX padding interne fixe 2500 bytes (tx_extra)
- Propagation des transactions via Dandelion++ (stem/fluff, héritée de Monero)
- Montants confidentiels via RingCT (engagements de Pedersen)
- Stealth addresses one-time (CryptoNote)
- Résistance quantique programmée (2027)

---

## 2. VISION & PROBLÉMATIQUE

### 2.1 Le problème de la vie privée financière

Les blockchains publiques traditionnelles (Bitcoin, Ethereum) exposent intégralement les flux de transactions. Même Monero, référence en matière de confidentialité, présente des limites : ring size fixe à 16, métadonnées réseau exploitables, timing attacks possibles.

### 2.2 La vision HIDERING

HIDERING vise la confidentialité maximale par construction :

- **Anonymat on-chain** : ring signatures 32–64, stealth addresses one-time, RingCT
- **Anonymat réseau** : propagation Dandelion++ (héritée de Monero), chiffrement bout-en-bout hérité de Monero
- **Confidentialité des montants** : RingCT (engagements de Pedersen) ; TX padding interne fixe 2500 bytes (tx_extra) pour uniformiser la taille des champs annexes
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
| TX padding | Variable | TX interne 2500 o (réseau : Monero 1024 o) |
| Réseau | Dandelion++ | Dandelion++ (hérité) |
| Montants | Masqués (RingCT) | Masqués (RingCT) |
| Stealth | One-time | One-time (CryptoNote) |
| PoW | RandomX | RandomX (identique) |
| Hard Fork | HFv15 | HFv15 dès bloc 1 |

> **Note transparence (audit juin 2026, mise à jour).** Le biais de routage stem « multi-hops best-effort » initialement annoncé a été **retiré** : le compteur de hops sérialisé sur le wire révélait la distance à l'origine de la transaction (un pair recevant un compteur à 0 identifiait l'émetteur), dégradant l'anonymat que Dandelion++ protège. La propagation repose désormais sur **Dandelion++ stock** hérité de Monero, sans mécanisme multi-hop spécifique à HIDERING. Le padding interne 2500 o porte sur le champ `tx_extra` ; le padding *réseau* reste celui de Monero (granularité 1024 o). Ces formulations ont été requalifiées pour refléter exactement le code.

---

## 4. TOKENOMICS (HRG)

### 4.1 Spécifications Économiques

| Paramètre | Valeur |
|-----------|--------|
| Ticker | HRG |
| Supply maximum | **18,000,000 HRG (hard cap irrévocable)** |
| Supply circulant max | 17,999,842.86 HRG (genesis NUMS 157.14 HRG déduit) |
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

- **Signatures** : ML-DSA-65 (FIPS 204, NIST PQC standard)
- **Échange de clés** : ML-KEM-768 (FIPS 203, NIST PQC standard)
- **Nouvelles adresses** : format BQ... dédié aux clés ML-KEM-768 (FIPS 203) (le format B... classique reste inchangé)
- **Impact** : les transactions futures (post-fork) utilisent les nouveaux schémas. La blockchain historique n'est pas affectée.
- **Compatibilité** : mise à jour obligatoire des binaires au moment du fork.

**État (juin 2026).** Le prototype post-quantique (intégration liboqs 0.15.0, keygen BQ, persistance des clés, signatures ML-DSA-65 (FIPS 204) en `tx_extra`, KEM ML-KEM-768 (FIPS 203)) est implémenté et a fait l'objet d'un audit de sécurité interne. Tout le code PQ reste **inerte** jusqu'à l'activation du hard fork (HFv16) ; la spécification du *binding* validateur des clés PQ doit être finalisée avant toute activation. Cible mainnet : **T2 2027**.

---

## 7. DISTRIBUTION FAIR LAUNCH

**Principe** : émission 100% via PoW RandomX, aucune vente privée, aucun seed round. Bloc 0 est un genesis avec 157.14 HRG verrouillés sous clé NUMS (cryptographiquement unspendable, héritage chaîne — voir `GENESIS_PROOF.md`). Bloc 1 onwards : récompense distribuée par compétition PoW pure.

> **Transparence (révision juin 2026).** La promesse « aucune allocation à l'équipe » du narratif v1.x **n'est plus exacte depuis v2.0.0** : la phase de pré-minage en réseau privé (§7.1) constitue une **réserve minée** détenue par les nœuds fondateurs avant la ré-ouverture publique. Cette réserve n'est pas une vente privée ni un seed round — elle est produite par PoW sous les mêmes règles RandomX — mais elle existe et son usage est détaillé ci-dessous.

### 7.1 Phase pré-publique v2.0.0 (post-attaque 16 mai 2026)

Peu après le lancement v1.x, le réseau HIDERING a subi une attaque 51% (16 mai 2026, chaîne v1.x à h≈3577) : un mineur externe disposant d'une puissance supérieure à l'ensemble du réseau a dominé la chaîne et causé une réorganisation majeure, rendant orphelins tous les blocs minés. La réponse v2.0.0 est duale :

- **Hard fork réseau** : le Network ID passe de `HRG\x01HIDERINGMAIN` à `HRG\x02HIDERINGMAIN` (magic `0x48524701` → `0x48524702`), isolant immédiatement v2.0.0 du mineur malveillant et permettant la consolidation du hashrate sur réseau privé avant ré-ouverture publique.
- **Phase de pré-minage** : avant l'ouverture publique, les nœuds fondateurs minent en réseau privé pour trois raisons :
  1. **Durcir le réseau dès le premier jour public** — accumuler suffisamment de hashrate pour rendre une nouvelle attaque 51% économiquement prohibitive dès le launch.
  2. **Financer le développement** — les tests du wallet GUI et l'implémentation des signatures post-quantiques (ML-DSA-65 (FIPS 204) + ML-KEM-768 (FIPS 203), prévues 2027) requièrent des transactions on-chain réelles.
  3. **Constituer une réserve communautaire** — qui sera soit redistribuée aux mineurs et DEX participants au lancement public, soit partiellement brûlée pour réduire la supply en circulation.

Le pré-minage v2.0.0 est un **bouclier défensif et un levier de financement développement**, pas une vente d'insiders : tous les coins sont produits par PoW. Une fois le réseau ouvert publiquement, tous les mineurs concourent sous les mêmes règles RandomX, sans avantage insider sur des coins inaccessibles au public. Le volume exact, la durée et la destination finale de la réserve seront communiqués publiquement au launch.

### 7.2 Engagements

- ✓ Algorithme PoW RandomX (CPU-only, ASIC-resistant)
- ✓ Aucune vente privée, aucun seed round, aucun ICO
- ✓ Émission 100% PoW — y compris la réserve minée en phase privée (§7.1)
- ✓ Genesis vérifiable on-chain (`GENESIS_PROOF.md`)
- ✓ Transparence sur la phase pré-publique v2.0.0 : durée, hashrate, volume et destination de la réserve minée (communiqués au launch public)

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
- P2 ✅ TX padding interne 2500 bytes (tx_extra) ; padding réseau = Monero 1024 o
- P3 ⊘ Routage stem multi-hops **retiré** : retour à Dandelion++ stock (le compteur de hops sérialisé fuyait la distance à l'origine ; voir Note transparence §3.2)
- P4 ✅ Montants confidentiels (RingCT — engagements de Pedersen)
- P5 ✅ Stealth addresses one-time (CryptoNote)

**PHASE 3 : INFRASTRUCTURE ✅**
- Rebranding complet HIDERING
- Seed nodes sur VPS dédiés (seed1/seed2.hidering.org)
- DNS seed1/seed2.hidering.org
- Block explorer (explorer.hidering.org)
- Site web hidering.org

**PHASE 4 : MAINNET LAUNCH (En cours)**
- ✅ Releases v1.0.0 → v1.0.2 Linux (rebrand, fix MONEY_SUPPLY 18M, fix bad_alloc boot daemon)
- ✅ **Hard fork v2.0.0 (16 mai 2026)** — NETWORK_ID bumpé `HRG\x01HIDERINGMAIN` → `HRG\x02HIDERINGMAIN`, magic `0x48524701` → `0x48524702`, version 1.0.2 → 2.0.0. Chaîne v1.x (pré-launch, ~h≈3577) retirée au profit du nouveau réseau v2.0.0.
- ✅ **Release v2.0.2 (1 juin 2026)** — dernière publique, bundles complets (daemon + wallet) Linux x64, macOS ARM64 et Windows x64, publiés automatiquement par CI
- ✅ Binaires multi-plateformes v2.0.2 (Linux / macOS ARM64 / Windows) — chaque asset accompagné de son sidecar SHA256
- ✅ Wallet GUI Desktop multi-plateformes (v2.0.2-gui : Linux AppImage, macOS ARM64, Windows x64)
- ✅ Pool mining RandomX/HRG en production (`pool.hidering.org:3333` — voir §9)
- ⏳ Launch public (Twitter, Reddit, BitcoinTalk)

**PHASE 5 : POST-QUANTIQUE (2027)**
- ML-DSA-65 (FIPS 204) + ML-KEM-768 (FIPS 203) signatures (prototype audité, code inerte jusqu'à HFv16)
- Finalisation spec binding PQ + tests end-to-end
- Hard fork PQC mainnet (cible T2 2027)

---

## 9. POOL MINING

HIDERING opère un pool de minage public RandomX en production pour abaisser la barrière d'entrée au minage CPU et faciliter la participation au fair launch.

| Paramètre | Valeur |
|-----------|--------|
| Endpoint stratum | `pool.hidering.org:3333` |
| Algorithme | RandomX (`rx/0`) |
| Stack | cryptonote-nodejs-pool + Redis + Nginx |
| Difficulté de départ | varDiff (auto-ajustée) |
| Récompense bloc | 42.857142857143 HRG (période 1) |

Exemple de commande mineur (xmrig) :

```
xmrig -o pool.hidering.org:3333 -u <votre_adresse_B...> -p x -a rx/0
```

Le minage solo reste pleinement supporté via `hideringd` + `hidering-wallet-cli`. RandomX étant CPU-only et résistant aux ASIC, un simple processeur de bureau permet de participer à l'émission.

### 9.1 Checkpoints d'intégrité

La chaîne v2.0.0 embarque des ancres de checkpoint (hauteurs 2939, 5000, 11000, 16000) facilitant la synchronisation initiale et l'alignement des nœuds. La sécurité du consensus repose avant tout sur le PoW RandomX cumulé ; ces ancres sont un aide à la convergence, pas un substitut à la preuve de travail.

---

## 10. COMPARAISON TECHNIQUE

| Fonctionnalité | Monero | **HIDERING** |
|---------------|--------|--------------|
| Ring size | 16 (fixe) | 32–64 (dynamique) |
| TX padding | Non | TX interne 2500 o (réseau : Monero 1024 o) |
| Routage réseau | Dandelion++ | Dandelion++ (hérité) |
| Montants | Masqués (RingCT) | Masqués (RingCT) |
| Stealth | One-time | One-time (CryptoNote) |
| Supply | Tail emission | 18M hard cap |
| Halving | Non | Tous les 210,000 blocs |
| PoW | RandomX | RandomX (identique) |
| Post-quantique | Non planifié | 2027 (ML-DSA-65 (FIPS 204) + ML-KEM-768 (FIPS 203)) |

---

## 11. SPÉCIFICATIONS TECHNIQUES

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
| Unlock window | 60 blocs |
| Émission | 100% PoW (voir §7) |
| Repo | github.com/AB-lab113/hidering |

---

## 12. CONFORMITÉ & LÉGAL

HIDERING est un logiciel open-source distribué sous licence identique à Monero (BSD 3-Clause). L'équipe ne donne aucun conseil financier. HRG est un actif expérimental. Les utilisateurs sont responsables du respect des lois locales relatives aux cryptomonnaies.

---

## 13. RESSOURCES

- **Site web** : https://hidering.org
- **GitHub** : https://github.com/AB-lab113/hidering
- **Releases** : https://github.com/AB-lab113/hidering/releases
- **Block Explorer** : https://explorer.hidering.org
- **Pool mining** : pool.hidering.org:3333
- **DNS Seed 1** : seed1.hidering.org
- **DNS Seed 2** : seed2.hidering.org

---

## 14. CONCLUSION

HIDERING (HRG) représente une évolution significative de la confidentialité financière on-chain. En combinant les meilleures innovations de Monero avec un modèle économique Bitcoin-style (supply fixe 18M, halving, émission 100% PoW) et des améliorations privacy substantielles (ring size 32–64, TX padding interne 2500 o) par-dessus la propagation Dandelion++ et le RingCT hérités de Monero, HIDERING offre une confidentialité forte par construction.

Le fair launch garantit une distribution par compétition PoW pure. La phase pré-publique v2.0.0 (réponse à l'attaque 51% du 16 mai 2026, §7.1) durcit le réseau et finance le développement avant la ré-ouverture publique ; la réserve qui en résulte est minée, transparente et son usage sera communiqué au launch. La roadmap post-quantique (2027) assure la pérennité du protocole face aux menaces futures.

**HIDERING — Privacy by design. Fair by launch.** 🚀

---

*Whitepaper v1.4 — Juin 2026 (révisé 6 juin 2026 après l'audit sécurité de juin 2026 : corrections privacy, transparence réserve minée, ajout pool mining + checkpoints)*  
*SHA256 release v2.0.2 (dernière publique) :*  
*  Linux x64 : `844b0cac1cb3192c9d616dffa50da538399447c05e8086a44447adccf5bd3f30`*  
*  macOS ARM64 : `5215a59ec18d444f7565d3351276049b39565e253b98875f733068d2e7e48a22`*  
*  Windows x64 : `0f687dc8bd84a0cdd3a7b0f631c79201ba0a8f0a0c869a15f6a1e8950b884cf3`*  
*SHA256 release v1.0.2 (historique, pré-HF) : `da92781f5e0d085a08a656b48ea49be3d54201b3db32b736204266ea89fd2b8b`*
