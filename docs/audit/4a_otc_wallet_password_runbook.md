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
