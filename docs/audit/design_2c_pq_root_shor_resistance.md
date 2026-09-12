# Design 2c — La racine post-quantique face à Shor (CRIT-4), et le résiduel quantique de CRIT-3

**Date :** 12 septembre 2026 — suite des items 2a / 2b, déclenché par CRIT-4 (ouvert au
11 septembre) et par le résiduel de CRIT-3 (§5 de son rapport).
**Statut : ✅ PARTIELLEMENT DÉCIDÉ ET IMPLÉMENTÉ le 12 septembre 2026.** La question 1 (la
racine) est tranchée — **option R2a**, deux mnémoniques — et livrée ; voir §8 en fin de
document et `CRIT-4_pq_root_was_the_spend_key_2026-09-12.md`. Les questions 3 (S1b, le résiduel
quantique de CRIT-3) et 4 à 6 restent ouvertes, délibérément : S1b dérive sa clé de la racine,
donc elle devait passer **après**. §0 à §7 sont le rapport d'origine, conservé tel quel.

## 0. Le problème

Deux findings ouverts, une seule cause.

* **CRIT-4** (`account.h:236-245`, `account.cpp:305-308`) : toute clé BQ — la paire primaire
  (M-4) comme chaque paire ML-KEM de subaddress (décision 4) — dérive de
  `get_pq_root_secret(keys)`, qui **renvoie `keys.m_spend_secret_key`**. Or la clé publique
  correspondante `B = b·G` est publiée dans l'adresse BQ elle-même
  (`cryptonote_basic_impl.cpp:375`). Un adversaire capable d'exécuter Shor sur `B` retrouve `b`,
  donc la racine, donc **toutes** les clés PQ du compte, présentes et futures.
* **CRIT-3 résiduel** (`CRIT-3_sender_can_reclaim_bq_output_2026-09-11.md` §5) : le check **d2**
  (`blockchain.cpp:3586-3601`) est une signature **Ed25519** par la clé unique `x'` de la sortie
  dépensée. `P' = x'·G` est révélée par la forme même de la dépense transparente
  (`txin_to_key_pq.real_output_key`, `cryptonote_basic.h:172`) : Shor la casse. Un émetteur
  quantique, qui détient déjà `ss` par sa propre encapsulation, redevient capable de redépenser
  la sortie qu'il a payée.

Dans les deux cas, la sécurité post-quantique de HIDERING est ancrée dans une clé dont la
contrepartie publique est **déjà publiée sur la chaîne ou dans l'adresse**. C'est le même angle
mort que celui de la spec de binding **C-1**, encore à finaliser : les trois doivent être tranchés
ensemble, avant toute activation de HFv16.

Ce rapport répond à une question précise — *une racine PQ dérivée directement de la seed par un
domaine de hash séparé suffirait-elle ?* — et la réponse, **vérifiée dans le code de ce dépôt,
est non**. §1 et §2 établissent pourquoi ; §3 à §6 en tirent les conséquences.

---

## 1. Ce que la seed 25 mots produit aujourd'hui — chemin exact

### 1.1 Le chemin, fichier par fichier

| Étape | Où | Ce qui se passe |
|---|---|---|
| mots → 32 octets | `simplewallet.cpp:4117` → `electrum-words.cpp` | `words_to_bytes(m_electrum_seed, m_recovery_key)` ; `seed_length = 24` (`electrum-words.h:63`) + 1 mot de checksum = 25 mots = **32 octets** |
| offset optionnel | `simplewallet.cpp:4127-4129` | si l'utilisateur a une *seed offset passphrase* : `m_recovery_key = decrypt_key(m_recovery_key, seed_pass)` |
| 32 octets → clé de dépense | `account.cpp:187` → `crypto.cpp:153-172` | `generate_keys(pub, sec, recovery_key, recover=true)` : `sec = recovery_key` puis **`sc_reduce32(sec)`** |
| clé de dépense → clé de vue | `account.cpp:190-193` | `second = keccak(m_spend_secret_key)` puis `generate_keys(..., second, recover=true)` → `a = sc_reduce32(keccak(b))` |
| clé de dépense → mots | `wallet2.cpp:1450-1453` | `get_seed` : `key = m_spend_secret_key` (+ offset si passphrase) puis `bytes_to_words(key, ...)` |

### 1.2 Réponse à la question 1 : `m_spend_secret_key` n'est **pas** un hash de la seed

`generate_keys` (`crypto.cpp:159-166`) ne fait **aucun hachage** sur le chemin `recover=true` :

```c
if (recover) { rng = recovery_key; } else { random_scalar(rng); }
sec = rng;
sc_reduce32(&unwrap(sec));   // seule transformation : réduction mod l
```

Et `get_seed` réencode littéralement `m_spend_secret_key` en mots. Donc :

> **La seed 25 mots n'est pas un ancêtre de la clé de dépense : elle EST la clé de dépense**,
> à la réduction modulaire près. Les deux directions sont calculables sans aucun secret :
> `mots → b` par `words_to_bytes` + `sc_reduce32`, et `b → mots` par `bytes_to_words`.

