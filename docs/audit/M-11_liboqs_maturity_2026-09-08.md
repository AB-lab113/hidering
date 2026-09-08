# M-11 — Réévaluation de la maturité de liboqs (condition bloquante HFv16)

**Date de la revue :** 8 septembre 2026
**Statut :** ⛔ **BLOCAGE MAINTENU** — la condition de levée n'est toujours pas remplie.
**Décision demandée :** aucune action d'upgrade n'a été prise. Ce document rapporte l'état
et les options ; l'arbitrage 0.15.0 → 0.16.0 revient au mainteneur (voir §5).

Contexte : le finding M-11 (audit du 10 juin 2026, série MOYEN) a accepté le risque
« liboqs se décrit comme bibliothèque de prototypage » **sous la condition expresse**
d'une réévaluation avant toute activation de HFv16. Ce document est cette réévaluation.

---

## 1. Version épinglée aujourd'hui

| Élément | Valeur |
|---|---|
| Submodule | `external/liboqs` |
| SHA épinglé | `97f6b86b1b6d109cfd43cf276ae39c2e776aed80` |
| Tag correspondant | `0.15.0` (vérifié `git tag --points-at HEAD`) |
| Algorithmes utilisés | ML-KEM-768 (FIPS 203), ML-DSA-65 (FIPS 204) |
| Build | statique, `OQS_USE_OPENSSL=ON`, `OQS_BUILD_ONLY_LIB=ON` |

## 2. Le disclaimer « prototyping » est-il toujours là ?

**OUI — inchangé, mot pour mot, sur `main` au 8 septembre 2026.**

> « **WE DO NOT CURRENTLY RECOMMEND RELYING ON THIS LIBRARY IN A PRODUCTION ENVIRONMENT
> OR TO PROTECT ANY SENSITIVE DATA.** This library is meant to help with research and
> prototyping. While we make a best-effort approach to avoid security bugs, this library
> has not received the level of auditing and analysis that would be necessary to rely on
> it for high security use. »

Présent à l'identique dans `external/liboqs/README.md:99` (notre 0.15.0) **et** dans le
README de `main` upstream. Deux mentions complémentaires relevées upstream :

* « caution is advised when deploying quantum-safe algorithms as most of the algorithms
  and software have not been subject to the same degree of scrutiny as for currently
  deployed algorithms » ;
* modèle de support : projet communautaire **non commercialement supporté**, best-effort,
  sans engagement de fiabilité.

➡️ **Condition de levée (1) du finding M-11 — « le disclaimer a-t-il sauté ? » — NON REMPLIE.**

## 3. Statut FIPS

* **Les algorithmes** sont bien standardisés : ML-KEM = FIPS 203 final, ML-DSA = FIPS 204
  final. C'est déjà ce que nous appelons depuis l'Étape 9 (migration du 9 juin 2026) ;
  aucun appel à une variante Round-3 / *-ipd ne subsiste.
* **La bibliothèque n'a AUCUNE validation FIPS 140-3.** OQS ne revendique pas de module
  CMVP et n'en vise pas. Pour un déploiement soumis à une exigence FIPS 140-3, liboqs est
  la *référence d'intégration*, pas le *module validé*.

➡️ **Aucun statut « FIPS-validated » n'existe pour liboqs.** La condition (2) du finding
(« envisager une implémentation FIPS-validée ou hybride ») ne peut donc pas être satisfaite
en restant sur liboqs seul.

**Nuance qui reste valable :** le disclaimer vise l'*implémentation*, pas les *algorithmes*.
Pour nos deux primitives, les datasheets de la version épinglée annoncent, sur les
implémentations que nous compilons (ref / x86_64 / aarch64) :
`no-secret-dependent-branching-claimed: true` **et** `checked-by-valgrind: true`.
Les seules entrées à `false` sont l'implémentation CUDA (`cupqc`), que nous ne compilons pas.
C'est le point positif du dossier : nos chemins chauds sont revendiqués constant-time et
testés comme tels, dans les limites du threat model OQS (§4).

## 4. Threat model OQS — ce qui reste hors couverture

`SECURITY.md` de la version épinglée : les tests constant-time ne couvrent que Linux/x86_64,
pour *certains* algorithmes, et « do not encompass all classes of non–constant-time
behaviour » (les instructions à temps variable type `DIV` sont explicitement citées comme
non détectées). Hors périmètre déclaré : side-channels sur système physique partagé, failles
CPU/matériel, injection de fautes, canaux d'observation physique (consommation, EM).

## 5. Ce qui a changé upstream depuis notre pin — **0.16.0**

liboqs **0.16.0 est sorti le 9 juillet 2026**. Faits marquants :

* **Notre 0.15.0 n'est plus une version supportée.** La politique OQS est « we only support
  the most recent release ». C'est un **changement d'état réel** depuis juin : à l'époque,
  0.15.0 *était* la version supportée. Aujourd'hui nous sommes sur une version EOL.
* Corrections de sécurité en 0.16.0 : déréférencement de pointeur non initialisé
  `encaps_derand` ; lecture hors bornes dans la vérification XMSS/XMSS^MT ; underflow entier
  dans `crypto_sign_open()` de CROSS ; taille de tableau incorrecte dans `secure_clean` ;
  barrière d'optimisation `OQS_MEM_BLACK_BOX` pour le constant-time de FrodoKEM.
  **Aucune ne vise ML-KEM-768 ni ML-DSA-65** : XMSS, CROSS et FrodoKEM ne sont pas compilés
  chez nous, et nous n'appelons jamais `encaps_derand` (nous utilisons `keypair_derand`
  côté KEM). Impact direct sur HIDERING : **nul** en l'état.
