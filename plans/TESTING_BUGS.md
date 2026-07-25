# Testing bugs — owner IDE-testing session (2026-07-17)

Bugs found by the owner manually testing the VS Code extension (colors → formatting →
IntelliSense passes) against the demo tree, with root causes from tokenizer-harness evidence
(`scratchpad/tmtest` — the real vscode-textmate engine over the shipped grammar). Fixes land on
the extension side (grammar + language config + formatter); every grammar edit syncs the VS2022
copy byte-identically, every formatter edit keeps C++ ↔ LSP parity via
`ide-extensions/lsp-server/test-fixtures/uetkx-formatter-cases.json` (shared by the C++
`ReactiveUI.Uetkx.Formatter` suite and the LSP replay test — NOT family-core-hashed, so cases
may be updated freely).

## TB-1 — Angle brackets: red "unmatched" brackets + generics parsed as markup tags

**Repro:** `AcceptanceLab.uetkx` (brackets "off" everywhere), `CustomDraw.uetkx`
(`UseMemo<TSharedPtr<FRuiDrawFn>>` — `<>` wrong), `DoomGameScreen.uetkx` /
`DoomGame.uetkx` / `DoomHUD` usage (cascading red `>` / `/>`).

**Two independent root causes:**
- (a) `language-configuration.json` declares `["<", ">"]` a **bracket pair** — VS Code's
  bracket matcher then tries to pair every `<`/`>` including comparisons, `->`, and `>>`
  generic closers, painting "unmatched" ones red. JSX/TSX deliberately omit angle brackets
  from `brackets` for exactly this reason.
- (b) The grammar's `tag-open` rule fires on ANY `<Identifier` inside code —
  scope-dump proof: in `TSharedPtr<FRuiDrawFn>&`, `<FRuiDrawFn>` tokenizes as
  `meta.tag.open` with `FRuiDrawFn` as `support.class.component`; in
  `UseMemo<TSharedPtr<`, `TSharedPtr` becomes a tag and `FRuiDrawFn` an
  `entity.other.attribute-name`.

**Fix:** (a) drop `<`,`>` from `brackets` + `surroundingPairs`; (b) guard the tag begin:
negative lookbehind (no identifier char / `)` / `]` before `<` — kills generics), mandatory
tag name, and a post-name lookahead requiring tag-ish continuation (`>`, `/>`, attr, or EOL —
kills `A < B` comparisons). **Status: FIXED (both grammar copies + language config).**

## TB-2 — `key=` attribute colored as keyword, not attribute

**Repro:** `DoomGame.uetkx` — `key="menu"` renders blue while sibling attributes are
attribute-colored.

**Root cause:** `key`/`classes` carry `keyword.other.attribute-name.structural` — the
`keyword.other` prefix wins in every theme.

**Fix:** rescope to `entity.other.attribute-name.structural.uetkx` (structural distinction
kept in the sub-scope, renders like an attribute). **Status: FIXED.**

## TB-3 — Raw text children colored as code

**Repro:** `RouterUser.uetkx` (`Back home` blue, `+1` green-numeric), `SignalCounter` /
`ClickCounter` button labels, `CustomDraw.uetkx` (`Shuffle(bump RedrawKey)` label renders
like a function call — the owner read it as code; it is a Button LABEL, line 73).

**Root cause:** the grammar has no children region at all — a tag's open region ends at `>`,
so raw text children fall through to `code-block-body` and tokenize as C++ (identifiers →
variables, `1` → numeric, `Shuffle(` → function call).

**Fix:** restructure `tag-open`/`tag-close` into a `tag-element` region spanning
`<Name …>` … `</Name>` (the TSX jsx-element approach): an anchored attributes sub-region up
to the open tag's `>`, then children patterns (nested tags recurse; `{ }` expressions,
`@`-directives and comments still tokenize) with a raw-text fallback that carries NO scope —
default foreground. **Status: FIXED (both grammar copies).**

## TB-4 — Formatter explodes short text children to 3 lines

**Repro:** `ClickCounter.uetkx` — `<Button OnClicked={ … } …>+</Button>` (fits the print
width) reformats to open-tag line / `+` line / `</Button>` line.

**Root cause:** the formatter always puts element children on their own lines; there is no
single-short-text-child inline form.

**Fix:** when an element's only child is raw text and the whole
`<Tag attrs>text</Tag>` fits `printWidth`, keep it inline. Implemented byte-identically in
`UetkxFormatter.cpp` and `formatUetkx.ts`; corpus case added to
`uetkx-formatter-cases.json` (both suites replay it). **Status: FIXED.**

## TB-5 — No dead-code dimming after an early `return null;`

**Repro:** `MvvmDemo.uetkx` — owner added `return null;` at the top of the setup body; the
statements below it do not gray out.

**Fix (shipped):** the family diagnostic — **UETKX0107 "Unreachable code after 'return'."**
(Unity ships the same rule as UITKX0107). Both scanners detect the FIRST top-level `return` of
a component body — a markup `return ( … )` *or* a plain C++ `return x;` (the collector never
sees those) — and hint-diagnose everything after it; the server attaches the LSP `Unnecessary`
tag so the editor FADES the range natively. Corpus-pinned (3 fileScanLeg cases: early plain
return, code after the final return, conditional return = no diagnostic). **Status: FIXED.**

## TB-6 — "Where does `Shuffle(bump RedrawKey)` come from?" (question, not a bug)

