# REBRAND PLAN — Unreal leg (`ReactiveUI-Unreal` → org `reactive-ui-toolkit`, repo `ruitk-unreal`)

**Status:** PLANNED — census measured 2026-07-27. Execution blocked on the family gates in §2.
**Authority:** the family rebrand ruling (owner, 2026-07-27): everything moves under the umbrella
**Reactive UI Toolkit**, GitHub org **`reactive-ui-toolkit`**, and it is a **FULL conversion** —
"if we do a rename its a complete rename, nothing stays". Clean break + codemod, no compat window.
Repo slugs are **Scheme C** (`ruitk-<engine>`): this repo becomes **`ruitk-unreal`**.
Sequencing (ratified): **org transfer FIRST, in-repo rename SECOND.**
**Sibling plans:** Godot `plans/REBRAND_PLAN.md` (the reference format, branch `docs/rebrand-plan`
in ReactiveUI-Gadot); Unity `Plans~/REBRAND_PLAN.md` (branch `docs/rebrand-plan` in the Unity repo).

This document is written to be executed by a **lesser model**. Every step names the exact file,
the exact OLD string, the exact NEW string, and a verification command. The executor contract
in §1 is binding.

---

## 1. Executor contract (binding — read first)

1. **Never free-lance a sweep.** Only perform the replacements this plan lists. If a grep during
   verification surfaces an occurrence this plan does not account for (not in a step, not in the
   expected-leftovers table §10), **STOP and report** — do not "helpfully" convert it.
2. **Exact-string edits.** Each step gives OLD → NEW. If OLD is not found exactly where stated,
   **STOP** — the tree has drifted from the census and the plan needs re-verification.
3. **Whole-word / boundary-aware replaces only** for identifier sweeps. The per-rule regexes in
   §7.D are normative; do not widen them.
4. **Order matters.** Groups run in the order given; inside §7.C and §7.D the numbered rules run
   in the numbered order (longest-token-first protects against substring clobbering, e.g.
   `ReactiveUICommonUI` must be mapped before any bare `ReactiveUI` rule could touch it).
5. **Enumerate-then-map** (§7.D rule D6): before running an open-class rule, enumerate the distinct
   tokens it would touch and check every one against the table. An unlisted token = STOP.
6. **Tier 3 is frozen.** Historical record — `CHANGELOG.md` entries below the new 0.15.0 section,
   `plans/archive/`, `plans/DISCORD_CHANGELOG.md` past posts, `research/`, `BENCH_BASELINES.md`,
   git history itself — keeps its old names forever. Only **live URLs** inside Tier-3 files are
   updated (they would otherwise 404 or point at a fork-target after transfer; GitHub redirects
   cover most, Pages URLs are NOT redirected — see §3).
7. **Engine-gated steps** (marked `[ENGINE]`) need a local UE 5.6.1 install (see `CLAUDE.md` for
   the engine path convention) or the self-hosted CI leg. If no engine is available, complete every
   non-engine step, then report the exact `[ENGINE]` steps left for the owner/CI.
8. **Branch flow (house rule, mandatory):** work on a feature branch (`rebrand/umbrella`) cut from
   `dev`; push the branch ONLY. Never push `master` or `dev`. The owner PRs into `dev`, waits for
   checks, merges, and fast-forwards `master` himself. No `Co-Authored-By` trailers on any commit.
9. **The working tree must be clean** before Phase 3 starts (`git status --short` empty). As of
   census day the repo's main checkout sat on `docs/discord-unreal-announcement` with a modified
   `ReactiveUIUnrealDemo.uproject` — that state belongs to another workstream; do not stage,
   stash, or discard it. Use a fresh worktree/clone of `dev` if the main checkout is busy.

---

## 2. Gates — what must happen before execution

| Gate | What | Who |
|---|---|---|
| G1 | GitHub org `reactive-ui-toolkit` exists; repo transferred and renamed to `ruitk-unreal` (transfer first, rename second — both preserve issues/PRs/stars/secrets/rulesets; git+web URLs redirect, **GitHub Pages URLs do NOT redirect**; the freed `yanivkalfa/ReactiveUI-Unreal` name must never be reused) | OWNER |
| G2 | Godot leg executed first (owner ruling: Godot-only first; this plan runs when the owner says go for Unreal) | OWNER |
| G3 | Open questions UE-Q1…UE-Q4 (§5) ratified | OWNER |
| G4 | GitHub Pages re-enabled on the transferred repo and the new URL confirmed serving (`https://reactive-ui-toolkit.github.io/ruitk-unreal/`) | OWNER (one dashboard click) + executor verify |
| G5 | CI secrets survive transfer (they do) — `VSCE_PAT`/`OVSX_PAT`-class secrets confirmed present in the org repo settings before any publish | OWNER |

