# Design 2b — Subaddresses BQ

**Date :** 8 septembre 2026 — item 2b de la revue post-audit du 7 septembre.
**Statut : ✅ DÉCIDÉ (décision 4, option B3) et IMPLÉMENTÉ le 11 septembre 2026** — voir §7
à §10 en fin de document. §0 à §6 sont le rapport de design d'origine, conservé tel quel à
l'exception du point 3 de §5 (budget de taille), qui était faux et est corrigé en §9.

## 0. État actuel

Un compte BQ n'a **qu'une seule adresse**, l'adresse primaire rendue par
`get_pq_address_str`. `generate_pq_keys` produit un unique couple ML-KEM-768 pour le compte.
Il n'existe aucun équivalent BQ de `get_subaddress` / `m_subaddresses`.

## 1. Comment les subaddresses classiques marchent — et pourquoi ça ne se transpose pas

`device_default::get_subaddress` (device_default.cpp:181-195) :

```
m = H_s("SubAddr" ‖ a ‖ major ‖ minor)      (a = view secret key)
D = B + m·G                                  (spend public)
C = a·D                                      (view public)
```

Toute la propriété utile vient de **l'homomorphisme additif de Ed25519** : la subaddress est
la clé du compte *décalée* par un scalaire. Deux conséquences que l'on tient pour acquises :

* **Le scan est O(1) en nombre de subaddresses.** Le wallet fait UNE dérivation par sortie,
  calcule un candidat de clé de dépense et le cherche dans la table `m_subaddresses`. Avoir
  10 ou 10 000 subaddresses ne change rien au coût par sortie.
* La restauration depuis la seed régénère tout, puisque tout dérive de `a`.

**ML-KEM-768 n'a aucune structure de ce genre.** Une clé d'encapsulation en réseau euclidien
ne se « décale » pas par un scalaire : il n'existe pas d'opération publique qui, à partir de
la clé du compte et d'un index, produise une clé de subaddress valide dont le destinataire
saurait décapsuler. C'est *le* point dur, et il n'a pas de contournement algébrique.

Il faut donc **une paire ML-KEM-768 par subaddress**, dérivée déterministiquement :

```
kem_seed(i) = SHAKE256("HRG_BQ_SUBADDR_KEM_v1" ‖ a ‖ major ‖ minor)   → OQS_KEM_keypair_derand
```

(déterminisme et restauration depuis la seed : acquis, c'est exactement le mécanisme M-4
existant). La moitié Ed25519 reste dérivée comme aujourd'hui — donc l'adresse BQ subaddress
serait `marker ‖ D ‖ C ‖ kem_pk(i)`, même format 1249 octets.

## 2. Le vrai problème : le coût du scan

Pour savoir si une sortie est pour la subaddress *i*, il faut décapsuler le ciphertext ML-KEM
de la transaction **avec la clé de décapsulation de *i***. La décapsulation ne dit pas
« ce n'est pas pour toi » : ML-KEM a un rejet implicite, elle réussit toujours et rend un
secret différent (c'est exactement le piège corrigé par l'audit E-1). Le wallet doit donc
essayer, subaddress par subaddress, et vérifier le match réel.