La clé de vue, elle, est bien en aval : `a = reduce(Keccak(b))`. C'est une relation **à sens
unique côté hash**, mais qui ne protège rien ici, puisque c'est `b` — la source — qui tombe.

### 1.3 Les 32 octets, et qui peut les atteindre

```
             Shor sur B (publiée dans toute adresse B... et BQ...)
                              │
                              ▼
  25 mots  ══════════════  b = m_spend_secret_key  ══════════════  mots
  (bytes_to_words est          │        │                     (identité)
   une bijection)              │        └─ Keccak ─► a = m_view_secret_key
                               │
                               └─ SHAKE256("HRG_PQ_KEM_v1"‖b) ─► ML-KEM-768 du compte
                               └─ SHAKE256("HRG_PQ_DSA_v1"‖b) ─► ML-DSA-65 du compte
                               └─ SHAKE256("HRG_BQ_SUBADDR_KEM_v1"‖b‖major‖minor) ─► subaddresses BQ
```

(`pqc.cpp`, `pqc_keygen_from_seed` et `pqc_kem_keygen_subaddress`.)

---

## 2. L'hypothèse soumise — vérification, et pourquoi elle s'effondre ici

> *Une racine PQ dérivée DIRECTEMENT de la seed par un domaine de hash séparé (parallèle à la
> chaîne Ed25519, pas à travers elle) survivrait à la compromission de `m_spend_secret_key` par
> Shor, parce que la résistance aux préimages d'un hash ne tombe pas sous Shor.*

### 2.1 Le raisonnement cryptographique, en soi, est juste

Shor résout le logarithme discret et la factorisation ; il ne s'applique pas à l'inversion d'une
fonction de hachage. Le meilleur algorithme quantique générique contre une préimage est Grover,
gain **quadratique** : 2¹²⁸ opérations quantiques pour un hash 256 bits — hors de portée, et
c'est exactement le budget de sécurité que ML-KEM-768 / ML-DSA-65 visent (catégorie NIST 3).
Sur le principe, ancrer la racine PQ derrière SHAKE256 est la bonne idée. **Ce n'est pas le
raisonnement qui est faux, c'est sa prémisse.**

### 2.2 La prémisse est fausse dans ce dépôt : « parallèle » et « à travers » désignent le même point

L'hypothèse suppose implicitement qu'il existe, en amont, une *entropie de seed* distincte de la
clé de dépense — comme dans BIP-39 (entropie → PBKDF2 → master seed → clés) ou polyseed. Ici,
§1.2 : il n'y en a pas. La « seed » et la clé de dépense sont les mêmes 32 octets.

La chaîne d'attaque est donc :

```
B  ──Shor──►  b  ──(identité)──►  la seed  ──H("domaine quelconque" ‖ seed)──►  racine PQ
```

Un hash n'offre aucune protection quand l'adversaire détient déjà son **entrée**. Dériver
« en parallèle » depuis la seed plutôt qu'« à travers » la clé de dépense ne déplace rien : les
deux branches partent du même nœud, et ce nœud est précisément celui que Shor obtient. La
formulation actuelle `SHAKE256("HRG_PQ_KEM_v1" ‖ b)` **est déjà** une dérivation parallèle
domain-séparée depuis la seed — le commentaire de `account.cpp:300-304` le dit d'ailleurs
exactement ainsi (« the spend key flows through domain-separated SHAKE256, so it is never
recoverable from the PQ material »). Cette affirmation est vraie **dans le sens qui n'intéresse
pas l'attaquant** : elle protège `b` depuis les clés PQ, pas les clés PQ depuis `b`.

> **Conclusion 2 : l'hypothèse s'effondre. Aucune redérivation à partir du matériel de seed
> existant ne peut être résistante à Shor. Fermer CRIT-4 exige de l'entropie qui n'existe pas
> aujourd'hui dans le wallet.**

### 2.3 La seule entropie indépendante aujourd'hui : la *seed offset passphrase* — et pourquoi elle ne fait pas une solution

Il existe littéralement un secret que Shor ne donne pas : la passphrase d'offset
(`cryptonote_format_utils.cpp:1762-1768`).

```c
crypto::secret_key encrypt_key(crypto::secret_key key, const epee::wipeable_string &passphrase)
{ cn_slow_hash(passphrase…, hash); sc_add(key, key, hash); return key; }
```

Les mots affichés valent `b + H_cn(p)` ; la clé stockée reste `b`. Un adversaire qui obtient `b`
par Shor **ne peut pas** recalculer les mots sans `H_cn(p)`. Une racine PQ dérivée de la *valeur
des mots* serait donc formellement hors de sa portée. Trois raisons de rejeter cette piste :

1. **Elle est optionnelle et vide par défaut** (`simplewallet.cpp:4128` : `if (!seed_pass.empty())`).
   La sécurité post-quantique du compte deviendrait une conséquence d'un choix d'interface
   utilisateur.
