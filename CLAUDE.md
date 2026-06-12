# HIDERING (HRG) — Claude Code Project Memory
**Version synchronisée : Whitepaper v1.3 + Roadmap v2.0 — 30 Avril 2026**
**Dernière MAJ : 12 Juin 2026 (checkpoint mainnet h=20000 ancré + déployé — commit `df7211b2d` sur v2-privacy : `ADD_CHECKPOINT2(20000, "5f7832fd…a54e", "0x56176a690c")` dans `src/checkpoints/checkpoints.cpp`, hash croisé seed1 OVH + seed2 Contabo. Déploiement git pull + make daemon + restart sur les 4 VPS (OVH → Contabo-1/-2/-3), tous `active` à HEAD `df7211b2d`, seeds alignés h=20480. NB : l'enforcement des checkpoints est RÉEL depuis le commit `934b43b01` (6 juin 2026, réactivation du hash-check `check_block()` court-circuité relevé par l'audit) — appelé main chain (`blockchain.cpp:4175`), alt blocks (`:1934`) et load DB (`:4732`). Gotcha build OVH : le build dir y est `~/hidering/build`, pas `build/release`. — Antérieur 4 Juin : Phase 5 Étape 6 DONE — keygen BQ + préfixe mnemonic "BQ" + détection des sorties BQ, commit `62cb02066` sur v2-privacy. Préfixe `0x3C11`→**62** + octet marqueur `0x33` dans le payload → l'adresse commence vraiment par "BQ" (prouvé : un préfixe seul ne le peut pas ; 62 collisionne avec le préfixe subaddress, désambiguïsé par taille payload 64 vs 1249 o). `generate_pq_keys`/`get_pq_address_str`/`get_keys_nonconst` ; `wallet2::generate(use_pq=false)` ; détection par dé-tweak `P−t·G` avant `is_out_to_acc_precomp` gardée par `pq_keys`. Build 0 nouveau warning, regtest 2 blocs clean, `pq_keygen_test.cpp` PASS. Items ouverts : `pq_keys` non sérialisé (clés BQ éphémères/session), subaddress BQ, cold-sign hf, bump `MAX_TX_EXTRA_SIZE` à HFv16. Voir POST-QUANTIQUE Étape 6. — Antérieur : Étape 5 DONE — format d'adresse BQ... + plumbing des clés Kyber768 côté wallet, commit `b77639979` sur v2-privacy, gardé par HFv16 + flag `is_pq`, chaîne hf 15 byte-identique. `account_public_address` porte désormais une clé Kyber768 optionnelle de 1184 octets (absente des deux maps de sérialisation → format B... inchangé) ; encaps utilise la vraie clé destinataire (throwaway Étape 4 retiré) ; caveat 2 résolu sur les 2 call sites online. Items ouverts Étape 5 : (1) rien ne set `account_keys.pq_keys` encore → keygen BQ + subaddress à faire (le tweak de scan est donc dead code), (2) préfixe `0x3C11` ne rend PAS "BQ" en base58 (rend "RLF..." sur payload 1184 o) → tuning préfixe mnemonic à faire, (3) cold-signing HFv16 nécessite `hf_version` dans les unsigned-tx exportées. Voir POST-QUANTIQUE Étape 5. — Antérieur 4 Juin : Étape 4 DONE (dérivation stealth BQ... via Kyber768, commit `0c489dbdc`). — Antérieur 3 Juin : Phase 4D GUI COMPLETE — release multi-plateformes `v2.0.2-gui` publiée : macOS ARM64 + Windows x64 buildés par CI GitHub Actions (workflows `build-{macos,windows}-gui.yml` sur `AB-lab113/hidering-gui` master) + Linux AppImage report de v2.0.1-gui. 4 bugs latents mac/win corrigés (Deploy.cmake + Info.plist target rebrand, gitlink monero pré-liboqs, link Windows ole32/shell32). Site hidering.org MAJ : GUI 3 OS, roadmap Phase 4H GUI Desktop = Complete, Phase 5 PQ = In Progress, mentions exchanges retirées. Voir RELEASE v2.0.2-gui + section HIDERING WALLET GUI. — Antérieur 23 Mai : Release Linux v2.0.0-gui du Hidering Wallet GUI publiée sur github.com/AB-lab113/hidering-gui/releases/tag/v2.0.0-gui : AppImage 54 MB self-contained + tar.gz 11 MB binaire dyn. Build reproductible via `scripts/build-appimage.sh` committé à HEAD `9c737436`. Voir section HIDERING WALLET GUI ci-dessous. — Plus tôt 23 mai : fork + rebrand + push initial. 21 mai : checkpoint mainnet h=5000 + 4-VPS inventory. 20 mai : harmonisation `--restricted-rpc`. 19 mai : MIGRATION INFRA Flux → VPS dédiés.)**

