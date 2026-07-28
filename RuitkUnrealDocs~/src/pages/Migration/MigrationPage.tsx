import type { FC } from 'react'
import {
  Alert,
  Box,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  Typography,
} from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'
import { GITHUB_URL } from '../../links'

const LEAF = `// Stage 1 — in the UMG Designer: drop a "Reactive UI Toolkit Host" (URuitkHostWidget)
// into your EXISTING UserWidget layout and set ComponentName to a registered
// component. Nothing else about your screen changes.
RUITK_COMPONENT(InventoryPanel);   // compiled .uetkx components self-register; short names resolve when unambiguous`

const SCREEN = `// Stage 2 — whole screens. Your CommonUI stack stays; the screen it pushes
// is now ours (see the CommonUI guide):
Stack->AddWidgetInstance(*CreateWidget<URuitkActivatableScreen>(OwningPlayer, ScreenClass));

// Or overlay screens with no UMG wrapper at all (Blueprint-callable):
World->GetSubsystem<URuitkWorldSubsystem>()->MountNamed(TEXT("PauseMenu"), /*ZOrder*/ 10);`

const INVERT = `// Stage 3 — inversion: Reactive UI Toolkit owns the tree; what remains of the old UI
// lives INSIDE it, and your viewmodels keep feeding data unchanged.
{ Ruitk::Umg::UserWidget(ULegacyMinimapWidget::StaticClass(), World) }
const float Health = Ruitk::Umg::UseField<float>(Ctx, Vm, FName(TEXT("Health")), 100.0f);`

const CONVERT = `// Hand-written C++ component (verbose builder calls)...
FRuitkNodeArray ScoreRow(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [Score, SetScore] = Ctx.UseState<int32>(0);
	FRuitkButtonProps P;
	P.SetContentPadding(FMargin(12, 4));
	// ...pages of props structs and Ch.Add(...)...
}

// ...becomes a .uetkx file (same reconciler, same hooks — readable markup):
export FRuitkNode ScoreRow() {
	auto [Score, SetScore] = UseState<int32>(0);
	return (
		<HorizontalBox>
			<TextBlock Text={ Ruitk::Fmt(TEXT("Score: {}"), Score) } />
			<Button OnClicked={ SetScore(Score + 10) } ContentPadding="12,4">+10</Button>
		</HorizontalBox>
	);
}`

const BRAND_RUN = `<Engine>\\UnrealEditor-Cmd.exe <proj>.uproject -run=RuitkMigrateBrand
<Engine>\\UnrealEditor-Cmd.exe <proj>.uproject -run=RuitkCompile -check`

const CORE_REDIRECTS = `[CoreRedirects]
+ClassRedirects=(OldName="/Script/ReactiveUIUMG.RuiHostWidget",NewName="/Script/RuitkUMG.RuitkHostWidget")
+ClassRedirects=(OldName="/Script/ReactiveUIUMG.RuiSignalViewModel",NewName="/Script/RuitkUMG.RuitkSignalViewModel")
+ClassRedirects=(OldName="/Script/ReactiveUIUMG.RuiWorldSubsystem",NewName="/Script/RuitkUMG.RuitkWorldSubsystem")
+ClassRedirects=(OldName="/Script/ReactiveUICommonUI.RuiActivatableScreen",NewName="/Script/RuitkCommonUI.RuitkActivatableScreen")
+ClassRedirects=(OldName="/Script/ReactiveUIMVVMBridge.RuiMvvmViewModel",NewName="/Script/RuitkMVVMBridge.RuitkMvvmViewModel")`

const RENAME_TABLE: Array<[string, string]> = [
  ['Plugins/ReactiveUI/ + ReactiveUI.uplugin', 'Plugins/ReactiveUIToolkit/ + ReactiveUIToolkit.uplugin'],
  ['ReactiveUI<Mod> modules (ReactiveUICore, …)', 'Ruitk<Mod> (RuitkCore, RuitkSlate, RuitkUMG, …) — Build.cs deps + RUITK<MOD>_API'],
  ['FRui* / URui* / SRui* / TRui* / IRui* / ERui* / ARui*', 'FRuitk* / URuitk* / … (FRuiNode → FRuitkNode)'],
  ['RUI:: / namespace RUI', 'Ruitk:: / namespace Ruitk'],
  ['RUI_* macros (RUI_PROP, RUI_COMPONENT, …)', 'RUITK_* (RUITK_PROP, RUITK_COMPONENT, …)'],
]

