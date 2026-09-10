# 4a — Retirer le mot de passe du wallet OTC de la ligne de commande (Contabo-2)

**Date :** 8 septembre 2026
**Statut : PRÉPARÉ ET TESTÉ EN LOCAL. ⛔ NON APPLIQUÉ EN PRODUCTION — en attente de confirmation explicite.**
Aucun fichier n'a été modifié sur Contabo-2, aucun processus touché, aucun fonds déplacé.

## 1. Le problème, constaté

Sur Contabo-2 (`207.180.214.164`), en lecture seule :

```
$ ps -eo pid,user,cmd | grep wallet-rpc
2533746 root /root/hidering-wallet-rpc --wallet-file /root/hrg-wallet-otc \
        --password otc2026secure --rpc-bind-port 19745 ...
```

Le mot de passe du **wallet d'escrow OTC** est lisible par tout processus local — `ps` est
lisible par tous les utilisateurs (`/proc/<pid>/cmdline` est world-readable).

**Deuxième exposition, non relevée par l'audit :** le mot de passe est aussi en clair dans
l'unit systemd, et **ce fichier est world-readable** :

```
-rw-r--r-- 1 root root 499 /etc/systemd/system/hidering-wallet-rpc-otc.service
```

Corriger seulement `ps` sans corriger les permissions de l'unit ne règle que la moitié du
problème. Le plan ci-dessous traite les deux.

**Correction au passage sur le périmètre :** le service est un **unit systemd**
(`hidering-wallet-rpc-otc.service`), pas un processus pm2. pm2 ne gère que `hidering-otc`
(le bot) et `hrg-pool`. Le redémarrage est donc `systemctl restart`, et **pm2 n'est pas
touché** — ce qui est cohérent avec la consigne de ne pas toucher aux process pm2.

**Observation annexe (à trancher séparément, pas incluse ici) :** le wallet-rpc du **pool**
(pid 165673, port 19743) tourne avec `--password ` — c'est-à-dire un **mot de passe vide**.
C'est un sujet distinct de 4a ; signalé, non traité.

## 2. Ce qui a été testé (en local, sur un wallet jetable)

`--password-file` est bien supporté par `hidering-wallet-rpc` (`wallet_args.cpp:79`), et lu
par `get_password` (`wallet2.cpp:523-533`), qui **supprime les retours à la ligne finaux**.
Trois tests sur un wallet créé pour l'occasion :

| Test | Résultat |
|---|---|
| Démarrage avec `--password-file` (0600, sans retour ligne) | ✅ wallet chargé, `get_address` répond |
| `ps` pendant l'exécution | ✅ **plus aucun mot de passe** — seulement `--password-file <chemin>` |
| Fichier écrit avec `echo` (donc avec `\n` final) | ✅ fonctionne — le `\n` est bien trimé |
| Mauvais mot de passe dans le fichier | ✅ `error::invalid_password`, refus de démarrer (le fichier est donc réellement utilisé) |
| Fichier absent / illisible | ✅ « the password file specified could not be read », refus de démarrer — **pas de démarrage silencieux sans mot de passe** |

**Piège vérifié et écarté :** `--password-file` est **refusé en combinaison avec
`--wallet-dir`** (`wallet_rpc_server.cpp:4972-4976`). L'unit OTC utilise `--wallet-file`,
donc c'est compatible. ⚠️ Mais le wallet OTC **a été créé** à l'origine avec `--wallet-dir` :
si une procédure future revient à `--wallet-dir`, elle sera incompatible avec ce changement.

## 3. La modification proposée

### 3.1 Créer le fichier de mot de passe

```bash
umask 077
printf '%s' 'otc2026secure' > /root/.hrg-otc-wallet.pass
chmod 600 /root/.hrg-otc-wallet.pass
chown root:root /root/.hrg-otc-wallet.pass
ls -la /root/.hrg-otc-wallet.pass    # doit afficher -rw------- root root
```

(`printf` plutôt que `echo` par principe, même si le `\n` serait trimé.)

### 3.2 Nouvelle unit — le diff exact

