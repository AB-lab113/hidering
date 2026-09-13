# Gate d'upgrade liboqs — le vecteur a été rejoué sur 0.16.0, **et il passe**

**Date d'origine :** 8 septembre 2026 — **réévaluation : 13 septembre 2026 (§7 à §10).**
**Statut :** ✅ vecteur figé vert sur 0.15.0 **ET sur 0.16.0** (mesuré, §8) → **l'obstacle
TECHNIQUE à l'upgrade est levé.** ⛔ **Le blocage M-11 (maturité), lui, reste ENTIER** : il est
indépendant de ce gate et ses trois conditions restent non remplies (§9).
**Rien n'a été mergé.** Le submodule est resté épinglé sur 0.15.0 ; la décision d'upgrader
revient au mainteneur (§10).

> §1 à §6 sont le document d'origine du 8 septembre, conservé tel quel — il énonçait une
> crainte, et la suite dit ce que la mesure en a fait.

## 1. Le risque, en une phrase

liboqs n'expose **aucun keygen dérandomisé pour ML-DSA-65** (vérifié jusqu'à 0.16.0 : la
struct `OQS_SIG` ne porte que `keypair`). `pqc_keygen_from_seed` le contourne en branchant un
flux SHAKE256 sur le hook `randombytes` de liboqs. La clé dérivée est donc fonction non
seulement de notre seed, **mais aussi du nombre d'octets d'aléa que l'implémentation ML-DSA
lit, et dans quel ordre**.

liboqs **0.16.0 remplace le backend ML-DSA par `mldsa-native`**. Si ce backend consomme
l'aléa différemment, **la même seed de 25 mots dériverait une adresse BQ différente** après
l'upgrade. C'est très exactement M-4 — le bug où un restore depuis la seed ne retrouvait plus
les fonds — réintroduit par une simple bump de dépendance. Silencieusement.

## 2. Pourquoi le test de déterminisme existant ne couvre PAS ce risque

`pq_keygen_test::test_bq_keygen_is_deterministic` vérifie que **deux dérivations à
l'intérieur d'un même build** coïncident. C'est vrai — et ça le reste — même si toute la
dérivation se décale d'une version à l'autre. Seul un **vecteur figé** détecte un changement
inter-version. C'est ce qui manquait.

## 3. Ce qui a été figé

`src/crypto/pq_vector_test.cpp`, généré sur la version épinglée
(`external/liboqs` @ `97f6b86b1b6d109cfd43cf276ae39c2e776aed80` = tag **0.15.0**) :

| Vecteur | Ce qu'il verrouille |
|---|---|
| **Seed brut → clés** (`seed[i] = 0xA0 + i`) | `pqc_keygen_from_seed` lui-même — isole liboqs de la dérivation de clés Monero, donc un échec est **imputable à liboqs** |
| **Compte → adresse BQ** (clé de restauration de test, publique) | l'artefact visible par l'utilisateur : l'adresse `BQ...` complète (1725 caractères, empreinte Keccak-256 de toute la chaîne) |
| **Clés ML-KEM-768 et ML-DSA-65** du compte | empreintes Keccak-256 des pk **et** sk, + les 16 premiers octets des pk en clair |
| **Clé ML-DSA-65 par sortie** (`ss = 0x5A^i`, index 7) | le chemin chaud d'une dépense BQ : s'il bouge, **les sorties BQ déjà créées deviennent indépensables** |
| **Bind tag** | le check *c* du validateur, calculé sur cette clé par sortie |

Le matériel de seed du test est une **constante publiée choisie pour ce test**. Ce n'est pas,
et ne doit jamais devenir, une seed de wallet réelle.

État actuel : **PASS** sur 0.15.0.

## 4. Procédure d'upgrade — l'ordre compte