2. **L'entropie est humaine.** `cn_slow_hash` ralentit une recherche exhaustive, il ne crée pas
   d'entropie : une passphrase typique reste à portée d'une attaque par dictionnaire, a fortiori
   d'un adversaire déjà capitalisé sur un ordinateur quantique.
3. **Décisif : le wallet ne conserve pas l'offset.** `seed_pass` est une variable locale
   redemandée à chaque usage (`simplewallet.cpp:900-912` pour l'affichage, `4020-4129` pour la
   restauration) ; aucun membre de `wallet2` ne la stocke. Une racine dérivée de la valeur des
   mots ne serait donc **pas calculable** après la création du wallet sans reprompter
   l'utilisateur à chaque dérivation de subaddress BQ. Inexploitable.

---

## 3. Options pour la racine — où loger la nouvelle entropie

L'axe de décision n'est pas « quel hash », c'est **où vit l'entropie indépendante et comment
l'utilisateur la sauvegarde**. Trois réponses distinctes.

### R0 — L'hypothèse telle quelle : nouveau domaine de hash sur la seed existante ❌
Rejetée par le §2. À citer explicitement dans la décision, pour que personne ne la re-propose :
c'est un changement qui *ressemble* à un correctif, casse le vecteur figé et les adresses BQ
existantes, et **n'apporte aucune sécurité**. Le pire rapport coût/bénéfice de la liste.

### R1 — Racine indépendante, tirée au hasard à la création, stockée dans `.keys` seulement
`get_pq_root_secret` renvoie un nouveau membre `account_keys::pq_root`, tiré par
`random_scalar` (ou 32 octets de `generate_random_bytes_thread_safe`) à la création du wallet
BQ, chiffré at-rest comme les autres secrets.

* **Le moins de code.** Le format `.keys` l'absorbe sans rupture (§6.1) ; aucune interface
  utilisateur nouvelle ; aucun mot supplémentaire à faire recopier.
* **Rédhibitoire : la restauration depuis les 25 mots ne reproduit plus l'adresse BQ.** C'est
  très exactement **M-4 recréé** (audit du 10 juin, clos le 14 juin par `1473779b6`) : la seed ne
  suffit plus, la sauvegarde devient le fichier. Une perte de fichier = fonds BQ perdus, alors
  que l'utilisateur a une seed en main et croit être couvert. **À écarter comme design
  principal**, sauf à assumer que les wallets BQ ne sont pas restaurables depuis la seed — ce qui
  contredit frontalement la décision M-4.

### R2 — L'entropie entre dans le matériel de seed, **sans toucher** à la dérivation classique ⭐ recommandée
Les 24+1 mots continuent d'encoder `b`, exactement comme aujourd'hui. Un **second morceau de
seed**, indépendant, porte la racine PQ :

```
racine_PQ = SHAKE256("HRG_PQ_ROOT_v1" ‖ S_pq)     avec S_pq = 16 ou 32 octets d'aléa frais
```

Deux emballages possibles, à trancher :

* **R2a — deux mnémoniques.** Les 25 mots classiques, inchangés, plus une « seed BQ » séparée
  (13 mots pour 16 octets, ou 25 pour 32). *Avantage décisif* : un binaire publié
  (v2.0.3, GUI v2.0.2-gui) restaure toujours correctement la moitié classique depuis les 25
  mots. Coût : deux artefacts à sauvegarder, et une consigne utilisateur à écrire noir sur blanc.
* **R2b — une mnémonique étendue.** Un seul jeu de mots plus long (p. ex. 37 ou 49 mots), dont
  les 32 premiers octets sont `b` à l'identique. Un seul artefact. Un ancien binaire **refuse
  proprement** la seed étendue — `words_to_bytes` teste `word_list.size() != seed_length + 1`
  (`electrum-words.cpp:479`) — donc pas de restauration silencieusement fausse, mais pas de
  restauration du tout sur les binaires publiés.

Dans les deux cas : **l'adresse B... d'un même jeu de 24+1 mots ne change jamais**, la
dérivation `crypto.cpp:153` n'est pas touchée, et aucun wallet classique existant n'est concerné.

### R3 — Redéfinir la seed existante en *master entropy*
Les 25 mots cessent d'encoder `b` et encodent `S` ; tout en dérive :

```
b         = sc_reduce32(Keccak("HRG_SPEND_v1" ‖ S))
racine_PQ = SHAKE256("HRG_PQ_ROOT_v1" ‖ S)
```

C'est l'architecture cryptographiquement la plus propre (un seul artefact, `b` devient l'image
d'un hash, donc Shor sur `B` rend `b` et s'arrête là) et c'est la seule version de l'hypothèse
d'origine qui *fonctionne*. Son coût est un piège de format sérieux :