```diff
 ExecStart=/root/hidering-wallet-rpc \
   --wallet-file /root/hrg-wallet-otc \
-  --password otc2026secure \
+  --password-file /root/.hrg-otc-wallet.pass \
   --rpc-bind-port 19745 \
   --rpc-bind-ip 127.0.0.1 \
   --daemon-address 127.0.0.1:19741 \
   --disable-rpc-login \
   --log-file /root/otc-walletrpc.log \
   --non-interactive
```

### 3.3 Fermer la deuxième exposition

```bash
chmod 600 /etc/systemd/system/hidering-wallet-rpc-otc.service
```

Sans effet sur systemd (il lit en root) et retire le mot de passe — puis le *chemin* du
fichier de mot de passe — de la vue de tout utilisateur non privilégié.

### 3.4 Séquence d'application

```bash
# 0) sauvegarde
cp -a /etc/systemd/system/hidering-wallet-rpc-otc.service \
      /root/hidering-wallet-rpc-otc.service.bak.$(date +%Y%m%d-%H%M%S)

# 1) fichier de mot de passe (§3.1)
# 2) édition de l'unit (§3.2)
# 3) permissions (§3.3)

systemctl daemon-reload
systemctl restart hidering-wallet-rpc-otc.service

# 4) vérifications
systemctl is-active hidering-wallet-rpc-otc.service          # -> active
ps -eo cmd | grep -c otc2026secure                            # -> 0
curl -s -X POST http://127.0.0.1:19745/json_rpc \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":"0","method":"get_balance","params":{"account_index":0}}'
# -> balance attendue : 4350 HRG (relevé du 8 sept. 2026)
```

### 3.5 Rollback

```bash
cp -a /root/hidering-wallet-rpc-otc.service.bak.<TS> \
      /etc/systemd/system/hidering-wallet-rpc-otc.service
systemctl daemon-reload && systemctl restart hidering-wallet-rpc-otc.service
```

## 4. Risques et fenêtre d'indisponibilité

* Le wallet-rpc redémarre : **quelques secondes à quelques dizaines de secondes**
  d'indisponibilité du port 19745, le temps de recharger et re-scanner le wallet.
* **Le bot OTC n'est pas redémarré** (il est en pm2, hors périmètre). Pendant la fenêtre, ses
  pollers HRG (60 s) prendront un `ECONNREFUSED` sur 19745 — exactement le symptôme observé
  pour l'ordre #10 (cf. §4b du rapport). C'est **sans conséquence financière** : le poller
  réessaie au cycle suivant et les legs de règlement sont idempotents. Aucune clé ni aucun
  fonds n'est touché : le fichier wallet, la seed et les soldes sont inchangés.
* **Le mot de passe lui-même ne change pas.** Si l'on veut aussi le faire tourner (il traîne
  en clair dans `ps` et dans un fichier world-readable depuis juin, donc il doit être
  considéré comme compromis vis-à-vis de tout utilisateur local), c'est une opération
  distincte, plus lourde, et à décider séparément.
* Meilleur moment : quand aucun ordre n'est en cours de règlement.

## 5. Décision demandée

Applique-t-on §3 sur Contabo-2 ? (§3.1 → §3.4, avec sauvegarde et rollback prêts.)
Et souhaite-t-on traiter en plus : (a) le mot de passe vide du wallet-rpc du pool,
(b) une rotation du mot de passe OTC ?

---

# APPLIQUÉ EN PRODUCTION — 10 septembre 2026, 18:27 CEST

Contabo-2 (`207.180.214.164`) uniquement. Aucun autre VPS touché. Aucun fonds déplacé,
aucun process pm2 touché.

## Ce qui a été fait

| Étape | Action | Résultat |
|---|---|---|
| Sauvegarde | `/root/hidering-wallet-rpc-otc.service.bak.20260910-182639` | ✅ |
| Fichier mot de passe | `/root/.hrg-otc-wallet.pass`, `0600 root:root`, 13 o, sans `\n` | ✅ |
| Unit | `--password <secret>` → `--password-file /root/.hrg-otc-wallet.pass` | ✅ diff d'une seule ligne |
| Permissions unit | `0644` → `0600` | ✅ |
| Redémarrage | `daemon-reload` + `systemctl restart` (pas de `kill -9`) | ✅ PID 2533746 → 2715006 |

