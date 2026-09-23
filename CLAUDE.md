# HIDERING (HRG) — Claude Code Project Memory
**Version synchronisée : Whitepaper v1.3 + Roadmap v2.0 — 30 Avril 2026**
**Dernière MAJ : 21 septembre 2026 — allègement de ce fichier.** L'historique daté, les releases passées et les journaux d'implémentation sont sortis vers `~/hidering-notes/CLAUDE-archive-2026-09-21.md` (hors dépôt, `chmod 600`, **jamais importé avec `@`**). La section « INDEX DE L'ARCHIVE » ci-dessous dit, pour chaque bloc, ce qu'il contient et dans quel cas le lire. L'original complet reste récupérable par `git show 896127c3e:CLAUDE.md` et par `~/hidering-notes/CLAUDE.md.bak-2026-09-21`.

**État courant** — branche active `v2-privacy` ; chaîne mainnet en **hf 15**, PQC inerte ; HFv16 à `h = 2 000 000`, **jamais activé**. Deux verrous avant activation : **M-11** (maturité liboqs, condition de levée dans SÉCURITÉ) et le binding C-1, dont la **spec 2e** a été **livrée** le 12 septembre 2026 — voir « État Phase 5 au 21 septembre 2026 » en fin de fichier. La règle **« Ne PAS activer HFv16 avant la finalisation du binding C-1 »**, en section POST-QUANTIQUE, reste en vigueur telle quelle. Détail des travaux post-quantiques : `docs/audit/*.md` (dans le dépôt) et blocs A15 à A25 de l'archive.
## INDEX DE L'ARCHIVE
`~/hidering-notes/CLAUDE-archive-2026-09-21.md` (hors dépôt, `chmod 600`, **jamais importé**). Texte original intégral, chaque bloc précédé de ses lignes d'origine. Format : **bloc — contenu** → *le lire quand*.

- **A1 — En-tête « Dernière MAJ » du 13 septembre 2026** → chronologie condensée de tout le projet.
- **A2 — Branche / HEAD / « source courante » (périmés)** → ce que le fichier affirmait au 16 mai 2026.
- **A3 — Tags publiés et releases GitHub v1.0.0 → v2.0.3** → commit ou URL d'une release antérieure à v2.0.3.
- **A4 — Historique des specs économiques (11 mai 2026)** → justifier le cap 18M et la récompense 42.86.
- **A5 — Analyses détaillées M-6 (DNSSEC), M-3 (.keys sans MAC), M-7 (padding 2500)** → rouvrir M-6/M-3/M-7, ou refaire la vérification DNSSEC.
- **A6 — Bug get_block_reward() — halving mal calculé (résolu 30 avril 2026)** → toucher get_block_reward ou ses call sites.
- **A7 — Récit de l'overflow uint64 MONEY_SUPPLY** → expliquer le choix de l'option (b) sur l'overflow.
- **A8 — ETAT DES PHASES — descriptions longues des phases 3C, 4A-C, 4D, 4E et 5** → commit ou date d'une phase / d'une étape PQ.
- **A9 — PROCHAINES ETAPES, items 1 à 12 (tous faits ou obsolètes)** → douter qu'un item du hard fork v2.0.0 ait été fait.
- **A10 — RELEASES v2.0.2 / v2.0.0 / v1.0.2 / v1.0.1 / v1.0.0 + les 4 punch lists** → vérifier un binaire publié ou refaire une release.
- **A11 — DOCKER ET DEPLOIEMENT (Flux — déprécié 19 mai 2026)** → rouvrir un déploiement conteneurisé, ou réutiliser la sonde P2P.
- **A12 — POOL MINING — stack monero-pool (écartée) + test local du 14 mai 2026** → envisager de remplacer cryptonote-nodejs-pool.
- **A13 — Release GUI v2.0.0-gui (23 mai 2026)** → chercher un asset GUI historique.
- **A14 — Releases GUI v2.0.1-gui et v2.0.2-gui + 4 bugs latents mac/win corrigés** → rebuild GUI macOS/Windows, ou chercher un SHA256.
- **A15 — POST-QUANTIQUE — Étapes 1 à 8 (mai–juin 2026) + audit du 5 juin** → savoir POURQUOI une constante PQ vaut ce qu'elle vaut.
- **A16 — Étapes A1–A4 et C2–C4 — dépense BQ transparente de bout en bout (25–26 juin 2026)** → toucher au chemin de dépense BQ.
- **A17 — 7 septembre 2026 — CRIT-1 (sorties BQ indépensables) + JSON invalide** → une sortie BQ introuvable, ou un explorateur qui ne parse plus.
- **A18 — 8 septembre 2026 — revue post-audit (MOYEN-4 RNG, cold-sign 1c, M-11, designs 2a/2b)** → toucher au hook randombytes ou au cold-sign.
- **A19 — 8 septembre 2026 — cold-sign BQ v4 + gate liboqs** → modifier le format unsigned-tx.
- **A20 — 8 septembre 2026 — CRIT-2 (création monétaire) + transactions hybrides** → toucher à l'équilibre RingCT.
- **A21 — 13 septembre 2026 — le gate ARM64 fermé par mesure, recette de test committée** → la CI macOS casse sur les tests standalone.
- **A22 — 13 septembre 2026 — M-11 réévalué, upgrade liboqs 0.16.0 prise** → avant le prochain bump de liboqs.
- **A23 — 12 septembre 2026 — spec 2e : binding C-1 + S1b, fin du résiduel CRIT-3** → toucher au format d'adresse BQ ou au bind tag.
- **A24 — 12 septembre 2026 — CRIT-4 corrigé : la racine PQ a sa propre seed (R2a)** → toucher à la dérivation des clés BQ.
- **A25 — 11 septembre 2026 — subaddresses BQ (B3) + CRIT-3 + CRIT-4 ouvert** → toucher au scan BQ.

## IDENTITÉ DU PROJET
Fork de Monero v0.18.1 rebrandé en HIDERING.
- Ticker : HRG
- Binaires : hideringd, hidering-wallet-cli, hidering-wallet-rpc
- Dernière release publique : **v2.0.3** → commit `c9b4f1d73` (19 juin 2026 — Linux x64 + macOS ARM64 + Windows x64, bundles complets daemon+wallet ; ajoute le checkpoint h=25000)
- Tags publiés et URLs de release (v1.0.0 → v2.0.3) : archivés, bloc **A3**. Branche/HEAD et « source courante » d'époque : bloc **A2**.
- Repo : https://github.com/AB-lab113/hidering
- Build actif : build/release/bin/

