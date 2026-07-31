import type { FC } from 'react'
import { Alert, Box, Table, TableBody, TableCell, TableContainer, TableHead, TableRow, Typography } from '@mui/material'

const CVARS: Array<[string, string]> = [
  ['ruitk.StrictMode', 'Double-invoke render to surface impure components and effect mistakes (default off).'],
  ['ruitk.HookValidation', 'Detect hook-order mismatches between renders (default on in dev, off in shipping).'],
  [
    'ruitk.StrictDiagnostics',
    'The [Ruitk][strict] misuse warnings — state update during the component’s own render, effect with no dependency array — deduped per component (default on in dev, off in shipping).',
  ],
  [
    'ruitk.TimeSlicing',
    'Run render passes as time-sliced scheduler actions; commits stay atomic (default ON — set to 0 for fully synchronous single-pass renders).',
  ],
  ['ruitk.TimeSliceMs', 'The render quantum per slice (default 2.0 ms).'],
  ['ruitk.FrameBudgetMs', 'The scheduler’s per-frame budget, cumulative across lanes (default 4.0 ms).'],
  ['ruitk.HostNodePool', 'Recycle childless leaf widgets (turn off to bisect pooling suspicions; default on).'],
  [
    'ruitk.TraceLevel',
    '0=None/1=Basic/2=Verbose — structural reconciler events (placements, updates, deletions, replacements, commit summaries) on the LogRuitkTrace category; Verbose adds per-element and per-hook detail (default None).',
  ],
  [
    'ruitk.DiffTracing',
    'Reconciler diff-decision logs — bailout/subtree-skip verdicts, props-equal breakdown, child-reconciliation tiers — independent of the trace level; Verbose implies it (default off).',
  ],
  [
    'ruitk.Environment',
    'auto/development/production — read-only in components via Ctx.GetEnvironment() for your own dev-vs-prod branches; the library never branches on it (default auto: development in any non-shipping build).',
  ],
]

export const DebuggingPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Debugging Guide
    </Typography>
    <Typography variant="body1" paragraph>
      When a component misbehaves, Reactive UI Toolkit gives you three vantage points: the generated C++, the
      compiler&apos;s diagnostics, and the runtime CVars. Because the markup compiles to ordinary
      committed C++, everything you already use to debug Unreal — breakpoints, the log, Live Coding —
      works unchanged.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Read the generated code
    </Typography>
    <Typography variant="body1" paragraph>
      Each <code>.uetkx</code> compiles to a committed <code>*.uetkx.inl</code> beside it. It is
      plain, readable C++ — the <code>Ruitk::FC</code> / <code>Ruitk::Slate::*</code> calls your markup
      lowered to. Reading it is the fastest way to confirm what the compiler actually produced, and
      you can set breakpoints in it directly.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Compiler diagnostics
    </Typography>
    <Typography variant="body1" paragraph>
      Every problem carries a <code>UETKX</code> code (see <strong>Diagnostics</strong>), written to
      a gitignored <code>*.uetkx.diags.json</code> sidecar and surfaced in the Reactive UI Toolkit message
      log. The editor extensions show the same diagnostics inline as you type — restart the language
      server (<code>UETKX: Restart Language Server</code>) if they ever look stale.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      Runtime CVars &amp; stats
    </Typography>
    <Typography variant="body1" paragraph>
      <code>stat Ruitk</code> shows the live reconciler counters — Renders, Commits,
      Placements, Updates, Deletions — the fastest way to see whether a change re-renders more
      than it should. The <code>ruitk.*</code> console variables tune behavior:
    </Typography>
    <TableContainer sx={{ mb: 2 }}>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell>CVar</TableCell>
            <TableCell>What it shows</TableCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {CVARS.map(([cvar, effect]) => (
            <TableRow key={cvar}>
              <TableCell>
                <code>{cvar}</code>
              </TableCell>
              <TableCell>{effect}</TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </TableContainer>
    <Typography variant="body1" paragraph>
      The same ten CVars are also editable in the <strong>Reactive UI Toolkit ▸ Settings</strong>{' '}
      window&apos;s <em>Runtime</em> section (opened from the plugin&apos;s main menu; mirrored under{' '}
      <em>Project Settings ▸ Plugins ▸ Reactive UI Toolkit</em>) — edits apply live in the editor and
      persist to your project&apos;s <code>DefaultGame.ini</code> (which ships with packaged builds),
      while console, command-line, and ini overrides keep the last word.
    </Typography>
    <Typography variant="body1" paragraph>
      Since the family parity wave, <strong>time slicing is on by default</strong>: render work
      spreads across frames in 2 ms slices under a 4 ms per-frame budget, and every commit stays
      atomic — mount and <code>FlushSync</code> are always synchronous. Set{' '}
      <code>ruitk.TimeSlicing 0</code> (or untick it in Settings) to bisect a suspicion against the
      fully synchronous single-pass world. All trace output rides the single{' '}
      <code>LogRuitkTrace</code> category — <code>log LogRuitkTrace off</code> silences it without
      touching the level. Migration note: <code>ruitk.FrameBudgetMs</code> used to be the render
      budget itself (default 8.0) — it is now the scheduler&apos;s per-frame budget (default 4.0),
      and the per-slice quantum is <code>ruitk.TimeSliceMs</code>. A project that saved the old 8.0
      keeps working (a more generous budget, never wrong); the editor logs a one-shot notice while
      the saved value still equals the old default.
    </Typography>

    <Alert severity="warning">
      A hook called conditionally desyncs the positional slots — the most common source of
      &quot;wrong state after a re-render.&quot; If state seems to jump between components, check that
      every hook runs unconditionally at the top (see the <strong>Hooks Guide</strong>).
    </Alert>
  </Box>
)