1. **NE PAS** bumper le submodule d'abord. Vérifier que `pq_vector_test` est vert sur 0.15.0
   (il doit l'être ; sinon quelque chose a déjà bougé et il faut comprendre quoi avant tout).
2. Bumper `external/liboqs` sur 0.16.0 dans une branche jetable, rebuild liboqs, rebuild
   `obj_cncrypto`.
3. Lancer `pq_vector_test`. **C'est le point de décision :**
   * **Vert** → la dérivation est inchangée, l'upgrade est sûre de ce point de vue.
     Poursuivre avec le reste de la suite (10 tests) puis le e2e regtest.
   * **Rouge** → **l'upgrade n'est PAS prenable telle quelle.** Elle abandonnerait tous les
     wallets BQ existants. Il faudrait alors soit renoncer à l'upgrade, soit livrer un plan de
     migration explicite (dérivation versionnée : les anciens comptes continuent de dériver à
     l'ancienne, les nouveaux à la nouvelle), ce qui est un chantier à part entière — pas un
     bump de version.
4. Ne jamais « corriger » un échec en réécrivant les valeurs attendues du vecteur. Un
   désaccord **est** le signal que ce fichier existe pour produire.

## 5. La vraie sortie de crise, à terme

Tant qu'on pilote un hook RNG, **toute bump de liboqs restera un risque de rupture
d'adresse**. La façon de supprimer définitivement ce risque est d'arrêter de dépendre du
comportement interne de liboqs : implémenter explicitement la dérivation FIPS 204
`ξ → (pk, sk)` (ξ est un seed de 32 octets dont la clé est une fonction déterministe
**spécifiée par le standard**), ou attendre que liboqs expose un `keypair_derand` pour SIG.
Le vecteur ci-dessus est un garde-fou, pas une solution.

## 6. Ce que ce document ne change pas

Le blocage **M-11** reste entier et indépendant de tout ceci : liboqs porte toujours son
avertissement « ne pas utiliser en production », n'a aucune validation FIPS 140-3, et notre
0.15.0 est désormais **EOL upstream** (OQS ne supporte que la dernière release). Voir
`docs/audit/M-11_liboqs_maturity_2026-09-08.md`. Le vecteur lève un obstacle *technique* à
l'upgrade ; il ne lève pas la condition de maturité qui bloque l'activation de HFv16.


---

# Réévaluation du 13 septembre 2026

## 7. Ce qui a changé en amont depuis le 8 septembre : **rien**

| Vérification | Résultat au 13 sept. 2026 |
|---|---|
| Dernière release liboqs | **0.16.0**, publiée le 9 juillet 2026 — **inchangée**, aucune 0.16.1 ni 0.17.0 (API GitHub releases) |
| Disclaimer « ne pas utiliser en production » | **intact, mot pour mot**, sur `main` (README §Limitations) |
| Commits sur `src/common/rand` depuis la 0.16.0 | **aucun** |
| Keygen dérandomisé pour SIG | **toujours absent** — `OQS_SIG` de la 0.16.0 n'expose ni `keypair_derand` ni `length_keypair_seed` (vérifié dans l'arbre 0.16.0 checkouté) |

Autrement dit : la version candidate est la même qu'au 8 septembre, et le contournement par le
hook `randombytes` reste nécessaire après upgrade comme avant.

**Une correction au dossier M-11.** `M-11_liboqs_maturity_2026-09-08.md` affirme « aucun audit
tiers publié ». C'est **faux** : **Trail of Bits a revu liboqs en 2024**, rapport publié en avril
2025 (`trailofbits/publications`, `reviews/2025-04-quantum-open-safe-liboqs-securityreview.pdf`),
et la page sécurité d'OQS le mentionne explicitement. Deux réserves qui expliquent pourquoi ça ne
lève pas la condition pour autant : la revue porte sur **des portions** de liboqs (« a review of
portions of liboqs »), et **OQS lui-même continue d'écrire, après cette revue**, que la
bibliothèque « has not received the level of auditing and analysis that would be necessary to
rely on it for high security use » et ne doit pas être considérée « production quality ». Un
audit partiel existe donc ; l'éditeur ne le tient pas pour suffisant. La condition (1) de M-11
reste non remplie, mais pour la bonne raison — et le dossier doit cesser de dire qu'il n'y a
aucun audit.

## 8. Le test empirique — procédure du §4 suivie à la lettre

| Étape | Fait | Résultat |
|---|---|---|
| 1. Vert sur 0.15.0 d'abord | `pq_vector_test` sur le pin `97f6b86` | **PASS** (4 groupes) |
| 2. Branche jetable | `liboqs-0.16.0-gate-test`, submodule → `5a1a854b0dc9f2141bdc771c555ee60c37950183` (tag `0.16.0`) | — |
| 3. Rebuild liboqs + `cncrypto` | `OQS_VERSION_TEXT "0.16.0"` confirmé dans `oqsconfig.h` | build propre |
| 4. **Rejouer le vecteur** | `pq_vector_test` | ✅ **PASS — les 4 groupes, aucune valeur ne bouge** |
| 5. Reste de la suite | les 15 tests standalone | ✅ **15/15 verts** (sur x86_64 ; voir §8.1 pour ARM) |
| 6. Restauration | submodule remis sur `97f6b86` (0.15.0), liboqs rebuildé | arbre rendu à l'état d'origine |

Détail du vecteur, puisque c'est le point de décision :

```
PASS: raw-seed vector — pqc_keygen_from_seed is unchanged (ML-KEM derand + ML-DSA over the SHAKE256 hook)
PASS: account vector — the two seeds still derive the same BQ address and the same ML-KEM/ML-DSA keys
PASS: spec 2e vector — per-subaddress identity key, commitment, blind and v2 binding tag unchanged
PASS: output vector — per-output ML-DSA-65 key and binding tag unchanged
```

La garde est **plus complète** qu'au 8 septembre : spec 2e y a ajouté la clé ML-DSA **par
subaddress** (un second chemin dérandomisé pilotant le même hook), `auth_commit`, `auth_blind` et
le `bind_tag` v2. Le test couvre donc aujourd'hui les deux chemins de dérivation ML-DSA, pas un
seul.

### Pourquoi ça passe — l'explication, vérifiée dans le code des deux versions

Ce n'est pas de la chance, et c'est ce qui rend le résultat solide :

| | 0.15.0 (`pqcrystals-dilithium-standard`) | 0.16.0 (`mldsa-native`) |
|---|---|---|
| Appel d'aléa dans `keypair` | `randombytes(seedbuf, SEEDBYTES)` — **1 appel, 32 octets** | `mld_randombytes(seed, MLDSA_SEEDBYTES)` — **1 appel, 32 octets** |
| Ensuite | `shake256` puis expansion, conforme FIPS 204 | `mld_sign_keypair_internal(pk, sk, seed, …)`, conforme FIPS 204 |

Les deux backends consomment **exactement la même quantité d'aléa, en un seul appel**, puis
appliquent la dérivation `ξ → (pk, sk)` **spécifiée par FIPS 204**, qui est déterministe. Notre
flux SHAKE256 rend donc le même ξ, et le standard fait le reste. **La crainte du §1 était fondée
en principe et se trouve infirmée en fait** : `mldsa-native` a changé l'implémentation, pas le
motif de consommation d'aléa.

Corollaire utile, vérifié au passage : la 0.16.0 dispatche `keypair` selon le CPU
(`ref` / `x86_64` / `aarch64`), mais les trois variantes lisent le même unique bloc de 32 octets
avant d'entrer dans la dérivation normalisée. **La clé dérivée ne dépend donc pas du CPU** — un
risque qu'on n'avait pas formulé et qui aurait été autrement plus vicieux qu'un changement de
version, puisqu'il aurait fait dépendre l'adresse BQ de la machine.

### 8.1 Le backend **aarch64** — vérifié par lecture de source, plus par analogie (13 septembre)

Le §8 ci-dessus avait lu `ref` et `x86_64` et **supposé** `aarch64` cohérent. Ce n'est pas une
hypothèse qu'on peut se permettre ici : nos livrables publics incluent des binaires **macOS
ARM64** (daemon `v2.0.3`, GUI `v2.0.2-gui`), donc une clé BQ créée là-bas doit dériver
bit-identique à celle créée ailleurs, sous peine d'adresses BQ dépendantes de la machine. Le
backend a donc été lu.

**Résultat : identique, et pour une raison plus forte que « même comportement » — c'est le même
fichier.** Lecture faite sur l'arbre `0.16.0` via `git show`, sans toucher au pin (le submodule
est resté sur `97f6b86` = 0.15.0).