## SPECS ÉCONOMIQUES
- Supply max : **18,000,000 HRG** (hard cap irrévocable — décision 11 mai 2026, voir BUG CRITIQUE MONEY_SUPPLY)
- Supply circulant max : **18,000,000.00 HRG exact** (corrigé M-2, audit 10 juin — vérifié `blockchain.cpp:4427` : le coinbase genesis NUMS 157.14 HRG est **court-circuité** (`if (blockchain_height == 0)` → pas de `validate_miner_transaction`) donc JAMAIS ajouté à `already_generated_coins`, qui n'accumule que depuis le bloc 1 et plafonne à `MONEY_SUPPLY` = 18M. Le NUMS est non-spendable ET hors comptabilité d'émission → il ne se déduit PAS du cap. Les chiffres antérieurs **17,999,957.14** (et la correction 06-06 **17,999,842.86**) étaient FAUX. **M-2 entièrement clos** : GENESIS_PROOF.md + whitepaper site harmonisés le 13 juin (`f5c5d0ecd`).)
- Émission : **100% PoW** (algo RandomX, CPU-only) — pas de seed round, pas de vente privée, pas de token allocation équipe
- Phase pré-publique v2.0.0 : depuis le hard fork du 16 mai 2026 (post-attaque 51% sur v1.x à h≈3577), les nœuds fondateurs minent en réseau privé avant ré-ouverture publique. Volume + durée non encore fixés ; communication publique au launch. Voir whitepaper §7.1 et FAQ site web pour le narratif officiel. **NB :** la promesse "Premine : 0 HRG" du v1.x N'EST PLUS valable depuis v2.0.0 — toute doc résiduelle à harmoniser.
- Block time : 120 secondes
- Halving interval : 210,000 blocs (~2.66 ans)
- Récompense initiale : **42.86 HRG/bloc** (= 9,000,000 / 210,000 — série géométrique somme à 18M)
- Algorithme PoW : RandomX (CPU-only, ASIC-resistant)


## SPECS RÉSEAU
- Network ID (Mainnet) : **HRG\x02HIDERINGMAIN** (v2.0.0, source `src/cryptonote_config.h:255`)
- Magic bytes : **0x48524702** (v2.0.0)
- *Valeurs v1.x (historique, chaîne abandonnée à h≈3577) : NETWORK_ID `HRG\x01HIDERINGMAIN`, magic `0x48524701`*
- *Collision prefixe : testnet utilise déjà byte[3] = 0x02 (`HRG\x02HIDERINGTEST`). UUID complet reste unique via suffixe `MAIN`/`TEST` — handshake P2P compare le UUID 16 octets, pas le prefixe. Toute intégration tooling qui keyerait sur les 4 premiers octets seuls est ambigüe.*
- P2P Port : 19740
- RPC Port : 19741
- Préfixe adresses : 60 (adresses commençant par "B")
- Unlock window : 10 blocs

## GENESIS BLOCK
- Hauteur : 0
- Récompense : 157.14 HRG verrouillé NUMS (unspendable cryptographiquement)
- Unlock time : 60 blocs
- Preuve : GENESIS_PROOF.md dans le repo
- Fix critique appliqué : commit f6029d971 (HFv1 à hauteur 0 obligatoire)


## PRIVACY ENHANCEMENTS (TOUS COMPLETES)
- Patch 1 : Ring size dynamique 32-64 (vs Monero 16) OK
- Patch 2 : padding réseau hérité de Monero (granularité 1024 bytes) + TX padding interne fixe 2500 bytes (tx_extra, `cryptonote_tx_utils.cpp:554`, actif sur la chaîne live, hors garde HFv16) OK (audit M1 : la revendication « padding réseau fixe 2500 » était FAUSSE — le padding réseau levin reste celui de Monero à 1024 o ; seul le padding tx_extra interne vaut 2500 o ; requalifié doc+site le 6 juin 2026)
- Patch 3 : Routage stem multi-hops (biais best-effort vers ≥3 hops, par-dessus Dandelion++) — câblé le 6 juin 2026 (audit H3 : le compteur `hidering_hop_count` était incrémenté mais jamais sérialisé en sortie → feature inerte ; corrigé en propageant le compteur sur le chemin stem via `send_txs`/`make_tx_message`, fluff→MIN_HOPS). NB : « obligatoire/garanti » N'EST PAS atteignable (le force-stem est greffé sur Dandelion++ dont l'epoch décide in fine) → revendication requalifiée doc+site. Filet de sécurité : embargo D++ inchangé.
- Patch 4 : ~~Normalisation montants 0.1 HRG~~ **RETIRÉE** (audit H5, commit `61bf9dd5d` le 6 juin 2026) — sous-payait silencieusement le destinataire (arrondi inférieur → 0 si <0,1 HRG, reliquat au change) ET sans valeur privacy (les montants sont déjà masqués par RingCT/engagements de Pedersen). La confidentialité des montants repose sur RingCT (hérité). Doc+site requalifiés.
- Patch 5 : Stealth addresses V2 OK (NB : « view keys temporelles » initialement annoncées mais JAMAIS implémentées dans le code — revendication retirée de la doc + du site le 6 juin 2026, audit H4. Les adresses furtives one-time standard CryptoNote restent en place.)


## FICHIERS CRITIQUES
- src/cryptonote_config.h — config réseau, ring size, fees, magic bytes
- src/cryptonote_core/blockchain.cpp — logique blockchain (call sites de get_block_reward)
- src/hardforks/hardforks.cpp — schedule HFv15 (présent dès bloc 1)
- src/checkpoints/checkpoints.cpp — ancres mainnet h=2939/5000/11000/16000/20000/25000/80386/90000, hash-enforced depuis `934b43b01` (6 juin 2026) ; branches TESTNET/STAGENET **vidées** (M-1, 13 juin) — anciennes ancres Monero héritées retirées (auraient halté tout testnet/stagenet HRG sous enforcement)
- src/cryptonote_basic/cryptonote_basic_impl.cpp — impl. de base, **get_block_reward()** (halving)
- src/simplewallet/simplewallet.{h,cpp} — wallet CLI
- src/p2p/net_node.inl — réseau P2P ; seeds Tor/i2p Monero purgés (M-5, commit `eee5dec50` 12 juin 2026 : 6 `.onion` + 3 `.b32.i2p` retirés de `get_seed_nodes`, zones tor/i2p mainnet rendent `{}` ; support Tor/i2p `--proxy`/`--anonymous-inbound` conservé, pas encore de seeds cachés HRG)
- src/common/dns_utils.cpp — seeds DNS (Monero purgés)
- src/debug_utilities/dns_checks.cpp — neutralisé
- GENESIS_PROOF.md — preuve NUMS genesis
- assets/hrg_logo.svg — logo


