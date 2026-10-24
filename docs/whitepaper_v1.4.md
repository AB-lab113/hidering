# HIDERING (HRG) — WHITE PAPER OFFICIEL
**Enhanced Privacy Cryptocurrency avec Bitcoin-Style Emission**  
**100% PoW MINING · RandomX CPU**  
**Version 1.4.3 (révisée octobre 2026)**  
**Octobre 2026**

---

## TABLE DES MATIÈRES
1. Abstract
2. Vision & Problématique
3. Architecture Technique
4. Tokenomics (HRG)
5. Émission Bitcoin-Style
6. Résistance Quantique
7. Distribution
8. Roadmap Complète
9. Pool Mining
10. Comparaison Technique
11. Spécifications Techniques
12. Conformité & Légal
13. Ressources
14. Conclusion

---

## 1. ABSTRACT

HIDERING (HRG) est un protocole de cryptomonnaie axé sur la confidentialité maximale, dérivé de Monero (série 0.18), combinant les innovations d'anonymat avancées avec un modèle économique Bitcoin-style.

Émission 100% via PoW RandomX, supply cap irrévocable **18 millions HRG**. Le code ne contient ni premine ni allocation ; voir §7.

**INNOVATIONS CLÉS** :
- Ring size 32 par défaut dans les wallets HIDERING (CLI, RPC et GUI) ; le consensus accepte 32 à 64 (Monero : 16 fixe)
- Padding réseau hérité de Monero (granularité 1024 bytes)
- Padding interne de `tx_extra` à 2500 o par le wallet HRG (règle du wallet, pas du consensus)
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

- **Anonymat on-chain** : ring signatures (32 membres par défaut, jusqu'à 64), stealth addresses one-time, RingCT. Un anneau plus grand contient plus de leurres par entrée, mais la taille d'anneau est publique on-chain : une taille peu utilisée rend la transaction plus facile à reconnaître ; la taille par défaut (32) est recommandée.
- **Anonymat réseau** : propagation Dandelion++ (héritée de Monero). Le trafic P2P n'est pas chiffré.
- **Confidentialité des montants** : RingCT (engagements de Pedersen) ; padding interne de `tx_extra` à 2500 o par le wallet HRG (règle du wallet, pas du consensus)
- **Résistance future** : migration post-quantique planifiée via hard fork (2027)

### 2.3 Modèle économique sain

HIDERING adopte le modèle Bitcoin (supply fixe, halving, émission PoW pure) pour garantir la prédictibilité monétaire et l'équité à long terme.

---

## 3. ARCHITECTURE TECHNIQUE

### 3.1 Base

HIDERING est un fork de **Monero (série 0.18)** (CryptoNote), bénéficiant de :
- RingCT (confidentialité des montants)
- Bulletproofs+ (taille réduite des preuves)
- RandomX (PoW conçu pour les CPU)
- Dandelion++ (diffusion des transactions)

### 3.2 Modifications principales

| Composant | Monero original | HIDERING |
|-----------|----------------|----------|
| Ring size | 16 | 32 par défaut (32–64 autorisé) |
| TX padding | Variable | `tx_extra` 2500 o, règle du wallet (réseau : Monero 1024 o) |
| Réseau | Dandelion++ | Dandelion++ (hérité) |
| Montants | Masqués (RingCT) | Masqués (RingCT) |
| Stealth | One-time | One-time (CryptoNote) |
| PoW | RandomX | RandomX (identique) |
| Hard Fork | HFv15 | HFv15 dès bloc 1 |

> **Note transparence (revue interne juin 2026, mise à jour).** Le biais de routage stem « multi-hops best-effort » initialement annoncé a été **retiré** : le compteur de hops sérialisé sur le wire révélait la distance à l'origine de la transaction (un pair recevant un compteur à 0 identifiait l'émetteur), dégradant l'anonymat que Dandelion++ protège. La propagation repose désormais sur **Dandelion++ stock** hérité de Monero, sans mécanisme multi-hop spécifique à HIDERING. Le padding interne 2500 o porte sur le champ `tx_extra` ; le padding *réseau* reste celui de Monero (granularité 1024 o). Ces formulations ont été requalifiées pour refléter exactement le code.

---

## 4. TOKENOMICS (HRG)

### 4.1 Spécifications Économiques

