# CRIT-2 — Le montant d'une entrée BQ transparente n'était lié à rien (création monétaire)

**Date :** 8 septembre 2026
**Gravité : CRITIQUE — inflation.** Portée : HFv16 uniquement, jamais activé → **aucun impact
sur la chaîne live (hf 15), aucun fonds réel exposé, aucune action opérateur requise.**
**Statut : ✅ CORRIGÉ** (commit `bab8ca00e`, check consensus **b2**).
Découvert en implémentant l'hybride (décision 3), dont l'équilibre reposait dessus.

## 1. Le bug

`txin_to_key_pq.amount` est la valeur révélée de la sortie BQ dépensée. Les checks consensus
existants établissent que le dépensier **possède** cette sortie :

| Check | Ce qu'il prouve |
|---|---|
| (a) | la sortie référencée existe et n'est pas déjà dépensée |
| (b) | la clé révélée `real_output_key` correspond à la clé on-chain |
| (c) | le bind tag publié à la création engage cette clé vers une clé ML-DSA-65 par sortie |
| (d) | cette clé ML-DSA-65 signe la transaction |

**Aucun ne dit ce que la sortie VAUT.** Et le check (e) équilibre la transaction *sur ce
nombre* : `Σ montants d'entrée PQ ≥ Σ montants de sortie`.

Conséquence : le propriétaire d'une sortie BQ de poussière pouvait la dépenser **en déclarant
`amount = 1000 HRG`** et créer des sorties pour 1000 HRG. Tous les checks passent — il possède
bien la sortie, il ne ment que sur sa valeur. **Création monétaire à partir de rien**, à la
portée de quiconque détenant une seule sortie BQ, dès l'activation de HFv16.

Le commentaire du code notait déjà « `in.amount` is attacker-supplied » — à propos du choix du
bucket LMDB. La conclusion n'avait pas été tirée pour la **valeur**.

## 2. Pourquoi ce n'était pas détectable autrement

L'engagement de Pedersen stocké on-chain est le seul enregistrement de la valeur, et il est
*hiding* : le validateur ne peut pas en déduire le montant. Il faut donc que le dépensier
**l'ouvre**. Rien ne le lui demandait.

Le test `pq_spend_test` modélisait déjà les checks (c)/(d)/(e) et passait — parce qu'il
utilisait des montants honnêtes. Un test ne peut pas révéler une contrainte absente s'il ne
tente jamais de la violer.

## 3. Le correctif — check (b2)

`txin_to_key_pq` porte désormais le **facteur d'aveuglement** (`mask`) de la sortie dépensée,
et le validateur exige :

```
engagement on-chain  ==  amount * H  +  mask * G
```

Les deux façons dont une sortie BQ peut exister sont couvertes :

* créée par une transaction RingCT classique → masque dérivé de l'ECDH, déjà enregistré par le
  wallet au scan (`td.m_mask`) ;
* créée par une dépense BQ transparente → stockée avec un masque identité (cf. CRIT-1), et
  `commit(a, I)` est exactement le `zeroCommit(a)` que le daemon a stocké.

Le wallet connaissait déjà le bon masque dans les deux cas ; `construct_tx` se contente de le
publier.

**Fuite d'information : aucune.** Le montant est déjà révélé en clair par cette forme de
transaction ; le masque permet seulement de le *confirmer*.

**Coût de compatibilité : nul.** C'est un changement de format wire de `txin_to_key_pq`, mais
HFv16 n'a jamais été actif — aucune entrée de ce type n'existe sur aucune chaîne.

## 4. Preuve (test `pq_hybrid_test`)

Le test montre les checks **(b)(c)(d)(e) accepter tous** un montant gonflé 1000×, et **seul
(b2) le rejeter** — y compris qu'aucun autre masque ne peut ouvrir l'engagement pour la valeur
gonflée (ce serait un logarithme discret). Il vérifie aussi que (b2) accepte le cas masque
identité et rejette un montant décalé d'une seule unité atomique.

## 5. Leçon pour la suite

Les checks d'**autorisation** (qui a le droit de dépenser) et de **valeur** (combien) sont
deux familles distinctes. Le design Option-2-transparent a soigneusement construit la première
et a supposé la seconde acquise parce que le montant était « révélé ». Révélé ≠ vérifié.

À réexaminer sous cet angle avant HFv16 : tout champ que le validateur *lit* d'une entrée ou
d'une sortie PQ sans le recouper avec une donnée déjà engagée sur la chaîne.