It is the Button's raw-text label at `CustomDraw.uetkx:73`. It LOOKED like code because of
TB-3 (labels tokenized as C++), which made `Shuffle(` render as a yellow function call.
Fixed by TB-3's children-text region; the label now renders plain. **Status: ANSWERED.**

## TB-7 — Parens inside raw-text children colored blue by the bracket-pair colorizer

**Repro:** `CustomDraw.uetkx` — the Button label `Shuffle(bump RedrawKey)`: text renders plain
(TB-3 ✓) but the `(` `)` render blue.

**Root cause:** VS Code's bracket-pair colorization runs OUTSIDE TextMate — literal brackets in
any non-string/non-comment token get depth colors, and a text child can't be scoped as
string/comment without tinting it. The Unity sibling ships
`"editor.bracketPairColorization.enabled": false` in its `[uitkx]` configurationDefaults for
exactly this reason (its screenshots' plain white brackets).

**Fix:** mirror the sibling — `[uetkx]` configurationDefaults now disables bracket-pair
colorization. **Status: FIXED.**

## TB-8 — Unreal editor: compile errors in the Message Log are not clickable (OPEN)

**Repro (owner question):** "In Unity you can go to the file that has errors from the console —
how does it work for Unreal?" Today: `UetkxWatcher.cpp` logs errors via
`FMessageLog::Error(plain FText)` — the ReactiveUI Message Log page shows the text but clicking
does nothing; the owner must find the file manually. (VS Code-side this already works — the
Problems panel jumps to file:line via the LSP.)

**Fix (shipped):** tokenized messages — each error row carries a `→ File(line,col)` action
token that opens the `.uetkx` at the error position: `code --goto file:line:col` when VS Code
is on PATH (probed once), else the OS default app. The watcher now passes structured diags
through, and line/col derive from the scanner's code-point offset. **Status: FIXED.**

## TB-9 — Host includes: wrong header name gives no squiggle, no go-to-definition

**Repro:** `CommonUiDemo.uetkx` — `import "@RuiDemoSupport.h"` / `import "@DemoInteropWidgets.h"`:
misspell the name → no diagnostic; click → no definition.

**Root cause:** host includes are nameless BY DESIGN (the C++ compiler owns the symbols), so
the resolver skips them entirely — but the LSP has workspace visibility the compiler lacks:
project headers under `Source/`/`Plugins/*/Source/` are findable. `UETKX2316` was reserved for
exactly this (docs said post-v1 because the COMPILER can't see UBT include paths — the LSP
partially can).

**Fix:** workspace header index (basename+suffix match) → go-to-definition on the specifier;
`UETKX2316` (error, LSP-only — owner call 2026-07-17) when a `/`-less specifier resolves to nothing in the workspace
(slashed specifiers stay undiagnosed — they're usually engine paths the LSP can't verify).
Docs updated (2316 no longer "reserved"). **Status: FIXED.**

## TB-10 — Embedded C++ intelligence completely dark (no diagnostics, no completion, weak hover)

**Repro (owner):** mangled `CommonUiDemo.uetkx` setup wholesale (`UseaState`,
`ProvideContesxt`, `Staste.`) — zero squiggles; no completion in C++ regions; hover far below
the Unity sibling.

**Root causes (three, stacked):**
1. **No clangd on this machine** — not on PATH; VS 2022 ships only clang-format/clang-tidy
   (clangd is a separate component). The channel degraded to markup-only exactly as designed —
   but SILENTLY, which is the real bug.
2. **No `compile_commands.json`** — never generated for this project (UBT
   `-mode=GenerateClangDatabase`), so even a present clangd would parse blind.
3. **Diagnostics were never forwarded** — `clangdProxy.ts` wires completion/hover/definition
   but has no `publishDiagnostics` handling, so C++ typos can't squiggle even with 1+2 solved.

**Fix:** (1) clangd autodiscovery (PATH → `uetkx.clangd.path` setting → common install
locations) + a one-time visible warning naming what's missing and how to get it; (2)
`GenerateClangDatabase` runbook + `findCompileCommands` covering the generated location; (3)
forward clangd diagnostics for virtual docs through the source map into the real document
(prelude-range diagnostics dropped). **Status: FIXED (channel verified live end-to-end).**

## TB-11 — Hover too thin (markup and C++) vs the Unity sibling

**Repro (owner):** hover on tags/attributes shows less than ReactiveUIToolkit's.

**Fix:** hover markdown rebuilt from the full schema row — element hover: Slate class, module,
description, slot model, `sinceUE`; attribute hover: type, which setter/property/delegate it
maps to (D-33 loyalty means the Unreal name IS the doc pointer), style-key/slot-key/event
classification, enum values where the schema pins them. C++ hover rides TB-10's clangd
channel. **Status: FIXED.**

## TB-12 — Markup as a VALUE (`auto X = (<VerticalBox>…);`) — silently unsupported

**Repro (owner):** `ClickCounter.uetkx` — assigned a markup tree to a local, embedded it with
`{ SomeComponent }`; nothing diagnosed it, nothing formatted it, and the generated C++ would
hand raw markup to MSVC (a brutal, misattributed compile error at build time).

**Root cause:** the grammar only admits markup in return position (family-wide — the Unity
sibling's samples never use markup-as-value either); everything else in a body is verbatim
C++. The gap is that violating this produced NO friendly diagnostic.

**Fix now:** `UETKX0114` (error, both scanners, corpus-pinned): "markup is only legal in a
`return ( … )` — markup-as-value is not supported; extract a component instead". Full
markup-as-value support recorded as **TD-032** (family RFC — same lane as TD-031's
`<Provider>`).

**RESOLVED by the markup-everywhere campaign (2026-07-17,
`plans/MARKUP_EVERYWHERE_PLAN.md` §4):** markup-as-value is now FIRST-CLASS — it lowers in
place at every family boundary position (assignment, call argument, ternary, short-circuit),
the LSP lifts it (typed `__rui_rn` placeholder + mapped inner expressions), AcceptanceLab §10
demos it, and the corrected owner repro (`auto X = (<VerticalBox>…); … { X }`) compiles and
renders. `UETKX0114` NARROWED to the one still-illegal spelling: a paren-less statement-level
markup return (`return <Tag/>;`). Rules-of-hooks landed alongside (UETKX0013-0016 — hooks are
top-level-body-only, never inside directives or markup). **Status: FEATURE SHIPPED; 0114
narrowed; TD-032 closed.**

## Decision — UETKX3007 stays (owner, 2026-07-17)

React-style branched finals (`if/else` in plain C++ each returning markup, no top-level final
return) stay REJECTED. The endorsed spelling is the markup directive inside one top-level
return — `return ( @if (cond) { return (…); } @else { return (…); } )` (DoomGame.uetkx is the
living example). Rationale: the setup body is verbatim, unparsed C++ — proving branch
exhaustiveness there means real C++ flow analysis, while `@if/@else` is grammar the compiler
enforces structurally. Owner reviewed and accepted the `@if` form; no grammar change.

---
*Verification per fix: tokenizer harness scope dumps (before/after) on SimpleCounter,
AcceptanceLab, CustomDraw, DoomGameScreen, RouterUser; formatter corpus replay green in BOTH
implementations; format sweep idempotent over all 56 committed `.uetkx`; battery unaffected
(formatting is `.inl`-insensitive — proven by `RUICompile -check` = 0 drift after the first
save-format wave).*

## TB-13 — **CRITICAL**: HMR v2 lost the family reset-on-hook-shape-change rule (silent state corruption)

**Found:** 2026-07-21, owner question during the HMR field-test session ("don't React / the
siblings reset when hook order changes?") — before hitting it live.

**Repro (any component, HMR active):** drive hook state to non-default values, then save an
edit that CHANGES THE HOOK LIST (add/remove/reorder a `UseState`/`UseMemo`/… call). After
the Live Coding patch, cells are read positionally by the NEW hook order: hooks silently
inherit a NEIGHBOR'S value (plausible-looking wrong state), or read type-punned cells. Only
signal: `[Hooks][order]` log lines, and only when `rui.HookValidation` is on.

**Root cause:** the family rule — "state preserved on stable hook shape, RESET on a real
shape change" — is stated in `plans/archive/HMR_V2_PLAN.md` (line ~122) and was implemented
in HMR v1 by the INTERPRETER: it computed an AST hook signature and drove
`SetComponentOverride(bResetState)` → `FRuiComponentState::HmrResetHooks()`. HMR v2 deleted
the interpreter and nothing inherited the job: `RegisterHookSignature`/`FindHookSignature`
(RuiNode.h) have no production callers left (one driver unit test simulates them), and the
v2 patch path (`HmrRefreshAll`) only dirties fibers — no signature compare, no reset. A
regression by omission, violating the v2 plan's own text; the worst failure class (silently
wrong values that look plausible).

**Fix direction (agreed):** codegen computes each component's hook-shape signature (kinds +
order of hook calls, which it already parses) and emits `RegisterHookSignature(Id, Hash)` in
the generated registration; component state stamps the signature it was created under; on
re-render after a patch, mismatch → `HmrResetHooks()` (exists, orphaned) + a MessageLog
"hook shape changed — state reset" line. Pins: driver signature test (half-exists at
`ReactiveUIUetkxDriverTest.cpp:344`), a reconciler reset test, and an HMR_FIELD_TEST.md
matrix item replacing the "re-enter the screen after hook edits" guidance.

**Status:** FIXED (code complete 2026-07-21, engine build + suite run pending editor close):
runtime detection — the HMR controller arms `RUI::SetHmrHookTracking` for the session and
bumps `RUI::BumpHmrGeneration()` on every patch-complete; the reconciler records the
flattened hook sequence per render and, when it differs ACROSS a generation boundary,
runs `HmrResetHooks()` (effect cleanups included) + re-renders clean, logging
`[ReactiveUI][HMR] <Comp>: hook shape changed by the edit (N -> M hooks) — state reset`.
A shape change without a generation bump stays the rules-of-hooks user error. Pinned by
`ReactiveUI.Hooks.HmrShapeReset` (preserve-on-stable / reset-on-change / no-reset-without-
boundary). v2 amendment (2026-07-24): the misaligned render is now MEMORY-SAFE — per-cell TypeHash +
tail truncation at every accessor (the v1 render-then-detect design crashed the editor:
a State cell destructed as a Memo cell). See F5_FIELD_TEST_BUGS round 16 for the full
design; the pre-render (codegen-signature) variant remains the post-v1 refinement. Field note
(2026-07-24): applying THIS fix via Ctrl+Alt+F11 into a live editor crashed it —
FRuiComponentState gained a field, and Live-Coded code read the new layout on old-layout
objects (plus LNK2019 on the new cross-module RUI:: exports). The fix itself is sound;
it simply requires the full rebuild, like any layout/API change.

## TB-14 — 2106 invisible live + the standing-error toast storm (the style-extraction session)

**Found:** 2026-07-21, owner HMR session: extracting SimpleCounter styles into a
`.style.uetkx` companion created a second `PanelBackground` export. The LSP validated the
file CLEAN; the watcher's next sweep hit UETKX2106 ("one exported name, one file" — the
driver's global NameToFile ledger, the locked ES-modules design) and Live Coding failed
with no editor-side hint. Meanwhile the "Recent Errors" panel re-rowed `save — 1 error(s)`
/ `stale-poll — 1 error(s)` on every sweep — every save of ANY file, the 10s stale-poll,
and every alt-tab (activation poll) — and the Errors counter summed the same standing
error forever.

**Root causes:** (a) the LSP never implemented the 2106 ledger — it's a full-sweep-only
check in the driver, so the one rule that spans FILES was invisible exactly where files
get created; (b) `FUetkxHmrController::NotifyCodegen` inserts a Recent-Errors row and
broadcasts unconditionally per erroring sweep, and `Status.Errors += N` is a lifetime sum.

**Fixes (code complete 2026-07-21, LSP verified; editor leg builds with TB-13):**
- LSP live UETKX2106 mirror: the open doc's export surface vs every other swept file's
  cached decls (project-rooted only; guarded so the mirror can never kill the publish).
  Smoke-pinned with the exact session shape (duplicate flags as-you-type, unique clean).
- Controller: identical consecutive error reports coalesce into the newest row as
  `(still failing ×N)`; `Status.Errors` now means CURRENT standing errors; recovery
  resets the coalescing state.
- The demo collision itself: `CounterPanelBackground` (renamed; the session unblocker).

**Status:** LSP half VERIFIED (91/91 + smoke); editor half pending the next engine build.

## TB-15 — value-export edits are invisible to HMR (Live Coding never re-runs global initializers)

**Found:** 2026-07-24, owner HMR session. Editing `SimpleCounter.style.uetkx` values
(paddings 12→30 etc.) produced clean sweeps, successful Live Coding patches, refreshed
roots — and ZERO visual change. Control experiment in the same session: a CODE edit (the
UseMemo title string) patched visibly; the DATA edits never did. Editor restart shows the
new values (fresh static init) — the classic signature.

**Root cause:** value exports lower as `inline const T Name = Init;` — namespace-scope
globals whose dynamic initializers run ONCE at module load. Live Coding's documented
limitation: changes to global/static initializers do not take effect on patch (data
symbols keep their original storage; only code is redirected). So the entire value-export
surface — the styling companion pattern we just introduced to the demos — is dead under
HMR by construction.

**Fix direction (proper, code-not-data):** lower value exports as inline FUNCTIONS
returning by value (`inline T Name() { return Init; }`) — a patch then replaces the CODE
that produces the value, which Live Coding handles perfectly; references get `()` appended
by the emitter's existing identifier-rewrite pass (the PrefixHooks walker already rewrites
hook identifiers in every emitted code region; BodyLocals oracle prevents shadowed-local
rewrites). Consequences owned: value exports are VALUES, not lvalue globals (`&Name` stops
compiling — acceptable, documented); per-reference construction cost is trivial at UI
scale; committed .inls regenerate tree-wide; the LSP virtual doc mirrors the new decl
shape so clangd agrees.

