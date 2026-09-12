# Design 2d — Le facteur post-quantique du destinataire : S1b, et la liabilité à la dépense

**Date :** 12 septembre 2026 — suite de 2a / 2b / 2c, déclenché par le résiduel quantique de
CRIT-3 (§5 de son rapport) une fois **CRIT-4 fermé** (`a691897f1`), qui était le préalable posé
par `design_2c §5.3`.
**Statut : RAPPORT DE DESIGN. Aucun code écrit. En attente de validation.**

## 0. La question posée

`design_2c §5.2` propose **S1b** pour fermer le dernier cas de CRIT-3 (un émetteur qui est *aussi*
quantique) : un engagement de 32 octets dans l'adresse BQ, et une **clé ML-DSA-65 d'identité par
(sub)adresse** révélée à la dépense, *en remplacement* de la clé ML-DSA par sortie. Coût en taille
nul — mais payé en **liabilité** : `dsa_pk` étant constante par subaddress, deux dépenses de
sorties reçues sur la même subaddress deviennent reliables.

C'est exactement la propriété que la décision 4 (B3) a payé 8 octets pour préserver côté
réception. La question est donc légitime : **existe-t-il une variante aveuglée**, dans l'esprit du
tag de sélection (`pqc_compute_sel_tag`), qui change à chaque transaction plutôt qu'exposer une
identité fixe ?

Ce rapport répond dans l'ordre : ce que 2c a réellement exploré (§1), **un fait vérifié dans le
code qui déplace la question** (§2), pourquoi l'aveuglement d'une preuve n'est pas l'aveuglement
d'un tag (§3), et les options (§4–§6).

---

## 1. Ce que 2c a exploré — et ce qu'il n'a pas exploré (question 1)

**Exploré** : l'aveuglement **à la création**. `design_2c §5.2 / S1` dit déjà que « le bind tag
engage `P'` vers cette clé sous un aveuglement dérivé de `ss`, pour que rien ne soit lié **à la
création** ». C'est acquis et ce n'est pas le problème.

**Exploré aussi** : l'idéal non-liable, sous la forme **S3** — une preuve ZK de connaissance de la
clé de décapsulation ML-KEM. 2c la nomme, la qualifie de « solution idéale sur le papier » et
l'écarte comme direction de recherche (cryptographie non standardisée, rien dans liboqs).

**Non exploré** : l'aveuglement **de la révélation elle-même**. 2c passe directement de « on
révèle une clé d'identité fixe » (S1/S1b) à « il faudrait une preuve ZK » (S3), sans poser la
question intermédiaire — *peut-on re-randomiser la clé d'identité à chaque dépense, comme le
`sel_tag` re-randomise l'empreinte de la clé ML-KEM à chaque sortie ?* C'est le trou, et c'est
l'objet du §3.

---

## 2. Le fait qui déplace la question : la liabilité à la dépense est **déjà** payée, et plus cher

Vérifié dans le code livré, pas déduit.

Depuis l'audit **C-1** (Étape 8, 5 juin) et son activation opt-in par **A4** (26 juin), toute
transaction qui **dépense** une sortie BQ publie, en clair dans `tx_extra` (tag `0x06`), la
**clé publique ML-DSA-65 persistante du compte** :

```
cryptonote_tx_utils.cpp:331   const bool any_pq = (hf_version >= HF_VERSION_PQ) && num_pq_sources > 0;
cryptonote_tx_utils.cpp:930   if (hf_version >= HF_VERSION_PQ && any_pq)          // → signe avec
cryptonote_tx_utils.cpp:945-946    sender_account_keys.pq_dilithium->dilithium_{sk,pk}
cryptonote_tx_utils.cpp:952        add_pq_sig_to_extra(tx.extra, pq_sig);          // pk(1952) ‖ sig(3309)
```

`pq_dilithium` est la clé **de compte**, stable pour la vie du wallet (`account.h`, dérivée une
fois par `generate_pq_keys`). Conséquences, à énoncer sans détour :

