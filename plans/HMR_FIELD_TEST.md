# HMR field test — the pre-release protocol (HMR v2, Live-Coding-driven)

> **Why this file:** `OWNER_ACCEPTANCE_CHECKLIST_v2.md` §A describes the DELETED HMR v1
> (interpreter overrides, "compiled→interp swap") — do not test against it. HMR v2
> (plans/archive/HMR_V2_PLAN.md, `FUetkxHmrController`) is a Start/Stop MODE: while active,
> any `.uetkx` save regenerates the `.inl`, triggers a Live Coding compile, and on
> patch-complete every live reconciler root re-renders with the freshly-patched COMPILED
> code — full fidelity (imports, hooks, effects), state preserved (hook cells are
> heap-resident; a code patch never touches them). No interpreter, no per-edit branching.
>
> Owner-only: Live Coding needs a running editor + compiler — no headless test can force
> the patch event. Everything below the patch (watcher codegen, debounce, diagnostics) is
> already suite-covered.

## Pre-flight (once, before the session)

- [ ] Baseline green on the fix branch (last verified 2026-07-21, `cab7427`):
      Development Editor build ✓, `RUICompile -check` → 42 files / 0 errors ✓, suites ✓.
- [ ] **Close any external Build.bat loops** — while HMR is active, Live Coding owns the
      compile; external builds pause (the documented tradeoff, by design).
- [ ] Editor open, gallery map loaded, **PIE running**, VS Code (dev-host or installed
      extension) on the same checkout.

## Start / stop

- Window: **Window ▸ ReactiveUetkx Hot Reload** (nomad tab) — the Start/Stop panel; a
  checkbox hides the external Live Coding console window while HMR is active (an engine
  window we can't parent — hidden via SW_HIDE, restored on Stop).
- Console alternative: `ReactiveUetkx.HMR.Start` / `.Stop`.
- Status lines print to **MessageLog ▸ "ReactiveUI"**.

## The matrix (~15 min)

- [ ] **1. Plain edit, state preserved.** In PIE, drive `SimpleCounter` (or `ClickCounter`)
      to a non-default count. Edit visible text/color in its `.uetkx`, save. ✅ UI updates
      after the patch; the count SURVIVES. (Hook cells heap-resident — the core v2 claim.)
- [ ] **2. Hook-order-sensitive state.** Component with `UseMemo`/`UseRef` before two
      `UseState`s; drive both states; save a cosmetic edit. ✅ Both states keep their
      values, no cross-seeding (stable shape = untouched cells).
- [ ] **2b. Hook-shape EDIT (TB-13 — the family reset rule).** Drive the states, then save
      an edit that ADDS or REMOVES a hook call. ✅ MessageLog: `[ReactiveUI][HMR] <Comp>:
      hook shape changed by the edit (N -> M hooks) — state reset`, and the component
      re-renders with DEFAULT values — clean reset, never a neighbor's value. (Suite pin:
      `ReactiveUI.Hooks.HmrShapeReset`.)
- [ ] **3. Cross-file: edit an IMPORTED component.** Edit `LabCard`/`DemoContextPanel`
      (something imported by an open screen), save. ✅ Every screen using it re-renders
      patched; importing screens' state survives.
- [ ] **4. Structural edit.** Add/remove a widget in the tree (not just a prop), save.
      ✅ Patch lands; the changed subtree remounts (its OWN local state resets — expected
      React semantics for a structural change); siblings/ancestors keep state.
- [ ] **5. Broken save while HMR is active (the R10–R16 gates in the loop).** Save with a
      typo and a wrong-cased key (`slot.fill`). ✅ Both flag in VS Code as-you-type
      (0106/0112) and in MessageLog on the sweep; the running UI keeps the LAST GOOD patch.
      The HMR window shows ONE Recent-Errors row that grows `(still failing ×N)` — not a new
      row per sweep/alt-tab — and the Errors counter reads CURRENT errors (drops to 0 on
      recovery). FILE_SCOPED_EXPORTS (2026-07-24): a duplicate export is NO LONGER an error —
      see 5b.
      ⚠ Known: the watcher deletes the `.uetkx.inl` on a failed markup compile — the next
      GOOD save regenerates it (never commit in that window).
