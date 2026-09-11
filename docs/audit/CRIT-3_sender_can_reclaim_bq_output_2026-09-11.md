# CRIT-3 — L'émetteur d'une sortie BQ pouvait la redépenser

**Date :** 11 septembre 2026
**Gravité : CRITIQUE — vol.** Portée : HFv16 uniquement, jamais activé → **aucun impact sur la
chaîne live (hf 15), aucun fonds réel exposé, aucune action opérateur requise.**
**Statut : ✅ CORRIGÉ contre un émetteur classique** (commit `678c170b9`, check consensus **d2**, voir §3).
**⚠️ Résiduel contre un émetteur doté d'un ordinateur quantique** (§5) — bloquant pour HFv16, au
même titre que le binding C-1.
Découvert en implémentant les subaddresses BQ (décision 4), en vérifiant quelle clé autorise
réellement une dépense BQ.

## 1. Le bug

Une dépense BQ transparente (`txin_to_key_pq`) était autorisée par exactement ceci :

| Check | Ce qu'il vérifie | Qui détient la donnée |
|---|---|---|
| (b) | la clé révélée `P'` est celle de la sortie on-chain | public |
| (b2) | `amount*H + mask*G` ouvre l'engagement on-chain | l'**émetteur** : il a choisi le montant, et le masque se déduit de sa propre clé de tx (`r·A`) |
| (c) | le bind tag publié à la création engage `P'` vers la clé ML-DSA par sortie | la clé ML-DSA dérive de `(ss, index)` |
| (d) | cette clé ML-DSA signe la transaction | idem |
| signature de compte | une ML-DSA-65 de compte signe la tx | **n'importe quelle clé** (C-1 : non liée au propriétaire) |
| double dépense | key image synthétique `Keccak(P')` | public |

`ss` est le secret partagé ML-KEM. Un KEM le produit **des deux côtés** : le destinataire en
décapsulant avec sa clé secrète, l'émetteur en **encapsulant** — `OQS_KEM_encaps` renvoie le
ciphertext **et** `ss` ensemble, sans aucune clé secrète. Dans `construct_tx`, c'est la variable
locale `kss` de l'émetteur ; le code honnête l'efface, un émetteur malveillant la garde.

**Aucun check ne demandait de preuve de possession de la clé secrète unique `x` (`P' = x·G`)** —
la seule chose que le destinataire possède et pas l'émetteur. Conséquence : quiconque avait payé
une sortie BQ pouvait la redépenser. Émetteur et destinataire entraient en course ; le premier
miné l'emportait, l'autre dépense était rejetée comme double dépense (même key image synthétique).
En pratique l'émetteur gagne toujours : il peut pré-signer et diffuser au premier bloc où la
sortie devient dépensable.

## 2. Pourquoi ce n'était pas visible

* Le test `pq_spend_test` modélisait la dépense **par le destinataire** : il ne tentait jamais
  une dépense par quelqu'un d'autre possédant `ss`. Même leçon que CRIT-2 : un test ne révèle
  pas une contrainte absente s'il ne tente jamais de la violer.
* Le design Option-2 a construit l'autorisation autour de la clé ML-DSA par sortie, et a traité
  « dérivé du secret KEM » comme « détenu par le destinataire ». Pour un KEM, c'est faux.
* `CLAUDE.md` affirmait « la dépense reste gardée par dlog Ed25519 » (M-11). C'était vrai pour
  une dépense ring, **faux** pour une dépense BQ transparente : aucune signature Ed25519 n'y était
  vérifiée.

## 3. Le correctif — check (d2)

`txin_to_key_pq` porte désormais une **signature Ed25519 `owner_sig` par la clé secrète unique**
`x` de la sortie dépensée, et le validateur exige :

```
check_signature(message, P', owner_sig)
```

sur **le même message** que la signature ML-DSA par entrée (hash du préfixe avec toutes les
signatures d'entrée PQ — `dsa.sig` **et** `owner_sig` — mises à zéro et la signature de compte
retirée de `tx_extra`).

`x' = H_s(d ‖ i) + b (+ m pour une subaddress) + t` : la part `b` est la clé de dépense du
destinataire. L'émetteur connaît `H_s(d ‖ i)` (sa dérivation) et `t` (son `ss`), mais pas `b`.

Côté wallet, `construct_tx` retrouve `x` comme pour une entrée ring : `generate_key_image_helper`
sur la clé **non tweakée** `P' − t·G`, puis `x' = x + t`, avec contrôle `x'·G == P'`. Aucun
changement de format wallet ; le cold-sign reçoit déjà `real_out_tx_key` et les clés
additionnelles de chaque source.

**Deux défauts de l'hybride (décision 3, commit `bab8ca00e`) corrigés dans le même geste.** Le
contrôle positif « hybride » du test les a fait apparaître ; aucun des deux n'avait été vu parce
que `pq_hybrid_test` exerce l'équilibre RingCT, jamais `construct_tx` suivi du validateur.