**Status:** FIXED in code (2026-07-24; engine regen/build/ladder pending editor close).
Value exports now lower as `inline <T> Name() { return <Init>; }`; every reference is
rewritten to a call by the PrefixHookCalls walker (new ValueCalls branch — same-file
values any visibility + imported names resolving to VALUE decls incl. default and
namespace-star, shadowed locals suppressed, existing call forms untouched, private names
compose with RuiPriv_:: qualification). The LSP virtual doc is deliberately UNCHANGED —
it mirrors SOURCE semantics (values are values to the author; lifted exprs are verbatim
source). Owned consequences: `&Name` no longer compiles (values are prvalues now);
legacy `module` bodies (verbatim C++, deprecated 2320) do not get the rewrite. Codegen
pins updated (inline FUNCTION shapes). Full-battery fallout, all legitimate
expectation updates (no gate weakened): the private-shadow re-qualify pin gains the call
suffix; contract goldens regenerated (32/32 — the .expected diffs are exactly the
function-form lowering); the Acceptance sweep count moves 42→43 (SimpleCounter.style
joined the tree); Demos/Umg pins follow the two-counter screen text (Count1:); the
shipped-schema drift pin now excludes `brushNames` (the R13 environment set differs
between the automation process and the editor that exported the shipped copy — static
vocabulary still byte-compared, presence of the brush set still asserted).