**Coût : O(N_subaddresses × N_ciphertexts) décapsulations ML-KEM par transaction.** Là où le
classique est O(1). Avec quelques centaines de subaddresses (usage courant d'un marchand),
la synchronisation devient inutilisable.

## 3. Trois options

### B1 — Une paire ML-KEM par subaddress, scan naïf
Correct et non-linkable, mais O(N) décapsulations par sortie. **Rédhibitoire au-delà de
quelques dizaines de subaddresses.** À écarter comme design principal.

### B2 — Une seule clé ML-KEM de compte, partagée par toutes les subaddresses BQ
Le scan redevient O(1) (une décapsulation par ciphertext, puis la machinerie Ed25519
classique identifie la subaddress). Mais **toutes les subaddresses BQ publieraient la même
clé ML-KEM de 1184 octets** : deux adresses données à deux payeurs différents sont
**trivialement reliables** par simple comparaison des chaînes.

Ça détruit la raison d'être des subaddresses (non-linkabilité entre payeurs). À n'envisager
que si l'on redéfinit explicitement les subaddresses BQ comme un outil de **comptabilité
interne** (séparer des comptes dans un même wallet) et pas de confidentialité — et alors il
faut le dire noir sur blanc dans la doc utilisateur, sinon c'est un piège à confidentialité.

### B3 — Une paire ML-KEM par subaddress + un tag de sélection *aveuglé*  ⭐ recommandé
On garde B1 (donc la non-linkabilité), et on rend le scan praticable en évitant les
décapsulations inutiles. L'émetteur ajoute, à côté du ciphertext, un petit tag :

```
tag = 8 premiers octets de H( derivation_Ed25519 ‖ kem_pk_destinataire )
```

où `derivation_Ed25519` est la dérivation partagée usuelle (`r·A`, que le destinataire
recalcule avec sa view key). Le destinataire calcule la dérivation une fois, puis parcourt
ses subaddresses en comparant **des hachages** — pas des décapsulations — et ne fait qu'**une
seule décapsulation ML-KEM**, celle qui correspond.

* Coût : O(N) hachages *bon marché* + O(1) décapsulation ML-KEM, au lieu de O(N)
  décapsulations. Plusieurs ordres de grandeur.
* Le tag est **aveuglé par la dérivation**, donc il change à chaque transaction : un
  observateur ne peut pas relier deux paiements vers la même subaddress. Un tag « nu »
  (`H(kem_pk)` seul) serait constant par subaddress et introduirait une **régression de
  confidentialité pire que le classique** — à ne surtout pas faire.
* C'est exactement l'idée des *view tags* de Monero, appliquée à la sélection de clé KEM.

Coût en taille : 8 octets par sortie BQ, négligeable devant les 1088 octets du ciphertext.

## 4. Impact sur le binding C-1 : aucun

Vérifié dans le code. Le binding par sortie ne dépend d'aucune donnée d'adresse :

* `pqc_keygen_output_dsa(ss, output_index)` dérive la clé ML-DSA par sortie de
  `SHAKE256("HRG_PQ_OUT_DSA_v1" ‖ ss ‖ index)` — donc du **secret partagé**, pas de l'adresse ;
* `pqc_compute_bind_tag(real_output_key, dsa_pk)` lie la clé de sortie réelle à cette clé
  ML-DSA — toujours pas d'adresse.

Que le secret partagé vienne de la clé ML-KEM primaire ou de celle d'une subaddress ne change
rien : les checks *c* et *d* du validateur restent valides mot pour mot. **Les subaddresses BQ
n'ouvrent pas de nouvelle question de binding.** C'était le risque principal à écarter, il
l'est.

## 5. Points à trancher avant tout code

1. **Non-linkabilité : exigée ou non ?** C'est ce qui départage B3 (oui, coût : un tag de
   8 octets sur le format de transaction, donc changement consensus sous HFv16) de B2
   (non, gratuit, mais les subaddresses BQ deviennent purement comptables).
2. **Le tag de 8 octets** est un ajout au format de sortie BQ. Il doit être posé **avant**
   l'activation de HFv16 — après, ce serait un nouveau hard fork. Ça en fait un item à
   traiter *avec* la finalisation du binding C-1, pas après.
3. ~~**Budget de taille** : chaque sortie BQ coûte déjà 1088 octets de ciphertext ML-KEM ;
   avec ML-DSA (5261 o) on est loin au-dessus de `MAX_TX_EXTRA_SIZE_PQ` (8192) dès quelques
   sorties.~~ **Faux — corrigé en §9.** La signature ML-DSA de 5261 o n'est pas par sortie :
   c'est une signature **par transaction**, émise seulement pour une dépense BQ.
4. **Wallet** : `m_subaddresses` devient une table à deux entrées (clé Ed25519 → index, et
   tag → index) ; `generate_pq_keys` doit se décliner en `generate_pq_subaddress_keys(i)` ;
   `.keys` ne doit stocker que la seed de compte (les clés par subaddress se redérivent), sinon
   le fichier explose en taille (3584 o par subaddress).

## 6. Décision demandée

1. Les subaddresses BQ doivent-elles être **non-linkables** (→ B3) ou seulement
   **comptables** (→ B2) ?
2. Si B3 : le tag de sélection aveuglé de 8 octets est-il accepté comme ajout au format de
   sortie BQ, sachant qu'il doit être figé avant HFv16 ?
3. Priorité relative à l'hybride (2a) et au binding C-1 ?

**Aucune ligne de code ne sera écrite avant réponse.**

---

