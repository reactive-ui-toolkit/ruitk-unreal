# File-Scoped Exports — per-file module identity for `.uetkx` (kill the global export ledger)

> **Status: DRAFT FOR OWNER REVIEW — authored 2026-07-24** from a full code investigation
> (this repo + the Unity sibling), triggered by TB-20 (owner, field-test session 2026-07-24:
> "every file is its own module, exactly like React"). **UETKX2106's "one exported name, one
> file" rule is recorded tech debt (TD-026), not design** — this plan removes the debt at the
> root. Every `file:line` anchor below was read from the working tree on 2026-07-24 (branch
> `fix/lsp-field-test-false-positives`, uncommitted tree included).
>
> **Branch:** own campaign branch off `dev` (after the current field-test branch merges), one
> branch, one PR. **Never commit unless the owner explicitly asks; NEVER push.**
>
> **Audience:** a model executing cold. Do not re-research the settled facts in §1–§2; do not
> re-litigate the FS-decisions in §3 (owner confirms them at plan review; a conflict found
> mid-execution = STOP AND ASK). Follow research → develop → test → bughunt → fix → commit per
> milestone (`dev-process` skill); never weaken a gate.
>
> **What does NOT change:** the `.uetkx` GRAMMAR (family byte-compatible corpus untouched — no
> new directives, no syntax), the reconciler/hooks semantics, the fiber model, the HMR
> Live-Coding pipeline shape, `RUI_COMPONENT` for hand-written C++ components, the strict-import
> rules (2300/2301/2302/2303/2304/2305/2307/2308/2326), and the `ExportHash` staleness engine.

---

## §0 — The mandate

ES-module semantics, exactly like React and exactly like the Unity sibling already ships:

1. **Every `.uetkx` file is its own module.** Two files may export the same name; they never
   interact. The ONLY place a collision can exist is inside one importing file — and `as`
   aliasing resolves it there (`import { PanelBg as MenuBg }`).
2. **UETKX2106 ("one exported name, one file") is retired.** The code number stays reserved
   (family discipline; the sibling retired its UITKX2310 the same way) but is never emitted.
3. **Identity is path-derived.** A component/value/hook/util's compile-time symbol and runtime
   identity derive from (module, path-under-module, file stem, name) — the Unreal translation
   of the sibling's asmdef-anchored file-keyed FQN and of React's module-URL keys.
4. **Renaming/moving a file changes its declarations' identity ⇒ remount** (state reset) — the
   already-documented G-01 semantic for privates now applies uniformly. React Fast Refresh
   behaves the same way (module URL is the key).

Why the current implementation forbids what ES allows: every export today is a **global-scope
C++ symbol**; each module's `.inl`s compile into one aggregated TU
([UetkxDriver.cpp:581-602](../Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxDriver.cpp))
so same-name exports are a C2084, and cross-module same-names are an ODR hazard on the inline
definitions — hence the project-wide `NameToFile` ledger. Privates already got per-file scoping
in the ES-modules campaign (`RuiPriv_<Basename>`, TD-026/M3); exports were left global and the
ledger papered the gap. This plan finishes the job the same campaign started.

---

## §1 — Current state (verified anchors — the complete "assumes global uniqueness" map)

### 1a. Codegen (`Plugins/ReactiveUI/Source/ReactiveUIToolchain/Private/UetkxCodegen.cpp`)

| Mechanism | Anchor | Today |
|---|---|---|
| Private namespace | `PrivNamespaceFor` :1119 | `RuiPriv_<sanitized BASENAME>` — **latent bug: two `Button.uetkx` in different folders collide** (basename-only) |
| Private wrap | `WrapPrivate` :1180 | wraps BOTH phases in the priv namespace |
| Qualification plane | `Qualified` map :2442-2478 | **privates only** → `RuiPriv_<Basename>::` prefix; exported decls emit bare at file scope |
| Alias plane | `FUetkxAliasPlane` :2480-2523 | `Rename` (local→target, incl. resolved default), `NamespaceStrip` (`X::` **stripped** for `* as X` — works only because targets are global) |
| Value-call rewrite (TB-15) | `ValueCalls` :2525-2583 | value refs → `Name()`; target spellings post-alias |
| Two-phase emit | :2593-2650 | modules+values DECL-phase-first; components via `FEmitter` (props struct + defaulted wrapper fwd-decl :2255 + impl) |
| Runtime id emit | :2329-2338 | `RegisterComponentId(FName("<Name>"))` for exported, `FName("RuiPriv_<Basename>::<Name>")` for private |
| Factory emit | :2345-2350 | exported components only: `RegisterNamedFactory(FName("<Name>"))` — short name |
| Hook sig emit | :2339-2340 | `<Name>_RUI_HOOK_SIG` constant + registration keyed by the ComponentId FName |
| Default export | `MarkPublic` :2416-2438 | flips `bExported`; joins the global ledger, emits at file scope |