1. **Toutes les dépenses BQ d'un même wallet sont déjà trivialement reliables entre elles**, par
   simple comparaison de 1952 octets publiés en clair. Pas par subaddress : **par compte**.
2. Et comme une dépense BQ transparente nomme `real_output_key` et `spent_output_index`
   (`cryptonote_basic.h:171-172`), relier deux dépenses **relie les deux sorties dépensées**, donc
   **les deux réceptions**. La non-liabilité que B3 protège côté réception est donc **déjà
   défaite, rétroactivement, pour toute sortie qui finit par être dépensée**.
3. Cette clé **n'autorise rien**. C'est le constat de C-1 lui-même (« binding validateur pleinement
   souverain impossible sur une chaîne privacy ») et le tableau de `CRIT-3 §1` le dit noir sur
   blanc : *signature de compte → n'importe quelle clé*. C'est donc aujourd'hui **un coût de
   confidentialité pur, en face d'une valeur d'autorisation nulle**, pour 5262 octets par tx.

Portée exacte, pour ne pas exagérer : le tag `0x06` n'est émis que si `num_pq_sources > 0`, donc
**seulement à la dépense**. Un paiement B... → BQ n'en porte pas ; la réception reste couverte par
B3. La fuite est strictement « qui dépense » — mais elle est totale à ce niveau-là.

> **Conséquence sur la question posée.** La prémisse « accepter S1b reviendrait à défaire ce qu'on
> vient de protéger » est juste en direction, mais **le dommage est déjà fait, et à une granularité
> plus grossière**. Remplacer une identité de **compte** par une identité **par subaddress** est,
> en soi, un **resserrement** du périmètre de liaison, pas un élargissement. S1b bien fait n'est
> pas une régression : c'est une réparation partielle.

Ce point n'apparaît pas dans 2c. Il change l'arbitrage, et il ajoute une exigence à toute option
retenue : **S1b doit remplacer la signature de compte `0x06`, pas s'y ajouter** (§5).

---

## 3. Pourquoi on n'aveugle pas une preuve comme on aveugle un tag (question 3)

### 3.1 L'asymétrie, en code

| | `sel_tag` (B3) | `bind_tag` + signature (autorisation) |
|---|---|---|
| Qui le vérifie | le **destinataire** | le **consensus** |
| Vérifié par `check_tx_inputs` ? | **non** — seuls les index le sont (`check_pq_output_field_indices`) | **oui** — `blockchain.cpp:3571-3577` recalcule et compare le tag, `:3580` vérifie ML-DSA, `:3597` vérifie d2 |
| Secret disponible au vérificateur | `d` (dérivation Ed25519, `a·R`) | **aucun** |
| Aveuglement possible | oui : masque jetable `pad(d, i)`, que seul le destinataire retire | — |

`pqc_sel_pad` prend en entrée `derivation`, **un secret partagé** (`pqc.cpp`, et la fonction
scrube sa copie pour cette raison). C'est ce qui rend le masque possible : le vérificateur en sait
plus que l'observateur.

### 3.2 Le mur, en une phrase

> **Un aveuglement ne cache quelque chose que si le vérificateur détient un secret que
> l'observateur n'a pas. Dans une règle de consensus, le vérificateur EST l'observateur.**

Tout ce qu'un nœud peut vérifier, n'importe qui peut le vérifier. Donc un masque que le consensus
sait retirer est public, et ne cache rien ; et un masque qu'il ne sait pas retirer rend le contrôle
impossible. Le secret ne peut donc pas venir de l'asymétrie d'information : il doit venir de la
**construction** — un énoncé *publiquement vérifiable* et pourtant *zero-knowledge* sur la clé qui
l'a satisfait.

Corollaire concret, et c'est lui qui tue les variantes « astucieuses » : **`Verify` a besoin de la
clé de vérification en clair.** Toute variante qui cache la clé de vérification transforme la
vérification de signature en preuve ZK de la relation de vérification. Il n'y a pas de milieu.

