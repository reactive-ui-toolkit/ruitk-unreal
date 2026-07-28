# Bugs found after the 0.15.0 "Reactive UI Toolkit" rebrand

**Audit scope:** branch `rebrand/umbrella` (worktree `wave-unreal`), the ReactiveUI→Ruitk umbrella
rebrand (~40 commits). **Find-only** review — nothing fixed, nothing committed.

**Method:** the mechanical identifier sweep is already green (Windows build + 132/132 suite; Linux
5.6/5.7 CI; byte-stable regen; contract goldens 36/36; corpus hash unchanged). So this audit targets
what a green build does **not** exercise: written-vs-read string pairs, editor-only UI ids, config
section ownership, the CoreRedirects block, the user-facing codemod + migration guide, the
localization pipeline, and the packaging/docs/publish paths (Publish has not run on this branch).
Findings were produced by five focused passes plus an independent structural sweep; the top two
were re-verified by hand. "Rui" is a substring of "Ruitk" throughout, so all leftover-token scans
used negative lookahead (`Rui(?!tk)`, `RUI_(?!)`, etc.).

**Bottom line:** the rebrand is well-executed. The runtime, loc pipeline, CoreRedirects, build
system, and packaging/publish paths are clean. The only findings of consequence are in the
**user-facing migration path** (the codemod + MIGRATION-0.15.md), not in the shipped runtime.
One HIGH, one MEDIUM, one LOW-MEDIUM; the rest are LOW/cosmetic.

> **SUPERVISOR VERIFICATION (2026-07-28):** independently re-verified against the tree — H1's
> documented step order, M1's `/Plugins/` skip token, LM1's UCLASS config line, L1's guard-less
> doc pattern vs guarded code, L3's exact-spacing uproject literal, L4's zero Demo* redirects,
> L5's `RUI.Suspense` FName, and every other anchor CONFIRMED. **Nothing deleted; all findings
> valid and cleared for the fix pass.** Fixes must NOT be committed — the supervisor reviews the
> diffs first.

---

## HIGH

### H1 — MIGRATION-0.15.md steps 1–3 deadlock a C++ project's migration
- **Where:** `MIGRATION-0.15.md:12-21` (steps 1–3) + `RuitkMigrateBrandCommandlet` lives in the new
  plugin's `RuitkEditor` module.
- **What:** The documented order is (1) delete `Plugins/ReactiveUI/`, (2) install
  `Plugins/ReactiveUIToolkit/`, (3) `UnrealEditor-Cmd <Proj>.uproject -run=RuitkMigrateBrand`. For a
  **C++ project**, after steps 1–2 the user's own `.cpp/.h` still reference `FRuiNode`,
  `#include "ReactiveUICore/…"`, `RUI::`, etc. — none of which exist anymore. Launching a commandlet
  via `UnrealEditor-Cmd` first compiles+loads the project's game modules; that compile now fails, so
  the editor never launches and the codemod in step 3 **never runs**. The commandlet cannot run
  before the swap either (it ships in the new plugin). Result: the documented path cannot complete.
- **Why it matters:** C++ projects are the primary migration audience, and this is the one-time
  upgrade gate every one of them hits. A safe order EXISTS but is undocumented: the old and new
  plugins have **disjoint** module names (`ReactiveUICore` vs `RuitkCore`) and disjoint reflected
  class names, so they can be installed side-by-side — install new **alongside** old, run the
  codemod, then delete old. (Blueprint-only / `.uetkx`-only projects are unaffected — nothing of
  theirs fails to compile.)
- **Fix direction (not applied):** document the side-by-side order, OR ship the codemod somewhere it
  can run pre-swap, OR tell users to keep the old plugin installed until after step 3.
- **Severity:** HIGH (blocks the documented C++ migration path; workaround exists but is undocumented).

---

## MEDIUM

### M1 — Codemod skips the user's OWN project plugins, not just the vendored one
- **Where:** `Plugins/ReactiveUIToolkit/Source/RuitkEditor/Private/RuitkMigrateBrandCommandlet.cpp:104-116`
  (`ShouldSkip`, skip token `TEXT("/Plugins/")`).
