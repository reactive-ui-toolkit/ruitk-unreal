# Migrating to 0.15.0 — the "Reactive UI Toolkit" rebrand

0.15.0 renames the product and **every public identifier**: the family umbrella is now
**Reactive UI Toolkit** (org `reactive-ui-toolkit`, repo `ruitk-unreal`), and the plugin,
modules, types, macros, and namespace follow one abbreviation system — `Ruitk` / `RUITK`.
Nothing else changed behaviorally: 0.15.0 is 0.14.0 with new names. It IS breaking — a
project that compiled against 0.14.0 will not compile against 0.15.0 until migrated.

The migration is mechanical and automated. Budget: minutes.

## Steps

> **Install the new plugin BEFORE deleting the old one — the order is load-bearing for C++
> projects.** The codemod ships *inside* the new plugin, and running it with `UnrealEditor-Cmd`
> compiles and loads your game modules first. If you delete `Plugins/ReactiveUI/` up front, your
> own still-unmigrated `.cpp/.h` (`FRuiNode`, `#include "ReactiveUICore/…"`, `RUI::`) no longer
> compile, the editor never finishes launching, and the codemod that would have fixed them can
> never run. Installing side by side is safe: the two plugins share no module name
> (`ReactiveUICore` vs `RuitkCore`) and no reflected class name, so they coexist for the one run.
> Blueprint-only / `.uetkx`-only projects have nothing that fails to compile and may use any order.

1. **Install** the new plugin `Plugins/ReactiveUIToolkit/` (from `ReactiveUIToolkit-0.15.0.zip`
   or this repo) **alongside** your existing `Plugins/ReactiveUI/` — leave the old folder in
   place for now. The `.uplugin` is `ReactiveUIToolkit.uplugin`; the plugin NAME your `.uproject`
   references becomes `"ReactiveUIToolkit"` (step 2 rewrites that reference for you).
2. **Run the codemod** over your project (idempotent — safe to re-run):

   ```
   UnrealEditor-Cmd <YourProject>.uproject -run=RuitkMigrateBrand
   ```

   Add `-dry` first if you want the report without writes. It rewrites your `.uetkx` files
   (heads + embedded C++), `Build.cs` module dependencies, `#include` paths, and the
   `.uproject` plugin reference, using exactly the rule tables below.

   It walks the whole project — `Source/` **and your own project plugins under `Plugins/`** —
   and skips only the two brand plugin folders (`Plugins/ReactiveUIToolkit/` already ships
   converted; `Plugins/ReactiveUI/` is left untouched so it keeps compiling until you remove it)
   plus `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` and dot-dirs.
3. **Delete** the old plugin folder `Plugins/ReactiveUI/`. Nothing references it any more.
4. **Paste the `[CoreRedirects]` block** below into your project's `Config/DefaultEngine.ini`
   so existing `.uasset`/`.umap` content referencing the old reflected class names loads and
   redirects cleanly.
5. **Resave** any Blueprints/assets that reference `URui*` types (open + save is enough — the
   redirect rewrites the reference on save).
6. **Verify**:

   ```
   UnrealEditor-Cmd <YourProject>.uproject -run=RuitkCompile -check
   ```

   must exit 0 (all generated code byte-stable under the new names).

## Module renames (Build.cs dependency names)

| 0.14.0 | 0.15.0 |
|---|---|
| `ReactiveUICore` | `RuitkCore` |
| `ReactiveUISlate` | `RuitkSlate` |
| `ReactiveUIUMG` | `RuitkUMG` |
| `ReactiveUICommonUI` | `RuitkCommonUI` |
| `ReactiveUIMVVMBridge` | `RuitkMVVMBridge` |
| `ReactiveUIInterp` | `RuitkInterp` |
| `ReactiveUIToolchain` | `RuitkToolchain` |
| `ReactiveUIEditor` | `RuitkEditor` |

Export macros follow: `REACTIVEUI<MOD>_API` → `RUITK<MOD>_API` (9 pairs, incl.
`RUIDEMO_API` → `RUITKDEMO_API` in the demo host project).

