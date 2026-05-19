# HIDERING (HRG) — Claude Code Project Memory
**Version synchronisée : Whitepaper v1.3 + Roadmap v2.0 — 30 Avril 2026**
**Dernière MAJ : 19 Mai 2026 (MIGRATION INFRA Flux → VPS dédiés — seed1.hidering.org pointe désormais sur OVH 135.125.243.137 (systemd hideringd.service, restart auto), seed2.hidering.org sur Contabo 207.180.211.96 (nouveau), block explorer migré sur http://135.125.243.137:8080. Flux `hideringseed1` en cours d'expiration, infrastructure désormais sur VPS propres avec IPs invariantes.)**

## IDENTITÉ DU PROJET
Fork de Monero v0.18.1 rebrandé en HIDERING.
- Ticker : HRG
- Binaires : hideringd, hidering-wallet-cli, hidering-wallet-rpc
- Branche active : v2-privacy (HEAD : 41f899049 au 16 mai 2026 + 2 fichiers uncommitted pour v2.0.0 : `src/cryptonote_config.h` NETWORK_ID byte[3] et `src/version.cpp.in` version string)
- Source courante : v2.0.0 (non tagué) — daemon local rebuilt 16 mai 2026, reporte `Hidering 'Privacy Enhanced' (v2.0.0-41f899049)`
- Dernière release publique : v1.0.2 → commit 9d0e593a6 (binaire incompatible avec source v2.0.0 — NETWORK_ID different)
- Tags publiés :
  - **v2.0.0** → *non tagué* — hard fork NETWORK_ID (HRG\x02HIDERINGMAIN, magic 0x48524702) + version 2.0.0 ; abandon volontaire chaîne v1.x à h≈3577 (16 mai 2026)
  - **v1.0.2** → commit 9d0e593a6 — fix RandomX/huge-pages bad_alloc au boot + version bump 1.0.2
  - **v1.0.1** → commit 47795728f — MONEY_SUPPLY corrigé (18M cap + 42.86 reward) + rebrand strings + version bump
  - **v1.0.0** → commit eb8ad1ee9 — rebrand banner initial + workflow CI (porte encore l'overflow MONEY_SUPPLY)
- Releases :
  - v2.0.0 — *non publié* (tag + binaires + Docker à faire, cf. punch list v2.0.0)
  - https://github.com/AB-lab113/hidering/releases/tag/v1.0.2 (Linux x64, stripped, 14.35 MB) — dernière publique, **NE PLUS UTILISER pour mainnet** post-HF
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
- Phase 4D : Binaires publics — **À REFAIRE** (v1.0.2 Linux publié 13 mai obsolète post-HF ; v2.0.0 source rebuilt local, binaires publics + tag à produire — CI GitHub Actions toujours bloquée par billing depuis 12 mai 2026)
- Phase 4E : Pool mining — **VALIDÉ LOCAL** (14 mai 2026, voir section POOL MINING ci-dessous). Stack : monero-pool jtgrassie. Pool doit linker contre les libs v2.0.0 et pointer un daemon v2.0.0 ; rebuild requis après HF.
- Phase 5 : Post-quantique (Dilithium3 + Kyber768) — A FAIRE (Cible T2 2027)

## PROCHAINES ETAPES (PAR ORDRE)
1. Commit v2.0.0 source patches (`src/cryptonote_config.h` + `src/version.cpp.in`)
2. Rebuild full : `make -j$(nproc)` (daemon target déjà rebuilt 16 mai ; wallet binaries encore en v1.0.2)
3. Wipe ~/.hidering + smoke test genesis NUMS sur le nouveau réseau
4. Daemon --offline valider genesis hash (inchangé — NETWORK_ID hors hash bloc)
5. Tag `v2.0.0` + release notes
6. ~~Rebuild Docker image `ab113hrg/hidering-seed:v2.0.0`~~ — **FAIT** 17 mai 2026 (utile uniquement pour Flux, désormais déprécié).
7. ~~Redéployer Flux seed node `hideringseed1`~~ — **OBSOLÈTE** (Flux abandonné 19 mai 2026, migration sur VPS dédiés).
8. ~~DNS seed nodes — vérifier que seed1/seed2.hidering.org pointent vers les nouveaux IPs~~ — **FAIT** 19 mai 2026 : `seed1` → OVH `135.125.243.137`, `seed2` → Contabo `207.180.211.96`.
9. ~~Block explorer — reset DB ou nouvelle instance~~ — **FAIT** 19 mai 2026 : nouvelle instance sur OVH (http://135.125.243.137:8080), remplace l'ancienne instance Flux `hrgexplorer.app.runonflux.io`.
10. Binaires publics v2.0.0 (Linux/Windows/Mac) — débloquer billing GH Actions sinon Linux-only via build local + `gh release create v2.0.0`
11. Pool mining — relink monero-pool contre les libs v2.0.0, pointer un daemon v2.0.0 sync mainnet
12. Site web public — mettre à jour docs/ (network ID, magic, version) pour redeploy Vercel auto
13. Announce hard fork (Twitter, Reddit, BitcoinTalk) — communiquer activation + abandon chaîne v1.x
14. Phase 4 Launch (public officiel)

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
7. ~~**Block explorer reset**~~ → **FAIT** 19 mai 2026 : nouvelle instance déployée sur OVH (http://135.125.243.137:8080), indexant la chaîne v2.0.0 depuis genesis. Ancienne instance Flux `hrgexplorer.app.runonflux.io` retirée.
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

## INFRASTRUCTURE VPS (19 Mai 2026 — REMPLACE Flux)
- **Décision 19 mai 2026 :** abandon de Flux pour les seed nodes et le block explorer. Containers éphémères + IPs variables (vu 3 IPs en une session sur `hideringseed1`) = mauvais fit pour des entry points DNS stables. Migration vers VPS dédiés avec IPs invariantes et systemd pour restart auto.
- **seed1.hidering.org → OVH `135.125.243.137`** (P2P 19740, RPC 19741) — daemon HRG v2.0.0 managé par `hideringd.service` (systemd, `Restart=on-failure`). Sert aussi le block explorer sur le port 8080.
- **seed2.hidering.org → Contabo `207.180.211.96`** (P2P 19740, RPC 19741) — daemon HRG v2.0.0 managé par systemd, redondance avec seed1.
- **Block explorer public : http://135.125.243.137:8080** — instance fraîche indexant la chaîne v2.0.0 depuis genesis. Remplace l'ancienne instance Flux `hrgexplorer.app.runonflux.io` (retirée).
- **DNS (records A) :** déjà à jour côté registrar (vérifié 19 mai 2026). Les binaires v2.0.0 publiés et `docs/index.html` pointent directement vers `seed1.hidering.org` / `seed2.hidering.org`.
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

## POST-QUANTIQUE (PHASE 5 — 2027)
Hard fork additif (n'altère pas la blockchain existante) :
- Dilithium3 (CRYSTALS) — signatures
- Kyber768 — échange de clés
- Calendrier : spec 2026 → impl+audit T1 2027 → hard fork mainnet T2 2027
