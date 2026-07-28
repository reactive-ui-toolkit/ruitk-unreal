import type { FC } from 'react'
import { Alert, Box, Table, TableBody, TableCell, TableContainer, TableHead, TableRow, Typography } from '@mui/material'
import { CodeBlock } from '../../components/CodeBlock/CodeBlock'

const ROOT = `#include "RuitkRoot.h"

// Three mount surfaces; keep the TSharedPtr alive for the UI's lifetime.
TSharedPtr<FRuitkRoot> Root = FRuitkRoot::CreateInViewport(Ruitk::FC(&MyComponent), /*ZOrder*/ 10);
FRuitkRoot::CreateInWindow(Window, Ruitk::FC(&MyTool));   // fill an SWindow
FRuitkRoot::Create(Ruitk::FC(&MyPanel));                   // detached — place GetWidget() yourself

Root->Update(NewTree);   // top-level re-render
Root->FlushSync();       // run coalesced work now
Root.Reset();            // unmount (cleanups run)`

type Entry = [string, string]
const CORE: Entry[] = [
  ['FRuitkRoot::Create / CreateInViewport / CreateInWindow', 'The three mount surfaces; plus Update / FlushSync / Unmount / GetWidget.'],
  ['URuitkWorldSubsystem::MountNamed(Name, Z) / MountNode / UnmountAll', 'World-scoped, Blueprint-callable mounting; roots tear down with the world.'],
  ['Ruitk::FC(&Component, props, children, key)', 'Instantiate a function component as a vnode.'],
  ['RUITK_COMPONENT(Fn) / Ruitk::RegisterNamedFactory / Ruitk::Named(Name)', 'The named-component registry — what ComponentName props (host widget, activatable screen, MountNamed) resolve against; compiled .uetkx components self-register.'],
  ['Ruitk::Fragment(children, key)', 'Group children with no wrapper widget.'],
  ['Ruitk::Portal(target, children, key)', 'Render children into an out-of-tree Slate target (see Portals).'],
  ['Ruitk::Suspense(isReady, fallback, children, key)', 'Fallback until IsReady() flips true (see Suspense).'],
  ['Ruitk::ErrorBoundary(fallback, children, resetKey, onError, key)', 'Structural error boundary; components fail cooperatively via RUITK_RENDER_FAIL(...) / Ruitk::FailRender.'],
  ['Ruitk::Presence(children, maxExitSeconds, key)', 'Keep exiting keyed children mounted for exit animations; pair with UsePresence.'],
  ['Ruitk::Router / Ruitk::Routes / FRuitkRoute / Ruitk::Link', 'The in-memory router (see Router).'],
  ['Ruitk::Fmt(TEXT("... {} ..."), args...)', 'Type-generic text interpolation returning FText.'],
  ['Ruitk::Deps(a, b, ...) / Ruitk::EveryCommit()', 'Dependency lists for UseMemo / UseCallback / UseEffect.'],
]
const SLATE: Entry[] = [
  ['Ruitk::Slate::VerticalBox() / Button() / Border() / …', 'Host-element factories — one per Slate widget (usually written as tags).'],
  ['Ruitk::Slate::ListView / TileView / TreeView / MakeItemRenderer / MakeChildAccessor', 'The item-model views (render-prop APIs — C++-first, no markup tag); TreeView adds a child accessor, controlled expansion, and header Columns.'],
  ['Ruitk::Slate::PushNotification(Handle, Text, Duration)', 'Fire a toast at a mounted <NotificationList> (capture its handle via Ref).'],
  ['Ruitk::Slate::WidgetFromHandle<TWidget>(Handle)', 'Resolve a captured Ref/handle to its live typed Slate widget — the imperative escape hatch (ScrollToEnd()-class calls); pairs with UseImperativeHandle.'],
  ['Ruitk::Slate::MakeDrawFn(lambda)', 'Wrap a paint lambda into an FRuitkDrawFn for RuitkCanvas.'],
  ['Ruitk::Slate::UseShortcut(...) / FRuitkShortcut', 'Register keyboard shortcuts from a component.'],
]
const STATE: Entry[] = [
  ['Ruitk::GetOrCreateSignal<T>(Key, Init)', 'Get the process-wide signal for a key (creates on first use).'],
  ['Ruitk::UseSignalKey<T>(Ctx, Key, Default)', 'Subscribe a component to a keyed signal.'],
  ['ProvideContext(Key, Value) / UseContext(Key)', 'Provide and consume a value down a subtree.'],
]
const INTEROP: Entry[] = [
  ['URuitkHostWidget', 'Designer-placeable UMG widget hosting a named component ("our UI inside theirs").'],
  ['URuitkActivatableScreen', 'UCommonActivatableWidget hosting a named component; publishes activation + input method.'],
  ['Ruitk::Umg::UserWidget(Class, World)', 'Embed a UMG UUserWidget inside the tree ("theirs inside ours").'],
  ['Ruitk::Umg::UseField<T>(Ctx, VM, FName, Default)', 'Read a FieldNotify view-model field reactively.'],
  ['URuitkSignalViewModel', 'A FieldNotify viewmodel our code writes — UMG/MVVM views bind to our state.'],
  ['Ruitk::Mvvm::RegisterGlobalViewModel / FindGlobalViewModel', 'Register/resolve viewmodels in the MVVM global collection.'],
  ['Ruitk::CommonUI::ActivationContext() / UseActivation / UseIsActive / UseInputMethod', 'Drive and read CommonUI activation + input method.'],
]

const Section: FC<{ title: string; rows: Entry[] }> = ({ title, rows }) => (
  <>
    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      {title}
    </Typography>
    <TableContainer sx={{ mb: 2 }}>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell>Symbol</TableCell>
            <TableCell>Purpose</TableCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {rows.map(([sym, purpose]) => (
            <TableRow key={sym}>
              <TableCell sx={{ fontFamily: 'monospace', fontSize: '0.85em' }}>{sym}</TableCell>
              <TableCell>{purpose}</TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </TableContainer>
  </>
)

export const ApiReferencePage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      API Reference
    </Typography>
    <Typography variant="body1" paragraph>
      Everything lives under the <code>Ruitk::</code> namespace, split by host: <code>Ruitk::Slate</code>{' '}
      for the default Slate host, <code>Ruitk::Umg</code> and <code>Ruitk::CommonUI</code> for the Epic
      interop layers. Hooks are free functions (<code>UseState</code>, <code>UseEffect</code>, …) —
      see the <strong>Hooks Guide</strong> for the full set.
    </Typography>
    <CodeBlock code={ROOT} language="uetkx" />

    <Section title="Core" rows={CORE} />
    <Section title="Slate host" rows={SLATE} />
    <Section title="State sharing" rows={STATE} />
    <Section title="Epic interop" rows={INTEROP} />

    <Alert severity="info">
      In <code>.uetkx</code> markup you rarely call the <code>Ruitk::Slate::*</code> factories directly
      — you write tags. The factories are what the compiler emits, and what you reach for in
      hand-written C++.
    </Alert>
  </Box>
)