---

## 3. Name Registry (UE-N1 … UE-N20)

Resolved names. Every step in §6–§9 refers to these by number.

| # | Thing | OLD | NEW |
|---|---|---|---|
| UE-N1 | GitHub owner/org | `yanivkalfa` | `reactive-ui-toolkit` |
| UE-N2 | Repo slug | `ReactiveUI-Unreal` | `ruitk-unreal` |
| UE-N3 | Product display name | "ReactiveUI for Unreal" | "Reactive UI Toolkit for Unreal" |
| UE-N4 | Plugin folder + `.uplugin` filename | `Plugins/ReactiveUI/`, `ReactiveUI.uplugin` | `Plugins/ReactiveUIToolkit/`, `ReactiveUIToolkit.uplugin` (folder and uplugin filename MUST stay identical — Unreal resolves plugins by that pairing) |
| UE-N5 | Plugin name as referenced by projects (`.uproject` `"Plugins"` list, `EnablePlugins`) | `ReactiveUI` | `ReactiveUIToolkit` |
| UE-N6 | The 8 plugin modules | `ReactiveUICore, ReactiveUISlate, ReactiveUIUMG, ReactiveUICommonUI, ReactiveUIMVVMBridge, ReactiveUIInterp, ReactiveUIToolchain, ReactiveUIEditor` | `RuitkCore, RuitkSlate, RuitkUMG, RuitkCommonUI, RuitkMVVMBridge, RuitkInterp, RuitkToolchain, RuitkEditor` (UE-Q1 rec; see §5) |
| UE-N7 | Module export macros (UBT derives them from UE-N6) | `REACTIVEUICORE_API` (91), `REACTIVEUISLATE_API` (203), `REACTIVEUIUMG_API` (20), `REACTIVEUICOMMONUI_API` (11), `REACTIVEUIMVVMBRIDGE_API` (3), `REACTIVEUIINTERP_API` (24), `REACTIVEUITOOLCHAIN_API` (16), `REACTIVEUIEDITOR_API` (1) — 369 total | `RUITKCORE_API, RUITKSLATE_API, RUITKUMG_API, RUITKCOMMONUI_API, RUITKMVVMBRIDGE_API, RUITKINTERP_API, RUITKTOOLCHAIN_API, RUITKEDITOR_API` |
| UE-N8 | Plain-struct/class prefix | `FRui*` (9,406 occ; classifier `FRuiNode` 2,023) | `FRuitk*` (`FRuitkNode`) |
| UE-N9 | UObject class prefix | `URui*` (388 occ, 12 distinct) | `URuitk*` + CoreRedirects (§7.D rule D4) |
| UE-N10 | C++ namespace | `RUI::` (2,420 occ; `namespace RUI`) | `Ruitk::` / `namespace Ruitk` |
| UE-N11 | Commandlet spellings (user-facing `-run=` names derive from class names) | `-run=RUICompile / RUIContractDump / RUIExportSchema / RUIMigrateEsModules / RUIMigrateImports` (classes `URUICompileCommandlet` etc.) | `-run=RuitkCompile / RuitkContractDump / RuitkExportSchema / RuitkMigrateEsModules / RuitkMigrateImports` (classes `URuitkCompileCommandlet` etc.) |
| UE-N12 | Host-project game modules | `Source/RuiDemo` (204 refs), `Source/RuiHostTests` (47 refs) | `Source/RuitkDemo`, `Source/RuitkHostTests` |
| UE-N13 | Host project + targets | `ReactiveUIUnrealDemo.uproject`, `ReactiveUIUnrealDemo{,Editor,Server}.Target.cs` | `RuitkUnrealDemo.uproject`, `RuitkUnrealDemo{,Editor,Server}.Target.cs` (UE-Q2) |
| UE-N14 | Docs site base path + Pages URL | `base: '/ReactiveUI-Unreal/'` → `https://yanivkalfa.github.io/ReactiveUI-Unreal/` | `base: '/ruitk-unreal/'` → `https://reactive-ui-toolkit.github.io/ruitk-unreal/` |
| UE-N15 | VS Code extension identity | publisher `ReactiveUITK`, name `uetkx`, displayName "UETKX (Unreal - VS Code)" | **UNCHANGED** (family ruling #3: marketplace identity AND display names both stay; publisher IDs are hard-immutable anyway) |
| UE-N16 | VS2022 extension identity | Id `UetkxVsix.ReactiveUITK`, Publisher "Yaniv Kalfa", DisplayName "UETKX (Unreal - VS2022)" | **UNCHANGED** (same ruling) |
| UE-N17 | Release asset zips (`publish.yml`) | `ReactiveUI-<ver>.zip`, `ReactiveUI-<ver>-UE<eng>.zip` | `ReactiveUIToolkit-<ver>.zip`, `ReactiveUIToolkit-<ver>-UE<eng>.zip` |
| UE-N18 | Wave versions | plugin `0.14.0` (`"Version": 15`), extensions/LSP `0.8.0` | plugin **0.15.0** (`"Version": 16`) BREAKING; extensions/LSP **0.9.0** (UE-Q4) |
| UE-N19 | Migration doc + codemod | (precedent: `-run=RUIMigrateEsModules`, record-driven) | `MIGRATION-0.15.md` + commandlet `URuitkMigrateBrandCommandlet` (`-run=RuitkMigrateBrand`), §7.G |
| UE-N20 | Fab listing | none live yet (`"MarketplaceURL": ""`) | first-ever listing created AFTER the rebrand under the new name — zero migration cost, best-possible timing |

**Naming system coherence** (why these forms): slug `ruitk-unreal` ↔ type prefixes `FRuitk*/URuitk*` ↔
namespace `Ruitk` ↔ modules `Ruitk*` ↔ macros `RUITK*_API` — one abbreviation system, identical to
Godot's `ruitk-godot` ↔ `Ruitk*` classes ↔ `reactive_ui_toolkit` folder. The plugin FOLDER uses the
full words (`ReactiveUIToolkit`) exactly as Godot's addon folder does, because folders are the
user-visible install artifact.

---

## 4. Census (measured 2026-07-27, tree @ `dev` ≈ `5c9bb7a`)

The scale of the Unreal leg — largest of the three:

| Token / thing | Count | Where |
|---|---|---|
| `FRui*` identifiers | **9,406** | everywhere (runtime, demo, tests, extensions, docs) |
| `FRuiNode` (the E-01 component classifier — grammar, not just naming) | **2,023** | `Source/` 1,335 · `Plugins/` 452 · `ide-extensions/` 149 · docs 35 |
| `RUI::` namespace refs | **2,420** | C++ throughout |
| `URui*` (UHT-reflected UObject classes) | **388** (12 distinct classes) | plugin + demo |
| Other `RUI`-prefixed identifiers (commandlets etc.) | **473** | Toolchain, CI, skills, docs |
| Module-name refs | Core 67 · Slate 80 · UMG 34 · CommonUI 20 · MVVMBridge 18 · Interp 52 · Toolchain 69 · Editor 41 | `Build.cs`, `.uplugin`, `#include` paths, CI |
| `_API` export macros | **369** (breakdown in UE-N7) | headers |
| `RuiDemo` / `RuiHostTests` | 204 / 47 | host project |
| Committed generated files | **45 × `*.uetkx.inl`** + **2 × `Uetkx.gen.cpp`** | demo + tests (regen §7.H) |
| `yanivkalfa` URLs | **34 occ / 18 files** | list in §7.A |
| CoreRedirects | **none exist today** | `Config/` — will be ADDED (§7.D rule D4) |
| **Family corpus** (`uetkx-scanner-cases.json` `_tiers`) | `FRuiNode` ×15 — **ALL in `fileScanLeg`, a `perLeg` section. The 3 `familyCore` sections contain ZERO brand tokens** (verified by tier-walk 2026-07-27) | §7.F — the family hash must come out UNCHANGED |

The corpus fact is the leg's biggest de-risk: like Godot and Unity, the Unreal rename is
**repo-local** — no family-wide corpus re-pin. The 15 `fileScanLeg` tokens convert in lockstep
with the scanner's classifier constant (§7.E) and only move the *per-leg* portion of the fixture.

---

## 5. Open questions (owner ratifies before execution)

- **UE-Q1 — module name form.** REC (plan-primary, all tables assume it): **`Ruitk*`**
  (`RuitkCore` …). Rationale: matches the family abbreviation system, keeps the UBT-derived
  export macros readable (`RUITKCORE_API` vs the 30-char `REACTIVEUITOOLKITMVVMBRIDGE_API`),
  and shortens every user's `PublicDependencyModuleNames` line. ALTERNATIVE (if the owner
  prefers folder-style full words): `ReactiveUIToolkitCore` … — same steps apply; substitute
  the names throughout §7.C and re-derive UE-N7 macros by upper-casing.