**Le mot de passe n'a jamais été retapé à la main** : il a été extrait de l'unit par
`grep -oP "(?<=--password )\S+"`, écrit avec `printf`, puis le fichier a été comparé à la
source par SHA-256 avant tout redémarrage.

**Correctif ajouté en cours de route (non prévu au runbook) :** le fichier de sauvegarde
contient lui aussi le mot de passe en clair et était créé en `0644` par `cp -a` (qui
préserve les permissions de l'original). Il a été passé en `0600` — sans quoi la
sauvegarde aurait réintroduit exactement l'exposition que l'opération supprime.

## Vérifications (avant → après)

| Contrôle | Avant | Après |
|---|---|---|
| Solde total | 4350 HRG | **4350 HRG — JSON identique octet pour octet** |
| Sous-adresses créditées | 9 | 9, mêmes montants, mêmes labels |
| Mot de passe dans `ps` / `/proc/*/cmdline` | exposé (PID 2533746) | **0 processus** (balayage de tous les `/proc/*/cmdline`) |
| Permissions unit | `0644` world-readable | `0600` |
| Service | active | active, `NRestarts=0`, `ExecMainStatus=0` — pas de crash loop |
| pm2 `hidering-otc` | pid 2427471 | pid 2427471 (intact) |
| pm2 `hrg-pool` | pid 456876 | pid 456876 (intact) |
| Chemin de code du bot | — | `hrg.getBalance()` → 4350 HRG, `validateAddress()` → true |

**Piège de vérification :** `ps -eo cmd | grep -c "$PW"` renvoie `1` — c'est le `grep`
lui-même qui porte le mot de passe en argument. Le contrôle qui fait foi est le balayage
de `/proc/[0-9]*/cmdline`, qui renvoie **0**.

**Fenêtre d'indisponibilité :** ~1 s. Le wallet a loggé `no connection to daemon` à
16:27:16–18 UTC (course au démarrage : le wallet-rpc interroge `hideringd` avant d'être
prêt), suivi de la stack trace habituelle de l'interposeur `__cxa_throw`
(faux positif connu). Résolu tout seul : **0 ligne d'erreur après 16:27:19**, wallet
aligné sur le daemon (h=83286).

**Le bot n'a rien loggé** — ni avant, ni après. Ses journaux pm2 datent du 28 août.
Ce n'est pas un symptôme : `cron.js:35` ne journalise qu'en cas d'**erreur**
(`console.error` dans un `catch`), et boucle sur les ordres en attente. Silence = aucune
erreur. Le `[hrg-poll order 10] ECONNREFUSED 127.0.0.1:19745` visible en fin de journal
est **historique (28 août)**, pas un effet de cette opération — vérifié par `mtime`.

Le mot de passe lui-même est **inchangé**. Exposé en clair dans `ps` et dans un fichier
world-readable depuis juin, il doit toujours être considéré comme compromis vis-à-vis de
tout utilisateur local ayant eu accès à la machine depuis. Sa **rotation reste à décider**
séparément.

---

# Décision annexe — mot de passe VIDE du wallet-rpc du pool (port 19743)

**Verdict : constaté, réel mais non exposé aujourd'hui. RIEN N'A ÉTÉ MODIFIÉ.**

## Pourquoi « le même correctif » ne s'applique pas

Appliquer `--password-file` ici serait un **no-op de sécurité**. Ce qui a été corrigé sur
l'OTC, c'est *un secret lisible par tous*. Ici il n'y a **aucun secret** : la ligne de
commande ne divulgue que le fait que le mot de passe est vide. Déplacer « rien » dans un
fichier `0600` ne change aucun modèle de menace — et coûterait un redémarrage d'un service
actuellement dans un état fragile (voir plus bas).

Le vrai correctif serait de **poser un mot de passe** (`change_wallet_password`), ce qui
réécrit `hrg-wallet-v2.keys` : une **mutation du fichier wallet**, pas un changement de
configuration. Hors du périmètre « config uniquement » de cette opération.

## Ce que le mot de passe vide risque vraiment

* Le wallet du pool détient **54 700 HRG** — **12,5× l'OTC**.
* `hrg-wallet-v2` et `.keys` sont en **`0600 root:root`** : correct. Aucun utilisateur
  local non privilégié ne peut les lire. **Aucune copie world-readable n'existe**
  (les seuls `.keys` en `0644` sont les fixtures de test Monero de
  `/root/hidering/tests/data/`, pas de vrais wallets).
* Donc **aucune exposition active** : contrairement à l'OTC, aucun identifiant n'est
  actuellement lisible par qui que ce soit.
* Le manque est une **défense en profondeur** : si `hrg-wallet-v2.keys` sort un jour du
  périmètre de permissions de la machine — instantané du fournisseur VPS, sauvegarde,
  image disque, copie accidentelle, exfiltration — un mot de passe vide signifie que le
  fichier se déchiffre avec une clé connue, donc **autorité de dépense immédiate sur
  54 700 HRG**. Avec un mot de passe, le fichier volé est inutile seul.

## Découverte opérationnelle plus urgente que le mot de passe

`hrg-wallet-rpc.service` est **`enabled` mais `inactive`/`dead`** (`MainPID=0`), alors que
le wallet-rpc du pool tourne réellement en **pid 165673, lancé à la main le 25 mai avec
`--detach`**, donc **hors supervision systemd** :

* si ce processus meurt, **rien ne le redémarre** — les paiements du pool (toutes les
  30 min, `minPayment` 1 HRG) s'arrêtent silencieusement ;
* un redémarrage de la machine lancerait l'unit, dans un état différent du processus
  courant.

**Cette incohérence doit être réglée AVANT toute manipulation du mot de passe** : poser un
mot de passe impose de redémarrer le wallet-rpc, et le faire tant qu'aucun superviseur ne
peut le relancer, sur un wallet à `blocks_to_unlock: 56`, risque de laisser les paiements
du pool à terre sans filet.

**Séquence recommandée, à traiter comme une opération distincte :** (1) réconcilier
unit systemd ↔ processus détaché, (2) *ensuite seulement* poser un mot de passe via
`change_wallet_password` + `--password-file`, en dehors d'une fenêtre de paiement.

---

# 10 septembre 2026 (soir) — clôture complète de 4a

Contabo-2 uniquement. Aucun fonds déplacé, aucun process pm2 touché.

## Point A — wallet du pool : supervision + mot de passe

### A.1 La cause racine : l'unit n'a JAMAIS fonctionné

Le diagnostic de ce matin (« unit `enabled` mais `dead`, process lancé à la main ») était
exact mais incomplet. La raison est dans l'`ExecStart` :

```
--wallet-file /root/hrg-wallet-v2 --password  --rpc-bind-port 19743 ...
```

**systemd n'est pas un shell : il découpe sur les espaces et ne crée pas d'argument vide.**
`--password` a donc avalé `--rpc-bind-port` comme valeur, `19743` est devenu un positionnel
inconnu, et le binaire a répondu en affichant son aide puis `status=1/FAILURE`. C'est
pourquoi quelqu'un avait démarré le wallet-rpc à la main le 25 mai — l'unit ne pouvait pas
marcher. Le process manuel n'était pas une négligence, c'était un contournement.

Découvert **en production** : `systemctl start` a échoué, le port 19743 est resté fermé
~2 min. Sans impact (voir A.3). Correctif : `--password ""` — systemd honore les guillemets
et passe un vrai argument vide. Service actif en 7 s.

### A.2 Séquence appliquée

| Étape | Action |
|---|---|
| Sauvegardes (0600) | `hrg-wallet-rpc.service.bak.20260910-2006`, `hrg-wallet-v2.keys.bak.20260910-2006` |
| Arrêt du process manuel | RPC `stop_wallet` (sauvegarde l'état) — sorti proprement en 2 s, **pas de `kill -9`** |
| Supervision | `systemctl start` → actif, `Restart=always`, `RestartSec=10` |
| Mot de passe | `change_wallet_password` (ancien vide → 40 caractères aléatoires, ~238 bits) |
| Référence | unit passée à `--password-file /root/.hrg-pool-wallet.pass` (0600), unit en 0600 |

Le fichier `.keys` a bien été réécrit : `9233007…` → `03e78ad…`. Le redémarrage suivant
**prouve** que le nouveau mot de passe fonctionne (le wallet s'ouvre via le fichier).

### A.3 Aucun paiement mineur ne pouvait être perdu — vérifié dans le code

`lib/paymentProcessor.js:237-242` : en cas d'échec RPC, `cback(false)` **précède toute
écriture Redis**. Les décréments de solde (`hincrby balance -amount`) et l'incrément `paid`
ne sont poussés qu'après un `transfer` réussi. Un wallet-rpc indisponible ne fait donc
que **reporter** le paiement au cycle suivant, sans jamais toucher au dû du mineur.

Fenêtre choisie en conséquence : opération lancée juste après le paiement de **20:05:52**,
soit ~29 min de marge avant le suivant. Solde inchangé de bout en bout
(`55057135329183092` avant / après).

## Point B — rotation du mot de passe OTC

Ancien mot de passe (`otc2026secure`) exposé dans `ps` et dans un fichier world-readable
depuis juin → considéré comme grillé. Le masquer d'hier ne suffisait pas.

| Étape | Action |
|---|---|
| Sauvegarde (0600) | `hrg-wallet-otc.keys.prepw.20260910-201103` |
| Rotation | `change_wallet_password` → 40 caractères aléatoires |
| Fichier | `/root/.hrg-otc-wallet.pass` réécrit, 0600 root:root |
| Redémarrage | `systemctl restart` — actif, `NRestarts=0`, `ExecMainStatus=0` |

`.keys` réécrit : `a1493c7…` → `d2f046d…`. **Preuve cryptographique de la rotation :** une
tentative `change_wallet_password` avec l'ancien mot de passe est refusée —
`Invalid original password`.

Vérifications : solde **4350 HRG identique**, chemin de code du bot
(`hrg.getBalance()`) → 4350 HRG, pm2 `hidering-otc`/`hrg-pool` PIDs inchangés.

## Où l'ancien mot de passe subsiste — et pourquoi ce n'est plus une fuite

Supprimés (`shred`) : `/root/.hrg-otc-wallet.pass.old` et la sauvegarde d'unit de la
veille. Cette dernière n'avait plus de valeur de rollback : elle porte l'ancien mot de
passe, la restaurer **casserait** désormais le service.

Il reste dans le **journal systemd** — précisément dans le champ de métadonnées
**`_CMDLINE=`** de **1709 entrées** réparties sur 44 fichiers (`/var/log/journal/`,
`0640 root:systemd-journal`) : systemd enregistre la ligne de commande du processus pour
**chaque ligne de log** émise par le service, pas seulement au démarrage.

Nuance de mesure à connaître : `journalctl | grep` renvoie **0** — la sortie par défaut
n'affiche que `MESSAGE`. Il faut `journalctl -o verbose` (ou un grep sur les fichiers
bruts) pour le voir. Un « rien trouvé » avec le grep naïf aurait été un faux négatif.

**Ce n'est plus un identifiant** — le wallet le rejette, c'est vérifié. Et l'accès y est
**root uniquement** : le groupe `systemd-journal` est **vide** et la machine n'a **aucun
compte non-root** (aucun UID ≥ 1000). Purger le journal détruirait tout l'historique
d'exploitation pour neutraliser une chaîne déjà morte : non fait, délibérément.

**Ce que cela dit du risque d'origine, rétrospectivement.** L'exposition corrigée hier
n'était pas théorique : la machine fait tourner 9 comptes de service non privilégiés, dont
**`redis`** et **`www-data`/nginx**, tous deux exposés au réseau (API du pool en 80/8117).
La compromission de l'un d'eux donnait le mot de passe du wallet OTC via `ps` ou l'unit en
0644 — mais **jamais** le journal ni les fichiers wallet, tous en 0600/0640 root. Les deux
correctifs ferment donc deux chemins distincts et réels : hier « service non privilégié
compromis → mot de passe », aujourd'hui la fenêtre historique de toute personne l'ayant
déjà lu.

**Les deux NOUVEAUX mots de passe sont propres** : absents de `/root`, `/etc`, `/opt`
(hors leurs fichiers 0600), absents du journal, absents de tout `/proc/*/cmdline`.

## ⚠️ Point de décision laissé à l'opérateur

Les sauvegardes `.keys` **antérieures à la rotation** sont conservées en 0600 :
`hrg-wallet-otc.keys.prepw.*`, `hrg-wallet-v2.keys.*`. Elles portent **les mêmes clés de
dépense**, chiffrées avec les **anciens** mots de passe — pour l'OTC, un mot de passe que
toute personne ayant lu `ps` avant hier connaît, et qui traîne dans les journaux.

Tant qu'elles existent, « ancienne sauvegarde exfiltrée + ancien mot de passe connu » =
**accès aux fonds**, malgré la rotation. Elles ne sont conservées que comme filet de
sécurité de l'opération du jour, désormais vérifiée. **Recommandation : les `shred` une
fois la confiance acquise** (le wallet reste restaurable depuis sa seed de 25 mots).
Non fait ici : détruire du matériel de récupération est votre décision, pas la mienne.

## Constats annexes, non traités (aucun lien avec cette opération)

* **`[unlocker]` du pool : ~30 `ECONNRESET`/minute** sur `getblockheaderbyheight` vers le
  daemon, pour 34 blocs en attente. **Antérieur** à l'opération — mesuré dès 19:46, soit
  20 min avant toute intervention. À investiguer séparément.
* **`/root/.pm2/logs/hrg-pool-out.log` fait 1,0 Go** — pas de rotation. Risque de
  saturation disque.

---

# 10 septembre 2026 (clôture) — destruction des sauvegardes de clés pré-rotation

Contabo-2 uniquement. Aucun fonds déplacé, aucun process pm2 touché.

## Comment les fichiers ont été trouvés (pas par leur nom)

Chercher `*.prepw.*` / `*.bak.*` n'aurait rien prouvé : une sauvegarde peut porter
n'importe quel nom. Recherche par **contenu**, en trois passes :

1. **Signature d'octets — échec, et c'est instructif.** Un fichier `.keys` commence par un
   **IV ChaCha aléatoire** : il n'y a **pas** de nombre magique constant. Toute recherche
   par en-tête est vouée à l'échec ici.
2. **Test structurel.** Le format est `chacha_iv(8) || varint(longueur) || blob`. Un fichier
   est un candidat si le varint en offset 8 vaut **exactement** le nombre d'octets
   restants. Validé d'abord contre les deux `.keys` vivants (les deux détectés), puis
   appliqué aux **163 759** fichiers < 64 Ko de tous les systèmes de fichiers locaux.
3. **Filtrage entropie + taille**, car le test structurel produit des faux positifs sur de
   petits fichiers texte (un varint qui coïncide). Les vrais `.keys` font 1,2–2 Ko avec une
   entropie ≈ 7,9 bits/octet. Passe complémentaire par nom **sans limite de taille**, pour
   ne pas rater une sauvegarde de cache (> 64 Ko), plus archives et tâches planifiées.

## Supprimés (`shred -n 3 -z -u`)

| Chemin | Taille | mtime source | sha256 (avant destruction) |
|---|---|---|---|
| `/root/hrg-wallet-otc.keys.prepw.20260910-201103` | 1716 | 2026-06-16 19:21:43 | `a1493c7c3a6502e61b1dbc4b9d9755bdfbe223a3a7e29e0f063090c39060294a` |
| `/root/hrg-wallet-v2.keys.bak.20260910-2006` | 1699 | 2026-05-25 19:25:57 | `923300708874a7b98b1ee2e92e95b2b0a7ca829044be0e3adac27cb072a2010a` |
| `/root/hrg-wallet-v2.keys.prepw.20260910-201015` | 1699 | 2026-05-25 19:25:57 | `923300708874a7b98b1ee2e92e95b2b0a7ca829044be0e3adac27cb072a2010a` |

Les deux sauvegardes du pool sont **byte-identiques** (même sha256) : deux copies de la
même source, prises à deux étapes de l'opération.

**Preuve qu'il s'agissait bien de copies pré-rotation** : leurs empreintes diffèrent de
celles des fichiers vivants (`d2f046d…` pour l'OTC, `03e78ad…` pour le pool), lesquels ont
été réécrits par `change_wallet_password`.

## Conservés délibérément

* **`/root/hrg-wallet-otc.keys`, `/root/hrg-wallet-v2.keys`** et les caches
  `/root/hrg-wallet-otc`, `/root/hrg-wallet-v2` — **fichiers vivants**. Empreintes
  vérifiées identiques avant et après la suppression.
* **`/root/hidering/tests/data/wallet_*.keys`** (3 fichiers, 16 mai) — **fixtures de test
  Monero amont**, versionnées dans le dépôt, sans rapport avec nos wallets et sans fonds.
  Les supprimer salirait l'arbre git. Elles sont en **0644** — c'est le finding connu et
  distinct « matériel de test dans l'historique git », **hors périmètre ici**.
* **`/root/hrg-wallet-rpc.service.bak.20260910-2006`** — sauvegarde d'unit, pas de `.keys`.
  Ne contient **aucun secret** (l'ancien mot de passe du pool était vide). Conservée comme
  trace de l'`ExecStart` d'origine cassé. ⚠️ Ne pas la restaurer telle quelle : elle porte
  la syntaxe `--password ` qui empêche systemd de démarrer le service.

## Recréation impossible

Aucune tâche planifiée ne peut régénérer une copie chiffrée avec un ancien mot de passe :
crontab root **vide** de toute tâche wallet, aucune référence wallet dans `/etc/cron*`,
et les seuls timers systemd sont ceux du système (apt, logrotate, sysstat, fwupd,
dpkg-db-backup). Les seules units mentionnant un wallet sont les deux services eux-mêmes,
qui pointent vers les fichiers **vivants**. Aucune archive contenant du matériel wallet
(seuls des `.gz` d'apt/dpkg). Pas de répertoire `backup_wallets/` sur cet hôte.

## Vérification finale

| Contrôle | Résultat |
|---|---|
| Chemins supprimés | les 3 absents |
| `*.prepw.*` / `*.keys.bak*` restants sur tout le disque | aucun |
| Recherche par empreinte des deux anciens blobs, tout le disque | **aucune occurrence** |
| Scan structurel de `/root` | seuls les 2 `.keys` vivants |
| `.keys` vivants | empreintes **identiques** à l'avant-suppression |
| Soldes | OTC **4350 HRG**, pool 55 564 HRG (croît : récompenses de blocs) |
| Services | tous deux `active`, `enabled`, `NRestarts=0` |
| Bot OTC | `hrg.getBalance()` → 4350 HRG |
| Pool | `daemon:"ok" wallet:"ok"`, stratum 3333 à l'écoute |
| pm2 | 2427471 / 456876 inchangés |
| Journaux des services depuis la suppression | **0 ligne d'erreur** |

## ⚠️ Limite honnête de `shred`

`shred` réécrit les blocs **via le système de fichiers**. Sur **ext4** (ici, `/dev/sda1`)
le journal peut conserver des restes, et surtout il s'agit d'un **VPS** : le stockage est
virtualisé, potentiellement en copy-on-write / thin provisioning côté hyperviseur, où
l'écrasement logique **ne garantit pas** que les blocs physiques sous-jacents ont été
réécrits. Des instantanés ou sauvegardes pris par l'hébergeur **avant** aujourd'hui peuvent
donc encore contenir ces fichiers.

Ce que la suppression garantit réellement : plus aucun accès **par le système de fichiers**
sur cet hôte. La protection de fond reste la **rotation** — les clés vivantes sont
désormais chiffrées avec des mots de passe qui n'ont jamais été exposés.

---

# 10 septembre 2026 — mot de passe Redis (Contabo-2)

Redis détient **toute la comptabilité du pool** : soldes des mineurs, parts, historique des
paiements. Il tournait **sans authentification**. Bien qu'en `bind 127.0.0.1` +
`protected-mode yes` (donc injoignable à distance), nginx/`www-data` sert l'API publique du
pool en 80/8117 **depuis la même machine** : la compromission d'un processus local non
privilégié donnait un accès **lecture/écriture non authentifié aux soldes des mineurs**.
C'est la même classe de risque que l'exposition OTC d'origine.

## Pourquoi un redémarrage du pool était indispensable

Redis **ne dé-authentifie pas** les connexions déjà établies quand on pose `requirepass` à
chaud. Poser le mot de passe **sans** redémarrer le pool aurait donc laissé un système qui
fonctionne… jusqu'à la première reconnexion (coupure réseau, redémarrage de Redis), où le
pool serait tombé **silencieusement**. Un changement à moitié appliqué est ici pire que
l'un ou l'autre extrême — d'où le `pm2 restart hrg-pool`, seule entorse à la consigne
« ne pas toucher aux process pm2 », assumée et vérifiée.

## Fenêtre choisie

Opération lancée **au milieu** du cycle de paiement (17 min après le précédent, 12 min
avant le suivant). Raison : redémarrer le pool à l'instant exact d'un paiement pourrait
tuer le processus **entre** le `transfer` réussi et le décrément Redis — le seul scénario
produisant un **double paiement** (le code a d'ailleurs une branche
`Double payments likely to be sent`).

## Séquence appliquée

| Étape | Action |
|---|---|
| Sauvegardes (0600) | `redis.conf.bak.*`, `hrg-pool-config.json.bak.*`, **`redis-dump.rdb.bak.*`** (BGSAVE) |
| Mot de passe | 40 caractères aléatoires → `/root/.hrg-redis.pass` (0600 root) |
| Runtime | `CONFIG SET requirepass` — effet immédiat |
| Persistance | ligne `requirepass` ajoutée à `/etc/redis/redis.conf` (0640 redis:redis) → survit à un redémarrage de Redis |
| Pool | `config.json` → `redis.auth` renseigné (édition ciblée, formatage préservé, JSON revalidé), fichier passé en 0600 |
| Redémarrage | `pm2 restart hrg-pool` |

## Vérifications

| Contrôle | Résultat |
|---|---|
| Accès sans authentification | `NOAUTH Authentication required` (ping et dbsize) |
| **Aucune clé perdue** | comparaison des noms de clés extraits des RDB avant/après : **0 manquante**, 2 nouvelles (`*:roundCurrent`, créées par le minage) |
| Préfixes de clés | tous présents (charts, scores, shares_actual, workers_ip, payments, workers, blocks…) |
| Mot de passe dans un cmdline | **0 processus** (`redis-server` n'affiche que `127.0.0.1:6379`) |
| Fichiers le contenant | 3, tous correctement permissionnés (0600 root ×2, 0640 redis:redis pour redis.conf que Redis doit lire) |
| Pool | `daemon:"ok" wallet:"ok"`, stratum 3333 à l'écoute, mineur connecté |
| Bot OTC | pid 2427471 **inchangé** (Redis ne le concerne pas : il utilise SQLite) |

**Piège de lecture à connaître :** `dbsize` est passé à 183 juste après le redémarrage
(contre 184 avant) puis à 186. Ce n'est **pas** une perte : le pool crée et expire en
permanence des clés transitoires (`roundCurrent`, charts, hashrate). La preuve solide n'est
pas `dbsize` mais la **comparaison des noms de clés** entre les deux instantanés RDB —
0 disparue.

## Preuve de bout en bout

Le redémarrage du pool déclenche un cycle de paiement immédiat. **14 secondes après le
redémarrage**, à 21:55:08, le pool a payé 50 HRG :

* tx `5718b3d7eadfe3e928755541dcc4465c86baf2249139d321677abe7075161a9d`
* **confirmée en chaîne au bloc 83368** (`in_pool: false`)
* `payments:all` : 555 → **556**

Ce seul événement prouve la boucle complète : **lecture Redis authentifiée → transfert via
le wallet-rpc (avec son nouveau mot de passe) → confirmation en chaîne → écriture Redis
authentifiée.**

## Reste ouvert

`.env` du bot OTC : `HD_MNEMONIC` (81 caractères) est la seed HD Ethereum contrôlant les
fonds USDT/USDC. Correctement en 0600 root, mais c'est le secret de plus grande valeur de
la machine et il vit en clair dans un `.env`. Non traité.