| Paramètre | Valeur |
|-----------|--------|
| Ticker | HRG |
| Supply maximum | **18,000,000 HRG (hard cap irrévocable)** |
| Supply circulant max | **18,000,000 HRG** (somme exacte 17,999,999.99999418 HRG : la récompense est arrondie vers le bas à chaque halving) — le NUMS genesis 157.14 HRG est unspendable et hors comptabilité d'émission, donc non déduit du cap |
| Émission | **100% PoW** (pas de seed round, pas de vente privée) |
| Block time | 120 secondes |
| Halving interval | 210,000 blocs (~292 jours) |
| Difficulty adjustment | Chaque bloc (RandomX) |
| Algorithme PoW | RandomX (optimisé CPU) |
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

La série géométrique converge vers le cap de **18M HRG**. Émission 100% PoW, pas d'émission de queue, cap irrévocable.

### 4.4 Note technique — Choix du cap 18M

Le cap original de 33M HRG provoquait un **overflow uint64** (33 × 10¹⁹ > UINT64\_MAX = 1.844 × 10¹⁹) avec 12 décimales atomiques héritées de Monero. Comportement observé sur les binaires v1.0.0 : wrap silencieux sous Linux gcc (cap effectif ~14.55M), erreur fatale de compilation sous macOS clang. Le cap de **18M HRG** (1.8 × 10¹⁹, marge uint64 ~2.5%) a été adopté le 11 mai 2026 et implémenté dans la release v1.0.1.

---

## 5. ÉMISSION BITCOIN-STYLE

### 5.1 Philosophie

| Aspect | Bitcoin | **HIDERING** |
|--------|---------|--------------|
| Supply cap | 21M BTC | **18M HRG** |
| Émission | 100% PoW | **100% PoW** |
| Mécanisme | Halving tous les ~4 ans | Halving tous les ~292 jours |
| PoW | SHA-256 (ASIC) | RandomX (optimisé CPU) |

### 5.2 Code Halving C++

Le halving est indexé sur les pièces déjà émises (pas directement sur la hauteur). Extrait de `get_block_reward()` (`src/cryptonote_basic/cryptonote_basic_impl.cpp`, commentaires abrégés) :

```cpp
const uint64_t MONEY_SUPPLY_LOCAL = 18000000000000000000ULL; // 18M HRG
const uint64_t HALVING_INTERVAL_LOCAL = 210000;
const uint64_t INITIAL_REWARD_LOCAL = 42857142857143ULL;     // 42.857142... HRG

if (already_generated_coins >= MONEY_SUPPLY_LOCAL) {
    reward = 0;
    return true;
}

// Nombre de halvings : on parcourt les ères tant que leurs pièces sont déjà émises
uint64_t halvings = 0;
{
    uint64_t coins_counted = 0;
    uint64_t period_reward = INITIAL_REWARD_LOCAL;
    while (period_reward > 0) {
        uint64_t period_coins = period_reward * HALVING_INTERVAL_LOCAL;
        if (coins_counted + period_coins > already_generated_coins)
            break;
        coins_counted += period_coins;
        halvings++;
        period_reward >>= 1;
    }
}

uint64_t base_reward = INITIAL_REWARD_LOCAL >> halvings;    // / 2^halvings

// Plafond : ne jamais dépasser le supply max
if (already_generated_coins + base_reward > MONEY_SUPPLY_LOCAL)
    base_reward = MONEY_SUPPLY_LOCAL - already_generated_coins;

// Pas de tail emission
if (base_reward == 0) {
    reward = 0;
    return true;
}
// (la pénalité de poids de bloc héritée de Monero s'applique ensuite)
```

---

## 6. RÉSISTANCE QUANTIQUE

La cryptographie post-quantique sera introduite via un **hard fork planifié en 2027** :

- **Signatures** : ML-DSA-65 (FIPS 204, NIST PQC standard)
- **Échange de clés** : ML-KEM-768 (FIPS 203, NIST PQC standard)
- **Nouvelles adresses** : format BQ... dédié aux clés ML-KEM-768 (FIPS 203) (le format B... classique reste inchangé)
- **Impact** : les transactions futures (post-fork) utilisent les nouveaux schémas. La blockchain historique n'est pas affectée.
- **Compatibilité** : mise à jour obligatoire des binaires au moment du fork.

