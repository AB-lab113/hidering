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
