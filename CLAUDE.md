# HIDERING (HRG) — Claude Code Project Memory
**Version synchronisée : Whitepaper v1.3 + Roadmap v2.0 — 30 Avril 2026**
**Dernière MAJ : 11 Mai 2026 (release v1.0.0 Linux publiée)**

## IDENTITÉ DU PROJET
Fork de Monero v0.18.1 rebrandé en HIDERING.
- Ticker : HRG
- Binaires : hideringd, hidering-wallet-cli, hidering-wallet-rpc
- Branche active : v2-privacy (HEAD : 739523ee6 au 11 mai 2026)
- Commit stable : 073f70af1
- Tag publié : **v1.0.0** → commit eb8ad1ee9 (rebrand banner + workflow CI)
- Release : https://github.com/AB-lab113/hidering/releases/tag/v1.0.0 (Linux x64 only)
- Repo : https://github.com/AB-lab113/hidering
- Build actif : build/release/bin/

## SPECS ÉCONOMIQUES
- Supply max : 33,000,000 HRG (hard cap irrévocable)
- Supply circulant max : 32,999,842.86 HRG (genesis NUMS déduit)
- Premine : 0 HRG (100% fair launch PoW)
- Block time : 120 secondes
- Halving interval : 210,000 blocs (~2.66 ans)
- Récompense initiale : 157.14 HRG/bloc
- Algorithme PoW : RandomX (CPU-only, ASIC-resistant)

## SPECS RÉSEAU
- Network ID (Mainnet) : HRG\x01HIDERINGMAIN
- Magic bytes : 0x48524701
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
- Patch 2 : TX padding fixe 2500 bytes OK
- Patch 3 : Mixnet natif 3-hops obligatoire OK
- Patch 4 : Normalisation montants 0.1 HRG OK
- Patch 5 : Stealth addresses V2 + view keys temporelles OK

## FICHIERS CRITIQUES
- src/cryptonote_config.h — config réseau, ring size, fees, magic bytes
- src/cryptonote_core/blockchain.cpp — logique blockchain (call sites de get_block_reward)
- src/hardforks/hardforks.cpp — schedule HFv15 (présent dès bloc 1)
- src/cryptonote_basic/cryptonote_basic_impl.cpp — impl. de base, **get_block_reward()** (halving)
- src/simplewallet/simplewallet.{h,cpp} — wallet CLI
- src/p2p/net_node.inl — réseau P2P
- src/common/dns_utils.cpp — seeds DNS (Monero purgés)
- src/debug_utilities/dns_checks.cpp — neutralisé
- GENESIS_PROOF.md — preuve NUMS genesis
- assets/hrg_logo.svg — logo

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

## BUG CRITIQUE — NON RESOLU (découvert 11 mai 2026, reporté à v1.0.1)
### MONEY_SUPPLY overflow uint64_t
Localisation : src/cryptonote_config.h:54 et src/cryptonote_basic/cryptonote_basic_impl.cpp:85

Le littéral `33000000000000000000ULL` (= 33 × 10¹⁹ = 33M HRG × 10¹² atomes)
**dépasse UINT64_MAX** (1.84 × 10¹⁹). Comportement actuel :
- Linux gcc : wrap silencieux → MONEY_SUPPLY effectif = 14,553,255,926,290,448,384
  atomes ≈ **14.55M HRG** (au lieu des 33M annoncés)
- macOS clang : erreur fatale de compilation, **macOS ne build pas**
- Windows MSYS2/mingw : non testé après le fix workflow (présumé : wrap comme Linux)

Conséquence sur v1.0.0 Linux : le cap d'émission est **14.55M HRG, pas 33M**.
À l'émission moyenne post-halvings (~78.57 HRG/bloc), le cap serait atteint
en moins d'un an de mainnet.

Cause racine architecturale : avec 12 décimales atomiques (héritées Monero),
uint64_t ne peut représenter QUE jusqu'à ~18.4M HRG. Tout cap > 18.4M HRG
nécessite soit (a) réduire les décimales à 11, soit (b) baisser le cap à
≤18.4M HRG, soit (c) élargir le type de already_generated_coins.

**Décision utilisateur (11 mai 2026) : v1.0.0 reste publié avec le cap
de fait à 14.55M HRG. Pas de retag. Fix groupé en v1.0.1 quand la
direction tokenomics sera tranchée.**

Détails complets et options dans la mémoire persistante :
`~/.claude/projects/-home-shark-hidering/memory/project_money_supply_overflow_v1.0.0.md`