* **25 mots anciens et 25 mots nouveaux sont indiscernables.** Restaurer une seed R3 sous les
  règles actuelles (ou l'inverse) produit **un wallet différent, valide, vide** — la classe de
  bug la plus coûteuse en support et la plus alarmante pour un utilisateur.
* Le drapeau `--bq-wallet` (`simplewallet.cpp:4652`, `4830`) désambiguïse en pratique… mais
  seulement si l'utilisateur s'en souvient : un wallet BQ possède **aussi** une adresse B..., qui
  différerait alors selon le drapeau passé à la restauration.
* Rendre la seed auto-descriptive (compte de mots différent, octet de version volé à l'entropie,
  liste de mots dédiée) est obligatoire — et **une fois cette contrainte acceptée, R3 converge
  vers R2b**, à une différence près : R3 change la dérivation de `b`, R2b non. Cette différence
  n'apporte rien en sécurité (`b` reste retrouvable par Shor dans les deux cas, c'est
  intrinsèque : `B` doit être publiée pour que les adresses furtives Ed25519 fonctionnent) et
  coûte toute la compatibilité.

### Recommandation

**R2**, sous-variante **R2a** (deux mnémoniques) sauf objection produit. R2 est la seule option
qui satisfait simultanément : racine hors de portée de Shor, restauration depuis la seed
préservée (M-4 tenu), dérivation classique et binaires publiés intacts, et échec *fail-closed*
sur les anciens outils. R3 est défendable si l'on accepte d'emblée un format de seed versionné —
décision de produit, pas de cryptographie.

### Bénéfice collatéral de R1/R2/R3, à ne pas rater : la clé de scan BQ view-only

`design_2b §8.1` refusait de dériver les clés ML-KEM de la clé de vue `a`, au motif qu'« une
dépense BQ transparente est autorisée par le secret partagé ML-KEM » — donner la clé ML-KEM à un
détenteur de clé de vue lui aurait donné le pouvoir de dépense. **Ce n'est plus vrai depuis le
check d2** : une dépense exige désormais `x' = H_s(d‖i) + b (+m) + t`, donc la clé de dépense `b`,
que `ss` ne donne pas. Découpler la racine de `b` permet donc, si on le souhaite, d'exporter la
**racine PQ seule** avec la clé de vue : un wallet view-only pourrait enfin détecter les sorties
BQ sans pouvoir les dépenser. Cela fermerait le premier item ouvert de `design_2b §10`. À
confirmer formellement avant de le promettre, mais c'est un argument de plus pour une racine
distincte.

---

## 4. Restauration depuis la seed (question 3)

### 4.1 La contrainte « avant toute lecture de la blockchain » est déjà satisfaite

Vérifié dans `wallet2::generate` (`wallet2.cpp:6241-6290`), qui sert **à la fois** la création et
la restauration (paramètre `recover`) :

1. `m_account.generate(recovery_param, recover, two_random)` (`:6253`) — clés Ed25519 ;
2. `cryptonote::generate_pq_keys(...)` (`:6260`) — clés BQ, **fonction pure de la clé de dépense** ;
3. `setup_keys` (`:6266`), `create_keys_file` (`:6273`), `setup_new_blockchain` (`:6275`) ;
4. `update_pq_subaddresses()` sous `wallet_keys_unlocker` (`:6280-6283`) — table d'empreintes ;
5. `estimate_blockchain_height` (`:6270`) et le refresh **ensuite**.

Tout le matériel PQ est donc produit avant le moindre accès daemon, et n'importe laquelle des
options R1/R2/R3 s'insère au même endroit : il suffit que `pq_root` soit disponible au point 2.
Pour R2, la seed BQ doit être saisie **au même écran** que les 25 mots — pas plus tard.

### 4.2 Par option

| | Restauration depuis la seed reproduit l'adresse BQ ? | Ce que l'utilisateur doit conserver |
|---|---|---|
| R1 | **Non** — M-4 recréé | le fichier `.keys` (les 25 mots ne suffisent plus) |
| R2a | Oui | 25 mots + seed BQ |
| R2b | Oui | une mnémonique étendue |
| R3 | Oui | 25 mots (nouveau format) |

### 4.3 Deux points d'interface à régler quelle que soit l'option

* **Rien n'enregistre « ce compte est BQ » dans la seed.** Aujourd'hui la restauration exige que
  l'utilisateur repasse `--bq-wallet` (`simplewallet.cpp:4652`) ; sans le drapeau il obtient un
  wallet classique valide et ses fonds BQ restent invisibles. R2 corrige ça **gratuitement** : la
  présence d'une seed BQ *est* le signal. Pour R1 et R3, il faut une marque explicite.