## TB-16 — diagnostic-surface polish (owner matrix-item-5 observations, 2026-07-24)

Item 5/6 PASSED (all three gates fire live + in the sweep; UI keeps the last good patch;
recovery costs nothing). Three cosmetic findings while eyeballing it:

- **(a) 1-character squiggles from sidecar diags.** `FUetkxDiag.Length` defaults to 1 and
  NO `Fail()` site sets it — every compiler diag reaches the editor as a 1-char range,
  while the live LSP diag for the same problem highlights the full token. Fix: set Length
  at the Fail sites (attr-name / value token length) — mechanical sweep.
- **(b) the same finding reports twice with two codes.** The hash-matched sidecar surfaces
  the compiler's UETKX0106 (no suggestion, 1-char) NEXT TO the live UETKX2311 (full range,
  did-you-mean). One problem, two rows. Fix: the LSP suppresses a sidecar diag whose
  offset falls inside a live diag of the mirrored rule family (0106↔2311, 2106, 0112 —
  the live one is strictly richer).
- **(c) the "standing compile error" console line repeats on every sweep** while broken
  (the HMR window coalesces correctly since TB-14; the OUTPUT LOG line doesn't). Fix:
  first report at Error, repeats at Verbose.

**Status:** FIXED (2026-07-24): (a) `Fail()` carries token lengths — every R10+ compile
diag now spans the attr name / value literal instead of one char; (b) the LSP suppresses a
sidecar diag whose range overlaps a live diag of the mirrored family (0106↔2311/0105 etc.
— the live copy is strictly richer); (c) the standing-error console line reports ONCE per
file at Error, repeats at Verbose, re-arms on recovery.

## TB-17 — memo caches serve PRE-PATCH values after an HMR refresh (stale derivations)

**Found:** 2026-07-24, owner matrix item 8: editing the STRING inside a
`UseMemo<FString>(factory, RUI::Deps())` produced a successful patch + refresh and NO
visual change — the memo cell returned the value cached at mount; empty deps never
recompute. Not an on/off-cycle bug (reproduces in plain HMR too); React's Fast Refresh
shares the gotcha, but the family rule should be COHERENT: **preserve state, recompute
derivations** — a patch changed the code that derives the value, so derived caches must
follow it (the TB-15 lesson at the hook level).

**Fix direction:** on the HMR refresh path (generation bump), MEMO-family cells
(`TRuiMemoCell`, `TRuiDeferredCell` deps) invalidate their cached deps (unset → DepsChanged
→ factory re-runs on the refresh render). `UseState`/`UseRef`/reducer cells stay untouched
(user data). Headless-testable (same harness as HmrShapeReset).

**Status:** FIXED (2026-07-24): `IRuiHookCell::HmrInvalidateDerived` — at the first
render after a patch (generation boundary), memo-family cells (Memo/Deferred) unset their
remembered deps so the freshly patched factory re-runs; State/Ref/Reducer untouched.
"Preserve state, recompute derivations." Pinned by `ReactiveUI.Hooks.HmrMemoInvalidate`
(cache holds without a boundary — plain React semantics; recomputes across one; state
survives the same refresh).

## TB-18 — importers kept stale "No problems" after an exporter edit (cross-file truth, single-file refresh)

**Found:** 2026-07-24, owner matrix 10e: renamed an export in `SimpleCounter.style.uetkx`;
the open importer's Problems stayed EMPTY (stale — computed before the rename) while the
compiler sweep correctly errored (2106 + 2302 + 2304 in the log). Touching the importer
(the misspell) triggered fresh validation and the diagnostics "suddenly" appeared — which
read as the checker being flaky when it was actually never re-run.

**Root causes:** (a) validation is per-document — nothing re-validated OTHER open docs
when a file changed, though an exporter edit changes their truth (2302/2307/2106/0105);
(b) cross-file resolution read DISK (mtime cache) — a dirty exporter buffer was invisible
even to a fresh validation.

**Fixes (LSP-only, 2026-07-24, bundle rebuilt):** (a) `onDidChangeContent` re-validates
every other open doc, debounced 150 ms; (b) the N-04 `TextOverlay` (dirty open buffers)
now threads through `getDecls`/`defaultExportOf`/`findExporter`/`resolveDiagnostics` and
validate's 2106 sweep + component-param resolution — live truth is the union of open
buffers over disk. Smoke pin: rename an export via didChange only (disk untouched) → the
UNTOUCHED importer flags 2302; rename back → it clears, still untouched.

**Status:** FIXED (91/91 + smoke; reload the dev host to pick up the bundle).

## TB-19 — a usage bound by NOTHING stayed silent (misspelled/deleted import, orphaned identifier)

**Found:** 2026-07-24, owner session (post-10e state): with the style export renamed AND the
import misspelled (`CosunterPanelBackgsround`), the usage
`BorderBackgroundColor={ CounterPanelBackground }` showed NO error — nothing bound the name,
only the import line was flagged (2302). "Should also have an error because nothing with this
name is defined anywhere."

**Root causes:** (a) UETKX2305 (strict usage: exported-elsewhere-but-not-imported,
UetkxResolve step 2) was sweep-only — the LSP shipped the code ACTION for it (the quick-fix
parses the `add: import { X } from "spec"` tail) but no live producer ever existed, so
deleting or misspelling an import left every usage undiagnosed until the next compile;
(b) the 2310 near-miss lint measured unresolvable references against LOCALS only — import
names (the nearest neighbor in this session) and same-file decl names were never candidates.

**Fixes (LSP-only, 2026-07-24, bundle rebuilt):** (a) live UETKX2305 mirror sharing the
TB-14 2106 sweep's export map — kind-matched like the compiler (a call-shaped ref never
2305s against a value export), compiler-identical message (the quick-fix lights up), sidecar
copy suppressed via the TB-16 MIRROR map; a name exported by NO file stays undiagnosed (may
be ambient C++ — clangd's judgment, compiler rule A4). (b) 2310 candidates now: locals, then
the file's import names, then same-file decls — "did you mean the import
'CosunterPanelBackgsround'?" lands directly on the orphaned usage. Import intact but exporter
renamed stays SINGLE-SOURCE (2302 on the import line only — the import binds the name; same
as TypeScript).

**Status:** FIXED (91/91 + smoke incl. the three-scenario pin: import misspelled while still
exported → 2305 with add-import tail, 2310 defers; exporter also renamed (the owner's exact
session) → 2310 toward the broken import, no false 2305; import line deleted → 2305; restore
→ clean. Reload the dev host to pick up the bundle).

## TB-20 — OPEN (design): exports are GLOBALLY scoped — "one exported name, one file" contradicts the ES-module design

**Found:** 2026-07-24, owner: renaming SimpleCounter.style's export to `PanelBackground`
errored UETKX2106 against ContextDemo.style.uetkx. Owner: "this is not how it was designed —
every file is its own module, exactly like React." In ES modules, same-named exports in
different files are NORMAL; importers disambiguate (and `as` aliasing exists for the one file
that wants both).

**What the code actually does:** the ES-modules campaign gave PRIVATE decls per-file scoping
(`RuiPriv_<Basename>` namespace, file-qualified runtime identity — TD-026). EXPORTS were left
as global-scope C++ symbols: each module's `.inl`s compile into one aggregated TU (C2084 on
same-name), and same-name across modules is an ODR hazard (global-scope inline definitions),
hence the driver's project-wide NameToFile ledger. UETKX2106 is the guardrail that surfaces
the collision early — honest about the implementation, but the implementation contradicts the
module semantics. The shortcut is RECORDED: TD-026's resolution ("exported components keep the
short name — 2106 ledger guarantees uniqueness") and ES_MODULES_EXECUTION_PLAN G-09 baked it
into HMR identity.

