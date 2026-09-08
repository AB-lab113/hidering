# Design 2a — Balance hybride : mixer une entrée transparente PQ et des entrées ring

**Date :** 8 septembre 2026 — item 2a de la revue post-audit du 7 septembre.
**Statut : RAPPORT DE DESIGN. Aucun code écrit. En attente de validation.**

## 0. Le problème

Aujourd'hui une transaction ne peut pas mélanger les deux mondes. Deux garde-fous
symétriques le refusent :

* **Construction** — `cryptonote_tx_utils.cpp:299-310` : si au moins une source est `is_pq`,
  alors *toutes* doivent l'être, sinon `construct_tx` échoue.
* **Consensus** — `blockchain.cpp:3531-3541` (check *e*) : dès qu'un `txin_to_key_pq` est
  présent, tout input non-PQ fait rejeter la transaction.

Conséquence pratique : **un wallet ne peut jamais consolider ses fonds B... et BQ... dans une
même transaction**, ni payer un montant qui dépasse le solde d'un seul des deux mondes. Pour
un utilisateur en cours de migration post-quantique — c'est-à-dire tout le monde, pendant des
années après HFv16 — c'est très pénalisant.

## 1. Pourquoi c'était refusé (et ce qui a changé)

Le raisonnement A3 était : *un `txin_to_key_pq` révèle son montant et n'a pas d'engagement de
Pedersen, donc sa valeur ne peut pas être équilibrée par RingCT ; on exige donc une tx
totalement transparente.* D'où le design actuel : tx v2 `RCTTypeNull`, montants révélés,
conservation monétaire arithmétique (check *e*).

Ce raisonnement est trop conservateur. **Un montant révélé s'injecte parfaitement dans
l'équation de balance RingCT** — c'est même déjà ce qu'on fait pour les frais.

## 2. Le cœur du design : une entrée transparente est un « frais négatif »

Rappel de la balance RingCT simple (types CLSAG / BulletproofPlus) :

```
Σ pseudoOuts  ==  Σ outPk  +  fee·H
```

où `pseudoOuts[i] = a_i·H + x'_i·G` est le ré-engagement de l'entrée ring *i*, et le CLSAG *i*
prouve que `C_réel_i − pseudoOuts[i]` est un engagement à zéro dont le signeur connaît la clé.

Une entrée transparente PQ de montant `a` n'a pas d'engagement… mais **le vérificateur peut le
calculer lui-même** depuis le montant révélé, avec un masque nul :

```
C_pq = zeroCommit(a) = a·H + 0·G
```

Le masque *doit* être nul : il n'y a pas de `C_réel` contre lequel prouver une égalité, donc la
seule valeur vérifiable publiquement est celle qui se recalcule depuis des données publiques.
Ce n'est pas une contrainte gênante — c'est exactement le traitement que Monero applique déjà
aux sorties coinbase v2, et (depuis le fix CRIT-1 du 7 septembre) aux sorties BQ transparentes
en base.

L'équation hybride devient :

```
Σ pseudoOuts  +  Σ a_pq·H  ==  Σ outPk  +  fee·H
⟺ Σ pseudoOuts == Σ outPk + (fee − Σ a_pq)·H
```

**Le terme transparent des entrées PQ se comporte exactement comme un frais négatif.** Toute la
machinerie de balance existante s'applique, il n'y a qu'un scalaire public à ajuster.

## 3. Choix structurel : ne PAS créer de pseudoOut pour les entrées PQ

Deux façons de câbler ça, et le choix n'est pas neutre.

* **H1 — un `pseudoOuts[i]` par entrée, PQ comprises**, contraint à valoir `zeroCommit(amount)`.
  Ça garde `pseudoOuts.size() == vin.size()`, mais ça **désaligne CLSAGs et pseudoOuts** :
  la vérification Monero indexe `verRctCLSAGSimple(i)` sur `pseudoOuts[i]`, et une entrée PQ
  n'a pas de CLSAG (son autorisation, ce sont les checks *c*/*d* : signature ML-DSA-65 par
  sortie + bind tag). Il faudrait une table d'indirection dans du code de vérification
  consensus très sensible. **À éviter.**
