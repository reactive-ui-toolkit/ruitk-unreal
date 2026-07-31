# Discord changelog

Community-facing release notes, newest first, pasted MANUALLY by the owner into the family
Discord server's `#unreal` channel after publishing — never posted by the AI.

**Entry format** (the family pattern — mirror `ReactiveUIToolKit/Plans~/DISCORD_CHANGELOG.md`):
`## [X.Y.Z] - YYYY-MM-DD`, a `### <headline>` subtitle, **bold-lead** prose paragraphs and
bullet groups distilled from that release's CHANGELOG section (real numbers, root causes named),
an update-instruction line, a `**Tooling:** UETKX <ver> (VS Code + VS 2022)` trailer when
extensions shipped, a battery/LSP test-count trailer, then a `---` separator. The repo's very
first announcement post lives at the BOTTOM of this file.

**Hard limit: ≤ 2000 characters per entry** (Discord message cap).

## Announcement post (the #unreal intro — owner pastes; added 2026-07-26)

Building UI in Unreal shouldn't feel like fighting Slate. **ReactiveUI for Unreal** brings the React component model to Slate — in **pure C++**: no JavaScript VM, no WebView, no extra tree layers. Write your UI in clean `.uetkx` files, hit save mid-PIE, and watch it update in about a second — with your state preserved.

**What it does:**
- Function components with 23 hooks (UseState, UseEffect, UseContext, UseMemo, UseReducer, and more)
- `.uetkx` markup compiled to reflection-free C++ at build time — zero runtime overhead
- Hot reload in Play mode (Live-Coding-driven HMR) — no editor restart, state survives the save
- 63 wrapped Slate widgets with 1:1 Unreal naming — no aliases to memorize
- Plays WITH Epic's stack, not against it: host our UI inside UMG, drop UMG widgets into our tree, CommonUI activatable screens, live MVVM data via `UseField`
- Full IDE support — completion, diagnostics, formatting, embedded-C++ IntelliSense — for VS Code and Visual Studio 2022
- Works with UE 5.6+

**A quick taste:**
```jsx
export FRuiNode Counter() {
	auto [Count, SetCount] = UseState<int32>(0);

	return (
		<VerticalBox>
			<TextBlock Text={ RUI::Fmt(TEXT("Count: {}"), Count) } />
			<Button OnClicked={ SetCount(Count + 1) }>+</Button>
		</VerticalBox>
	);
}
```

:package: **GitHub (free, source):** https://github.com/reactive-ui-toolkit/ruitk-unreal
:book: **Docs & guides:** https://reactive-ui-toolkit.github.io/ruitk-unreal/

Happy to answer any questions about the approach or architecture. Fab listing and demo video are on the way as we work toward v1.0!

Our discord channel - https://discord.gg/Knedqu4Wyv - currently under construction.

---

## [0.16.0] - 2026-07-31

### One window for every setting

**Reactive UI Toolkit ▸ Settings** — a new window on the plugin's own main menu (also under Window ▸ Tools) now holds every knob the plugin has, in two sections:
- **Runtime** — the six `ruitk.*` reconciler CVars (TimeSlicing, FrameBudgetMs, HostNodePool, HookValidation, StrictDiagnostics, StrictMode) are now real, persistable settings (`URuitkSettings`): edit them live in the editor and they save to your project's `DefaultGame.ini` — which packaged builds ship. Console, command-line, and ini overrides still win (project-setting priority), and untouched projects behave byte-identically.
- **Editor / Hot Reload** — the seven HMR options (watched roots, debounce, notifications, follow-PIE, Live Coding console hiding, …) plus the two rebindable HMR shortcuts.

Both sections edit the same objects their Project Settings pages show — the pages stay as mirrors, now reading as a pair: *Reactive UI Toolkit* and *Reactive UI Toolkit — Editor*. The **Hot Reload window now only runs HMR** (Start/Stop, live stats — the Watched row reports your real watched roots instead of echoing the defaults — and recent errors); its settings checkboxes and shortcut rows moved into the new window, and its "All settings…" link opens it directly.

Family parity: the Unity and Godot legs ship the same one-window settings surface — same tunables, engine-native spelling on each.

**Update:** drop-in — no API or markup changes.