**Proper fix (campaign-sized):** per-file namespaces for EXPORTS too — emit every file's
exports inside a file-derived namespace (derived from the project-relative path, not the bare
basename: `RuiPriv_<Basename>` already carries a latent same-basename-two-dirs collision);
importers lower to using-declarations/alias rewrites per their import lists (the alias plane
exists); runtime registry keys become file-qualified for exports as they already are for
privates. Ripples: CodegenVersion bump, 32 contract goldens, HMR identity (G-09),
RegisterNamedFactory/`RUI::Named` consumers, 2106 retired or reduced to real collisions, LSP
mirror, docs, migration codemod.

**Status: PLANNED (2026-07-24)** — owner directive: "exactly like react, es modules … scale
doesn't matter, this must be right." Full investigation done (this repo's codegen/driver/
runtime/LSP + the Unity sibling's shipping mechanics as family precedent) and written up as
**[plans/FILE_SCOPED_EXPORTS_PLAN.md](FILE_SCOPED_EXPORTS_PLAN.md)** — per-file C++ namespaces
for ALL decls, FQN runtime identity, suffix resolution at the designer edges (short names keep
working; ambiguity is loud), 2106 retired-not-reused, 2303 becomes the load-bearing collision
diagnostic. The earlier a/b/c question dissolved: the family answer (React + Unity sibling) is
file-qualified identity everywhere with short-name convenience only at human edges.

