import type { FC } from 'react'
import { Alert, Box, Chip, Table, TableBody, TableCell, TableContainer, TableHead, TableRow, Typography } from '@mui/material'

type Status = 'done' | 'progress' | 'planned'
const PHASES: Array<[string, string, Status, string]> = [
  ['1', 'Core reconciler + hooks', 'done', 'Fiber reconciler (subtree-skip, keyed diff, error boundaries), all hooks, signals + Suspense.'],
  ['2', 'Slate host + first widgets', 'done', 'Adapter registry, event proxies, mount surfaces, the first widget set, style v1, demo gallery.'],
  ['3', '.uetkx compiler + build', 'done', 'Lexer/parser → committed reflection-free .inl, schema sidecars, RuitkCompile commandlets, formatter.'],
  ['4', 'Hot reload', 'done', 'Live Coding-driven HMR, editor watcher, the Reactive UI Toolkit window, status line.'],
  ['5', 'IDE extensions', 'done', 'uetkx-language-server + VS Code and VS2022 extensions (completion, hover, diagnostics, formatting).'],
  ['6', 'UMG / CommonUI / MVVM interop', 'done', 'URuitkHostWidget, Ruitk::Umg::UserWidget, UseField, CommonUI activatables, MVVM collection, UMG prop-map.'],
  // `media` was struck by the 2026-07-14 audit — only UseSfx exists, so the note says so.
  ['7', 'Production gaps', 'done', 'Virtualized lists, focus, animation + SFX hooks, portals, drag-and-drop, widget batch 2, localization (gather + live culture switch).'],
  ['8', 'Demos, docs, benchmarks', 'done', 'Demo gallery, Doom demo, benchmark baselines, and this docs site — built out and committed.'],
  ['9', 'Release & publishing', 'progress', 'Owner-gated: per-engine packages, the Fab listing and demo video, and the v1 ship gate.'],
]

const LABEL: Record<Status, { text: string; color: 'success' | 'warning' | 'default' }> = {
  done: { text: 'Done', color: 'success' },
  progress: { text: 'In progress', color: 'warning' },
  planned: { text: 'Planned', color: 'default' },
}

export const RoadmapPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Roadmap
    </Typography>
    <Typography variant="body1" paragraph>
      The living status of the project, mirrored from <code>plans/ROADMAP.md</code>. The runtime,
      compiler, tooling, Epic interop, demos and these docs are built; release and publishing are
      the remaining work.
    </Typography>

    <TableContainer sx={{ mb: 2 }}>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell>#</TableCell>
            <TableCell>Phase</TableCell>
            <TableCell>Status</TableCell>
            <TableCell>Notes</TableCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {PHASES.map(([n, phase, status, notes]) => (
            <TableRow key={n}>
              <TableCell>{n}</TableCell>
              <TableCell>
                <strong>{phase}</strong>
              </TableCell>
              <TableCell>
                <Chip size="small" label={LABEL[status].text} color={LABEL[status].color} />
              </TableCell>
              <TableCell>{notes}</TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </TableContainer>

    <Alert severity="info">
      Post-v1 subsystems and current limitations are tracked on the <strong>Known Issues</strong>{' '}
      page.
    </Alert>
  </Box>
)