**Tooling:** UETKX 0.9.1 (VS Code + VS 2022) — embedded-C++ ERROR false-positives on UE-interop `.uetkx` files are suppressed (TB-31): cascades from UE headers clang can't fully parse, bogus overload-resolution errors on the hook/API surface, and "undeclared identifier" noise on cross-file imports are dropped; genuine local typos still surface, and `RuitkCompile` + your C++ build stay the authoritative error source.

Full automation battery green on UE 5.6: 133/133.

---

## [0.15.0] - 2026-07-28

### One family, one name: Reactive UI Toolkit

**We are now Reactive UI Toolkit** — the Unity, Godot, and Unreal siblings ship under one umbrella (org `reactive-ui-toolkit`; this repo is now `ruitk-unreal`). 0.15.0 is 0.14.0 with new names — zero behavior changes — but it IS a breaking release: every public identifier moves to one abbreviation system.

**What renamed:**
- Plugin: `Plugins/ReactiveUIToolkit/` (`ReactiveUIToolkit.uplugin`); the 8 modules `ReactiveUI*` → `Ruitk*`; export macros `RUITK*_API`
- Types: `FRuiNode` → `FRuitkNode` and all seven prefixes (`F/U/S/T/I/E/A`); namespace `RUI::` → `Ruitk::`; public macros `RUI_*` → `RUITK_*`
- Automation suite `ReactiveUI.*` → `Ruitk.*`; commandlets `-run=RUI*` → `-run=Ruitk*`; release zips `ReactiveUIToolkit-<ver>.zip`
- License retitled **Reactive UI Toolkit Community License 1.1** — same terms, same US $250k free tier; the credit line is now "Made with Reactive UI Toolkit" (1.0 licensees keep 1.0)

**Migrating is mechanical** (minutes, not hours): install the new plugin, run the shipped idempotent codemod `-run=RuitkMigrateBrand`, paste the CoreRedirects block from `MIGRATION-0.15.md`, resave Blueprints that referenced `URui*` types, and `-run=RuitkCompile -check` exiting 0 proves you're done.

**New home:** https://github.com/reactive-ui-toolkit/ruitk-unreal · docs: https://reactive-ui-toolkit.github.io/ruitk-unreal/ (old GitHub links redirect; re-bookmark the docs — Pages URLs don't).

**Tooling:** UETKX 0.9.0 (VS Code + VS 2022) — same marketplace listings and install ids, content converted, umbrella search keywords added. The 132-entry automation suite runs under the new `Ruitk` filter.

---

## [0.14.0] - 2026-07-25

### Files are modules for real, return null lands family-wide, and the compiler stops being polite about silent mistakes

**File-scoped exports (ES semantics).** Two files can export the same name now — a file IS its own module, collisions only exist inside one importer and `as` aliasing solves them there. Runtime identity is file-qualified with short-name lookup at the designer edges (ambiguity is loud, never first-wins). One-time remount on upgrade (generated code fully regenerates).

**`return null;` renders nothing** — as an early guard OR as a component's only return (React semantics, now pinned by the shared family corpus on all three engines' grammars).

**Ten silent surfaces now fail the compile instead of failing at runtime:** enum-string typos (`HAlign="cesnter"`), malformed typed style/slot strings (`RenderOpacity="0.5"` used to render INVISIBLE), duplicate attributes, duplicate sibling keys, slot keys the parent never reads, wrong-cased attributes (`slot.fill` silently disarmed all validation), unresolvable brush names, and more.

**HMR got crash-proof at the root:** generated code is Live-Coding-safe by construction (stable registration shims + content-hashed bodies — old closures can never run wrong-layout code), hook-shape edits reset state cleanly instead of crashing, value/style edits actually apply, and rapid saves coalesce into one crisp toast.

**In the editor:** unimported tags and orphaned usages error as-you-type with add-import quick-fixes, attribute VALUES validate live, single-quoted imports get full tooling, specifier completion is nearest-first and replaces instead of appending, and embedded-C++ intel no longer false-flags early returns.

Update to **ReactiveUI 0.14.0** (Unreal), verified on UE 5.6 + 5.7. **Tooling:** UETKX 0.8.0 (VS Code + VS 2022). Battery 132/132, LSP 94/94.

---
## [0.13.0] - 2026-07-18

### ReactiveUI is staying free — here's the plan that keeps it alive