## 7. Décision (11 septembre 2026)

1. **B3** : les subaddresses BQ sont **non-liables**. B2 écarté.
2. Le tag de sélection aveuglé est accepté comme ajout au format de sortie BQ, à figer avant
   HFv16 au même titre que le binding C-1.
3. Aucun ordre imposé avec 2a et C-1 (§4 : pas de dépendance).

## 8. Ce qui a été implémenté — et où ça s'écarte de la spec

Trois écarts à la formule demandée, chacun pour une raison de sécurité ou de coût. Tous sont
isolés dans une seule fonction : revenir à la lettre de la spec est un changement local.

### 8.1 Racine de dérivation : la racine PQ du compte, **pas** la clé de vue `a`

```
kem_seed(major, minor) = SHAKE256("HRG_BQ_SUBADDR_KEM_v1" ‖ racine ‖ major_le4 ‖ minor_le4)
(pk, sk)               = OQS_KEM_keypair_derand(kem_seed)
```

`racine = get_pq_root_secret(keys)`, c'est-à-dire aujourd'hui la clé de dépense — la même que
la clé BQ primaire (M-4). La spec disait `a`. Refusé, parce qu'une dépense BQ transparente est
autorisée par le secret partagé ML-KEM : une clé ML-KEM dérivée de `a` aurait donné le **pouvoir
de dépense** à tout détenteur de la clé de vue (wallet view-only, auditeur, serveur de scan).

⚠️ La racine actuelle a elle-même un défaut, **CRIT-4** (hors périmètre de ce chantier) : la clé
de dépense est le logarithme discret de la clé publique de dépense publiée dans l'adresse BQ,
donc un adversaire quantique la retrouve, et avec elle toutes les clés BQ — primaire comme
subaddresses. C'est précisément pourquoi la racine est derrière **une seule** fonction : le jour
où le format wallet la remplace, les subaddresses suivent sans autre changement.

`(0,0)` reste l'adresse BQ primaire et garde sa dérivation M-4 (le vecteur figé
`pq_vector_test` est inchangé).

### 8.2 Le tag : `fp(kem_pk) XOR pad(d, i)`, pas `H(d ‖ kem_pk)[0..8)`

```
fp(kem_pk) = Keccak("HRG_BQ_KEMPK_v1" ‖ kem_pk)[0..8)        statique, par subaddress
pad(d, i)  = Keccak("HRG_BQ_SEL_v1" ‖ d ‖ i_le8)[0..8)       frais, par sortie
sel_tag    = fp(kem_pk) XOR pad(d, i)
```

* **Coût O(1) au lieu de O(N).** Avec `H(d ‖ kem_pk)`, le destinataire doit calculer un hash
  par subaddress et par sortie — ~10 000 hashs par sortie BQ pour la lookahead par défaut
  (50×200). Avec le XOR, il calcule **un** pad, le retire, et cherche `fp` dans une table
  statique. C'est exactement la « table à deux entrées (clé Ed25519 → index, **tag → index**) »
  demandée, qui n'est pas réalisable si le tag dépend de `d` de façon non inversible.
* **L'index de sortie est dans le pad.** Sans lui, deux sorties d'une même tx vers la même
  subaddress, sans clés additionnelles, porteraient le **même** tag : un observateur verrait
  qu'elles vont au même destinataire. Le view tag Monero inclut l'index pour la même raison.
* **Non-liabilité inchangée.** `pad` est inconnu sans `d` : pour un observateur, le tag est une
  empreinte chiffrée par masque jetable — uniforme, et sans rapport entre deux paiements à la
  même subaddress. Le détenteur de `d` (émetteur, détenteur de la clé de vue) apprend `fp`, ce
  qu'il pouvait déjà tester avec la formule d'origine.

### 8.3 Le champ `0x07` porte l'index de sortie

`[ 0x07 | output_index:varint | sel_tag:8 | ct:1088 ]` — 1098 o pour un index < 128.
Avant, l'association ciphertext ↔ sortie n'existait que par l'ordre d'émission. Nouvelle règle
consensus (`check_pq_output_field_indices`, appelée par `Blockchain::check_tx_inputs` à HFv16) :
les index des champs ML-KEM et des champs de binding sont dans les bornes, sans doublon, et
**nomment le même ensemble de sorties**.

### 8.4 Le reste

