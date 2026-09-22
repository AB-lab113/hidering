# M-11 — Clôture par acceptation de risque documentée : posture hybride (livrable A)

**Date :** 22 septembre 2026
**Statut :** ✅ **Acceptation de risque actée par l'opérateur** pour la maturité de liboqs.
M-11 est clos **par acceptation de risque documentée, et NON par un audit tiers.**
⚠️ **La levée du blocage HFv16 reste conditionnée** aux deux livrables qui complètent
celui-ci (§8) : B — preuve de temps constant, C — vecteurs NIST. Tant qu'ils n'existent
pas, la règle « ne pas activer HFv16 avant la levée de M-11 » (CLAUDE.md, NE JAMAIS FAIRE)
reste en vigueur.
**Document précédent :** `M-11_liboqs_maturity_2026-09-08.md` (réévaluation du 8 septembre,
mise à jour le 13 septembre). Ce document ne le modifie pas ; il le complète.

---

## 1. Décision et périmètre

La réévaluation du 8 septembre posait trois conditions de levée (§8 de ce document) :

| # | Condition d'origine | Traitement retenu |
|---|---|---|
| 1 | Le disclaimer « prototyping » de liboqs a-t-il sauté ? Audit tiers publié ? | **Remplacée** par la présente acceptation de risque. Le disclaimer est toujours là (`external/liboqs/README.md:91`, tag 0.16.0) et l'opérateur a décidé de ne pas commander d'audit tiers. La condition n'est donc pas « remplie » : elle est **abandonnée au profit d'une acceptation explicite**, compensée par les mesures des livrables A, B et C. |
| 2 | Implémentation FIPS-validée (CMVP) **ou** posture hybride | **Traitée ici** (livrable A) : la posture hybride est formalisée comme exigence de conception (§2 à §5). Aucune validation FIPS 140-3 (CMVP) n'est revendiquée par liboqs : l'arbre 0.16.0 ne contient aucune mention de « CMVP » ni de « FIPS 140 » (README, SECURITY.md, docs/) — ses seules références FIPS désignent les standards d'algorithmes FIPS 203/204/205 (`README.md:49-72`). C'est une **absence constatée**, pas une négation prouvée : la base CMVP du NIST n'a pas été consultée. La branche « hybride » de la condition est celle retenue. |
| 3 | Figer la version + re-audit temps constant des chemins utilisés | **Livrable B, à venir.** Version figée : submodule épinglé `5a1a854b0dc9f2141bdc771c555ee60c37950183` (tag 0.16.0, dernière release upstream au 22 septembre 2026). Re-audit temps constant : non fait à ce jour (§7). |
| — | *(mesure compensatoire ajoutée par l'opérateur)* | **Livrable C, à venir :** vecteurs officiels NIST ACVP pour ML-KEM-768 et ML-DSA-65, versionnés avec leur empreinte. |

## 2. Exigence de conception

> **La dépense d'une sortie BQ ne doit jamais dépendre de la seule correction de liboqs.**
> Toute dépense transparente (`txin_to_key_pq`) doit satisfaire simultanément une condition
> post-quantique (vérifiée par liboqs) et une condition classique (vérifiée par le code
> HIDERING, hors liboqs). Une faille qui ne toucherait que l'une des deux ne suffit pas.

## 3. Les checks qui portent cette exigence

Source : `src/cryptonote_core/blockchain.cpp`, validation de chaque `txin_to_key_pq`
(bloc gardé par `hf_version >= HF_VERSION_PQ`). Les trois checks sont **conjonctifs** :
chacun rejette la transaction à lui seul (`tvc.m_verifivation_failed = true; return false;`).

| Check | Ce qu'il exige | Lignes (`blockchain.cpp`) | Implémentation |
|---|---|---|---|
| **(c)** liaison C-1 | la clé d'autorisation révélée ouvre l'engagement publié dans le tag de liaison du tx créateur | 3488-3538 (type d'autorisation l.3521, engagement l.3528, comparaison et rejet l.3534) | **HIDERING / Keccak** : `pqc_compute_auth_commit` et `pqc_compute_bind_tag_v2` (`src/crypto/pqc.cpp:395-405`, `427-440`) reposent sur `crypto::cn_fast_hash`. **Pas liboqs.** |
| **(d)** signature post-quantique | la signature ML-DSA-65 de cette clé vérifie sur le hash du préfixe | 3540-3545 (appel l.3541) | **liboqs** : `pqc_tx_verify` → `OQS_SIG_verify` (`src/crypto/pqc.cpp:303`, `317`). **Seul check qui passe par liboqs.** |
| **(d2)** signature du propriétaire | la clé à usage unique Ed25519 de la sortie dépensée signe le même message | 3547-3563 (appel l.3558, rejet l.3560) | **HIDERING / Ed25519** : `crypto::check_signature`, code classique hérité de Monero. **Pas liboqs.** |

Les checks (a), (b), (b2) et (e) ne concernent pas liboqs (existence et non-dépense de la
sortie, clé révélée, ouverture de l'engagement de montant, conservation monétaire).

## 4. Matrice de défaillance

| Hypothèse de défaillance | Ce que l'attaquant obtient | Ce qui l'arrête | Résultat |
|---|---|---|---|
| **liboqs cassé ou fuyant** (défaut d'implémentation ML-DSA, fuite de la clé secrète d'identité par canal auxiliaire), attaquant **classique** | il peut satisfaire (c) et (d) | **(d2) seul** : il lui manque la clé à usage unique `x'`, qui dépend de la clé de dépense Ed25519 du destinataire | **Pas de vol.** |
| **Ed25519 cassé par Shor** (attaquant quantique, liboqs sain) | `x'` à partir de la clé publique `P'` révélée : (d2) satisfait | **(d)** : il lui faut la clé secrète ML-DSA d'identité de la (sous-)adresse, dérivée de `pq_root` et jamais publiée. **(c)** l'arrête aussi tant que la clé d'autorisation de cette (sous-)adresse n'a jamais été révélée | **Pas de vol**, tant que ML-DSA tient (voir §7). |
| **ML-KEM fuyant** (fuite de la clé secrète ML-KEM, défaut d'implémentation) | le secret partagé `ss` de chaque sortie BQ de l'adresse : il peut les **détecter**, dériver leur tweak et leur facteur d'aveuglement, donc les **relier** entre elles et à l'adresse | (d) et (d2) : `ss` ne donne ni la clé ML-DSA d'identité (dérivée de `pq_root`, indépendante de ML-KEM), ni `x'` (qui exige la clé de dépense) | **Atteinte à la confidentialité et à la non-associabilité des sorties. Pas de vol.** |

**Transactions hybrides** (entrées en anneau + entrées BQ) : les entrées en anneau restent
protégées par la signature CLSAG classique et la RingCT, indépendamment de tout ce qui
précède ; les entrées BQ de la même transaction passent par les mêmes checks (c), (d), (d2)
que dans une dépense BQ pure. Une défaillance de liboqs n'affecte pas la validité des entrées
en anneau. À l'inverse, la CLSAG n'est pas post-quantique : la protection quantique ne
concerne que la partie BQ.

## 5. Tests de non-régression

`src/crypto/pq_sender_clawback_test.cpp` (suite standalone, PASS au 22 septembre 2026)
reproduit le validateur et prouve deux contrôles négatifs :

* **émetteur classique** (connaît `ss`, forge sa propre clé d'autorisation, devine `x'`) :
  satisfait (b)(b2)(d)(e), **rejeté par (c) et par (d2)** (l.344-403) ;
* **émetteur quantique** (reçoit `x'` en clair, comme Shor le lui donnerait) : satisfait
  (b)(b2)(d)(d2)(e), **rejeté par (c) seul** (l.405-440).

⚠️ **Trou de couverture :** aucun test n'isole la ligne 1 de la matrice — un attaquant qui
satisfait (c) **et** (d) sans `x'` (simulation d'une clé ML-DSA d'identité fuitée ou d'une
signature forgée), et qui ne serait arrêté que par (d2). Cette ligne est aujourd'hui établie
par lecture du code (§3), pas par un test. **À ajouter** au même fichier : un troisième cas
qui donne à l'attaquant la clé secrète d'identité légitime et vérifie que (d2) est le seul
check en échec.

## 6. Ce que la posture hybride ne couvre pas

* **Attaquant quantique + défaut de liboqs.** Contre un attaquant quantique, (d2) tombe
  (Shor) ; la barrière durable est alors (d), donc ML-DSA dans liboqs. Un défaut
  d'implémentation de liboqs exploité par un attaquant quantique n'est **pas** couvert par
  la posture hybride : c'est la limite intrinsèque de tout schéma hybride.
* **(c) n'est une barrière contre Shor que si la clé d'autorisation n'a jamais été
  révélée**, c'est-à-dire si chaque sous-adresse BQ n'est dépensée qu'une fois. Cette règle
  (T2) est **imposée par le wallet** (`m_pq_spent_subaddresses`, `wallet2.h:1286`, `1295` ;
  refus R-a, `wallet2.cpp:1718`, `simplewallet.cpp:9457`), **pas par le consensus**. Un wallet
  tiers qui réutiliserait une sous-adresse BQ perdrait cette barrière et ne garderait que (d).
* **Confidentialité.** La posture hybride protège contre le vol, pas contre une fuite ML-KEM
  (ligne 3 de la matrice).

## 7. Limites connues, non couvertes par ce document

### 7.1 Temps constant sur ARM64 : **aarch64 non vérifié, pour ML-KEM-768 ET ML-DSA-65**

C'est la limite la plus importante du dossier, et elle n'est **pas** résolue par ce document.

* HIDERING compile liboqs avec `OQS_DIST_BUILD=ON` : toutes les variantes (ref, x86_64,
  aarch64) sont compilées et **le CPU choisit à l'exécution**. Les binaires ARM64, dont les
  binaires macOS Apple Silicon publiés, exécutent donc les **variantes aarch64**.
* Les datasheets de liboqs 0.16.0 déclarent, pour ces variantes aarch64,
  `no-secret-dependent-branching-claimed: true` mais
  **`no-secret-dependent-branching-checked-by-valgrind: false`**, pour **ML-KEM-768**
  (`docs/algorithms/kem/ml_kem.yml`) **et** pour **ML-DSA-65**
  (`docs/algorithms/sig/ml_dsa.yml`). Seules les variantes ref et x86_64 sont vérifiées par
  le test Valgrind d'OQS, lequel ne tourne que sur Linux x86_64 (`SECURITY.md:20`).
* **Aggravation par l'upgrade 0.16.0 pour ML-DSA-65 :** en 0.15.0, ML-DSA-65 n'avait **pas**
  de variante aarch64 (seulement ref et AVX2) ; sur ARM64, c'était la variante ref, vérifiée,
  qui tournait. Le passage au backend `mldsa-native` en 0.16.0 a introduit une variante
  aarch64 native **non vérifiée**, désormais sélectionnée sur ARM64. Pour ML-KEM-768, la
  variante aarch64 n'était déjà pas vérifiée en 0.15.0.
* **Correction d'un document antérieur :** `M-11_liboqs_maturity_2026-09-08.md` §3
  (l.66-69) affirme que ref, x86_64 **et aarch64** sont `checked-by-valgrind: true`. C'est
  **faux** pour aarch64, pour les deux algorithmes, en 0.16.0 ; pour ML-KEM-768, c'était déjà
  faux en 0.15.0.

**Traitement :** livrable B (preuve de temps constant), qui devra soit exécuter le test
Valgrind sur un Linux aarch64, soit forcer la variante ref sur ARM64, soit documenter ce
point comme risque accepté à part — **décision non prise à ce jour.**

### 7.2 Autres limites

* Le test temps constant d'OQS ne détecte pas les instructions à temps variable (type `DIV`)
  et exclut canaux physiques, fautes et failles matérielles (`SECURITY.md:20`).
* Le hook `randombytes` SHAKE256 de HIDERING (dérivation déterministe de la clé ML-DSA, M-4)
  est du code HIDERING, hors de tout test liboqs : à couvrir par le livrable B.
* Vecteurs NIST : liboqs ne les embarque pas ; son script les télécharge sans contrôle
  d'intégrité. Livrable C.

## 8. Livrables de la clôture

| Livrable | Objet | État |
|---|---|---|
| **A** | Posture hybride (ce document) | ✅ rédigé |
| **B** | Preuve de temps constant : ML-KEM-768 et ML-DSA-65, variantes réellement exécutées (dont aarch64), hook RNG HIDERING | ⏳ à venir |
| **C** | Vecteurs NIST ACVP versionnés avec empreinte, exécutés sur nos deux algorithmes | ⏳ à venir |

**HFv16 reste bloqué jusqu'à la livraison de B et C.**

## 9. Mention publique à reprendre (whitepaper, site)

> « La cryptographie post-quantique de HIDERING (ML-KEM-768, ML-DSA-65 via liboqs) a fait
> l'objet d'une **revue assistée par IA et d'une acceptation de risque documentée ; elle
> n'a pas été auditée par un tiers.** »