- [ ] **5b. Same-name export across files (FILE_SCOPED_EXPORTS M6a).** Rename an export so it
      matches one in ANOTHER file (e.g. SimpleCounter.style's `PanelBackground`, which now
      deliberately matches ContextDemo.style's). ✅ NO diagnostic anywhere (VS Code clean,
      sweep clean, Live Coding patch lands); both screens keep their own value. Edit EACH
      file's `PanelBackground` independently → each screen updates with its own color.
- [ ] **6. Fix the break, save again.** ✅ Patch lands, UI resumes from the same state —
      the error round-trip costs nothing.
- [ ] **7. Rapid saves (debounce).** Save 3–4 edits in quick succession. ✅ One coherent
      final patch (the watcher debounces); no half-applied UI.
- [x] **8. Stop → external build → restart.** (owner PASS 2026-07-25) Stop HMR, run the normal Build.bat, restart
      HMR. ✅ Both directions work; Stop restores the Live Coding console visibility.
- [x] **9. PIE stop/start under HMR.** (owner PASS 2026-07-25) Stop PIE, start PIE again with HMR still active,
      save an edit. ✅ Fresh session patches normally.

## File-manipulation matrix (owner request 2026-07-24 — the watcher's Added/Removed/Rename
## legs; run each once WITH HMR active, then spot-check 10a/10c with it stopped)

- [ ] **10a. Create a new component file + wire it in.** Create
      `Source/RuiDemo/Screens/SimpleCounter/CounterBadge.uetkx` exporting a tiny component
      (`export FRuiNode CounterBadge() { return ( <TextBlock Text="BADGE" /> ); }`), then
      import + render `<CounterBadge />` in SimpleCounter, save both. ✅ HMR on: the sweep
      compiles BOTH (new file scanned + registered, importer recompiled), one patch, badge
      appears live. HMR off: `.inl`s regenerate, nothing patches until F11 + remount.
- [ ] **10b. COPY a file (FILE_SCOPED_EXPORTS: now LEGAL).** Copy `SimpleCounter.style.uetkx`
      to `SimpleCounterCopy.style.uetkx` (same directory is fine). ✅ NO diagnostic — the copy
      is its own module; the sweep compiles both clean (2106 is retired). The ONE thing that
      still errors is importing the same name from both files into one importer WITHOUT an
      `as` alias (UETKX2303 on the second import). Delete the copy → clean sweep, orphan
      `.inl` removed. Bonus check: a file-rename of the copy remounts its components (G-01 —
      identity is path-derived).
- [ ] **10c. RENAME a file (the Added/Removed pair).** Rename `CounterBadge.uetkx` →
      `CounterBadgeX.uetkx` WITHOUT touching the import. ✅ The importer flags the dead
      specifier live (2300/2302); sweep errors; UI keeps last good. Fix the import
      specifier → recovers in one save. (The watcher sees rename as Removed+Added; the
      orphaned `CounterBadge.uetkx.inl` gets swept.)
- [ ] **10d. DELETE an imported file.** Delete `CounterBadgeX.uetkx` while SimpleCounter
      still imports it. ✅ Same failure shape as 10c live + in the sweep; the orphan sweep
      unregisters the dead component (`.inl` removed); removing the import + usage
      recovers. Confirm no ghost `CounterBadgeX` remains in tag completion afterward.
- [ ] **10e. Rename an EXPORT (not the file).** In `SimpleCounter.style.uetkx` rename one
      export and save WITHOUT updating the importer. ✅ Importer flags 2302 live naming the
      exporter file; recovery on either side works in one save.
- [ ] **10f. New file while HMR is STOPPED, then Start.** Create a valid new component
      file with HMR off (watcher regenerates its `.inl` silently), then Start HMR and wire
      it into a screen. ✅ First save after Start patches it in — the mode picks up files
      born outside it.

## What "done" means

All nine boxes green in one session ⇒ HMR v2 is field-verified; note the session (date,
engine version) in `plans/PENDING_CHANGELOG.md`'s next drain and proceed to
`release-process` (§0: the pending ledger currently holds the round 10–15 bullets).

## If something fails

Ledger it in `plans/F5_FIELD_TEST_BUGS.md` (the round pattern: symptom → root cause →
fix → pins) — same loop as rounds 1–15. MessageLog "ReactiveUI" + `LogRuiEditor` in the
Output Log are the first evidence to capture.