### 1b. Driver (`UetkxDriver.cpp`) + resolver (`UetkxResolve.cpp`)

- **The 2106 ledger, all producers:** `CheckDrift` :849-874 (`NameToFile`), `CompileAllRoots`
  converged tally :1031-1050, the codemod gate (RUIMigrate path, :338-365 area — "a CROSS-FILE
  UETKX2106 collision that a per-file compile cannot see"), and the skip-path `ExportedNames`
  recovery from the sidecar `exports` array (DRV-1). All die together.
- `BuildAggregators` :581 — per-module grouping via nearest `*.Build.cs`
  (`FUetkxConfig::ModuleRootFor`, UetkxConfig.cpp:70 — "byte-identical to the aggregator's
  grouping walk"); two-phase includes; topo source-truth order; cycle remainder appended.
- `CodegenVersion = 3` (UetkxDriver.h:60); fingerprint `uetkx-codegen-v%d` (:32) — bumping it
  auto-regenerates every `.inl`/aggregator on the next sweep. **This is the whole migration
  story for generated code.**
- `FUetkxResolve::ExportHash` :348 — `name|kind|exported` lines + default marker. Unaffected
  (preamble shape doesn't change); importer staleness keeps working untouched.
- Import validation: 2300/2308/2326/2302/2301 (:638-718); strict usage 2305/2307 (:721-855,
  kind-matched, `FindExporter` + `SuggestSpecifier`); 2304 unused (:920+).
- **UETKX2303 duplicate-import already exists** (UetkxFileScan.cpp:1126 area — "duplicate-import
  diagnostics (UETKX2303); ImportedFrom tracks name → first specifier"). This is the ES-legal
  collision point's diagnostic and it SURVIVES (see §4).
- Free compiler codes: **2328, 2329** (2320–2327 allocated by the ES campaign; verified by
  fixture sweep `Err2321..Err2327` + grep).

### 1c. Runtime (from the 2026-07-24 registry sweep — `RuiNode.h/.cpp`, `RuiReconciler.cpp`)

- Three FName-keyed singleton registries, **all overwrite-on-duplicate, zero collision
  detection**: `PtrToId` (RuiNode.cpp:17, RegisterComponentId :59 — re-registration after Live
  Coding is intentional), `Factories` (:42, `Add` overwrite, **always returns true** :79),
  `FRuiHmrRegistry` (:103-118 — `HookSignatures` + `Overrides`, both keyed by ComponentId FName).
- `RUI::Named` (RuiNode.cpp:82-94): unknown name → **silent `Fragment({})`**. `HasNamedFactory`
  = `Contains`. **No enumeration API exists.**
- **The HMR identity seam is exactly the ComponentId FName**: fibers store it
  (RuiReconciler.cpp:770), override lookup keys it (:438), hook-shape diagnostics print it
  (:524/:542/:552), and the patched module's re-run static initializers re-point the new fn-ptr
  at the SAME FName (RuiNode.cpp:14-16). Change what codegen emits → every layer moves in
  lockstep; nothing else holds the old key.
- `RUI_COMPONENT(Fn)` (RuiNode.h:169-170) stringizes the short fn name — hand-written C++
  components; NOT touched by this plan. Built-ins hard-code short ids (`RuiRoutesComp`
  RuiRouter.cpp:601, `RUI.Suspense` RuiCoreElements.cpp:96) — unaffected.

### 1d. The human-typed name surfaces (the ONLY flat-name UX)

| Surface | Anchor | Behavior today |
|---|---|---|
| `URuiHostWidget.ComponentName` (UPROPERTY) | RuiHostWidget.h:28, .cpp:18/:30-35 | `HasNamedFactory` gate → explicit `[ReactiveUI: '{0}' is not a registered component]` text |
| `URuiActivatableScreen.ComponentName` (UPROPERTY) | RuiActivatableScreen.h:28, .cpp:40-42/:54-55 | same pattern |
| `URuiWorldSubsystem::MountNamed` (BlueprintCallable) | RuiWorldSubsystem.h:25, .cpp:11-17 | same pattern + error log |
| Editor preview | UetkxPreview.cpp:37-80 | name from the SCANNED source; branches exported-vs-private (TD-026) |
| Demo gallery + its lists | RuiDemoScreens.cpp:16-55 | **hand-maintained hard-coded short-name lists** (19 entries), pinned by ReactiveUIDemoTests.cpp:87 and ReactiveUIAcceptanceTest.cpp:95 |

### 1e. Tests that pin identity (update set)

`ReactiveUIUetkxCodegenTest.cpp:579-604` (PrivPair keys + short exported name + no plain `Row`
factory), `ReactiveUIUetkxDriverTest.cpp:337-362` (per-FILE private HMR signatures; file-rename
= fresh id), `ReactiveUIEditorPreviewTest.cpp:81-158` (private-vs-exported preview),
`ReactiveUIHmrShapeResetTest.cpp` (TB-13/17 seams), Demos/Acceptance short-name mounts, plus
~8 suites mounting by `RUI::Named(FName("..."))` literals (Children :40, Cycle :42, WaveG :40,
IncludeRetirement :41, DoomMountedFrame :51). 32 contract goldens in
`Source/RuiHostTests/ContractFixtures/` (all will diff — deliberate re-pin window).

### 1f. LSP (`ide-extensions/lsp-server/src/`)

- Live 2106 mirror: server.ts R16 block (~:947-985, `otherExports` map) + smoke pin
  (scripts/smoke.js "live 2106 OK") + `MIRROR` map entry. All retire.
- `findExporter` (uetkxWorkspace.ts:342-351) — first-wins single hit; consumers: 2310 bail,
  live 2305 (TB-19), code actions. Becomes multi-hit aware (§8).
- `suggestSpecifier` :180 (mirrors `SuggestSpecifier`), `workspaceRelLabel` :450 — unchanged.
- The embedded/clangd virtual TU (virtualDoc.ts) declares imported surfaces per-file
  synthetically — per-file scoping does not change the vdoc contract (it never modeled the
  aggregate TU).

### 1g. Docs claiming short self-registered names

`src/pages/API/ApiReferencePage.tsx:19-21`, `Interop/InteropOverviewPage.tsx:64-73` ("register
under **their own name**", "designer pick a screen from a dropdown"),
`Interop/UmgGuidePage.tsx:5-70`, `Interop/CommonUiGuidePage.tsx:9`,
`Migration/MigrationPage.tsx:5-69`, `Uetkx/ImportsPage.tsx` (2106 + codemod caveat),
`Diagnostics/DiagnosticsPage.tsx` (2106 row), plus `src/docs.tsx` search blobs
(:134/:336/:344-346/:356).

---

## §2 — The family precedent (Unity sibling, read 2026-07-24) → Unreal translation

| Mechanism | Unity (shipping today) | Unreal translation (this plan) |
|---|---|---|
| Anchor | nearest owning `.asmdef` dir (walk stops at `Assets`); no-asmdef → config root | nearest `*.Build.cs` dir = `FUetkxConfig::ModuleRootFor` (already byte-identical with the aggregator walk; `~/` alias + 2308 fences already agree) |
| Namespace formula | `prefix + '.' + sanitized folder segments + '.' + sanitized FILE STEM` (file-keyed: "two files in the same folder no longer share one") | `RuiUetkx::<Module>::<FolderSegs>::<FileStem>` (C++17 nested form), FS-01 |
| Prefix override | `uitkx.config.json namespacePrefix` > asmdef `rootNamespace` > `ReactiveUITK.Uitkx` | DEFERRED (no config override in v1 — no Unreal-side ask yet; slot noted §11) |
| `@namespace` escape hatch | wins in both modes | NOT ported — would be a grammar addition (family-frozen). §11 |
| Sanitization | keep `[A-Za-z0-9_]`, else `_`; leading digit → `_`-prefix; reserved keyword → `_`-prefix; dots in stem → `_` (`AppRoot.style` → `AppRoot_style`); pinned + mirrored on HMR/LSP sides | identical rule, C++ keyword set, ONE helper (FS-01) |
| Export accessibility | `export` → `public`, else `internal` (documented same-assembly softness R11) | privacy stays RESOLVER-enforced (2301) + no factory registration + not qualified-referenced; full-qualification reachability from hand-written C++ is the analogous documented softness (FS-02) |
| Value/util container | per-file `public static partial class __Exports` | not needed — C++ namespaces hold free functions/values directly |
| Import lowering | inside-namespace `global::`-qualified using-aliases / `using static` container / `internal static` forwarding bridges for renamed members | **FQN text-rewrite via the existing Qualified/Rename planes** (FS-03 — deliberate divergence, see rationale there) |
| Runtime identity | Family registry keyed by **FQN string** `{ns}.{Name}` ("two `Button`s must NOT alias"); duplicate FQN silently overwrites; HMR batch guard rejects same-FQN unions | ComponentId/factory FName = the C++ FQN (FS-04); dup-FQN impossible from codegen (path-unique); registration keeps replace-on-re-register (Live Coding) |
| Global export ledger | **NONE.** Only UITKX0113: LSP-only, Warning, component short names, asmdef-scoped | UETKX2106 retired-not-reused (FS-06); optional LSP-only advisory deferred §11 |
| Duplicate binding in one file | UITKX2303 (dup import, Error) + UITKX2325 (alias collision incl. vs local decl) | UETKX2303 survives; add **UETKX2328** import-binding-vs-same-file-decl collision (FS-07) |
| Short-name UX | none at runtime (method groups, compiler-checked); markup tags resolve compile-time by namespace search | Unreal genuinely has string edges (UMG/CommonUI/BP/preview) → suffix resolution + ambiguity surfaces (FS-05) — the one place we must go beyond the sibling |
| Rename-file semantics | FQN changes → new Family → remount | same; uniform G-01 semantic (§7) |

---

## §3 — Decisions (FS-01..FS-10) — owner confirms at plan review

**FS-01 Namespace scheme (locked-by-precedent; AMENDED at M1).** One helper, one truth:
`FUetkxCodegen::FileNamespaceFor(ProjectRelPath, Basename)` → `RuiUetkx::<sanitized path
segments>::<sanitized stem>`.
> **M1 AMENDMENT (executed):** the anchor is the SAME machine-stable relative-path string the
> `#line` mapping already uses (driver: project-relative via `ProjectRelPathFor`; contract
> harness + unit tests: `<Basename>.uetkx`; resolver targets: `LabelForKey`) — NOT a
> `*.Build.cs` filesystem walk. Rationale: zero filesystem access, every caller (codegen,
> driver, contract harness, unit tests, preview, LSP) derives the identical namespace from
> strings it already has, and fixture mode stays machine-independent. The module segment is
> implied by the path (`RuiUetkx::Source::RuiDemo::…`); the 2308 fence still scopes imports.
Original sketch (superseded):
`{ "RuiUetkx", <ModuleName>, <sanitized dir segments under module root>, <sanitized file stem> }`.
Emitted as C++17 nested namespace `namespace RuiUetkx::RuiDemo::Screens::SimpleCounter::SimpleCounter_style { … }`.
Sanitization = sibling rule verbatim (identifier chars, `_` fallback, leading-digit prefix, C++
keyword guard, stem dots → `_`, casing preserved). Example:
`Source/RuiDemo/Screens/SimpleCounter/SimpleCounter.style.uetkx` (module root `Source/RuiDemo`) →
`RuiUetkx::RuiDemo::Screens::SimpleCounter::SimpleCounter_style`. Files directly at the module
root get `RuiUetkx::<Module>::<Stem>`. No `.uproject`-name segment: modules are already unique
per project, and the 2308 fence keeps imports inside one module.

**FS-02 One namespace per file; `RuiPriv_` retires (locked).** ALL of a file's declarations
(exported AND private) emit inside `FileNamespaceFor`. Privacy remains what it already is:
resolver-enforced (2301/2302), tree-shaken (no factory), never qualified-referenced by other
files' generated code. This also FIXES the latent `RuiPriv_<Basename>` same-basename-two-dirs
collision (§1a) and simplifies emission (same-file references need no qualification at all —
the same-file `Qualified` entries for privates disappear). `WrapPrivate` generalizes to
`WrapFileNamespace` applied to every decl's both phases.

**FS-03 Import lowering = FQN text-rewrite, NOT using-declarations (locked, deliberate
divergence from the sibling).** Rationale: (a) the rewrite planes exist and are
battle-tested (privates + aliases + TB-15 value calls ride them today); (b) using-declarations
in the DECL phase have a cycle-ordering hazard (a using-decl names a namespace that may not be
opened yet when the cycle remainder breaks topo order), which C# never had; (c) generated code
doesn't need ergonomics. Mechanics:
- **Named import (aliased or not):** `Qualified.Add(<local>, "<TargetFileNs>::")` (+ existing
  `Aliases.Rename` local→target for the `as` form). The walker already rewrites word-start,
  non-member, non-shadowed occurrences — the same rules that qualify privates today.
- **`import * as X`:** `Aliases.NamespaceStrip` becomes `Aliases.NamespaceMap: X → <TargetFileNs>`
  (rewrite `X::Member` → `<TargetFileNs>::Member`). The TB-15 value-call rewrite composes after
  (namespace-qualified value refs still gain `()`).
- **Default import:** resolver supplies the target name (existing :2499-2514); then the named
  path applies with the target file's namespace.
- **Component tags:** `<B/>` where B is imported emits the qualified wrapper call; the
  two-phase pass needs no new fwd-decls (all DECL phases still precede all BODY phases; markup
  calls live in BODY phase).
- **Member defaults in DECL phase referencing imported values:** already constrained by topo
  order today; the qualified spelling does not change that constraint (M0 probe re-verifies the
  cycle-remainder case).

**FS-04 Runtime identity = the C++ FQN string (locked-by-precedent).** Codegen emits
`RegisterComponentId(FName("RuiUetkx::RuiDemo::…::SimpleCounter"))` and the factory under the
same FName; the hook-signature registration follows the ComponentId as today. The reconciler,
override map, HMR generation machinery need ZERO changes (§1c — single seam). FName length is
fine (1023 cap). **Watch-item:** FName comparison is case-insensitive — two exports differing
only by case in ONE file are already a scanner-level duplicate; across files the C++ symbols
differ but the FNames would collide → M2 adds a cheap driver check (error, allocate UETKX2329)
for case-folded FQN collisions (expected never to fire in practice).

**FS-05 Short names keep working at the designer edges via suffix resolution (proposed
default).** New Core API:
```
namespace RUI {
  enum class EResolveNamed { Hit, Miss, Ambiguous };
  REACTIVEUICORE_API EResolveNamed ResolveNamed(FName NameOrFqn, FName& OutKey,
                                                TArray<FName>* OutCandidates = nullptr);
  REACTIVEUICORE_API void GetRegisteredFactoryNames(TArray<FName>& Out); // first enumeration API
}
```
Resolution: exact key hit → else scan for keys ending in `::<Name>` (ordinal, case-sensitive on
the tail) → exactly one → Hit; none → Miss; several → Ambiguous. `Named`/`HasNamedFactory`
route through it (`Named` on Ambiguous returns `Fragment({})` + one-time Error log listing the
candidate FQNs — never a silent clobber). The four consumer surfaces (§1d) upgrade their
error/design-time texts to show candidates ("ambiguous — use RuiUetkx::…::Foo"). Existing
`.uasset`s holding short `ComponentName` values keep working untouched; the demo gallery's
hand-typed short lists keep working AND become the living proof of the suffix path.

**FS-06 UETKX2106 retired, number reserved (locked).** Both driver producers, the codemod
gate, the sidecar `exports` skip-path recovery, the LSP live mirror + MIRROR entry + smoke pin,
and the docs rows all go. The replacement truth: nothing — cross-file same-name is legal. The
sibling's retired-slot precedent (UITKX2310) is the family pattern for keeping the number.

**FS-07 New diagnostics (proposed).** `UETKX2328`: an import binding (local name or alias)
collides with a same-file declaration name — Error, both compiler + LSP live mirror (the
sibling's UITKX2325 semantics; M0 first verifies FileScan doesn't already catch it).
`UETKX2329`: reserved for the FS-04 case-folded-FQN registry collision check.

**FS-08 `FindExporter` becomes deterministic-multi (locked).** With duplicates legal, both the
C++ `Resolver.FindExporter` (2305 suggestions) and the LSP `findExporter` return the FULL match
set; 2305/auto-import pick the path-nearest exporter (fewest `../` hops from the importer, tie
= lexicographic) and the message appends "(also exported by …)" when >1. The 2310 bail and
completion auto-import consume the same set.

**FS-09 Version + migration (locked).** `CodegenVersion` 3 → 4 (fingerprint regenerates every
committed `.inl`/aggregator on first sweep). Plugin version: **minor bump** (0.13.0) — shipped
codegen output and runtime identity change; extensions patch-bump with the LSP changes. One
release-note MUST-READ: on first load after upgrade, every mounted uetkx component remounts
once (identity change); `.uasset` `ComponentName` short names keep resolving.

**FS-10 Hand-written C++ interop stays registry-mediated (locked).** Direct C++ calls to
generated component symbols were never a documented surface (docs route through
`ComponentName`/`MountNamed`; zero direct-call sites found in the sweep). No global-scope
compatibility using-declarations are emitted. `RUI_COMPONENT` users are untouched.

---

## §4 — Diagnostics delta (complete)

| Code | Today | After |
|---|---|---|
| UETKX2106 | Error, project-wide dup-export ledger (2 driver sites + codemod gate + LSP live mirror) | **RETIRED** — reserved, never emitted |
| UETKX2303 | Error, duplicate import binding (name → first specifier) | unchanged — now the load-bearing ES collision diagnostic; add explicit tests |
| UETKX2305 | "defined in X but not imported — add: import …" (single exporter assumed) | multi-exporter aware (FS-08): nearest-path suggestion + alternates listed |
| UETKX2307 | tag/hook exported by no file | unchanged |
| UETKX2328 | — | NEW: import binding collides with a same-file declaration (Error, compiler + LSP) |
| UETKX2329 | — | RESERVED: case-folded FQN registry collision (driver check, FS-04) |
| LSP live 2106 mirror + smoke pin | exists (TB-14) | deleted; smoke asserts the OPPOSITE (same-name pair publishes NO diagnostic) |
| LSP 2310/2311/2312/2313 + TB-19 2305 mirror | live | unchanged mechanics; 2305 mirror gains multi-exporter |

Bookkeeping that references 2106 and must be edited in the same milestone that retires it:
`plans/HMR_FIELD_TEST.md` (item 10b "copy→2106 trap" becomes "copy → legal; dup IMPORT →
2303"), `plans/OWNER_ACCEPTANCE_CHECKLIST_v2.md:128`, `plans/TECH_DEBT.md` TD-026 (amendment
note), `plans/archive/ES_MODULES_EXECUTION_PLAN.md` G-09 (dated supersede banner: HMR identity
for EXPORTS is now file-qualified), `plans/TESTING_BUGS.md` TB-20 (close with pointer here),
docs pages per §1g.

---

## §5 — Milestones

Engine commands per CLAUDE.md (build → `-run=RUICompile -full`/`-check` → suites headless with
`-ReportExportPath`, parse `report\index.json`). LSP: `npm test` + `node scripts/smoke.js` in
`ide-extensions/lsp-server`, then client bundle rebuild.

### M0 — Probe + audits (no production edits)
1. **Scratch-TU probe** (scratchpad, hand-written mini gen.cpp compiled via the demo project):
   nested C++17 namespaces per "file", two same-named `inline` component wrappers + value fns in
   sibling namespaces, FQN `RegisterComponentId`/`RegisterNamedFactory` strings, a cycle-shaped
   pair with member-default cross-reference under topo + remainder order. Proves every emission
   pattern compiles under UE 5.6 MSVC before touching the emitter.
2. **Audits:** (a) does FileScan already error import-binding-vs-same-file-decl (decides
   UETKX2328's shape)? (b) any consumer of `FUetkxCompileOutput::ExportedNames` beyond the
   ledger? (c) sidecar `exports` array consumers beyond DRV-1? (d) confirm no direct C++ calls
   to generated component symbols outside generated TUs (re-run of the FS-10 sweep on the
   campaign branch).
3. Write the derivation-table unit-test spec (paths → namespaces incl. sanitization edges:
   `.style`/`.hooks` stems, digits, `template`/`class` keyword segments, non-ASCII).
   **Verify:** probe TU compiles + boots; audit notes appended to this file.

> **M0 RESULTS (2026-07-24, executed):**
> - **Probe:** a temporary `FseM0Probe.cpp` in RuiDemo compiled clean (Build.bat Succeeded)
>   with every planned pattern: C++17 nested namespaces, SAME-NAMED `inline` value fns +
>   props structs in sibling file namespaces, a qualified imported-value member default,
>   `namespace T = ::…;` star-import lowering, FQN `RegisterComponentId`/
>   `RegisterNamedFactory` strings, and reopened namespaces across the two phases. File
>   deleted after the check.
> - **Audit (a) — 2303 is keyed on TARGET names** (`RecordNamedImportDups`,
>   UetkxFileScan.cpp:1261-1291: `ImportedFrom.Find(Name)`). Under file-scoped exports,
>   `import { A as B } from "./x"; import { A as C } from "./y";` is LEGAL ES (distinct
>   local bindings of same-named exports) but would false-positive. **M2 re-keys 2303 to
>   LOCAL binding names** (`LocalNames[idx] ?? Names[idx]`), keeping the same code + message
>   shape. Host-include payload dup check unaffected.
> - **Audit (b) — import-vs-same-file-decl is uncovered** anywhere in scan/resolve →
>   UETKX2328 confirmed as planned.
> - **Audit (c) — there is NO sidecar `exports` array.** DRV-1's skip-path recovery is a
>   fresh preamble re-scan (UetkxDriver.cpp:492-524) feeding `ExportedNames`; the whole
>   block retires with the ledger. `FUetkxCompileOutput::ExportedNames` has exactly four
>   consumers — the two driver ledger sites, the skip-path fill, and the codemod gate
>   (RUIMigrateImportsCommandlet.cpp:360) — so the FIELD and its fills retire too.

### M1 — Codegen core
`UetkxCodegen.cpp/.h`: add `FileNamespaceFor` (public static; module root via
`FUetkxConfig::ModuleRootFor`, project-root fallback for fixture mode); wrap every decl's both
phases (`WrapFileNamespace`); retire `PrivNamespaceFor`/`WrapPrivate`; extend `Qualified` to
imported bindings (FS-03) and DELETE the same-file private entries; `NamespaceStrip` →
`NamespaceMap`; registration/factory/hook-sig FQN emission (FS-04); preview's name construction
switches to the shared helper (`UetkxPreview.cpp:55`).
**Tests first (red):** codegen unit derivation table; ExpPair emission (new fixtures
`ExpPairA/B.uetkx` — same-named exported component AND value in both + an importer of A);
PrivPair expectations rewritten to the new namespaces; :37/:67/:387/:393/:519 short-name pins
updated. **Verify:** `ReactiveUI.Uetkx` codegen suite green; `-run=RUIContractDump` regenerates
ALL goldens (32 + new pair) — diff review is the deliverable, committed as the deliberate
re-pin window (ES-campaign M8 precedent).

### M2 — Driver + resolver
Retire both 2106 producers + codemod gate + skip-path `ExportedNames`/sidecar `exports` (per M0
audit); `CodegenVersion` 3→4; `FindExporter` multi + nearest-path `SuggestSpecifier` (FS-08);
UETKX2328 (+ its FileScan/Resolve home per M0); UETKX2329 case-fold check at aggregator build.
**Tests:** driver suite — same-name pair full-sweep 0 errors (RED under today's ledger, the
plan's headline red→green); dup-import 2303 + 2328 pins; DriverTest :337-362 extended to
EXPORTED pairs (independent HMR signatures per file; file-rename = fresh id).
**Verify:** build; `-run=RUICompile -full` then `-check` exit 0 over the demo tree (fingerprint
regenerates all 43); driver + codegen suites green.

### M3 — Runtime registry + consumer surfaces
`RuiNode.h/.cpp`: `ResolveNamed` + `GetRegisteredFactoryNames` + `Named`/`HasNamedFactory`
routing + ambiguous logging (FS-05). Consumers: `RuiHostWidget.cpp`, `RuiActivatableScreen.cpp`,
`RuiWorldSubsystem.cpp`, `UetkxPreview.cpp` — candidate-listing error/design-time texts.
**Tests:** new `ReactiveUI.Core.Registry` (exact/suffix/miss/ambiguous incl. the two-candidate
case via ExpPair), Umg/CommonUI/Editor.Preview suites updated.
**Verify:** those suites + `ReactiveUI.Boot` (never optional).

### M4 — Whole-tree proof
Add a demo collision pair (e.g. `Screens/StyleLab/Panel.style.uetkx` + reuse of an existing
same-named export) proving live coexistence; gallery lists stay short-named (living suffix
proof). Full battery `Automation RunTests ReactiveUI` — Demos/Acceptance/Doom/WaveG/Cycle etc.
mounts keep short literals (suffix path); HmrShapeReset/HmrMemoInvalidate re-green untouched.
**Verify:** full battery green; `ReactiveUI.Bench` numbers to `plans/BENCH_BASELINES.md`
(registry suffix scan is mount-time-only; expect noise-level).

### M5 — LSP + smoke
Delete the 2106 mirror + MIRROR entry; `findExporter` → `findExporters` (FS-08) with 2305
mirror + code action + completion auto-import + 2310 bail consuming the set; 2328 live mirror;
smoke rewrites: same-name pair open in two tabs → ZERO diagnostics; dup-import → 2303; 2328
case; TB-19 pins re-based. Bundle rebuild (`npm run build` both, per `rebuild-ide-extensions`).
**Verify:** `npm test` + smoke PASSED.

### M6 — HMR field verification (owner-driven)
`plans/HMR_FIELD_TEST.md` new section: (a) edit each of two same-named exports live —
independent patches, state preserved per file; (b) rename a FILE with HMR on → remount (the
G-01 semantic, now uniform); (c) 10-series re-run (create/copy/delete/rename) — copy now LEGAL
(no 2106), dup-import path shows 2303. Owner runs; findings → TESTING_BUGS rounds as usual.

### M7 — Docs + release prep
`docs-sync` across §1g pages (scoping model page section in ImportsPage; DiagnosticsPage 2106
retired + 2328/2329; Interop pages: qualified names + ambiguity + fix the "dropdown" claim —
there is no dropdown, but `GetRegisteredFactoryNames` now makes one buildable, note as idea);
`docs.tsx` search blobs; `node scripts/docs-drift.mjs` green. Changelogs: Lane A minor
(0.13.0, root+mirror `cp`), Lane B both extensions, `plans/PENDING_CHANGELOG.md` drained at
release per `release-process`; Discord entry ≤2000 chars. Bookkeeping edits per §4 tail
(TD-026 amendment, G-09 supersede banner, TB-20 close, REMAINING/ROADMAP rows via
`plan-progress`).

### M8 — Ship gate
Full battery + Boot; `-check` 0 drift; contract goldens stable across a double run; corpus
hash UNCHANGED (grammar untouched — assert `f8ae9961…` still matches, tri-repo note:
**no sibling PRs needed**, this is Unreal-internal semantics); `verify-mirror`/`check-headers`/
`lint-skills`/`docs-drift`/`changelog verify` all green; packaged-plugin sanity if the release
follows immediately (`package-plugin.ps1 -StrictIncludes` + packaged-fidelity boot).

---

## §6 — Test matrix (new + rewritten)

| Area | Test | Asserts |
|---|---|---|
| Derivation | codegen unit (new) | path→namespace table incl. sanitization edges; helper is the ONLY producer (preview parity call) |
| Emission | ExpPairA/B goldens (new) | same-named exports in sibling namespaces; FQN registrations; importer's qualified rewrites |
| Emission | PrivPair goldens (rewrite) | privates in the FILE namespace; no `RuiPriv_`; exported wrapper FQN-registered |
| Ledger death | driver (new, red→green) | same-name pair: full sweep 0 errors, `-check` 0 |
| Collisions | FileScan/Resolve (new) | 2303 dup-import pins; 2328 import-vs-decl; 2329 case-fold check |
| Registry | Core.Registry (new) | exact/suffix/miss/ambiguous; enumeration; replace-on-re-register preserved |
| HMR identity | DriverTest ext + HmrShapeReset/MemoInvalidate | exported per-file signature independence; file-rename remount; TB-13/17 seams unmoved |
| Surfaces | Umg/CommonUI/Preview | short-name resolve + ambiguous candidate text; private-preview message intact |
| Whole tree | Demos/Acceptance/Battery | 19 gallery short names resolve; collision demo coexists; 131+ green |
| LSP | npm 91+ + smoke | no-2106 assertion; multi-exporter 2305; 2328 mirror; TB-19 re-based |

---

## §7 — Semantics & migration (user-facing contract)

- **State/HMR:** identity = (file path, name). Editing a file in place preserves state exactly
  as today (FQN stable). Renaming/moving a file (or renaming the export) remounts its
  components — uniform G-01; React-parity (Fast Refresh keys by module URL). Document beside
  the existing divergences table (MASTER_PLAN §5 pointer).
- **Upgrade:** open project → fingerprint mismatch → full regenerate → one-time remount of all
  uetkx-mounted UI. `.uasset`/BP `ComponentName` short names keep resolving unless the project
  ACQUIRES a genuine duplicate — then the surface says "ambiguous" with the qualified options.
- **Grammar/corpus:** unchanged; no codemod; sibling repos unaffected (their scoping already
  matches — this closes the family-parity gap on the Unreal side).

## §8 — Risks

| Risk | Mitigation |
|---|---|
| Golden churn hides a real regression | M1 lands tests-first; ContractDump diff reviewed file-by-file before re-pin (ES-M8 precedent) |
| Cycle remainder breaks a qualified DECL-phase reference | FS-03 chose rewrites over using-decls precisely for this; M0 probe includes the cycle case |
| FName case-insensitive collision | UETKX2329 driver check (M2) |
| Suffix match surprises (short name suddenly ambiguous after adding a file) | Ambiguity is loud everywhere (log + on-widget text + preview message); never silent clobber |
| Preview/codegen derivation drift | single shared helper + a parity unit test that calls both paths |
| Live Coding continuity across the upgrade boundary | first patch after upgrade re-registers NEW FQNs while stale fibers hold old ids → the M4 battery + M6 field item (a) cover; worst case = one editor restart, release-noted |
| Watcher/standing-error bookkeeping | keyed by file path (§1c sweep) — unaffected |

## §9 — Out of scope / deferred

- `@namespace`-style override directive (grammar change → family-coordinated, only if a real
  user need appears).
- Config-file namespace prefix (sibling parity item; no Unreal ask yet).
- An LSP-only advisory (sibling UITKX0113-style Warning) for same-MODULE same-name components
  as a courtesy hint — explicitly optional, decide post-field-test.
- A designer dropdown for `ComponentName` fed by `GetRegisteredFactoryNames` (now buildable;
  separate feature).
- Re-exports (`export { a } from "./x"`) — still deferred family-wide (ES campaign note).

## §10 — Rollback

Single campaign branch; no grammar/corpus movement; goldens re-pin in one commit window.
Reverting the branch restores CodegenVersion 3 and the ledger wholesale. Nothing ships until
the full M8 gate is green.
