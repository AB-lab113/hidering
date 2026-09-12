# Spec 2e — Engagement d'autorisation dans l'adresse BQ, et champ d'autorisation à la dépense
## (spécification du binding C-1 + S1b / T1 / T2 / T4)

**Date :** 12 septembre 2026.
**Nature : SPÉCIFICATION, pas un rapport d'options.** 2a/2b/2c/2d posaient des choix ; celui-ci
les fige.
**Statut : ✅ IMPLÉMENTÉ le 12 septembre 2026.** Voir §8 en fin de document : ce qui a été livré,
les trois chiffres que l'implémentation a mesurés plutôt que repris de ce texte, et les deux
défauts latents trouvés en chemin.
**Décisions en amont :** `design_2c §5.2` (S1b), `design_2d §4` (T1 + T2 + T4) et `design_2d §2`
(suppression de la signature de compte `0x06`). Préalable levé : **CRIT-4** (`a691897f1`).
**Hors périmètre : T3** (identité aveuglée / re-randomisable). Seule la **place** qu'elle occupera
est réservée ici (§1.2, §2.2).

### Règle d'arbitrage de ce document

À chaque endroit où une propriété de sécurité ou de non-liabilité s'oppose à la taille, à la
simplicité d'implémentation ou au confort d'usage, **la propriété gagne, et l'arbitrage est écrit
en clair** (marqué ⚖). Les résidus de risque qui subsistent malgré tout sont énoncés en toutes
lettres (marqués ⚠), jamais laissés implicites.

---

## 0. Ce que la spec doit établir, et pourquoi la construction actuelle ne le fait pas

Aujourd'hui, une dépense BQ transparente est autorisée par une clé ML-DSA-65 **dérivée du secret
partagé ML-KEM** : `pqc_keygen_output_dsa(ss, output_index)`, engagée à la création par
`bind_tag = Keccak("HRG_PQ_BIND_v1" ‖ P' ‖ dsa_pk)` (`pqc.cpp`), vérifiée par les checks (c) et
(d) (`blockchain.cpp:3571-3584`). **`ss` est partagé : l'émetteur l'obtient par sa propre
encapsulation.** C'est CRIT-3, refermé contre un émetteur *classique* par le check (d2) — une
signature Ed25519 par la clé unique `x'` (`blockchain.cpp:3597`) — mais **pas** contre un émetteur
qui sait aussi casser Ed25519.