**État (septembre 2026).** Le prototype post-quantique (intégration liboqs (0.15.0 dans les binaires publiés, 0.16.0 sur la branche de développement), keygen BQ, persistance des clés, signatures ML-DSA-65 (FIPS 204) en `tx_extra`, KEM ML-KEM-768 (FIPS 203)) est implémenté sur la branche de développement et a fait l'objet d'une revue interne assistée par IA (pas d'audit tiers). Tout le code PQ reste **inerte**. Le code actuel porte une hauteur d'activation provisoire (2 000 000) qui n'est pas la date prévue ; la vraie hauteur d'activation sera fixée par une future release et annoncée très en avance. Le chemin nominal est implémenté : binding C-1, checks validateur a–e, spend-side wallet2, fix H-5 multi-ciphertext, acceptation mempool, activation wallet. Le flow e2e complet B...→BQ...→dépense est validé sur regtest (commits `7d53cb940`→`b72425afe`, branche v2-privacy). Cible : **2027, pas encore planifiée**.

### 6.1 Délai réel d'activation — ce qu'il faut vraiment pour passer à HFv16

Les versions antérieures de ce document annonçaient une « activation possible en 24–48h via
changement de la constante `HF_HEIGHT_PQ` ». **Cette affirmation était fausse et est retirée.**
Modifier une constante prend effectivement quelques minutes ; ce n'est pas ce qui gouverne le
délai. Une activation HFv16 est aujourd'hui bloquée par des points ouverts, et le sera encore
au minimum plusieurs mois :

**Bloquants de sécurité.** La bibliothèque post-quantique utilisée (liboqs) porte toujours,
dans sa version courante, l'avertissement de ses auteurs : *« nous ne recommandons pas de vous
reposer sur cette bibliothèque en environnement de production ni pour protéger des données
sensibles »*. Elle ne détient aucune validation FIPS 140-3 — seuls les **algorithmes**
(ML-KEM/ML-DSA) sont des standards NIST finaux, pas l'implémentation. Tant que ce point n'est
pas résolu (audit tiers, implémentation validée, ou posture hybride formalisée), **HFv16 ne
sera pas activé.**

**Points fonctionnels ouverts.** Les subaddresses BQ sont implémentées dans le cœur du wallet et la CLI (branche de développement), pas encore exposées par wallet-rpc ni la GUI. *(Deux points
ouverts ont été fermés le 8 septembre 2026 : le **cold-signing** d'une dépense BQ — le fichier
de transfert transporte le ciphertext ML-KEM-768, jamais le secret partagé, et la machine
hors-ligne re-dérive celui-ci avec ses propres clés ; et la **consolidation B.../BQ**, via les
transactions hybrides décrites au §6.3.)*

**Le coût réel, c'est la coordination.** Un hard fork n'est pas un déploiement logiciel : il
faut publier des binaires pour toutes les plateformes, laisser mineurs, pools, services et
détenteurs de wallets mettre à jour, et annoncer la hauteur d'activation **très en avance**
pour que le réseau converge. Un nœud non mis à jour se retrouve sur une chaîne morte. C'est un
processus de **plusieurs semaines**, irréductible, quelle que soit l'urgence.

**En cas de menace quantique réelle**, le calendrier serait resserré au maximum et la
communication faite en urgence — mais l'ordre de grandeur reste **des semaines**, pas des
heures. Tout engagement contraire serait malhonnête.

### 6.2 Compromis assumé : une dépense BQ est transparente

Ce point est essentiel et n'apparaissait pas dans les versions précédentes de ce document.

HIDERING revendique une confidentialité forte, assurée par les signatures de cercle (ring
32–64), les adresses furtives et RingCT (montants masqués par engagements de Pedersen). **Ces
garanties ne s'appliquent pas de la même façon à une dépense d'output BQ.**

Techniquement : une entrée post-quantique (`txin_to_key_pq`) ne porte pas d'engagement de
Pedersen, donc sa valeur ne peut pas être équilibrée par RingCT. La transaction est par
conséquent construite en **v2 `RCTTypeNull`** — c'est-à-dire :

- **les montants sont révélés en clair** sur la chaîne (entrées comme sorties) ;
- **il n'y a pas de signature de cercle** sur l'entrée BQ : l'output dépensé est désigné
  directement, sans leurres. L'anonymat de cercle est donc **nul** sur cette entrée ;
- l'autorisation repose sur la signature ML-DSA-65 par sortie et le bind tag, pas sur un ring.

Ce que cela veut dire concrètement : **une dépense depuis une adresse BQ est aussi
transparente qu'une transaction Bitcoin.** La confidentialité HIDERING complète reste celle des
adresses classiques B... . Les adresses BQ achètent une résistance quantique **au prix de la
confidentialité de la dépense.**

Ce qui reste protégé : *recevoir* sur une adresse BQ ne révèle rien de plus qu'une réception
classique — la sortie est furtive et non liée à l'adresse publiée. C'est la **dépense** de cet
output qui est transparente.

Une véritable signature de cercle post-quantique est un problème de recherche ouvert, et
HIDERING ne prétend pas l'avoir résolu.

### 6.3 Transactions hybrides — récupérer la confidentialité des montants de sortie

Depuis le 8 septembre 2026, une transaction peut **mélanger** une entrée transparente BQ et
des entrées ring classiques. Deux conséquences :

- **Consolidation possible.** Un wallet peut enfin dépenser ses fonds B... et BQ... dans une
  même transaction, et payer un montant supérieur au solde de l'un ou l'autre monde. Sans
  cela, un utilisateur en cours de migration se retrouvait avec deux tirelires étanches.
- **Plus de confidentialité, pas moins.** Une dépense BQ « pure » est une transaction
  entièrement transparente (§6.2) : **tous** les montants de sortie sont en clair. Une
  transaction hybride est une vraie transaction RingCT — **seuls les montants des entrées PQ
  sont révélés, les sorties redeviennent des engagements de Pedersen avec preuves de portée.**

Comment l'équilibre tient : une entrée transparente de valeur *a* n'a pas d'engagement, mais
le vérificateur peut en recalculer un depuis des données publiques, avec un masque nul (*a·H*).
L'équation RingCT devient `Σ pseudoOuts + Σ a_pq·H == Σ sorties + frais·H` — autrement dit,
**les entrées transparentes se comportent exactement comme des frais négatifs.**

Ce qui reste vrai dans tous les cas : le montant d'une entrée BQ dépensée est public. L'hybride
ne masque pas *ce que vous dépensez* depuis une adresse BQ ; il masque de nouveau *ce que vous
envoyez*. La recommandation du §6.2 est inchangée.

**Recommandation aux utilisateurs, tant que ce compromis existe :** utilisez les adresses
classiques B... pour vos usages courants. Les adresses BQ sont destinées à ceux dont le modèle
de menace inclut un adversaire quantique et qui acceptent, en connaissance de cause, la
transparence des montants et l'absence de ring sur leurs dépenses.

---

## 7. DISTRIBUTION

**Principe** : émission 100% via PoW RandomX, aucune vente privée, aucun seed round. Bloc 0 est un genesis avec 157.14 HRG verrouillés sous clé NUMS (cryptographiquement unspendable, héritage chaîne — voir `GENESIS_PROOF.md`). Depuis le bloc 1, chaque récompense de bloc revient à celui qui le mine ; les nœuds du projet minent sous les mêmes règles que tout le monde.

### 7.1 v2.0.0 (post-attaque 16 mai 2026)

Après l'attaque 51% sur la chaîne v1.x (16 mai 2026, h≈3577), v2.0.0 est passé à un nouveau Network ID (`HRG\x02HIDERINGMAIN`), ce qui a séparé le nouveau réseau de la chaîne v1.x.

Des versions antérieures de ce document annonçaient une réserve minée détenue par les nœuds fondateurs, dont le volume et l'usage devaient être divulgués au lancement. Cette annonce est retirée.

### 7.2 Engagements

- ✓ Algorithme PoW RandomX (conçu pour les CPU)
- ✓ Aucune vente privée, aucun seed round, aucun ICO
- ✓ Émission 100% PoW
- ✓ Genesis vérifiable on-chain (`GENESIS_PROOF.md`)

---

## 8. ROADMAP COMPLÈTE

**PHASE 0 : SETUP ✅**
- Fork de Monero
- Compilation Ubuntu AMD
- Dependencies OK

**PHASE 1 : CORE TOKENOMICS ✅**
- MONEY_SUPPLY = 18M HRG (uint64-safe)
- HALVING_INTERVAL = 210,000 blocs
- INITIAL_REWARD = 42.86 HRG/bloc
- Genesis vérifiable (NUMS, unspendable)

**PHASE 2 : PRIVACY ENHANCEMENTS ✅**
- P1 ✅ Ring size 32 par défaut (consensus : 32 à 64)
- P2 ✅ Padding interne de `tx_extra` à 2500 o par le wallet HRG (règle du wallet, pas du consensus) ; padding réseau = Monero 1024 o
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
- ✅ **Release v2.0.3 (19 juin 2026)** — dernière publique, ajoute checkpoint h=25000 ; bundles complets (daemon + wallet) Linux x64, macOS ARM64 et Windows x64, publiés automatiquement par CI
- ✅ Binaires multi-plateformes v2.0.2 (Linux / macOS ARM64 / Windows) — chaque asset accompagné de son sidecar SHA256
- ✅ Wallet GUI Desktop multi-plateformes (v2.0.2-gui : Linux AppImage, macOS ARM64, Windows x64)
- ✅ Pool mining RandomX/HRG en production (`pool.hidering.org:3333` — voir §9)
- ⏳ Launch public (Twitter, Reddit, BitcoinTalk)

**PHASE 5 : POST-QUANTIQUE (2027)**
- ML-DSA-65 (FIPS 204) + ML-KEM-768 (FIPS 203) — prototype, revue interne assistée par IA, aucun audit tiers ; code inerte
- 🔧 Implémenté sur la branche de développement (A1 spec, A2 validateur, A3 spend-side, A4 wallet) — pas encore dans une release
- ✅ **Tests e2e BQ regtest validés (juin 2026)**
- Hard fork PQC mainnet (cible T2 2027)

---

## 9. POOL MINING

HIDERING opère un pool de minage public RandomX en production pour abaisser la barrière d'entrée au minage CPU et faciliter la participation à l'émission 100% PoW.

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

Le minage solo reste pleinement supporté via `hideringd` + `hidering-wallet-cli`. RandomX étant conçu pour les CPU, un simple processeur de bureau permet de participer à l'émission.

### 9.1 Checkpoints d'intégrité

La chaîne v2.0.0 embarque des ancres de checkpoint (hauteurs 2939, 5000, 11000, 16000, 20000, 25000, 80386, 90000) facilitant la synchronisation initiale et l'alignement des nœuds. Les binaires publics de la release v2.0.3 n'en contiennent que 6 (jusqu'à h=25000). La sécurité du consensus repose avant tout sur le PoW RandomX cumulé ; ces ancres sont un aide à la convergence, pas un substitut à la preuve de travail.

---

## 10. COMPARAISON TECHNIQUE

| Fonctionnalité | Monero | **HIDERING** |
|---------------|--------|--------------|
| Ring size | 16 (fixe) | 32 par défaut (32–64 autorisé) |
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
| Fork base | Monero (série 0.18) |
| Network ID | `HRG\x02HIDERINGMAIN` (v2.0.0) |
| Magic bytes | `0x48524702` (v2.0.0) |
| Port P2P | 19740 |
| Port RPC | 19741 |
| Préfixe adresses | 60 → "B" |
| Décimales | 12 (10¹²) |
| Block time | 120 secondes |
| PoW | RandomX (optimisé CPU) |
| HFv15 | Dès bloc 1 |
| Ring size | 32 par défaut (32–64 autorisé) |
| Supply | 18,000,000 HRG |
| Reward initial | 42.857142857143 HRG/bloc |
| Halving | 210,000 blocs (~292 jours) |
| Unlock | 60 blocs (coins minés) · 10 blocs (sorties reçues) |
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
- **OTC Trading Bot** : @HideringOTCBot (Telegram) — échange P2P HRG ⇄ USDT/USDC via escrow automatique

---

## 14. CONCLUSION

HIDERING (HRG) représente une évolution significative de la confidentialité financière on-chain. En combinant les meilleures innovations de Monero avec un modèle économique Bitcoin-style (supply fixe 18M, halving, émission 100% PoW) et des améliorations privacy substantielles (ring size 32–64, TX padding interne 2500 o) par-dessus la propagation Dandelion++ et le RingCT hérités de Monero, HIDERING offre une confidentialité forte par construction.

La distribution se fait par compétition PoW. La roadmap post-quantique (2027) assure la pérennité du protocole face aux menaces futures.

**HIDERING — Privacy by design. Pure PoW.** 🚀

---

*Whitepaper v1.4.3 — Octobre 2026 (révisions antérieures : 26 juin 2026, implémentation HFv16 complète — binding C-1, validateur, spend-side, wallet, e2e regtest validé ; revue de sécurité interne juin 2026, corrections privacy, pool mining + checkpoints)*  
*SHA256 release v2.0.3 (dernière publique) :*  
*  Linux x64 : `32d93e00e0413cbf398f324c5daa31a772e0ee76ee67b0a6796c705a60d797a7`*  
*  macOS ARM64 : `f98124eb6230f2ea26bca09db6468f898b99446f689ceda5d8affb35131dbb54`*  
*  Windows x64 : `f8dfc4447b69b7fa75083da55be4925a11f35cc5c35888b2d47c8d08c0a9bfb3`*  
*SHA256 release v1.0.2 (historique, pré-HF) : `da92781f5e0d085a08a656b48ea49be3d54201b3db32b736204266ea89fd2b8b`*
