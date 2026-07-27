# REBRAND PLAN v2 — Unreal leg (`ReactiveUI-Unreal` → org `reactive-ui-toolkit`, repo `ruitk-unreal`)

**Status: PLANNED + FULLY RATIFIED — v2. Census re-verified 2026-07-28 against `dev` @
`8986188`** (v1's counts were measured on a drifted checkout; every anchor below is
re-pinned). **UE-Q1–UE-Q6 are ALL RESOLVED (owner, 2026-07-28; Q5/Q6 in §5). Blocked only
on the §2 gates. UE-Q1–UE-Q4:** modules → `Ruitk*`; host project → `RuitkUnrealDemo`; the
localization binaries get regenerated; versions bump one minor (plugin **0.15.0**, extensions
**0.9.0**). v2 also closes the gaps a full verification pass found in v1: five type-prefixes
that had no sweep rule, the 24 public `RUI_*` macros, ~180 brand-named source files, the 135
automation-spec names, and a dozen smaller sites — all enumerated below.

**Authority:** family rebrand ruling (owner, 2026-07-27/28) — umbrella **Reactive UI Toolkit**,
org **`reactive-ui-toolkit`**, FULL conversion ("a complete rename, nothing stays"), clean
break + codemod, Scheme C slugs, transfer FIRST / rename SECOND, extension marketplace
identity AND display names frozen. Siblings: Godot `plans/REBRAND_PLAN.md` v4, Unity
`Plans~/REBRAND_PLAN.md` v2 — each on `docs/rebrand-plan` in its repo.

Written for a **lesser-model executor**. §1 is binding.

---

## 1. Executor contract (binding — read first)

1. **Never free-lance a sweep.** Only the replacements this plan lists. A verification grep
   hit not accounted for by a step or §10 = **STOP and report**.
2. **Exact strings.** OLD not found exactly where stated → STOP (tree drifted; re-verify).
3. **The regexes in §7.D are normative** — do not widen or "improve" them.
4. **Order matters.** Groups in order; numbered rules inside a group in numbered order
   (longest-token-first prevents substring clobbering).
5. **Enumerate-then-map** where a rule says so; **blanket rules** (D5/D6) instead use the
   before/after token-count audit written into the rule. An unmapped token in an
   enumerate-then-map rule = STOP.
6. **Tier 3 frozen (by name):** `plans/archive/**`, `research/**`, `BENCH_BASELINES.md`,
   `CHANGELOG.md` + mirror `Plugins/ReactiveUI/CHANGELOG.md` bodies below the new 0.15.0
   section, `plans/DISCORD_CHANGELOG.md` old post bodies, git history. Live URLs inside them
   still update (§7.A). Everything else in `plans/` (MASTER_PLAN, REMAINING, ROADMAP,
   PENDING_CHANGELOG) is live and converts.
7. **[ENGINE] steps** need UE 5.6.1+ (see `CLAUDE.md` for the path convention) or the
   self-hosted CI leg. Without an engine: finish all non-engine steps, report the rest.
8. **Branch flow (house rule):** `rebrand/umbrella` off `dev`; push the branch ONLY; owner
   PRs → dev → checks → merge. No `Co-Authored-By` trailers.