* **H2 — `pseudoOuts` reste indexé sur les seules entrées ring** (donc aligné 1:1 avec les
  CLSAGs, aucun changement d'indexation), et les montants PQ entrent dans la balance comme le
  scalaire public du §2.

**Recommandation : H2.** Elle ne touche pas l'indexation CLSAG↔pseudoOut, elle isole tout le
delta hybride dans une seule équation, et elle laisse le code RingCT amont quasi intact.

Contrainte à documenter : `pseudoOuts.size() == nombre d'entrées ring`, alors que
`vin.size()` compte les deux. C'est le seul invariant Monero qu'on relâche, et il doit être
vérifié explicitement dans le validateur (sinon un attaquant joue sur le décompte).

## 4. Effet de bord : l'hybride est PLUS privé que l'existant

Contre-intuitif mais important. Dans une tx BQ actuelle (A3, tout-PQ), la tx est
`RCTTypeNull` : **tous les montants de sortie sont en clair**. Dans une tx hybride, les sorties
redeviennent des engagements de Pedersen avec preuves de portée : **seuls les montants des
entrées PQ sont révélés, les sorties sont de nouveau masquées.**

Donc l'hybride n'est pas qu'un confort de consolidation : c'est une **amélioration de
confidentialité** par rapport au chemin BQ d'aujourd'hui. Argument fort pour le faire, et à
répercuter dans le whitepaper §6 (cf. item 3b, qui documente justement la transparence des
dépenses BQ).

Note connexe : depuis le fix CRIT-1, les sorties BQ transparentes sont stockées en base dans
le bucket 0 avec un engagement `zeroCommit(amount)` — elles sont donc **déjà mixables comme
membres de ring**, ce qui est cohérent avec ce design.

## 5. Ce qu'il faudrait changer (esquisse, non implémentée)

1. **`cryptonote_tx_utils.cpp`** — retirer le refus « tout-ou-rien », et distinguer trois cas :
   * 0 entrée PQ → chemin classique, inchangé (**la chaîne live doit rester byte-identique**) ;
   * 0 entrée ring → chemin transparent A3 actuel, inchangé ;
   * les deux → nouveau chemin hybride : type RingCT normal, `pseudoOuts` sur les seules
     entrées ring, masques choisis pour que `Σ x'_i == Σ y_j` (le dernier masque d'entrée ring
     absorbe l'écart — **il faut donc ≥ 1 entrée ring**, ce qui est vrai par définition du cas
     hybride), montants PQ injectés au §2.
2. **`blockchain.cpp` check *e*** — remplacer le rejet par un branchement :
   * toutes les entrées PQ → règle A3 actuelle (`RCTTypeNull` + arithmétique) inchangée ;
   * mixte → exiger un type RingCT non-Null, vérifier la balance avec le terme transparent, et
     vérifier `pseudoOuts.size()` == nombre d'entrées ring.
3. **Vérification RingCT** — une variante de `verRctSemanticsSimple` acceptant la somme
   transparente en paramètre. À faire **sans toucher** le chemin classique : la fonction
   existante reste l'appel utilisé quand la somme transparente vaut 0.
4. **`get_tx_fee`** — pour une tx hybride, le frais est le `txnFee` RingCT explicite, pas la
   différence des montants révélés (règle actuelle pour les tx tout-PQ).
5. **Règles annexes à revisiter pour les tx mixtes** : plancher mixin (la boucle n'inspecte que
   les `txin_to_key`, à re-vérifier), minimum 2 sorties, tri/unicité des key images (une entrée
   PQ utilise une key image synthétique).
6. **`wallet2`** — sélection des entrées autorisée à mixer les deux mondes, et calcul du frais
   et du change adapté.

## 6. Risques

* Toucher à la balance RingCT, c'est toucher au cœur anti-inflation. Toute erreur =
  **création monétaire**. Le garde-fou est que tout est sous `hf_version >= HF_VERSION_PQ` et
  que le chemin où la somme transparente vaut 0 doit rester bit-à-bit l'actuel.
* Combinatoire de tests : il faut couvrir tout-ring, tout-PQ, mixte, et les cas d'attaque
  (entrée PQ non déclarée dans la balance, `pseudoOuts` mal dimensionné, montant PQ falsifié).
* `MAX_TX_EXTRA_SIZE_PQ` : une tx hybride porte à la fois les champs PQ et les preuves de
  portée → re-vérifier les plafonds de taille.

## 7. Décision demandée

1. Fait-on l'hybride, ou assume-t-on définitivement « BQ et B... ne se mélangent jamais » ?
2. Si oui : **H2** (pseudoOuts sur les seules entrées ring, montants PQ comme frais négatif)
   est-elle validée comme structure ?
3. Ordre de priorité : avant ou après la finalisation du binding C-1 et la levée de M-11 ?
   (les deux sont déjà bloquants pour HFv16, donc l'hybride n'est pas sur le chemin critique).

**Aucune ligne de code ne sera écrite avant réponse.**