## Macro renames (the public `RUI_*` family)

Every `RUI_*` preprocessor macro is now `RUITK_*`. The ones user code typically touches:

| 0.14.0 | 0.15.0 |
|---|---|
| `RUI_PROP` | `RUITK_PROP` |
| `RUI_PROP_EVENT` | `RUITK_PROP_EVENT` |
| `RUI_PROPS_BODY` | `RUITK_PROPS_BODY` |
| `RUI_COMPONENT` | `RUITK_COMPONENT` |
| `RUI_EQ` | `RUITK_EQ` |
| `RUI_ROW` | `RUITK_ROW` |
| `RUI_UETKX_DECL_PHASE` | `RUITK_UETKX_DECL_PHASE` (emitted by codegen — regenerate, don't hand-edit) |

## Identifier rules (what the codemod applies, verbatim)

Ordered, and idempotent — a second run rewrites nothing. Two mechanisms: the `Rui`-stem rules
carry an explicit `(?!tk)`/`(?!TK)` lookahead, and the rest (module tokens, export macros, path
and `.uproject` rules, the namespace rules, `_RUI_HOOK_SIG`) are self-guarding because their
output no longer matches their own pattern. `\b` = word boundary.

Every `Rui`-stem rule also requires the **next character to be uppercase or `_`** (`(?=[A-Z_])`).
That is what keeps the codemod off ordinary words and unrelated user identifiers: without it,
`FRuit` becomes `FRuitkt` and a comment or string reading "Ruined save" becomes "Ruitkned save",
silently. The trade is deliberate: the word `Rui` used as English prose in your comments ("the
Rui tree") is left alone rather than risk corrupting real text.

1. Module tokens, longest first (table above), whole-word.
2. The 9 export-macro pairs, whole-word.
3. Paths: `Plugins/ReactiveUI/` → `Plugins/ReactiveUIToolkit/` (both separators);
   `ReactiveUI.uplugin` → `ReactiveUIToolkit.uplugin`; `.uproject`
   `"Name"\s*:\s*"ReactiveUI"` → `"Name": "ReactiveUIToolkit"` (any spacing around the colon
   is matched; the replacement writes the canonical form).
4. Type prefixes: `\bFRui(?!tk)(?=[A-Z_])` → `FRuitk`, and the same for `URui`, `SRui`, `TRui`,
   `IRui`, `ERui`, `ARui` (e.g. `FRuiNode` → `FRuitkNode`, `URuiHostWidget` →
   `URuitkHostWidget`; `_` is in the class so template placeholders like `FRui__TAG__Props`
   convert too).
5. Namespace: `\bRUI::` → `Ruitk::`; `\bnamespace RUI\b` → `namespace Ruitk`.
6. Boundary-blocked families (a word character before the token defeats `\b`):
   `LogRui(?!tk)(?=[A-Z_])` → `LogRuitk`, and the same for `CVarRui` and `GRui`.
7. Bare-`Rui` blanket: `\bRui(?!tk)(?=[A-Z_])` → `Ruitk` (`RuiPriv_*` → `RuitkPriv_*`,
   `RuiUetkx_*` → `RuitkUetkx_*`, …).
8. Case-class `RUI`: PascalCase `\bRUI(?!TK)(?=[A-Z][a-z])` → `Ruitk` (`RUICompile` →
   `RuitkCompile` — all `-run=` commandlet names follow); ALL-CAPS `\bRUI(?!TK)(?=[A-Z_])` →
   `RUITK` (`RUI_PROP` → `RUITK_PROP`, `RUIBENCH` → `RUITKBENCH`).
9. Console variables, by name rather than by prefix — `\brui\.` followed by one of
   `DumpTree`, `FrameBudgetMs`, `Hmr`, `HookValidation`, `HostNodePool`, `Stats`,
   `StrictDiagnostics`, `StrictMode`, `TimeSlicing` → `ruitk.<Name>`. (Naming them outright is
   what stops the rule from rewriting an ordinary member access on a variable called `rui`.)
   **Update any `rui.*` lines in your own `.ini` files, console shortcuts, and docs** — the
   codemod rewrites source files, not your saved editor console history.
10. Baked constants: `<Component>_RUI_HOOK_SIG` → `<Component>_RUITK_HOOK_SIG` — running
    `-run=RuitkCompile` regenerates them anyway; the codemod rule just keeps a
    not-yet-regenerated tree consistent.

Also renamed in-editor: the stats group is now `stat Ruitk` (was `stat ReactiveUI`), the
Message Log page id is `"Ruitk"` (shown as "Reactive UI Toolkit"), and the preview
nomad-tab id is `RuitkPreview` — a saved editor layout referencing the old tab id simply
drops the tab; reopen it from Tools (harmless, one-time).

### One-time editor-preference reset (`.uetkx` HMR)

The `.uetkx` HMR settings class moved module and name with everything else, and a
`UCLASS(config)` section is keyed by both — so your saved section in
`Saved/Config/.../EditorPerProjectUserSettings.ini` moves from

```ini
[/Script/ReactiveUIEditor.ReactiveUetkxEditorSettings]   ; 0.14
[/Script/RuitkEditor.RuitkUetkxEditorSettings]           ; 0.15
```

There is **no redirect for config section names**, so the seven HMR toggles (notifications,
verbose watcher, hide Live Coding console, follow PIE, disable-session-on-stop, debounce ms,
watched roots) fall back to their defaults once, per user, on first 0.15 launch. Nothing else
reads the old section — this is a cosmetic reset, not a functional change. Re-set them in
Editor Preferences, or rename the section header in that ini by hand to carry your values over.
Old sections are inert and can be deleted.

## CoreRedirects (paste into Config/DefaultEngine.ini)

```ini
[CoreRedirects]
+ClassRedirects=(OldName="/Script/ReactiveUIUMG.RuiHostWidget",NewName="/Script/RuitkUMG.RuitkHostWidget")
+ClassRedirects=(OldName="/Script/ReactiveUIUMG.RuiSignalViewModel",NewName="/Script/RuitkUMG.RuitkSignalViewModel")
+ClassRedirects=(OldName="/Script/ReactiveUIUMG.RuiWorldSubsystem",NewName="/Script/RuitkUMG.RuitkWorldSubsystem")
+ClassRedirects=(OldName="/Script/ReactiveUICommonUI.RuiActivatableScreen",NewName="/Script/RuitkCommonUI.RuitkActivatableScreen")
+ClassRedirects=(OldName="/Script/ReactiveUIMVVMBridge.RuiMvvmViewModel",NewName="/Script/RuitkMVVMBridge.RuitkMvvmViewModel")
```

That is the complete set for a user project: these five are every reflected class the shipped
plugin ever exposed, and no reflected `FRui*` USTRUCTs or `ERui*` UENUMs existed. (This repo's
own `Config/DefaultEngine.ini` carries additional redirects for the demo/test modules
`RuiDemo`/`RuiHostTests`; those modules are never part of a user project, so a redirect for them
in your ini would be dead config.)

## Also renamed

- Automation specs: `ReactiveUI.<Suite>.<Test>` → `Ruitk.<Suite>.<Test>`; the CI filter is
  `-ExecCmds="Automation RunTests Ruitk; Quit"`.
- Commandlets (all five that 0.14 shipped): `-run=RUICompile` / `RUIContractDump` /
  `RUIExportSchema` / `RUIMigrateImports` / `RUIMigrateEsModules` → `-run=Ruitk*`.
- Release zips: `ReactiveUI-<ver>.zip` → `ReactiveUIToolkit-<ver>.zip`.
- License: the ReactiveUI Community License 1.0 is now the
  **Reactive UI Toolkit Community License 1.1** (terms unchanged; licensees under 1.0 keep
  1.0; the credit line is now "Made with Reactive UI Toolkit").