export const MigrationPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Migration &amp; Adoption
    </Typography>
    <Typography variant="body1" paragraph>
      Nobody rewrites a shipping UI. Reactive UI Toolkit is adopted <strong>incrementally</strong> — it
      emits the widgets Epic&apos;s stack already governs, so every stage below ships on its own
      and nothing forces the next one.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Stage 1 — leaf islands (a panel inside your existing screens)
    </Typography>
    <Typography variant="body1" paragraph>
      Start with one self-contained panel — an inventory grid, a stat block, a settings group.
      Author it as a <code>.uetkx</code> component and drop a <code>URuitkHostWidget</code> where the
      old panel sat. Your screen, your stack, your input handling — one reactive island inside it.
    </Typography>
    <CodeBlock code={LEAF} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Stage 2 — whole screens (their stacks, our screens)
    </Typography>
    <Typography variant="body1" paragraph>
      When a full screen is worth it, push a <code>URuitkActivatableScreen</code> onto your existing
      CommonUI stack — activation, back-handling and input routing stay CommonUI&apos;s job — or
      mount overlays straight into the viewport with <code>MountNamed</code>.
    </Typography>
    <CodeBlock code={SCREEN} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Stage 3 — inversion (ours hosting theirs)
    </Typography>
    <Typography variant="body1" paragraph>
      Eventually the tree flips: Reactive UI Toolkit owns the screen, surviving UMG widgets embed via{' '}
      <code>Ruitk::Umg::UserWidget</code>, and your FieldNotify viewmodels keep feeding data through{' '}
      <code>UseField</code> — they own values, we own structure. No stage ever requires deleting
      working UI.
    </Typography>
    <CodeBlock code={INVERT} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Converting hand-written components to <code>.uetkx</code>
    </Typography>
    <Typography variant="body1" paragraph>
      If you started with C++ <code>Ruitk::</code> builder calls, moving to markup is mechanical —
      the reconciler and hooks are identical; only the authoring changes. The demo gallery&apos;s
      19 screens are all compiled <code>.uetkx</code> and double as conversion references.
    </Typography>
    <CodeBlock code={CONVERT} language="uetkx" />

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Upgrading to 0.15 — the Reactive UI Toolkit rename
    </Typography>
    <Typography variant="body1" paragraph>
      0.15.0 renames the product and <strong>every public identifier</strong> to the family&apos;s
      umbrella name, <strong>Reactive UI Toolkit</strong> — behaviorally it is 0.14.0 with new
      names, but a 0.14 project will not compile until migrated. The rename is mechanical:
    </Typography>
    <TableContainer sx={{ mb: 2 }}>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell>0.14</TableCell>
            <TableCell>0.15</TableCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {RENAME_TABLE.map(([oldForm, newForm]) => (
            <TableRow key={oldForm}>
              <TableCell>
                <code>{oldForm}</code>
              </TableCell>
              <TableCell>
                <code>{newForm}</code>
              </TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </TableContainer>
    <Typography variant="body1" paragraph>
      Install the new plugin folder, then one idempotent codemod rewrites your{' '}
      <code>.uetkx</code> files, <code>Build.cs</code> dependencies, <code>#include</code> paths,
      and the <code>.uproject</code> plugin reference; the compile gate proves you&apos;re done:
    </Typography>
    <CodeBlock code={BRAND_RUN} language="bash" />
    <Typography variant="body1" paragraph>
      If your assets or Blueprints reference the renamed reflected classes, paste the redirects
      into <code>Config/DefaultEngine.ini</code> and resave them once:
    </Typography>
    <CodeBlock code={CORE_REDIRECTS} language="ini" />
    <Alert severity="info" sx={{ mb: 2 }}>
      Full detail — the complete rule set, the automation-suite (<code>Ruitk.*</code>) and
      commandlet (<code>-run=Ruitk*</code>) renames, <code>ruitk.*</code> console variables, and
      the license retitle to the Reactive UI Toolkit Community License 1.1 — lives in{' '}
      <a href={`${GITHUB_URL}/blob/HEAD/MIGRATION-0.15.md`} target="_blank" rel="noreferrer">
        MIGRATION-0.15.md
      </a>{' '}
      in the repo.
    </Alert>

    <Alert severity="info" sx={{ mt: 2 }}>
      Upgrading an existing <code>.uetkx</code> tree to the ES-modules grammar (strict imports
      included) is one command — <code>-run=RuitkMigrateEsModules</code> — covered on the{' '}
      <strong>Imports &amp; exports</strong> page. Version-to-version API changes follow the deprecation policy in{' '}
      <code>VERSIONING.md</code> (one minor version of warnings before removal) and are always
      listed in the CHANGELOG.
    </Alert>
  </Box>
)
