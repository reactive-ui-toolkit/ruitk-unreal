import type { FC } from 'react'
import { Alert, Box, Table, TableBody, TableCell, TableContainer, TableHead, TableRow, Typography } from '@mui/material'

const PILLARS: Array<[string, string, string]> = [
  [
    'Slate',
    'The render target',
    'Our output is ordinary SWidgets — Widget Reflector, styling, and the whole Slate toolchain see normal widgets. No parallel renderer, no custom paint path.',
  ],
  [
    'UMG',
    'A door in both directions',
    'Designers drop our UI inside their UserWidgets (URuitkHostWidget); their widgets work inside our tree (Ruitk::Umg::UserWidget). See the UMG guide.',
  ],
  [
    'CommonUI',
    'Keeps owning menus & input',
    'Input routing, back-handling, and focus stay CommonUI’s job. Our screens are pushed onto their activatable stacks (URuitkActivatableScreen) and react via activation hooks. See the CommonUI guide.',
  ],
  [
    'MVVM / FieldNotify',
    'Feeds us data',
    'UseField(VM, "Health") re-renders the component when the field broadcasts. They own values; we own structure — and two bridges let their views bind our state back. See the MVVM guide.',
  ],
]

export const InteropOverviewPage: FC = () => (
  <Box>
    <Typography variant="h4" component="h1" gutterBottom>
      Integration Overview
    </Typography>
    <Typography variant="body1" paragraph>
      <strong>
        We are the layer that decides which Slate widgets exist each frame — everything of
        Epic&apos;s stays in place, either feeding us data or hosting our output.
      </strong>{' '}
      Reactive UI Toolkit replaces nothing: it emits the widgets Epic&apos;s stack already governs, so
      adopting it is incremental — a leaf panel first, a whole screen later, never a rewrite.
    </Typography>

    <TableContainer sx={{ my: 2 }}>
      <Table size="small">
        <TableHead>
          <TableRow>
            <TableCell>Layer</TableCell>
            <TableCell>Role</TableCell>
            <TableCell>How</TableCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {PILLARS.map(([layer, role, how]) => (
            <TableRow key={layer}>
              <TableCell>
                <strong>{layer}</strong>
              </TableCell>
              <TableCell>{role}</TableCell>
              <TableCell>{how}</TableCell>
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </TableContainer>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      The named-component registry — how hosts find your UI
    </Typography>
    <Typography variant="body1" paragraph>
      Every Epic-side host — <code>URuitkHostWidget</code>, <code>URuitkActivatableScreen</code>,{' '}
      <code>URuitkWorldSubsystem::MountNamed</code> — takes a <code>ComponentName</code> and resolves
      it against the <strong>named-component registry</strong>. Compiled <code>.uetkx</code>{' '}
      components self-register under a <em>file-qualified</em> id (every file is its own module,
      so two files may export the same component name); the <strong>short name still works</strong>{' '}
      whenever it is unambiguous — the registry resolves it by suffix. If two files export the
      same name, the host shows the qualified candidates and you paste one. Hand-written C++
      components register with <code>RUITK_COMPONENT(Fn)</code> or{' '}
      <code>Ruitk::RegisterNamedFactory</code>; <code>Ruitk::GetRegisteredFactoryNames</code>{' '}
      enumerates everything registered.
    </Typography>

    <Typography variant="h5" component="h2" gutterBottom sx={{ mt: 3 }}>
      See it live
    </Typography>
    <Typography variant="body1" paragraph>
      The demo gallery&apos;s <strong>Interop Showcase</strong> screen composes all four pillars in
      one tree — a UMG widget hosted inside, a FieldNotify viewmodel feeding data, CommonUI
      activation state driving a probe, all rendering to ordinary Slate. <strong>MvvmDemo</strong>{' '}
      and <strong>CommonUiDemo</strong> isolate the data and activation halves.
    </Typography>

    <Alert severity="info">
      Both doors are fully parameterizable: <code>URuitkHostWidget</code> takes designer/Blueprint
      <code>InitialProps</code> and a <code>ViewModel</code> (see the UMG guide), and a hosted
      tree designates its gamepad focus target with{' '}
      <code>Ruitk::CommonUI::UseDesiredFocus</code> (see the CommonUI guide).
    </Alert>
  </Box>
)