- **UE-Q2 — host project rename.** REC: yes, `ReactiveUIUnrealDemo` → `RuitkUnrealDemo`
  (uproject + 3 Target.cs + `TargetName`/`ProjectName` strings + CI paths). It is not shipped,
  but "nothing stays" and it appears in every CI command line. Cost: one more rename cascade,
  fully enumerated in §7.C step C8.
- **UE-Q3 — demo localization binary.** `RuiDemo.locres` (binary, contains old identifier
  strings). REC: regenerate via the editor's Localization Dashboard after the sweep `[ENGINE]`;
  if skipped, the stale binary is demo-only cosmetic and goes on the §10 leftovers list.
- **UE-Q4 — wave versions.** REC: plugin **0.15.0** / `"Version": 16` (BREAKING — every user
  `.uetkx` head, `Build.cs` dep, include path, and C++ callsite changes); vscode + vs2022 + LSP
  **0.9.0**. Tags stay scheme `v*` / `vscode-v*` / `vs2022-v*`.

---

## 6. Phase structure

| Phase | What | Who |
|---|---|---|
| 0 | Preflight (§7.0) | executor |
| 1 | Org transfer + repo rename + Pages re-enable (G1/G4) | OWNER |
| 2 | Branch `rebrand/umbrella` off post-transfer `dev` | executor |
| 3 | Groups A–I (§7) in order | executor |
| 4 | Verification battery (§8) | executor + `[ENGINE]` legs |
| 5 | Release wave (§9) | executor prepares, OWNER merges + tags |