> **TB-20 EXECUTED (2026-07-24):** the FILE_SCOPED_EXPORTS campaign shipped on this branch —
> per-file flat namespaces for every decl (`RuiUetkx_<path>_<stem>`), FQN runtime identity with
> short-name suffix resolution at the designer edges, UETKX2106 retired (UETKX2329 case-fold
> check is the one survivor), UETKX2303 re-keyed to local bindings, CodegenVersion 4. Evidence:
> full battery 132/0, LSP 91/91 + smoke, `-check` 0 drift, the demo tree carries the owner's
> original `PanelBackground` rename as the living same-name proof. Owner-side remainder:
> HMR_FIELD_TEST items 5b + 10b (same-name live) and the M6 legs.

## TB-21 — Live-Coding-patched Core split the registries (PIE-stop AV + fresh-boot gallery assert)

**Found:** 2026-07-25, owner running field-test 10c/10d WITHOUT HMR: (a) EXCEPTION_ACCESS_VIOLATION
at PIE-stop (ReleaseFiberTree destroying a pre-patch tree), (b) after an editor restart,
Ctrl+Alt+F11 then Play asserted `IsValid()` in the gallery's `WithSlot` at first render. Both
stacks show `ReactiveUICore_patch_0` — Core got Live-Coding-patched with ZERO Core source
changes (the strict-includes experiment's `-DisableUnity -NoPCH` run left UBT action state that
made the in-editor compile rebuild Core).

**Root cause (product-level, latent since D-05):** every runtime registry was a FUNCTION-LOCAL
static (`static FRuiComponentRegistry Instance;`). A Live-Coding patch that recompiles the
holder gets its OWN fresh copy (guard + storage are function-internal), so patched lookups read
an EMPTY registry while the base one holds every registration → `RUI::Named` returned the empty
Fragment (null Props) → `WithSlot`'s `ToSharedRef()` assert. The PIE-stop AV is the same
Frankenstein session (base + patched Core tearing down a mixed-generation tree). Users WILL
live-code Core themselves — this was a real product landmine, not just tooling fallout.

**Fixes:** (a) all four registries (Component/ElementType/NamedFactory/Hmr) now use the
ZERO-INITIALIZED namespace-scope POINTER + first-touch allocation idiom — patch-stable
(namespace-scope data keeps BASE storage across patches, the TB-15-proven behavior) AND
static-init-order-safe (zero-init precedes all dynamic init; a plain namespace-scope INSTANCE
AV'd at module load — cross-TU init order — caught by the battery on the first attempt and is
now warned against in the code comment). (b) demo hardening: `WithSlot` passes a props-less
node through instead of asserting. (c) the UBT state that triggered the accidental Core patch
was re-synced by a normal rebuild.

**Status:** FIXED (battery 132/0 incl. Boot; the owner's crash pair needs a live re-test —
fresh editor, Ctrl+Alt+F11 with no changes should now compile NOTHING, and even a patched Core
keeps its registrations).

