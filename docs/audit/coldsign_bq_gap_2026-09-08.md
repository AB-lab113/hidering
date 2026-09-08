# Cold-signing d'une dépense BQ — état réel, correctif appliqué, et décision en attente

**Date :** 8 septembre 2026 — item 1c de la revue post-audit du 7 septembre.

## TL;DR

L'item d'audit disait : *« `account_boost_serialization.h` omet `pq_keys`/`pq_dilithium`.
Un compte exporté vers un signeur offline perd tout son matériel BQ. »*

**Le symptôme est réel, la cause identifiée était la mauvaise.** Vérification faite :

1. `account_boost_serialization.h` omettait bien les deux clés PQ → **corrigé** (versionné,
   avec test). Mais **ce sérialiseur n'a aucun appelant dans l'arbre** : vérifié par
   inspection des symboles des bibliothèques compilées — `serialize(account_keys)` a
   **zéro instanciation**, là où son voisin `serialize(account_public_address)` en a
   plusieurs (le cache wallet archive `m_account_public_address`). Le correctif est donc
   une **assurance**, pas le déblocage du cold-signing.
2. **Le signeur offline ne reçoit jamais de clés par le fichier de transfert.** Il charge
   son propre `.keys`, où `pq_keys` et `pq_dilithium` sont déjà persistés (Étape 7 + C-1).
   De ce côté-là, rien n'est perdu.
3. **La vraie rupture est ailleurs : le fichier unsigned-tx perd les champs PQ des entrées
   et des sorties.** C'est ce qui empêche réellement un cold-signer de construire une
   dépense BQ. Elle demande une décision de format — **rien n'a été codé, voir §3.**

---

## 1. Ce qui a été corrigé

`serialize(cryptonote::account_keys)` sérialise désormais `pq_keys` (ML-KEM-768) et
`pq_dilithium` (ML-DSA-65), sous `BOOST_CLASS_VERSION(account_keys, 1)` : les archives
version 0 s'arrêtent après les clés Ed25519 comme avant, donc rien d'existant ne casse.
Les copies transitoires en clair sont `memwipe`-ées (discipline audit M-4).

**Volontairement NON modifié : `serialize(account_public_address)`.** Celui-là est *vivant*
— le cache wallet l'utilise (`wallet2.h:1278`) — et y ajouter un champ changerait le format
du cache et casserait tous les wallets existants. La clé ML-KEM voyage à l'intérieur de
`pq_keys`, et `account.cpp` réhydrate `m_account_address.pq_kyber_pk` au chargement ; c'est
exactement la décision prise à l'Étape 7, elle reste valable.

Test : `src/crypto/pq_coldsign_test.cpp` — round-trip d'archive d'un compte BQ, puis
vérification que le compte **restauré** sait faire les deux opérations d'un cold-signer :
décapsuler sa propre sortie BQ (ML-KEM-768) et signer avec sa clé ML-DSA-65 persistante.
Contre le header pré-correctif le test échoue (`pq_keys ... dropped by the archive`).

## 2. Pourquoi le cold-signing BQ ne marche toujours pas

`wallet2::sign_tx` appelle `construct_tx_and_get_tx_key(m_account.get_keys(), …,
sd.sources, sd.splitted_dsts, …)`. Les clés viennent du `.keys` local (OK). Mais
`sd.sources` et `sd.splitted_dsts` viennent du fichier unsigned-tx, et ce fichier les
ampute :

| Donnée | Où elle vit | Sérialisée ? | Conséquence côté signeur offline |
|---|---|---|---|
| `tx_source_entry::is_pq` | entrée | ❌ absente de `BEGIN_SERIALIZE_OBJECT` | l'entrée BQ arrive en `false` → construite comme une entrée ring classique |
| `tx_source_entry::pq_ss` | entrée | ❌ idem | pas de secret partagé ML-KEM → impossible de rebâtir la clé ML-DSA par sortie (check *d*) |
| `tx_destination_entry::is_pq` | sortie | ❌ idem | la destination BQ est traitée comme un B... classique |
| `tx_destination_entry::addr.pq_kyber_pk` | sortie | ❌ omise **exprès** par le sérialiseur binaire d'`account_public_address` (Étape 5, pour garder les B... byte-identiques) | pas de clé d'encapsulation → aucune sortie BQ constructible |

