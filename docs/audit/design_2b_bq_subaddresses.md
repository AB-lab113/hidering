# Design 2b — Subaddresses BQ

**Date :** 8 septembre 2026 — item 2b de la revue post-audit du 7 septembre.
**Statut : RAPPORT DE DESIGN. Aucun code écrit. En attente de validation.**

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
3. **Budget de taille** : chaque sortie BQ coûte déjà 1088 octets de ciphertext ML-KEM ;
   avec ML-DSA (5261 o) on est loin au-dessus de `MAX_TX_EXTRA_SIZE_PQ` (8192) dès quelques
   sorties. À re-vérifier globalement avant d'ajouter quoi que ce soit.
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