## SÉCURITÉ — RISQUES ACCEPTÉS/DOCUMENTÉS
> **⚠️ NB nomenclature M-x — DEUX séries homonymes.** Les findings `M-n` de CETTE section (et du header « Dernière MAJ ») sont les **MOYENS (M-1..M-13) de l'audit complet du 10 juin 2026** (table de findings C/H/M/F ; source = session `272ab834`, résumée en prose dans [[project_full_audit_2026-06-10]]). Ils sont **distincts et SANS RAPPORT** avec les `M-1..M-6` de l'**audit PQC du 5 juin** (section POST-QUANTIQUE › Étape 8 : M-1/M-2 TLV tx_extra, M-3 memwipe secrets PQ, M-4 discipline at-rest, M-5 validation longueurs Dilithium, M-6 `HF_HEIGHT_PQ` 1M→2M). **M-3 / M-5 / M-6 sont des homonymes** entre les deux séries → toujours désambiguïser par la section. (Mémoires : `project_audit_findings_2026-06-06` + `MEMORY.md` = série backlog 10 juin ; `project_phase5_security_audit_fixes` + `project_full_audit_2026-06-10` = série audit PQC.)
> **État du backlog 10 juin (série MOYEN, vérifié 14 juin 2026)** — **FAITS (12/13)** : M-3 `.keys` sans MAC (risque accepté), M-5 seeds Tor/i2p purgés (`eee5dec50`), M-6 DNSSEC seeds (`fbf0e0600`), M-7 padding 2500 = non-problème (`a220b960d`), M-9 rapidjson CVE = faux positif, M-10/M-11 liboqs (`174c4beac`), **M-2** chiffres de supply corrigés → circulant max = **18 000 000,00 exact** (NUMS genesis hors `already_generated_coins`, `blockchain.cpp:4427`) dans CLAUDE.md + GENESIS_PROOF.md + whitepaper site (`b3f427705` + `f5c5d0ecd`), **M-12** `test_tx_utils.cpp` migré ML-DSA sig 3293→3309 (`592af786a`), **M-1** checkpoints testnet/stagenet Monero hérités **vidés** + guard underflow `pt.first-2` (`checkpoints.cpp` branches TESTNET/STAGENET → `return true` vides ; `blockchain.cpp:4739` clamp `>=2?:0`) — mainnet byte-identique (ancres h=2939…20000 intactes), testnet boote sans halt (smoke OK), M-13 RPC pool localhost, **M-4** clés BQ dérivées **déterministe­ment de la spend key** (`1473779b6`, 14 juin) → restore depuis seed 25 mots reproduit l'adresse BQ : nouveau `pqc_keygen_from_seed` (sous-seeds SHAKE256 domain-séparés ; ML-KEM via `keypair_derand`, ML-DSA — pas de keygen dérandomisé dans liboqs 0.15.0 — via RNG SHAKE256 sur le hook `randombytes` global d'OQS, mutex) ; signature `generate_pq_keys` inchangée ; gardé HFv16, chaîne live intacte ; **NB encore ouvert hors M-4 : dérivation des subaddress BQ** (seule l'adresse BQ primaire est dérivée), **M-14 OBSOLÈTE** (script `deploy-checkpoint-h11000.sh` à hash non vérifié → jamais commité, remplacé par la procédure standard `git pull + make -j$(nproc)` sur le VPS — cf. INFRASTRUCTURE VPS ; finding sans objet). **OUVERTS (1/13)** : **M-8** « ring = ensemble total » si `num_outs ≤ ~97` (`wallet2.cpp:9531-9540`) — non déclenché (chaîne >11K outputs), **item de surveillance au launch** de toute chaîne relancée vierge.
- *Renvois des deux lignes ci-dessus* : le **header « Dernière MAJ »** est archivé bloc **A1** ; la **série M-1..M-6 de l'audit PQC du 5 juin** (section POST-QUANTIQUE › Étape 8) est archivée bloc **A15**. Analyses détaillées de **M-6** (DNSSEC seeds), **M-3** (`.keys` sans MAC) et **M-7** (padding 2500 o) : archivées bloc **A5** — les trois sont closes ou acceptées, leur état tient dans la ligne ci-dessus.
- **M-10 : pas de vérification de provenance/intégrité au build de liboqs — risque FAIBLE, procédure de vérification documentée (13 juin 2026).** État vérifié : liboqs est intégré en **submodule git** (`external/liboqs`, `url=https://github.com/open-quantum-safe/liboqs`) **épinglé au SHA exact `97f6b86b1b6d109cfd43cf276ae39c2e776aed80`** (= tag `0.15.0`, croisé `git rev-parse 0.15.0^{commit}` == HEAD == SHA enregistré dans le parent). **Le SHA épinglé EST l'ancre d'intégrité** : git est content-addressé (SHA-1 + détection de collision sha1dc dans git moderne), donc tout checkout de l'arbre liboqs DOIT correspondre bit-pour-bit à ce SHA, lui-même committé dans notre repo — substituer du code malveillant exigerait soit une collision SHA-1, soit une modif du gitlink dans HRG (visible en revue/diff). **Ce qu'OQS ne fournit PAS** (vérifié 13 juin sur la release page + repo) : aucune signature GPG détachée (`.asc`/`.sig`), aucun checksum SHA256 publié pour les tarballs, et le tag `0.15.0` est **léger** (objet commit nu → `git tag -v` échoue « cannot verify a non-tag object », rien à vérifier). Seul élément cryptographique amont : le **commit** de release `97f6b86` est signé par la clé web-flow GitHub (`GPG key ID B5690EEEBB952194`) → atteste qu'il a été créé/mergé sur github.com dans le repo OQS (attribution, pas signature mainteneur). **Procédure de vérification simple (à exécuter sur tout clone/CI AVANT le build liboqs)** : `git -C external/liboqs rev-parse HEAD` == `5a1a854b0dc9f2141bdc771c555ee60c37950183` **ET** `git -C external/liboqs tag --points-at HEAD` contient `0.16.0` (valeurs **mises à jour le 13 sept. 2026** par l'upgrade 0.16.0 ; auparavant `97f6b86…` / `0.15.0`). Si l'un échoue → ne pas builder. (C'est déjà implicitement garanti par le submodule pin ; l'assert explicite est la matérialisation de M-10.) **Risque résiduel borné** : code PQC **inerte jusqu'à HFv16 (h=2,000,000)** → aucune surface d'attaque sur le mainnet actuel. À durcir d'un cran (pin SHA explicite en check CI + capture du `.a` hash) lors de la finalisation pré-HFv16. Lié à [[project_phase5_step9_fips_migration]].
- **M-11 : liboqs se décrit comme bibliothèque de prototypage/recherche, non certifiée production — risque ACCEPTÉ sous condition (réévaluation OBLIGATOIRE avant HFv16) (13 juin 2026).** Vérifié au commit épinglé `97f6b86` (0.15.0), le `README.md` (l.99) porte TOUJOURS l'avertissement : *« WE DO NOT CURRENTLY RECOMMEND RELYING ON THIS LIBRARY IN A PRODUCTION ENVIRONMENT OR TO PROTECT ANY SENSITIVE DATA. This library is meant to help with research and prototyping… has not received the level of auditing… necessary to rely on it for high security use. »* Le tagline officiel du repo reste *« C library for prototyping and experimenting with quantum-resistant cryptography »*. **0.15.0 n'a donc PAS abandonné le statut prototype** — la maturité n'a pas évolué malgré le passage aux algos FIPS-finaux. **Nuance importante** : l'avertissement vise l'**implémentation logicielle** (constant-time partiel, audit incomplet — cf. `SECURITY.md` : threat model couvrant *certains* canaux temporels seulement), PAS la maturité des **algorithmes** : ML-KEM-768/ML-DSA-65 sont standardisés FIPS-203/204, et liboqs 0.15.0 appelle les variantes finales (cf. Étape 9). Contexte de gouvernance : OQS est sous la **Post-Quantum Cryptography Alliance / Linux Foundation** (pas un projet abandonné). `SECURITY.md` : seule 0.15.0 supportée ; pré-0.12.0 = CVE HQC connue (on n'utilise NI HQC NI une version < 0.12). **Pourquoi acceptable aujourd'hui** : tout le code PQC est gardé `hf_version >= HF_VERSION_PQ` (16) et/ou `pq_keys` → **inerte jusqu'à HFv16 (h=2,000,000, cible T2 2027)** ; zéro TX PQC sur le mainnet actuel → l'implémentation prototype ne protège aucune donnée live. **CONDITION DE LEVÉE (bloquante avant activation HFv16, à joindre à la finalisation du binding C-1)** : (1) réévaluer le statut de maturité de liboqs à la version alors courante (le disclaimer a-t-il sauté ? audit tiers publié ?), (2) envisager une implémentation FIPS-validée (module CMVP) ou hybride (PQ + Ed25519, déjà la posture HRG : la dépense reste gardée par dlog Ed25519) plutôt que liboqs seul pour le matériel sensible, (3) figer la version + re-audit constant-time des chemins ML-KEM/ML-DSA réellement utilisés. **Ne PAS activer HFv16 tant que (1)–(3) ne sont pas tranchés.**