---

## 7. Phase 3 — the work, group by group

### 7.0 Preflight

1. `git status --short` → must be empty (contract rule 9).
2. Record the corpus baseline: `node scripts/corpus-hash.mjs` → save the printed familyCore hash
   to compare in §7.F. (The script hashes only `_tiers.familyCore` sections, prefix-normalized
   `UETKX→TKX` — type names are NOT normalized, which is exactly why familyCore must stay
   brand-token-free.)
3. Verify census anchor-counts still hold (STOP on drift):
   ```bash
   git grep -oh "\bFRui[A-Za-z_]*" | wc -l        # expect ≈9406
   git grep -c  "RUI::" | awk -F: '{s+=$2}END{print s}'   # expect ≈2420
   git grep -oh "REACTIVEUI[A-Z]*_API" | wc -l    # expect 369
   ```

### 7.A Group A — URL swap (runs FIRST after transfer; touches Tier-3 files for URLs ONLY)

Replace, in the 18 files below, every live URL:

| OLD | NEW |
|---|---|
| `https://github.com/yanivkalfa/ReactiveUI-Unreal` | `https://github.com/reactive-ui-toolkit/ruitk-unreal` |
| `https://github.com/yanivkalfa` (bare profile link, e.g. `CreatedByURL`) | `https://github.com/reactive-ui-toolkit` |
| `https://yanivkalfa.github.io/ReactiveUI-Unreal/` | `https://reactive-ui-toolkit.github.io/ruitk-unreal/` |