9. **Clean tree required.** As of census the main checkout sat on another branch with a
   locally-modified `.uproject` (an `EngineAssociation` GUID — another workstream's state):
   never stage, stash, or discard it; work from a fresh worktree/clone of `dev`.

---

## 2. Gates

| Gate | What | Who |
|---|---|---|
| G1 | Org exists; repo transferred + renamed `ruitk-unreal` (issues/PRs/stars/secrets survive; git+web URLs redirect; **Pages URLs do NOT**; never reuse the freed name) | OWNER |
| G2 | Godot leg executed first | OWNER |
| G3 | Remaining open questions §5 answered | OWNER |
| G4 | Pages re-enabled on the transferred repo; `https://reactive-ui-toolkit.github.io/ruitk-unreal/` serving | OWNER + executor verify |
| G5 | CI secrets present post-transfer (marketplace PATs, self-hosted runner registration) | OWNER |
| G6 | **GitHub repo VARIABLE `RUI_CI_ENGINE_ARMED` renamed to `RUITK_CI_ENGINE_ARMED`** in repo settings (referenced as `vars.RUI_CI_ENGINE_ARMED` in `publish.yml:152` + `engine-tests.yml`; a text sweep alone would break arming) — dashboard action, coordinated with the D6 sweep landing | OWNER |

---

## 3. Name Registry (UE-N1 … UE-N24)

| # | Thing | OLD | NEW |
|---|---|---|---|
| UE-N1 | GitHub owner/org | `yanivkalfa` | `reactive-ui-toolkit` |
| UE-N2 | Repo slug | `ReactiveUI-Unreal` | `ruitk-unreal` |
| UE-N3 | Product display name | "ReactiveUI for Unreal" | "Reactive UI Toolkit for Unreal" |
| UE-N4 | Plugin folder + `.uplugin` | `Plugins/ReactiveUI/`, `ReactiveUI.uplugin` | `Plugins/ReactiveUIToolkit/`, `ReactiveUIToolkit.uplugin` (folder ↔ uplugin filename must stay paired) |
| UE-N5 | Plugin name in `.uproject` | `"Name": "ReactiveUI"` (line 20 — the SOLE site, verified) | `"Name": "ReactiveUIToolkit"` |
| UE-N6 | The 8 plugin modules **(UE-Q1 RATIFIED)** | `ReactiveUICore, ReactiveUISlate, ReactiveUIUMG, ReactiveUICommonUI, ReactiveUIMVVMBridge, ReactiveUIInterp, ReactiveUIToolchain, ReactiveUIEditor` | `RuitkCore, RuitkSlate, RuitkUMG, RuitkCommonUI, RuitkMVVMBridge, RuitkInterp, RuitkToolchain, RuitkEditor` |
| UE-N7 | Module export macros (UBT-derived; **9**, not 8 — v1 missed the game module's) | `REACTIVEUISLATE_API` 206 · `REACTIVEUICORE_API` 95 · `REACTIVEUIINTERP_API` 24 · `REACTIVEUIUMG_API` 20 · `REACTIVEUITOOLCHAIN_API` 16 · `REACTIVEUICOMMONUI_API` 11 · `REACTIVEUIMVVMBRIDGE_API` 3 · `REACTIVEUIEDITOR_API` 1 · **`RUIDEMO_API` 44** = **420** (errata: the 8 plugin macros sum to 376; `RUIDEMO_API` is on top) | `RUITKSLATE_API, RUITKCORE_API, RUITKINTERP_API, RUITKUMG_API, RUITKTOOLCHAIN_API, RUITKCOMMONUI_API, RUITKMVVMBRIDGE_API, RUITKEDITOR_API, RUITKDEMO_API` |
| UE-N8 | Type prefixes — **all five UE letters, not just F/U** | `FRui*` 10,866 (classifier `FRuiNode` 2,313) · `URui*` 387 · `SRui*` 316 (17 distinct) · `TRui*` 220 (17) · `IRui*` 181 (5) · `ERui*` 406 (9) · `ARui*` 5 (1) | `FRuitk*/URuitk*/SRuitk*/TRuitk*/IRuitk*/ERuitk*/ARuitk*` |
| UE-N9 | Bare `Rui*` identifiers (incl. generated-symbol prefixes `RuiPriv_*`, `RuiUetkx_*`, and 108 source FILENAMES) | 185 distinct tokens / 2,266 occ | `Ruitk*` (blanket rule D5) |
| UE-N10 | C++ namespace | `RUI::` 2,419 · `namespace RUI` | `Ruitk::` / `namespace Ruitk` |
| UE-N11 | **Public preprocessor macros** (v1 missed all of them) | 24 distinct `RUI_*` / 1,010 occ — user-facing API: `RUI_PROP` 231, `RUI_EQ` 215, `RUI_COMPONENT` 110, `RUI_UETKX_DECL_PHASE` 87 (**emitted by codegen** into `*.Uetkx.gen.cpp`), `RUI_ROW` 84, `RUI_PROPS_BODY` 71, `RUI_PROP_EVENT` 62, + 17 test-flag/log macros | `RUITK_*` (rule D6; codegen emit site changes in lockstep; breaking → codemod + MIGRATION) |
| UE-N12 | Commandlets (`-run=` names derive from class names) | `-run=RUICompile/RUIContractDump/RUIExportSchema/RUIMigrateEsModules/RUIMigrateImports`; classes `URUI*Commandlet` in 4 headers (`RUIMigrateImportsCommandlet.h` holds two classes) | `-run=Ruitk*`; classes `URuitk*Commandlet`; the commandlet FILES rename too (§7.C9) |
| UE-N13 | Automation spec names (v1 missed) | **135** `IMPLEMENT_*_AUTOMATION_TEST` specs `"ReactiveUI.<Suite>.<Test>"` across 54 test files; CI filter `-ExecCmds="Automation RunTests ReactiveUI; Quit"` (`engine-tests.yml:67`); suite size 132 entries | `"Ruitk.<Suite>.<Test>"`; CI filter `Automation RunTests Ruitk`; skills/docs updated |
| UE-N14 | Host project **(UE-Q2 RATIFIED)** | `ReactiveUIUnrealDemo.uproject` + 3 `Target.cs`; `Source/RuiDemo` (202 refs) + `Source/RuiHostTests` (47) | `RuitkUnrealDemo.uproject`; `Source/RuitkDemo`, `Source/RuitkHostTests` |
| UE-N15 | `ReactiveUetkx*` editor types/files (v1 missed — no rule matched them) | 6 files in `ReactiveUIEditor/Private/`: `ReactiveUetkxCommands.{h,cpp}`, `ReactiveUetkxEditorSettings.h`, `ReactiveUetkxMenu.{h,cpp}`, `SReactiveUetkxHmrPanel.{h,cpp}` + their type names | `RuitkUetkx*` / `SRuitkUetkxHmrPanel` |
| UE-N16 | Seller-repo marker | `.rui-seller-repo` (root file; read by `FUetkxCodegen::IsSellerRepo` to pick the generated-file copyright banner) | `.ruitk-seller-repo` + the C++ constant in lockstep |
| UE-N17 | Docs site folder + base path | `ReactiveUIUnrealDocs~/`; `vite.config.ts:69` `base: '/ReactiveUI-Unreal/'` | `RuitkUnrealDocs~/`; `base: '/ruitk-unreal/'` (router basename DERIVES from `BASE_URL` — `main.tsx:9`; only the comment on `main.tsx:7` mentions the old path) |
| UE-N18 | Extension identities | vscode `uetkx` / publisher `ReactiveUITK` / "UETKX (Unreal - VS Code)" / 0.8.0; vsix `UetkxVsix.ReactiveUITK` / `Publisher="Yaniv Kalfa"` / "UETKX (Unreal - VS2022)"; lsp `uetkx-language-server` 0.8.0 | **identity + display UNCHANGED**; versions → 0.9.0; content underneath converts |
| UE-N19 | Release asset zips | `ReactiveUI-<ver>.zip`, `ReactiveUI-<ver>-UE<eng>.zip` | `ReactiveUIToolkit-<ver>.zip`, `-UE<eng>.zip` |
| UE-N20 | Wave versions **(UE-Q4 RATIFIED)** | plugin `0.14.0` / `"Version": 15`; extensions+LSP `0.8.0` | plugin **0.15.0** / `"Version": 16` (BREAKING); extensions+LSP **0.9.0** |
| UE-N21 | Migration + codemod | (model: record-driven `URuitkMigrateEsModulesCommandlet`) | `MIGRATION-0.15.md` + `URuitkMigrateBrandCommandlet` (`-run=RuitkMigrateBrand`) |
| UE-N22 | Fab listing | none live (`"MarketplaceURL": ""`) | created fresh post-rename — zero migration |
| UE-N23 | Localization set **(UE-Q3 RATIFIED: regenerate)** | `Content/Localization/RuiDemo/{RuiDemo.locmeta, RuiDemo.manifest, en/RuiDemo.locres, en/RuiDemo.archive}` + `Config/Localization/RuiDemo_Gather.ini` + `DefaultGame.ini` `+LocalizationPaths=…/RuiDemo` | `…/RuitkDemo/` set, target renamed, regenerated via the Localization Dashboard `[ENGINE]` |
| UE-N24 | Demo project display strings (v1 missed) | `.uproject` `"Description": "ReactiveUI for Unreal — demo host project…"`; `DefaultGame.ini` `ProjectName=ReactiveUI Unreal Demo` + `Description=…ReactiveUI for Unreal.` | UE-N3 phrasing (`ProjectName=Reactive UI Toolkit Unreal Demo`) |

**Naming coherence:** slug `ruitk-unreal` ↔ prefixes `FRuitk*/…/ARuitk*` ↔ `namespace Ruitk` ↔
modules `Ruitk*` ↔ macros `RUITK*` — one abbreviation system; the plugin FOLDER uses full words
(`ReactiveUIToolkit`) exactly like Godot's addon folder, because folders are the install artifact.

---

## 4. Census (v2, measured 2026-07-28 @ `dev` = `8986188`)

§7.0 re-runs these and STOPs on drift:

```bash
git grep -ohI "\bFRui[A-Za-z_]*" | wc -l    # 10866
git grep -oI  "RUI::" | wc -l               #  2419
git grep -ohI "\bURui[A-Za-z_]*" | wc -l    #   387   (+1 binary match line from RuiDemo.locres)
git grep -ohI "\bSRui[A-Za-z_]*" | wc -l    #   316
git grep -ohI "\bTRui[A-Za-z_]*" | wc -l    #   220
git grep -ohI "\bIRui[A-Za-z_]*" | wc -l    #   181
git grep -ohI "\bERui[A-Za-z_]*" | wc -l    #   406
git grep -ohI "\bRUI_[A-Z_]*" | wc -l       #  1010   (24 distinct)
git grep -ohI "REACTIVEUI[A-Z]*_API\|RUIDEMO_API" | wc -l   # 420 (9 distinct: 376 across the 8 REACTIVEUI* + RUIDEMO_API 44)
git grep -ohI "FRuiNode" | wc -l            #  2313
git grep -cI  "Ruitk" | wc -l               #     0   ← collision check
```

Other pinned facts: `FRuiNode` split — `Source/` 1,543 · `Plugins/` 539 · `ide-extensions/`
148 · docs 36 · plans 31 · rest ≤5 each. `RuiDemo` 202 / `RuiHostTests` 47. Bare-`Rui` blanket:
185 distinct tokens / 2,266 occ / **108 brand-named source files**. Bare-word `ReactiveUI`
appears in **206 files** (dominated by the 135 automation specs + 54 test filenames + module
files). `yanivkalfa`: 18 files / 34 occ (per-file figures in §7.A are LINE counts;
`LicensingPage.tsx` has 6 occ on 4 lines). Committed generated: 45 `*.uetkx.inl` + 2
`Uetkx.gen.cpp` (`Source/RuiDemo/Private/RuiDemo.Uetkx.gen.cpp`,
`Source/RuiHostTests/Private/RuiHostTests.Uetkx.gen.cpp`). CoreRedirects: none exist today.
No `.uasset`/`.umap` tracked at all → the D4 redirects serve USER projects only.
Suite baseline: **132 automation-suite entries; last recorded run 129/132 with 3 known
owner-in-flight failures (`plans/ROADMAP.md:13`); 132/132 was green on UE 5.7 on 2026-07-25**.

**Family corpus (STOP-gate fact):** `ide-extensions/lsp-server/test-fixtures/uetkx-scanner-cases.json`
— familyCore = `[skipNoncodeMarkup, findMatchingMarkup, fileScan]` holds **ZERO** brand tokens
(`FRui/URui/SRui/IRui/TRui/ERui/RUI/ReactiveUI/Rui`); `FRuiNode` ×15 sit entirely in perLeg
`fileScanLeg`. Family hash pinned: `plans/family-corpus.hash` =
`71a37c75b16a1666c8ce20eae9dbddc50c6e7f583cc5b9e9e287bc14f3a6b069` — must be byte-identical
after the wave. **Caveat:** `nonBmp` cases route by a per-case `section` field and CAN land in
familyCore sections — the sweep exclusion in §7.D therefore covers the `nonBmp` block too.

---

## 5. Open questions — ALL RESOLVED (owner, 2026-07-28)

- **UE-Q5 — RESOLVED, and OVERRULED into a family ruling: the license itself renames +
  version-bumps, in all three repos.** `ReactiveUI Community License 1.0` →
  **`Reactive UI Toolkit Community License 1.1`**: title + internal product references +
  credit-line clause (`Made with Reactive UI Toolkit`) + version refs update; legal terms
  otherwise unchanged; copyright holder stays; licensees under 1.0 keep 1.0. For THIS repo:
  the five license copies (§7.B.2) get the full retitle, and the SPDX ref in
  `ide-extensions/lsp-server/package.json:5` (+ lockfile) becomes
  **`LicenseRef-Reactive-UI-Toolkit-Community-1.1`** (valid SPDX idstring:
  letters/digits/`.`/`-` only).
- **UE-Q6 — RESOLVED: ADDITIVE.** Keep the existing vsix `<Tags>` / vscode `keywords`
  tokens AND add the umbrella-name tokens (`Reactive UI Toolkit`) — old search terms keep
  finding the extensions through the transition.

---

## 6. Phases

| Phase | What | Who |
|---|---|---|
| 0 | Preflight §7.0 | executor |
| 1 | Org transfer + rename + Pages + G6 variable | OWNER |
| 2 | Branch `rebrand/umbrella` off post-transfer `dev` | executor |
| 3 | Groups A–I (§7, in order) | executor |
| 4 | Battery §8 | executor + `[ENGINE]` |
| 5 | Release wave §9 | executor prepares, OWNER merges + tags |

## 7. Phase 3 — the work

### 7.0 Preflight
Clean tree (contract rule 9) → run every §4 anchor, STOP on drift → record
`node scripts/corpus-hash.mjs` output as the §7.F baseline.

### 7.A Group A — URL swap (first after transfer; Tier-3 touched for URLs only)

| OLD | NEW |
|---|---|
| `https://github.com/yanivkalfa/ReactiveUI-Unreal` | `https://github.com/reactive-ui-toolkit/ruitk-unreal` |
| `https://github.com/yanivkalfa` (profile form, `CreatedByURL`) | `https://github.com/reactive-ui-toolkit` |
| `https://yanivkalfa.github.io/ReactiveUI-Unreal/` | `https://reactive-ui-toolkit.github.io/ruitk-unreal/` |

The 18 files (occurrence counts; some lines carry two): `plans/DISCORD_CHANGELOG.md`,
`ReactiveUIUnrealDocs~/src/pages/Licensing/LicensingPage.tsx` (6 occ / 4 lines),
`Plugins/ReactiveUI/README.md`, `README.md`, `Plugins/ReactiveUI/ReactiveUI.uplugin`
(`CreatedByURL`/`DocsURL`/`SupportURL`), `ide-extensions/vscode-uetkx/package.json`,
`research/round2-implementation/godot-ecosystem.md`, 3× `plans/archive/*`,
`ide-extensions/vscode-uetkx/{readme-template.md, README.md}`,
`ide-extensions/visual-studio/UetkxVsix/{source.extension.vsixmanifest, publishManifest.json, overview-template.md}`,
`ReactiveUIUnrealDocs~/src/links.ts`, `…/components/TopBar/TopBar.tsx`, `LICENSE-COMMERCIAL.md`.
Docs base: `ReactiveUIUnrealDocs~/vite.config.ts` line 69 `base: '/ReactiveUI-Unreal/',` →
`base: '/ruitk-unreal/',` + the comment on lines 67–68 AND `src/main.tsx:7` (comment only —
the basename itself derives from `BASE_URL`, no functional edit).
`git remote set-url origin https://github.com/reactive-ui-toolkit/ruitk-unreal.git`.
**Verify:** remaining `yanivkalfa` hits are person-attributions only (`CreatedBy`, vsix
`Publisher`, license copyright, mailtos).

### 7.B Group B — display strings + licenses

1. uplugin `"FriendlyName": "ReactiveUI for Unreal"` → `"Reactive UI Toolkit for Unreal"`;
   `Description` verified brand-free — don't edit.
2. Licenses — **the full license-1.1 rewrite (UE-Q5 family ruling):** the files are
   `LICENSE` (no extension), NOT `LICENSE.md`:
   `git grep -n "ReactiveUI" LICENSE LICENSE-COMMERCIAL.md Plugins/ReactiveUI/LICENSE ide-extensions/vscode-uetkx/LICENSE ide-extensions/visual-studio/UetkxVsix/LICENSE.txt`
   (8+6+8+8+8 hits) → retitle to `Reactive UI Toolkit Community License 1.1`, version refs
   `1.0` → `1.1`, product references + credit-line clause updated, terms otherwise
   unchanged; keep all copies byte-identical to the root `LICENSE`; matching labels in
   `LICENSE-COMMERCIAL.md`. Ambiguous hit → STOP-list.
3. `README.md`, `Plugins/ReactiveUI/README.md`, docs-site titles/nav, `templates/fab-listing.template.md` (2) → UE-N3.
4. `CLA.md` (3), `THIRD_PARTY_NOTICES.md` (1), `VERSIONING.md` (3), `.github/PULL_REQUEST_TEMPLATE.md`,
   `.github/ISSUE_TEMPLATE/bug_report.yml` (2), `.github/dependabot.yml` comment,
   `.vscode/launch.json`, `.gitattributes`/`.gitignore` comments — product-name prose → UE-N3.
5. UE-N24: uproject `Description`; `Config/DefaultGame.ini` `ProjectName=Reactive UI Toolkit Unreal Demo`
   + its `Description`; `Config/DefaultEditorPerProjectUserSettings.ini:1` comment.
   (`CompanyName`/`CopyrightNotice` = person attribution — stays.)
6. `publish.yml:142` `--title "ReactiveUI for Unreal …"` → UE-N3 (the zip NAMES are C7's).
7. Live operator docs (`CLAUDE.md`, `.claude/skills/*`, `plans/MASTER_PLAN.md`, `plans/REMAINING.md`,
   `plans/ROADMAP.md`, `plans/PENDING_CHANGELOG.md`): display-name prose here; identifiers
   change via C/D sweeps.

### 7.C Group C — modules, folders, files, project (ordered!)

C1. Module token sweep, whole-word, LONGEST FIRST (same 8-row table, ratified `Ruitk*` targets):
    `ReactiveUIMVVMBridge→RuitkMVVMBridge`, `ReactiveUICommonUI→RuitkCommonUI`,
    `ReactiveUIToolchain→RuitkToolchain`, `ReactiveUIEditor→RuitkEditor`,
    `ReactiveUIInterp→RuitkInterp`, `ReactiveUISlate→RuitkSlate`, `ReactiveUICore→RuitkCore`,
    `ReactiveUIUMG→RuitkUMG`.
C2. Export-macro sweep — **9 pairs** (UE-N7), incl. `RUIDEMO_API → RUITKDEMO_API`. 420 total.
C3. Module dirs + Build.cs + **module implementation files** (v1 missed the third):
    ```bash
    for m in Core Slate UMG CommonUI MVVMBridge Interp Toolchain Editor; do
      git mv "Plugins/ReactiveUI/Source/ReactiveUI$m" "Plugins/ReactiveUI/Source/Ruitk$m"
      git mv "Plugins/ReactiveUI/Source/Ruitk$m/ReactiveUI$m.Build.cs" "…/Ruitk$m.Build.cs"
      # module impl: Private/ReactiveUI<m>Module.cpp → Private/Ruitk<m>Module.cpp (8 files)
    done
    ```
C4. Uplugin file, then plugin folder:
    `git mv Plugins/ReactiveUI/ReactiveUI.uplugin Plugins/ReactiveUI/ReactiveUIToolkit.uplugin`
    then `git mv Plugins/ReactiveUI Plugins/ReactiveUIToolkit`.
    `Plugins/ReactiveUIToolkit/Config/FilterPlugin.ini` is brand-free — verify it survived the move.
C5. Path-string sweep — BOTH separators (v1 missed the backslash form):
    `Plugins/ReactiveUI/` AND `Plugins\ReactiveUI\` → Toolkit forms. Sites: workflows,
    `scripts/package-plugin.ps1:32` (`Join-Path … 'Plugins\ReactiveUI\ReactiveUI.uplugin'`),
    `scripts/{check-headers,verify-mirror,docs-drift,check-style-builders,bump}.mjs` hardcoded
    paths (`check-style-builders.mjs` also hard-codes `RuiStyle.h` + `RUI::Style()`/`RUI::Slot()`
    — D-rules rewrite the tokens; verify the script still resolves post-D5 filenames),
    `.claude/skills/`, docs.
C6. Plugin name refs (UE-N5): the uproject line 20 — sole site, verified.
C7. Release assets (UE-N19) — the FULL `publish.yml` line set: 104 (release-body printf),
    112 (zip create), **113** (`-x 'ReactiveUI/Binaries/*' …` exclusions), **114** + **130**
    (echo lines), 122–128 (stage-dir cp/zip loop), **144** (`dist/ReactiveUI-*.zip` glob), plus
    `scripts/package-plugin.ps1:67` (`dist/ReactiveUI-$versionName-UE$shortVer.zip`).
    (Line 156 is a brand-free sync comment — v1 pointed there in error; nothing to edit.)
C8. Host project (UE-Q2 RATIFIED — unconditional):
    ```bash
    git mv Source/RuiDemo Source/RuitkDemo
    git mv Source/RuiHostTests Source/RuitkHostTests
    git mv ReactiveUIUnrealDemo.uproject RuitkUnrealDemo.uproject
    for t in "" Editor Server; do git mv "Source/ReactiveUIUnrealDemo$t.Target.cs" "Source/RuitkUnrealDemo$t.Target.cs"; done
    ```
    + whole-word `ReactiveUIUnrealDemo` → `RuitkUnrealDemo` (Target classes/`TargetName`, CI
    command lines, skills). **`Config/DefaultEngine.ini:17`
    `GlobalDefaultGameMode=/Script/RuiDemo.RuiDemoGameMode` →
    `/Script/RuitkDemo.RuitkDemoGameMode`** (v1 missed — the game breaks at boot without it).
    Localization set per UE-N23: `git mv` the `Content/Localization/RuiDemo` tree +
    `Config/Localization/RuiDemo_Gather.ini` (fix its internal `Plugins/…` + `ReactiveUIEditor/*`
    paths too) + `DefaultGame.ini` `+LocalizationPaths`; regenerate binaries in §8 `[ENGINE]`.
C9. **File renames the D-rules imply (v1 had no step):** driven by
    `git ls-files | grep -E "(^|/)(ReactiveUI|Rui|SRui|RUI)[A-Za-z_]*\.(h|cpp|cs|uetkx|inl)"`,
    `git mv` each per the SAME mapping as its token rule: 108 `Rui*/SRui*` plugin+demo files
    (`RuiNode.h → RuitkNode.h`, `SRuiCanvas.cpp → SRuitkCanvas.cpp`, …), 54 test files
    (`ReactiveUIAcceptanceTest.cpp → RuitkAcceptanceTest.cpp`, …), the 4 commandlet headers +
    cpps (`RUICompileCommandlet.h → RuitkCompileCommandlet.h`, …; `RUIMigrateImportsCommandlet.h`
    holds two classes — one file, one rename), the 6 `ReactiveUetkx*` files (UE-N15 →
    `RuitkUetkx*`), the 2 `*.Uetkx.gen.cpp` (follow their module dirs). `#include` strings are
    rewritten by the token sweeps — filenames and includes converge; the §8 build proves it.
C10. UE-N16: `git mv .rui-seller-repo .ruitk-seller-repo` + the `IsSellerRepo` constant in
    the Toolchain codegen (locate: `git grep -n "rui-seller-repo"`).
C11. Docs folder (UE-N17): `git mv ReactiveUIUnrealDocs~ RuitkUnrealDocs~` + its **18 external
    references** (zero self-references inside, verified): `.github/dependabot.yml:7`,
    `publish.yml:29,37,53`, `test.yml:98,110`, `.gitignore:33`, `CLAUDE.md:43,83`, `README.md:92`,
    `.claude/skills/engine-catchup/SKILL.md:58`, `scripts/check-headers.mjs:14`,
    `scripts/docs-drift.mjs` ×7, `templates/hook_doc.template.tsx:4,5` (+ live `plans/` mentions).

### 7.D Group D — the identifier sweep

Repo-wide over tracked text files, EXCLUDING: binaries, the §1.6 Tier-3 set, and — for
`uetkx-scanner-cases.json` — the three familyCore sections AND the `nonBmp` block (§4 caveat).
The 15 perLeg `fileScanLeg` `FRuiNode` tokens DO convert (lockstep with the scanner).

| # | Rule | Covers | Census |
|---|---|---|---|
| D1 | `\bFRui` → `FRuitk` | all `FRui*` incl. `FRuiNode→FRuitkNode` | 10,866 |
| D2 | `\bURui` → `URuitk` · `\bSRui` → `SRuitk` · `\bTRui` → `TRuitk` · `\bIRui` → `IRuitk` · `\bERui` → `ERuitk` · `\bARui` → `ARuitk` (six sub-rules — v1 had only URui) | all reflected + Slate + template + interface + enum + actor types | 387+316+220+181+406+5 |
| D3 | `\bRUI::` → `Ruitk::` · `\bnamespace RUI\b` → `namespace Ruitk` · `using namespace RUI\b` → same | the namespace | 2,419 |
| D4 | **CoreRedirects (ADD):** in the DEMO's `Config/DefaultEngine.ini`, new `[CoreRedirects]` block for the UHT-reflected renames — the verified inventory is **8 `URui*` classes + `ARuiDemoGameMode`** (NOT 12; `URuiLogic`/`URuiMarkupAsset`/`URuiViewModelBridge` are research-only prose, `URuiSubsystem` is superseded prose): `URuiHostWidget`/`URuiSignalViewModel`/`URuiWorldSubsystem` in **`ReactiveUIUMG`** (v1's worked example wrongly said Slate), `URuiActivatableScreen` in `ReactiveUICommonUI`, `URuiMvvmViewModel` in `ReactiveUIMVVMBridge`, `URuiTestViewModel`/`URuiTestUserWidget`/`URuiTestMvvmSubViewModel` in `RuiHostTests`, `ARuiDemoGameMode` in `RuiDemo`. Form: `+ClassRedirects=(OldName="/Script/ReactiveUIUMG.RuiHostWidget",NewName="/Script/RuitkUMG.RuitkHostWidget")`. **No reflected `FRui*` USTRUCTs / `ERui*` UENUMs exist** → no Struct/EnumRedirects. No tracked assets in-repo → the block serves USER projects; ship it verbatim in MIGRATION-0.15.md. | user-project load-fix | 9 types |
| D5 | **Blanket:** `\bRui(?!tk)` → `Ruitk`. 185 distinct tokens is too many for a map table; instead AUDIT: before — `git grep -ohI "\bRui[A-Za-z_]*" \| sort -u \| wc -l` (expect 185); after — the same count must appear as `Ruitk*` tokens and `git grep -ohI "\bRui(?\!tk)"` → 0. Sample-verify 10 random tokens converted sanely (`RuiPriv_*→RuitkPriv_*`, `RuiUetkx_*→RuitkUetkx_*`). | RuiNode/RuiRouter/RuiPriv_/RuiUetkx_/RuiDemo/RuiHostTests/… | 2,266 |
| D6 | **Prefix by case-class (v1's regex and prose contradicted; this is normative):** PascalCase `\bRUI(?=[A-Z][a-z])` → `Ruitk` (the commandlet family: `RUICompile→RuitkCompile`, `URUICompileCommandlet→URuitkCompileCommandlet`, …); ALL-CAPS `\bRUI(?=[A-Z_])` incl. `\bRUI_` → `RUITK` (`RUI_PROP→RUITK_PROP`, `RUIDEMO→RUITKDEMO` — already via C2, `RUIBENCH→RUITKBENCH`, `RUISelfTest→RuitkSelfTest` is PascalCase → first rule). Enumerate first: `git grep -ohI "\bRUI[A-Za-z_]\+" \| sort -u` — 29 letters-only + 24 underscore tokens expected; anything new → STOP. The codegen's emit of `RUI_UETKX_DECL_PHASE` changes in the SAME commit as the `.gen.cpp` sweep (else §7.H's byte-stable gate fails). `RUI_CI_ENGINE_ARMED` → `RUITK_CI_ENGINE_ARMED` in yml, coordinated with gate G6. | commandlets + macros + log prefixes | 473 + 1,010 |
| D7 | UE-N13 automation specs: `"ReactiveUI.` → `"Ruitk.` inside the 135 `IMPLEMENT_*_AUTOMATION_TEST` macro lines; `engine-tests.yml:67` filter `Automation RunTests ReactiveUI` → `… Ruitk`; the `test-run` skill's suite map + incantations. Verify: `git grep -c "IMPLEMENT.*AUTOMATION_TEST.*\"Ruitk\."` → 135. | the test suite's identity | 135 specs |
| D8 | `-run=RUI` → 0 remaining (D6 covered it) — verify across the **44 files** that carry it (v1 said 8): skills ×6, `engine-tests.yml`, `CLAUDE.md`, `README.md`, CHANGELOG mirror pair (new-section only), 6 docs pages, 3 LSP sources (quick-fix text), 4 plugin C++ files, commandlet headers, and the golden fixture `Source/RuitkHostTests/ContractFixtures/ImportError.uetkx.diags.expected` (regenerated in §7.F — hand-edit only if regen is unavailable). | CI + docs + quick-fixes | 44 files |

After D: `git grep -nI "\bFRui[A-Z]\|\bURui[A-Z]\|\bSRui[A-Z]\|\bTRui[A-Z]\|\bIRui[A-Z]\|\bERui[A-Z]\|RUI::\|\bRUI_" -- ':!plans/archive' ':!research' ':!BENCH_BASELINES.md'`
→ 0 outside §10.

### 7.E Group E — ide-extensions (identity frozen, content converts)

1. Frozen lines (byte-identical at the end): vscode `package.json` `name`/`publisher`/`displayName`;
   vsix `Identity Id`/`Publisher`/`DisplayName`; lsp `package.json` `name`. Everything else in
   `ide-extensions/` converts via A/C/D (incl. `formatUetkx.ts:570,573`, grammar, fixtures —
   the 15 `fileScanLeg` tokens land at `FRuitkNode`, verify 15/0 old).
2. UE-Q6 (ADDITIVE, resolved): keep existing vsix `<Tags>` + vscode `keywords`, ADD the
   umbrella-name tokens.
3. UE-Q5 (resolved): SPDX ref in lsp `package.json:5` + lockfile →
   `LicenseRef-Reactive-UI-Toolkit-Community-1.1`.
4. Rebuild + tests in §8.

### 7.F Group F — contracts, schema, goldens, corpus `[ENGINE]`

1. `-run=RuitkContractDump` (no `--check`) regenerates goldens — incl. the
   `ImportError.uetkx.diags.expected` fixture from D8; then `--check` green.
2. `-run=RuitkExportSchema` → refresh `ide-extensions/lsp-server/src/uetkx-schema.json`
   (exact path — v1 gave a bare filename).
3. **Corpus STOP-gate:** `node scripts/corpus-hash.mjs` == §7.0 baseline
   (`71a37c75…b069`), byte-identical. Drift = a sweep leaked into familyCore or `nonBmp` →
   revert that hunk; never re-pin.
4. Run all `scripts/*.mjs` checkers; fix path constants C5/C11 missed (STOP if logic breaks).
5. `engine-tests.yml:53`: fix the stale "25 committed generated files" comment → 47.

### 7.G Group G — user codemod + migration doc

1. `URuitkMigrateBrandCommandlet` (`-run=RuitkMigrateBrand`) in RuitkToolchain, modeled on
   the record-driven `URuitkMigrateEsModulesCommandlet`. Rewrites a USER project with the SAME
   rule set as §7.C/D: `.uetkx` heads + embedded C++ (`FRuiNode`, all seven prefixes, `RUI::`,
   **the `RUI_*` macros** — v1's codemod spec missed them and the `E/T/I/S/A` prefixes),
   `Build.cs` module deps (C1 table), `#include` paths, `.uproject` plugin name. Idempotent.
2. `MIGRATION-0.15.md`: delete old `Plugins/ReactiveUI` → install `Plugins/ReactiveUIToolkit`
   → run the codemod → paste the D4 CoreRedirects block → resave Blueprints referencing
   `URui*` types → `-run=RuitkCompile -check` exits 0. Include the C1 module table, the
   macro table (`RUI_PROP→RUITK_PROP` …), and the D-rules verbatim.
3. `CHANGELOG.md` (+ mirror): new `## 0.15.0` BREAKING section; old bodies frozen.

### 7.H Group H — committed generated files `[ENGINE]`

The 45 `.uetkx.inl` + 2 `Uetkx.gen.cpp` were swept as text; prove the compiler agrees:
`-run=RuitkCompile` over the demo tree → `git status --short` empty (byte-stable). A diff means
the codegen's emitted strings (banner via UE-N16, `RUITK_UETKX_DECL_PHASE`, `RuitkUetkx_*`
symbol prefix) weren't updated in lockstep — fix the emitter, commit the REGENERATED form,
STOP-report the diff summary. Regenerate the UE-N23 localization set here too.

### 7.I Group I — expected-leftovers audit
Run §10; any unexplained hit = STOP.

## 8. Phase 4 — battery

Non-engine:
```bash
cd ide-extensions/lsp-server && npm ci && npm run build && node --test out/test/*.test.js && node scripts/smoke.js
cd ide-extensions/vscode-uetkx && npm ci && npm run build
cd RuitkUnrealDocs~ && npm ci && npm run build && npm run lint      # renamed folder
node scripts/corpus-hash.mjs      # == baseline
node scripts/check-headers.mjs && node scripts/verify-mirror.mjs && node scripts/docs-drift.mjs && node scripts/check-style-builders.mjs
```
`[ENGINE]`:
```
UnrealEditor-Cmd RuitkUnrealDemo.uproject -run=RuitkCompile -check          # exit 0
UnrealEditor-Cmd RuitkUnrealDemo.uproject -run=RuitkContractDump --check
Automation suite: -ExecCmds="Automation RunTests Ruitk; Quit" — green vs the recorded baseline
  (132 entries; 129/132 with 3 known owner-side in-flight failures per plans/ROADMAP.md:13 —
  the SAME 3 may fail; any NEW failure is a rebrand regression)
Editor boot: gallery opens; log shows the CoreRedirects block parsed with no warnings
Localization Dashboard: regenerate the RuitkDemo target (UE-N23)
```
VS2022 vsix build per its CI job (Windows/msbuild-gated).

## 9. Phase 5 — release wave (owner merges first)

1. `Plugins/ReactiveUIToolkit/ReactiveUIToolkit.uplugin`: `"Version": 15 → 16`,
   `"VersionName": "0.14.0" → "0.15.0"` (`scripts/bump.mjs` — inspect before use; it
   hard-codes the uplugin path, fixed in C5).
2. Extensions + LSP → `0.9.0` (vscode + vsix + lsp-server package.json).
3. `plans/PENDING_CHANGELOG.md` → fold into CHANGELOG 0.15.0; new Discord post (old frozen).
4. Owner: PR → dev → checks → merge → ff master → tag `v0.15.0` (zips now
   `ReactiveUIToolkit-0.15.0*.zip`), then `vscode-v0.9.0` / `vs2022-v0.9.0`.
5. Fab (UE-N22): first listing under the new name; fill `MarketplaceURL` post-listing.

## 10. Expected leftovers (the ONLY permitted survivors)

| Grep | Allowed |
|---|---|
| `ReactiveUI` / `FRui\|URui\|SRui\|TRui\|IRui\|ERui\|ARui\|RUI::\|RUI_` | `plans/archive/**` · `research/**` · `BENCH_BASELINES.md` (incl. the old `RUIBENCH` log prefix it documents) · CHANGELOG bodies below 0.15.0 (+ mirror) · `plans/DISCORD_CHANGELOG.md` old posts · MIGRATION/codemod OLD columns |
| `ReactiveUITK` | the frozen vscode `publisher` field (family marketplace identity) |
| `yanivkalfa` / `Yaniv Kalfa` | person attributions: uplugin `CreatedBy`, vsix `Publisher`, license copyright, mailtos — people keep their names; only URLs moved |
| `ReactiveUI-Unreal` | Tier-3 prose; NOWHERE as a live URL |
| old tokens in `RuiDemo.locres` etc. | none — UE-Q3 ratified regeneration replaces the set |

## 11. Rollback

Pre-merge: delete `rebrand/umbrella`, re-clone. Post-merge: revert the merge on dev; org
transfer stays (redirects hold; never reuse freed names). CoreRedirects are additive and safe
to leave. G6's variable rename reverts in the dashboard if the wave reverts.