**TL;DR: nothing changes for almost everyone.** From 0.13.0 the plugin ships under the **ReactiveUI Community License 1.0** (previously PolyForm Shield): free to use, modify, and ship commercially for any team earning under **$250k/year**. Above that, shipping a game takes a commercial license — **$2,000 per title** (one-time, perpetual) or **$2,500/studio/year** — same terms and prices on every engine we support (Godot, Unity, Unreal). Development, evaluation, jams, and learning stay free at ANY company size; the threshold only applies when you *ship*.

**What stays exactly the same:** the full plugin, all features, all updates — no feature-gated "pro version", no telemetry, no code gate. Everything you've already downloaded keeps the license it shipped with, forever.

**What we ask of everyone:** a "Made with ReactiveUI" line in your credits, next to your other middleware — that's how the project gets found.

**Also in 0.13.0:** unused imports (UETKX2304) are now error-tier family-wide, ES combined import forms (`import Def, { a, b as c } from`), and the ES-modules field-wave fixes.

Update to **ReactiveUI 0.13.0** (Unreal). **Tooling:** UETKX 0.7.0 (VS Code + VS 2022). Weird case (nonprofit, just-over-the-line)? Email us — we'd rather you ship with ReactiveUI than not.

---
## [0.12.0] - 2026-07-18

### ES modules — a .uetkx file IS a module now

**The wrapper keywords are gone (deprecated).** No more `component`/`hook`/`module` — a
declaration's kind is read from its plain C++ signature: `export FRuiNode Card(FString Label)`
is a component, `export int32 UseTick(...)` a hook, `export FLinearColor Cool = ...;` a **value
export**, any other function a util. Style modules become plain value exports.

**The full ES import surface:** `import { Chip as Badge }` (tags too: `<Badge/>`),
`import * as Palette` → `Palette::Cool`, default imports + `export default`, and
`export { A, B };` lists.

**Privacy is real at runtime now.** Private components get file-qualified identity — no more
HMR last-swap-wins collisions. Heads-up: renaming a FILE remounts its privates (state reset);
exported members keep identity.

**One command migrates your project:** `-run=RUIMigrateEsModules` — idempotent,
record-driven, flips wrappers to plain declarations, hoists modules to value exports,
gates on a zero-diagnostics compile. Old syntax parses this one minor (warns UETKX2320).

**IDE:** completions/hover/F12 resolve through aliases; diagnostics 2320–2327 live. NEW —
find-all-references, rename (a declaration rename updates every importer, `as` aliases
included; embedded-C++ LOCALS rename via clangd, all-or-nothing), document/workspace
symbols, and quickfixes for missing/unused imports, unexported names, and wrapper migration.
Locals shadowing exported names no longer false-diagnose, markup-only usage finally counts
(no phantom 2304/2305). Also fixed: compiler diagnostics silently stopped reaching the
editor after a sidecar schema bump — they're back.

Update: reinstall the plugin zip + grab UETKX 0.6.0 (VS Code + VS 2022). Re-run the codemod
per the docs' "Migrating to ES modules" page.

**Tooling:** UETKX 0.6.0 (VS Code + VS 2022) · battery 128/128 · LSP 71/71 · 32 contract
goldens · corpus pinned tri-repo

---

## [0.11.0] - 2026-07-17

### Markup everywhere — markup is a first-class expression now

**`auto Card = (<VerticalBox>…</VerticalBox>);` just works.** Markup is legal at every
expression boundary — assignments (both spellings), call arguments, ternary branches,
short-circuit `cond && <Chip/>` — detected by the family's position-gated jsx scan (so
`a < b` can never be mistaken for a tag) and lowered in place to an `FRuiNode` value you can
splice with `{ Card }`, even more than once. Same grammar as the Godot and Unity siblings;
the shared scanner corpus now hashes identically across all three repos.

**Rules of hooks land too (UETKX0013–0016).** A `Use*` call under an `if`, a loop, a
`switch`/`@match` branch, a lambda, or anywhere inside markup is now a compile error instead
of a hook-order landmine — hooks run unconditionally at the top of the body, period. And hook
signatures now ignore markup text entirely: editing markup can never spuriously reset your
live HMR state again (components with early returns get a one-time signature change on first
swap).

