# CRIT-4 — La racine post-quantique était la clé de dépense

**Date :** 12 septembre 2026
**Gravité : CRITIQUE — la sécurité post-quantique était nulle.** Portée : HFv16 uniquement,
jamais activé → **aucun impact sur la chaîne live (hf 15), aucun fonds réel exposé, aucune
action opérateur requise.**
**Statut : ✅ CORRIGÉ** — décision **R2a**, commit de ce jour. Voir §3.
**Hors périmètre, toujours ouvert : le résiduel quantique de CRIT-3** (§6) — un émetteur
capable d'exécuter Shor reste capable de redépenser une sortie BQ qu'il a payée. C'est un
chantier séparé (S1b), volontairement pris **après** celui-ci.
Ouvert le 11 septembre en implémentant les subaddresses BQ ; analysé dans
`design_2c_pq_root_shor_resistance.md`.

## 1. Le bug

Toute clé BQ d'un compte — la paire ML-KEM-768/ML-DSA-65 primaire (M-4) **et** chaque paire
ML-KEM de subaddress (décision 4) — dérivait de `get_pq_root_secret(keys)`, qui renvoyait
`keys.m_spend_secret_key`.

Or la clé publique correspondante `B = b·G` est publiée **dans l'adresse BQ elle-même** :

```
charge utile BQ = marker(1) ‖ B(32) ‖ A(32) ‖ kem_pk(1184)      (cryptonote_basic_impl.cpp:361-378)
```

Donc, pour un adversaire disposant d'un ordinateur quantique :

```
B  ──Shor──►  b  ──SHAKE256("HRG_PQ_KEM_v1" ‖ b)──►  kyber_sk
                  ──SHAKE256("HRG_PQ_DSA_v1" ‖ b)──►  dilithium_sk
                  ──SHAKE256("HRG_BQ_SUBADDR_KEM_v1" ‖ b ‖ i)──►  toute subaddress BQ
```

**Un seul logarithme discret sur une valeur publique livrait la totalité du matériel
post-quantique du compte, présent et futur.** La promesse « adresses résistantes au
quantique » était vide : le chiffrement en réseau euclidien protégeait des clés dont la
graine était, elle, gardée par de la cryptographie sur courbe elliptique.

## 2. Pourquoi ce n'était pas visible

* **Le commentaire in-code disait vrai, dans le mauvais sens.** `account.cpp` affirmait « the
  spend key flows through domain-separated SHAKE256, so it is never recoverable from the PQ
  material ». C'est exact — et sans rapport avec l'attaque : le hash protège `b` **depuis** les
  clés PQ, pas les clés PQ **depuis** `b`.
* **M-4 avait poussé dans cette direction.** Fermer M-4 (14 juin) exigeait que la restauration
  depuis la seed reproduise l'adresse BQ ; la clé de dépense était le seul secret déjà
  reconstruit au bon moment. Le correctif de M-4 était juste ; c'est son choix de racine qui
  ne l'était pas.
* **Le test de détermination ne pouvait pas le voir.** `pq_keygen_test` vérifiait « même seed →
  mêmes clés BQ ». Cette propriété est encore vraie après le correctif. Il manquait l'assertion
  inverse : *même clé de dépense, racine différente → clés BQ différentes* — qui ne pouvait
  littéralement pas s'écrire tant que la racine **était** la clé de dépense.

## 3. Le correctif — R2a, deux mnémoniques