* **Wallets sans clé de dépense.** Un wallet view-only (`create_from_viewkey`,
  `account.cpp:270-276`, clé de dépense mise à zéro) et un wallet matériel
  (`create_from_device`, `:241-266`, dont la clé de dépense ne sort pas d'un vrai périphérique)
  n'ont aujourd'hui **aucune racine PQ utilisable**. Une racine indépendante rend ces cas
  *représentables* (cf. §3, bénéfice collatéral) — c'est une clarification, pas une régression,
  mais il faut décider si BQ + matériel doit être supporté du tout.

---

## 5. CRIT-3 résiduel : remplacer la racine ne suffit pas (question 4)

### 5.1 Réponse directe : non — mais ce n'est pas sans effet

`x'` ne dérive **pas** de la racine PQ. Sa formule est
`x' = H_s(d ‖ i) + b (+ m pour une subaddress) + t`, où `t = derive_bq_output_tweak(ss, i)`
(`cryptonote_tx_utils.cpp:113-127`) et `b` est la clé de dépense **classique**. Un adversaire
quantique n'a même pas besoin de la reconstruire : `P' = x'·G` est publiée en clair par
`txin_to_key_pq.real_output_key` et Shor la renverse directement. Changer la racine PQ ne touche
pas une ligne de ce chemin.

En revanche, la matrice de CRIT-3 §5 **bouge sur une ligne** :

| Attaquant | A `ss` ? | A `x'` ? | Aujourd'hui | Après correction de la racine seule |
|---|---|---|---|---|
| tiers classique | non | non | non | non |
| émetteur classique | oui | non | non (d2) | non (d2) |
| **tiers quantique** | **oui** (racine = `b`, donc `kyber_sk`) | oui (Shor) | **oui — vol** | **non** ✅ |
| émetteur quantique | oui (encapsulation) | oui (Shor) | oui | **oui — inchangé** ❌ |

C'est un gain réel : aujourd'hui, CRIT-4 rend le « tiers quantique » aussi dangereux que
l'émetteur, parce que `b` lui livre `kyber_sk` donc `ss` donc la clé ML-DSA par sortie (checks
*c* et *d*). Corriger la racine ramène cette ligne à « non ». Le cas émetteur, lui, reste ouvert :
`ss`, il l'a obtenu de sa propre encapsulation, sans aucune clé secrète.

### 5.2 Ce qu'il faut de plus, pour `x` lui-même

Fermer le dernier cas exige qu'une **clé post-quantique détenue par le seul destinataire** entre
dans l'autorisation de dépense. L'argument d'impossibilité de CRIT-3 §5 tient : au moment de créer
la sortie, l'émetteur doit publier le bind tag engageant la clé qui signera la dépense ; les clés
ML-DSA ne sont pas homomorphes, donc si l'émetteur sait calculer cette clé publique à partir de ce
qu'il détient, il en calcule aussi la clé secrète. La seule échappatoire est que cette clé
publique **vienne de l'adresse**, et non d'un secret partagé.

#### S1 — Clé ML-DSA-65 par (sub)adresse, publiée dans l'adresse BQ
Le format d'adresse passe de `marker ‖ B ‖ A ‖ kem_pk` (1249 o de charge utile,
`cryptonote_basic_impl.cpp:361-378`) à `… ‖ dsa_pk` : **+1952 o → 3201 o**, soit une adresse
base58 d'environ 4400 caractères (contre 1725 aujourd'hui). Le bind tag engage `P'` vers cette
clé sous un aveuglement dérivé de `ss`, pour que rien ne soit lié **à la création**.

#### S1b — Le même schéma, mais l'adresse ne publie qu'un engagement de 32 octets ⭐
Raffinement de la piste esquissée dans CRIT-3 §5, et nettement moins cher :

* l'adresse publie `Q_h = Keccak("HRG_BQ_ADDR_DSA_v1" ‖ dsa_pk)` — **+32 o** seulement
  (charge utile 1281 o, adresse ≈ 1770 caractères) ;