- **What:** `ShouldSkip` returns true when `Path.Contains(TEXT("/Plugins/"))`, intending to skip the
  vendored `ReactiveUIToolkit` plugin (comment: "The 0.15 plugin ships converted"). But it skips
  **every** path under any `Plugins/` folder — including a user who keeps their UI in
  `<Project>/Plugins/MyUI/Source/MyUI/Private/Screen.cpp` (a very common UE layout). That file, full
  of `FRuiNode` / `#include "ReactiveUICore/…"`, is silently never rewritten → partial migration →
  compile failure with **no diagnostic** pointing at the cause.
- **Why it matters:** silent under-migration of a common project shape; the user is left debugging a
  build failure the tool was supposed to prevent, and `MIGRATION-0.15.md:23-25` reinforces the trap
  by promising it rewrites "your `.uetkx` files … `#include` paths" without noting the exclusion.
- **Fix direction:** skip only the vendored plugin path (match `/Plugins/ReactiveUIToolkit/` or the
  plugin's own root), not all `/Plugins/`.
- **Severity:** MEDIUM (common layout, silent, but self-inflicted only on projects using project-plugins).

---

## LOW-MEDIUM

### LM1 — HMR editor-settings rename orphans existing users' per-user prefs (no redirect/migration)
- **Where:** `Plugins/ReactiveUIToolkit/Source/RuitkEditor/Private/RuitkUetkxEditorSettings.h:18-19`
  (`UCLASS(config = EditorPerProjectUser, defaultconfig)` `URuitkUetkxEditorSettings`, module
  `RuitkEditor`). Old shipped name (per `plans/REBRAND_PLAN.md:79`, `plans/archive/HMR_V2_PLAN.md:160`):
  `UReactiveUetkxEditorSettings` in module `ReactiveUIEditor`.
- **What:** A `UCLASS(config)` section name is `[/Script/<Module>.<Class>]`, so the rename moves the
  section from `[/Script/ReactiveUIEditor.ReactiveUetkxEditorSettings]` to
  `[/Script/RuitkEditor.RuitkUetkxEditorSettings]`. Existing users' saved HMR prefs live under the old
  section. There is **no** CoreRedirect for it (`Config/DefaultEngine.ini:33-41` has 9 redirects, none
  for this class — the `ReactiveUetkx*` token was never matched by any codemod rule; `REBRAND_PLAN.md:79`
  records it as "v1 missed"), and no migration carries them forward. On upgrade, the 7 toggles
  (bShowNotifications, bVerboseWatcher, bHideLiveCodingConsole, bFollowPie, bDisableSessionOnStop,
  DebounceMs, WatchedRoots) silently reset to code defaults.
- **Why NOT higher:** there is **no written-here/read-there split** — every code path
  (`SRuitkUetkxHmrPanel.cpp`, `UetkxHmrController.cpp`, `UetkxWatcher.cpp`) reads the NEW class via
  `GetDefault/GetMutableDefault<URuitkUetkxEditorSettings>`; nothing still reads the old section. So the
  impact is a one-time cosmetic reset of per-user editor toggles, not a functional break.
- **Fix direction:** either accept the reset (document it), or add a settings/section upgrade path.
- **Severity:** LOW-MEDIUM (cosmetic one-time reset; editor-only; developer-facing).

---

## LOW

### L1 — MIGRATION-0.15.md prints codemod rule 8 WITHOUT its idempotency guard (doc ≠ code)
- **Where:** `MIGRATION-0.15.md:86-88` documents the ALL-CAPS rule as `\bRUI(?=[A-Z_])` → `RUITK`; the
  actual code is `\bRUI(?!TK)(?=[A-Z_])` (`RuitkMigrateBrandCommandlet.cpp:70`).
- **What:** The guide is headed "what the codemod applies, verbatim" and "Ordered; all
  idempotency-guarded", but the printed rule 8 drops the `(?!TK)` guard. Applying the doc's literal
  pattern twice: `RUITK_PROP` → `RUITKTK_PROP`. Anyone re-deriving the rule from the guide gets a
  double-applying rule.
- **Severity:** LOW-MEDIUM (documentation defect; the shipped code is correct).

### L2 — Codemod over-matches real words / user identifiers (silent corruption)
- **Where:** `RuitkMigrateBrandCommandlet.cpp:67,52,74` — rules `\bFRui(?!tk)`, `\bRui(?!tk)`,
  `\brui\.(?=[A-Z])`.
- **What:** The guards only exclude the already-migrated continuation (`tk`/`TK`), not a word boundary
  after `Rui`, so non-brand tokens are rewritten: comment/string `TEXT("Ruined save")` → `"Ruitkned save"`;
  a user struct `FRuit` → `FRuitkt`; a local variable `rui.GetValue()` → `ruitk.GetValue()`. Silent, in
  comments / user-facing string literals / uncommon identifiers.
- **Severity:** LOW (rare in practice, but real and silent — a `-dry` report would surface it).

### L3 — Codemod `.uproject` plugin-reference rule is whitespace-brittle
- **Where:** `RuitkMigrateBrandCommandlet.cpp:50` matches the literal `"Name": "ReactiveUI"` (exactly one
  space after the colon).
- **What:** A hand-edited or differently-serialized `.uproject` (`"Name":"ReactiveUI"` or
  `"Name" : "ReactiveUI"`) is not matched, so the project keeps referencing a plugin that no longer
  exists → "plugin not found" on load.
- **Severity:** LOW (most `.uproject` files use the canonical spacing).

### L4 — CoreRedirects block is internally inconsistent (demo interop widgets omitted)
- **Where:** `Config/DefaultEngine.ini:33-41` redirects `RuiDemo.RuiDemoGameMode` but omits the sibling
  demo interop widgets in the same renamed module: `RuiDemo.DemoActivatableScreen`,
  `RuiDemo.DemoHostUserWidget`, `RuiDemo.DemoVmBoundWidget`, `RuiDemo.DemoStackHostWidget`,
  `RuiDemo.DemoUmgWidget` (defs at `Source/RuitkDemo/Private/DemoInteropWidgets.h:31,41,62,72`,
  `DemoUmgWidget.h:14`). These are `UUserWidget`/`UCommonActivatableWidget` subclasses whose reflected
  paths all changed with the module rename.
- **Why nil impact (still LOW):** the demo host project is **not shipped**, and this repo tracks no
  `.uasset/.umap` that could reference them — so nothing actually breaks. It is an internal-consistency
  gap (redirect one class in a module, skip its siblings), not a live defect.
- **Severity:** LOW.

### L5 — Suspense built-in registered with the OLD brand id `"RUI.Suspense"`
- **Where:** `Plugins/ReactiveUIToolkit/Source/RuitkCore/Private/RuitkCoreElements.cpp:97` —
  `RegisterComponentId((void*)&Ruitk::SuspenseComponent, FName(TEXT("RUI.Suspense")))`. The sibling
  built-in was correctly renamed: `RuitkRouter.cpp:602` uses `TEXT("RuitkRoutesComp")`.
- **What:** The only `TEXT("RUI.` brand-string literal left in shipping source. **Not a runtime break** —
  component identity is registered and resolved by function POINTER (`FindComponentId((void*)Fn)`), so
  this FName is never matched against a separately-written string; it only surfaces in diagnostic/debug
  output as "RUI.Suspense". Cosmetic incomplete-rebrand leftover.
- **Severity:** LOW.

### L6 — Docs search-keyword index carries stale brand tokens
- **Where:** `RuitkUnrealDocs~/src/docs.tsx:503,519` — hidden search keywords still contain
  "what is reactiveui for unreal", "reactiveui community license 1.0", "made with reactiveui".
- **What:** These are the docs site's search index for NEW user-facing content; the RENDERED pages are
  correct (`LicensingPage.tsx:124/141/164` say "Reactive UI Toolkit Community License" / "Made with
  Reactive UI Toolkit"). Search-only, no rendered impact — and old-brand search terms are arguably
  desirable for discoverability, so this is borderline-intentional.
- **Severity:** LOW.

### L7 — Docs package name still on old brand
- **Where:** `RuitkUnrealDocs~/package.json:2` (+ `package-lock.json:2,8`) — `"name": "reactiveui-unreal-docs"`.
- **What:** Stale brand in the renamed folder. It's a `private` package, so `npm ci` / `vite build` /
  Pages deploy are unaffected.
- **Severity:** LOW/cosmetic.

---

## NIT / cosmetic (recorded for completeness)

- **N1 — `RuitkMigrateBrandCommandlet.h:8`** claims "every record is guarded (`(?!tk)`/`(?!TK)`)", but the
  module tokens, the 9 export macros, the path/uproject/namespace rules, the `rui.` cvar rule, and
  `_RUI_HOOK_SIG` carry NO lookahead (they are idempotent only because their output no longer matches
  their input). Functionally fine; the stated invariant is inaccurate and could mislead a maintainer
  who later adds a rule that is NOT self-guarding.
- **N2 — `MIGRATION-0.15.md:125-126`** commandlet-rename list omits `RUISelfTest` (which exists in
  v0.14.0 and the codemod DOES convert correctly via rule 35). Documentation incompleteness only.
- **N3 — `RuitkCoreMisc.h:16-20`** (`STAT_RuiRenders/Commits/Placements/Updates/Deletions`, DEFINE side
  `RuitkCoreMisc.cpp:7-11`) and the macro-internal token `_RuiId` (`RuitkNode.h:193`) keep the old `Rui`
  stem. Compile-time identifiers with consistent declare/use — non-breaking; the `stat Ruitk` display
  names (`TEXT("Renders")` etc.) and the group `TEXT("Ruitk")` are correct.
- **N4 — `MIGRATION-0.15.md:111-114` / `Config/DefaultEngine.ini:38-41`** paste redirects for
  non-shipped modules (`RuitkHostTests`, `RuitkDemo`) into the USER-facing migration doc. Harmless (a
  redirect for an absent module never fires) but noise.

---

## Verified CLEAN (so the reader knows the scope covered)

- **Runtime string lookups** — tab ids (`RuitkPreview`, `RuitkUetkxHmr` register⇄invoke), MessageLog page
  id `"Ruitk"` (5 sites), plugin/module name strings (`FindPlugin("ReactiveUIToolkit")`, boot-test
  module names), component-factory FQN mangling (`RuitkUetkx_…` codegen⇄runtime⇄test), cvars
  (`ruitk.*` register⇄`FindConsoleVariable`), stat group, custom element `RuitkCanvas` (adapter⇄codegen
  tag map⇄markup⇄schema): all write/read pairs MATCH.
- **CoreRedirects (the 9 shipped)** — every OldName byte-exact to what v0.14.0 shipped; every NewName
  resolves to a class that exists now; zero reflected `FRui*` USTRUCT / `ERui*` UENUM exist (so the
  block's "complete set" claim holds).
- **Localization pipeline** — gather config (`RuitkDemo_Gather.ini`), codegen `NSLOCTEXT("Uetkx.%s")`,
  committed `.manifest/.locres/.archive` under `Content/Localization/RuitkDemo`, and the loc test's
  assertions all agree on namespaces `Ruitk`/`RuitkDemo`/`Uetkx.*` and the new paths.
- **Codemod idempotency + preserved tokens** — a second pass re-matches nothing; no rule touches
  `.uetkx`/`UETKX####`/`uetkx*`, the intentional `ReactiveUITK` publisher / `UetkxVsix.ReactiveUITK`,
  or "Yaniv Kalfa".
- **Build system** — Build.cs / Target.cs module-dependency strings, `.uproject` Modules + Plugins list,
  `IMPLEMENT_MODULE`/`IMPLEMENT_PRIMARY_GAME_MODULE("RuitkDemo")`, `GlobalDefaultGameMode`,
  commandlet class↔`-run=` names, automation spec prefix (`Ruitk.*`) ↔ CI filter (`RunTests Ruitk`),
  seller sentinel `.ruitk-seller-repo` (read⇄file): all consistent.
- **Packaging / publish / docs** — `publish.yml` uses the new plugin folder in the source-zip AND
  per-engine stamped-zip loop (`ReactiveUIToolkit`, not old `ReactiveUI`), zip/title/asset names correct,
  `RUITK_CI_ENGINE_ARMED`, `RuitkUnrealDemo.uproject`, clang-format globs, `FilterPlugin.ini` (+ its
  listed files exist), Fab template, docs `base: '/ruitk-unreal/'`, and version lockstep
  (uplugin 16/0.15.0, extensions 0.9.0 across all manifests, CHANGELOG `[0.15.0]` + byte-identical
  plugin mirror) all verified.

---

## Fix pass (2026-07-28) — disposition

All findings fixed on `rebrand/umbrella`, **uncommitted** for supervisor diff review. No engine run;
no commit; no push. Non-engine gates re-run green: `corpus-hash.mjs --check` unchanged
(`71a37c75…`), `verify-mirror.mjs` byte-identical, docs `npm ci && npm run build` clean,
clang-format **18.1.8** (the CI version, via `@wasm-fmt/clang-format@18.1.8`) clean over all 6
touched C++ files.

| # | Disposition | Where |
|---|---|---|
| H1 | FIXED — steps reordered to install-alongside → codemod → delete, with a blockquote explaining *why* (the codemod ships in the new plugin; `UnrealEditor-Cmd` compiles game modules before running it). Mirrored on the docs site as a `severity="warning"` Alert + a commented 3-step `BRAND_RUN`. | `MIGRATION-0.15.md:11-40`, `RuitkUnrealDocs~/src/pages/Migration/MigrationPage.tsx:53-57,163-170` |
| M1 | FIXED — `ShouldSkip` now matches `/Plugins/ReactiveUIToolkit/` **and** `/Plugins/ReactiveUI/` instead of all `/Plugins/`; user project-plugins are migrated. | `RuitkMigrateBrandCommandlet.cpp:118-134`, doc at `MIGRATION-0.15.md:35-39` |
| LM1 | FIXED (documented, no code) — new "One-time editor-preference reset" subsection naming both ini section headers and the 7 toggles, with the hand-rename workaround. | `MIGRATION-0.15.md:130-146` |
| L1 | FIXED — doc rule 8 now prints the `(?!TK)` guard, matching code. | `MIGRATION-0.15.md:112-114` |
| L2 | FIXED — see the enumeration proof below. All `Rui`-stem rules now require `(?=[A-Z_])`; the cvar rule enumerates its 9 names instead of matching an open `rui.` prefix. | `RuitkMigrateBrandCommandlet.cpp:51-86`, doc `MIGRATION-0.15.md:86-119` |
| L3 | FIXED — `"Name"\s*:\s*"ReactiveUI"`; replacement writes the canonical spacing. Still idempotent (output no longer matches input). | `RuitkMigrateBrandCommandlet.cpp:50` |
| L4+N4 | FIXED, one policy: **the repo ini is complete for the repo's own modules; the user-facing paste ships only what a user's content can reference.** So the 5 demo-widget redirects were ADDED to `Config/DefaultEngine.ini` (now 14, split into two commented groups), and the `RuiHostTests`/`RuiDemo` lines were REMOVED from the MIGRATION paste (now the 5 shipped-plugin classes). The docs-site paste already carried only those 5 — now consistent. | `Config/DefaultEngine.ini:28-58`, `MIGRATION-0.15.md:148-165` |
| L5 | FIXED — `TEXT("RUI.Suspense")` → `TEXT("Ruitk.Suspense")`. Zero `TEXT("RUI.` literals remain. | `RuitkCoreElements.cpp:97` |
| L6 | FIXED — added "reactive ui toolkit community license 1.1" and "made with reactive ui toolkit"; old-brand terms KEPT for discoverability (additive precedent). | `RuitkUnrealDocs~/src/docs.tsx:519` |
| L7 | FIXED — `ruitk-unreal-docs` in `package.json` + lockfile (2 name fields only). | `RuitkUnrealDocs~/package.json:2`, `package-lock.json:2,8` |
| N1 | FIXED — header now states the honest two-mechanism invariant (explicit lookahead on `Rui`-stem records; self-guarding for the rest) and the rule a new record must satisfy. | `RuitkMigrateBrandCommandlet.h:4-24` |
| N2 | **NOT DONE — finding's premise is false.** v0.14.0 shipped exactly five commandlets (`URUICompileCommandlet`, `URUIContractDumpCommandlet`, `URUIExportSchemaCommandlet`, `URUIMigrateImportsCommandlet`, `URUIMigrateEsModulesCommandlet`). There is **no** `RUISelfTest` commandlet in 0.14 and no `RuitkSelfTest` in 0.15; the token appears only in `research/round2-implementation/uetkx-toolchain.md:56` as a hypothetical ("…or a `-run=RUISelfTest` commandlet") and in `REBRAND_PLAN.md:293` as an illustrative token for the D6 rule. Adding it would document a commandlet that never existed. The list was instead made explicit: "all five that 0.14 shipped". | `MIGRATION-0.15.md:169-170` |
| N3 | FIXED — `STAT_Rui{Renders,Commits,Placements,Updates,Deletions}` → `STAT_Ruitk*` (5 DECLARE + 5 DEFINE + 5 INC, declare/use lockstep) and `FnName##_RuiId` → `_RuitkId`. **No engine regen needed:** `git grep` over `*.inl`/`*.gen.cpp`/`*.generated.h` finds neither token, and the pasted `_RuitkId` symbol has exactly one definition site with no external referent, so nothing generated couples to either stem. | `RuitkCoreMisc.h:16-20,67-99`, `RuitkCoreMisc.cpp:7-11`, `RuitkNode.h:193` |

Also updated for accuracy (not a finding): the `[0.15.0]` CHANGELOG entry described the MIGRATION
redirect block as "8 renamed `URui*` classes + `ARuiDemoGameMode`", which L4's policy change made
stale; it now states the five shipped classes, the project-plugin coverage, and the side-by-side
order. Applied to both `CHANGELOG.md` and its byte-identical plugin mirror (gate re-run green).

### L2 — enumeration proof (the codemod cannot be compiled here, so this stands in for a test)

**Guard is `(?=[A-Z_])`, not `(?=[A-Z])`.** The narrower guard the audit implied would have been a
regression: `templates/widget_test.template.cpp` and `widget_wrapper.template.cpp` use `FRui__TAG__Test`
/ `FRui__TAG__Props` / `FRui__TAG__Adapter` placeholders, which are `FRui` followed by `_`. Including
`_` in the class preserves those and still blocks the reported bug (`FRuit` → `t`, lowercase → no match).

**Follower census over the 0.14 tree (`003587e`), every occurrence:**

- bare `\bRui` — 2,246 followed by an uppercase letter; **0** followed by a lowercase letter, digit or
  `_`; 44 followed by punctuation/space, and all 44 are the English word "Rui" in a comment or doc
  ("the Rui side", "a Rui tree", "the `Rui` mark").
- `\bFRui` — followers are uppercase, plus **9 × `_`** (the templates above); no lowercase in real code.
- `\bURui/SRui/TRui/IRui/ERui/ARui` — uppercase only; zero `_`, zero lowercase.
- `LogRui` / `CVarRui` / `GRui` — uppercase only. (Guard extended to these three for the same reason;
  `GRui` already had `(?=[A-Z])`.)
- `rui.` — 9 distinct PascalCase cvar tokens (`DumpTree`, `FrameBudgetMs`, `Hmr`, `HookValidation`,
  `HostNodePool`, `Stats`, `StrictDiagnostics`, `StrictMode`, `TimeSlicing`). The only lowercase forms,
  `rui.reload` and `rui.stats`, exist solely in `plans/TECH_DEBT.md` and `research/` as never-shipped
  design notes — the old rule never converted them either (it required a following uppercase).

**Differential simulation.** Both rule tables (pre-fix and post-fix) were transcribed to equivalent
JS regexes and run over all **372** `{.uetkx,.h,.cpp,.inl,.cs,.uproject}` files of the 0.14 tree
(`Source/`, `templates/`, `Plugins/ReactiveUI/Source/`). They disagree on **20 lines across 11 files,
and every one is the English word "Rui" inside a C++ comment** — e.g.
`RuiUmgElement.h:3` "hosted as a Rui element", `ReactiveUIUmgTests.cpp:284` "Rui writes the VM".
**Zero identifiers, zero string literals, zero code differ.** Real brand tokens on those same lines
still convert identically under the new table (`FRuiValue`→`FRuitkValue`, `URuiHostWidget`→
`URuitkHostWidget`, `RUI::`→`Ruitk::`), and `templates/` produced **no** differences, confirming the
`_` in the guard class is both necessary and sufficient. The trade is therefore exactly: a prose word
in a comment is left alone, in exchange for never silently corrupting `FRuit`, `"Ruined save"`, or
`rui.GetValue()`. It is documented as such in the guide.

### Deliberately NOT done

- **N2** — premise false (above); documenting a non-existent commandlet would be a new defect.
- **No engine run.** `-run=RuitkMigrateBrand`, `RuitkCompile -check`, the automation suites and any
  build were not executed, per the hard rule. The C++ edits are unverified by a compiler: they are
  three literal-string/token renames, one `static const TCHAR*` array, and regex-literal edits inside
  an existing table — no signature, type or control-flow change. Recommended engine gate after diff
  review: `Ruitk.Boot` + `Ruitk.Core` (the `STAT_Ruitk*`/`_RuitkId` rename is compile-only, and
  `RUITK_COMPONENT` expands in every generated `.inl`, so a full build is the real check).
- Frozen surfaces untouched: `plans/archive`, `research/`, `REBRAND_PLAN.md`, `BENCH_BASELINES`,
  changelog history (only the `[0.15.0]` entry was touched), marketplace identities, old Discord posts.
