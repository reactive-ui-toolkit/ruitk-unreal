# Pending changelog ledger

One bullet per merged user-relevant change, staged by the `plan-progress` skill at
phase/milestone completion — while the knowledge is fresh. **Drained by `release-process` §0**
into the real lanes (A: root CHANGELOG.md · B: `changelog.mjs add` · C: Discord) and then
EMPTIED. Bullets that never reach a lane are how release notes go missing — drain, don't append
around.

**Format:** `- [lane A|B|C] [artifact] one-line summary (PR #n / sha)`

<!-- Drained 2026-07-16 for the widget-completion releases (plugin 0.5.0→0.9.0 + extensions
     0.3.0): every bullet verified present in Lane A ([0.5.0]/[0.6.0]/[0.7.0]/[0.8.0]/[0.9.0]
     sections), Lane B (changelog.json 0.3.0 — schema catch-up + sinceUE + wave G), or
     Lane C (DISCORD_CHANGELOG [0.5.0]→[0.9.0]). -->
<!-- Drained 2026-07-16 for the extension-listing release (extensions 0.3.1, no plugin
     bump): both bullets went straight into Lane B (changelog.json 0.3.1 — listing overhaul
     shared bullet + vscode README bullet) via plans/archive/EXTENSION_LISTING_PLAN.md's execution;
     no Lane A/C entries apply (listing-only change, not a Discord-worthy release). -->
<!-- Drained 2026-07-16 for the include-retirement release (plugin 0.10.0, extensions 0.4.0):
     the single bullet went straight into Lane A ([0.10.0]), Lane B (changelog.json 0.4.0 —
     shared entry), and Lane C (DISCORD_CHANGELOG [0.10.0]) as it was authored — nothing was
     staged here mid-campaign, so there was nothing to drain from THIS file, only to verify. -->
<!-- Drained 2026-07-17 for the markup-everywhere release (plugin 0.11.0, extensions 0.5.0):
     the §4 grammar bullet went into Lane A ([0.11.0] Added/Changed) + Lane C
     (DISCORD_CHANGELOG [0.11.0]); the Message Log menu bullet into Lane A ([0.11.0] Added);
     the LSP-mirror, crash-proofing/§2-channel, and §5 bundled-clangd bullets into Lane B
     (changelog.json 2026-07-17 shared entries, vscode+vs2022 0.5.0). -->
<!-- Drained 2026-07-18 for the ES-modules release (plugin 0.12.0, extensions 0.6.0): the
     campaign wrote its lanes directly at M8 (nothing was staged here mid-campaign) — Lane A
     ([0.12.0] Added/Changed/Fixed), Lane B (changelog.json shared entries: ES-modules LSP,
     TD-024 sidecar gate, Value.-payload ordering — vscode+vs2022 0.6.0), Lane C
     (DISCORD_CHANGELOG [0.12.0], 1895 chars). -->
(empty — drained 2026-07-18)

<!-- Drained 2026-07-25 for the field-test-campaign release (plugin 0.14.0, extensions 0.8.0):
     every staged bullet verified present in Lane A (CHANGELOG.md [0.14.0] — FSE, return null,
     the 0106/0109-0112/2311-2313/2329 validation wave, TB-13/14/15/21/23/26/29 fixes, DoomHUD),
     Lane B (changelog.json 2026-07-25 entry, 13 shared bullets — vscode+vs2022 0.8.0), or
     Lane C (DISCORD_CHANGELOG [0.14.0], 1868 chars). Field-test sessions: UE 5.6 full matrix
     owner-PASS 2026-07-25; UE 5.7 battery 132/132 same day (matrix pending); 5.8 pending
     engine repair. -->
(empty — drained 2026-07-25)

<!-- Drained 2026-07-28 for the rebrand release (plugin 0.15.0, extensions 0.9.0): nothing was
     staged here (the ledger was empty since 2026-07-25); the rebrand campaign wrote its lanes
     directly — Lane A (CHANGELOG.md [0.15.0] BREAKING section + mirror), Lane B
     (changelog.json 2026-07-28 entry, 4 shared bullets — vscode+vs2022 0.9.0), Lane C
     (DISCORD_CHANGELOG [0.15.0]). -->

<!-- Drained 2026-07-31 for the unified-settings release (plugin 0.16.0, extensions 0.9.1,
     staged-unpublished): both lane-A bullets verified present in Lane A (CHANGELOG.md
     [0.16.0] — URuitkSettings/persistable CVars under Added, the settings window under
     Added, the HMR-window slimdown + "Reactive UI Toolkit — Editor" page rename under
     Changed; mirror resynced); the lane-C bullet became DISCORD_CHANGELOG [0.16.0],
     written at staging time per the 0.15.0 precedent (owner pastes after publishing).
     Lane B this wave (changelog.json 2026-07-31, vscode+vs2022 0.9.1 — the TB-31
     embedded-clangd false-positive suppression) was authored directly at release prep;
     nothing extension-side was staged here, so there was nothing to drain for B. -->