1. **Toute tx hybride était invalide.** Les signatures PQ (ML-DSA par entrée, et la signature de
   compte) étaient calculées sur le préfixe **avant** que le chemin RingCT remette à zéro les
   montants des entrées ring et des sorties. Le préfixe diffusé n'était donc pas celui signé : les
   checks (d) et de signature de compte rejetaient chaque hybride. Corrigé : une seule règle,
   `zero_rct_amounts`, sert à la fois au hash signé (sur une copie) et à la mise à zéro réelle.
2. **Clés de signature attribuées à la mauvaise entrée.** `construct_tx` rangeait les clés des
   entrées PQ par **position dans `vin` avant le tri des entrées** (ring d'abord pour un hybride) :
   la position pouvait ensuite désigner une autre entrée PQ, ou une entrée ring — `boost::get`
   lève alors une exception. Les clés sont désormais retrouvées par `real_output_key`, après le tri.

**Coût :** 64 octets par entrée BQ, dans `vin` (pas dans `tx_extra`, donc sans effet sur le budget
de `MAX_TX_EXTRA_SIZE_PQ`). **Fuite :** aucune — `P'` est déjà révélé par cette forme de
transaction. **Compatibilité :** changement du format wire de `txin_to_key_pq`, gratuit puisque
HFv16 n'a jamais été actif.

## 4. Preuve (test `pq_sender_clawback_test`)

Le test utilise le code réel des deux côtés : `construct_tx_and_get_tx_key` construit le paiement
B... → BQ et les dépenses, `wallet2` scanne le paiement, et les checks du validateur sont
reproduits ligne à ligne depuis `Blockchain::check_tx_inputs`.

* **Avant le correctif** (rouge) : une dépense construite par un compte ne détenant **aucune** clé
  de la victime, avec seulement ce que l'émetteur avait en main (`ss`, montant, masque déduit de sa
  clé de tx), passait (b), (b2), (c), (d), la signature de compte et (e) ; sa key image était celle
  de la victime.
* **Après** (vert) :
  * la dépense **légitime** du destinataire passe tous les checks, (d2) compris — contrôle positif,
    pour une sortie reçue sur l'adresse primaire **et** une reçue sur une subaddress BQ, ainsi
    que pour l'entrée BQ d'une transaction **hybride** remise au constructeur dans l'ordre
    PQ-puis-ring (couvre le défaut d'indexation ci-dessus) ;
  * le vrai `construct_tx` refuse de construire la dépense de l'émetteur (il ne peut pas dériver `x`) ;
  * une dépense forgée à la main, signée avec le meilleur `x` que l'émetteur puisse former
    (`H_s(d ‖ i) + t`, sans `b`), passe toujours (b)(b2)(c)(d) — **et seul (d2) la rejette**.

## 5. Résiduel : un émetteur quantique

(d2) est une signature Ed25519. Un adversaire capable d'exécuter Shor calcule `x'` directement
depuis `P'`, qui est public. S'il est **aussi l'émetteur**, il connaît `ss` : il redevient capable
de redépenser la sortie. La matrice après correctif :

| Attaquant | A `ss` ? | A `x'` ? | Peut dépenser ? |
|---|---|---|---|
| tiers classique | non | non | non |
| **émetteur classique** | oui | non | **non** (corrigé) |
| tiers quantique | non (ML-KEM) | oui (Shor) | non — *sous réserve de CRIT-4* |
| **émetteur quantique** | oui | oui (Shor) | **oui (résiduel)** |

Fermer ce dernier cas exige qu'une clé **post-quantique détenue par le seul destinataire** entre
dans l'autorisation. C'est structurellement impossible avec la construction actuelle : au moment
de créer la sortie, l'émetteur doit publier l'engagement (bind tag) vers la clé qui signera la
dépense ; les clés ML-DSA ne sont pas homomorphes, donc si l'émetteur peut calculer cette clé
publique à partir de ce qu'il sait, il peut en calculer la clé secrète.

Piste principale : une **clé ML-DSA par (sub)adresse publiée dans l'adresse BQ** (+1952 o), le
bind tag engageant `P'` vers elle sous un aveuglement dérivé de `ss` (pour que la chaîne ne relie
pas les sorties à la création). Coût : à la dépense, la clé est révélée, donc **deux dépenses de
sorties reçues sur la même subaddress deviennent liables**. Arbitrage à trancher avec la
finalisation de C-1 — ce n'est pas un correctif, c'est un choix de design.

Note : le cas « tiers quantique » ne tient que si les clés BQ ne sont pas elles-mêmes retrouvables
par Shor. Aujourd'hui elles le sont (**CRIT-4** : racine PQ = clé de dépense, logarithme discret
d'une clé publiée). Les deux doivent être fermés avant HFv16.

## 6. Leçon pour la suite

**« Dérivé d'un secret partagé » n'est pas « détenu par le destinataire ».** Un secret partagé est
partagé : tout ce qu'on en dérive est connu des deux parties. Après CRIT-2 (autorisation ≠ valeur),
c'est la même famille d'erreur : attribuer à une donnée une propriété qu'elle n'a pas parce qu'elle
« a l'air » privée. À réexaminer sous cet angle avant HFv16 : toute clé ou tout secret que le
validateur traite comme une preuve d'identité — qui d'autre peut le calculer ?
