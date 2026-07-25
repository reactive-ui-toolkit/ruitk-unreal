# Remaining work — THE consolidated backlog (the only open-work file)

> **What this file is.** Every open work item across the project, in one place — consolidated
> 2026-07-25 (owner request) from the plan corpus after the finished campaign docs moved to
> `plans/archive/` (`FILE_SCOPED_EXPORTS_PLAN`, `F5_FIELD_TEST_BUGS`, both
> `OWNER_ACCEPTANCE_CHECKLIST`s, `HMR_FIELD_TEST` — their open items live HERE now).
> Division of labor between the living docs: [MASTER_PLAN.md](MASTER_PLAN.md) holds the locked
> decisions (D-01..D-33) and the phase record; [ROADMAP.md](ROADMAP.md) is the plain-English
> status table; [TECH_DEBT.md](TECH_DEBT.md) keeps the per-item deep entries (TD-###);
> [TESTING_BUGS.md](TESTING_BUGS.md) is the LIVE field-test bug ledger (TB-19..TB-28 —
> new field-test findings go there, round pattern: symptom → root cause → fix → pins);
> [WIDGET_INVENTORY.md](WIDGET_INVENTORY.md) is the authoritative per-widget tracker.
> **When an item finishes, strike it here** (and resolve its TD entry in the same PR).
>
> **Baseline evidence (2026-07-25, this consolidation, `fix/lsp-field-test-false-positives`):**
> battery 129/132 — the 3 failures are the owner's in-flight SimpleCounter/CounterBadge test
> files (unconditional early fragment return renders nothing; Acceptance sweep counts 46),
> not regressions · LSP 94/94 + smoke PASSED · contract goldens 36/36 · all node gates green ·
> TB-26 toast fix BUILT.

## 1. The HMR field test — UE 5.6 COMPLETE; 5.7 + 5.8 legs remain (block the release runway)

*(Protocol history in `archive/HMR_FIELD_TEST.md`. HMR v2 is a Start/Stop MODE:
Window ▸ ReactiveUetkx Hot Reload, or `ReactiveUetkx.HMR.Start`/`.Stop`; status in
MessageLog ▸ "ReactiveUI". Owner-only — Live Coding needs a running editor. A failure gets
ledgered in [TESTING_BUGS.md](TESTING_BUGS.md).)*

- [x] **UE 5.6 — the full matrix PASSED** (owner declaration 2026-07-25, after the rounds
      1–16 fix campaign): items 1–7 (plain/hook-order/hook-shape/cross-file/structural/
      broken-save/recovery/debounce), 5b (same-name exports per-file), 8 (stop → external
      build → restart), 9 (PIE cycle under HMR), and the 10-series file-manipulation legs
      (create/copy/rename/delete/export-rename/born-outside-the-mode). Bugs found on the way
      are ledgered as TB-19..TB-28 (all FIXED, pinned, built).
- [ ] **UE 5.7 — repeat the matrix.** Engine payload needs REINSTALL first (folder shell
      only). Then: stale-UHT clean (CLAUDE.md environment facts), build, battery, walk the
      matrix (recreate the transient test vehicles per the items — the 5.6 ones were cleaned
      2026-07-25 and the battery re-verified green on the clean tree + rebuilt binary).
      ⚠ Lesson from the 5.6 wrap-up: after reverting/editing `.inl` files, REBUILD before
      trusting suite results — the suites run the compiled binary, not the files.
- [ ] **UE 5.8 — repeat the matrix** (payload intact; same sequence).

**All three engines done ⇒** note the sessions (dates, engine versions) in
`PENDING_CHANGELOG.md`'s next drain, and proceed to §2.

## 2. Release runway (Phase 9 — in order, after §1)

1. **Drain `PENDING_CHANGELOG.md`** (rounds 10–16 + FSE staged) via the `release-process`
   skill §0 → Lane A/B/C entries + version bumps (any lsp-server change bumps BOTH
   extensions).
2. **Per-engine zips** (`scripts/package-plugin.ps1` per engine, `-StrictIncludes`) +
   the **packaged-fidelity test** (fresh project, packaged plugin, banner + gallery
   renders). Needs UE 5.7 reinstalled for its leg (owner removed the payload — see memory).
3. **Fab listing + upload** (manual, owner; identity verification has lead time), rendered
   from `templates/fab-listing.template.md`; compliance items per D-29.
4. **Demo video** (AI storyboards, owner records) + showcase copy — the Doom demo is the
   vehicle (playtested 2026-07-16, 3 rounds).
5. **Discord announcement** (owner pastes; ≤2000 chars, `DISCORD_CHANGELOG.md`).

## 3. Interactive verification debt (needs eyes/hands — consolidated from both acceptance
## checklists, now archived; do opportunistically or fold into the §1 sitting)

- [ ] **PIE input/interop spot-checks** (from checklist v2 §B/§C/§E/§F): CMU-1 input-method
      switch + owning-player change (gamepad); ComboBox keyboard/gamepad picks +
      remove-selected-option; router `UseBlocker` on back/POP; asset-brush GC stress
      (`obj gc` behind a re-render loop).
- [ ] **Visual spot-checks** (v2 §D, optional but reassuring): string slot values, dynamic
      `Slot.Column` move, WrapBox no-reorder on slot change, NumericEntryBox clamp, `@theme`
      token restyle-not-rebuild, Separator pooling.
- [ ] **Interop smoke** (v1 checklist §F): ReactiveUI Host in a Blueprint UserWidget renders;
      PIE end leaves no lingering-root warnings; `UseField` re-renders on a FieldNotify VM.
- [ ] **VS2022 extension hands-on** (v1 §E — never formally verified): `build-local.ps1`,
      F5 → experimental instance, colors/completions/diagnostics on a `.uetkx`.
- [ ] **LSP niche re-checks** (v2 §I): multi-line import quick-fix, formatter keeps import
      comments, embedded intel with non-ASCII in strings.
- [ ] **Fresh-clone sanity** (v1 §G): clone → generate → build → battery on a second machine
      (or clean checkout).
- [ ] **CI engine arming decision** (v2 §H): set `RUI_CI_ENGINE_ARMED=true` + `EPIC_GHCR_PAT`
      and verify the armed leg, or consciously stay local-battery-only (current documented
      state: unarmed).
- [ ] **Owner setup:** re-add the branch ruleset with THIS repo's check names.
- **Accessibility screen-reader pass** — DEFERRED by owner 2026-07-15 (no screen reader);
  docs stay softened to what is verified; revisit at/after v1.

## 4. Product tails (small, tracked)

- **TD-011 tail**: construct-only prop CHANGES rebuild via `ReplaceWidget`; the
  reconstruct-mask audit stays a checklist item on the component pipeline.
- **TD-035 (2026-07-25)**: mirror the 5 return-null fileScan corpus cases + 3 formatter
  goldens to the Godot repo (PR), flag Unity; decide the directive-body
  `return null` → `continue;` leg (implement or `.pending`-pin).
- **TD-009 / TD-018**: the standing cross-repo corpus-mirroring PRs (process items).
- **Demo-scope decisions** (audit §9-Q5): which of todo / 5k-virtualized-inventory /
  world-space are v1-blocking vs v1.x gallery additions.
- **Q3 (owner call)**: `URuiWorldSubsystem : UWorldSubsystem` vs locked D-17's
  `UGameInstanceSubsystem` — bless the as-built design (banner it) or treat as a defect.

## 5. Post-v1 by locked decision (tracked, not forgotten — full entries in TECH_DEBT)

- **TD-005** Rider support (skipped for v1 — owner 2026-07-11).
- **TD-007** on-device remote reload (TCP).
- **TD-008** scripting adapters (UnrealSharp/AngelScript).
- **TD-HMR-XPLAT** live HMR is Windows-only (Live Coding); Hot Reload as the potential
  cross-platform path. **TD-HMR-DEMOS** the `ReactiveUetkx ▸ Demos` launcher submenu.
- **VS2022 `.uetkx` file icon** (VSSDK image-manifest; cosmetic).
- **TD-013 tail** typed authoring API follow-ons; **TD-015** deliberate v1 grammar cuts;
  **TD-016** event payload single-`Value` surface; **TD-019** hook-state value migration
  (superseded in practice by HMR v2); **TD-026** accepted interp-era divergences (record).
- **Combined imports vs the LEGACY hoist codemod**: `RUIMigrateImports`'s hoisted-module pass
  deliberately skips a COMBINED import's named part; the codemod's zero-diagnostics gate
  catches it. Revisit only on a field report.