- [lane A] [plugin] FlushSync is now genuinely "synchronously and unsliced" (family-parity M1/P-06): a scoped force-sync pass + run-to-quiescence loop replaces the voided `bWasSlicing` no-op — with time slicing on, mount surfaces/HMR/item-model row roots can no longer return with parked uncommitted WIP; regression test `Ruitk.Core.FlushSyncUnderSlicing` (PR #53)
- [lane A] [plugin] `FRuitkScheduler` (family-parity M2/P-01..P-03): the family render scheduler ported from ruitk-unity `RenderScheduler.cs` — High/Normal/Low/Idle lanes on a per-frame budget cumulative across lanes, frame-start Low-cancel, High-starvation escalation counting, idle budget/2 gate + sub-budget, per-lane key dedup (C++ has no delegate identity), batch-deferred non-High enqueues, UNBUDGETED frame-end batched-effects flush, injectable clock; owned by the Slate host and pumped once per PreTick, surfaced via `IRuitkHostConfig::GetScheduler()`; `Ruitk.Scheduler` suite (12 tests, fake clock, no sleeps) (PR #53)
- [lane A] [plugin] Defer-don't-restart (family-parity M3/P-11, `FiberReconciler.cs:304-325`/`:884-909` port): a setState arriving mid-render, mid-park, or mid-commit now DEFERS and replays post-commit as ONE coalesced follow-up render — the restart-from-root machinery (25-restart guard) is DELETED (restarting starved large trees under sustained per-frame updates and leaked the aborted walk's fibers); superseded-tree updates redirect to the live twins, detached-fiber updates warn and bail, and a runaway setState-during-render cascade now hits a render-depth-25 guard (LogError + render-nothing for the pass — no crash, no hang; error-boundary activation keeps its own bounded rebuild-from-root); with time slicing on, render passes run as self-re-enqueueing Slice actions on the scheduler's Normal lane under the NEW `ruitk.TimeSliceMs` quantum CVar (default 2.0 ms; the per-frame budget is the scheduler's) and the passive-effect flush rides the frame-end batched-effects lane; FlushSync additionally claims/cancels the queued Slice; mount stays ALWAYS synchronous; defaults unchanged (`ruitk.TimeSlicing` still off — the flip is M8); `Ruitk.Core.Defer*`/`Slice*` suite (10 tests, both worlds via CVar fixture) (PR #53)
- [lane A] [plugin] Knob surface completed (family-parity M4): `ruitk.TimeSliceMs` gets its `URuitkSettings` row (Performance, clamp 0.1..16 ms); **`ruitk.FrameBudgetMs` re-pointed to the scheduler meaning with default 8.0 → 4.0** (per-frame budget cumulative across lanes — the per-slice quantum is now `ruitk.TimeSliceMs`; a saved 8.0 stays valid, it is simply a more generous budget, and the editor logs a one-shot notice when a project's saved value still equals the old default); `ruitk.TimeSlicing` help text now names the scheduler-bypass semantics; settings defaults-equality test locks the new ctor↔CVar pairs (behavior at defaults still unchanged — `ruitk.TimeSlicing` remains off until the flip) (PR #53)
- [lane A] [plugin] `ruitk.StrictDiagnostics` is now real (family-parity M5/P-10 — the knob existed but gated nothing): the two family misuse warnings, prefixed `[Ruitk][strict]` and deduped per component — (1) a state update during the component's OWN render (per-component discriminator via the finally-consumed `bIsRendering`; a parent's setter fired from a child's render is the legal lift-state-up shape and stays silent; behavior is unchanged — the update still defers per P-11), and (2) an effect/layout effect registered with no dependency array (`Ruitk::EveryCommit()`; library-internal effects are exempt via `InternalUseEffect` so users are never blamed for plumbing they didn't write). StrictMode's double invoke counts diagnostics ONCE and the post-commit deferred replay can never re-warn; dev builds default on, Shipping off (unchanged compiled defaults); `Ruitk.Core.StrictDiagnostics*` suite (4 tests) (PR #53)
- [lane A] [plugin] Environment label (family-parity M6/P-07, family knob 10): NEW `ruitk.Environment` (0=auto/1=development/2=production, default auto = development in any non-shipping build, production in Shipping) surfaced READ-ONLY to components as `Ctx.GetEnvironment()` (`ERuitkEnvironment`) — a plain accessor, NOT a hook (no slot, no subscription; changing it never re-renders by itself) — for YOUR dev-vs-prod branches (debug overlays, dev panels); the library itself never branches on it (grep-gated); `URuitkSettings::Environment` enum row (Development category) + push; `Ruitk.Core.Environment*` tests (auto/override/out-of-range resolution, in-render visibility, read-only clause) (PR #53)
- [lane A] [plugin] The trace ladder (family-parity M7/P-08, family knobs 8 + 9): NEW `ruitk.TraceLevel` (0=None/1=Basic/2=Verbose, default None) + NEW `ruitk.DiffTracing` (bool, default false) — Basic emits structural events on the NEW dedicated `LogRuitkTrace` category (placements, updates, deletions, node-replacement decisions, per-commit summaries with counts); Verbose adds per-element detail (element type/ComponentId/key on each structural line) and per-hook detail (effect captures, state/reducer writes) and implies diff tracing; `ruitk.DiffTracing` is the INDEPENDENT OR-switch for reconciler diff-decision logs (`[Ruitk][diff]` bailout/subtree-skip/render verdicts with the props-equal breakdown + child-reconciliation tiers fast-leaf/keys-stable/full-keyed/positional). One category carries all trace output (`log LogRuitkTrace off` silences it); every emission early-outs before formatting when off; `stat Ruitk` and the diagnostics counters are untouched; `URuitkSettings` rows (`TraceLevel` enum + `bDiffTracing`, Development category) + push + parity rows; `Ruitk.Core.Trace*` suite (5 tests incl. the family six-row gate matrix, exact per-kind counts in both slicing worlds) (PR #53)