Files (occurrence counts from census): `plans/DISCORD_CHANGELOG.md` (4 — URL swap only, do NOT
touch the posts' prose), `ReactiveUIUnrealDocs~/src/pages/Licensing/LicensingPage.tsx` (4),
`Plugins/ReactiveUI/README.md` (4), `README.md` (3), `Plugins/ReactiveUI/ReactiveUI.uplugin` (3 —
`CreatedByURL`, `DocsURL`, `SupportURL`), `ide-extensions/vscode-uetkx/package.json` (2 —
`repository.url` + any homepage/bugs), `research/round2-implementation/godot-ecosystem.md` (1),
`plans/archive/PR_DESCRIPTION_uetkx-imports.md` (1), `plans/archive/EXTENSION_LISTING_PLAN.md` (1),
`plans/archive/AUDIT_2026-07-14.md` (1), `ide-extensions/vscode-uetkx/readme-template.md` (1),
`ide-extensions/vscode-uetkx/README.md` (1),
`ide-extensions/visual-studio/UetkxVsix/source.extension.vsixmanifest` (1),
`ide-extensions/visual-studio/UetkxVsix/publishManifest.json` (1),
`ide-extensions/visual-studio/UetkxVsix/overview-template.md` (1),
`ReactiveUIUnrealDocs~/src/links.ts` (1), `ReactiveUIUnrealDocs~/src/components/TopBar/TopBar.tsx` (1),
`LICENSE-COMMERCIAL.md` (1).

Also:
- `ReactiveUIUnrealDocs~/vite.config.*` line ≈69: `base: '/ReactiveUI-Unreal/'` →
  `base: '/ruitk-unreal/'`, AND the router `basename` in `ReactiveUIUnrealDocs~/src/main.tsx`
  (the vite config comment says the two mirror each other — change both).
- `git remote -v` sanity: after transfer the old remote URL still redirects; update local remotes
  to the canonical new URL anyway (`git remote set-url origin …/ruitk-unreal.git`).

Verify: `git grep -n "yanivkalfa" -- . ':!plans/archive' ':!research'` → only hits allowed are
Tier-3 prose (author attribution "Yaniv Kalfa" / `CreatedBy` stays — it's a person, not a URL).

### 7.B Group B — display strings + licenses

1. `Plugins/ReactiveUI/ReactiveUI.uplugin`:
   - `"FriendlyName": "ReactiveUI for Unreal"` → `"FriendlyName": "Reactive UI Toolkit for Unreal"`
   - `"Description"`: keep the sentence, no brand token inside it today — verify, don't edit.
   - (URLs already done in Group A; `"Version"`/`"VersionName"` bump happens in §9, not here.)
2. LICENSE product labels: grep both license files —
   `git grep -n "ReactiveUI" LICENSE.md LICENSE-COMMERCIAL.md` — replace every product-label
   occurrence (the strings naming THIS product, e.g. "ReactiveUI (Unreal)") with
   "Reactive UI Toolkit for Unreal". The license TEXT (Community License 1.0 terms) does not
   change. STOP and list them if any hit is ambiguous between product-label and prose.
3. `README.md` + `Plugins/ReactiveUI/README.md`: title lines and product-name prose →
   "Reactive UI Toolkit for Unreal" (first mention), "the toolkit" thereafter. Historical
   changelog links inside README stay as-is apart from Group A URL swaps.
4. Docs site display strings: `ReactiveUIUnrealDocs~/` — grep `"ReactiveUI"` in `src/` and
   convert TITLES and NAV labels (`TopBar`, `<title>`, landing hero) to the UE-N3 name.
   Code-sample content converts in Group D/E sweeps, not here.
5. `.claude/skills/*.md` + `CLAUDE.md`: these are LIVE operator docs (Tier 1) — their `-run=RUI*`
   command spellings and module/class names update in Groups C/D sweeps automatically; in THIS
   step only fix display-name prose ("ReactiveUI for Unreal" → UE-N3).
6. `plans/MASTER_PLAN.md`, `plans/REMAINING.md`, `plans/PENDING_CHANGELOG.md` — live planning
   docs: same treatment as 5. (`plans/archive/` stays frozen.)

### 7.C Group C — module + folder + project renames (ordered!)

**Rule order protects substrings: rename module tokens LONGEST-FIRST, then the plugin folder,
then bare `ReactiveUI` display leftovers are Group B's problem (already done) — after C, a bare
`ReactiveUI` token should only survive where §10 expects it.**

C1. Module token sweep — replace as WHOLE WORDS, in this exact order (longest first):

| # | OLD | NEW |
|---|---|---|
| 1 | `ReactiveUIMVVMBridge` | `RuitkMVVMBridge` |
| 2 | `ReactiveUICommonUI` | `RuitkCommonUI` |
| 3 | `ReactiveUIToolchain` | `RuitkToolchain` |
| 4 | `ReactiveUIEditor` | `RuitkEditor` |
| 5 | `ReactiveUIInterp` | `RuitkInterp` |
| 6 | `ReactiveUISlate` | `RuitkSlate` |
| 7 | `ReactiveUICore` | `RuitkCore` |
| 8 | `ReactiveUIUMG` | `RuitkUMG` |

   Touches: `.uplugin` `Modules[].Name`, every `*.Build.cs` (own name + dependency lists),
   `#include "ReactiveUICore/..."`-style paths, CI yml, docs, skills. Boundary regex:
   `\bReactiveUIMVVMBridge\b` etc.
C2. Export-macro sweep (UE-N7): 8 exact whole-word pairs `REACTIVEUICORE_API` → `RUITKCORE_API`
    etc. (369 total). UBT derives the macro from the module name — after C1+C2 the two agree again.
C3. Rename module DIRECTORIES + their `.Build.cs` files via `git mv` (preserves history):
    ```bash
    for m in Core Slate UMG CommonUI MVVMBridge Interp Toolchain Editor; do
      git mv "Plugins/ReactiveUI/Source/ReactiveUI$m" "Plugins/ReactiveUI/Source/Ruitk$m"
      git mv "Plugins/ReactiveUI/Source/Ruitk$m/ReactiveUI$m.Build.cs" "Plugins/ReactiveUI/Source/Ruitk$m/Ruitk$m.Build.cs"
    done
    ```
    (If a module's Build.cs was already renamed by a glob step, skip — verify each exists first.)
C4. Rename the uplugin file, THEN the plugin folder (this order keeps paths valid at each step):
    ```bash
    git mv Plugins/ReactiveUI/ReactiveUI.uplugin Plugins/ReactiveUI/ReactiveUIToolkit.uplugin
    git mv Plugins/ReactiveUI Plugins/ReactiveUIToolkit
    ```
C5. Path-string sweep: `Plugins/ReactiveUI/` → `Plugins/ReactiveUIToolkit/` and
    `Plugins/ReactiveUI ` (trailing-space/quote/paren forms) across `.github/workflows/*.yml`
    (publish.yml zips at lines ≈112/122/128, engine-tests.yml), `scripts/package-plugin.ps1`,
    `scripts/check-headers.mjs`, `scripts/verify-mirror.mjs`, `.claude/skills/`, docs. Use grep
    `"Plugins/ReactiveUI"` and convert every hit (the folder no longer exists — no ambiguity).
C6. Plugin NAME references (UE-N5): in `ReactiveUIUnrealDemo.uproject` `"Plugins"` list,
    `"Name": "ReactiveUI"` → `"Name": "ReactiveUIToolkit"`. Grep for other `"ReactiveUI"` exact
    JSON-name tokens (`EnablePlugins`, test configs).
C7. Release-asset names (UE-N17): in `publish.yml`, `ReactiveUI-${{ steps.ver.outputs.version }}`
    zip name fragments → `ReactiveUIToolkit-…` (3+ sites incl. the release-body printf at line
    ≈104 and the sync-comment at ≈156), and the `jq -r .VersionName Plugins/ReactiveUI/…` path
    (already covered by C5 — verify).
C8. Host project (UE-Q2, if ratified):
    ```bash
    git mv Source/RuiDemo Source/RuitkDemo
    git mv Source/RuiHostTests Source/RuitkHostTests
    git mv ReactiveUIUnrealDemo.uproject RuitkUnrealDemo.uproject
    for t in "" Editor Server; do git mv "Source/ReactiveUIUnrealDemo$t.Target.cs" "Source/RuitkUnrealDemo$t.Target.cs"; done
    ```
    Then whole-word sweeps `ReactiveUIUnrealDemo` → `RuitkUnrealDemo` (Target.cs class names +
    `TargetName`, CI command lines in `engine-tests.yml` line ≈59, `.claude/skills/test-run` etc.)
    and `RuiDemo` → `RuitkDemo` (204), `RuiHostTests` → `RuitkHostTests` (47) — these two run in
    Group D's D5 rule ordering (after `FRui`/`URui` rules, see below; `\bRui` does not match
    inside `FRui`/`URui` so order is actually safe either way — keep D5 anyway for auditability).

### 7.D Group D — the identifier sweep (C++ + everywhere)

Run rules in this order, each as a single repo-wide regex replace over TRACKED TEXT files
(exclude: `.git/`, binary `*.locres` `*.uasset` `*.png`…, and the §10 frozen Tier-3 paths —
`plans/archive/`, `research/`, `BENCH_BASELINES.md`, `plans/DISCORD_CHANGELOG.md` below its
newest entry, `CHANGELOG.md` below the new 0.15.0 section):

| # | Rule (regex → replacement) | Covers | Census |
|---|---|---|---|
| D1 | `\bFRui` → `FRuitk` | ALL `FRui*` types incl. `FRuiNode`→`FRuitkNode` | 9,406 |
| D2 | `\bURui` → `URuitk` | the 12 UObject classes | 388 |
| D3 | `\bRUI::` → `Ruitk::` AND `\bnamespace RUI\b` → `namespace Ruitk` AND `using namespace RUI\b` → `using namespace Ruitk` | the C++ namespace | 2,420 |
| D4 | **CoreRedirects (ADD, don't replace):** for every renamed UHT-reflected type — enumerate first: `git grep -B3 "class REACTIVEUI\|class RUITK" -- '*.h' \| grep -A3 "UCLASS\|USTRUCT\|UENUM"` (post-C2 macros) — add to the demo project's `Config/DefaultEngine.ini` a `[CoreRedirects]` block: `+ClassRedirects=(OldName="/Script/ReactiveUISlate.RuiHostWidget",NewName="/Script/RuitkSlate.RuitkHostWidget")`-style entries for each of the 12 `URui*` classes (old `/Script/<OldModule>.<OldClassNameNoU>` → new), plus `StructRedirects`/`EnumRedirects` for any reflected `FRui*`/`ERui*` UHT types the enumeration surfaces. The SAME block ships verbatim inside `MIGRATION-0.15.md` for users' projects. No CoreRedirects existed before — this is a new section. | binary `.uasset`/BP references load-fix | 12 classes |
| D5 | `\bRui(?!tk)` word-start → `Ruitk` — but ONLY via enumerate-then-map: `git grep -oh "\bRui[A-Za-z_]*" \| sort -u` and check every token against: `RuiDemo→RuitkDemo`, `RuiHostTests→RuitkHostTests` (+ their `.locres`/config name forms). Unlisted token = STOP. | host modules | 204+47 |
| D6 | `\bRUI(?=[A-Z])` → `Ruitk` — enumerate-then-map: `git grep -oh "\bRUI[A-Za-z_]*" \| sort -u`; expected tokens are exactly the commandlet family `RUICompile, RUIContractDump, RUIExportSchema, RUIMigrateEsModules, RUIMigrateImports` (+ their `URUI*Commandlet` class forms, which D2 does NOT cover — `URUI` ≠ `URui` case) → `RuitkCompile…` / `URuitkCompileCommandlet…`. Any OTHER `RUI*` token (e.g. an all-caps log category or macro the census bucketed into the 473) gets mapped `RUI→RUITK` (`RUI_LOG`-style → `RUITK_LOG`) and MUST be added to the executed-mapping list in the commit message. | commandlets + misc | 473 |
| D7 | `-run=RUI` → `-run=Ruitk` — should already be covered by D6; run the grep to verify ZERO remaining: `engine-tests.yml`, `.claude/skills/{dev-process,engine-catchup,grammar-contract,new-component,rebuild-ide-extensions,test-run}/SKILL.md`, `CLAUDE.md` | CI + operator docs | — |

After D: `git grep -n "\bFRui\b\|\bFRui[A-Z]\|\bURui[A-Z]\|RUI::" -- ':!plans/archive' ':!research'`
→ ZERO hits outside §10 leftovers.

### 7.E Group E — ide-extensions content (identity FROZEN, content converts)

The marketplace identities (UE-N15/16) do NOT change: `publisher`, `name`, `displayName`,
vsix `Id`. Everything underneath converts:

1. LSP scanner/formatter classifier: the D1 sweep already turned `FRuiNode` → `FRuitkNode` in
   `ide-extensions/lsp-server/src/**` (incl. `formatUetkx.ts` ≈line 570/573 and every test file)
   AND in `ide-extensions/lsp-server/test-fixtures/uetkx-scanner-cases.json` — the 15 fixture
   occurrences all sit in the `fileScanLeg` per-leg section, in lockstep with the scanner code.
   Verify the lockstep: `grep -c "FRuitkNode" ide-extensions/lsp-server/test-fixtures/uetkx-scanner-cases.json` → 15, `grep -c "FRuiNode"` → 0.
2. TextMate grammar + snippets in `ide-extensions/vscode-uetkx/` — D1 covered the token; verify
   with `git grep -n "FRuiNode" ide-extensions/` → 0.
3. `ide-extensions/vscode-uetkx/package.json`: `repository.url` already fixed in Group A; verify
   `description`/README/readme-template prose uses UE-N3 for the product (Group B style), but the
   extension's OWN displayName stays byte-identical.
4. `ide-extensions/visual-studio/UetkxVsix/`: same split — manifest identity frozen,
   overview-template prose + URLs converted.
5. Rebuild + tests are in §8 (LSP suite ≈152 tests + smoke).

### 7.F Group F — contracts, schema, goldens, corpus `[ENGINE]`

1. Regenerate grammar goldens: `<Engine>\UnrealEditor-Cmd.exe <abs>\RuitkUnrealDemo.uproject
   -run=RuitkContractDump` (WITHOUT `--check` — we are re-dumping, exactly like the Godot plan's
   Group F), commit the refreshed goldens; the TS side replays them in the LSP suite.
2. Refresh the schema: `-run=RuitkExportSchema` → `uetkx-schema.json` (LSP + generated docs pages
   pick it up).
3. **Corpus gate (STOP-gate):** `node scripts/corpus-hash.mjs` → the familyCore hash MUST equal
   the §7.0 baseline byte-for-byte. familyCore contained zero brand tokens, so any drift means a
   sweep leaked into a familyCore section — revert that hunk, do NOT re-pin. (The per-leg
   `fileScanLeg` change is invisible to the hash by design.)
4. `scripts/check-headers.mjs`, `scripts/verify-mirror.mjs`, `scripts/docs-drift.mjs`,
   `scripts/check-style-builders.mjs` — run all; they are name-based and consume the renamed tree.
   Fix path constants inside them if C5 grep missed any (STOP-report if logic, not paths, breaks).

### 7.G Group G — user codemod + migration doc

1. New commandlet `URuitkMigrateBrandCommandlet` (`-run=RuitkMigrateBrand`) in `RuitkToolchain`,
   modeled 1:1 on the existing record-driven `URuitkMigrateEsModulesCommandlet` (post-rename
   name). It rewrites a USER project: `.uetkx` heads (`FRuiNode`→`FRuitkNode` + any `FRui*`/
   `URui*`/`RUI::`/module tokens in embedded C++), `Build.cs` dependency names (C1 table),
   `#include` path prefixes, `.uproject` plugin name (UE-N5). Idempotent — a clean tree reports 0.
2. `MIGRATION-0.15.md` at repo root: the manual steps (delete old `Plugins/ReactiveUI`, install
   `Plugins/ReactiveUIToolkit`, run the codemod, paste the D4 CoreRedirects block, resave any
   Blueprint referencing a `URui*` type, then `-run=RuitkCompile -check` must exit 0). Include
   the full C1 module table and the D1–D6 rule list verbatim so users can hand-migrate.
3. `CHANGELOG.md`: new `## 0.15.0` section at top — BREAKING, the rename in one table, link to
   MIGRATION-0.15.md. Entries below it are Tier-3 frozen.

### 7.H Group H — committed generated files `[ENGINE]`

The 45 `*.uetkx.inl` + 2 `Uetkx.gen.cpp` are committed text — the D sweeps already rewrote them.
Prove the compiler agrees: run `-run=RuitkCompile` over the demo tree, then `git status --short`
→ empty (byte-stable regen). If the regen differs from the sweep result, commit the REGENERATED
form (the compiler is the source of truth) and STOP-report the diff summary.

### 7.I Group I — expected-leftovers audit

Run §10's greps. Every hit must match the table. Anything else = STOP (contract rule 1).

---

## 8. Phase 4 — verification battery

Non-engine (executor runs all):
```bash
cd ide-extensions/lsp-server && npm ci && npm run build && node --test out/test/*.test.js && node scripts/smoke.js
cd ide-extensions/vscode-uetkx && npm ci && npm run build
cd ReactiveUIUnrealDocs~ && npm ci && npm run build && npm run lint
node scripts/corpus-hash.mjs          # familyCore hash == §7.0 baseline
node scripts/check-headers.mjs && node scripts/verify-mirror.mjs && node scripts/docs-drift.mjs
```
`[ENGINE]` (owner machine or the self-hosted `engine-tests.yml` leg):
```
UnrealEditor-Cmd RuitkUnrealDemo.uproject -run=RuitkCompile -check     # exit 0
UnrealEditor-Cmd RuitkUnrealDemo.uproject -run=RuitkContractDump --check
full automation suite per .claude/skills/test-run (all legs green — the suite was 132/132 pre-rename)
Editor boot: demo gallery opens, no CoreRedirects warnings in the log after asset resave
```
VS2022 extension build per `ide-extensions/README`/CI job (Windows + msbuild `[ENGINE]`-class gate).

---

## 9. Phase 5 — release wave (owner merges first)

1. Bump `Plugins/ReactiveUIToolkit/ReactiveUIToolkit.uplugin`: `"Version": 15 → 16`,
   `"VersionName": "0.14.0" → "0.15.0"` (UE-Q4). `scripts/bump.mjs` if it automates this —
   inspect before use.
2. Extensions + LSP `package.json`/vsixmanifest versions `0.8.0 → 0.9.0`.
3. `plans/PENDING_CHANGELOG.md` → fold into `CHANGELOG.md` 0.15.0 per house flow;
   `plans/DISCORD_CHANGELOG.md` gets a NEW 0.15.0 post (old posts frozen).
4. Owner: PR → dev → checks → merge → fast-forward master → tag `v0.15.0` (publish.yml attaches
   `ReactiveUIToolkit-0.15.0.zip` + per-engine stamped zips), then `vscode-v0.9.0` / `vs2022-v0.9.0`.
5. Fab (UE-N20): first listing, created fresh under "Reactive UI Toolkit for Unreal" — nothing to
   migrate. `MarketplaceURL` in the uplugin gets filled when the listing exists (post-wave patch).

---

## 10. Expected leftovers (the ONLY permitted old-name survivors)

| Grep | Where hits are allowed |
|---|---|
| `ReactiveUI` | `CHANGELOG.md` below 0.15.0 · `plans/archive/**` · `plans/DISCORD_CHANGELOG.md` old posts · `research/**` · `BENCH_BASELINES.md` · MIGRATION-0.15.md's OLD column · the codemod's OLD-token tables · git history |
| `FRui\|URui\|RUI::` | same Tier-3 set + MIGRATION/codemod OLD columns |
| `RuiDemo.locres` binary content | if UE-Q3 skipped (demo-only) |
| `yanivkalfa` | Tier-3 prose + "Yaniv Kalfa" author attributions (`CreatedBy`, vsix `Publisher`, license copyright line) — PEOPLE keep their name; only URLs moved |
| `ReactiveUI-Unreal` | Tier-3 prose; and NOWHERE as a live URL |

---

## 11. Rollback

Everything before the owner's merge is one feature branch — `git branch -D rebrand/umbrella` and
re-clone. After merge: revert the merge commit on dev; the org transfer itself is NOT rolled back
(redirects keep old URLs alive; Pages URL would need the old repo name back — avoid, per G1 never
reuse freed names). CoreRedirects are additive and safe to leave in place.