Ed25519 dispose de cet énoncé depuis longtemps et pour pas cher — c'est CLSAG : « je connais le
dlog de l'un de ces N points », preuve OR quasi gratuite grâce à la structure de groupe. C'est
exactement ce qui manque côté réseaux euclidiens.

### 3.3 Ce qu'il faudrait nommément

Deux familles, et deux seulement, donnent la propriété recherchée sans ZK générique :

* **Signature à clé re-randomisable / aveuglable** : une opération publique `Blind(pk, r) → pk'`
  telle que le détenteur de `sk` peut signer sous `pk'`, que `pk'` soit indistinguable d'une clé
  fraîche, et que **seul** le détenteur du secret long terme puisse produire `sk'`. L'émetteur
  calculerait `pk'` à la création (il a `r` depuis `ss`) sans jamais pouvoir signer.
* **Signature basée sur l'identité (IBS)** : l'adresse publie une clé maître, l'émetteur choisit
  `ID` = la sortie, et seul le détenteur de la trappe dérive `sk_ID`.

Les deux existent dans la littérature sur réseaux (les trappes GPV — celles-là mêmes qui fondent
Falcon — donnent de l'IBE/IBS). **Aucune des deux n'est standardisée, et aucune n'est dans
liboqs.**

Et — point décisif pour l'IBS — elle ne résout pas la liaison : `Verify` a besoin de la **clé
maître**, qui est par construction par adresse. Révélée à la dépense → on retombe sur S1b. L'IBS
déplace le problème du « qui peut signer » (qu'elle résout élégamment) sans toucher au « qui peut
voir » (§3.2).

### 3.4 Pourquoi ML-DSA-65 y résiste spécifiquement

Trois obstacles distincts, du plus concret au plus politique :

1. **La clé publique est compressée.** En FIPS 204, `pk = (ρ, t1)` où `t1` sont les bits de poids
   fort de `t = A·s1 + s2` (`Power2Round`, d = 13) ; les bits bas `t0` vivent dans la clé secrète.
   Un tiers ne connaît donc **pas** `t`, seulement son arrondi. Or `HighBits(t + Δ) ≠
   HighBits(t) + HighBits(Δ)` à cause des retenues : **la re-randomisation additive n'est même pas
   bien définie sur la forme publique**. Il faudrait publier `t` entier — ce n'est plus ML-DSA.
2. **Les normes.** Le secret doit rester court (`s1, s2` à coefficients dans `[-η, η]`). Un tweak
   additif allonge le secret ; les bornes de rejet (`‖z‖ < γ₁ − β`) et le hint `h` sont calibrés
   sur ces normes. Re-randomiser change le taux de rejet et la marge de sécurité : ça demande une
   nouvelle analyse, pas un wrapper.
3. **FIPS 204 fige `ξ → (pk, sk)`.** Toute variante re-randomisée est, par définition, hors du
   standard. Et c'est frontalement contraire à **M-11**, dont la condition de levée avant HFv16 est
   d'aller vers **plus** de maturité (implémentation validée, audit tiers), pas d'ajouter une
   construction de recherche au chemin critique de l'autorisation de dépense.

Note honnête sur **Falcon** : sa clé publique `h` n'est **pas** compressée, donc l'objection 1 ne
s'applique pas. Mais le secret est une **base courte** satisfaisant une relation NTRU
*multiplicative* (`h = g·f⁻¹`) : un décalage additif de `h` ne préserve pas la trappe. Falcon n'est
pas plus aveuglable que ML-DSA, pour une autre raison.

### 3.5 Ce que la dépendance offre réellement

Vérifié dans `external/liboqs/build/include/oqs/oqsconfig.h` (pin `97f6b86` = 0.15.0), familles de
signature activées : **ML-DSA, Falcon, SLH-DSA/SPHINCS+, MAYO, CROSS, SNOVA, LMS/XMSS**.

**Zéro** schéma de signature de cercle, aveugle, de groupe ou à clé re-randomisable. Ce n'est pas
un oubli de configuration : liboqs est une bibliothèque KEM + SIG, elle n'expose aucune primitive
d'anonymat. La variante non-liable ne se construit donc pas « en assemblant ce qu'on a » ; elle
demande une primitive à importer ou à écrire.

### 3.6 Les impasses explorées, et pourquoi elles ferment

Consignées parce qu'elles paraissent prometteuses et qu'il faut qu'on cesse de les reproposer :

* **Clé PQ par sortie détenue par le seul destinataire.** L'émetteur doit publier l'ancrage au
  moment où il crée la sortie ; tout ce qu'il peut calculer depuis des données publiques et son
  propre aléa a un secret qu'il peut calculer aussi. C'est l'argument d'impossibilité de
  `CRIT-3 §5`, et il tient — **sauf** via §3.3, qui est justement ce qui n'est pas disponible.
* **Dériver la clé par sortie de `ss` ET de la racine PQ du destinataire.** L'émetteur ne pourrait
  alors pas calculer la clé *publique*, donc pas publier le bind tag. Ferme immédiatement.
* **Arbre de Merkle d'identités jetables dans l'adresse** (racine 32 o, une feuille par dépense).
  La dépense révèle feuille + chemin ; le chemin **recalcule la racine**, qui est par subaddress.
  Liable à l'identique. Cacher le chemin = preuve ZK d'appartenance (§3.2).
* **Anneau d'engagements d'adresses** (l'analogue direct de CLSAG). Il faudrait une preuve OR sur
  `K` vérifications ML-DSA : ZK générique, §3.2.
* **Aveugler l'engagement plutôt que la clé** — publier `Keccak(P' ‖ Q_h ‖ ss)` et révéler moins à
  la dépense. Inutile : pour lancer `Verify`, le validateur a besoin de `Q` **en clair** (§3.2,
  corollaire). Retirer le masque, c'est révéler `Q`.
* **Réclamer puis dépenser** (le destinataire ancre d'abord une clé jetable dans une tx antérieure).
  L'ancrage doit lui-même être autorisé, donc par `d2`, qui est classique : un adversaire quantique
  devance simplement l'ancrage. Coût : une transaction de plus, pour zéro gain.
* **SLH-DSA à la place de ML-DSA.** `pk` = 32 o (l'adresse porterait la clé, sans engagement), mais
  signature 7856 o pour la catégorie 1 / 16224 o pour la catégorie 3 — et **liabilité identique**.
  Change la facture, pas la propriété.

> **Réponse à la question 3 : c'est un mur — mais un mur de *primitives standardisées*, pas une
> impossibilité de l'information.** La construction existe dans la littérature ; elle n'existe ni
> dans une norme, ni dans notre dépendance. La distinction compte, parce qu'elle dit que la bonne
> réponse est une **réservation de format**, pas un renoncement définitif (option T4).

---

## 4. Options

### T1 — S1b tel que spécifié en 2c : identité ML-DSA fixe par subaddress
L'adresse publie `Q_h = Keccak(domaine ‖ dsa_pk)` (+32 o) ; la dépense révèle `dsa_pk` (1952 o) et
signe, **en remplacement** de la clé ML-DSA par sortie (5261 o déjà dans `vin`), **et en
remplacement de la signature de compte `0x06`** (§2, §5).

* Ferme le résiduel de CRIT-3 : le facteur d'autorisation devient post-quantique **et** détenu par
  le seul destinataire.
* Liabilité : **par subaddress**, au lieu de **par compte** aujourd'hui → resserrement.
* Taille : **−5262 o** de `tx_extra` par dépense BQ, ±0 dans `vin`, +32 o dans l'adresse.
* Ne dépend d'aucune primitive nouvelle : ML-DSA-65, déjà utilisé.

### T2 — T1 + subaddress BQ à usage unique, **imposée par le wallet**
Même cryptographie. La politique convertit « liable par subaddress » en « liable par demande de
paiement », c'est-à-dire au niveau que le payeur connaît déjà.

* Réalisable seulement parce que B3 existe : les subaddresses BQ sont redérivées à la demande
  (≈17 µs) et le scan reste O(1) quel qu'en soit le nombre (`design_2b §8.4`).
* Il ne suffit **pas** de le conseiller : un wallet qui réutilise une subaddress BQ déjà dépensée
  perd la propriété en silence. Donc refus au niveau du wallet, et change d'une dépense BQ
  systématiquement dirigé vers une subaddress neuve (le §10 de 2b laisse justement la politique de
  change ouverte — elle se décide ici).
* Résiduel assumé : plusieurs sorties reçues **sur la même demande de paiement** restent liables
  entre elles à la dépense.

### T3 — Identité PQ aveuglée / re-randomisable (la variante non-liable demandée)
Ce que la question appelle de ses vœux, et qui serait strictement meilleur : une clé d'identité
`pk'` fraîche à chaque dépense, dérivée publiquement de la clé d'adresse et d'un tweak issu de
`ss`, dont seul le destinataire peut produire le secret.

* **Propriété** : non-liabilité complète à la dépense, +32 o d'adresse, taille d'entrée comparable.
* **Blocage** : la primitive n'existe ni dans FIPS 204, ni dans liboqs (§3.4, §3.5). Pour ML-DSA
  elle bute sur la compression de `t1` et sur les bornes de norme ; pour Falcon sur la structure
  NTRU multiplicative.
* **Coût réel si on la voulait quand même** : importer ou écrire un schéma de signature à clé
  aveuglable sur réseaux, l'auditer, et l'exposer dans le chemin d'autorisation de dépense — c'est-à-
  dire faire exactement l'inverse de ce que **M-11** exige avant HFv16.
* **Verdict** : direction de recherche, pas option livrable. À ne pas confondre avec « écartée ».

### T4 — Réserver le format maintenant, trancher la primitive plus tard ⭐ à combiner
Le coût irréversible n'est pas la primitive : c'est le **format**. Après HFv16, ajouter un champ
d'adresse ou un champ d'entrée est un second hard fork.

* Poser dès maintenant, dans la charge utile de l'adresse BQ, un **octet de version / capacité**
  devant l'engagement de 32 o, et traiter l'engagement comme opaque (« un engagement vers le
  facteur d'autorisation du destinataire », pas « un Keccak d'une clé ML-DSA »).
* Poser de même, côté entrée, un champ d'autorisation **typé** plutôt qu'un `pq_tx_sig` de taille
  fixe.
* Effet : T1/T2 sont déployables immédiatement avec ML-DSA-65, et T3 devient un **ajout de type**
  le jour où une primitive aveuglable est standardisée — sans toucher au format d'adresse, donc
  sans second hard fork, et sans invalider les adresses BQ déjà distribuées.
* Coût : 1 octet dans l'adresse, plus la discipline de ne pas figer le type dans le consensus.

**Recommandation : T4 + T2** — la clôture effective de CRIT-3 avec ce qui est standardisé
aujourd'hui, la liabilité ramenée au niveau « demande de paiement » et imposée par le wallet, et
le format assez ouvert pour que la variante non-liable soit un ajout et non une rupture.

---

## 5. Coût comparé

Base : `design_2b §9`, mesuré sur des transactions réelles, `MAX_TX_EXTRA_SIZE_PQ` = 8192.

| Élément | Aujourd'hui | T1/T2 | T3 |
|---|---|---|---|
| Charge utile de l'adresse BQ | 1249 o (1725 car.) | **1281 o** (1769 car.) | 1281 o + 1 o de version (T4) |
| ML-DSA par sortie, dans `vin` | 5261 o | remplacée (±0) | remplacée (±0) |
| Signature de compte `0x06`, `tx_extra` | **5262 o / tx** | **supprimée (−5262 o)** | supprimée |
| `owner_sig` Ed25519 (d2) | 64 o | conservée | conservée |
| Liabilité des dépenses | **par compte** | par subaddress → par demande de paiement (T2) | **aucune** |
| Primitive requise | — | ML-DSA-65 (déjà là) | inexistante en norme |

Le **gain de place n'est pas cosmétique** : `design_2b §9` mesure qu'une dépense BQ transparente
plafonne aujourd'hui à **2 sorties BQ** (7570 o), ce qui interdit de payer deux destinataires BQ
depuis un wallet BQ. Retirer les 5262 o de la signature de compte lève l'essentiel de cette
contrainte. Ordre de grandeur : ~1164 o par sortie BQ supplémentaire, soit de l'ordre de **6 à 7**
sorties dans le même budget — **à mesurer, pas à croire sur parole** : la note du §5.3 de 2b
comptait la ML-DSA par sortie et s'est révélée fausse au banc d'essai (`design_2b §9`).

Comparaison demandée avec les 8 octets de B3 : le `sel_tag` achète la non-liabilité **en
réception** pour 8 o par sortie, parce qu'il masque une donnée que seul le destinataire vérifie.
La non-liabilité **en dépense** ne s'achète pas en octets — il n'y a pas de prix en taille qui la
rende disponible, seulement un changement de primitive (§3). C'est la différence de fond entre les
deux moitiés du problème, et la raison pour laquelle 8 o ont suffi d'un côté et ne suffisent
d'aucune manière de l'autre.

---

## 6. Ce que ça change pour B3 et pour C-1

* **B3 (décision 4) n'est pas remis en cause.** Il protège la réception, et la réception n'émet
  aucune des données discutées ici (le tag `0x06` n'apparaît qu'à la dépense, `any_pq` exige
  `num_pq_sources > 0`). Ce que le §2 établit, c'est que sa garantie était déjà **érodée en aval**,
  par la dépense — T1/T2 réduisent cette érosion au lieu de l'aggraver.
* **C-1 trouve enfin sa réponse.** Le binding cherché depuis l'Étape 8 — lier la clé ML-DSA
  publiée à l'émetteur réel — est impossible pour une clé **de compte** sur une chaîne privacy.
  Il devient trivial pour une clé **d'adresse** : l'engagement de 32 o dans l'adresse, et le bind
  tag qui pointe vers elle, sont ce lien. La spec C-1 et S1b sont le même objet ; elles doivent
  être écrites ensemble, et **avant HFv16**, puisqu'elles touchent le format d'adresse.
* **Ordre vis-à-vis de 2c** : le préalable posé par `design_2c §5.3` est levé — la clé d'identité
  dérive maintenant de `pq_root`, indépendante de la clé de dépense (`a691897f1`). Sans ce
  correctif, S1b aurait protégé une racine que Shor livrait.

---

## 7. Décision demandée

1. **Le §2 est-il accepté comme constat ?** — la signature de compte `0x06` coûte 5262 o par
   dépense BQ, lie toutes les dépenses BQ d'un wallet, et n'autorise rien. Si oui, **sa suppression
   est acquise indépendamment du reste**, et elle peut être traitée séparément.
2. **T1 (ou T2) est-elle retenue** pour fermer le résiduel de CRIT-3, en actant que la liabilité
   passe de « par compte » à « par subaddress » (donc : amélioration, pas régression) ?
3. **Si T2 : la subaddress BQ à usage unique doit-elle être imposée par le wallet** (refus de
   réutilisation, change dirigé vers une subaddress neuve), ou seulement documentée ?
4. **T4 est-elle retenue** — octet de version dans l'adresse BQ et champ d'autorisation typé dans
   l'entrée — pour que la variante non-liable reste ajoutable sans second hard fork ?
5. **Confirme-t-on que T3 est une direction de recherche** et non un blocage supplémentaire à
   HFv16, au vu du §3.4 et de M-11 ?

**Aucune ligne de code ne sera écrite avant réponse.**