`account_keys::pq_root` : 32 octets d'entropie **indépendante**, tirés du CSPRNG système à la
création, sauvegardés par leur **propre liste de 25 mots**. `get_pq_root_secret` renvoie
désormais ce champ (et `nullptr` s'il est absent — jamais de repli sur la clé de dépense).

Le rapport de design compare quatre options ; le choix s'est fait sur un fait vérifié dans le
code, pas sur une préférence :

> **La seed 25 mots n'est pas un ancêtre de la clé de dépense : elle EST la clé de dépense.**
> `generate_keys` sur le chemin `recover=true` fait `sec = recovery_key` puis `sc_reduce32`,
> sans aucun hachage (`crypto.cpp:159-166`), et `get_seed` réencode littéralement
> `m_spend_secret_key` en mots (`wallet2.cpp:1450-1453`).

C'est ce qui **réfute R0**, l'idée intuitive de « redériver la racine depuis la seed sous un
domaine de hash séparé » : parallèle ou à travers, les deux branches partent du même nœud, et
ce nœud est exactement ce que Shor donne. Un hash ne protège rien quand l'adversaire détient
déjà son entrée. **Fermer CRIT-4 exigeait de l'entropie qui n'existait pas dans le wallet** —
d'où une seconde seed.

R1 (racine aléatoire stockée seulement dans le `.keys`) a été écartée pour la raison inverse :
elle recrée M-4, la seed ne suffisant plus à retrouver ses fonds. R3 (redéfinir les 25 mots en
*master entropy*) est plus propre sur le papier mais rend deux seeds de 25 mots indiscernables
alors qu'elles désignent des wallets différents.

### Ce que ça donne concrètement

| | Avant | Après |
|---|---|---|
| Racine BQ | `m_spend_secret_key` | `pq_root`, 32 o d'entropie indépendante |
| Sauvegarde | 25 mots | 25 mots **+ 25 mots BQ** |
| Adresse B... pour une même seed classique | — | **inchangée** (dérivation Ed25519 non touchée) |
| Shor sur `B` donne les clés BQ | **oui** | non |

* **Création** (`--bq-wallet`) : `wallet2::generate` tire la racine et `simplewallet` affiche la
  seconde seed juste après la première, avec la consigne qu'il faut les deux.
* **Restauration** (`--bq-wallet --restore-deterministic-wallet`) : la seed BQ est demandée sur
  le même écran que les 25 mots (`--bq-seed=` en non-interactif). **Vide = refus.** Choix
  fail-closed délibéré : tirer une racine neuve produirait une adresse BQ parfaitement valide
  qui n'est simplement pas celle qui détient les fonds — M-4, en silencieux. `wallet2::generate`
  applique la règle elle-même (`recover && use_pq && pq_root == nullptr` → exception), pas
  seulement la CLI.
* **Chargement** : un `.keys` portant `pq_keys` **sans** `pq_root` est un wallet BQ d'avant ce
  correctif ; il est **refusé bruyamment** au load. Ni repli sur la clé de dépense (le trou
  resterait ouvert), ni racine neuve (adresse différente).
* **At-rest** : `pq_root` est chiffré comme les autres secrets, **ajouté en fin** du flux
  chacha20 (`account.cpp:87-125`) — le flux est positionnel, tout ajout ailleurs casserait les
  fichiers BQ existants. Champ KV nommé et optionnel, comme `pq_keys`/`pq_dilithium` : **un
  wallet classique n'émet rien et son `key_data` est inchangé**. `BOOST_CLASS_VERSION` de
  `account_keys` 1 → 2.
* **Pas d'offset de seed sur la seconde mnémonique.** L'offset est une addition scalaire mod l
  (`cryptonote_format_utils.cpp:1762`) et `pq_root` est de l'entropie brute jamais réduite :
  l'appliquer ne ferait pas d'aller-retour. Laissé ouvert plutôt que bricolé (§7).

## 4. Migration : aucune

`HF_HEIGHT_PQ = 2000000` (`cryptonote_config.h:227`), chaîne live en hf 15, tip de l'ordre de
8×10⁴ → **aucune sortie BQ n'a jamais pu exister sur le mainnet**. Les seuls wallets BQ sont
locaux (regtest, tests). Précédent identique : l'Étape 9 (migration FIPS, 9 juin) avait déjà
invalidé les wallets BQ expérimentaux sans chemin de migration. Il n'y avait donc **rien à
migrer** — seulement une fenêtre à ne pas manquer, qui se referme à l'activation de HFv16.

## 5. Preuve (tests)

**Nouveau : `src/crypto/pq_root_restore_test.cpp`** — code réel des deux côtés
(`wallet2::generate` crée et restaure, `construct_tx_and_get_tx_key` construit le paiement,
`wallet2::process_new_transaction` le scanne) :

* un wallet BQ émet **deux** listes de 25 mots, distinctes ; restaurer avec les deux reproduit
  l'adresse B..., l'adresse BQ, les clés ML-KEM/ML-DSA **et** les subaddresses BQ ;
* **la propriété CRIT-4 elle-même** : même seed classique + seed BQ différente → **même** adresse
  B..., **autre** adresse BQ. Avant le correctif, cette assertion était inexprimable ;
* restaurer un wallet BQ avec la seule seed classique est **refusé** ;
* un vrai paiement vers l'adresse BQ primaire et une subaddress BQ est trouvé par le wallet
  **restauré** (qui n'a jamais vu le fichier de clés d'origine), sur les bonnes subaddresses,
  et le secret par sortie qu'il récupère **reproduit le bind tag on-chain** — donc il peut
  réellement dépenser. Le wallet restauré avec une **mauvaise** seed BQ ne trouve rien ;
* la racine survit à `store()` / `load()` ;
* **(6)** avec les clés chiffrées en mémoire (le mode par défaut), `get_pq_seed` **refuse** au
  lieu de rendre des mots issus de la racine brouillée ; déverrouillé, les mots qu'il rend
  restaurent bien la même adresse BQ.

**Smoke test CLI** (hors dépôt, pty) sur le binaire réel : `--bq-wallet` imprime bien **deux**
listes de 25 mots distinctes avec la mention de la seconde ; `--electrum-seed` + `--bq-seed`
reproduit l'adresse BQ ; sans `--bq-seed` la restauration est refusée **et aucun fichier n'est
écrit** ; avec une mauvaise seed BQ on obtient la **même** adresse B... et une **autre** adresse
BQ ; la commande `seed` affiche les deux seeds.

**Un défaut trouvé en pilotant la vraie CLI, et corrigé.** Le premier jet affichait la
seconde seed **depuis la racine chiffrée en mémoire** : en mode par défaut
(`AskPasswordToDecrypt`), `setup_keys` chiffre les secrets avant que `new_wallet` n'imprime
quoi que ce soit. L'utilisateur recevait donc **25 mots parfaitement valides qui restaurent un
AUTRE wallet** — la pire défaillance possible pour une sauvegarde, et exactement le mode
d'échec silencieux que ce chantier existe pour supprimer. C'est la même famille que le défaut
ÉLEVÉ de `design_2b §8.5` (détection BQ avec un `kyber_sk` brouillé). Correctif : `get_pq_seed`
**refuse** quand les clés sont verrouillées (même garde `pq_secrets_usable()` que
`get_pq_subaddress_as_str`), et `simplewallet` déverrouille explicitement à la création — le
chemin de la commande `seed` était déjà sous `SCOPED_WALLET_UNLOCK`. **Le cas est désormais
pinné par un test** (§5, point 6) : un test unitaire seul ne l'aurait pas vu, parce que les
wallets de test tournent en mode « unattended », où les clés ne sont pas chiffrées en mémoire.

**`pq_keygen_test`** : le test de détermination est réécrit pour pinner les deux propriétés à la
fois — même racine → mêmes clés (restauration), et **même clé de dépense + racine différente →
clés différentes** (CRIT-4). **`pq_persistence_test`** : la racine est écrite chiffrée, absente
en clair du blob, restaurée au load, et redérive les mêmes subaddresses ; un wallet classique
n'émet ni `pq_keys` ni `pq_root`.

### `pq_vector_test` — une régénération délibérée, et une seule

Le vecteur figé pinne la dérivation BQ contre une dérive de liboqs. Changer la racine change
nécessairement les valeurs **au niveau compte** — mais rien d'autre. Le protocole annoncé dans
le rapport de design a été appliqué à la lettre :

* **régénérés** : adresse BQ (Keccak de la chaîne complète) et les 4 empreintes de clés de
  compte, désormais fonction d'une nouvelle constante `V_PQ_ROOT`, **choisie différente de
  `V_RECOVERY_KEY`** — avec les deux égales, ce test aurait continué à passer contre le code
  vulnérable, et n'aurait donc rien pinné ;
* **inchangés, et re-mesurés pour le prouver** : `V_RAW_*` (entrée brute de liboqs),
  `V_OUT7_MLDSA_PK_KECCAK` (clé ML-DSA par sortie, dérivée du secret partagé) et `V_BIND_TAG`.
  Les trois sont revenus **identiques au bit près**. C'est ce qui distingue « nous avons changé
  l'entrée » de « la dérivation a bougé sous nos pieds », et c'est tout l'intérêt du fichier.

La règle de l'en-tête tient : on ne réécrit jamais une valeur attendue pour faire passer un test.
Ici la régénération est adossée à une décision écrite, et le fichier dit lesquelles et pourquoi.

## 6. Ce que ça ne corrige pas

La matrice de CRIT-3 §5 bouge **d'une ligne**, celle du tiers quantique — qui obtenait `ss` via
la racine cassée :

| Attaquant | A `ss` ? | A `x'` ? | Avant | Après |
|---|---|---|---|---|
| tiers classique | non | non | non | non |
| émetteur classique | oui | non | non (d2) | non (d2) |
| **tiers quantique** | oui (racine = `b`) | oui (Shor) | **oui — vol** | **non** ✅ |
| **émetteur quantique** | oui (encapsulation) | oui (Shor) | oui | **oui — résiduel** |

`x' = H_s(d‖i) + b (+m) + t` ne dépend pas de la racine, et `P' = x'·G` est révélée par la forme
même d'une dépense transparente : Shor la renverse quoi qu'il arrive. Fermer le dernier cas
demande qu'une **clé post-quantique détenue par le seul destinataire** entre dans l'autorisation
— l'option **S1b** du rapport (engagement de 32 o dans l'adresse, clé ML-DSA d'adresse révélée à
la dépense **en remplacement** de la clé par sortie, coût en taille nul, prix payé en liabilité
entre dépenses d'une même subaddress). **L'ordre compte et il est celui-ci** : S1b dérive sa clé
de la racine ; appliquée avant ce correctif, elle aurait protégé une racine elle-même cassable.

## 7. Reste ouvert

* **S1b / CRIT-3 résiduel**, à livrer avec la finalisation du binding **C-1** — les deux
  touchent le format d'adresse BQ, donc ils doivent tenir dans un même changement, avant HFv16.
* **Offset de seed sur la mnémonique BQ** : non implémenté (§3). Un aveuglement par XOR sur
  `H_cn(passphrase)` serait la voie évidente, mais le rapport de design ne la spécifie pas et
  ce n'est pas le moment de l'inventer.
* **Attacher une racine BQ neuve à une seed classique existante** : le flux n'existe pas
  aujourd'hui (une restauration BQ exige les deux seeds). Si on le veut, il lui faut son propre
  drapeau explicite — jamais une seed BQ vide traitée comme « fais-m'en une ».
* **Clé de scan BQ view-only** : maintenant que la racine ne confère plus le pouvoir de dépense
  (celui-ci exige `b`, depuis le check d2), elle pourrait être exportée avec la clé de vue. Ça
  fermerait le premier item ouvert de `design_2b §10`. À spécifier séparément.
* **Wallets matériels** : un périphérique ne livre pas sa clé de dépense, et ne connaît pas
  encore de racine PQ. BQ + matériel reste non supporté, désormais de façon explicite.

## 8. Leçon pour la suite

**Une dérivation n'est pas plus forte que son entrée.** Tout le raisonnement « SHAKE256 avec
séparation de domaine, donc irréversible » était correct et sans effet : on l'appliquait à un
secret que l'adversaire obtient par ailleurs. Après CRIT-2 (autorisation ≠ valeur) et CRIT-3
(« dérivé d'un secret partagé » ≠ « détenu par le destinataire »), c'est la même famille
d'erreur : une propriété attribuée à une donnée qu'elle n'a pas, parce que la construction
autour d'elle *a l'air* solide. Question à reposer sur chaque secret avant HFv16 : **d'où vient
son entropie, et qui d'autre peut l'atteindre ?**