**The IDE side is a rebuild.** UETKX 0.5.0: crash-proof server, and embedded C++
intelligence in EVERY markup expression — completion/hover/diagnostics inside attr values,
event handlers, and directive bodies, with the compile database sanitized until the demo
tree sweeps with zero false positives. Biggest news: **clangd 22.1.6 now ships INSIDE the
extension** — the VS Code win32-x64 flavor and the VS2022 VSIX bundle it, so embedded C++
intelligence works with zero machine setup. Plus a new demo: AcceptanceLab §10 shows every
markup-as-value shape live.

Update to **ReactiveUI for Unreal 0.11.0** (GitHub release). **Tooling:** UETKX 0.5.0
(VS Code + VS 2022). Battery `128/128`, LSP `46/46`.

---

## [0.10.0] - 2026-07-16

### `.uetkx` preambles are imports-only now

**Raw `#include` lines are retired.** Every generated file already carries the library's own
headers automatically — `RuiContext.h`, `RuiRouter.h`, the UMG/CommonUI/MVVM interop headers
when that plugin is linked, and more. For your own project headers, a new import form:
`import "@MyTypes.h"` compiles to `#include "MyTypes.h"` — nameless by design, since the C++
compiler resolves the header's own symbols. This mirrors the family: Unity shipped the same
shape (`import "@Namespace"`) first.

A raw `#include` still works — nothing breaks — but naming a header the generated file already
auto-includes now gets a friendly redundancy hint. `-run=RUIMigrateImports -tidy` migrates an
existing tree for you (idempotent, surgical — it never touches a line that also carries a
comment). The demo gallery ships fully migrated: `MvvmDemo.uetkx` now opens with zero includes
at all.

Update to **ReactiveUI for Unreal 0.10.0** (GitHub release). **Tooling:** UETKX 0.4.2 (VS Code +
VS 2022) — syntax highlighting, live diagnostics, and a discoverable `import "@"` completion
snippet for the new form. Battery `128/128`.

---

## [0.9.0] - 2026-07-16

### Wave G — early returns + short-circuit rendering; the completion plan is DONE

**The `.uetkx` grammar catches up with the family.** Guard clauses now work at component
level — `if (x) { return ( <A/> ); }` anywhere in the body, your C++ branches, every markup
return lowers in place (the Unity verbatim-emit model). And short-circuit rendering lands:
`{ cond && <X/> }` renders only when true, `{ cond || <X/> }` only when false — the
`UETKX3002` diagnostic is retired. Existing files compile byte-identically.

Pinned by new contract goldens + corpus cases in BOTH the C++ compiler and the TS language
server, plus a compiled proof component mounted by a new automation suite.

Update to **ReactiveUI for Unreal 0.9.0** (GitHub release). Battery `127/127`.

---

## [0.8.0] - 2026-07-16

### Wave 4 — TreeView lands; the widget-completion plan is DONE

**63 markup tags, 65+ wrapped widgets — every widget in the completion plan is now
implemented.** `TreeView` closes the last item-model gap (TD-022): a virtualized
hierarchical tree with per-row reconciler sub-roots, a `GetChildren` accessor, controlled
expansion (`ExpandedItems` in, `OnExpansionChanged` out), and `HeaderRow` columns built
from a declarative `Columns` list. C++-first like `ListView`. Plus `VectorInputBox` /
`RotatorInputBox` for controlled XYZ / Roll-Pitch-Yaw editing, and `Splitter2x2` — four
resizable quadrants routed by `Slot.Role`.

Also fixed: `ConstraintCanvas` slot ZOrder tripped an engine ensure when applied before
the slot committed — attach-then-apply now.

Update to **ReactiveUI for Unreal 0.8.0** (GitHub release). Battery `126/126`.

---

## [0.7.0] - 2026-07-16

### Wave 3 — popups, toasts, anchors, splitters

**60 markup tags.** `MenuAnchor` brings the popup primitive (anchor + `Slot.Role="menu"`
content, controlled open state); `NotificationList` + `PushNotification` bring toasts;
`ConstraintCanvas` brings UMG-style anchors to markup; `Splitter` brings resizable panes.
Plus `NumericDropDown`, `BreadcrumbTrail`, `LinkedBox`, `WindowTitleBarArea`,
`VirtualJoystick`, and `SearchableComboBox` — the first engine-gated widget (5.7+, schema-
annotated). 16 slot keys.

Update to **ReactiveUI for Unreal 0.7.0** (GitHub release). Battery `125/125`.

---

## [0.6.0] - 2026-07-16