## CVE / FAUX POSITIFS DÉPENDANCES
- **CVE-2024-38517 (rapidjson, CVSS 7.8) — FAUX POSITIF, déjà corrigée dans notre arbre (vérifié 12 juin 2026).** Underflow d'entier dans `GenericReader::ParseNumber()` (`reader.h`, exposants négatifs : `exp + expFrac` peut passer sous `INT_MIN`). Le fix upstream est le commit Tencent **`8269bc2bc289e9d343bae51cdf6d23ef0950e001`** (« Prevent int underflow when parsing exponents », **mai 2018**) ; notre submodule `external/rapidjson` est pinné à `129d19ba7` (juillet 2018) qui en **descend** (`git merge-base --is-ancestor` OK ; le clamp `maxExp = (expFrac + 2147483639) / 10` est présent dans `include/rapidjson/reader.h:~1640`). La CVE n'a été assignée qu'en 2024 car rapidjson n'a jamais re-releasé après v1.1.0 (2016) : elle vise les distros packageant le tarball v1.1.0, PAS les pins de master post-mai-2018. **Les scanners (Snyk/Wiz/etc.) keyent sur la version « 1.1.0 » et flaggeront ce submodule à chaque audit → ne pas re-patcher.** Unique copie dans l'arbre (le repo GUI pointe sur le même backend via gitlink/symlink). NB : les références circulant pour cette CVE (issue `#2460`, commit `e3081d55`) sont fausses — 404 chez Tencent/rapidjson.

## BUG CRITIQUE — RESOLU (v1.0.1, commit dca7dd433, 12 mai 2026)
### MONEY_SUPPLY overflow uint64_t — option (b) appliquée : cap = 18M HRG
Localisation : src/cryptonote_config.h:54 et src/cryptonote_basic/cryptonote_basic_impl.cpp:85
Récit de l'overflow (comportement par plateforme, trois options évaluées) : archivé, bloc **A7**.

**Décision tranchée 11 mai 2026 — option (b) : cap = 18,000,000 HRG.**
Récompense initiale ajustée à 42.86 HRG/bloc pour que la série géométrique
des halvings converge sur 18M (= 9,000,000 / 210,000). Interval halving
inchangé (210K blocs, parallélisme Bitcoin préservé). 12 décimales atomiques
maintenues (compat Monero tooling : Cake, block explorers, etc.).

**Patch v1.0.1 appliqué (commit dca7dd433, 12 mai 2026) :**
- src/cryptonote_config.h:54 : `MONEY_SUPPLY ((uint64_t)18000000000000000000ULL)` (1.8e19, marge uint64 ~2.5%)
- src/cryptonote_config.h:56 : `INITIAL_BLOCK_REWARD ((uint64_t)42857142857143ULL)` (macro doc, alignée par cohérence)
- src/cryptonote_basic/cryptonote_basic_impl.cpp:85 : `MONEY_SUPPLY_LOCAL = 18000000000000000000ULL`
- src/cryptonote_basic/cryptonote_basic_impl.cpp:87 : `INITIAL_REWARD_LOCAL = 42857142857143ULL`

NB : `src/gen_genesis/gen_genesis.cpp:18` (`INITIAL_REWARD = 157140000000000ULL`)
**laissé inchangé** intentionnellement — y toucher modifierait le hash genesis et
invaliderait la chaîne déployée. Le whitepaper et GENESIS_PROOF.md **ont été
harmonisés 13 mai 2026** (commits `de1db6c5e` + `f7ba0b1dd`) : cap = 18M, reward
initial = 42.86 HRG, et le NUMS genesis reste explicité comme « 157.14 HRG locked,
héritage v1.0.0 » pour préserver la chaîne déployée.

Mémoires persistantes associées :
`~/.claude/projects/-home-shark-hidering/memory/project_money_supply_overflow_v1.0.0.md`
`~/.claude/projects/-home-shark-hidering/memory/project_v1.0.1_punch_list.md`

## ETAT DES PHASES
- Phase 0 : Setup, fork, compilation — COMPLETE
- Phase 1 : Core tokenomics, genesis, testnet 284 blocs — COMPLETE
- Phase 2 : Privacy enhancements (5 patches) — COMPLETE
- Phase 3A : Rebranding complet — COMPLETE
- Phase 3B : Validation, sécurité, purge git — COMPLETE
- Phase 3C : Infrastructure seed nodes — **COMPLETE** (v2.0.0 sur VPS dédiés, 19 mai 2026) — voir INFRASTRUCTURE VPS.
- Phase 4A-C : Mainnet public launch — **RESET v2.0.0** (chaîne v1.x abandonnée à h≈3577, décision du 16 mai 2026).
- Phase 4D : Binaires publics — **COMPLETE** (3 juin 2026) : daemon v2.0.2 et GUI v2.0.2-gui, Linux x64 + macOS ARM64 + Windows x64, CI-publiés.
- Phase 4E : Pool mining — **EN PRODUCTION** (25 mai 2026) — voir POOL MINING.
- Phase 5 : Post-quantique (ML-DSA-65 + ML-KEM-768) — **EN COURS**, cible hard fork mainnet T2 2027. Journal des étapes 1–9, A1–A4 et des audits : archivé, blocs **A15**, **A16** et **A17** à **A25**. État courant : voir « État Phase 5 au 21 septembre 2026 » en fin de fichier.