Autrement dit : **une dépense BQ exportée vers un signeur offline arrive en ressemblant à
une dépense ring classique vers un destinataire classique.** Le signeur ne peut pas la
construire correctement.

`recover_pq_spend_secret` ne sauve pas la situation côté froid : il a besoin du `tx_extra`
de la transaction source (les ciphertexts ML-KEM), que le signeur n'a pas de façon fiable
dans le chemin `new_transfers` (`exported_transfer_details`, forme compacte).

Le test `pq_coldsign_test::test_transfer_format_still_drops_pq_fields` **assère cet état
courant** : le jour où le format est corrigé, ce test échoue en disant explicitement que le
contrat a bougé, au lieu de laisser l'incohérence passer en silence.

## 3. ⛔ Décision requise avant de coder (rien n'a été implémenté)

Corriger §2 n'est pas un ajout de champ anodin, pour deux raisons.

### 3.1 C'est une rupture de format du fichier de transfert

`tx_source_entry` et `tx_destination_entry` sont sérialisés en epee **positionnel**, sans
champ de version propre. Ajouter des `FIELD` inconditionnels casserait l'interopérabilité
du cold-signing **classique** avec tous les binaires v2.0.3 déjà publiés — pour une
fonctionnalité (BQ) qui est inerte jusqu'à HFv16. Inacceptable tel quel.

Deux options praticables :

* **Option A — table latérale versionnée au niveau `unsigned_tx_set`.** Bumper le
  `VERSION_FIELD` 3 → 4 et écrire, seulement en v4, une table `{index d'entrée → (is_pq,
  pq_ss)}` et `{index de sortie → (is_pq, ml_kem_pk)}`. Les fichiers v ≤ 3 se lisent
  exactement comme aujourd'hui ; les structures elles-mêmes ne bougent pas, donc le
  cold-signing classique reste byte-identique. Rupture unidirectionnelle normale (un vieux
  signeur ne lit pas un fichier v4), du même genre que le bump v2 → v3 fait pour F-1.
* **Option B — n'émettre les champs que sous HFv16.** Plus intrusif : impose de faire
  descendre `pq_hf_version` jusque dans les sérialiseurs des deux structures, qui n'ont
  aucun accès au contexte parent. Nettement plus fragile.

**Recommandation : Option A.**

### 3.2 Elle met un secret dans le fichier unsigned-tx

`pq_ss` est le **secret partagé ML-KEM-768 (32 octets)** de la sortie dépensée. L'écrire
dans le fichier de transfert, c'est décider que ce fichier contient du matériel secret par
sortie. Contexte : un unsigned-tx révèle déjà les montants, les destinations et les
transfer_details au porteur du fichier — c'est un artefact sensible par nature, transporté
entre une machine « chaude » et une machine froide. Mais `pq_ss` est d'une autre nature : il
permet de **rebâtir la clé ML-DSA par sortie**, donc c'est un pouvoir de dépense sur cette
sortie une fois combiné au reste.

Alternatives à arbitrer :

* **A1 — transmettre `pq_ss`** (simple, marche tout de suite ; le fichier devient
  strictement plus sensible qu'aujourd'hui) ;
* **A2 — transmettre le ciphertext ML-KEM de la sortie** et laisser le signeur froid le
  décapsuler avec son propre `pq_keys` (le secret ne quitte jamais la machine froide —
  **posture nettement plus propre**, coût : 1088 o par entrée BQ dans le fichier) ;
* **A3 — transmettre le `tx_extra` complet de la tx source** et laisser
  `recover_pq_spend_secret` refaire tout son travail côté froid (le plus fidèle au design
  existant, le plus volumineux).

**Recommandation : A2.** Elle préserve l'invariant qui fait tout l'intérêt du cold-signing —
aucun secret dépensable ne transite par la machine chaude — pour un surcoût de taille
modeste et localisé aux entrées BQ.

### Question posée au mainteneur

1. Option A (table latérale + bump `unsigned_tx_set` v3 → v4) : validée ?
2. Transport du matériel PQ d'entrée : A1 (`pq_ss`), **A2 (ciphertext, recommandé)** ou A3
   (`tx_extra` complet) ?

Tant que ces deux points ne sont pas tranchés, le cold-signing d'une dépense BQ reste
**non fonctionnel et documenté comme tel**. Ce n'est pas bloquant pour la chaîne live (hf 15)
ni pour les dépenses BQ en ligne, qui fonctionnent (e2e A4).