| Vérification | `ref` | `x86_64` | `aarch64` |
|---|---|---|---|
| `sha256(mldsa/src/sign.c)` | `a18fd65d…8c25` | `a18fd65d…8c25` | `a18fd65d…8c25` |
| `sha256(mldsa/src/params.h)` | `9752e4fb…390f` | `9752e4fb…390f` | `9752e4fb…390f` |
| `MLDSA_SEEDBYTES` / `MLDSA_RNDBYTES` | 32 / 32 | 32 / 32 | 32 / 32 |
| Appels `mld_randombytes` dans `sign.c` | l.414, l.982, l.1033 | idem | idem |
| dont **dans `mld_sign_keypair`** | **1 seul**, `(seed, MLDSA_SEEDBYTES)` l.414 | idem | idem |
| `mld_randombytes` → | `OQS_randombytes(ptr, len)` | idem | idem |
| Occurrences de `randombytes` dans tout l'arbre du backend | 13 | 13 | 13 |
| Fichiers communs qui **diffèrent** | — | — | **aucun** |

Points saillants :

* **Aucun fichier commun ne diffère entre `aarch64` et `ref`.** Le backend aarch64 **est** le
  backend ref, *plus* un répertoire `native/aarch64/` qui n'ajoute que des noyaux arithmétiques
  en assembleur (NTT/iNTT, `pointwise_montgomery`, `poly_caddq`, `poly_chknorm`,
  `poly_decompose`, tables de zetas). **Rien de ce qui touche à l'aléa, au seed ou au flux de
  contrôle n'est spécifique à l'architecture.**