## PROCHAINES ETAPES (PAR ORDRE)
*(Items 1 à 12 archivés le 21 septembre 2026 sur décision de l'opérateur, non revérifiés un par un — bloc **A9**. Numérotation d'origine conservée ; seuls #13 et #14 restent ouverts.)*
13. Announce hard fork (Twitter, Reddit, BitcoinTalk) — communiquer activation + abandon chaîne v1.x
14. Phase 4 Launch (public officiel)

## REGLES DE TRAVAIL
1. Toujours travailler sur la branche v2-privacy
2. Ne jamais toucher aux fichiers external/ sans raison explicite
3. Build partiel : make -j$(nproc) hideringd
4. Build complet : cmake -D CMAKE_BUILD_TYPE=Release ../.. && make -j$(nproc)
5. Tester dans ~/.hideringtest (daemon testnet séparé)
6. Committer avec messages clairs : [HRG] description
7. Séquence redéploiement : local stable → Docker rebuild → Flux redeploy


## NE JAMAIS FAIRE
- Modifier les magic bytes réseau sans coordination
- Merger avec Monero upstream sans analyse complète des diffs
- Déployer seed nodes avant validation daemon locale
- Supprimer backup_wallets/ ou GENESIS_PROOF.md
- Committer des clés privées ou tokens GitHub
- Réécrire les valeurs attendues de `pq_vector_test` pour faire passer le test
- `git add monero` dans le repo `hidering-gui` — ça écrase le gitlink 160000 par un symlink absolu, cassé pour tout clone
- scp un binaire buildé sur WSL2 vers un VPS (recette : git pull + make SUR le VPS ; sur OVH, GLIBC 24.04 vs 22.04)
- Re-patcher rapidjson pour CVE-2024-38517 — faux positif, notre pin descend déjà du fix
- Activer HFv16 avant la levée de M-11 (1)(2)(3) et la finalisation du binding C-1
- Faire afficher une seed, une clé privée ou un mot de passe de wallet (commandes seed, spendkey, viewkey, fichiers .keys) par un wallet contenant des fonds, ou en laisser dans un transcript ou un fichier : pour tester un comportement, utiliser un wallet jetable créé pour l'occasion. Ne jamais ouvrir un wallet contenant des fonds sans instruction explicite. Si un secret s'affiche par accident : arrêter, le signaler, ne rien répéter.

## INFRASTRUCTURE VPS (19 Mai 2026 — REMPLACE Flux ; 4 hôtes au 21 mai)
- **Décision 19 mai 2026 :** abandon de Flux pour les seed nodes et le block explorer. Containers éphémères + IPs variables (vu 3 IPs en une session sur `hideringseed1`) = mauvais fit pour des entry points DNS stables. Migration vers VPS dédiés avec IPs invariantes et systemd pour restart auto.

### Inventaire 4 hôtes (vérifié 21 mai 2026 ; OS + SSH + recette deploy MAJ 12 juin 2026)
| Rôle | Hôte | IP | OS | User SSH | Binaire | Service | RPC publique ? |
|---|---|---|---|---|---|---|---|
| seed1 public | `seed1.hidering.org` | `135.125.243.137` (OVH) | Ubuntu 22.04 | `ubuntu` | `/home/ubuntu/hidering/build/bin/hideringd` | `hideringd.service` | ✅ (19741 → 0.0.0.0) |
| seed2 public | `seed2.hidering.org` | `207.180.211.96` (Contabo-1) | Ubuntu 24.04 | `root` | `/root/hidering/build/bin/hideringd` | `hideringd.service` | ✅ (19741 → 0.0.0.0) |
| relay privé | (pas de DNS) | `207.180.214.164` (Contabo-2) | Ubuntu 24.04 | `root` | `/root/hidering/build/bin/hideringd` | `hideringd.service` | 🔒 localhost only |
| relay privé | (pas de DNS) | `167.86.74.202` (Contabo-3) | Ubuntu 24.04 | `root` | `/root/hidering/build/bin/hideringd` | `hideringd.service` | 🔒 localhost only |

**Auth SSH (MAJ 12 juin 2026) :** key-only (ed25519) sur les 4 hôtes — plus de password. **Recette de déploiement canonique : git pull + make SUR le VPS, JAMAIS de scp de binaires depuis WSL2.** Raison : OVH est en Ubuntu 22.04 → un binaire buildé sur WSL2 (24.04, glibc 2.39) ne tourne PAS dessus (GLIBC incompatible) ; sur les Contabo (24.04) le build local au VPS reste plus sûr (mêmes libs dynamiques). Séquence : `ssh ubuntu@135.125.243.137` (ou `root@<ip-contabo>`) → `cd ~/hidering && git pull` → `cd build` (**attention : le build dir VPS est `~/hidering/build`, PAS `build/release` — brûlé le 12 juin sur OVH**) → `make -j$(nproc) daemon` (wallet : `make -j2` obligatoire partout, OOM `wallet2.cpp`) → `[sudo] systemctl restart hideringd.service` (`sudo` sur OVH uniquement). **Gotcha Contabo-2 (brûlé 12 juin 2026)** : son daemon mine (`--mining-threads 6`, dataset RandomX ~2,3 GB) + stack pool (Redis/PM2/nginx) → `cc1plus` OOM-killed sur les grosses TU (`core_rpc_server.cpp`) même en `-j1`. Séquence obligatoire sur Contabo-2 : `systemctl stop hideringd` → `make -j2 daemon` → `systemctl start hideringd` (le pool re-health-check tout seul, vérifier `curl localhost:8117/stats` → `daemon_ok`). Contabo-1 peut aussi OOM sporadiquement en `-j$(nproc)` → retry en `-j2`. *Ancienne recette scp + password (21 mai 2026) OBSOLÈTE.*

- **Block explorer public : https://explorer.hidering.org** — instance fraîche indexant la chaîne v2.0.0 depuis genesis. Remplace l'ancienne instance Flux `hrgexplorer.app.runonflux.io` (retirée). Le port 8080 derrière nginx sert toujours le backend Flask en clair (HTTP 200) — pas de redirect, à fermer/binder localhost si on veut couper le double accès.
- **Unit file systemd canonique (vérifié 20 mai 2026)** : `/etc/systemd/system/hideringd.service`, `Type=simple` (pas de `--detach`), `Restart=always RestartSec=10`. Flags daemon harmonisés sur les nœuds publics (OVH + Contabo-1) : `--non-interactive --log-level 0 --p2p-bind-ip 0.0.0.0 --p2p-bind-port 19740 --rpc-bind-ip 0.0.0.0 --rpc-bind-port 19741 --confirm-external-bind --restricted-rpc`. **Contabo-2 et Contabo-3 : RPC localhost CONFIRMÉ (M-13 mitigé, vérifié 12 juin 2026).** Contabo-2 (daemon pool) tourne **sans** `--restricted-rpc` (intentionnel — `getblocktemplate` requis par le pool) mais avec `--rpc-bind-ip 127.0.0.1` explicite dans l'unit → `ss -tlnp` montre 19741/19742 (ZMQ)/19743 (wallet-rpc)/6379 (redis) tous sur 127.0.0.1, et un probe externe `curl 207.180.214.164:19741` échoue (connection refused). Seuls ports publics : 3333 (stratum), 19740 (P2P), 22, 80 (nginx), 8117 (API pool). Pas de règle iptables (policy ACCEPT) ni ufw — la protection EST le bind localhost ; si un jour le bind change, ajouter un firewall AVANT. Contabo-3 : 19741 sur 127.0.0.1 + `--restricted-rpc` (double protection). NB : l'unit Contabo-2 porte aussi `--start-mining B5TLmi…` (6 threads) + 4 priority nodes. `--restricted-rpc` ajouté à seed1 OVH le 20 mai 2026 (avant : RPC complète exposée publiquement, leak version/peers/free_space) ; après restart le daemon a repris sa sync immédiatement, peers reconnectés en <30s. Backup pré-modif conservé en `/etc/systemd/system/hideringd.service.bak.20260520-*` sur OVH.
- **DNS (records A) :** à jour côté registrar (vérifié 19 mai 2026) :
  - `seed1.hidering.org` → `135.125.243.137` (OVH)
  - `seed2.hidering.org` → `207.180.211.96` (Contabo)
  - `explorer.hidering.org` → `135.125.243.137` (OVH — même VPS que seed1, sert l'UI block explorer sur le port 8080)
  Les binaires v2.0.0 publiés et `docs/index.html` pointent directement vers ces noms DNS, plus jamais vers les IPs nues.
- **Source de vérité pour les IPs :** les IPs sont fixes, **on peut les hard-coder**. (À l'inverse, Flux exigeait `curl https://api.runonflux.io/apps/location/hideringseed1` à chaque fois — plus jamais nécessaire post-migration.)

## DOCKER ET DEPLOIEMENT (Flux — déprécié 19 Mai 2026)
Flux abandonné le 19 mai 2026 au profit des VPS dédiés (voir INFRASTRUCTURE VPS ci-dessus). Image, digests, spec de l'app, gotcha de redeploy ZelCore et **sonde P2P d'identification du binaire réellement actif sur un nœud distant** : archivés, bloc **A11**.

## POOL MINING (Phase 4E — EN PRODUCTION)
Pool publique `pool.hidering.org:3333` sur Contabo-2, stack **cryptonote-nodejs-pool** + Redis + Nginx + PM2. La stack `monero-pool` (jtgrassie), validée en local le 14 mai 2026 mais **non retenue**, avec sa recette de build et ses valeurs de `pool.conf` : archivée, bloc **A12**.

### Déploiement cryptonote-nodejs-pool — Contabo-2 (25 Mai 2026 — OPÉRATIONNELLE)
Stack alternatif au monero-pool validé : **cryptonote-nodejs-pool (dvandal)** + Redis + Nginx + PM2, sur Contabo-2 (`root@207.180.214.164`, daemon RPC localhost:19741). Repo cloné dans `/opt/hrg-pool`. Choix utilisateur (Redis-only, plus léger que MoneroOcean nodejs-pool). Artefacts de déploiement hors repo : `~/hrg-pool-deploy/` (deploy script, `config.json` validée, NOTES.md, `ovh-add-pool-dns.py`). **NB (MAJ 12 juin 2026) : SSH désormais key-only (ed25519) sur Contabo-2 — déploiement direct possible (cf. [[infra_vps_inventory_4hosts]]).**
- **GOTCHA redis (brûlé 25 mai) : `package.json` upstream épingle `"redis": "*"`** → `npm install` tire node_redis v4/v5 (`@redis/client`), incompatible avec le code qui suppose l'API v3 (callback : `redisClient.info(cb)`, `createClient(port, host, {auth_pass})`, `.multi().exec(cb)` partout dans `lib/*.js`). Symptôme : `Error: The client is closed` dans `checkRedisVersion` (`init.js:147`). **Fix : pin v3 →** `cd /opt/hrg-pool && npm install redis@3.1.2 --save`. Patcher `init.js` seul (ajouter `await client.connect()`) est un **piège** : l'API callback v3 est utilisée dans api.js/pool.js/paymentProcessor.js/blockUnlocker.js/charts.js — ça casserait ailleurs aussitôt.
- **Gotcha config.json** : `poolAddress` est imbriqué sous `config.poolServer` (pas à la racine). `lib/configReader.js` lit aussi `config.blockUnlocker.devDonation` + `config.symbol` AVANT `init.js:27`, donc tout manque de bloc remonte une erreur `Cannot read properties of undefined`. Structure de référence = `config_examples/monero.json` du repo (coin RandomX). Valeurs HRG : `cnAlgorithm:"randomx"`, `isRandomX:true`, `coinUnits:1e12`, `coinDifficultyTarget:120`, `blockUnlocker.depth:60` (= `CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW`, **pas 10** comme indiqué à tort dans SPECS RÉSEAU), `intAddressPrefix:61`, prefix adresse 60.
- **Bloqueurs runtime — TOUS LEVÉS (validé 25 mai)** : (1) le daemon local servant le pool tourne **sans** `--restricted-rpc` → `getblocktemplate` OK (sinon le pool ne minerait pas ; cf. [[project_restricted_rpc_zeroes_peer_counts]]) ; (2) `hidering-wallet-rpc` up sur 127.0.0.1:19743 — confirmé via le health-monitor du pool (`/stats` → `health.HIDERING.wallet:"ok"`, le `getbalance` passe → wallet opérateur ouvert) ; (3) `payments.mixin:31` (ring 32) car HRG impose ring 32-64, pas le défaut Monero (7) — **un vrai `transfer` de payout n'a pas encore été exercé**, à reconfirmer au 1er payout réel ≥1 HRG.
- DNS : `pool.hidering.org` → `207.180.214.164` — record A **ajouté côté OVH**, résout (1.1.1.1 / 8.8.8.8). Site : section « Mining Pool » publiée sur hidering.org (commit `36b6acaf6` + fix layout flex `b3720eb41`, redeploy Vercel auto depuis v2-privacy). Script DNS de secours si à refaire : `~/hrg-pool-deploy/ovh-add-pool-dns.py` (besoin de creds OVH).
- **Validation minage end-to-end (25 mai)** : xmrig 6.22.0 2 threads → **16 shares acceptées / 0 rejetée**, varDiff a grimpé 1000→2000→4000, ~304 H/s ; `/stats` a reflété `miners:1, workers:1, hashrate:107` en live. Stratum public `:3333`, API publique `:8117/stats`. `lastblock` reward = `42857142857143` atomes = **42.857 HRG** (émission v1.0.1 confirmée live sur la chaîne v2.0.0). Commande mineur : `xmrig -o pool.hidering.org:3333 -u <B-address> -p x -a rx/0`. État au test : `totalBlocks:0` (diff réseau ~13,4 M, aucun bloc réel attendu à ce hashrate).

## HIDERING WALLET GUI (Phase 4D — COMPLETE 3 Juin 2026)
Sister repo : **github.com/AB-lab113/hidering-gui** (public, créé 23 mai). Fork de `monero-project/monero-gui` @4b94d7e + rebrand complet HRG. Local : `~/hidering-gui` master @`9c737436` (commits : `c32f380` initial fork + rebrand, `79b12384` `scripts/build-appimage.sh` + .gitignore initial, `9c737436` .gitignore étendu pour artefacts AppImage). **Recipe AppImage reproductible : `./scripts/build-appimage.sh v2.0.1`** depuis une base glibc 2.39 (Ubuntu 24.04+) — bundling via **linuxdeploy + linuxdeploy-plugin-qt** (PAS linuxdeployqt, cassé — voir gotcha) ; requiert **patchelf** (auto-installé par le script).
- Backend : `monero/` submodule pointe vers `AB-lab113/hidering@v2-privacy` (gitlink 160000, commit `93d7d3b46`). Localement `~/hidering-gui/monero` est un **symlink vers `~/hidering`** pour build incrémental rapide ; git ignore le symlink car l'index contient un gitlink. **Ne JAMAIS `git add monero`** dans hidering-gui — `git add -A` écrase le gitlink par un symlink 120000 (absolute path → cassé pour tout clone). Si ça arrive : `git rm --cached monero && git update-index --add --cacheinfo 160000,<backend-HEAD>,monero`.
- Build recipe (WSL2 7.4 GB RAM) :
  ```bash
  cd ~/hidering-gui/build
  cmake .. -DCMAKE_BUILD_TYPE=Debug -DMANUAL_SUBMODULES=ON   # MANUAL_SUBMODULES=ON requis
  make -j2   # NOT -j$(nproc) — -j8 OOM-kill à 58% (cc1plus ~1 GB/job × 8 > 7.4 GB)
  export LD_LIBRARY_PATH=$(find . -name "*.so" -exec dirname {} \; | sort -u | tr '\n' ':')
  ./bin/hidering-wallet-gui
  ```
- **Ne PAS passer `-DDEV_MODE=ON`** : ça force C++17 (utile) mais déclenche `git checkout -f origin/master` dans `monero/` (catastrophique sur notre fork). Pour C++17 ciblé, patch `src/openpgp/CMakeLists.txt` avec `target_compile_features(openpgp PUBLIC cxx_std_17)` (déjà committé) — requis car epee/span.h utilise `std::is_standard_layout_v` ; si un autre target GUI pull span.h, ajouter la même ligne.
- Rebrand mécanique : sed perl boundary-safe `\bMonero\b(?!::) → Hidering` + variantes lower/UPPER/XMR/xmr sur 97 QML/JS + 246 .cpp/.h + 48 translations/*.ts. **0 résidu Monero/XMR**. Préservés à dessein : `namespace Monero::` (backend ABI), `MoneroComponents` (namespace QML, 1435 refs), `MoneroSettings` (classe interne GUI).
- **Sed trap brûlé une fois** : `\bMonero\b(?!::)` NE protège PAS `namespace Monero { ... }` (le caractère suivant ` {`, pas `::`). 5 headers ont eu leur forward-decl cassée (TransactionHistory/Wallet/AddressBook/WalletManager/PendingTransaction). Restaurés manuellement. Pour tout futur sed sur ce repo : protéger aussi `namespace Monero\b` et `using namespace Monero\b`.
- Réseau : daemon RPC port défaut `18081 → 19741` mainnet (`+stagenet 39741, testnet 29741`). Backend submodule fournit prefix 60, NETWORK_ID `HRG\x02HIDERINGMAIN`.
- Visuel : palette gold #FFD700 sur noir/dark, app icons (8 tailles + .ico multi-size) générés depuis `~/hidering/assets/hrg_logo.png` via ImageMagick, splash SVG `hidering-vector.svg` HRG cercle gold.
- **Gotchas de bundling et de link — valables pour tout rebuild** (extraits de la release v2.0.0-gui, archivée bloc **A13**) :
  - **Surprise Release build** : les libs HRG (libwallet_api, libepee, libopenpgp, libcommon, libnet, libversion, libtranslations, libeasylogging) sont **statically linked** dans `hidering-wallet-gui` en mode Release (find . -name "*.so" sort uniquement Qt5 + system libs). Debug build les laisse dynamic. Mécanisme cmake exact pas identifié — possiblement cmake target type default ou interaction `-fPIC`.
  - **Gotcha AppImage (runtime download)** : le download appimagetool du runtime-x86_64 timeout/502 souvent depuis github — workaround : `gh release download continuous --repo AppImage/type2-runtime --pattern runtime-x86_64` + `appimagetool --runtime-file ./runtime-x86_64 AppDir <output>.AppImage`.
  - **GOTCHA linuxdeployqt CASSÉ (brûlé 25 mai 2026, 3 builds ratés)** : `linuxdeployqt` « continuous » build 107 (2025-10) détecte les libs Qt via ldd mais **ne les copie jamais** dans l'AppDir (0 lib Qt5, pas de plugin platforms) → `appimagetool` échoue ensuite « Desktop file not found ». Pas un problème de qmake/patchelf/PATH (tous vérifiés OK). **Solution : bundler avec linuxdeploy + linuxdeploy-plugin-qt** (TheAssassin) à la place — bundle Qt correctement (16 libs + libqxcb + AppRun wrapper + qt.conf + rpath `$ORIGIN/../lib`). Ces outils **requièrent patchelf**, absent par défaut → `pip install --user --break-system-packages patchelf` (PEP 668). `scripts/build-appimage.sh` réécrit en conséquence (commit GUI `80008d1f`) : DL linuxdeploy/plugin-qt/appimagetool (`--appimage-extract`), `QMAKE=/usr/lib/qt5/bin/qmake QML_SOURCES_PATHS=$REPO linuxdeploy --appdir AppDir -e <bin> -i <icon> --icon-filename hidering-wallet-gui -d <desktop> --plugin qt`, ajoute `libqoffscreen.so` (smoke test headless via `QT_QPA_PLATFORM=offscreen`), packe avec `appimagetool --runtime-file`. Les `ERROR: Missing qml module: moneroComponents.*` au bundling sont **bénins** (types QML enregistrés en C++, pas des modules `.qml` disque).
  - **NB sed-trap historique** : `-extra-plugins=platforms/libqxcb.so` (slash dans le nom) crashait linuxdeployqt — non pertinent désormais (linuxdeploy gère les plugins via `--plugin qt`).
- **Build CI multi-plateformes** (workflows `build-{macos,windows}-gui.yml` ; assets, SHA256 et les 4 bugs latents mac/win corrigés : bloc **A14**) :
  - Recette CI : liboqs buildé en premier dans `monero/external/liboqs` (pqc.cpp inclut oqs.h inconditionnellement), `-DMANUAL_SUBMODULES=ON -DDEV_MODE=OFF -DUSE_DEVICE_TREZOR=OFF`, link dynamique. Release jobs (concurrency group partagé `hidering-gui-release-<ref>`) attachent les assets au tag `v*`.
- **TODO Phase 4D GUI restantes** :
  1. ~~macOS .dmg + Windows .exe (billing GH Actions)~~ → **FAIT autrement 3 juin 2026** : CI livre `.app` (tar.gz) + Windows portable (`.zip`). Reste optionnel : vrais installers .dmg/.exe + signature notarisée (actuellement codesign ad-hoc macOS, .exe non signé).
  2. Vraie wordmark SVG vectorielle (actuellement raster ImageMagick `convert -annotate text`)
  3. `appicon.icns` macOS
  4. Retirer ou remplacer le module `qt/updater` (inerte : pointe vers `:/hidering/utils/gpg_keys/` qui n'existe pas, devkeys Monero retirées)
  5. QA traduction multilingue (sed touche 48 `.ts` mais pas de pass native-speaker)
- Mémoire associée : `~/.claude/projects/-home-shark-hidering/memory/project_hidering_gui_fork.md`

## POST-QUANTIQUE (PHASE 5 — 2027)
Hard fork additif (n'altère pas la blockchain existante) :
- **ML-DSA-65** (FIPS 204, ex-Dilithium3) — signatures
- **ML-KEM-768** (FIPS 203, ex-Kyber768) — échange de clés
- **Migration FIPS ✅ 9 juin 2026 (Étape 9)** : liboqs 0.10.1→0.15.0, `OQS_SIG_alg_ml_dsa_65`/`OQS_KEM_alg_ml_kem_768` (les Round 3 pré-FIPS ne sont plus appelés). Tailles ML-DSA-65 : sk 4000→**4032**, sig 3293→**3309** (pk 1952 inchangé ; champ tx_extra PQ_SIG = 5262 o tag inclus). Tailles ML-KEM-768 = identiques à Kyber768 (1184/2400/1088/32) → adresse BQ (1249 o), blob `pq_keys` (3584 o) et CT (1088 o) inchangés en taille, mais **clés/sigs/CT incompatibles avec les versions draft** (wallets BQ expérimentaux à régénérer ; aucun impact chaîne, HFv16 inactif). Constantes renommées `ML_DSA_65_*`/`ML_KEM_768_*` ; noms de membres/clés KV historiques (`dilithium3_pk`, `kyber_sk`, `"pq_dilithium"`, `"pq_keys"`) **conservés** pour ne pas casser le format wallet. NB : les sections Étapes 1–8 ci-dessous sont des archives historiques et conservent la terminologie Dilithium3/Kyber768 de l'époque.
- Calendrier : Étapes 1–7 (implémentation) ✅ mai–juin 2026 + **audit de sécurité ✅ 5 juin 2026** (18 findings corrigés, voir Étape 8) → finalisation spec binding PQ + tests end-to-end T1 2027 → hard fork mainnet T2 2027
- **Statut : prototype durci, NON production-final.** Tout le code PQ reste inerte (`hf_version >= HF_VERSION_PQ` (16) et/ou `pq_keys`) jusqu'à HFv16, désormais à **h=2,000,000** (placeholder repoussé de 1,000,000 par l'audit M-6). **Ne PAS activer HFv16 avant la finalisation du binding C-1** (voir Étape 8).

*Renvois de la section ci-dessus* : **Étapes 1 à 8** (implémentation mai–juin 2026 et audit du 5 juin, dont les 18 findings) → archivées bloc **A15** ; **Étape 9** (migration FIPS) → décrite en entier dans la ligne « Migration FIPS » ci-dessus ; **Étapes A1–A4 / C2–C4** → bloc **A16**. Le binding C-1 dont parle le statut est livré : voir « État Phase 5 au 21 septembre 2026 » juste en dessous.

### État Phase 5 au 21 septembre 2026 (synthèse)
Ce bloc remplace neuf sections datées du 7 au 13 septembre 2026, archivées **A17** à **A25** ; les rapports `docs/audit/*.md` qu'elles citent restent dans le dépôt. Synthèse de ce que ces sections établissaient à leur dernière date (13 septembre 2026), rédigée le 21 septembre ; à revérifier dans le code et dans `docs/audit/` avant toute décision.

**Format d'adresse et binding C-1 (spec 2e, 12 septembre 2026 — `docs/audit/spec_2e_bq_address_auth_binding.md`)**
- Adresse BQ : `marker(1) ‖ auth_ver(1) ‖ B ‖ A ‖ kem_pk ‖ auth_commit(32)` = **1282 o → 1770 caractères** (mesuré). `auth_ver` est placé **avant tout champ interprétable**, pour qu'un parseur rejette une version inconnue sans avoir rien lu.
- `auth_commit = Keccak("HRG_BQ_ADDR_AUTH_v1" ‖ auth_ver ‖ auth_pk)` — le type est DANS le préimage, donc deux types ne peuvent pas collisionner. Le consensus traite l'engagement comme **opaque**, ce qui rend T3 ajoutable sans hard fork.
- Binding : `bind_tag v2 = Keccak("HRG_PQ_BIND_v2" ‖ auth_ver ‖ P' ‖ auth_commit ‖ auth_blind)`, publié à la création par l'émetteur, recalculé à la dépense depuis la clé révélée. **`auth_blind` est indispensable** : sans lui, révéler `auth_pk` une seule fois permettrait d'identifier **toutes** les sorties jamais envoyées à cette subaddress. C'est une image à sens unique de `ss`, qui n'est jamais révélé.
- Signature de compte `0x06` **supprimée** de `tx_extra` → budget **7 sorties BQ par dépense** (mesuré), `MAX_TX_EXTRA_SIZE_PQ` inchangé à 8192. Pin wire de `txin_to_key_pq` = **5512 o**.

**Checks de consensus d'une entrée BQ** — (a) la sortie existe et n'est pas dépensée ; (b) la clé révélée correspond à la clé on-chain ; **(b2)** l'engagement vaut `amount*H + mask*G` — ferme CRIT-2, la création monétaire ; (c) le bind tag ; (d) la signature ML-DSA-65 ; **(d2)** `owner_sig` Ed25519 par la clé unique `x'` — ferme CRIT-3 contre un émetteur classique ; (e) la conservation monétaire. (c)+(d) et (d2) sont **complémentaires, aucun n'est redondant** : sans d2 un émetteur engage son propre engagement, sans c/d un quantique forge `x'`. **Résiduel CRIT-3 FERMÉ**, avec contrôle négatif explicite : un émetteur à qui l'on **donne** `x'` (ce que Shor lui rendrait) et qui détient `ss` satisfait (b)(b2)(d)(d2)(e) et **échoue sur (c) seul**.

**Formes de transaction** — une dépense BQ pure est une tx **v2 `RCTTypeNull` à montants révélés** ; une tx **hybride** (entrées ring + entrées BQ) est une vraie tx RingCT et se révèle donc **plus privée** que la dépense BQ pure. Règle d'ordre consensus : toute entrée ring précède toute entrée PQ.

**Racine PQ — CRIT-4 fermé (12 septembre 2026, décision R2a)** — `account_keys::pq_root` : 32 octets d'entropie indépendante du CSPRNG système, sauvegardés par **leur propre liste de 25 mots** (deux mnémoniques). `get_pq_root_secret` ne retombe **jamais** sur la clé de dépense. Fail-closed : restaurer avec `--bq-wallet` sans seed BQ est **refusé** dans `wallet2::generate` ; un `.keys` portant `pq_keys` mais pas `pq_root` est **refusé au load**. Une même seed classique rend la même adresse `B...`.

**T2 imposée par le wallet** — `m_pq_spent_subaddresses` marque **à la construction** ; R-a refuse explicitement dans `get_pq_subaddress_as_str`, R-b envoie le change vers une subaddress BQ **neuve**, R-c refuse de fusionner deux subaddresses BQ sans opt-in. Le change d'une dépense BQ n'atterrit plus ni sur l'index primaire ni sur une sortie classique `B...` (R-b : chemin normal sans test direct — voir « Résidus connus » en fin de fichier).

**liboqs** — submodule épinglé `5a1a854b0dc9f2141bdc771c555ee60c37950183` (tag **0.16.0**, upgrade prise le 13 septembre 2026). Le vecteur figé de `pq_vector_test` vaut pour **0.15.0 ET 0.16.0**, valeurs inchangées au bit près. **M-11 reste bloquant pour HFv16** : disclaimer « not for production » intact, aucune validation FIPS 140-3, et toujours **pas de `keypair_derand` pour SIG** → la dérivation reste suspendue au hook `randombytes`, risque structurel pour les bumps futurs.

**Tests (au 13 septembre 2026)** — suite standalone = **15 tests, tous verts** ; la recette de build est committée dans `tests/standalone/build_pq_test.sh`. `pq_vector_test` tourne en **gate** sur le runner `macos-14` (Apple Silicon) à chaque build macOS : run `34766997637` du 13 septembre 2026, `uname -m` = arm64, les 4 groupes de vecteurs PASS — les vecteurs figés sur x86_64 se reproduisent **bit pour bit** sur ARM64.

**Résidus connus post spec_2e (12 sept 2026) :**
- Cold-sign (`confirm_pq_output(require_binding=false)`) ne bénéficie pas de la
  détection §3.4 (émetteur ayant engagé son propre auth_commit) — la machine
  froide n'a pas la tx de création. Compromis accepté, pas corrigé.
- R-b (redirection du change vers une subaddress BQ neuve) n'a qu'un test de
  garde-fou (§4.3, échec si change non-BQ) — le chemin normal n'est pas testé
  en isolation, ça demande un daemon. Backlog, pas bloquant tant que le
  garde-fou tient.
- ~~Backend ML-DSA aarch64 (0.16.0) vérifié identique par hash (source uniquement,
  cette machine est x86_64) — jamais mesuré en exécution réelle sur ARM.~~
  **FERMÉ le 13 sept. 2026** : `pq_vector_test` tourne désormais sur `macos-14`
  (Apple Silicon) à chaque build macOS, comme **gate** du job — run `34766997637`,
  `uname -m` = arm64, les 4 groupes de vecteurs PASS. Les vecteurs figés sur x86_64
  se reproduisent **bit pour bit** sur ARM64. Détail : `liboqs_0.16.0_upgrade_gate.md` §13.