### Wave 2 — 12 interactive composites + imperative handles

**50 markup tags.** New: `EditableText`, `InlineEditableTextBlock`, `VirtualKeyboardEntry`,
`ColorWheel`, `ColorSpectrum`, `ColorGradingWheel`, `InputKeySelector`, `VolumeControl`,
`RadialBox`, `TextScroller`, `LayeredImage`, `ExpandableButton` (role slots). Plus the
family's `UseImperativeHandle` ref-publishing form and `WidgetFromHandle<T>()` — call
`ScrollToEnd()`-class widget APIs through any captured `Ref`. Slider/ScrollBox/TextBlock
gained their remaining runtime setters (16 style keys).

Update to **ReactiveUI for Unreal 0.6.0** (GitHub release). Battery `125/125`.

---

## [0.5.0] - 2026-07-16

### Widget-completion wave 1 — 8 new widgets on the road to "1.0 wraps everything"

**The coverage campaign begins: 1.0 will ship with EVERY official runtime Slate widget
wrapped, and this is the first wave.** New tags: `ColorBlock`, `SimpleGradient`,
`ComplexGradient`, `Hyperlink`, `EnableBox`, `ScissorRectBox`, `BackgroundBlur`,
`InvalidationPanel` — 38 markup tags total.

**Tooltips, everywhere.** `ToolTipText` is a universal style key — put it on ANY element,
markup or C++, same as the Unity/Godot siblings' universal tooltip prop.

Also: `Button` gets `bIsFocusable`, `ProgressBar` gets `BarFillType`, the `RUI::Style()`/
`RUI::Slot()` C++ builders caught up with every markup key (and a new CI gate keeps it that
way), and a registry-wide contract test now enforces the widget-replacement rules every new
adapter must follow — the rails for the ~30 widgets still coming in waves 2-4.

Update to **ReactiveUI for Unreal 0.5.0** (GitHub release). Battery `125/125` · LSP `33/33`.

---

## [0.4.0] - 2026-07-16

### DOOM in the widget tree — plus localization closes the last v1 gap

**The family's marquee demo lands on Unreal, and a whole game frame costs ~0.2 ms of CPU.**
A fully playable raycast FPS where every wall strip, sprite, and tracer is a real keyed
`<Image>` in ONE `<Canvas>`, diffed by the reconciler at 60+ Hz. Six levels, 100% procedural
textures, zero assets — in the gallery under "Doom" (WASD + mouse, Ctrl+R shows FPS).
Measured: sim 19 µs, geometry 34 µs, reconcile + Slate apply of ~470 live widgets ≈ **197 µs
median per frame**.

**Localization.** `.uetkx` text has always compiled to real `NSLOCTEXT` — now the gather
pipeline is verified (add `*.inl` to your gather masks; a complete reference target ships),
and a culture switch re-renders every mounted screen live.

**Epic interop rounds out:** `URuiHostWidget` takes designer-set props + a viewmodel from the
Details panel (`UseHostProp`/`UseHostViewModel`), CommonUI screens answer
`GetDesiredFocusTarget()` for gamepad focus, and `UseOwnedViewModel` + public marshaling
helpers land.

**New in markup:** the universal `Ref={ }` attribute, the `Canvas` widget (absolute placement,
live slot mutation), a universal `Clipping` style key, `ScaleBox` alignment, and
`<Image Image={ Brush } />`.

**Fixed** (found by the Doom playtests): `ColorAndOpacity` was silently dropped on
`Image`/`Separator` from markup; `UseStableFunc`/`UseStableAction` were uncallable from markup.

Update to **ReactiveUI for Unreal 0.4.0** (GitHub release / Fab listing).
**Tooling:** UETKX 0.2.0 (VS Code + VS 2022) — completion inside embedded C++ is clangd-backed.
Battery `124/124` · LSP `33/33`.

---

## [0.3.0] - 2026-07-14

### Unreal Engine 5.8 support

**The full battery is green on 5.6, 5.7, AND 5.8.** The 5.6→5.8 API diff found no removals —
additive-only FArguments surface across 26 wrapped widgets, all recorded as v1.x candidates.

**Fixed:** UE 5.8 changed `FJsonObject`'s key type (`FString` → `UE::FSharedString`) — the
`.uetkx` sidecar reader now iterates keys engine-agnostically. The engine-api-diff tool
hard-fails on partial engine installs instead of emitting a wrong diff.