* Les deux autres appels (l.982, l.1033) sont dans le chemin de **signature** — le `rnd` de la
  variante *hedged* de ML-DSA — pas dans la génération de clés. Sans effet sur la dérivation :
  `pq_vector_test` documente déjà que la signature est randomisée et donc non épinglable.
* `PQCP_MLDSA_NATIVE_MLDSA65_{C,X86_64,AARCH64}_keypair` sont trois instances **namespacées de la
  même fonction** `mld_sign_keypair` du `sign.c` identique (`MLD_CONFIG_NAMESPACE_PREFIX`), pas
  trois implémentations.

➡️ **Le risque « une clé BQ créée sur macOS ARM64 dérive autrement » est écarté au niveau de la
consommation d'aléa.** Les trois backends que nous compilons sont désormais vérifiés par lecture,
zéro sur trois par déduction.

**Ce que cette lecture n'établit pas, et qu'il faut dire.** Elle prouve que le **seed** est
identique (même appel, même taille, même source). Elle ne prouve pas que les noyaux arithmétiques
NEON calculent le même résultat que le C portable — ça, ça repose sur (a) le fait que c'est la
même fonction *spécifiée* par FIPS 204 et (b) les KAT/ACVP qu'OQS exécute par backend en CI. Et
empiriquement, le §8 n'a mesuré que x86_64 : **aucun test n'a tourné sur ARM sur cette machine**
(x86_64) et aucun ne le peut.