## ETAT DES PHASES
- Phase 0 : Setup, fork, compilation — COMPLETE
- Phase 1 : Core tokenomics, genesis, testnet 284 blocs — COMPLETE
- Phase 2 : Privacy enhancements (5 patches) — COMPLETE
- Phase 3A : Rebranding complet — COMPLETE
- Phase 3B : Validation, sécurité, purge git — COMPLETE
- Phase 3C : Infrastructure seed nodes — EN COURS
- Phase 4A-C : Mainnet public launch — A FAIRE (Cible T2 2026)
- Phase 4D : Binaires publics — **PARTIEL** (Linux v1.0.0 publié, macOS bloqué par MONEY_SUPPLY, Windows à re-tester en v1.0.1)
- Phase 5 : Post-quantique (Dilithium3 + Kyber768) — A FAIRE (Cible T2 2027)

## PROCHAINES ETAPES (PAR ORDRE)
1. Build local : make -j$(nproc) hideringd
2. Wipe ~/.hidering + smoke test genesis NUMS
3. Daemon --offline valider genesis hash
4. Rebuild Docker image ab113hrg/hidering-seed:latest
5. Redéployer Flux seed node hideringseed1 (IP dynamique via Flux API, cf. section DOCKER ET DEPLOIEMENT)
6. DNS seed nodes
7. Block explorer
8. Binaires publics (Linux/Windows/Mac) — Linux DONE (v1.0.0), Win/Mac → v1.0.1
9. Pool mining compatible
10. Site web public
11. Phase 4 Launch

## RELEASE v1.0.0 (11 Mai 2026)
- Tag : v1.0.0 → commit eb8ad1ee9 (rebrand "Monero '" → "Hidering '" sur 19 sites + bump version 0.18.1.0 → 1.0.0)
- Asset publié : `hidering-v1.0.0-linux-x64.tar.gz` (14 MB, stripped, statically built sur Ubuntu 24.04)
- SHA256 : `d4a75a0d3c626e7dc3e726225b2cc67e8eb7396508c5cf6e54c459dedf333b8b`
- Contenu tarball : hideringd, hidering-wallet-cli, hidering-wallet-rpc + README + LICENSE + GENESIS_PROOF.md
- Workflow CI : `.github/workflows/build-release.yml` (déclenché sur `git tag v*`, jobs ubuntu/macos/windows + release softprops). Fix commit 739523ee6 sur v2-privacy corrige le target name `wallet-cli` → `simplewallet` (sera appliqué automatiquement au prochain tag depuis v2-privacy HEAD).

### Punch list v1.0.1 (à traiter avant de retag)
1. **MONEY_SUPPLY overflow** — décision tokenomics requise (voir section BUG CRITIQUE ci-dessus). Bloque le build macOS.
2. **std::bad_alloc au démarrage du daemon** — exception levée sur thread auxiliaire après "Genesis block" log (probablement le thread DNS checkpoint malgré --offline). Le daemon survit et continue normalement, mais l'exception au boot mérite investigation. Reproduction : `hideringd --offline --data-dir /tmp/foo`.
3. **Strings "Monero" / "monero-wallet-cli" résiduels** dans les help/usage/log filenames (le sed initial n'a touché que les sites `"Monero '" << MONERO_RELEASE_NAME`). Liste complète des file:line dans la mémoire `project_v1.0.1_punch_list.md`.
4. **CI workflow** — fix déjà mergé sur v2-privacy (commit 739523ee6), pas à refaire.

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

## DOCKER ET DEPLOIEMENT
- Image : `ab113hrg/hidering-seed:b43a29862` (tag `latest` = même digest)
- Digest pushé : `sha256:0a92296ce842edc690c72f2eecae32f737628381f4e9de58c556984119557c5e`
- Flux app : `hideringseed1`
  - Ports : P2P 19740, RPC 19741
  - IP courante : **ne pas hard-coder** — Flux re-schedule sur d'autres nodes à chaque delete/recreate (vu 3 IPs différentes en une session). Source de vérité : `curl https://api.runonflux.io/apps/location/hideringseed1`
  - Spec déployée (vérif) : `curl https://api.runonflux.io/apps/appspecifications/hideringseed1`
- Dockerfile : `Dockerfile.flux` à la racine (binaire copié depuis `build/release/bin/hideringd`)
- Spec Flux complète (pour delete/recréer) : `flux-hideringseed1.json` à la racine
- Gotcha : tout changement qui modifie le hash genesis (ex. NUMS migration `073f70af1`) nécessite un **wipe du volume persistant** au redeploy, sinon l'app reste sur l'ancien fork.

## POST-QUANTIQUE (PHASE 5 — 2027)
Hard fork additif (n'altère pas la blockchain existante) :
- Dilithium3 (CRYSTALS) — signatures
- Kyber768 — échange de clés
- Calendrier : spec 2026 → impl+audit T1 2027 → hard fork mainnet T2 2027
