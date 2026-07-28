# Migrating to 0.15.0 — the "Reactive UI Toolkit" rebrand

0.15.0 renames the product and **every public identifier**: the family umbrella is now
**Reactive UI Toolkit** (org `reactive-ui-toolkit`, repo `ruitk-unreal`), and the plugin,
modules, types, macros, and namespace follow one abbreviation system — `Ruitk` / `RUITK`.
Nothing else changed behaviorally: 0.15.0 is 0.14.0 with new names. It IS breaking — a
project that compiled against 0.14.0 will not compile against 0.15.0 until migrated.

The migration is mechanical and automated. Budget: minutes.

## Steps

1. **Delete** the old plugin folder `Plugins/ReactiveUI/` from your project.
2. **Install** the new one: `Plugins/ReactiveUIToolkit/` (from `ReactiveUIToolkit-0.15.0.zip`
   or this repo). The `.uplugin` is `ReactiveUIToolkit.uplugin`; the plugin NAME your
   `.uproject` references is now `"ReactiveUIToolkit"`.
3. **Run the codemod** over your project (idempotent — safe to re-run):

   ```
   UnrealEditor-Cmd <YourProject>.uproject -run=RuitkMigrateBrand
   ```

   Add `-dry` first if you want the report without writes. It rewrites your `.uetkx` files
   (heads + embedded C++), `Build.cs` module dependencies, `#include` paths, and the
   `.uproject` plugin reference, using exactly the rule tables below.
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

Ordered; all idempotency-guarded. `\b` = word boundary.

1. Module tokens, longest first (table above), whole-word.
2. The 9 export-macro pairs, whole-word.
3. Paths: `Plugins/ReactiveUI/` → `Plugins/ReactiveUIToolkit/` (both separators);
   `ReactiveUI.uplugin` → `ReactiveUIToolkit.uplugin`; `.uproject`
   `"Name": "ReactiveUI"` → `"Name": "ReactiveUIToolkit"`.
4. Type prefixes: `\bFRui(?!tk)` → `FRuitk`, and the same for `URui`, `SRui`, `TRui`,
   `IRui`, `ERui`, `ARui` (e.g. `FRuiNode` → `FRuitkNode`, `URuiHostWidget` →
   `URuitkHostWidget`).
5. Namespace: `\bRUI::` → `Ruitk::`; `\bnamespace RUI\b` → `namespace Ruitk`.
6. Boundary-blocked families: `LogRui*` → `LogRuitk*`, `CVarRui*` → `CVarRuitk*`,
   `GRui*` → `GRuitk*`.
7. Bare-`Rui` blanket: `\bRui(?!tk)` → `Ruitk` (`RuiPriv_*` → `RuitkPriv_*`,
   `RuiUetkx_*` → `RuitkUetkx_*`, …).
8. Case-class `RUI`: PascalCase `\bRUI(?=[A-Z][a-z])` → `Ruitk` (`RUICompile` →
   `RuitkCompile` — all `-run=` commandlet names follow); ALL-CAPS `\bRUI(?=[A-Z_])` →
   `RUITK` (`RUI_PROP` → `RUITK_PROP`, `RUIBENCH` → `RUITKBENCH`).

Deliberately unchanged: the `rui.*` console variables (`rui.StrictMode`,
`rui.TimeSlicing`, …), the lowercase `__rui_*` generated-symbol prefix, and the baked
`<Component>_RUI_HOOK_SIG` constants — regenerated code keeps matching them.

## CoreRedirects (paste into Config/DefaultEngine.ini)

```ini
[CoreRedirects]
+ClassRedirects=(OldName="/Script/ReactiveUIUMG.RuiHostWidget",NewName="/Script/RuitkUMG.RuitkHostWidget")
+ClassRedirects=(OldName="/Script/ReactiveUIUMG.RuiSignalViewModel",NewName="/Script/RuitkUMG.RuitkSignalViewModel")
+ClassRedirects=(OldName="/Script/ReactiveUIUMG.RuiWorldSubsystem",NewName="/Script/RuitkUMG.RuitkWorldSubsystem")
+ClassRedirects=(OldName="/Script/ReactiveUICommonUI.RuiActivatableScreen",NewName="/Script/RuitkCommonUI.RuitkActivatableScreen")
+ClassRedirects=(OldName="/Script/ReactiveUIMVVMBridge.RuiMvvmViewModel",NewName="/Script/RuitkMVVMBridge.RuitkMvvmViewModel")
+ClassRedirects=(OldName="/Script/RuiHostTests.RuiTestViewModel",NewName="/Script/RuitkHostTests.RuitkTestViewModel")
+ClassRedirects=(OldName="/Script/RuiHostTests.RuiTestUserWidget",NewName="/Script/RuitkHostTests.RuitkTestUserWidget")
+ClassRedirects=(OldName="/Script/RuiHostTests.RuiTestMvvmSubViewModel",NewName="/Script/RuitkHostTests.RuitkTestMvvmSubViewModel")
+ClassRedirects=(OldName="/Script/RuiDemo.RuiDemoGameMode",NewName="/Script/RuitkDemo.RuitkDemoGameMode")
```

(The three `RuiHostTests` and the `RuiDemo` lines only matter if you referenced the demo
host project's types; they are harmless to include. No reflected `FRui*` USTRUCTs or
`ERui*` UENUMs existed, so class redirects are the complete set.)

## Also renamed

- Automation specs: `ReactiveUI.<Suite>.<Test>` → `Ruitk.<Suite>.<Test>`; the CI filter is
  `-ExecCmds="Automation RunTests Ruitk; Quit"`.
- Commandlets: `-run=RUICompile` / `RUIContractDump` / `RUIExportSchema` /
  `RUIMigrateImports` / `RUIMigrateEsModules` → `-run=Ruitk*`.
- Release zips: `ReactiveUI-<ver>.zip` → `ReactiveUIToolkit-<ver>.zip`.
- License: the ReactiveUI Community License 1.0 is now the
  **Reactive UI Toolkit Community License 1.1** (terms unchanged; licensees under 1.0 keep
  1.0; the credit line is now "Made with Reactive UI Toolkit").