**Action concrète qui en découle, à faire avant d'activer BQ dans un binaire ARM64 publié :**
faire tourner `pq_vector_test` sur un runner ARM réel. La CI GUI construit déjà sur `macos-14`
(ARM), donc c'est un ajout de quelques lignes, pas un chantier — et c'est la seule chose qui
transformerait « spécifié identique » en « mesuré identique » sur l'architecture qui nous
intéresse. Tant que ce n'est pas fait, le risque est **écarté par lecture, pas par mesure**.

## 9. Ce que ce résultat ne lève PAS

`pq_vector_test` vert sur 0.16.0 lève **un obstacle technique** : l'upgrade ne casserait pas les
adresses BQ dérivées. Il ne touche à aucune des trois conditions de M-11 :

| # | Condition M-11 | État au 13 sept. 2026 |
|---|---|---|
| 1 | Disclaimer levé ? Audit tiers publié ? | ❌ Disclaimer **intact**. Audit tiers : il en existe **un**, partiel (Trail of Bits 2024/2025) — correction au §7 — mais OQS maintient après lui que le niveau d'audit reste insuffisant pour un usage à forte sécurité |
| 2 | Implémentation FIPS-validée (CMVP) ou posture hybride formalisée | ❌ Aucune validation FIPS 140-3 n'existe pour liboqs. L'hybride reste notre posture **de fait**, toujours pas formalisée comme exigence écrite |
| 3 | Version figée + re-audit constant-time des chemins réellement utilisés | ⚠️ Figée, mais **0.15.0 est EOL upstream** ; re-audit constant-time toujours non fait (on s'appuie sur les datasheets OQS) |

**Le blocage HFv16 posé par M-11 reste donc MAINTENU.** Conformément à la règle de ce dossier :
en cas de doute sur la maturité, le blocage est la position par défaut, et un test vert ne
s'échange pas contre une condition de maturité.

## 10. Décision demandée

Le gate technique étant franchi, **l'upgrade 0.15.0 → 0.16.0 devient prenable** — elle ne l'était
pas le 8 septembre. Ce qui reste à arbitrer :

1. **Upgrade-t-on ?** Pour : 0.15.0 est **EOL**, donc une future CVE sur ML-KEM-768 / ML-DSA-65 ne
   serait corrigée que sur la branche courante — c'est le seul argument qui grossit avec le temps.
   Contre : aucune correction de la 0.16.0 ne nous concerne aujourd'hui (XMSS, CROSS, FrodoKEM,
   `encaps_derand` — rien de ce que nous compilons ou appelons), donc l'upgrade n'apporte rien
   d'immédiat. **Recommandation : upgrader**, maintenant que la mesure a retiré le risque qui la
   bloquait, et pendant que la fenêtre est ouverte (HFv16 inactif, aucun wallet BQ en production).
2. **Si oui**, le commit doit porter : le bump du submodule, la mise à jour de l'en-tête de
   `pq_vector_test.cpp` (« Frozen against liboqs 0.15.0 » devient faux), et la note que les
   valeurs sont **identiques** sur les deux versions — c'est-à-dire que le vecteur vaut désormais
   pour les deux, ce qui est le meilleur état possible pour un tel fichier.
3. **Ne jamais** confondre ce point avec M-11 : l'upgrade ne rapproche pas de HFv16 d'un pouce.

**Aucune action n'est prise sans confirmation explicite.** Le submodule est resté sur 0.15.0 et la
branche de test `liboqs-0.16.0-gate-test` n'a pas été mergée.

## 11. La sortie de crise du §5 reste la bonne cible

Le résultat du §8 est une **bonne nouvelle contingente** : elle tient parce que deux backends
indépendants ont choisi de lire 32 octets une fois. Rien ne garantit que le troisième le fera.
Tant que la dérivation BQ pilote un hook RNG, chaque bump restera un pari à vérifier — ce que le
vecteur rend au moins détectable. La façon de supprimer le pari est inchangée : implémenter
nous-mêmes `ξ → (pk, sk)` de FIPS 204, ou attendre un `keypair_derand` pour SIG.