## TB-22 — deleting a .uetkx ON DISK left open importers clean until a keystroke

**Found:** 2026-07-25, owner running 10d: deleting an imported file produced NO squiggle in the
open importer until any edit was made — TB-18's cross-file re-validation only fires on
`didChange` of OPEN docs; a disk deletion of a non-open file has no trigger.

**Fixes (LSP + client, bundle rebuilt):** the client watches `**/*.uetkx`
(`synchronize.fileEvents`) and the server's `onDidChangeWatchedFiles` drops the scan/surface
caches for the changed paths and re-validates every open doc (debounced 150 ms, the TB-18
pattern). The resolution layer was already deletion-aware (stat-guarded reads); only the
trigger was missing. Smoke pin models 10d exactly: delete the exporter on disk → the untouched
importer flags 2300/2302; recreate → clears.

**Status:** FIXED (LSP 92/92 + smoke incl. the watched-files pin; reload the dev host).

> **TB-21 AMENDMENT (2026-07-25, second owner session):** the "re-synced by a normal rebuild"
> claim above was WRONG — incremental builds after the `-DisableUnity -NoPCH` experiment left
> MIXED unity/non-unity intermediates, so Live Coding kept finding "work" on every fresh boot
> (patched Core again → the same Frankenstein: a fresh-boot f11+Play AV'd in the BASE Slate
> adapter over patched-Core-built props, and plain Play served stale UI from incoherent
> outputs). Cure applied: full clean (`Intermediate/Build`, `Binaries`, plugin
> `Intermediate`+`Binaries`) + from-scratch rebuild — battery 132/0 on the pristine outputs.
> **Ops rule (CLAUDE.md):** any `-DisableUnity`/`-NoPCH` experiment MUST be followed by that
> clean, or in-editor Live Coding compiles stay permanently confused.

## TB-23 — cross-patch lambda identity: destroying pre-patch closures ran the WRONG code (AV)

**Found:** 2026-07-25, owner running 10a/10b WITH HMR: after adding `<CounterBadge />` and
patching, the post-patch refresh AV'd tearing down the old tree — twice, reproducibly, both
stacks dying in `TFunction_OwnedObject<…SimpleCounter_UetkxImpl…lambda_3/4>` destructors that
live in EARLIER patch DLLs (`patch_12`, `patch_4`), called from
`OnPatchComplete → HmrRefreshAll → ReconcileFiber → ~FRuiButtonProps`.

**Root cause (the last layer of the TB-13 class):** anonymous lambdas mangle by ORDINAL under
their enclosing function (`…SimpleCounter_UetkxImpl…<lambda_3>`), and Live Coding redirects
functions BY MANGLED NAME across every loaded generation. Inserting/removing a markup child
shifts every later lambda's ordinal, so the OLD stored closure's destructor thunk got
redirected to a DIFFERENT lambda's code with a different capture layout — an AV the moment the
old tree was torn down (post-patch reconcile, PIE stop, unmount).

**Fix (codegen, root):** every emitted BODY symbol carries a per-generation CONTENT HASH —
`<Name>_UetkxImpl_<hash>` for component impls, and `<Name>_RuiBody_<hash>` behind a STABLE
forwarder for hooks and utils (importers keep binding the stable cross-file symbol; the
forwarder recompiles in the same patch and calls the new body). Different content can never
share a mangled name across patches, so old objects keep their own still-loaded code
generation for destruction. Values noted as residual (initializer closures are theoretical;
same treatment available if ever observed).

**Status:** FIXED (battery 131/132 — the one failure is the Acceptance 45-file sweep pin
correctly flagging the owner's two in-flight 10a/10b test files, not a regression; goldens
re-pinned with hashed symbols). Owner re-test: repeat the exact 10a flow — add a widget,
patch, keep clicking, stop PIE.

