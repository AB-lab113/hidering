# Gate d'upgrade liboqs — le vecteur de non-régression est posé, l'upgrade reste EN ATTENTE

**Date :** 8 septembre 2026
**Statut :** ✅ vecteur figé et vert sur 0.15.0 — ⛔ **upgrade 0.16.0 NON FAIT, et interdit tant que ce vecteur n'a pas été rejoué en vert sur 0.16.0.**

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