## IDENTITÉ DU PROJET
Fork de Monero v0.18.1 rebrandé en HIDERING.
- Ticker : HRG
- Binaires : hideringd, hidering-wallet-cli, hidering-wallet-rpc
- Branche active : v2-privacy (HEAD : 41f899049 au 16 mai 2026 + 2 fichiers uncommitted pour v2.0.0 : `src/cryptonote_config.h` NETWORK_ID byte[3] et `src/version.cpp.in` version string)
- Source courante : v2.0.2 (tag `c37d5e285`) — `DEF_MONERO_VERSION "2.0.2"`, binaires publics multi-plateformes CI-publiés (cf. RELEASE v2.0.2)
- Dernière release publique : **v2.0.2** → commit c37d5e285 (Linux x64 + macOS ARM64 + Windows x64, bundles complets daemon+wallet)
- Tags publiés :
  - **v2.0.2** → commit c37d5e285 — bundles complets (hideringd + wallet-cli + -rpc) Linux/macOS-arm64/Windows, CI-auto-publiés ; version bump 2.0.1→2.0.2
  - **v2.0.1** → daemon-only multi-plateformes (1ère release CI verte) ; version bump 2.0.0→2.0.1
  - **v2.0.0** → commit ff02d1f80 — hard fork NETWORK_ID (HRG\x02HIDERINGMAIN, magic 0x48524702) + version 2.0.0 ; abandon volontaire chaîne v1.x à h≈3577 (16 mai 2026)
  - **v1.0.2** → commit 9d0e593a6 — fix RandomX/huge-pages bad_alloc au boot + version bump 1.0.2
  - **v1.0.1** → commit 47795728f — MONEY_SUPPLY corrigé (18M cap + 42.86 reward) + rebrand strings + version bump
  - **v1.0.0** → commit eb8ad1ee9 — rebrand banner initial + workflow CI (porte encore l'overflow MONEY_SUPPLY)
- Releases :
  - **https://github.com/AB-lab113/hidering/releases/tag/v2.0.2 — dernière publique** (Linux x64 13.1 MB + macOS ARM64 10.2 MB + Windows x64 29.3 MB, chaque asset + sidecar `.sha256`)
  - https://github.com/AB-lab113/hidering/releases/tag/v2.0.1 (daemon-only, multi-plateformes) — historique
  - https://github.com/AB-lab113/hidering/releases/tag/v2.0.0 (Linux x64) — hard fork, historique
  - https://github.com/AB-lab113/hidering/releases/tag/v1.0.2 (Linux x64, stripped, 14.35 MB) — **NE PLUS UTILISER pour mainnet** post-HF
  - https://github.com/AB-lab113/hidering/releases/tag/v1.0.1 (Linux x64, stripped, 14.3 MB) — historique
  - https://github.com/AB-lab113/hidering/releases/tag/v1.0.0 (Linux x64) — historique
- Repo : https://github.com/AB-lab113/hidering
- Build actif : build/release/bin/

## SPECS ÉCONOMIQUES
- Supply max : **18,000,000 HRG** (hard cap irrévocable — décision 11 mai 2026, voir BUG CRITIQUE MONEY_SUPPLY)
- Supply circulant max : 17,999,957.14 HRG (genesis NUMS 42.86 HRG déduit)
- Émission : **100% PoW** (algo RandomX, CPU-only) — pas de seed round, pas de vente privée, pas de token allocation équipe
- Phase pré-publique v2.0.0 : depuis le hard fork du 16 mai 2026 (post-attaque 51% sur v1.x à h≈3577), les nœuds fondateurs minent en réseau privé avant ré-ouverture publique. Volume + durée non encore fixés ; communication publique au launch. Voir whitepaper §7.1 et FAQ site web pour le narratif officiel. **NB :** la promesse "Premine : 0 HRG" du v1.x N'EST PLUS valable depuis v2.0.0 — toute doc résiduelle à harmoniser.
- Block time : 120 secondes
- Halving interval : 210,000 blocs (~2.66 ans)
- Récompense initiale : **42.86 HRG/bloc** (= 9,000,000 / 210,000 — série géométrique somme à 18M)
- Algorithme PoW : RandomX (CPU-only, ASIC-resistant)

### Historique des specs (changement 11 mai 2026)
Le whitepaper original annonçait : supply max 33,000,000 HRG, récompense initiale 157.14 HRG/bloc.
La valeur 33M × 10¹² atomes overflow uint64 (max ~18.4M HRG avec 12 décimales), bug détecté lors de
la CI v1.0.0 sur macOS clang. Décision tokenomics (11 mai 2026) : baisser le cap à 18M HRG pour
rester représentable en uint64 sans toucher aux décimales. La récompense initiale est ajustée à
42.86 HRG/bloc pour que la somme géométrique des halvings (210K interval × ½) converge exactement
vers 18M. Les binaires v1.0.0 publiés portent encore les anciennes valeurs (157.14 / cap 14.55M
effectif après wrap). **Patch source appliqué en v1.0.1 (12 mai 2026, commit dca7dd433).**

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
- src/checkpoints/checkpoints.cpp — ancres mainnet h=2939/5000/11000/16000/20000, hash-enforced depuis `934b43b01` (6 juin 2026)
- src/cryptonote_basic/cryptonote_basic_impl.cpp — impl. de base, **get_block_reward()** (halving)
- src/simplewallet/simplewallet.{h,cpp} — wallet CLI
- src/p2p/net_node.inl — réseau P2P
- src/common/dns_utils.cpp — seeds DNS (Monero purgés)
- src/debug_utilities/dns_checks.cpp — neutralisé
- GENESIS_PROOF.md — preuve NUMS genesis
- assets/hrg_logo.svg — logo

## SÉCURITÉ WALLET — RISQUES ACCEPTÉS/DOCUMENTÉS
- **M-3 : fichiers `.keys` sans MAC/HMAC — risque ACCEPTÉ, fix refusé car format-breaking (décision 12 juin 2026).** État : `load_keys_buf` (`wallet2.cpp:5135`) fait `parse_binary(keys_file_data{iv, account_data})` → ChaCha20 → parse JSON, sans authentification du blob. **Identique à Monero upstream release-v0.18** (vérifié ligne à ligne, 0 hmac dans wallet2.cpp upstream) → rien à backporter, tout l'écosystème Monero vit avec ce design. Mitigations existantes : (1) le matériel de clé est auto-authentifié — `hwdev.verify_keys` re-dérive les clés publiques depuis les secrètes déchiffrées et compare (`wallet2.cpp:5536-5541`) ; (2) une altération aléatoire casse le parse JSON → échec bruyant ; (3) forger un ciphertext ciblé exige le mot de passe (KDF). **Risque résiduel réel mais borné** : ChaCha20 = stream cipher malléable → un attaquant avec accès disque en écriture et connaissance du layout JSON peut bit-flipper des valeurs de *settings* (ex. digits de `refresh_from_block_height`, flag `ask_password`) sans casser le parse — dégradation/DoS, PAS vol de clés. Ce même attaquant peut de toute façon remplacer le binaire wallet (game over) → un MAC n'élève la barre que marginalement. **Pourquoi pas de fix** : `keys_file_data` est sérialisé epee positionnel SANS champ de version → ajouter un champ MAC rendrait les nouveaux `.keys` illisibles par TOUS les binaires publiés (v2.0.2 CLI/RPC/GUI 3 OS dans la nature) et imposerait un dual-parse fragile pour les anciens — violation directe de la compat hrg-wallet-v2/hrg-wallet-pool (le hot wallet pool sur Contabo-2 serait touché au 1er rewrite). À réévaluer seulement si un format-break wallet coordonné est un jour planifié (le blob PQ `pq_keys` vit DANS `account_data` chiffré → couvert par la même analyse).

## CVE / FAUX POSITIFS DÉPENDANCES
- **CVE-2024-38517 (rapidjson, CVSS 7.8) — FAUX POSITIF, déjà corrigée dans notre arbre (vérifié 12 juin 2026).** Underflow d'entier dans `GenericReader::ParseNumber()` (`reader.h`, exposants négatifs : `exp + expFrac` peut passer sous `INT_MIN`). Le fix upstream est le commit Tencent **`8269bc2bc289e9d343bae51cdf6d23ef0950e001`** (« Prevent int underflow when parsing exponents », **mai 2018**) ; notre submodule `external/rapidjson` est pinné à `129d19ba7` (juillet 2018) qui en **descend** (`git merge-base --is-ancestor` OK ; le clamp `maxExp = (expFrac + 2147483639) / 10` est présent dans `include/rapidjson/reader.h:~1640`). La CVE n'a été assignée qu'en 2024 car rapidjson n'a jamais re-releasé après v1.1.0 (2016) : elle vise les distros packageant le tarball v1.1.0, PAS les pins de master post-mai-2018. **Les scanners (Snyk/Wiz/etc.) keyent sur la version « 1.1.0 » et flaggeront ce submodule à chaque audit → ne pas re-patcher.** Unique copie dans l'arbre (le repo GUI pointe sur le même backend via gitlink/symlink). NB : les références circulant pour cette CVE (issue `#2460`, commit `e3081d55`) sont fausses — 404 chez Tencent/rapidjson.

## BUG CRITIQUE — RESOLU (commit 0b6d81b3e, 30 avril 2026)
### get_block_reward() — halving mal calculé
Localisation : src/cryptonote_basic/cryptonote_basic_impl.cpp:83
(et non blockchain.cpp comme initialement noté)

Bug d'origine : le calcul estimait la hauteur via
  current_height = already_generated_coins / INITIAL_REWARD_LOCAL
Correct uniquement avant le 1er halving — après halving 1 la récompense
change mais le diviseur reste constant, d'où halvings count inflé.

Fix appliqué : simulation itérative exacte qui parcourt chaque période
d'émission en accumulant les coins exacts, indépendante de la profondeur
de halving. Pas de changement de signature de get_block_reward()
(toujours basée sur already_generated_coins).

NB : approche différente du fix décrit dans une version antérieure de ce
document (qui prescrivait un passage par current_height). La simulation
itérative est mathématiquement équivalente et évite un refactor des ≥6
call sites de get_block_reward (blockchain.cpp, tx_pool.cpp,
cryptonote_tx_utils.cpp).

## BUG CRITIQUE — RESOLU (v1.0.1, commit dca7dd433, 12 mai 2026)
### MONEY_SUPPLY overflow uint64_t — option (b) appliquée : cap = 18M HRG
Localisation : src/cryptonote_config.h:54 et src/cryptonote_basic/cryptonote_basic_impl.cpp:85

Le littéral `33000000000000000000ULL` (= 33 × 10¹⁹ = 33M HRG × 10¹² atomes)
**dépassait UINT64_MAX** (1.84 × 10¹⁹). Comportement des binaires v1.0.0 (historique) :
- Linux gcc : wrap silencieux → MONEY_SUPPLY effectif = 14,553,255,926,290,448,384
  atomes ≈ **14.55M HRG** (au lieu des 33M annoncés à l'époque)
- macOS clang : erreur fatale de compilation, **macOS ne buildait pas**
- Windows MSYS2/mingw : non testé après le fix workflow (présumé : wrap comme Linux)

Cause racine architecturale : avec 12 décimales atomiques (héritées Monero),
uint64_t ne peut représenter QUE jusqu'à ~18.4M HRG. Trois options évaluées :
(a) réduire les décimales à 11, (b) baisser le cap à ≤18.4M HRG, (c) élargir
le type de already_generated_coins.

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
- Phase 3C : Infrastructure seed nodes — **COMPLETE (v2.0.0 sur VPS dédiés, 19 mai 2026)** : `seed1.hidering.org` → OVH `135.125.243.137` (systemd `hideringd.service`, restart auto) ; `seed2.hidering.org` → Contabo `207.180.211.96`. Flux `hideringseed1` en cours d'expiration (containers éphémères + IPs variables = mauvais fit, écarté définitivement).
- Phase 4A-C : Mainnet public launch — **RESET v2.0.0** (chaîne v1.x à h≈3577 abandonnée par décision 16 mai 2026 ; nouveau mainnet v2.0.0 démarre vierge dès activation. Launch *officiel public* toujours cible T2 2026.)
- Phase 4D : Binaires publics — **COMPLETE (3 juin 2026)**. Daemon : v2.0.2 multi-plateformes CI-publiés (Linux x64 + macOS ARM64 + Windows x64, cf. RELEASE v2.0.2) — CI GitHub Actions débloquée. GUI : **v2.0.2-gui multi-plateformes publié 3 juin 2026** (macOS ARM64 + Windows x64 buildés par CI via `.github/workflows/build-{macos,windows}-gui.yml`, + Linux AppImage report de v2.0.1-gui ; cf. RELEASE v2.0.2-gui + section HIDERING WALLET GUI). Items résiduels non bloquants : vrais installers .dmg/.exe (actuellement .app tar.gz + .zip portable), icns, module updater, QA i18n.
- Phase 4E : Pool mining — **EN PRODUCTION** (25 mai 2026) : pool publique `pool.hidering.org:3333` live sur Contabo-2, stack **cryptonote-nodejs-pool** (et non monero-pool finalement retenu) ; minage validé end-to-end (16 shares, wallet-rpc `ok`). Voir sous-section « Déploiement cryptonote-nodejs-pool — Contabo-2 » dans POOL MINING. NB historique : prototype validé en local 14 mai 2026 avec monero-pool jtgrassie.
- Phase 5 : Post-quantique (Dilithium3 + Kyber768) — **EN COURS** (étape 1 ✅ liboqs intégré le 30 mai 2026 ; étape 2 ✅ types de clés PQ + ops Dilithium3/Kyber768 + HFv16 placeholder le 31 mai 2026 ; étape 3 ✅ intégration Dilithium3 dans le flow de signature des TX le 3 juin 2026, commit `3c92a61d9`, gardé par HFv16 ; étape 4 ✅ dérivation stealth addresses BQ... via Kyber768 le 4 juin 2026, commit `0c489dbdc`, gardé par HFv16 + flag `is_pq` ; étape 5 ✅ format d'adresse BQ... + plumbing clés Kyber768 wallet le 4 juin 2026, commit `b77639979`, gardé par HFv16 + `is_pq`, format B... byte-identique ; étape 6 ✅ keygen BQ + préfixe mnemonic "BQ" + détection des sorties BQ le 4 juin 2026, commit `62cb02066` : préfixe `0x3C11`→**62** + octet marqueur `0x33` dans le payload (un préfixe seul ne PEUT PAS rendre "BQ" — prouvé) → l'adresse commence vraiment par "BQ" ; `generate_pq_keys`/`get_pq_address_str` ; `wallet2::generate(use_pq=false)` ; détection par dé-tweak `P−t·G` avant `is_out_to_acc_precomp` ; items ouverts : `pq_keys` toujours non sérialisé (clés BQ éphémères par session), subaddress BQ, `hf_version` cold-signing, bump `MAX_TX_EXTRA_SIZE` à l'activation HFv16 ; voir POST-QUANTIQUE Étape 6 ; étape 7 ✅ persistance des clés BQ (sérialisation chiffrée de `pq_keys` dans le fichier wallet) le 4 juin 2026, commit `4419f7c5b` : blob `pq_stealth_keys` (3584 o) dans le KV map de `account_keys`, écrit uniquement si set → wallets B... byte-identiques ; `kyber_sk` chiffré ChaCha20 comme les secrets Ed25519 ; `m_account_address.pq_kyber_pk` réhydraté au load ; `account_public_address` non touché (B... wire/base58 identiques) ; `pq_persistence_test.cpp` PASS ; voir POST-QUANTIQUE Étape 7 ; **étape 8 ✅ audit de sécurité le 5 juin 2026** (18 findings corrigés : 1 CRIT + 5 ÉLEVÉ + 6 MOYEN + 5 FAIBLE, 9 commits `fccfd6b07`→`f6d2d62a9` ; clé Dilithium jetable→persistante C-1, parsing TLV canonique E-3, DoS scan E-1/E-2 corrigé, tweak domaine+index E-4 ; `HF_HEIGHT_PQ` repoussé 1M→2M ; **résiduel C-1 : binding validateur complet impossible sur chaîne privacy → spec à finaliser AVANT activation HFv16** ; chaîne live confirmée intacte) ; voir POST-QUANTIQUE Étape 8 ; cible hard fork mainnet T2 2027)

## PROCHAINES ETAPES (PAR ORDRE)
1. Commit v2.0.0 source patches (`src/cryptonote_config.h` + `src/version.cpp.in`)
2. Rebuild full : `make -j$(nproc)` (daemon target déjà rebuilt 16 mai ; wallet binaries encore en v1.0.2)
3. Wipe ~/.hidering + smoke test genesis NUMS sur le nouveau réseau
4. Daemon --offline valider genesis hash (inchangé — NETWORK_ID hors hash bloc)
5. Tag `v2.0.0` + release notes
6. ~~Rebuild Docker image `ab113hrg/hidering-seed:v2.0.0`~~ — **FAIT** 17 mai 2026 (utile uniquement pour Flux, désormais déprécié).
7. ~~Redéployer Flux seed node `hideringseed1`~~ — **OBSOLÈTE** (Flux abandonné 19 mai 2026, migration sur VPS dédiés).
8. ~~DNS seed nodes — vérifier que seed1/seed2.hidering.org pointent vers les nouveaux IPs~~ — **FAIT** 19 mai 2026 : `seed1` → OVH `135.125.243.137`, `seed2` → Contabo `207.180.211.96`.
9. ~~Block explorer — reset DB ou nouvelle instance~~ — **FAIT** 19 mai 2026 : nouvelle instance sur OVH (https://explorer.hidering.org), remplace l'ancienne instance Flux `hrgexplorer.app.runonflux.io`.
10. Binaires publics v2.0.0 (Linux/Windows/Mac) — débloquer billing GH Actions sinon Linux-only via build local + `gh release create v2.0.0`
11. Pool mining — relink monero-pool contre les libs v2.0.0, pointer un daemon v2.0.0 sync mainnet
12. Site web public — mettre à jour docs/ (network ID, magic, version) pour redeploy Vercel auto
13. Announce hard fork (Twitter, Reddit, BitcoinTalk) — communiquer activation + abandon chaîne v1.x
14. Phase 4 Launch (public officiel)

## RELEASE v2.0.2 (1 Juin 2026 — dernière publique, multi-plateformes CI)
- Tag : **v2.0.2** → commit `c37d5e285` ([HRG] chore(release): bump DEF_MONERO_VERSION 2.0.1 → 2.0.2)
- `src/version.cpp.in:2` : `DEF_MONERO_VERSION "2.0.2"`, release name `Privacy Enhanced` inchangé.
- **Type : 1ère release publique multi-plateformes à bundles complets** (daemon + wallet). v2.0.1 (publiée plus tôt le 1er juin) était daemon-only ; v2.0.2 ajoute `hidering-wallet-cli` + `hidering-wallet-rpc` dans chaque archive.
- Assets publiés (CI-auto-publiés, billing GH Actions débloqué — cf. mémoire `project_ci_build_workflows.md`) :
  - `hidering-linux-x64.tar.gz` — 13.1 MB — sha256 `844b0cac1cb3192c9d616dffa50da538399447c05e8086a44447adccf5bd3f30`
  - `hidering-macos-arm64.tar.gz` — 10.2 MB — sha256 `5215a59ec18d444f7565d3351276049b39565e253b98875f733068d2e7e48a22`
  - `hidering-windows-x64.zip` — 29.3 MB — sha256 `0f687dc8bd84a0cdd3a7b0f631c79201ba0a8f0a0c869a15f6a1e8950b884cf3`
  - Chaque asset accompagné de son sidecar `.sha256`. macOS = **ARM64 uniquement** (runner macos-13 Intel jamais dispo sur ce compte). Noms d'archives **sans préfixe de version** (ex-`hidering-v2.0.0-linux-x64.tar.gz` → `hidering-linux-x64.tar.gz`).
- CI : workflows `build-{linux,macos,windows}.yml` (ajoutés 1er juin, commit `5de0bc27e`) + job release-publish par workflow avec concurrency group partagé. Recette build : link dynamique (PAS STATIC — casse libunbound), unbound+zeromq requis, liboqs buildé en premier (pqc.cpp inclut oqs.h inconditionnellement), Windows `CMAKE_*_STANDARD_LIBRARIES="-lws2_32 -lcrypt32 -lbcrypt -lgdi32 -liphlpapi"`.
- Site : `docs/index.html` MAJ vers v2.0.2 (section Downloads multi-plateformes + liens directs vers les assets + Mining step 01 dossier `hidering-linux-x64/`) — déployé sur hidering.org via Vercel.
- Mémoire associée : `~/.claude/projects/-home-shark-hidering/memory/project_ci_build_workflows.md`

## RELEASE v2.0.0 (16 Mai 2026 — source patché, non tagué/publié)
- Tag : **non tagué** — source HEAD `41f899049` + 2 fichiers uncommitted
- Type : **hard fork coordonné** (flag-day) — chaîne v1.x à h≈3577 abandonnée par décision explicite utilisateur
- Patches source appliqués :
  - `src/cryptonote_config.h:255` — mainnet NETWORK_ID byte[3] `0x01` → `0x02` → UUID `HRG\x02HIDERINGMAIN`, magic `0x48524702`
  - `src/version.cpp.in:2` — `DEF_MONERO_VERSION "1.0.2"` → `"2.0.0"`
- Genesis intentionnellement **inchangé** : `GENESIS_NONCE = 10000`, `GENESIS_TX` blob et `src/gen_genesis/gen_genesis.cpp:18` (`INITIAL_REWARD = 157140000000000ULL`) preservés. NETWORK_ID n'entre pas dans le hash bloc → genesis valide sous v2.0.0, c'est uniquement le handshake P2P qui sépare les réseaux.
- Build local : 16 mai 2026 — daemon target rebuilt clean, binaire reporte `Hidering 'Privacy Enhanced' (v2.0.0-41f899049)` (15.02 MB, non strippé, dans `build/release/bin/hideringd`)
- Wallet binaries (`hidering-wallet-cli`, `hidering-wallet-rpc`) : libs sous-jacentes recompilées au link daemon, mais les exécutables wallets ne sont **pas** régénérés tant qu'un `make -j$(nproc)` complet n'est pas lancé.
- Compat : **wire-break** total — peers v1.x rejetés au handshake (NETWORK_ID mismatch), LMDB v1.x orphelin pour un daemon v2.0.0.
- Mémoire associée : `~/.claude/projects/-home-shark-hidering/memory/project_v2.0.0_networkid_hardfork.md`

## RELEASE v1.0.2 (13 Mai 2026 — dernière publique avant HF v2.0.0)
- Tag : v1.0.2 → commit 9d0e593a6
- Commits inclus depuis v1.0.1 :
  - `6b9cf60de` — fix(daemon): bad_alloc on auxiliary thread at startup (RandomX huge-pages probe)
  - `9928215e7` — docs(claude-md): mark v1.0.2 bad_alloc resolved
  - `9d0e593a6` — chore(release): bump DEF_MONERO_VERSION 1.0.1 → 1.0.2 + release notes
- Asset publié : `hidering-v1.0.2-linux-x64.tar.gz` (14.35 MB, strippé, gcc Ubuntu 24.04)
- SHA256 : `da92781f5e0d085a08a656b48ea49be3d54201b3db32b736204266ea89fd2b8b`
- Contenu tarball : hideringd, hidering-wallet-cli, hidering-wallet-rpc + README.md + LICENSE + GENESIS_PROOF.md
- Build CI : même situation que v1.0.1 — workflow déclenché par le tag mais billing GH Actions toujours bloqué. Asset Linux uploadé manuellement via `gh release create`. macOS/Windows à shipper plus tard sur la release existante (`gh release upload v1.0.2 ... --clobber`).
- Compat : drop-in v1.0.1 → v1.0.2 (pas de change consensus/wire/LMDB). RELEASE_NOTES_v1.0.2.md à la racine du repo.

## RELEASE v1.0.1 (12 Mai 2026)
- Tag : v1.0.1 → commit 47795728f
- Commits inclus depuis v1.0.0 :
  - `dca7dd433` — fix(consensus): cap 18M HRG + reward 42.86 HRG (résout overflow uint64)
  - `b58e13850` — chore(rebrand): 12+ strings "Monero"/"monero-wallet-cli" résiduelles → Hidering
  - `47795728f` — chore(release): bump DEF_MONERO_VERSION 1.0.0 → 1.0.1
- Asset publié : `hidering-v1.0.1-linux-x64.tar.gz` (14.3 MB, strippé, gcc Ubuntu 24.04)
- SHA256 : `33e58c23e534fa7614e4731fca858a1a289b63fb9f069ef3096203cd39dd82b0`
- Contenu tarball : hideringd, hidering-wallet-cli, hidering-wallet-rpc + README.md + LICENSE + GENESIS_PROOF.md
- Build CI : workflow `.github/workflows/build-release.yml` déclenché par le tag mais **stoppé immédiatement par le billing GitHub Actions** (run `25739049536`). Asset Linux uploadé manuellement via `gh release create`. Pour macOS/Windows : régler le billing puis `gh run rerun 25739049536`, ou supprimer/recréer le tag.

## RELEASE v1.0.0 (11 Mai 2026 — historique)
- Tag : v1.0.0 → commit eb8ad1ee9 (rebrand "Monero '" → "Hidering '" sur 19 sites + bump version 0.18.1.0 → 1.0.0)
- Asset publié : `hidering-v1.0.0-linux-x64.tar.gz` (14 MB, stripped, statically built sur Ubuntu 24.04)
- SHA256 : `d4a75a0d3c626e7dc3e726225b2cc67e8eb7396508c5cf6e54c459dedf333b8b`
- Contenu tarball : hideringd, hidering-wallet-cli, hidering-wallet-rpc + README + LICENSE + GENESIS_PROOF.md
- Cap d'émission effectif : 14.55M HRG (overflow uint64 non corrigé — cf. BUG CRITIQUE MONEY_SUPPLY). Conserver pour traçabilité, **ne pas réutiliser pour mainnet**.

### Punch list v2.0.0 (post-HF 16 Mai 2026 — EN COURS)
1. **Commit source patches** — `git add src/cryptonote_config.h src/version.cpp.in && git commit -m "[HRG] v2.0.0 hard fork: bump NETWORK_ID HRG\\x01→HRG\\x02 + version 1.0.2→2.0.0"`. Travailler sur la branche v2-privacy comme d'habitude.
2. **Rebuild complet** — `cd build/release && make -j$(nproc)` pour régénérer `hidering-wallet-cli` + `hidering-wallet-rpc` (le 16 mai seul `daemon` a été rebuilt).
3. **Tag v2.0.0 + release notes** — `git tag -a v2.0.0 -m "..."` puis `gh release create v2.0.0` avec asset Linux + `RELEASE_NOTES_v2.0.0.md` à la racine.
4. **Binaires macOS + Windows v2.0.0** — débloquer billing GH Actions (item récurrent depuis v1.0.1), sinon Linux-only via `gh release upload v2.0.0 ...`. Tant que le billing reste bloqué, chaque release reste Linux-only.
5. ~~**Docker `ab113hrg/hidering-seed:v2.0.0`**~~ → **FAIT** 17 mai 2026 (push DockerHub, digest manifest `sha256:bb9bfc6ffcb5a0…`). NB : image utile uniquement pour Flux, désormais déprécié — VPS dédiés tournent un binaire natif via systemd, pas via Docker.
6. ~~**Flux `hideringseed1` redeploy avec volume wipe**~~ → **OBSOLÈTE** 19 mai 2026 : migration complète sur VPS dédiés (`seed1.hidering.org` → OVH `135.125.243.137`, `seed2.hidering.org` → Contabo `207.180.211.96`). Flux `hideringseed1` en cours d'expiration. Voir nouvelle section INFRASTRUCTURE VPS ci-dessous.
7. ~~**Block explorer reset**~~ → **FAIT** 19 mai 2026 : nouvelle instance déployée sur OVH (https://explorer.hidering.org), indexant la chaîne v2.0.0 depuis genesis. Ancienne instance Flux `hrgexplorer.app.runonflux.io` retirée.
8. ~~**Docs CLAUDE.md + whitepaper**~~ → harmonisés 16 mai 2026 dans cette même session (CLAUDE.md SPECS RÉSEAU + Tags + Releases + RELEASE v2.0.0 section ; `docs/whitepaper_v1.3.md` lignes 257-258 + roadmap Phase 4).
9. **GENESIS_PROOF.md** — à vérifier : pas d'impact direct (genesis inchangé) mais ajouter mention que v2.0.0 hard fork n'altère pas le genesis ; le NUMS reste sous l'ancien hash bloc même si plus jamais activé en mainnet.
10. **Announce hard fork** — Twitter, Reddit, BitcoinTalk + email aux miners connus. Préciser activation flag-day et abandon chaîne v1.x.

### Punch list v1.0.3 — partiellement OBSOLÈTE post-HF v2.0.0
1. ~~**Binaires macOS + Windows v1.0.2**~~ — **OBSOLÈTE** : binaires v1.0.2 incompatibles avec mainnet v2.0.0 post-HF. Item ré-instancié en punch list v2.0.0 item #4.
2. ~~**Whitepaper + GENESIS_PROOF.md (v1.0.x)**~~ → harmonisés 13 mai 2026. Commit `de1db6c5e` publie `docs/whitepaper_v1.3.md` (cap 18M, reward 42.86 HRG, roadmap v1.0.0→v1.0.2 à jour, code snippet `MONEY_SUPPLY = 18000000000000000000ULL` uint64-safe). Commit `f7ba0b1dd` patche `GENESIS_PROOF.md` (cap 33M → 18M dans TL;DR et section unspendability, circulant effectif 32 999 842.86 → 17 999 842.86 HRG ; construction NUMS, domain strings et amount genesis 157.14 HRG inchangés pour préserver le hash de la chaîne déployée). Item récurrent depuis v1.0.1 clos.

### Punch list v1.0.2 — RESOLUE partiellement (13 mai 2026)
1. ~~**std::bad_alloc au démarrage du daemon**~~ → fix dans commit `6b9cf60de`. Cause racine : thread `rx_set_main_seedhash_thread` (`src/crypto/rx-slow-hash.c:350`) appelle `randomx_alloc_cache(... | RANDOMX_FLAG_LARGE_PAGES)`, et `LargePageAllocator::allocMemory` (`external/randomx/src/allocator.cpp:55`) throw `std::bad_alloc` quand `mmap(MAP_HUGETLB)` échoue sur un hôte sans huge pages (vm.nr_hugepages = 0, le défaut partout). Le throw est rattrapé en interne mais l'interposer `__cxa_throw` (`src/common/stack_trace.cpp:91`) logge tout throw avant le catch. Fix : helper `rx_large_pages_available()` qui sonde `/proc/sys/vm/nr_hugepages` une fois et désactive `RANDOMX_FLAG_LARGE_PAGES` sur les 4 call sites (rx_alloc_dataset, rx_alloc_cache, rx_init_full_vm, rx_init_light_vm). Hypothèse initiale (thread DNS) **incorrecte**.
2. (déplacé en v1.0.3 punch list item #1 — macOS/Windows binaires, blocage billing GH Actions toujours actif)
3. (déplacé en v1.0.3 punch list item #2 — révision doc whitepaper/GENESIS_PROOF, ✅ résolu en v1.0.3 le 13 mai 2026)

### Punch list v1.0.1 — RESOLUE (12 mai 2026)
1. ~~MONEY_SUPPLY~~ → fix dans commit `dca7dd433`.
2. (déplacé en v1.0.2 punch list item #1 — `bad_alloc` non traité, ✅ résolu en v1.0.2)
3. ~~Rebrand strings résiduelles~~ → fix dans commit `b58e13850`.
4. ~~CI workflow target name~~ → déjà fixé dans `739523ee6`, présent sur v2-privacy.
5. (déplacé en v1.0.2 punch list item #3 — révision doc whitepaper, ✅ résolu en v1.0.3 le 13 mai 2026)

Mémoires persistantes associées :
- `~/.claude/projects/-home-shark-hidering/memory/project_money_supply_overflow_v1.0.0.md`
- `~/.claude/projects/-home-shark-hidering/memory/project_v1.0.1_punch_list.md`

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

## INFRASTRUCTURE VPS (19 Mai 2026 — REMPLACE Flux ; 4 hôtes au 21 mai)
- **Décision 19 mai 2026 :** abandon de Flux pour les seed nodes et le block explorer. Containers éphémères + IPs variables (vu 3 IPs en une session sur `hideringseed1`) = mauvais fit pour des entry points DNS stables. Migration vers VPS dédiés avec IPs invariantes et systemd pour restart auto.

### Inventaire 4 hôtes (vérifié 21 mai 2026 ; OS + SSH + recette deploy MAJ 12 juin 2026)
| Rôle | Hôte | IP | OS | User SSH | Binaire | Service | RPC publique ? |
|---|---|---|---|---|---|---|---|
| seed1 public | `seed1.hidering.org` | `135.125.243.137` (OVH) | Ubuntu 22.04 | `ubuntu` | `/home/ubuntu/hidering/build/bin/hideringd` | `hideringd.service` | ✅ (19741 → 0.0.0.0) |
| seed2 public | `seed2.hidering.org` | `207.180.211.96` (Contabo-1) | Ubuntu 24.04 | `root` | `/root/hidering/build/bin/hideringd` | `hideringd.service` | ✅ (19741 → 0.0.0.0) |
| relay privé | (pas de DNS) | `207.180.214.164` (Contabo-2) | Ubuntu 24.04 | `root` | `/root/hidering/build/bin/hideringd` | `hideringd.service` | 🔒 localhost only |
| relay privé | (pas de DNS) | `167.86.74.202` (Contabo-3) | Ubuntu 24.04 | `root` | `/root/hidering/build/bin/hideringd` | `hideringd.service` | 🔒 localhost only |

**Auth SSH (MAJ 12 juin 2026) :** key-only (ed25519) sur les 4 hôtes — plus de password. **Recette de déploiement canonique : git pull + make SUR le VPS, JAMAIS de scp de binaires depuis WSL2.** Raison : OVH est en Ubuntu 22.04 → un binaire buildé sur WSL2 (24.04, glibc 2.39) ne tourne PAS dessus (GLIBC incompatible) ; sur les Contabo (24.04) le build local au VPS reste plus sûr (mêmes libs dynamiques). Séquence : `ssh ubuntu@135.125.243.137` (ou `root@<ip-contabo>`) → `cd ~/hidering && git pull` → `cd build` (**attention : le build dir VPS est `~/hidering/build`, PAS `build/release` — brûlé le 12 juin sur OVH**) → `make -j$(nproc) daemon` (wallet : `make -j2` obligatoire partout, OOM `wallet2.cpp`) → `[sudo] systemctl restart hideringd.service` (`sudo` sur OVH uniquement). *Ancienne recette scp + password (21 mai 2026) OBSOLÈTE.*

- **Block explorer public : https://explorer.hidering.org** — instance fraîche indexant la chaîne v2.0.0 depuis genesis. Remplace l'ancienne instance Flux `hrgexplorer.app.runonflux.io` (retirée). Le port 8080 derrière nginx sert toujours le backend Flask en clair (HTTP 200) — pas de redirect, à fermer/binder localhost si on veut couper le double accès.
- **Unit file systemd canonique (vérifié 20 mai 2026)** : `/etc/systemd/system/hideringd.service`, `Type=simple` (pas de `--detach`), `Restart=always RestartSec=10`. Flags daemon harmonisés sur les nœuds publics (OVH + Contabo-1) : `--non-interactive --log-level 0 --p2p-bind-ip 0.0.0.0 --p2p-bind-port 19740 --rpc-bind-ip 0.0.0.0 --rpc-bind-port 19741 --confirm-external-bind --restricted-rpc`. **Contabo-2 et Contabo-3 : RPC localhost CONFIRMÉ (M-13 mitigé, vérifié 12 juin 2026).** Contabo-2 (daemon pool) tourne **sans** `--restricted-rpc` (intentionnel — `getblocktemplate` requis par le pool) mais avec `--rpc-bind-ip 127.0.0.1` explicite dans l'unit → `ss -tlnp` montre 19741/19742 (ZMQ)/19743 (wallet-rpc)/6379 (redis) tous sur 127.0.0.1, et un probe externe `curl 207.180.214.164:19741` échoue (connection refused). Seuls ports publics : 3333 (stratum), 19740 (P2P), 22, 80 (nginx), 8117 (API pool). Pas de règle iptables (policy ACCEPT) ni ufw — la protection EST le bind localhost ; si un jour le bind change, ajouter un firewall AVANT. Contabo-3 : 19741 sur 127.0.0.1 + `--restricted-rpc` (double protection). NB : l'unit Contabo-2 porte aussi `--start-mining B5TLmi…` (6 threads) + 4 priority nodes. `--restricted-rpc` ajouté à seed1 OVH le 20 mai 2026 (avant : RPC complète exposée publiquement, leak version/peers/free_space) ; après restart le daemon a repris sa sync immédiatement, peers reconnectés en <30s. Backup pré-modif conservé en `/etc/systemd/system/hideringd.service.bak.20260520-*` sur OVH.
- **DNS (records A) :** à jour côté registrar (vérifié 19 mai 2026) :
  - `seed1.hidering.org` → `135.125.243.137` (OVH)
  - `seed2.hidering.org` → `207.180.211.96` (Contabo)
  - `explorer.hidering.org` → `135.125.243.137` (OVH — même VPS que seed1, sert l'UI block explorer sur le port 8080)
  Les binaires v2.0.0 publiés et `docs/index.html` pointent directement vers ces noms DNS, plus jamais vers les IPs nues.
- **Source de vérité pour les IPs :** les IPs sont fixes, **on peut les hard-coder**. (À l'inverse, Flux exigeait `curl https://api.runonflux.io/apps/location/hideringseed1` à chaque fois — plus jamais nécessaire post-migration.)

## DOCKER ET DEPLOIEMENT (Flux — déprécié 19 Mai 2026)
- **Statut 19 mai 2026 :** Flux `hideringseed1` en cours d'expiration. Infrastructure migrée sur VPS (voir section INFRASTRUCTURE VPS ci-dessus). Les sections ci-dessous restent comme référence historique du déploiement Flux jusqu'au 19 mai. Plus de redeploy Flux prévu.
- **Statut post-HF v2.0.0 (17 mai 2026, soirée) :** image `:v2.0.0` **pushed** sur DockerHub + **Flux redéployé** avec succès — l'app `hideringseed1` sert maintenant le binaire v2.0.0, vérifié par probe P2P (handshake `COMMAND_HANDSHAKE INVOKED OK` depuis un daemon v2.0.0 local). Volume wipe confirmé via height 1 + top_block_hash genesis. Punch list v2.0.0 items #5 (Docker) et #6-7 (Flux redeploy) closés.
- Image courante (live Flux) : `ab113hrg/hidering-seed:v2.0.0` — push DockerHub 17 mai 2026, build local depuis `build/release/bin/hideringd` (commit `ff02d1f80` = tag v2.0.0)
- Note historique : avant le 17 mai 2026 soir, Flux servait `:v1.0.2` (push DockerHub 13 mai 2026) — chaîne v1.x abandonnée post-attaque 51%. Plus reachable depuis le delete.
- Digest v2.0.0 pushé : `sha256:bb9bfc6ffcb5a0456e1ed3453e17b1071ecb09284c36c0cc7c5ada33bb6ce9d6` (manifest list, multi-arch) — amd64-specific manifest : `sha256:74cf7031aa589b5cf3d9d2a94ef788b46cde02fddb0f44dae3756829ba80ff39`. `:latest` aliasé sur le même digest.
- Digest v1.0.2 pushé : `sha256:2ec8eb28cfe7c80d8c24e00032a227995583f3659026c0bc01b6464e586e32b0`
- Digest historique v1.0.1 (build b43a29862) : `sha256:0a92296ce842edc690c72f2eecae32f737628381f4e9de58c556984119557c5e`
- Flux app : `hideringseed1`
  - Ports : P2P 19740, RPC 19741
  - IP courante : **ne pas hard-coder** — Flux re-schedule sur d'autres nodes à chaque delete/recreate (vu plusieurs IPs en une session : `.197` pré-redeploy, `.112` post-redeploy 17 mai soir). Source de vérité : `curl https://api.runonflux.io/apps/location/hideringseed1`
  - Spec live (17 mai 2026 soir) : hash `e81f32fdde3f6ae2a83ba7aded83c6e25e83f7b9e74b383226163496bb9875a9`, registration height `2605818`, `repotag` pinned `ab113hrg/hidering-seed:v2.0.0`
  - Spec déployée (vérif) : `curl https://api.runonflux.io/apps/appspecifications/hideringseed1`
- Dockerfile : `Dockerfile.flux` à la racine (binaire copié depuis `build/release/bin/hideringd`)
- Spec Flux v2.0.0 (live, registered) : `flux-hideringseed1.v2.0.0.json` à la racine — `:v2.0.0` pinned, descriptions mises à jour, `hash`/`height` stripped (Flux les remplit au register)
- Spec Flux v1.x (historique) : `flux-hideringseed1.json` à la racine — référence du déploiement v1.0.2 supprimé le 17 mai 2026
- **Gotcha redeploy validé** : Update App via ZelCore peut sembler "signé" mais le tx ne broadcast pas toujours sur le réseau Flux (vécu 2 fois en cette session sur Update App). **Toujours vérifier** côté API Flux registry (`curl …/apps/appspecifications/hideringseed1` → `.data.hash` doit changer) avant de considérer le redeploy fait. Si le hash bouge pas après 10 min, c'est que le tx n'a pas atteint le réseau — fallback canonique : **Delete + Register fresh** (a fonctionné, ~17:22 → 18:56 timeline). Plus de Soft Update tant qu'on n'a pas compris pourquoi le broadcast échoue.
- **Probe P2P pour confirmer le binaire actif** : `hideringd --add-priority-node <IP>:19740 --out-peers 1 --log-level 2` pendant 45s. Si log dit `COMMAND_HANDSHAKE INVOKED OK` → image v2.0.0 active. Si `LEVIN_ERROR_CONNECTION_DESTROYED` → toujours v1.0.2 (NETWORK_ID mismatch). RPC `get_info`/`get_version` **ne disambigue pas** (RPC protocol version identique entre v1.0.2 et v2.0.0, `version` field stripped par `--restricted-rpc`).
- Gotcha genesis : tout changement qui modifie le hash genesis (ex. NUMS migration `073f70af1`) nécessite un **wipe du volume persistant** au redeploy, sinon l'app reste sur l'ancien fork. Pour v2.0.0 : NETWORK_ID a changé mais pas le genesis ; wipe **toujours requis** parce que la LMDB sur disque contient la chaîne v1.x abandonnée — un daemon v2.0.0 la chargerait sans erreur (mêmes règles consensus) mais resterait stuck à h≈3577 sans peers v2.0.0 partageant cette histoire.

## POOL MINING (Phase 4E — VALIDÉ LOCAL 14 Mai 2026)
Stack retenue : **monero-pool** (https://github.com/jtgrassie/monero-pool) — C, single binary ~2.6 MB, link statique aux libs HRG, config plain text. Alternatives écartées : MoneroOcean nodejs-pool (multi-coin/MySQL, 206 default config rows, ~4-8h adapt + cryptonote-util native binding à patcher) ; p2pool (sidechain hardcoded Monero — prefix 18, reward formula, network ID — fork sidechain + bootstrap 1-2 semaines).

### Build recipe (sur la machine de build HRG)
```bash
sudo apt install -y libjson-c-dev uuid-dev liblmdb-dev libevent-dev
cd ~ && git clone https://github.com/jtgrassie/monero-pool
# Patch Makefile ligne 76 : -std=c++14 → -std=c++17
# (HRG epee headers utilisent std::is_standard_layout_v + has_unique_object_representations_v)
export MONERO_ROOT=/home/shark/hidering
export MONERO_BUILD_ROOT=/home/shark/hidering/build/release  # override : Makefile cherche build/Linux/<branch>/release/ par défaut
make release
```
Le binaire `build/release/monero-pool` linke `libcryptonote_basic.a` HRG → **prefix 60 (adresses B...) reconnu automatiquement**, aucun patch source nécessaire.

### Config pool.conf — valeurs HRG critiques
- `rpc-port = 19741` (daemon HRG)
- `wallet-rpc-port = 19743` (optionnel en solo single-address ; requis multi-worker payouts)
- `pool-port = 3333` (stratum) / `webui-port = 4243`
- `pool-wallet = B...` (adresse HRG mainnet, prefix 60)
- `pool-start-diff = 100` (production) / `1` (test local pour trouver des blocs vite)
- `disable-payouts = 0` (par défaut ; `1` pour test sans wallet-rpc)

### Test local 14 mai 2026 — résultat
Daemon offline + monero-pool + xmrig 6.22.0 (2 threads) → 472+ shares acceptées en ~30s, 2 blocs validés par daemon (height 1→3), reward 42.86 HRG/bloc encodée dans coinbase miner_tx (= confirme fix v1.0.1 MONEY_SUPPLY live dans v1.0.2). Wallet `~/hidering/hrg-wallet` montre 472,591.91 HRG préexistants ≈11K blocs mainnet préalables → confirme indirectement le scheme pool→adresse. Reorg refusé pour scanner notre fork offline (3574 blocs derrière chaîne du wallet), comportement attendu.

Workspace test : `~/hidering-pool/` (hors repo, non versionné) — daemon dédié `~/hidering-pool/data/`, pool data `~/hidering-pool/pool-data/`, logs `~/hidering-pool/logs/{hideringd,monero-pool,xmrig}.log`.

### Prod TODO (Phase 4E suite)
- VPS dédié — Hetzner CX22 (~5€/mois) ou équivalent. **Flux écarté** : containers éphémères, IPs variables (vu 3 IPs en une session sur hideringseed1), mauvais fit pour stratum permanent.
- Daemon HRG dédié pool, RPC `--rpc-bind-ip 127.0.0.1`, sync mainnet via `--add-priority-node <IP-courante-Flux>:19740` + `--block-notified` pour template refresh instantané.
- TLS sur stratum (`pool-ssl-port`) + frontend CDN sur webui:4243.
- Monitoring : pool hashrate, orphan rate, payout queue.

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
- Build verifié 23 mai : `bin/hidering-wallet-gui` 35 MB + 18 binaires CLI HRG (hideringd, hidering-wallet-cli/rpc, blockchain-tools…), smoke test 15s alive sous WSLg, Qt 5.15.13, aucun warning d'asset manquant.
- **Release Linux v2.0.0-gui publiée 23 mai 2026** : https://github.com/AB-lab113/hidering-gui/releases/tag/v2.0.0-gui — deux assets :
  - `Hidering_Wallet-v2.0.0-x86_64.AppImage` (54 MB) — **recommandé**, self-contained (Qt5 + plugins + QML modules + libs système bundlées). `chmod +x` + run. Sha256 `423904575e9235d5c158109580b6caeb1a8d0609ac6be32d45cf295919df8a87`.
  - `hidering-wallet-gui-v2.0.0-linux-x64.tar.gz` (11 MB) — binaire stripped, dynamically linked, nécessite Qt5.15+boost1.83+openssl3+libsodium côté host. Sha256 `0defa8ab45285de46d98bf5828dbe86c797a6df593198458e984d0f52fc7460b`.
  - Build sur Ubuntu 24.04 glibc 2.39 avec `-unsupported-allow-new-glibc` → target distros doivent avoir glibc ≥ 2.39 (Ubuntu 24.04+, Debian 13+, Fedora 39+). Pour broader compat il faudrait rebuilt sur une base distro plus ancienne.
  - **Surprise Release build** : les libs HRG (libwallet_api, libepee, libopenpgp, libcommon, libnet, libversion, libtranslations, libeasylogging) sont **statically linked** dans `hidering-wallet-gui` en mode Release (find . -name "*.so" sort uniquement Qt5 + system libs). Debug build les laisse dynamic. Mécanisme cmake exact pas identifié — possiblement cmake target type default ou interaction `-fPIC`.
  - **Gotcha AppImage (runtime download)** : le download appimagetool du runtime-x86_64 timeout/502 souvent depuis github — workaround : `gh release download continuous --repo AppImage/type2-runtime --pattern runtime-x86_64` + `appimagetool --runtime-file ./runtime-x86_64 AppDir <output>.AppImage`.
  - **GOTCHA linuxdeployqt CASSÉ (brûlé 25 mai 2026, 3 builds ratés)** : `linuxdeployqt` « continuous » build 107 (2025-10) détecte les libs Qt via ldd mais **ne les copie jamais** dans l'AppDir (0 lib Qt5, pas de plugin platforms) → `appimagetool` échoue ensuite « Desktop file not found ». Pas un problème de qmake/patchelf/PATH (tous vérifiés OK). **Solution : bundler avec linuxdeploy + linuxdeploy-plugin-qt** (TheAssassin) à la place — bundle Qt correctement (16 libs + libqxcb + AppRun wrapper + qt.conf + rpath `$ORIGIN/../lib`). Ces outils **requièrent patchelf**, absent par défaut → `pip install --user --break-system-packages patchelf` (PEP 668). `scripts/build-appimage.sh` réécrit en conséquence (commit GUI `80008d1f`) : DL linuxdeploy/plugin-qt/appimagetool (`--appimage-extract`), `QMAKE=/usr/lib/qt5/bin/qmake QML_SOURCES_PATHS=$REPO linuxdeploy --appdir AppDir -e <bin> -i <icon> --icon-filename hidering-wallet-gui -d <desktop> --plugin qt`, ajoute `libqoffscreen.so` (smoke test headless via `QT_QPA_PLATFORM=offscreen`), packe avec `appimagetool --runtime-file`. Les `ERROR: Missing qml module: moneroComponents.*` au bundling sont **bénins** (types QML enregistrés en C++, pas des modules `.qml` disque).
  - **NB sed-trap historique** : `-extra-plugins=platforms/libqxcb.so` (slash dans le nom) crashait linuxdeployqt — non pertinent désormais (linuxdeploy gère les plugins via `--plugin qt`).
- **Release Linux v2.0.1-gui publiée 25 mai 2026** : https://github.com/AB-lab113/hidering-gui/releases/tag/v2.0.1-gui — **thème gold** (#FFD700, remplace l'orange Monero #FF6C3C ; erreurs en rouge #FF4444), wordmark titlebar « HIDERING », **monogramme rond HRG sur les cartes de compte** (ex-logo ɱ Monero baked dans `card-background-black*.png`), `.desktop` rebrandé (Name=HIDERING Wallet). Tag au commit `17a5f65e`. Assets : `Hidering_Wallet-v2.0.1-x86_64.AppImage` (55 MB, sha256 `04f79f096a3c3791f07b70e31e197888f84b51d06e1d30e3c24d221c6376f45a`) + `hidering-wallet-gui-v2.0.1-linux-x64.tar.gz` (11.5 MB, sha256 `6fad0b79f3a124cc7b8ad3b191de75ac69a8fa019c8e12ce74f1cf648249280e`). Site `docs/index.html` MAJ vers v2.0.1-gui (commit backend `7944d86e2`).
- **Release multi-plateformes v2.0.2-gui publiée 3 juin 2026** : https://github.com/AB-lab113/hidering-gui/releases/tag/v2.0.2-gui — **1ère release GUI multi-OS, buildée par CI GitHub Actions** (workflows `.github/workflows/build-{macos,windows}-gui.yml` ajoutés sur `master`, commit `5caa0431` ; CI PAS bloquée par billing, cf. [[project_ci_build_workflows]]). Tag au commit `5caa0431`, backend submodule `monero` bumpé à v2-privacy HEAD `55815b345` (v2.0.2 + PQC/liboqs). Assets :
  - `Hidering_Wallet-macos-arm64.tar.gz` — 36 MB — sha256 `63b96e4c1dfe343f1d591a12171eb98af6331ac0e7b6f4344b4e5e7e9947c9da` (macos-14, Qt5 via Homebrew `qt@5`, `deploy` target macdeployqt + codesign ad-hoc → `.app` bundle)
  - `Hidering_Wallet-windows-x64.zip` — 68 MB — sha256 `9cb4f9c39bb5db070ab293656d1b24a2d59a2ef6f2726d598398e63217a5e0ca` (windows-latest MSYS2/mingw64, Qt5, windeployqt + bundling récursif des DLL mingw via ldd)
  - `Hidering_Wallet-v2.0.1-x86_64.AppImage` — 52 MB — sha256 `04f79f096a3c3791f07b70e31e197888f84b51d06e1d30e3c24d221c6376f45a` (**report inchangé de v2.0.1-gui** — Linux pas encore rebuildé en v2.0.2)
  - Chaque asset + sidecar `.sha256`.
  - **Bugs latents corrigés (jamais buildé mac/win avant — seul l'AppImage Linux l'avait été)** : (1) `cmake/Deploy.cmake` référençait la cible pré-rebrand `monero-wallet-gui` → configure cmake échoue sur APPLE/WIN32 ; renommé `hidering-wallet-gui`. (2) `share/Info.plist` `CFBundleExecutable=monero-wallet-gui` → macdeployqt « Could not find bundle binary » ; rebrandé. (3) gitlink submodule `monero` épinglé `93d7d3b46` (pré-liboqs 30 mai) → CI « monero/external/liboqs does not exist » (masqué localement par le symlink) ; bumpé `55815b345`. (4) link Windows `undefined reference to __imp_CoTaskMemFree` (oshelper.cpp) → ajout `-lole32 -lshell32 -luuid -lshlwapi`.
  - Recette CI : liboqs buildé en premier dans `monero/external/liboqs` (pqc.cpp inclut oqs.h inconditionnellement), `-DMANUAL_SUBMODULES=ON -DDEV_MODE=OFF -DUSE_DEVICE_TREZOR=OFF`, link dynamique. Release jobs (concurrency group partagé `hidering-gui-release-<ref>`) attachent les assets au tag `v*`.
  - Site `docs/index.html` MAJ vers v2.0.2-gui 3 plateformes + roadmap Phase 4H « Wallet GUI Desktop » = Complete + Requirements multi-OS. Mémoire associée : [[project_hidering_gui_fork]].
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

### Étape 1 — liboqs intégré ✅ (30 Mai 2026, commit `2842e364e` sur v2-privacy)
- **Dépendance** : Open Quantum Safe **liboqs 0.10.1** (submodule `external/liboqs`, pinné `5dd87dca`, URL dans `.gitmodules`). Buildé static/OpenSSL-only :
  `cmake -S external/liboqs -B external/liboqs/build -DBUILD_SHARED_LIBS=OFF -DOQS_USE_OPENSSL=ON -DOQS_BUILD_ONLY_LIB=ON && cmake --build external/liboqs/build -j$(nproc)`
  → `external/liboqs/build/lib/liboqs.a` (9.7 MB), adossé à OpenSSL 3.0.13. Dilithium3 + Kyber768 activés (vérifié dans `oqsconfig.h`).
- **CMake** : `external/CMakeLists.txt` expose la cible **IMPORTED `oqs`** (PAS `add_subdirectory(liboqs)`). **GOTCHA brûlé** : `add_subdirectory(liboqs)` casse la config full-tree — liboqs définit en interne une cible `common` qui entre en collision avec la cible `common` de HIDERING (CMP0002 : « another target with the same name already exists »). La cible imported ne tire que l'archive prébuildée + headers → 0 collision, et liboqs n'est pas rebuildé par le build daemon. Config full-tree re-vérifiée rc=0, 0 erreur.
- **Smoke test** : `src/crypto/pqc_test.cpp` — Dilithium3 keygen/sign/verify de `"HIDERING_PQC_TEST"` (+ rejet signature altérée) et round-trip Kyber768 encaps/decaps. Compile standalone (`g++ -std=c++17 -I external/liboqs/build/include ... external/liboqs/build/lib/liboqs.a -lcrypto`) → **RESULT: PASS**. Tailles : Dilithium3 pk 1952 / sk 4000 / sig 3293 B ; Kyber768 pk 1184 / sk 2400 / ct 1088 / ss 32 B.
- **NB** : l'archive `.a` est un artefact de build non versionné — un clone doit lancer le build liboqs une fois (recette ci-dessus, documentée dans le commentaire `external/CMakeLists.txt`).

### Étape 2 — types de clés PQ + ops + HFv16 placeholder ✅ (31 Mai 2026, commit `3a7bc33fc` sur v2-privacy)
Architecture (additive, n'altère PAS le ring signature existant) : CLSAG + Ed25519 conservés ; Dilithium3 ajouté comme **signature externe dans le champ `extra` des TX** ; Kyber768 remplace l'ECDH pour les **nouvelles adresses BQ...** ; HFv16 (additif) à hauteur 1,000,000 (placeholder).
- **`src/crypto/pqc.{h,cpp}`** (namespace `crypto::pqc`) : structs `pq_public_key` (dilithium3_pk 1952 + kyber768_pk 1184), `pq_secret_key` (dilithium3_sk 4000 + kyber768_sk 2400), `pq_signature` (sig 3293), `kyber_ciphertext` (ct 1088), `kyber_shared_secret` (ss 32) ; fonctions `pqc_keygen` / `pqc_sign` / `pqc_verify` / `pqc_kem_encaps` / `pqc_kem_decaps` implémentées sur liboqs (Dilithium3 + Kyber768). Tailles compile-time `constexpr` recroisées contre les valeurs runtime liboqs (un futur bump liboqs qui changerait une taille échoue bruyamment).
- **`src/cryptonote_config.h`** : `CRYPTONOTE_PQ_ADDRESS_PREFIX = 0x3C11` (adresses BQ..., dans le namespace `config` mainnet), `HF_VERSION_PQ = 16`, `HF_HEIGHT_PQ = 2000000ULL` (placeholder ; **repoussé de 1000000ULL → 2000000ULL par l'audit M-6 le 5 juin 2026** pour écarter le risque d'auto-halt — threshold 0 + hauteur fixe = règle réelle).
- **`src/hardforks/hardforks.cpp`** : HFv16 enregistré dans `mainnet_hard_forks[]` → `{ HF_VERSION_PQ, HF_HEIGHT_PQ, 0, 1713700002 }`, **inactif** (h=2,000,000 depuis l'audit M-6, très au-dessus du tip ~h11000). **DÉVIATION assumée** : la tâche disait « table des hard forks dans `checkpoints.cpp` » mais ce fichier ne contient QUE des ancres de hash de bloc (h=2939/5000/11000/16000/20000), pas de table HF. La vraie table est `hardforks.cpp` → c'est là que c'est posé. `hardforks.cpp` inclut désormais `cryptonote_config.h` (résolu via `include_directories(... src ...)` global). **NB consensus** : threshold 0 + hauteur fixe = règle réelle ; à h=1,000,000 tout bloc DEVRA être major_version 16. À finaliser/ajuster avant que le tip n'en approche.
- **`src/crypto/CMakeLists.txt`** : `pqc.cpp` ajouté à `crypto_sources`, cible IMPORTED `oqs` linkée `PUBLIC` à `cncrypto`. **GOTCHA brûlé** : `monero_add_library` compile les sources dans une object lib `obj_cncrypto` qui n'hérite QUE de `INTERFACE_COMPILE_DEFINITIONS` du target réel, **pas des include dirs des link deps** → `<oqs/oqs.h>` introuvable au build. Fix : `target_include_directories(obj_cncrypto PRIVATE $<TARGET_PROPERTY:oqs,INTERFACE_INCLUDE_DIRECTORIES>)`.
- **Build** : `make -j$(nproc) daemon` clean, `hideringd` v2.0.0 linké. Les fonctions `pqc_*` sont dead-strippées du daemon (aucun call site encore — attendu) ; `pqc.cpp.o` compile avec les refs `OQS_*` correctes. **Smoke test wrappers standalone** : `sign_len=3293`, verify OK, rejet signature altérée OK, Kyber768 encaps/decaps shared-secret match OK.
- **Reste à faire (Phase 5)** : design complet du format tx PQ (sérialisation des signatures Dilithium3 dans `tx_extra`, dérivation d'adresse BQ... via Kyber768), câblage des `pqc_*` dans les call sites (wallet keygen, signature/vérif tx, scan), règles consensus HFv16, impl+audit T1 2027, hard fork mainnet T2 2027.

### Étape 3 — intégration Dilithium3 dans le flow de signature des TX ✅ (3 Juin 2026, commit `3c92a61d9` sur v2-privacy)
Signature Dilithium3 externe portée dans `tx.extra`, **entièrement conditionnée par `hf_version >= HF_VERSION_PQ` (16, h=1,000,000)**. La chaîne actuelle (hf 15, tip ~h11400) est intacte : tous les chemins PQ sont skippés. CLSAG + Ed25519 inchangés.
- **`src/cryptonote_config.h`** : `static const uint8_t TX_EXTRA_TAG_PQ_SIG = 0x06` (libre : tx_extra.h utilise 0x00–0x04, 0xDE).
- **`src/crypto/pqc.{h,cpp}`** : `struct pq_tx_sig { pk[1952]; sig[3293]; }` + `pqc_tx_sign` / `pqc_tx_verify` sur liboqs Dilithium3. **DÉVIATION assumée** : `pqc_tx_sign` prend aussi `pk` (la signature de la tâche ne passait que `sk`) — liboqs ne dérive pas la pk depuis la sk Dilithium3, donc `pk` est requis pour remplir `out_sig.pk`.
- **`src/cryptonote_core/cryptonote_tx_utils.cpp`** : helper `add_pq_sig_to_extra()` (sérialise `[tag|pk|sig]`) ; `construct_tx_with_tx_key` append la sig PQ comme **dernier** champ de `tx.extra` sous la garde hf. Garde threadée via un nouveau param trailing `uint8_t hf_version = 0` (sur `construct_tx_with_tx_key` + `construct_tx_and_get_tx_key`) → faux à tous les call sites actuels.
- **`src/cryptonote_core/blockchain.cpp`** : `check_tx_inputs` parse le champ PQ en fin de `extra` et le vérifie contre le prefix hash **avec ce champ retiré** (= le message signé à la construction) ; rejet si sig manquante/invalide. Skippé sous HF_VERSION_PQ.
- **Build** : `make -j$(nproc) daemon` clean, **0 warning**. **Smoke test** : daemon regtest `--offline` mine 2 blocs (coinbase-only → chemin PQ non exercé), démarre et tourne sans throw.
- **CAVEATS à traiter en Étape 4** :
  1. **Keypair Dilithium3 jetable par TX** (`pqc_keygen` à chaque construction, dans le bloc gardé) — le vrai plumbing des clés BQ-address dans la construction de TX est à câbler en Étape 4. Code actuel = stub compilable/runnable, PAS le schéma final.
  2. **`construct_tx_with_tx_key` passe `hf_version=0` par défaut** — le wallet doit passer la vraie HF version. Asymétrie regtest (fakechain force hf=16 → validateur exige la sig PQ alors que le wallet n'en émet pas) tant que ce câblage n'est pas fait. Mainnet (hf 15) non affecté.
  3. **`MAX_TX_EXTRA_SIZE` (3000) trop petit** pour pk(1952)+sig(3293) — à relever **uniquement à l'activation HFv16** (changement consensus).

### Étape 4 — intégration Kyber768 dans la dérivation des stealth addresses (adresses BQ...) ✅ (4 Juin 2026, commit `0c489dbdc` sur v2-privacy)
Kyber768 (KEM) câblé dans la dérivation d'adresse furtive pour le format BQ... (`CRYPTONOTE_PQ_ADDRESS_PREFIX = 0x3C11`). Additif, **entièrement conditionné par `hf_version >= HF_VERSION_PQ` (16, h=1,000,000)** ET, pour le mélange par sortie, par un flag `is_pq` par destination qu'aucun call site actuel ne met à `true` → chaîne hf 15 doublement inerte. ECDH Ed25519 des adresses B... inchangé.
- **`src/crypto/pqc.{h,cpp}`** : `struct pq_stealth_keys { kyber_pk[1184]; kyber_sk[2400]; }` + `pqc_stealth_encaps` / `pqc_stealth_decaps` (wrappers KEM Kyber768, tailles recroisées contre liboqs).
- **`src/cryptonote_config.h`** : `TX_EXTRA_TAG_KYBER_CT = 0x07` (libre) ; `MAX_TX_EXTRA_SIZE_PQ = 8192` (caveat 3). **`MAX_TX_EXTRA_SIZE` reste 3000** pour la chaîne live — `tx_pool.cpp:185` l'applique à toute TX entrante ; le plafond PQ n'est consulté que sous la garde HFv16 (sinon on relâcherait la policy de relais hf 15).
- **`tx_destination_entry`** (`cryptonote_tx_utils.h`) : flag `is_pq` **non sérialisé** (défaut false) marquant une destination BQ... ; n'altère jamais le wire format des TX.
- **`src/cryptonote_core/cryptonote_tx_utils.cpp`** : pour une sortie BQ... sous la garde, `pqc_stealth_encaps` → mélange du shared secret dans la one-time key par `P' = P + H_s(ss)·G` (point valide et récupérable par le destinataire — PAS un XOR brut qui casserait le point) ; la kyber_ciphertext (1088 B) est appendue à `tx.extra` (tag 0x07) **après `sort_tx_extra`** (qui dropperait les tags inconnus) et **avant** la signature Dilithium3 finale → l'invariant Étape 3 « la sig PQ est le dernier champ » est préservé.
- **`src/cryptonote_core/blockchain.cpp`** : documenté que les champs Kyber CT sont **hors consensus** (le validateur n'a pas les clés du destinataire, ne peut pas décapsuler) ; le décapsulage est côté wallet via `pqc_stealth_decaps`. Le calcul d'offset de la sig Dilithium reste correct (CT placées avant elle).
- **3 caveats de l'Étape 3 traités** : caveat 1 → TODO commenté pour signer avec la vraie clé Dilithium3 persistante du sender (account_keys n'a pas encore de champ pq, throwaway gardé) ; caveat 2 → TODO aux 3 call sites `construct_tx_and_get_tx_key` dans `wallet2.cpp` pour passer la vraie `hf_version` (omise → 0 → PQ inerte) ; caveat 3 → `MAX_TX_EXTRA_SIZE_PQ` sous garde.
- **Build** : `make -j$(nproc) daemon` clean, **0 warning**. **Smoke test** : daemon regtest `--offline` mine 2 blocs (h1→3, coinbase-only → chemin PQ/Kyber non exercé), log clean, aucun throw.

### Étape 5 — format d'adresse BQ... + plumbing des clés Kyber768 wallet ✅ (4 Juin 2026, commit `b77639979` sur v2-privacy)
La clé Kyber768 du destinataire est désormais portée de bout en bout → les adresses BQ... sont constructibles/dépensables, **entièrement gardé par HFv16 (h=1,000,000) et/ou `addr.is_pq()`**. Chaîne hf 15 intacte ; format d'adresse B... classique **byte-identique** (wire/base58/fichier wallet) — les nouveaux champs optionnels sont absents de TOUTES les maps de sérialisation (y compris `account_boost_serialization.h`).
- **`src/cryptonote_basic/cryptonote_basic.h`** : `account_public_address` gagne `boost::optional<std::array<uint8_t,1184>> pq_kyber_pk` + `is_pq()`. Hors des deux macros de sérialisation → B... inchangé ; la clé BQ est (dé)sérialisée hors-bande par `get_account_address_{as,from}_str_pq()`.
- **`src/cryptonote_basic/account.h`** : `account_keys` gagne `boost::optional<crypto::pqc::pq_stealth_keys> pq_keys` (clé de décapsulation du destinataire) ; non sérialisé, `boost::none` pour tout wallet existant.
- **`src/cryptonote_basic/cryptonote_basic_impl.{cpp,h}`** : `get_account_address_as_str_pq()` / `get_account_address_from_str_pq()` — encode/parse `spend(32)|view(32)|kyber768_pk(1184)` sous `::config::CRYPTONOTE_PQ_ADDRESS_PREFIX` (0x3C11). Le parser clé sur le préfixe **numérique**.
- **`src/cryptonote_core/cryptonote_tx_utils.cpp`** : `tx_destination_entry.is_pq` dérivé de `addr.is_pq()` ; l'encaps Kyber768 utilise la **vraie** clé destinataire (throwaway Étape 4 retiré).
- **`src/wallet/wallet2.{cpp,h}`** : `apply_pq_output_tweak()` (récupération du tweak Kyber au scan, gardé par `pq_keys` → no-op sans clé PQ, donc inerte sur la chaîne live) ; caveat 2 résolu via `get_pq_hf_version()` sur les 2 call sites `construct_tx_and_get_tx_key` online (le site cold-signing reste à 0 — pas de daemon pour interroger le HF).
- **`src/wallet/message_store.{cpp,h}`** : `account_public_address` n'est plus trivially-copyable → 2 `memset(0)` remplacés par value-init (corrige `-Wclass-memaccess`). NB : `AUTO_VAL_INIT` reste OK (boost::value_initialized, pas de memset) ; le chemin legacy hex `public_address_outer_blob` (reinterpret_cast pragma-pack) est size-gardé avant le cast.
- **Build** : `make -j daemon wallet simplewallet` clean, **0 nouveau warning** (restent les pré-existants `refreshed`/`loaded` dans wallet2.cpp + trezor protobuf). **Smoke** : `src/crypto/pq_address_test.cpp` (committé, standalone comme `pqc_test.cpp`) → B... round-trip avec `is_pq()=false` + rejet BQ ; BQ... encode/parse round-trip avec la clé 1184 o. Daemon regtest `--offline` a miné 2 blocs (h1→3), log clean.

**Items ouverts Étape 5 (aucun ne bloque la chaîne live)** :
1. **Rien ne set `account_keys.pq_keys` encore** → keygen BQ + dérivation subaddress BQ à faire ; tant que c'est le cas, le tweak de scan (`apply_pq_output_tweak`) est du **dead code** et l'Étape 4 reste exerçable mais pas le schéma final côté destinataire.
2. **Le préfixe `0x3C11` ne rend PAS "BQ" en base58** — les chars de tête dépendent du blob complet `varint(tag)||payload||checksum`, et sur le payload 1184 o on observe "**RLF...**" (le commentaire Étape 4 "begins BQ" était faux, corrigé dans `cryptonote_config.h`). Tuning du tag pour un préfixe mnemonic à faire.
3. **Cold-signing HFv16** : le site de signature offline (`sign_tx`) ne peut pas interroger le HF (pas de daemon) → `hf_version` doit être **threadé dans les unsigned-tx exportées** (`tx_construction_data`) plutôt que fetché live.
4. **Détection des sorties BQ** : l'Étape 5 ne récupère le secret QUE post-détection dans `scan_output` ; détecter une sortie BQ exige de dé-tweaker la clé avant `is_out_to_acc_precomp` (chemin chaud) — à câbler avec le keygen BQ (item 1).

Mémoire associée : `~/.claude/projects/-home-shark-hidering/memory/project_phase5_step5_bq_address.md`

### Étape 6 — keygen BQ + préfixe mnemonic "BQ" + détection des sorties BQ ✅ (4 Juin 2026, commit `62cb02066` sur v2-privacy)
Résout les items ouverts de l'Étape 5 (sauf la persistance de `pq_keys`). Tout gardé par HFv16 (h=1,000,000) et/ou la possession d'une clé Kyber768 par le compte ; chaîne hf 15 + wallets existants byte-identiques.
- **Préfixe mnemonic "BQ" — un préfixe base58 seul ne PEUT PAS rendre "BQ" (prouvé empiriquement + analytiquement).** Les chars de tête de `encode_addr` viennent du 1er bloc de 8 octets = `varint(tag)||début payload`. Un 'B' de tête (index 10) exige que l'octet de poids fort du bloc ∈[60,65], ce qui n'arrive que pour un **varint 1 octet** (tag 60–65) — le 1er octet d'un varint multi-octets est ≥128 (bit de continuation) → char de tête ≥ 'N'. Mais un tag 1 octet laisse 7 octets aléatoires (spend key) dans le bloc → le 2e char varie. Parmi 60–65 le 2e digit de base est {60:'3',61:'D',62:'N'=21,64:'i'=41} et un octet marqueur positif ne pousse le 2e digit que de ≤~9 crans → **seul le tag 62 peut atteindre 'Q'=23** (balayage 1..70000 : aucun tag ne donne un "BQ" 2-chars stable). Solution : **tag 62 + octet marqueur fixe `0x33` préfixé au payload d'adresse** → "BQ" stable (vérifié sur 5000 payloads aléatoires ; plage marqueur valide 0x29–0x41). `CRYPTONOTE_PQ_ADDRESS_PREFIX` 0x3C11 → **62** ; nouveau `CRYPTONOTE_PQ_ADDRESS_MARKER=0x33`. Payload BQ = `marker(1)|spend(32)|view(32)|kyber(1184)=1249 o`.
- **Collision préfixe 62 = préfixe subaddress** (c'est le seul tag qui rend "BQ", collision assumée). Désambiguïsée par la taille du payload décodé : subaddress=64 o, BQ=1249 o. `parse_binary` ne rejette PAS les octets en trop → ajout d'un size-guard dans la branche subaddress de `get_account_address_from_str` (`data.size()!=64 → false`). Le parser BQ vérifie aussi la taille (1249) + l'octet marqueur.
- **Keygen** (`account.{h,cpp}`) : `generate_pq_keys(account_keys&)` (keygen Kyber768 → `keys.pq_keys` + `keys.m_account_address.pq_kyber_pk`, is_pq()=true ; moitié Dilithium ignorée ici) ; `get_pq_address_str(keys, nettype)` ; `account_base::get_keys_nonconst()`. `wallet2::generate(...)` gagne un param trailing `use_pq=false` (défaut → wallet classique B...).
- **Détection des sorties BQ** (`wallet2.{h,cpp}`) : `get_pq_output_untweak_point(tx)` décapsule la CT Kyber de la tx et renvoie `t*G` (t=H_s(ss)) ; nullptr pour les wallets classiques. `check_acc_out_precomp*` prennent un `pq_untweak` optionnel — si le match classique `is_out_to_acc_precomp` échoue, retry sur le **candidat dé-tweaké `P_onchain − t*G`** (le view tag dépend de la dérivation seule → matche toujours). L'overload caché retombe sur le chemin complet en cas de cache-miss. Calculé une fois par tx. Cohérent avec l'émetteur Étape 4 (`P'=P+t*G`, view_tag calculé AVANT le tweak) et `apply_pq_output_tweak` Étape 5 (ajoute t au secret post-détection).
- **Tests/build** : `src/crypto/pq_keygen_test.cpp` (nouveau, standalone) PASS — classique reste is_pq()=false, BQ rend "BQ..." + round-trip. `pq_address_test.cpp` rend désormais "BQ" aussi. `make -j daemon wallet simplewallet` clean, **0 nouveau warning** (pré-existants `refreshed`/`loaded` restent). Daemon regtest `--offline` a miné 2 blocs, log clean.

**Items ouverts Étape 6 (aucun ne bloque la chaîne live)** : (1) `pq_keys` toujours NON sérialisé (Étape 5 a gardé le fichier wallet byte-identique) → les clés BQ sont éphémères par session ; persistance chiffrée à faire, et tant que c'est le cas un wallet BQ rechargé ne peut ni scanner ni dépenser. **(RÉSOLU Étape 7.)** (2) Dérivation des subaddress BQ non faite (seule l'adresse BQ primaire). (3) Cold-signing `hf_version` toujours 0 (pas de daemon) — à threader dans l'unsigned-tx exportée. (4) `MAX_TX_EXTRA_SIZE` → `MAX_TX_EXTRA_SIZE_PQ` uniquement à l'activation HFv16 (changement consensus).

Mémoire associée : `~/.claude/projects/-home-shark-hidering/memory/project_phase5_step6_bq_keygen.md`

### Étape 7 — persistance des clés BQ (sérialisation chiffrée dans le fichier wallet) ✅ (4 Juin 2026, commit `4419f7c5b` sur v2-privacy)
Résout l'item ouvert #1 de l'Étape 6 : `account_keys.pq_keys` est désormais sérialisé (chiffré) dans le fichier `.keys` → un wallet BQ rechargé peut de nouveau scanner/dépenser. Additif, gardé par la présence de `pq_keys` ; **wallets B... classiques byte-identiques**.
- **`src/cryptonote_basic/account.h`** : `pq_keys` ajouté au `BEGIN_KV_SERIALIZE_MAP()` de `account_keys` comme **un seul blob de taille fixe `crypto::pqc::pq_stealth_keys`** (`kyber_pk||kyber_sk` = 3584 o), écrit UNIQUEMENT si `pq_keys` est set. Bloc manuel `if (is_store) {...} else {...}` via `epee::serialization::selector<is_store>::serialize_t_val_as_blob` (KV_SERIALIZE_OPT ne sait pas prendre un `boost::optional<struct>`). Les wallets classiques n'émettent AUCUN champ `"pq_keys"` → `key_data` au format identique. **Au chargement, réhydratation de `m_account_address.pq_kyber_pk`** depuis le `kyber_pk` (en clair) du blob → `is_pq()` redevient true.
- **DÉVIATION assumée vs la tâche** : la tâche demandait littéralement `FIELD_OPT(pq_kyber_pk)` dans la sérialisation de `account_public_address`. **Refusé** — le design de l'Étape 5 garde les octets B... wire/base58/fichier identiques en portant la clé BQ hors-bande ; toucher au `BEGIN_SERIALIZE_OBJECT` binaire risquerait le format wire/consensus des TX. La clé PQ de l'adresse voyage donc imbriquée sous `pq_keys` + réhydratée au load → contrainte absolue respectée, zéro risque.
- **`src/cryptonote_basic/account.cpp`** : `xor_with_key_stream` chiffre aussi `kyber_sk` (2400 o, après les clés multisig dans le flux dérivé) quand `pq_keys` présent, exactement comme les secrets Ed25519 ; `kyber_pk` reste en clair. `pq_sk_bytes = 0` pour les wallets classiques → longueur du flux + octets disque inchangés. L'état chiffré de `kyber_sk` est toujours synchrone avec la spend key (mêmes appels xor).
- **Aucune modif `wallet2.cpp`** : `store_keys`/`load_keys` passent déjà par le KV map de `account_keys` (`store_t_to_binary`/`load_t_from_binary`) + `encrypt_keys`/`decrypt_keys`. Le **cache** wallet sérialise `m_account_public_address` (copie redondante, spend+view seuls) mais PAS `m_account` → l'état PQ autoritaire vit uniquement dans `.keys`.
- **Test** : `src/crypto/pq_persistence_test.cpp` (nouveau, standalone comme `pqc_test.cpp`) → BQ round-trip encrypt→serialize→reload→decrypt (kyber_sk absent en clair du blob, kyber_pk présent en clair, is_pq()=true, spend/view/kyber_sk/kyber_pk tous récupérés) ; wallet classique round-trip inchangé, pas de champ pq_keys, is_pq()=false. **RESULT: PASS**.
- **Build** : `make -j8 daemon wallet` clean, **0 nouveau warning** (pré-existants `refreshed`/`loaded` + trezor protobuf). **Smoke** : daemon regtest `--offline` a miné 2 blocs (log clean ; l'unique `Exception: std::system_error` à stack_trace.cpp:134 est le faux positif connu de l'interposer `__cxa_throw`).

**Items ouverts Étape 7 (aucun ne bloque la chaîne live)** : (1) dérivation des subaddress BQ ; (2) cold-signing `hf_version` à threader dans l'unsigned-tx exportée ; (3) bump `MAX_TX_EXTRA_SIZE`→`MAX_TX_EXTRA_SIZE_PQ` à l'activation HFv16 ; (4) le chemin `account_boost_serialization.h` omet toujours `pq_keys` (utilisé pour cold-sign/multisig, lié à l'item cold-sign).

Mémoire associée : `~/.claude/projects/-home-shark-hidering/memory/project_phase5_step7_bq_persistence.md`

### Étape 8 — Audit de sécurité Phase 5 ✅ (5 Juin 2026, 9 commits `fccfd6b07` → `f6d2d62a9` sur v2-privacy)
Audit complet du code PQC (Dilithium3/Kyber768) : **18 findings** (1 CRITIQUE + 5 ÉLEVÉ + 6 MOYEN + 5 FAIBLE), **tous corrigés**. Daemon + wallet buildent clean ; regtest miné jusqu'à h6 status OK → **chaîne live (hf 15) confirmée intacte**. Chaque correctif reste gardé par `hf_version >= HF_VERSION_PQ` et/ou `pq_keys`. Reward/magic/ring floor/supply **non touchés**.
- **C-1 (CRITIQUE)** : la clé Dilithium3 **jetable par TX** (n'authentifiait rien) remplacée par une **clé persistante** `account_keys.pq_dilithium` (générée dans `generate_pq_keys`, chiffrée ChaCha20 at-rest via `xor_with_key_stream` comme `m_spend_secret_key`) ; `construct_tx` signe avec. **RÉSIDUEL documenté (in-code + mémoire)** : un binding validateur *pleinement souverain* « pq_sig.pk ↔ émetteur » est **impossible sur une chaîne privacy** (le validateur ne voit pas l'émetteur masqué par le ring). La sig du ring engage déjà le champ PQ ; le binding d'identité PQ complet nécessite une **spec de registre d'adresses BQ — À FINALISER AVANT TOUTE ACTIVATION HFv16.**
- **E-3/M-1/M-2** : parsing **TLV canonique** — types variant `tx_extra_pq_sig` (0x06) / `tx_extra_kyber_ct` (0x07) enregistrés dans `tx_extra.h` (**wire-identique** aux blobs bruts) ; le validateur (`blockchain.cpp`) exige via `parse_tx_extra` exactement 1 sig (dernière), CT ≤ nb outputs, zéro octet résiduel ; plus de copie profonde de la TX (tronquer/restaurer en place). `TX_EXTRA_PADDING_MAX_COUNT` relevé 255→2500 (côté parse seulement — le padding HIDERING dépassait déjà 255 et n'était donc jamais parsable canoniquement ; sans impact consensus).
- **E-1/E-2/E-4** : usage du *implicit-rejection* Kyber corrigé (succès decaps ≠ « pour nous » ; la propriété = le vrai match `is_out_to_acc_precomp`) ; flag `tx_scan_info.received_via_pq_untweak` → le tweak n'est appliqué QUE si l'output a matché via dé-tweak (un output classique partageant une TX avec un CT Kyber n'est plus corrompu — c'était un DoS de refresh). Helper **partagé** `cryptonote::derive_bq_output_tweak(ss, output_index)` = `H_s("HRG_BQ_TWEAK"‖ss‖idx)` (séparation de domaine + binding index + rejet du zéro), utilisé par l'émetteur (tx_utils) ET le récepteur (wallet2).
- **F-2/F-3** : ceiling ring testé sur `max_actual_mixin>63` (était `min`) ; guard `key_offsets` vide avant `size()-1`.
- **F-4** : padding **fixe 2500 o** (était hybride/proportionnel) ; check de taille déplacé après l'ajout de la sig. **E-5** : `MAX_TX_EXTRA_SIZE_PQ` (8192) sur relay (`tx_pool`) + consensus à HFv16. **M-5** : validation des longueurs Dilithium avant `OQS_SIG_verify`. **M-6** : `HF_HEIGHT_PQ` 1M→2M. **M-3** : memwipe des secrets PQ. **M-4** : pq_keys/pq_dilithium suivent la même discipline at-rest que `m_spend_secret_key` (mlock global = durcissement plus large, hors périmètre PQ).
- **F-1** : `pq_hf_version` threadé dans `unsigned_tx_set` (bump v3 rétro-compatible) → cold-signing gate correctement (inerte sur live, hf 0). **F-5** : `verify_pq_tx_well_formed()` auto-check structurel avant broadcast (gated HFv16).
- **Build gotcha** : `wallet2.cpp` OOM-kill `cc1plus` sous `-j$(nproc)` sur ce WSL2 7.4 GB → `make -j2 wallet` (même gotcha que le build GUI).

**Prochaines étapes Phase 5 (par ordre)** : (1) **finaliser la spec de binding PQ** (registre d'adresses BQ permettant au validateur de lier `pq_sig.pk` à l'émetteur) — bloquant pour l'activation HFv16 ; (2) **tests end-to-end complets** du flow BQ (keygen → envoi → scan → dépense → cold-sign) sur regtest avec hf forcé à 16 ; (3) **dérivation des subaddress BQ** (seule l'adresse BQ primaire existe) ; (4) **cold-signing** : vérifier le round-trip `pq_hf_version` à HFv16 + couvrir `account_boost_serialization.h` (omet encore `pq_keys`, lié multisig/cold-sign) ; (5) rafraîchir les tests standalone `pq_*_test.cpp` pour couvrir la clé Dilithium persistante (C-1).

Mémoire associée : `~/.claude/projects/-home-shark-hidering/memory/project_phase5_security_audit_fixes.md`