* **Changement d'implémentation ML-DSA : le backend par défaut de `SIG_ml_dsa_*` devient
  `mldsa-native` (C90, optimisations x86_64/aarch64).** ⚠️ **C'est le point qui compte pour
  nous** — voir §6.
* Ajout des vecteurs de test Wycheproof et ACVP pour ML-DSA (bon signal de maturation).
* SPHINCS+ retiré ; FrodoKEM renommé/restructuré ; HQC mis à jour. Sans objet pour nous.

## 6. ⚠️ Risque spécifique d'un upgrade 0.15.0 → 0.16.0 : les adresses BQ dérivées

Depuis M-4 (commit `1473779b6`), la clé BQ est **dérivée déterministement de la spend key**,
et la moitié ML-DSA est produite en pilotant le hook `randombytes` de liboqs avec un flux
SHAKE256 (voir §7). **La reproductibilité de l'adresse BQ dépend donc du nombre et de
l'ordre des lectures d'aléa que fait l'implémentation ML-DSA de liboqs.**

Un changement de backend (`mldsa-native` en 0.16.0) peut modifier ce motif de consommation.
Si c'est le cas, **la même seed 25 mots produirait une adresse BQ différente avant et après
l'upgrade** — c'est-à-dire exactement la perte de fonds que M-4 a corrigée, réintroduite par
une bump de dépendance.

**Test de non-régression obligatoire avant tout upgrade** (à faire, pas encore fait) :
figer un vecteur de test seed → (pk ML-DSA, pk ML-KEM) avec la 0.15.0, puis vérifier bit à
bit qu'il est reproduit par la 0.16.0. Le test `pq_keygen_test::test_bq_keygen_is_deterministic`
ne le détecte PAS : il compare deux dérivations faites par la *même* build.

## 7. Point connexe confirmé pendant cette revue : pas de keygen dérandomisé ML-DSA

Vérifié sur `src/sig/sig.h` de la **0.16.0** : la struct `OQS_SIG` n'expose toujours que
`keypair(pk, sk)` — **ni `keypair_derand`, ni `length_keypair_seed`**. Le contournement
HIDERING (flux SHAKE256 branché sur le hook `randombytes`) reste donc nécessaire ; un
upgrade ne le supprimerait pas. C'est ce contournement qui a produit le finding MOYEN-4
(RNG global non isolé), corrigé séparément le 8 septembre 2026.

## 8. Conclusion et conditions de levée restantes

**Le blocage HFv16 posé par M-11 est MAINTENU.** Aucune des trois conditions n'est levée :

| # | Condition (finding M-11) | État au 8 sept. 2026 |
|---|---|---|
| 1 | Le disclaimer « prototyping » a-t-il sauté ? Audit tiers publié ? | ❌ Disclaimer intact ; aucun audit tiers publié |
| 2 | Implémentation FIPS-validée (CMVP) ou posture hybride | ⚠️ Aucune validation FIPS n'existe pour liboqs. **L'hybride est déjà notre posture de fait** (la dépense reste gardée par dlog Ed25519) mais n'est pas formalisée comme exigence |
| 3 | Figer la version + re-audit constant-time des chemins réellement utilisés | ⚠️ Version figée (SHA pin) mais **désormais EOL upstream** ; re-audit constant-time non fait (on s'appuie sur les datasheets OQS) |

### Recommandations (par ordre, aucune appliquée sans validation)

1. **Décider de l'upgrade 0.16.0.** Argument pour : 0.15.0 n'est plus supportée upstream,
   donc une future CVE ML-KEM/ML-DSA ne sera corrigée que sur la branche courante.
   Argument contre : aucune des corrections 0.16.0 ne nous concerne aujourd'hui, et
   l'upgrade porte le risque §6 (adresses BQ). **Recommandation : upgrader, mais seulement
   après avoir écrit le vecteur de non-régression seed → clés du §6.** C'est un
   prérequis, pas une option.
2. **Formaliser la posture hybride** comme exigence de conception écrite (§2 condition 2) :
   documenter que la sécurité de dépense ne repose jamais sur liboqs seul.
3. **Remplacer la dépendance à l'ordre des lectures d'aléa.** La façon propre de neutraliser
   §6 est de ne plus dépendre du comportement RNG interne de liboqs : implémenter la
   dérivation ML-DSA `ξ → (pk, sk)` explicitement selon FIPS 204 (ξ est un seed de 32 octets
   dont la clé est une fonction déterministe spécifiée), ou attendre que liboqs expose un
   `keypair_derand` pour SIG. Tant qu'on pilote un hook RNG, **toute bump de liboqs est un
   risque de rupture d'adresse.**
4. **Ne pas activer HFv16** tant que 1–3 ne sont pas tranchés, conformément au finding M-11
   initial. Ce point est inchangé.

### Ce que ce document NE fait pas

Aucun changement de code, aucun changement de pin, aucune modification du submodule.
Le SHA reste `97f6b86` (0.15.0). Le rapport attend validation.