* à la dépense, l'entrée révèle `dsa_pk` (1952 o) et signe ;
* **et surtout : la clé ML-DSA par sortie devient redondante.** `txin_to_key_pq.dsa` porte déjà
  `pk(1952) ‖ sig(3309) = 5261 o` (`cryptonote_basic.h:195`) ; la clé d'adresse **prend sa
  place** au lieu de s'y ajouter. Le coût en taille d'entrée est **nul**, et le coût en
  `tx_extra` est nul aussi (tout vit dans `vin`, donc le budget mesuré en `design_2b §9` —
  7 sorties BQ en paiement, 2 en dépense — n'est pas affecté).

Le prix à payer n'est donc pas la taille, c'est la **liabilité** : `dsa_pk` est constante par
subaddress, donc deux dépenses de sorties reçues sur la même subaddress deviennent reliables par
un observateur. C'est une régression de confidentialité réelle par rapport au classique, où les
dépenses sont non-liables même au sein d'une subaddress.

*Atténuation praticable* : les subaddresses BQ sont désormais gratuites et redérivées à la
demande (≈17 µs, `design_2b §8.4`), et le tag de sélection aveuglé rend le scan **O(1)** quel que
soit leur nombre. La consigne « une subaddress par demande de paiement » — déjà la bonne pratique
Monero — réduit la fuite à « deux sorties du même payeur », c'est-à-dire à ce que le payeur
savait déjà. À décider si cette atténuation est jugée suffisante, ou si elle doit être **imposée**
par le wallet (refuser de réutiliser une subaddress BQ ayant déjà servi à une dépense).

#### S2 — Garder les deux clés (par sortie **et** par adresse)
`+5261 o par entrée`, aucune amélioration de la liabilité par rapport à S1b (c'est la clé
d'adresse révélée qui lie, qu'il y en ait une autre à côté ou non). Le seul gain est de ne pas
toucher au binding existant — un argument de churn, pas de sécurité. À ne retenir que si la revue
du binding C-1 conclut que la clé par sortie porte une propriété que S1b perdrait.

#### S3 — Preuve de connaissance de la clé de décapsulation ML-KEM
La solution idéale sur le papier : la clé publique ML-KEM du destinataire **est déjà dans
l'adresse** et déjà transmise au payeur seul. Si le dépensier pouvait prouver en zero-knowledge
qu'il connaît la clé de décapsulation correspondante, on obtiendrait l'autorisation détenue par
le seul destinataire, **sans croissance d'adresse et sans liabilité**. Ce n'est pas disponible :
aucune primitive de ce type dans liboqs, cela suppose une preuve ZK sur réseaux euclidiens,
c'est-à-dire de la cryptographie non standardisée. **À documenter comme direction de recherche,
pas comme option livrable** — et à garder en tête, car elle rendrait S1b caduque.

*Variante de taille sur S1b* : remplacer ML-DSA-65 par une signature à base de hachage change le
compromis sans changer l'analyse — SPHINCS+-128s a une clé publique de 32 o (l'adresse porterait
la clé elle-même, plus d'engagement) mais une signature de ~7,8 ko ; LMS/XMSS descend à ~2-3 ko
au prix d'un schéma **à état**, dont la réutilisation de feuille après une restauration depuis la
seed est une perte de clé silencieuse. À écarter pour cette dernière raison, sauf étude dédiée.

### 5.3 Ordre imposé : CRIT-4 est un **préalable** à toute correction de CRIT-3

Point à ne pas manquer. Dans S1/S1b, la clé ML-DSA d'adresse est dérivée de la racine PQ. **Si la
racine reste `b`, un émetteur quantique la dérive lui aussi** — il casse `B`, obtient la racine,
reconstruit `dsa_pk` *et* sa clé secrète, et signe la dépense. S1b deviendrait du théâtre de
sécurité, exactement au sens où l'enforcement DNSSEC sans zone signée en était (cf. M-6).

> **L'ordre n'est pas négociable : racine d'abord (§3), autorisation ensuite (§5.2).** Et comme
> les deux modifient le format d'adresse BQ et le format d'entrée, il faut les poser **dans le
> même geste**, avec la finalisation du binding C-1 — après HFv16, chacun serait un hard fork
> supplémentaire.

---

## 6. Coût (question 5)

### 6.1 Format du fichier `.keys` : **aucun bump de version nécessaire**

Le `.keys` sérialise `account_keys` via un **KV map epee à champs nommés**
(`account.h:74-146`), pas un format positionnel. Le précédent est déjà là : `pq_keys` et
`pq_dilithium` sont écrits **uniquement si présents** (`account.h:95-100`, `128-131`), et un
wallet classique n'émet aucun de ces champs — d'où la propriété « byte-identique » des wallets
B.... Un nouveau champ optionnel `pq_root` suit exactement le même moule : absent = dérivation
historique, présent = nouvelle racine. Pas de dual-parse, pas de rupture.

Trois points d'attention, tous mécaniques :

1. **`xor_with_key_stream` est positionnel** (`account.cpp:87-120`). Le flux chacha20 est dérivé
   pour `2 + m_multisig_keys.size()` clés Ed25519, puis `kyber_sk`, puis `dilithium_sk`. Une
   racine chiffrée doit être **ajoutée en fin de flux** et sa longueur intégrée à `pq_sk_bytes`
   (`:96-99`), sinon tout `.keys` BQ existant se déchiffre en bouillie. (Aucun n'existe en
   production — §6.3 — mais les wallets de test locaux, si.)
2. **`account_boost_serialization.h:144`** : `BOOST_CLASS_VERSION(cryptonote::account_keys, 1)`
   → passer à 2 et sérialiser la racine sous garde de version. Rappel du 8 septembre : ce
   sérialiseur **n'a aucun appelant** ; c'est de l'hygiène, pas un chemin chaud.
3. **Cold-sign** : `unsigned_tx_set` v4 (préfixe fichier `\006`) **n'a jamais été publié dans un
   binaire** → libre de changer. La machine froide charge sa racine de son propre `.keys`, donc
   S1b n'ajoute rien à transporter ; seul le format d'adresse BQ des destinations évolue.

### 6.2 Le vrai changement de format, c'est la **seed**, pas le fichier

R2 ajoute un artefact de sauvegarde ; R3 redéfinit le sens des 25 mots. C'est là que se
concentrent le risque utilisateur et le travail de documentation (site, whitepaper §6, README,
messages de `simplewallet`), pas dans la sérialisation.

### 6.3 Migration des comptes BQ déjà utilisés : **il n'y en a pas** — confirmé

* `HF_HEIGHT_PQ = 2000000ULL`, `HF_VERSION_PQ = 16` (`cryptonote_config.h:226-227`) ; la chaîne
  live est en hf 15, tip de l'ordre de 8×10⁴. Tout le code PQ est gardé
  `hf_version >= HF_VERSION_PQ` et/ou `pq_keys`.
* Aucune sortie BQ n'a donc **jamais** pu exister sur le mainnet : aucune transaction portant un
  `txin_to_key_pq` ou un champ `0x07` / `0x08` n'est acceptable sous hf 15.
* Les seuls wallets BQ sont locaux (regtest, tests standalone, e2e A4 — dont la LMDB regtest ne
  survit même pas à un redémarrage).
* **Précédent** : l'Étape 9 (migration FIPS, 9 juin 2026) a déjà invalidé tous les wallets BQ
  expérimentaux **sans aucun chemin de migration**, pour la même raison. Le geste est connu et
  accepté.

> **Il n'y a pas de migration à écrire. Il y a une fenêtre à ne pas manquer : elle se ferme à
> l'activation de HFv16.**

### 6.4 Le vecteur figé `pq_vector_test` — protocole à décider explicitement

`src/crypto/pq_vector_test.cpp` épingle la dérivation actuelle, et son en-tête porte l'instruction
« Do not edit to make a failing test pass ». Un changement de racine **fera échouer** ce test par
construction. Il faut donc distinguer deux causes d'échec, sans quoi la garde anti-dérive liboqs
perd tout sens :

* les vecteurs **de compte** (`V_BQ_ADDRESS_*`, `V_MLKEM_*`, `V_MLDSA_*`) dépendent de la racine →
  à **régénérer délibérément**, en consignant dans le fichier la date, le commit et la décision
  qui l'autorise ;
* les vecteurs **brut-seed** (`V_RAW_MLKEM_PK_KECCAK`, `V_RAW_MLDSA_PK_KECCAK`) et le vecteur
  **par sortie** (`V_OUT7_MLDSA_PK_KECCAK`, dérivé de `ss`, pas de la racine) isolent liboqs de
  la dérivation Monero → ils doivent rester **inchangés**. S'ils bougent, ce n'est pas ce
  chantier : c'est la dérive liboqs que le fichier existe pour attraper.

Cette séparation doit être écrite dans le fichier **avant** de toucher à quoi que ce soit, sinon
la prochaine régénération « parce que le test échoue » deviendra une habitude. Lié à
`docs/audit/liboqs_0.16.0_upgrade_gate.md`, dont le point de décision reste ouvert.

### 6.5 Interaction avec C-1

Le binding C-1 cherche depuis l'Étape 8 comment lier `pq_sig.pk` à un émetteur sur une chaîne
privacy, et son résiduel documenté est « une spec de registre d'adresses BQ ». S1b apporte une
réponse partielle et concrète : **une clé PQ d'identité par (sub)adresse, engagée dans l'adresse
et révélée à la dépense**. Ce n'est pas un registre, c'est l'adresse elle-même qui joue ce rôle.
Les trois items — racine (CRIT-4), autorisation du destinataire (CRIT-3 résiduel), binding
(C-1) — se referment sur le même objet et **doivent être spécifiés ensemble**. À ajouter à la
liste des bloquants HFv16, aux côtés de M-11 (maturité liboqs).

### 6.6 Ce qui ne change pas

Chaîne live (hf 15), format d'adresse B..., dérivation `crypto.cpp:153`, `.keys` classiques,
cold-sign classique (`\005`, v3), émission, ring, checkpoints. Sous R2, même l'adresse B... d'un
wallet BQ est inchangée pour un même jeu de 24+1 mots.

---

## 7. Décision demandée

1. **Racine (CRIT-4)** — R1, **R2a** (recommandée), R2b ou R3 ? Et confirmation que **R0**
   (l'hypothèse initiale : redériver depuis la seed existante) est écartée pour de bon, avec la
   raison du §2.2 consignée.
2. **Si R2** : deux mnémoniques (R2a, compatibilité maximale avec les binaires publiés) ou une
   mnémonique étendue (R2b, un seul artefact, échec propre sur les anciens outils) ?
3. **Autorisation du destinataire (CRIT-3 résiduel)** — **S1b** est-elle acceptée (engagement de
   32 o dans l'adresse, clé ML-DSA d'adresse révélée à la dépense **en remplacement** de la clé
   par sortie, coût en taille nul), en assumant la liabilité entre dépenses d'une même
   subaddress ? Si oui : cette liabilité doit-elle être **atténuée par consigne** ou **imposée
   par le wallet** (interdiction de réutiliser une subaddress BQ déjà dépensée) ?
4. **Ordre** — validation du §5.3 : racine d'abord, autorisation ensuite, les deux livrées avec
   la spec C-1 dans un même changement de format, avant HFv16.
5. **Vecteur figé** — validation du protocole §6.4 (régénération délibérée des vecteurs de
   compte, immuabilité des vecteurs brut-seed et par sortie).
6. **Migration** — confirmation du §6.3 : aucune sortie BQ n'a jamais existé sur le mainnet,
   aucun chemin de migration à écrire, la fenêtre se ferme à HFv16.

**Aucune ligne de code ne sera écrite avant réponse.**


---

## 8. Décision et livraison (12 septembre 2026)

### 8.1 Ce qui a été décidé

1. **Question 1 — racine : R2a.** Deux mnémoniques séparées. R0 est formellement écartée (§2.2 :
   la seed *est* la clé de dépense, donc une redérivation sous un autre domaine de hash
   n'apporte rien) ; R1 est écartée parce qu'elle recrée M-4 ; R3 parce que deux seeds de 25
   mots désignant des wallets différents sont indiscernables.
2. **Question 2 — emballage : R2a** plutôt que R2b, pour la raison du §3 : les binaires publiés
   (v2.0.3, GUI v2.0.2-gui) continuent de restaurer correctement la moitié classique depuis les
   25 mots habituels.
3. **Questions 3 à 6** : non tranchées dans cette passe. L'ordre du §5.3 est respecté — racine
   d'abord, autorisation du destinataire ensuite.

### 8.2 Ce qui a été implémenté — et où ça s'écarte du rapport

Deux points que §3 laissait ouverts, tranchés à l'exécution :

* **Taille de la seed BQ : 32 octets / 25 mots**, pas 16 / 13. Le rapport laissait le choix.
  Deux raisons convergent. (a) *Sécurité* : 16 octets d'entropie plafonnent une recherche de
  Grover à 2⁶⁴, **sous** le budget des primitives (§2.1 pose 2¹²⁸ comme la cible) ; 32 octets
  rendent 2¹²⁸. (b) *Mécanique* : `words_to_bytes(words, crypto::secret_key&, lang)` traite
  déjà 32 octets verbatim, **sans `sc_reduce32`** — exactement ce dont une entropie brute a
  besoin — donc zéro changement dans la couche mnémonique.
* **Restauration sans seed BQ : refus explicite.** Le rapport dit « la présence d'une seed BQ
  *est* le signal » (§4.3), ce qui règle le cas sans `--bq-wallet` : on obtient un wallet
  classique. Il ne dit rien du cas `--bq-wallet` **avec** une seed BQ vide. Tranché fail-closed,
  et la règle est posée dans `wallet2::generate`, pas seulement dans la CLI : une racine neuve
  produirait une adresse BQ valide qui n'est pas celle des fonds. Corollaire assumé : le flux
  « attacher une racine BQ neuve à une seed classique existante » n'existe pas ; s'il est voulu,
  il lui faudra son propre drapeau.

Un défaut trouvé en pilotant la CLI réelle, et corrigé : le premier jet **affichait la seconde
seed depuis la racine chiffrée en mémoire** (`setup_keys` chiffre avant l'affichage), soit 25
mots valides restaurant un autre wallet. `get_pq_seed` refuse désormais quand les clés sont
verrouillées — même garde `pq_secrets_usable()` que `get_pq_subaddress_as_str`, même famille que
le défaut ÉLEVÉ de `design_2b §8.5`. Les tests unitaires seuls ne pouvaient pas le voir : leurs
wallets tournent « unattended », donc sans chiffrement mémoire. Cas désormais pinné.

Non implémenté, et pas deviné : **l'offset de seed sur la mnémonique BQ**. `encrypt_key` est une
addition scalaire mod l et la racine est de l'entropie jamais réduite — l'aller-retour ne se
ferait pas. Un aveuglement par XOR serait la voie évidente, le rapport ne la spécifie pas, elle
reste ouverte.

### 8.3 Les trois points mécaniques du §6.1, vérifiés

1. **Flux positionnel** — `pq_root` est ajouté **en fin** du flux chacha20 et compté dans
   `pq_sk_bytes` (`account.cpp:87-125`).
2. **`BOOST_CLASS_VERSION`** — `account_keys` 1 → 2, la racine derrière un `if (ver < 2) return`.
3. **Cold-sign v4** — jamais publié dans un binaire, donc rien à ménager ; et la machine froide
   charge sa racine depuis son propre `.keys`, donc rien à transporter non plus.

Le §6.1 disait « pas de bump de version nécessaire » pour le `.keys` lui-même : confirmé, le
champ nommé optionnel suffit et un wallet classique n'émet rien.

### 8.4 Le protocole `pq_vector_test` du §6.4, appliqué

Régénérés : adresse BQ et les 4 empreintes de clés de compte, sous une nouvelle constante
`V_PQ_ROOT` **délibérément différente de `V_RECOVERY_KEY`** (sinon le vecteur passerait encore
contre le code vulnérable). Inchangés **et re-mesurés pour le prouver** : `V_RAW_*`,
`V_OUT7_MLDSA_PK_KECCAK`, `V_BIND_TAG` — revenus identiques au bit près.

### 8.5 Ce qui reste sur la table

Inchangé par rapport au §7, moins la question 1 : **S1b + binding C-1** dans une même passe
(les deux touchent le format d'adresse BQ, donc avant HFv16), l'offset de seed BQ, la clé de
scan BQ view-only (§3, bénéfice collatéral — désormais réellement atteignable), et le sort de
BQ sur wallet matériel.