Update to **ReactiveUI for Unreal 0.3.0** (GitHub release). Battery `103/103` per engine.

---

## [0.2.0] - 2026-07-14

### Unreal Engine 5.7 support + first-beta fixes

**5.6 and 5.7, one plugin.** The dev `.uplugin` no longer pins `EngineVersion` (the pin
silently skipped loading the whole plugin on 5.7 headless); per-engine packages stamp it at
release. Full 5.7 deprecation sweep (TObjectPtr `AddReferencedObject`, FieldNotification
include, C5038).

**Fixed:**
- **Portal content no longer leaks on unmount** — deleting a subtree containing a portal now
  explicitly detaches its children from the external target panel.
- **`URuiHostWidget::Remount()` actually remounts** — UMG caches `RebuildWidget`'s return, so
  the host now swaps content inside a stable wrapper; runtime `ComponentName` changes work.
- Packaged README described a "pre-alpha scaffold"; it now states the beta reality.

Update to **ReactiveUI for Unreal 0.2.0** (GitHub release). Battery `103/103`.

---

## [0.1.0] - 2026-07-13

### First beta — the complete React-style stack for Unreal, in pure C++

**Function components return a virtual tree; a fiber reconciler patches real Slate widgets;
state lives in hooks.** 23 hooks (`UseState`, `UseEffect`, `UseContext`, signals, tweens, …),
structural error boundaries, `<Presence>` exit animations — no UObject dependency in the core.

- **35+ wrapped Slate widgets** incl. virtualized `ListView`/`TileView`; setter-based styling
  (a style tweak never rebuilds a widget); typed events, drag-and-drop, shortcuts.
- **`.uetkx` markup** — JSX-like, compiles to native C++ (committed, drift-gated); static
  imports/exports from day one with a migration codemod.
- **HMR** — save a `.uetkx`, the running UI patches mid-PIE with state preserved (Live Coding).
- **Epic interop** — UMG both directions, CommonUI activatable screens, MVVM/FieldNotify via
  `UseField`.
- **`@theme`/`@uss` stylesheets**, a 17-hook router, a 15-screen example gallery.
- **IDE tooling** — `.uetkx` language server + VS Code / VS 2022 clients.

**ReactiveUI for Unreal 0.1.0** is on GitHub (beta — API may shift before 1.0).
**Tooling:** UETKX 0.1.0 (VS Code + VS 2022).

---

## The initial announcement post (pinned here; owner posts once, before/with 0.1.0)

Building UI in Unreal shouldn't feel like fighting the engine. **ReactiveUI for Unreal** brings
the React component model to Slate — reusable function components, state hooks, and a JSX-like
markup language called **UETKX**. Write your UI in clean `.uetkx` files, hit save, and watch it
update mid-PIE with hot reload (state preserved) — and it all compiles to plain C++ for
shipping. No script VM in your game.

**What it does:**
- Function components with hooks (state, effect, context, memo, reducer, signals, and more)
- `.uetkx` markup compiled to native C++ at build time — zero runtime overhead
- Hot reload that patches the running UI in PIE without restarting (Live Coding, ~seconds)
- Full IDE support — completion, go-to-definition, diagnostics, formatting — for VS Code and
  Visual Studio 2022
- UMG, CommonUI, and MVVM stay in place: host inside UserWidgets, ship CommonUI screens, feed
  FieldNotify viewmodels straight into components
- Works with UE 5.6 – 5.8

**A quick taste:**
```jsx
component Counter {
	auto [Count, SetCount] = UseState<int32>(0);

	return (
		<VerticalBox>
			<TextBlock Text={ RUI::Fmt(TEXT("Count: {}"), Count) } />
			<Button OnClicked={ SetCount(Count + 1) }><TextBlock Text="+" /></Button>
		</VerticalBox>
	);
}
```

:clapper: **Demo video:** __VIDEO_URL__ <!-- the Doom demo recording — owner records -->
:book: **Docs & guides:** https://reactive-ui-toolkit.github.io/ruitk-unreal/
:package: **GitHub:** https://github.com/reactive-ui-toolkit/ruitk-unreal

Happy to answer any questions about the approach or architecture. More updates to come as we
work toward v1.0!

Our discord channel - https://discord.gg/Knedqu4Wyv
