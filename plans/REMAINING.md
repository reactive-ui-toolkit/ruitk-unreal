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

## 1. The HMR field test — owner-driven, IN PROGRESS (blocks the release runway)

*(Protocol history in `archive/HMR_FIELD_TEST.md`. HMR v2 is a Start/Stop MODE:
Window ▸ ReactiveUetkx Hot Reload, or `ReactiveUetkx.HMR.Start`/`.Stop`; status in
MessageLog ▸ "ReactiveUI". Owner-only — Live Coding needs a running editor. A failure gets
ledgered in [TESTING_BUGS.md](TESTING_BUGS.md).)*

Passed so far (owner, 2026-07-25): **8** (Stop → external build → restart), **9** (PIE
stop/start under HMR), **5b** (same-name export across files — independent per-file edits
confirmed). Verbally exercised across rounds 1–16 but never formally ticked: 1–7 — re-run
any that feel unproven, in one sitting:

- [ ] **1. Plain edit, state preserved.** Drive `SimpleCounter` to a non-default count; edit
      visible text/color, save. ✅ UI updates after the patch; the count SURVIVES.
- [ ] **2. Hook-order-sensitive state.** `UseMemo`/`UseRef` before two `UseState`s; drive
      both; cosmetic save. ✅ Both keep their values, no cross-seeding.
- [ ] **2b. Hook-shape EDIT (TB-13).** Add/remove a hook call and save. ✅ MessageLog
      `hook shape changed … state reset`; DEFAULT values, never a neighbor's.
- [ ] **3. Cross-file: edit an IMPORTED component** (`LabCard`/`DemoContextPanel`). ✅ Every
      user re-renders patched; importers' state survives.
- [ ] **4. Structural edit** (add/remove a widget). ✅ Changed subtree remounts (its local
      state resets — React semantics); siblings/ancestors keep state.
- [ ] **5. Broken save while active** (typo + wrong-cased `slot.fill`). ✅ 0106/0112 live in
      VS Code + MessageLog on sweep; UI keeps LAST GOOD; ONE Recent-Errors row growing
      `(still failing ×N)`; counter drops to 0 on recovery.
      ⚠ Known: the watcher deletes the `.uetkx.inl` on a failed markup compile — the next
      good save regenerates it (never commit in that window).
- [ ] **6. Fix the break, save again.** ✅ Patch lands, same state — the round-trip is free.
- [ ] **7. Rapid saves (debounce).** 3–4 quick saves. ✅ One coherent final patch — and ONE
      crisp toast updating its text in place (TB-26's fix, built 2026-07-25, needs eyes).

File-manipulation legs (the watcher's Added/Removed/Rename paths; run each once WITH HMR
active, spot-check 10a/10c with it stopped):

- [ ] **10a. Create a new component file + wire it in** (`CounterBadge.uetkx` export →
      import + render in SimpleCounter, save both). ✅ HMR on: sweep compiles BOTH, one
      patch, badge appears live. HMR off: `.inl`s regenerate; nothing patches until
      Ctrl+Alt+F11 + remount.
- [ ] **10b. COPY a file (FILE_SCOPED_EXPORTS: now LEGAL).** Copy `SimpleCounter.style.uetkx`
      → `SimpleCounterCopy.style.uetkx`. ✅ NO diagnostic (2106 retired); both compile clean.
      Only importing the same name from both into ONE file without `as` errors (2303).
      Delete the copy → clean sweep, orphan `.inl` removed. Bonus: renaming the copy
      remounts its components (G-01 — identity is path-derived).
- [ ] **10c. RENAME a file** without touching the import. ✅ Importer flags the dead
      specifier live (2300/2302); sweep errors; UI keeps last good; fixing the specifier
      recovers in one save.
- [ ] **10d. DELETE an imported file.** ✅ Same failure shape live + sweep; orphan sweep
      unregisters the dead component; removing import + usage recovers; no ghost tag left
      in completion.
- [ ] **10e. Rename an EXPORT (not the file)** without updating the importer. ✅ 2302 live
      naming the exporter; recovery on either side in one save.
- [ ] **10f. New file while HMR is STOPPED, then Start.** ✅ First save after Start patches
      it in.

**Done ⇒** note the session (date, engine version) in `PENDING_CHANGELOG.md`'s next drain
and proceed to §2.

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