> **TB-23 AMENDMENT (2026-07-25, owner 10a re-test):** the first-cut fix was WRONG and briefly
> froze HMR entirely — hashing the impl symbol itself broke Live Coding's redirect (patch
> initializers for same-named globals never re-run, so the renamed impl never re-registered
> and old fibers invoked dead code forever: "component does not update", with or without HMR).
> The correct architecture DECOUPLES the two requirements: `<Name>_UetkxImpl` stays STABLE
> (the registered pointer, the FC target, Live Coding's redirect anchor — a one-line shim)
> and calls `<Name>_UetkxBody_<hash>` where every lambda actually lives. Hooks/utils already
> had this shape (stable forwarder + hashed body). Pinned by the codegen invariance test:
> an edited generation keeps the STABLE registration symbol and gets a DIFFERENT body name.

## TB-24 — unimported component TAGS never flagged live (and the sidecar copy flickered)

**Found:** 2026-07-25, owner: `<CounterBadge />` with the exporter on disk but NO import showed
no squiggle as-you-type; a red squiggle appeared only after "several changes" and flickered
between red and clean while editing.

**Root causes:** (a) the TB-19 live strict-usage mirror deliberately covered CODE references
only — component TAGS were left to the compiler sweep; (b) the sweep's verdict reaches the
editor through the HASH-GATED sidecar, so the diagnostic appeared only when the buffer matched
the last compile and vanished on every divergence — the flicker.

**Fix (LSP, bundle rebuilt):** live TAG policing in validate(), the compiler's UetkxResolve
step-2 tag rule: a PascalCase non-host tag that is neither a same-file component nor an import
binding → UETKX2305 (importer-nearest exporter, the add-import quick-fix works) or UETKX2307
when no file exports it. First occurrence per tag, deduped against the existing broken-parse
2307 producer and the sidecar by shared keys. Smoke-pinned (unimported tag → 2305 with fix
tail; unknown tag → 2307; importing clears).

**Status:** FIXED (LSP 92/92 + smoke; battery 131/132 — the one failure is the Acceptance
45-file pin correctly counting the owner's in-flight test files).

## TB-25 — import-specifier DX: single quotes uncolored/untooled, unordered completions, ././ append

**Found:** 2026-07-25, owner during the 10-series: (a) `from './X'` renders white and gets no
tooling (the C++ AND TS scanners both ACCEPT single quotes — C_QUOTE/C_APOS — and the formatter
canonicalizes to `"` on save; only the TextMate grammar and the cursor classifier were
double-quote-only); (b) specifier completions arrive in workspace-walk order — the nearest
file should lead; (c) accepting a suggestion APPENDED to the typed text (`./` + accept
`./Foo` → `././Foo`).

**Fixes (extension + LSP, bundle rebuilt; grammar copies byte-identical):** (a) all four
grammar specifier patterns (3 import forms + host include) accept `'…'`, and `importCursorAt`
classifies a cursor inside a single-quoted specifier — coloring, completion, and every
downstream feature now work in both quote styles, with the formatter still canonicalizing to
the family's double-quote form on save; (b) items carry hops-based `sortText` — same-folder
first, then by distance, then alphabetical; (c) items carry a `textEdit` replacing from just
after the opening quote through the closing quote (the R14 whole-token rule) — never an
append. Smoke-pinned (nearest-first order, replace range, single-quote trigger).

**Status:** FIXED (LSP 92/92 + smoke; reload the dev host).

## TB-26 — rapid HMR patches stacked fading toasts (the "blurry" notification)

**Found:** 2026-07-25, owner: several quick saves made the editor's corner notification turn
into a blurry smear — the controller spawned ONE toast PER patch-complete, and the overlapping
fade-outs render as mush.

**Fix (editor controller):** one toast, coalesced — the live notification's text updates in
place (`SetText` + restarted expire countdown via `bFireAndForget = false` +
`ExpireAndFadeout()`); a new toast spawns only after the previous fully faded. Bonus: the text
now carries the running patch total.

**Status:** FIXED in source; C++ build pending the owner's editor closing (Live Coding holds
the build mutex) — verify visually on the next session: hammer 4-5 quick saves, expect ONE
crisp toast updating its text.

## TB-27 — clangd "void function should not return a value" on EVERY early return

**Found:** 2026-07-25, owner: `return (<></>);` before the main return in SimpleCounter showed
"Void function '__rui_setup_SimpleCounter' should not return a value". Investigation: NOT a
fragment bug — the vdoc lifted setup into a `void __rui_setup_<Name>` scaffold while early-
return windows keep their real `return ( … )` glue with the markup neutralized to `__rui_rn`,
so EVERY early return (element, fragment, and the excludeSpans directive fallback) was illegal
C++ inside it. The compiler pipeline was proven clean (the same shape lowers to
`RUI::Fragment` via codegen); the second message the owner saw (0114 "must be parenthesized")
was transient keystroke state, not reproducible from the committed scanners.

**Fix (vdoc):** the scaffold returns `FRuiNode` (the prelude's `__rui_rn` type) and closes
with a synthetic `return __rui_rn;` tail so the no-early-return shape has no C4715. Early
returns are now legal in place. Pinned: embeddedCpp tests (fragment early return + tail;
no `void __rui_setup_` anywhere) + smoke (owner's exact shape publishes zero errors).

**Status:** FIXED (LSP 94/94 + smoke; bundle rebuilt — reload the dev host).

## TB-28 — `return null;` render-nothing was missing (family parity gap)

**Found:** 2026-07-25, owner (same report): "IIRC we allow or should allow return null."
Confirmed against the Unity sibling: `return null;` IS first-class render-nothing there
(HmrCSharpEmitter rewrites it to `continue;` when inlining loop bodies; shipped samples use
it) — C# gets component-level `return null` for free (null VisualElement). Our C++ side had
no support: the scanners ignored it (2101 if it was the only return) and codegen would have
spliced uncompilable `return null;` verbatim.

**Fix (both scanners + codegen + both formatters + vdoc):** `CollectMarkupReturns` /
`collectMarkupReturns` recognize `return null ;` (bare) and `return ( null );` (paren) at
paren-depth 0 as null spans (`bNull`/`isNull` — no markup window, no Root; the `;` is required
so a mid-edit `null…` identifier prefix stays plain code; `null` is not a C++ identifier, so
there are no false positives). They satisfy 2101, anchor the setup split, terminate 0107
reachability, and codegen lowers them to `return {};` — an empty node array, the exact state
the error-boundary path already feeds the reconciler. Formatters canonicalize the final form
to bare `return null;`; the vdoc neutralizes the token to `__rui_rn` (typed by the TB-27
FRuiNode scaffold). NOT implemented (follow-up if the corpus ever exercises it): Unity's
`return null` → `continue` rewrite INSIDE @for directive bodies — our directive bodies are
real C++ loops where authors write `continue;` directly.

**Coverage:** 5 fileScan corpus cases + 3 formatter goldens (replayed by BOTH sides), codegen
unit pins (`return {};` on all three shapes), ContractFixtures/ReturnNull.uetkx (golden dumped
with the next engine run), vdoc unit + smoke pins.

**Status:** FIXED (LSP 94/94 + smoke; C++ battery pending the TB-26 build).