Il faut donc un facteur d'autorisation qui soit **à la fois post-quantique et détenu par le seul
destinataire**. C'est l'objet du §1 (ce que l'adresse publie) et du §2 (ce que la dépense révèle) ;
le §3 est la règle de consensus qui relie les deux — c'est-à-dire le **binding C-1** lui-même.

---

## 1. Format de la charge utile d'adresse BQ

### 1.1 Format fixé

Aujourd'hui (`cryptonote_basic_impl.cpp:355-378`, constante `PQ_ADDRESS_PAYLOAD_SIZE`) :

```
marker(1) ‖ B(32) ‖ A(32) ‖ kem_pk(1184)                                    = 1249 octets
```

Nouveau format, **obligatoire pour toute adresse BQ** :

```
marker(1) ‖ auth_ver(1) ‖ B(32) ‖ A(32) ‖ kem_pk(1184) ‖ auth_commit(32)    = 1282 octets
```

| Champ | Taille | Valeur |
|---|---|---|
| `marker` | 1 | `0x33` adresse primaire, `0x35` subaddress — **inchangé** (`cryptonote_config.h:323-327`) |
| `auth_ver` | 1 | version/capacité du facteur d'autorisation. **`0x01`** = ML-DSA-65 par (sub)adresse, le seul défini ici |
| `B`, `A` | 32+32 | clés Ed25519 de dépense / de vue — inchangées, mêmes offsets relatifs |
| `kem_pk` | 1184 | clé ML-KEM-768 de la (sub)adresse — inchangée |
| `auth_commit` | 32 | **engagement opaque** vers le facteur d'autorisation (§1.3) |

Longueurs base58 rendues (calculées avec la table de blocs de `tools::base58`, la valeur actuelle
1725 étant vérifiée par `pq_vector_test::V_BQ_ADDRESS_LEN`) : **1249 o → 1725 caractères**,
**1282 o → 1770 caractères**.

`design_2d §5` annonçait 1281 o / 1769 caractères : ce chiffre **ne comptait pas l'octet de
version** de T4. La valeur correcte est 1282 / 1770.

### 1.2 Position de `auth_ver` : immédiatement après le marker ⚖

Deux positions étaient possibles : juste après le marker, ou juste avant l'engagement.

**Retenu : juste après le marker**, c'est-à-dire à l'offset 1, **avant tout champ interprétable**.
Raison : un parseur doit connaître la version **avant** d'interpréter le moindre octet, pour
pouvoir rejeter une version inconnue sans avoir rien parsé. Placer la version en fin de charge
utile obligerait à lire `B`, `A` et `kem_pk` sous des hypothèses de format non encore confirmées.
⚖ *Coût accepté : les offsets de `B`/`A`/`kem_pk` se décalent d'un octet. Sans conséquence — la
taille de charge utile change de toute façon, et c'est la taille qui désambiguïse une adresse BQ
d'une subaddress classique (64 o), cf. `cryptonote_config.h:316-320`.*

**Contrainte à vérifier empiriquement avant d'allouer toute valeur de `auth_ver`.** Le préfixe
lisible « BQ » est une propriété du **premier bloc base58 de 8 octets**, soit
`varint(62) ‖ marker ‖ auth_ver ‖ B[0..4]` : `auth_ver` y entre. La décision 4 avait dû vérifier
le marker `0x35` « sur 2000 charges aléatoires » (`design_2b §8.4`) ; la même vérification est
**obligatoire ici, pour chaque couple (marker, auth_ver)**, et fait partie de la définition de
« terminé ». Si `0x01` ne rendait pas « BQ » de façon stable pour les deux markers, la valeur
allouée changerait — **jamais la position du champ**.

⚠ **Résidu : l'espace des versions est contraint par une propriété cosmétique.** Toutes les
valeurs de `auth_ver` ne conviendront pas. C'est la même contrainte que celle qui pèse déjà sur le
marker ; elle doit être documentée pour que l'allocation d'une future version (T3) commence par
ce test et non par du code.

### 1.3 Construction de l'engagement `auth_commit`

```
auth_commit = Keccak( "HRG_BQ_ADDR_AUTH_v1" ‖ auth_ver ‖ auth_pk )
```

* `Keccak` = `crypto::cn_fast_hash` (variante Monero), comme `pqc_compute_bind_tag`.
* `auth_ver` **entre dans le préimage**. ⚖ Sans lui, une future version pourrait produire le même
  engagement à partir d'un matériel différent : le consensus accepterait une clé d'un autre type
  contre un engagement écrit pour celui-ci. La séparation de domaine par type est une condition de
  sécurité, pas une commodité.
* `auth_pk` pour `auth_ver = 0x01` est la **clé publique ML-DSA-65 de la (sub)adresse** (1952 o).

**Le consensus traite `auth_commit` comme opaque.** Aucune règle ne dit « c'est le Keccak d'une clé
ML-DSA » ; la règle dit « l'autorisation révélée doit s'ouvrir sur cet engagement, selon la méthode
définie par `auth_ver` ». C'est ce qui permet d'ajouter T3 comme un nouveau `auth_ver` sans
toucher au format d'adresse (T4).

### 1.4 Dérivation de la clé d'identité par (sub)adresse

```
(0,0)              : auth_pk = keys.pq_dilithium->dilithium_pk          (inchangé, M-4)
(major, minor) ≠ 0 : dsa_seed = SHAKE256("HRG_BQ_SUBADDR_DSA_v1" ‖ racine ‖ major_le4 ‖ minor_le4)
                     (pk, sk)  = keygen ML-DSA-65 piloté par ce flux
```

* `racine` = `cryptonote::get_pq_root_secret(keys)` — **jamais la clé de vue, jamais la clé de
  dépense** (CRIT-4, `a691897f1`). Sans ce préalable, la clé d'identité aurait été retrouvable par
  Shor et toute cette spec aurait été du théâtre (`design_2c §5.3`).
* Structure identique à `pqc_kem_keygen_subaddress` (`pqc.cpp`), **domaine distinct**.
* ⚠ **liboqs n'expose pas de keygen dérandomisé pour SIG** : la dérivation passe par le hook
  `randombytes` isolé par thread (MOYEN-4, `cda1842b2`), donc elle est **sensible au motif de
  lectures d'aléa de l'implémentation**. C'est exactement le risque que `pq_vector_test` existe
  pour attraper, et cette nouvelle dérivation **doit y être épinglée** (§5.4).
* Rien de nouveau dans `.keys` : comme les clés ML-KEM de subaddress, ces clés se **redérivent à
  la demande** depuis la racine (`design_2b §8.4`).

---

## 2. Champ d'autorisation côté entrée

### 2.1 Ce qui disparaît

1. **`txin_to_key_pq.dsa`** (`cryptonote_basic.h:195`, `pq_tx_sig` = pk 1952 ‖ sig 3309) et toute
   la dérivation `pqc_keygen_output_dsa(ss, i)` qui l'alimentait.
2. **La signature de compte `tx_extra_pq_sig` (tag `0x06`)** : émission
   (`cryptonote_tx_utils.cpp:930-952`) et validation (`blockchain.cpp:3383-3416`), ainsi que la
   branche « aucune signature de compte attendue » (`:3419-3426`).

⚖ La suppression de `0x06` est **acquise indépendamment du reste** (`design_2d §2`) : cette clé
est **de compte**, publiée en clair à chaque dépense BQ, elle **lie entre elles toutes les dépenses
BQ d'un wallet**, et elle **n'autorise rien** (n'importe quelle clé passe le check). 5262 octets
par transaction de coût de confidentialité pur.

Bénéfice structurel au passage : les deux endroits qui **tronquaient `tx.extra` de la longueur
fixe du champ terminal** pour reconstruire le message signé (`blockchain.cpp:3400-3406` et
`:3472-3477`) disparaissent. C'est une hypothèse fragile en moins — « le champ `0x06` est le
dernier et fait exactement 5262 octets » — dans la même famille que les interactions
padding/parse canonique de F-4 vs E-3.

### 2.2 Ce qui le remplace : un champ **typé**

`txin_to_key_pq` porte désormais, à la place de `dsa` :

```
auth_type : varint          // même espace de valeurs que auth_ver ; 0x01 ici
auth_body : blob de taille FIXE, entièrement déterminée par auth_type
```

Corps pour `auth_type = 0x01` :

```
auth_blind(32) ‖ auth_pk(1952) ‖ auth_sig(3309)          = 5293 octets
```

soit **5294 octets** avec le varint de type, contre 5261 aujourd'hui : **+33 octets par entrée**.

⚖ `design_2d §5` annonçait « ±0 dans `vin` ». C'est **corrigé ici** : le rapport comptait le
remplacement 1952+3309 sans l'aveuglement ni l'octet de type. Le bilan réel d'une dépense BQ à une
entrée est **−5262 (extra) + 33 (vin) = −5229 octets**. Le gain reste massif, mais il devait être
énoncé juste — c'est le même réflexe que la correction mesurée de `design_2b §9`.

**Pas de préfixe de longueur auto-descriptif.** ⚖ La taille du corps est une fonction pure du
type ; un type inconnu est **rejeté au parse**, jamais sauté. Un champ « tag + longueur + données »
inviterait un parseur à ignorer ce qu'il ne comprend pas et ouvrirait une malléabilité de taille
(donc de hash de transaction) — exactement la classe de défaut du padding greedy `0x00`.

### 2.3 Ce qui est révélé, et ce qui ne l'est pas

| Donnée | Révélée à la dépense ? | Conséquence |
|---|---|---|
| `auth_pk` (1952 o) | **oui** | lie entre elles les dépenses d'outputs reçus sur **cette** (sub)adresse — c'est le coût assumé de T1 |
| `auth_sig` (3309 o) | oui | rien (signature) |
| `auth_blind` (32 o) | **oui** | rien sur les autres sorties : c'est une image à sens unique de `ss` (§2.4) |
| `ss` (secret partagé ML-KEM) | **non** | le tweak `t = derive_bq_output_tweak(ss, i)` reste caché |
| clé secrète d'identité | non | — |
| `auth_commit` de l'adresse | recalculable depuis `auth_pk` | idem `auth_pk` |

### 2.4 `auth_blind` : pourquoi un aveuglement dédié, et pourquoi pas `ss` ⚖

L'engagement de sortie doit être **aveuglé**, sinon la révélation de `auth_pk` lors d'**une seule**
dépense permettrait de recalculer `auth_commit`, puis de **balayer toute la chaîne** en testant
`Keccak(… ‖ P'_j ‖ auth_commit)` contre chaque bind tag publié — et d'identifier ainsi **toutes
les sorties jamais envoyées à cette subaddress, dépensées ou non**. Ce serait une destruction
rétroactive et totale de B3, bien pire que le coût assumé de T1.

L'aveuglement doit donc être révélé à la dépense pour que le consensus puisse recalculer (§3.2 de
`design_2d` : le vérificateur est l'observateur). Deux candidats :

* révéler `ss` — **refusé** : `ss` est aussi l'entrée du tweak `t` de la sortie et de toute
  dérivation future assise sur lui ;
* révéler un aveuglement **dédié et à sens unique** :

```
auth_blind = Keccak( "HRG_BQ_BINDBLIND_v1" ‖ ss ‖ output_index_le8 )
```

⚖ **Retenu.** Coût : 32 octets par entrée. Gain : la révélation n'apprend rien sur `ss`, donc rien
sur le tweak, donc rien qui puisse être recoupé avec une autre sortie. Payer 32 octets pour ne pas
élargir la surface de révélation est exactement l'arbitrage que ce document impose.

L'index de sortie entre dans le préimage pour la même raison que dans `pqc_sel_pad` : deux sorties
d'une même transaction vers la même subaddress ne doivent pas partager leur aveuglement.

---

## 3. Règles de consensus

### 3.1 Le bind tag publié à la création

`tx_extra_pq_bind` (tag `0x08`) est **inchangé en structure** (`output_index:varint ‖ bind_tag:32`,
`tx_extra.h:240-249`). Seul son contenu change :

```
v1 (actuel)  : bind_tag = Keccak("HRG_PQ_BIND_v1" ‖ P' ‖ dsa_pk_par_sortie)
v2 (cette spec) : bind_tag = Keccak("HRG_PQ_BIND_v2" ‖ auth_ver ‖ P' ‖ auth_commit ‖ auth_blind)
```

L'émetteur peut le calculer : il a `P'` (il crée la sortie), `auth_ver` et `auth_commit` (lus dans
l'adresse du destinataire), et `auth_blind` (dérivé du `ss` que son encapsulation lui a donné). Il
**ne peut pas** en dériver la clé secrète d'identité : `auth_commit` est un hash.

### 3.2 Table de vérification (modèle `pq_sender_clawback_test`)

Pour chaque `txin_to_key_pq in` d'une transaction avec `hf_version >= HF_VERSION_PQ`, dans cet
ordre exact :

```
(a)  INCHANGÉ   od := db.get_output_key(bucket 0, in.spent_output_index)      // existe
                reject si have_tx_keyimg_as_spent(get_pq_input_key_image(in.real_output_key))
(b)  INCHANGÉ   require od.pubkey == in.real_output_key
(b2) INCHANGÉ   require od.commitment == rct::commit(in.amount, in.mask)
(c)  REMPLACÉ   // binding C-1 : la clé révélée ouvre l'engagement choisi à la création
                require in.auth_type est connu                                 // sinon reject
                auth_commit := Keccak("HRG_BQ_ADDR_AUTH_v1" ‖ in.auth_type ‖ in.auth_pk)
                expect      := Keccak("HRG_PQ_BIND_v2" ‖ in.auth_type
                                       ‖ in.real_output_key ‖ auth_commit ‖ in.auth_blind)
                stored      := bind_tag publié par la tx de CRÉATION de cette sortie,
                               au champ 0x08 dont output_index == index local de la sortie
                require expect == stored
(d)  REMPLACÉ   require ML_DSA_65.Verify(in.auth_pk, pq_in_hash, in.auth_sig)
(d2) INCHANGÉ   require crypto::check_signature(pq_in_hash, in.real_output_key, in.owner_sig)
(e)  INCHANGÉ   conservation monétaire (A3 transparent / terme transparent hybride)
```

`pq_in_hash` : hash du préfixe de transaction avec **toutes** les `auth_sig` et **toutes** les
`owner_sig` mises à zéro. ⚖ **La troncature de `tx.extra` disparaît** (il n'y a plus de champ
`0x06` terminal à retirer) : le message signé ne dépend plus d'une hypothèse de position et de
longueur dans `tx_extra`. Simplification **et** durcissement.

Règle annexe conservée : `check_pq_output_field_indices` (indices `0x07`/`0x08` dans les bornes,
sans doublon, désignant le même ensemble de sorties) — inchangée.

Règle annexe supprimée : « exactement une `tx_extra_pq_sig`, en dernière position » et son
pendant « aucune si pas d'entrée PQ ». Remplacées par : **`tx_extra_pq_sig` (tag `0x06`) n'est plus
jamais accepté**, avec ou sans entrée PQ. ⚖ Rejet explicite plutôt que tolérance : un champ
ignoré est un champ malléable.

### 3.3 Pourquoi (c)+(d) et (d2) sont tous deux nécessaires

C'est le cœur de la spec, et aucun des deux ne peut sauter :

| Attaquant | passe (c)+(d) ? | passe (d2) ? | peut dépenser ? |
|---|---|---|---|
| tiers classique | non | non | non |
| émetteur classique (a `ss`, donc `auth_blind`) | non — il n'a pas de préimage de `auth_commit` | non | **non** |
| tiers quantique (casse `P'`) | non | oui (Shor) | **non** |
| **émetteur quantique** | **non** | oui (Shor) | **non — résiduel CRIT-3 fermé** |
| émetteur ayant engagé **son propre** `auth_commit` | oui (sa propre clé) | non — il lui faut `b` | non |
| ci-dessus **et** quantique | oui | **oui** (Shor) | **oui** → §3.4 |

* **(d2) seul ne suffit pas** : Ed25519 tombe sous Shor.
* **(c)+(d) seuls ne suffisent pas** : rien n'oblige l'émetteur à engager l'adresse du
  *destinataire* plutôt que la sienne (§3.4). C'est (d2) qui empêche l'émetteur classique
  d'exploiter cette liberté.
* **Ensemble** : il faut casser Ed25519 **et** détenir la clé d'identité post-quantique du
  destinataire. Un émetteur quantique a la première, jamais la seconde.

### 3.4 ⚠ Résidu structurel : l'émetteur choisit ce qu'il engage

Le consensus ne connaît pas l'adresse du destinataire — c'est la définition même d'une adresse
furtive. Il ne peut donc **pas** vérifier que `auth_commit` engagé dans le bind tag est bien celui
du destinataire. Un émetteur malveillant peut engager **le sien**, créant une sortie que le
destinataire ne pourra jamais dépenser et que lui-même pourra reprendre s'il casse aussi Ed25519.

**Ce n'est pas un vol** : c'est un paiement qui n'a jamais eu lieu. Mais il doit être **détecté
immédiatement**, sinon le wallet afficherait des fonds inexistants.

**Règle wallet obligatoire (détection au scan).** À la détection d'une sortie BQ, avant de la
compter comme reçue, le wallet **recalcule** `auth_commit` depuis sa propre clé d'identité de la
subaddress identifiée par le `sel_tag`, recalcule `auth_blind` depuis le `ss` qu'il vient de
décapsuler, et **exige** l'égalité avec le bind tag on-chain. À défaut, la sortie est **rejetée**
(ni créditée, ni affichée comme reçue) et signalée. Détection déterministe, au premier scan, avant
toute confiance.

⚠ Ce contrôle **ne peut pas** être porté par le consensus. Il doit donc être re-spécifié pour tout
réimplémenteur de wallet, et testé explicitement (§5.5).

---

## 4. Application de T2 côté wallet — refus actif

T2 est une **règle d'exécution**, pas une recommandation. Ce qu'elle protège : `auth_pk` étant
constante par (sub)adresse, tout ce qui partage une subaddress partage un ensemble de liaison.

### 4.1 État à ajouter

`m_pq_spent_subaddresses` : ensemble des `subaddress_index` **BQ** dont au moins une sortie a
été engagée dans une dépense. Persisté dans le **cache** du wallet, en queue du sérialiseur boost
versionné de `wallet2` (le ladder `if (ver < N) return;`, `wallet2.h:1470-1529`), sous
`BOOST_CLASS_VERSION(tools::wallet2, 32)`. **Pas dans `.keys`** : c'est de l'historique, pas du
matériel de clé — même raison que `m_pq_subaddress_indices` n'y est pas.

⚖ L'entrée est ajoutée **au moment de la construction de la transaction**, pas à sa confirmation.
Une transaction construite puis abandonnée « brûle » la subaddress pour rien ; c'est le prix à
payer pour qu'un plantage ou un double envoi ne puisse pas rouvrir un ensemble de liaison.

### 4.2 Les trois refus

**R-a — ne plus jamais distribuer une subaddress BQ déjà dépensée.**
Point d'application : `wallet2::get_pq_subaddress_as_str` (`wallet2.cpp:1705-1716`), qui porte
déjà le précédent exact — il **lève** `error::password_needed` plutôt que de rendre une adresse
indérivable. Même forme : si l'index est dans `m_pq_spent_subaddresses`, **lever une erreur
explicite**, jamais rendre une chaîne vide ni retomber sur une autre adresse. Une intégration
tierce (wallet-rpc, GUI, bot OTC) reçoit ainsi une erreur, pas un silence.

**R-b — le change d'une dépense BQ va sur une subaddress BQ neuve.**
Point d'application : `wallet2.cpp:10867` (et le chemin jumeau `:10587`), où
`change_dts.addr = get_subaddress({subaddr_account, 0})` aujourd'hui.

> **Constat vérifié, deux défauts distincts selon le compte.** `device_default::get_subaddress`
> (`device_default.cpp:181-195`) renvoie `keys.m_account_address` si l'index est nul, et sinon un
> `account_public_address` construit à partir des seuls `(C, D)` — **sans `pq_kyber_pk`**. Donc :
>
> * **compte `major == 0`** (le cas courant) : le change part sur `{0,0}`, c'est-à-dire l'adresse
>   BQ **primaire**. Il est bien post-quantique, mais **toujours sur le même index**. Sous T1, la
>   moindre dépense de ce change révèle l'`auth_pk` primaire — et fusionne dans un unique ensemble
>   de liaison la quasi-totalité de l'activité du wallet. C'est le pire cas de T1.
> * **compte `major > 0`** : `is_pq()` est faux → **le change est une sortie classique B...**.
>   Dépenser des fonds BQ reconvertit alors silencieusement le reliquat en monnaie vulnérable au
>   quantique. C'est le point laissé ouvert par `design_2b §10`.
>
> Les deux sont corrigés par la même règle, et elle relève de la **sécurité**, pas seulement de la
> non-liabilité.

Règle : pour toute transaction comportant au moins une entrée BQ, le change est dirigé vers une
subaddress **BQ** fraîchement allouée (`add_subaddress` sur le compte courant, puis
`get_pq_subaddress`), jamais `(major, 0)`, jamais un index présent dans
`m_pq_spent_subaddresses`. La subaddress de change est ajoutée à cet ensemble dans le même geste.

**R-c — ne pas fusionner deux subaddresses BQ dans une même dépense.**
Dépenser, dans une même transaction, des sorties reçues sur deux subaddresses BQ distinctes révèle
deux `auth_pk` **dans la même transaction** et fusionne leurs ensembles de liaison. ⚖ Refus par
défaut à la sélection d'entrées, avec **opt-in explicite** de l'appelant (option de transfert), et
message d'erreur nommant les subaddresses concernées.

Cette règle **va au-delà de la lettre de T2**, qui ne mentionnait que la réutilisation et le
change. Elle en découle directement et est retenue au titre de la règle d'arbitrage : sans elle,
la garantie « un ensemble de liaison par demande de paiement » est fausse dès la première
consolidation.

⚠ **Coût d'usage assumé** : un utilisateur dont les fonds sont éparpillés sur plusieurs
subaddresses BQ devra émettre plusieurs transactions, donc payer plusieurs fois les frais, et
pourra se retrouver dans l'impossibilité de payer un montant qu'aucune subaddress ne couvre seule
sans lever l'opt-in. C'est un vrai inconvénient, il est choisi en connaissance de cause.

### 4.3 Point d'arrêt en dernier ressort

`construct_tx_with_tx_key` ne peut pas connaître la politique du wallet, mais il peut refuser
l'incohérence manifeste : **si la transaction comporte une entrée BQ et une sortie de change
non-BQ, échouer**. ⚖ Échec bruyant plutôt que dégradation silencieuse, pour qu'une intégration qui
construit ses transactions elle-même ne puisse pas contourner R-b sans s'en apercevoir.

### 4.4 ⚠ Résidu assumé de T2 (rappel)

Plusieurs sorties reçues **sur la même demande de paiement** (donc la même subaddress) restent
liables entre elles lorsqu'elles sont dépensées. T2 ramène l'ensemble de liaison au niveau
« une demande de paiement », **il ne le supprime pas**. Seule T3 le supprimerait, et T3 est hors
périmètre (`design_2d §4/T3`).

---

## 5. Ce qu'il ne faut pas casser

### 5.1 B3 (décision 4) — non touché, et à re-vérifier explicitement

Le `sel_tag` est calculé depuis `pqc_sel_pad(derivation, output_index)`, où `derivation` est la
dérivation **Ed25519** (`a·R`). Il **n'est pas fonction de `ss`**. Or la seule chose que cette spec
ajoute à la révélation est `auth_blind`, une image à sens unique de `ss`. **Le `sel_tag` reste
donc aveuglé**, et les sorties non dépensées vers une subaddress restent non-liables.

Le champ `tx_extra_kyber_ct` (tag `0x07`, `output_index ‖ sel_tag ‖ ct`) est **inchangé**, ainsi
que la règle `check_pq_output_field_indices`.

### 5.2 CRIT-3 / check (d2) — conservé, et désormais load-bearing

`owner_sig` reste tel quel (`cryptonote_basic.h:194`, `blockchain.cpp:3597`). §3.3 montre qu'il
n'est **pas** redondant avec la nouvelle identité PQ : il est ce qui empêche un émetteur
*classique* d'exploiter la liberté du §3.4. Les deux facteurs sont complémentaires et aucun ne
doit être retiré au motif que l'autre existe.

### 5.3 Format wire — ce qui doit être ré-épinglé

* `tests/unit_tests/test_tx_utils.cpp`, `pq_consensus.txin_to_key_pq_json_valid_and_wire_unchanged`
  (`:405-460`) : la taille pinnée **5479** et le hash Keccak du blob changent. Taille attendue
  `5479 − 5261 + 5294 = **5512**` — **à confirmer par le test lui-même**, pas à inscrire d'avance :
  c'est le rôle du pin. Le commentaire d'historique du test (`:438-443`) doit gagner sa ligne, dans
  le format déjà établi.
* Le même test doit ajouter une assertion de **rejet d'un `auth_type` inconnu** au parse.
* **Cold-sign v4** : `unsigned_tx_set` / `pq_source_ct` **ne changent pas**. Le champ porte déjà
  l'index de subaddress (`wallet2.h:716-733`), ce qui suffit à la machine froide pour redériver la
  clé d'identité, et le `ss` qu'elle recalcule depuis le ciphertext lui donne `auth_blind`. Aucun
  secret supplémentaire ne transite — la propriété centrale du v4 est préservée.
* Le préfixe de fichier `\006` des jeux cold-sign porteurs de PQ reste inchangé.

### 5.4 `pq_vector_test` — ajouts obligatoires

* **Ajouter** un vecteur figé pour la dérivation **ML-DSA-65 par subaddress** (§1.4). C'est un
  nouveau chemin chaud pilotant le hook `randombytes` : si liboqs change son motif de lecture
  d'aléa, les fonds BQ deviennent indépensables. C'est précisément le risque que ce fichier existe
  pour attraper (`docs/audit/liboqs_0.16.0_upgrade_gate.md`).
* **Ajouter** un vecteur pour `auth_commit` et pour `bind_tag` v2 (fonctions de hash pures,
  vecteurs bon marché et stables).
* **Ne pas supprimer** `V_OUT7_MLDSA_PK_KECCAK` (clé ML-DSA par sortie) tant que
  `pqc_keygen_output_dsa` existe : il reste une sentinelle de dérive liboqs. Sa suppression, le
  jour où la fonction disparaît, suit le protocole de `design_2c §6.4` — décision écrite, motif
  consigné dans le fichier, jamais « pour faire passer un test ».

### 5.5 Tests à ajouter

* **Émetteur quantique simulé** : l'attaquant reçoit `x'` par construction (on le lui donne, au
  lieu de simuler Shor) et détient `ss` ; il doit échouer sur (c). C'est le contrôle négatif qui
  prouve que le résiduel de CRIT-3 est fermé.
* **Émetteur ayant engagé son propre `auth_commit`** : le validateur accepte (§3.4), et le
  **wallet du destinataire rejette la sortie au scan**. Sans ce test, la seule défense contre §3.4
  n'est pas couverte.
* **Non-liabilité** : deux sorties non dépensées vers la même subaddress ne partagent aucune donnée
  on-chain corrélable ; après la dépense de l'une, la seconde reste non identifiable (c'est ce que
  `auth_blind` achète, §2.4).
* **T2** : R-a lève une erreur, R-b produit un change BQ sur un index neuf, R-c refuse sans opt-in.
* **Non-régression classique** : une transaction B... reste byte-identique et aucun champ `0x06`
  n'est plus jamais émis ni accepté.

---

## 6. Migration et compatibilité

`HF_HEIGHT_PQ = 2000000` (`cryptonote_config.h:227`), chaîne live en **hf 15**, tip de l'ordre de
8×10⁴. Aucune transaction portant `txin_to_key_pq` ou un champ `0x06`/`0x07`/`0x08` n'est
acceptable avant le fork : **aucune sortie BQ n'a jamais existé sur le mainnet, aucune adresse BQ
n'a jamais été payée.** Il n'y a donc **rien à migrer** — seulement un nouveau format pour toute
adresse et toute sortie BQ future.

Conséquences opérationnelles :
* les adresses BQ de test locales (1249 o) ne parsent plus : la **taille** les rejette, proprement
  et sans ambiguïté (`cryptonote_basic_impl.cpp:401-406`) ;
* les wallets BQ locaux doivent être recréés — troisième invalidation de ce type après la migration
  FIPS (Étape 9) et CRIT-4, et pour la même raison : la fenêtre est ouverte tant que HFv16 ne l'est
  pas ;
* **rien de ce document ne touche la chaîne live** : tout est gardé `hf_version >= HF_VERSION_PQ`
  et/ou par la présence de clés BQ.

⚠ **Cette fenêtre se referme à l'activation de HFv16.** Après, chacun des champs fixés ici —
l'octet `auth_ver`, l'engagement de 32 o, le champ typé d'entrée — coûterait un hard fork
supplémentaire. C'est la raison d'être de T4, et la raison pour laquelle cette spec doit être
implémentée **avant** l'activation, même si T3 ne l'est jamais.

---

## 7. Récapitulatif des arbitrages et des résidus

**Arbitrages tranchés côté sécurité / anonymat (⚖)**
1. `auth_ver` placé avant tout champ interprétable, au prix d'un décalage d'offsets (§1.2).
2. `auth_ver` inclus dans le préimage de `auth_commit`, contre une séparation de domaine implicite (§1.3).
3. `auth_blind` dédié et à sens unique, +32 o par entrée, plutôt que révéler `ss` (§2.4).
4. Corps de taille fixe par type, pas de longueur auto-descriptive, type inconnu rejeté (§2.2).
5. `0x06` rejeté en toutes circonstances plutôt que toléré (§3.2).
6. Subaddress marquée comme dépensée à la **construction**, pas à la confirmation (§4.1).
7. R-c (pas de fusion de subaddresses BQ) ajoutée au-delà de la lettre de T2, au prix de frais et
   de confort (§4.2).
8. Échec bruyant de `construct_tx` sur un change non-BQ d'une dépense BQ (§4.3).

**Résidus documentés (⚠)**
1. L'émetteur choisit ce qu'il engage ; non vérifiable par le consensus, **détecté au scan par le
   wallet**, obligatoirement (§3.4).
2. Les sorties reçues sur une **même demande de paiement** restent liables entre elles à la
   dépense ; seule T3 le supprimerait (§4.4).
3. L'espace des valeurs de `auth_ver` est contraint par la stabilité du préfixe « BQ », à vérifier
   empiriquement pour chaque valeur (§1.2).
4. Le coût d'usage de R-c : plusieurs transactions, plusieurs frais, opt-in nécessaire pour
   consolider (§4.2).
5. La dérivation ML-DSA par subaddress dépend du motif de lectures d'aléa de liboqs, faute de
   keygen dérandomisé pour SIG ; épinglage obligatoire (§1.4, §5.4).

**Reste bloquant pour HFv16, hors de ce document** : **M-11** (maturité liboqs) et le budget
`tx_extra` à re-mesurer une fois `0x06` retiré (`design_2d §5` estime 6 à 7 sorties BQ par dépense
au lieu de 2 — **estimation, à mesurer**, comme l'a imposé la correction de `design_2b §9`).


---

## 8. Livraison (12 septembre 2026)

### 8.1 Les chiffres, mesurés et non recopiés

| Grandeur | Annoncé ici | Mesuré | Verdict |
|---|---|---|---|
| Charge utile d'adresse BQ | 1282 o | **1282 o** | conforme |
| Adresse BQ rendue | 1770 caractères | **1770** (`pq_vector_test`) | conforme |
| Pin wire `txin_to_key_pq` | 5512 o « à confirmer » | **5512 o** (`test_tx_utils`) | conforme |
| Sorties BQ par dépense transparente | « de l'ordre de 6 à 7, à mesurer » | **7** (`pq_subaddress_test`, `construct_tx` réel) | conforme, au haut de la fourchette |

Le plafond d'une dépense BQ passe donc de **2 à 7** sorties : la contrainte que `design_2b §9`
avait mesurée disparaît, et payer deux destinataires BQ depuis un wallet BQ redevient possible.
Les trois valeurs ont été obtenues d'un test avant d'être écrites où que ce soit.

### 8.2 La vérification empirique exigée au §1.2

`pq_address_test::test_bq_prefix_is_stable` rend **2000 charges utiles aléatoires par marker**
(`0x33` primaire et `0x35` subaddress) avec `auth_ver = 0x01` : le préfixe « BQ » tient dans les
4000 cas. La valeur `0x01` est donc allouée. Le test est permanent, pas un script jeté : toute
future valeur de `auth_ver` devra y passer avant d'être allouée.

### 8.3 Deux défauts latents trouvés en implémentant

1. **`cryptonote_boost_serialization.h` — `txin_to_key_pq` amputé.** Le sérialiseur boost n'avait
   jamais été étendu quand l'entrée a gagné `mask` (CRIT-2) puis `owner_sig` (CRIT-3). Or
   `transfer_details::m_tx` est un `transaction_prefix`, donc une dépense BQ enregistrée dans un
   **cache de wallet** revenait sans son masque ni sa signature de propriétaire. Inerte — aucune
   transaction BQ n'a jamais existé — mais la première aurait été silencieusement corrompue.
   Tous les champs y sont désormais, avec la consigne de ne plus en oublier.
2. **`account_public_address::pq_auth_commit` non réhydraté au rechargement.** Détecté par
   `pq_root_restore_test` (store/reload) : un wallet BQ rouvert rendait une adresse **différente**
   de celle qu'il avait distribuée. Corrigé au même endroit que la réhydratation de `pq_kyber_pk`,
   depuis la clé publique d'identité en clair.

### 8.4 Écarts assumés par rapport à la lettre de la spec

* **§4.3 déplacé dans `construct_tx`, pas dans `verify_pq_tx_well_formed`.** Le premier jet
  cherchait le change parmi `dests` par heuristique (`amount > 0 && original.empty()`), ce qui est
  du devinage. `construct_tx_with_tx_key` reçoit `change_addr` **explicitement** : le refus y est
  sans ambiguïté. C'est aussi ce que la spec disait ; l'heuristique était un raccourci, retiré.
* **`confirm_pq_output` a gagné un paramètre `require_binding` explicite.** La machine froide
  reçoit un ciphertext sans la transaction créatrice : elle ne **peut pas** rejouer le contrôle du
  §3.4. Plutôt qu'un contournement implicite (« pas de bind tag, on laisse passer »), le seul
  appelant concerné le demande par son nom, et le paramètre porte la justification.
* **`pqc_keygen_output_dsa` et `pqc_compute_bind_tag` (v1) conservés.** Plus aucun appelant de
  consensus, mais `pq_vector_test` les épingle comme sentinelles de dérive liboqs, générées sur
  0.15.0 — les supprimer affaiblirait la garde d'upgrade pour rien. Marqués comme tels.

### 8.5 Tests

**Suite standalone : 15 tests, tous verts** (nouveau : `pq_auth_binding_test`). Ce qui est prouvé
et ne l'était pas :

* **Le résiduel de CRIT-3 est fermé** (`pq_sender_clawback_test`) : un émetteur à qui l'on
  **donne** `x'` — exactement ce que Shor lui rendrait — et qui détient `ss`, satisfait
  (b)(b2)(d)(d2)(e) et **échoue sur (c) seul**. Contrôle négatif explicite.
* **Le résidu du §3.4, ses deux moitiés** (`pq_auth_binding_test`) : le validateur accepte une
  sortie engagée vers l'engagement de l'émetteur, **et** le wallet du destinataire la refuse au
  scan (jamais créditée), avec un contrôle positif montrant que le même paiement honnête est bien
  crédité.
* **La non-liabilité que l'aveuglement achète** : deux sorties vers une même subaddress ne
  partagent rien on-chain, et l'aveuglement révélé par la dépense de l'une ne permet pas de
  recalculer le tag de l'autre.
* **T2** : R-a lève une erreur explicite, une subaddress fraîche n'est jamais `(major,0)`, et
  `construct_tx` refuse une dépense BQ dont le change n'est pas BQ.
* **Rejet d'un `auth_type` inconnu au parse** et **absence définitive du champ `0x06`**.

Vecteurs figés ajoutés : clé d'identité ML-DSA-65 de la subaddress (3,9) (pk et sk),
`auth_commit`, `auth_blind`, `bind_tag` v2. Les sentinelles liboqs (`V_RAW_*`, `V_OUT7_*`,
`V_BIND_TAG` v1) et les empreintes de clés de compte sont revenues **identiques au bit près** —
c'est ce qui atteste que seul l'ENCODAGE d'adresse a bougé, aucune dérivation.

### 8.6 Ce qui reste

Inchangé : **T3** (identité aveuglée) reste une direction de recherche ; sa place est réservée
(`auth_ver`, champ d'entrée typé) et son ajout ne coûtera pas de second hard fork. **M-11**
(maturité liboqs) reste le blocage restant avant HFv16. Les cinq résidus du §7 tiennent tels
quels.