* **Adresse** : même charge utile de 1249 o, marqueur `0x35` au lieu de `0x33` → rend toujours
  « BQ » (vérifié sur 2000 charges aléatoires) et le parseur renvoie `is_subaddress = true`.
  L'émetteur en a besoin : clé de tx `R = r·D`, clés additionnelles — exactement la raison
  d'être du couple de préfixes 60/62 classique.
* **Wallet** : `m_pq_subaddresses` (empreinte → index), en mémoire uniquement, reconstruit depuis
  la seed (≈17 µs par subaddress, ~0,2 s pour la lookahead par défaut). **Rien de nouveau dans
  `.keys`.** Commande `address bq <index>` dans `simplewallet`.
* **Cold-sign** : `pq_source_ct` (unsigned_tx_set v4) porte l'index de subaddress ; la machine
  froide re-dérive la clé et **vérifie** que le ciphertext ouvre bien une sortie de cette
  subaddress (un index faux est refusé au lieu de produire une dépense invalide). v4 n'a jamais
  été publié dans un binaire.
* **`sort_tx_extra`** gère désormais le champ de binding `0x08` (il l'aurait fait échouer).

### 8.5 Défaut préexistant corrigé au passage (gravité ÉLEVÉE)

En mode par défaut (`AskPasswordToDecrypt`), la clé de dépense **et `kyber_sk`** sont chiffrées en
mémoire pendant le refresh ; seule la clé de vue est en clair. L'ancienne détection BQ
décapsulait avec ce `kyber_sk` brouillé : **aucune sortie BQ n'était jamais détectée par un wallet
interactif ordinaire**, silencieusement. (L'e2e A4 passait parce que le wallet utilisé ne chiffrait
pas ses clés.) Le tag règle ça proprement : le pré-filtre ne demande que la clé de vue ; le mot de
passe n'est demandé que si un tag correspond à l'une de nos subaddresses, avec la même politique
que `scan_output`. Et `get_pq_subaddress_as_str` **refuse** de dériver une adresse depuis des clés
verrouillées — sinon il afficherait une adresse bien formée pour laquelle personne ne pourra jamais
décapsuler.

## 9. Budget `tx_extra` — correction (mesuré, pas calculé)

La note du §5.3 comptait la signature ML-DSA (5261 o) **par sortie**. C'est faux :

| Élément de `tx_extra` | Taille | Fréquence |
|---|---|---|
| clé publique de tx | 33 o | par tx |
| payment ID chiffré factice | 11 o | par tx, si ≤ 2 destinations |
| clés additionnelles | 2 + 32 o/sortie | par tx, si paiement à des subaddresses |
| champ ML-KEM `0x07` (dont tag 8 o) | 1098 o | **par sortie BQ** |
| champ de binding `0x08` | 34 o | **par sortie BQ** |
| signature ML-DSA-65 de compte `0x06` | 5262 o | **par tx**, seulement pour une dépense BQ |

La ML-DSA **par entrée** (5261 o) vit dans `vin` (`txin_to_key_pq`), pas dans `tx_extra`, et ne
compte donc pas contre `MAX_TX_EXTRA_SIZE_PQ` (8192).

**Mesuré sur des transactions réelles** (`pq_subaddress_test`, `construct_tx` réel) :

| Forme de transaction | Max de sorties BQ | `tx_extra` au max |
|---|---|---|
| paiement B... → BQ | **7** | 7957 o |
| dépense BQ transparente | **2** | 7570 o |

Le tag de 8 o ne fait franchir **aucun** seuil (sans lui : 7 et 2 aussi). La vraie contrainte est
la dépense BQ : **2 sorties BQ au plus**, soit un paiement BQ + un change BQ. Payer deux
destinataires BQ depuis un wallet BQ (dont le change est BQ) échoue à la construction. Options,
à trancher avant HFv16 (changement consensus dans les deux cas) : relever
`MAX_TX_EXTRA_SIZE_PQ` (16384 → ~9 sorties BQ en dépense), ou sortir la signature de compte de
`tx_extra`.

## 10. Reste ouvert

* Un wallet **view-only** ne peut pas détecter de sortie BQ (les empreintes dérivent de la racine
  secrète) — préexistant pour l'adresse primaire.
* Le change d'une dépense depuis un compte `major > 0` retombe sur une subaddress **classique**
  (non-BQ) — politique de change à décider.
* Le budget du §9.
