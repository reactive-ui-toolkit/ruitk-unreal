# Family Parity — Unreal leg EXECUTION PLAN (scheduler / slicing / strict-diagnostics / trace / environment)

> **Status:** EXECUTION-GRADE, authored 2026-07-31 against the owner-locked FAMILY PARITY
> CONTRACT (§1 — rulings dated 2026-07-31, **not to be relitigated**). Reference
> implementation: **ruitk-unity**. Every file:line anchor below was verified against the
> working tree on 2026-07-31 (plugin **0.15.0** + the *uncommitted* unified-settings surface:
> `URuitkSettings`, `SRuitkSettingsPanel`, `RuitkSettingsTest`). Anchors into those
> uncommitted files WILL shift when that campaign merges — M0 re-verifies every anchor
> before the first edit (the ES-modules-plan precedent).
> **Branch:** a fresh `feat/family-parity` off `dev`, cut only AFTER the in-flight
> unified-settings campaign merges (this plan extends the very files it adds). If it has not
> merged when work starts: STOP AND ASK the owner for the cut point.
> **Vehicle:** one branch, one PR, ONE-GO campaign per the owner ruling — semantics port
> green under unchanged defaults first, then the default flip + coupling fixes in the SAME
> campaign. **Never commit unless the owner explicitly asks; NEVER push; no
> `Co-Authored-By`.**
> **Audience:** an AI session executing cold. Do not re-research settled decisions; follow
> research → develop → test → bughunt → fix per milestone (`dev-process` skill); never
> weaken a gate. The **full automation battery (133+ tests) must be green after EVERY
> milestone**, not just at the end.

---

## §1 — THE FAMILY PARITY CONTRACT (normative — substance identical across all three legs' plans)

Reference implementation: **ruitk-unity**. Owner rulings (2026-07-31), not to be relitigated:

- **All settings ship into builds**, defaults off/production — untouched builds behave as today.
- **One-go campaign**: semantics port green under unchanged defaults, then flip + coupling
  fixes, same campaign.
- **strict_mode**: all legs default OFF, opt-in, force-off-in-shipping.
- **No Unity-UI-Toolkit pooling** (deliberately removed 2025-11-17, state-bleed) — do not
  resurrect it anywhere; engine-local pooling that never had the bleed (our Slate leaf pool)
  stays.
- **exceptionControlFlow stays removed** family-wide (no analog, ever).
- **Basic trace level restored family-wide.**
- **Pool caps stay per-leg constants** (not knobs).

**Canonical knobs** (identical semantics and defaults everywhere; each leg spells the name
engine-natively):

1. **time_slicing** — bool, default **TRUE post-campaign** (false = scheduler bypass,
   synchronous single-pass).
2. **time_slice_ms** — float **2.0** — render-phase quantum, checked after each unit of
   work, no preemption (ruitk-unity/Shared/Core/Fiber/FiberReconciler.cs:31, ~:429-472).
3. **frame_budget_ms** — float **4.0** — scheduler per-frame budget, cumulative across
   lanes (ruitk-unity/Runtime/Core/RenderScheduler.cs:118-189).
4. **host_node_pool** — bool, default true.
5. **hook_validation** — tri-state **auto** (dev on / shipping off).
6. **strict_diagnostics** — tri-state **auto** — misuse warnings ONLY:
   state-update-during-render + missing dependency array, deduped per component.
7. **strict_mode** — bool, default false — double-invoke render fns, first result
   discarded, effects NOT double-invoked, diagnostics count once (**THIS repo is the family
   reference**: `Plugins/ReactiveUIToolkit/Source/RuitkCore/Private/RuitkReconciler.cpp:565-570`).
8. **trace_level** — None/Basic/Verbose, default **None** — Basic = structural events
   (placements/deletions/replacements/commits), Verbose = + per-element/per-hook detail.
9. **diff_tracing** — bool, default false — independent OR-switch for reconciler
   diff-decision logs.
10. **environment** — auto/development/production, default **auto** (editor-or-debug →
    development), surfaced READ-ONLY to user components, the library never branches on it.

Leg-specific extras are allowed only if marked "(engine-only)".

**Scheduler semantics** (port faithfully from ruitk-unity/Runtime/Core/RenderScheduler.cs):

- Four lanes: High / Normal / Low / Idle.
- Frame flow: High + Low both non-empty at frame start ⇒ the ENTIRE Low queue is
  **cancelled** (dropped, counted). High runs; Normal only if High drained (else an
  escalation is counted); Low runs; Idle only if nothing else ran + all three queues empty +
  elapsed < budget/2 — and Idle gets a budget/2 sub-budget.
- Per-lane enqueue dedup by delegate identity.
- Batched-effects flush is **UNBUDGETED** at frame end.
- Render passes are **self-re-enqueueing Slice actions** — a 2 ms slice can run twice
  within the 4 ms budget in one frame.
- **Mount is ALWAYS synchronous** (ruitk-unity/Shared/Core/Fiber/FiberReconciler.cs:125-129).

**Defer-don't-restart** (ruitk-unity/Shared/Core/Fiber/FiberReconciler.cs:304-325, :884-909):
mid-flight/parked updates are **deferred, never restart the pass**; replayed post-commit,
coalesced into ONE follow-up; commit-time updates use the same queue; guard flags
(`_isReplayingDeferred` analog); superseded-tree redirect + detached-fiber bail per the
reference (:254-289); the restart-from-root machinery is **REPLACED**; the runaway guard
becomes **render-depth 25 on setState-during-render**
(ruitk-unity/Shared/Core/Fiber/FiberFunctionComponent.cs:18, :140-155).

---

## §2 — Verified anchor table (verified 2026-07-31; re-verify any line that looks shifted — M0)

All paths repo-relative; the plugin source root is `Plugins/ReactiveUIToolkit/Source/`.

| Surface | File | Anchors |
|---|---|---|
| Reconciler (header) | `RuitkCore/Public/RuitkReconciler.h` | FlushSync doc "synchronously and unsliced" :49 (decl :50); restart fields `bRestart`/`bTickPending`/`RestartCount` :176-178; `MaxRestarts = 25` :180; `bIsCommitting` :173 + `DeferredUpdates` :174; header comment "setState mid-render restarts from the root (25-restart guard); setState mid-commit defers and replays" :19-20 |
| Reconciler (impl) | `RuitkCore/Private/RuitkReconciler.cpp` (1768 lines) | `ScheduleUpdateOnFiber` dirty-walk :130-142, commit-defer :143-147, **mid-render restart path :148-151**; `EnsureTick` → `Host.RequestFrame` :160-171; **`FlushSync` :173-183 — `bWasSlicing` captured :178 then voided `(void)bWasSlicing` :181, just calls `Tick()` which re-reads the CVar**; `Tick` restart guard :222-244 (abort log :229-234); **single-axis budget loop :246-277** (`bSliced`/`BudgetSec` from `FrameBudgetMs` :247-248, park via `EnsureTick` :275); `RenderComponent` `_begin`/`_end` writes `State->bIsRendering` :479/:504 (**unread today**), `Ruitk::SetRendering` :495/:503; **StrictMode double-invoke :565-570** (`RunOnce` twice, first result discarded; `FRuitkDiagnostics::OnRender()` counted ONCE at :579); error latch consume :572-577; SUBTREE-SKIP bailout region :396+; `ReconcileFiber` reuse/replace decision :722-725 (`OldFiber->Matches(VNode)`); `CommitRoot` :1166-1233 (**OnCommit :1169**, deferred replay :1224-1232); **OnPlacement :1261**; OnUpdate :1272; **OnDeletion :1277** |
| Config + diagnostics | `RuitkCore/Private/RuitkCoreMisc.cpp` | the six `ruitk.*` CVars :15-48 (`ruitk.TimeSlicing` false :15-17; `ruitk.FrameBudgetMs` **8.0** :19-21; `ruitk.HostNodePool` true :23-25; `ruitk.HookValidation` shipping-false/dev-true :27-34; `ruitk.StrictDiagnostics` shipping-false/dev-true :36-43; `ruitk.StrictMode` false :45-48); `FRuitkConfig` accessors :50-77 (**StrictMode force-off-in-shipping :70-77**); render-error latch :108-143; **`Ruitk::IsRendering()` :135-138 — ZERO callers repo-wide (grep-verified 2026-07-31)** |
| Config (header) | `RuitkCore/Public/RuitkCoreMisc.h` | `stat Ruitk` group :12-20 (`STATGROUP_Ruitk` :15); `FRuitkConfig` :26-42 (new accessors land here); `FRuitkDiagnostics` :48-105 (`Emit`/`bCapture` :52-56, `On*` counters :65-104); latch API :114-125; `RUITK_RENDER_FAIL` :127-128 |
| Per-component state | `RuitkCore/Public/RuitkComponentState.h` | `bIsRendering` :54 (written every render, never read) |
| WarnOnce dedup | `RuitkCore/Private/RuitkContext.cpp` | **`FRuitkContext::WarnOnce` :61-70** (`State.DiagWarned` FName set; `FRuitkDiagnostics::Emit` + `UE_LOG(LogRuitkCoreHooks, Warning, …)`); stub-warn usage precedent :95-99. Decl `RuitkCore/Public/RuitkContext.h:534` |
| Effect hooks (missing-deps site) | `RuitkCore/Public/RuitkContext.h` | `UseEffect` overloads :165/:186; `UseEffectImpl` :197; internal path :509-511 |
| FlushSync mount surfaces | `RuitkUMG/Private/RuitkHostWidget.cpp` :59/:93 · `RuitkUMG/Private/RuitkWorldSubsystem.cpp` :53 · `RuitkCommonUI/Private/RuitkActivatableScreen.cpp` :82/:106 · `RuitkSlate/Private/RuitkComboBox.cpp` :102/:107/:115 | every mount surface renders then `Root->FlushSync()` — all depend on M1 |
| FlushSync other callers | `RuitkSlate/Private/RuitkListView.cpp` :40/:70 · `RuitkSlate/Private/RuitkTreeView.cpp` :30/:57 · `RuitkSlate/Private/RuitkRoot.cpp` :75-79 · `RuitkEditor/Private/UetkxHmrController.cpp` :426 · `RuitkEditor/Private/UetkxPreview.cpp` :91 | item-model row roots + HMR + preview — the M8 coupling sweep universe |
| Slate host / frame pump | `RuitkSlate/Public/RuitkSlateHost.h` | frame-queue drain doc (PreTick batch rule) :93; **`PoolCapPerType = 256` :123 (stays a const)**; cap enforcement `RuitkSlateHost.cpp:231`; no-Slate-app queue note `RuitkSlateHost.cpp:433` |
| Fiber slab | `RuitkCore/Public/RuitkFiber.h` | `PageSize = 256` :183 (stays a const) |
| Settings (UNCOMMITTED — re-verify at M0) | `RuitkUMG/Public/RuitkSettings.h` + `Private/RuitkSettings.cpp` | `URuitkSettings : UDeveloperSettings`, `config=Game, defaultconfig` h:46-47; why-C++-defaults + why-not-BackedByCVars + why-RuitkUMG rationale h:7-33; the six mirrored UPROPERTYs h:74-101 (`FrameBudgetMs` clamp `UIMax=16` h:79); ctor defaults `bTimeSlicing=false` / `FrameBudgetMs=8.0f` cpp:12-13; `PushSettingsToCVars` push rows cpp:95-96; ini section is `[/Script/RuitkUMG.RuitkSettings]` in the project's `DefaultGame.ini` |
| Settings window (UNCOMMITTED) | `RuitkEditor/Private/SRuitkSettingsPanel.h/.cpp` | nomad tab; **two IDetailsView panels over the SAME settings CDOs** — a new UPROPERTY on `URuitkSettings` appears in both the tab and Project Settings with ZERO panel code |
| Log categories | plugin-wide | 26 `DEFINE_LOG_CATEGORY*`/`DECLARE_LOG_CATEGORY_EXTERN` occurrences across 25 files (grep 2026-07-31) — the "Log,All" composition question M7 answers |
| Tests | `Source/RuitkHostTests/Private/` | 136 `IMPLEMENT_*_AUTOMATION_TEST` macros across 55 files (incl. the uncommitted `RuitkSettingsTest.cpp`); harnesses `RuitkMockHost.h`, `RuitkSlateTestHarness.h` |
| Reference: scheduler | `ruitk-unity/Runtime/Core/RenderScheduler.cs` | lanes+trackers :10-17; `frameBudgetMs = 4.0f` :20; dedup enqueue :59-85; batch defer :54-58/:88-114; **frame flow :116-164** (Low-cancel :120-128, Normal-gated-on-High-drained :131-142, Idle gate + budget/2 :149-161, unbudgeted `FlushBatchedEffects` :162/:225-243); `ExecuteQueue` budget :166-204 (budget/2 via `allowOverBudget:false` :177); `PumpNow` unbudgeted drain :214-223; metrics :245-258 |
| Reference: reconciler | `ruitk-unity/Shared/Core/Fiber/FiberReconciler.cs` | `_isReplayingDeferred` guard :23; `_deferredUpdates` :29-30; **`TimeSliceMs = 2.0f` :31**; **mount-always-sync :125-129**; **superseded-tree redirect :254-281 + detached-fiber bail :282-289**; **commit-defer :304-309 + mid-flight defer :311-325** (with the starvation/leak rationale comment); sync-vs-scheduler dispatch :363-373; `WorkLoop` :380-400; **self-re-enqueueing `Slice` :405-424**; **`ProcessWorkUntilDeadline` :429-472** (quantum checked AFTER each unit :444-455 — no preemption); **replay-once-from-CommitRoot-finally :884-909** (`scheduleWork:false` re-mark loop :886-892, schedule ONCE :899-909) |
| Reference: depth guard | `ruitk-unity/Shared/Core/Fiber/FiberFunctionComponent.cs` | `[ThreadStatic] s_renderDepth` :16-17; **`MaxRenderDepth = 25` :18**; guard + error + null-child :140-155 |
| Reference: trace | `ruitk-unity/Shared/Core/Diagnostics/DiagnosticsConfig.cs` | `TraceLevel { None, Basic, Verbose }` :11-16; default None :21; **`EnableDiffTracing` independent bool :26**; (`UseExceptionBoundaryFlow` :32 exists there — DO NOT port, §9); Verbose-gated hook detail precedent `ruitk-unity/Shared/Core/Hooks.cs:1241` |

## §3 — Knob map: canonical name ↔ Unreal spelling ↔ state ↔ this plan

| # | Canonical | Unreal CVar | `URuitkSettings` property | Today | Post-campaign | Work |
|---|---|---|---|---|---|---|
| 1 | time_slicing | `ruitk.TimeSlicing` (exists) | `bTimeSlicing` | default **false**; false = fully synchronous | default **TRUE**; false = scheduler bypass, synchronous single-pass | M2-M4 port, **M8 flip** |
| 2 | time_slice_ms | `ruitk.TimeSliceMs` (**NEW**) | `TimeSliceMs` (NEW) | does not exist (the render quantum is conflated with `FrameBudgetMs`) | float **2.0**, render-phase quantum | M3/M4 |
| 3 | frame_budget_ms | `ruitk.FrameBudgetMs` (exists, **REPURPOSED**) | `FrameBudgetMs` | **8.0**, single-axis render-phase budget (`RuitkReconciler.cpp:246-277`) | **4.0**, scheduler per-frame budget cumulative across lanes | M2/M4 (+ ini migration story, P-05) |
| 4 | host_node_pool | `ruitk.HostNodePool` (exists) | `bHostNodePool` | true, Slate leaf pool, cap 256 const | unchanged | **no-op** (§8) |
| 5 | hook_validation | `ruitk.HookValidation` (exists) | `bHookValidation` | compiled default dev-true/shipping-false | unchanged | **no-op** (§8, tri-state realization P-09) |
| 6 | strict_diagnostics | `ruitk.StrictDiagnostics` (exists, **DEAD**) | `bStrictDiagnostics` | knob exists, plumbing written every render, **nothing reads it** | wired: the two contract warnings, per-component dedup | M5 |
| 7 | strict_mode | `ruitk.StrictMode` (exists) | `bStrictMode` | false, force-off shipping, double-invoke live | unchanged — **this leg is the family reference** | **no-op** (§8) |
| 8 | trace_level | `ruitk.TraceLevel` (**NEW**) | `TraceLevel` enum (NEW) | does not exist | int 0/1/2 = None/Basic/Verbose, default 0 | M7 |
| 9 | diff_tracing | `ruitk.DiffTracing` (**NEW**) | `bDiffTracing` (NEW) | does not exist | bool false, OR-switch | M7 |
| 10 | environment | `ruitk.Environment` (**NEW**) | `Environment` enum (NEW) | does not exist | auto/development/production, default auto | M6 |

## §4 — Engine-local decisions (P-01..P-12; settled here, do not re-litigate)

- **P-01 — The scheduler is a plain RuitkCore class, `FRuitkScheduler`** (new files
  `RuitkCore/Public/RuitkScheduler.h` + `Private/RuitkScheduler.cpp`). RuitkCore stays
  UObject-free (MASTER_PLAN D-27) and engine-blind: time comes from an injectable
  `TFunction<double()>` clock (defaults to `FPlatformTime::Seconds`) so the mock-host suites
  drive it headlessly with a fake clock — deterministic lane/budget tests, no sleeps.
- **P-02 — Delegate-identity dedup = an explicit key.** C++ `TFunction`s have no identity
  (Unity dedups `HashSet<Action>`), so `Enqueue(const void* Key, TFunction<void()> Fn,
  ERuitkLane Lane)` dedups on `Key` per lane (`TSet<const void*>` trackers). The
  reconciler's Slice action key is the reconciler instance pointer. Cancel-Low removes
  tracker entries too (RenderScheduler.cs:120-128 does both).
- **P-03 — One scheduler per frame pump, pumped by the existing host frame path.** The
  Slate host already owns the once-per-frame seam (`RuitkSlateHost.h:93` PreTick batch
  rule; `EnsureTick` → `Host.RequestFrame` `RuitkReconciler.cpp:160-171`). The scheduler
  instance lives with the host layer; `IRuitkHostConfig` gains the minimal pass-through the
  reconciler needs (enqueue-to-lane + pump-now). The mock host pumps manually. Do NOT
  create a UEngineSubsystem for this — the reconciler and mock suites must run engine-blind.
- **P-04 — Lane usage at parity scope:** render passes enqueue on **Normal**;
  batched-effects flush uses the scheduler's frame-end unbudgeted flush; High/Low/Idle are
  implemented, tested, and exposed on the scheduler API but the reconciler does not yet
  classify updates into them (a later campaign may — the family has no High/Low producer
  yet either; the Unity reconciler enqueues Normal, FiberReconciler.cs:367). Semantics
  (cancel/escalate/idle-gate) must be exact NOW so producers can arrive later without
  re-litigating.
- **P-05 — `ruitk.FrameBudgetMs` ini migration story (someone saved 8.0):** NO silent
  value rewrite — an explicit 8.0 may be intentional and 8.0-as-scheduler-budget is safe
  (more generous, never wrong). Instead: (a) the loud Lane-A changelog entry states the
  semantic change and the new 4.0 default; (b) `URuitkSettings` property tooltip + docs
  updated to the scheduler meaning; (c) a ONE-SHOT editor-only startup notice
  (`UE_LOG` Display in `PushSettingsToCVars`, `WITH_EDITOR` only) when the loaded ini value
  still equals the old 8.0 default, naming the new meaning and default. No config-version
  field, no auto-edit of the user's `DefaultGame.ini`.
- **P-06 — FlushSync force-unsliced mechanism:** a reconciler member
  `bool bForceSyncPass = false`; `FlushSync` sets it (scoped guard), and every slicing
  decision reads `FRuitkConfig::IsTimeSlicing() && !bForceSyncPass`. FlushSync then loops
  passes-to-quiescence: run the pending pass to completion (park impossible), and if the
  commit's deferred replay scheduled a follow-up, run that too — bounded by the M3 depth
  guard, mirroring `PumpNow` (RenderScheduler.cs:214-223) + the sync-mode replay
  (FiberReconciler.cs:901-905). Post-M2 it also drains this reconciler's queued Slice
  action so no parked WIP survives the call.
- **P-07 — Environment surface:** `enum class ERuitkEnvironment : uint8 { Development,
  Production }` + `Ruitk::GetEnvironment()` in `RuitkCoreMisc.h` reading NEW CVar
  `ruitk.Environment` (int: 0=auto, 1=development, 2=production; default 0). Auto resolves
  **development in any non-shipping build (editor included), production in shipping** —
  the "editor-or-debug → development" contract clause in UE terms. Component-facing
  read-only surface = a plain accessor `FRuitkContext::GetEnvironment()` — NOT a hook (no
  hook slot, no hook-shape/HMR impact). The library itself NEVER calls it outside the
  accessor + its test (grep-gated in M6's Done-when).
- **P-08 — Trace transport:** one NEW dedicated category `LogRuitkTrace` (default
  verbosity Log) carries ALL trace output; `ruitk.TraceLevel` gates CONTENT (what is
  emitted), the log category gates TRANSPORT (one switch to silence/redirect, composing
  cleanly with the existing ~25 categories instead of scattering across them).
  Basic = structural events at the existing four commit-phase sites (placement :1261,
  deletion :1277, update :1272, commit :1169) + the replacement decision
  (`ReconcileFiber` :722-725, old fiber present but `Matches` false). Verbose adds
  per-element detail (element type / ComponentId / key on each structural line) and
  per-hook detail (hook writes — the Hooks.cs:1241 precedent). Diff-decision logs
  (bailout taken/skipped, SUBTREE-SKIP, props-equal verdict, child-reconciliation tier
  fast-leaf/keys-stable/full-keyed) emit when `ruitk.DiffTracing OR TraceLevel==Verbose` —
  the contract's independent OR-switch. `FRuitkDiagnostics` counters and `stat Ruitk`
  are UNTOUCHED. Trace calls must compile to a cheap early-out when off (level check
  before any FString::Printf).
- **P-09 — Tri-state "auto" realization (knobs 5/6):** engine-native = a bool CVar whose
  COMPILED default is per-build (dev-true/shipping-false, `RuitkCoreMisc.cpp:27-43`) plus
  the CVar priority ladder for explicit on/off (ini / settings page / console). auto = don't
  set it; on/off = set it. No enum rewrite — record this equivalence in the docs page and
  the family sync notes. (Same pattern already shipped for both knobs; this is the no-op's
  justification, §8.)
- **P-10 — Strict-diagnostics message prefix:** `[Ruitk][strict]` (this leg's brand token +
  the family-fixed `[strict]` marker; siblings use their own brand token + the same
  marker). Dedup per component via the SAME `DiagWarned` set `WarnOnce` uses
  (`RuitkContext.cpp:61-70`) — the setState-during-render site runs outside `FRuitkContext`,
  so hoist the dedup core to a small static (`FRuitkComponentState`-keyed) helper both
  call; one dedup mechanism, two entry points.
- **P-11 — What replaces the restart machinery, exactly:** `bRestart`, `RestartCount`,
  `MaxRestarts`, the Tick abort block (:222-244) and the mid-render `bRestart = true`
  (:148-151) are DELETED. In their place: (a) mid-flight/parked updates append to
  `DeferredUpdates` (the queue that exists for commit-time :143-147/:174) and return;
  (b) `CommitRoot`'s replay tail (:1224-1232) becomes the Unity shape — re-mark flags
  without scheduling per item under a `bReplayingDeferred` guard, then schedule ONCE
  (FiberReconciler.cs:884-909); (c) `ScheduleUpdateOnFiber` gains the walk-to-root
  membership check with superseded-tree redirect (re-mark pending flags on the live
  counterparts) and detached-fiber warn-and-bail (FiberReconciler.cs:254-289); (d) the
  runaway guard moves INTO `RenderComponent`: a depth counter around the component
  invoke, `MaxRenderDepth = 25`, exceeded ⇒ `UE_LOG` Error naming the component + render
  null children (FiberFunctionComponent.cs:16-18/:140-155). `BeginRender`'s
  abandoned-pass reclamation (:283-299) STAYS — it is how an interrupted mount pass and
  Unmount-mid-park stay leak-free.
- **P-12 — Versioning:** next MINOR at release time via `node scripts/bump.mjs` (0.15.0 is
  shipped; the unified-settings campaign takes the next free minor first — do not hardcode
  a number in edits, read the `.uplugin` at execution). The default flip gets a **loud
  Lane-A entry at the TOP of the release section** ("time slicing is now ON by default…"),
  Changed + a migration note, mirrored byte-identically.

---

## §5 — Milestones (mechanical order; each ends with the FULL battery green)

Engine command prefix used below (the MASTER_PLAN §4 canonical form; engines live under the
standard `Program Files\Epic Games` root; always redirect output to a file and parse
`report\index.json`, never exit codes):
`<Engine>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe <abs>\RuitkUnrealDemo.uproject`

**Per-milestone gate (identical for M1-M9, referenced as "FULL BATTERY" below):**
```bat
<Engine>\Engine\Build\BatchFiles\Build.bat RuitkUnrealDemoEditor Win64 Development -Project=<abs>\RuitkUnrealDemo.uproject -WaitMutex
<Engine>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe <abs>\RuitkUnrealDemo.uproject -run=RuitkCompile -check
<Engine>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe <abs>\RuitkUnrealDemo.uproject -ExecCmds="Automation RunTests Ruitk; Quit" -unattended -nopause -nosplash -nullrhi -log -stdout -FullStdOutLogOutput -ReportExportPath=<scratch>\report
:: pass/fail: parse <scratch>\report\index.json — 133+ tests, zero failures
```

### M0 — Baseline, anchors, bench snapshot

- Branch per the banner rule (unified-settings merged? else STOP AND ASK).
- Re-verify EVERY §2 anchor (the uncommitted-files rows especially); fix drifted line
  numbers in your working copy of the task list, not by guessing offsets.
- Run FULL BATTERY on the untouched branch — record the exact test count (the "133+"
  baseline this campaign must never drop below).
- Bench snapshot: run the `Ruitk.Bench` suites at CURRENT defaults; record rows in
  `plans/BENCH_BASELINES.md` per its machine/config rules — this is the "before" of M8's
  no-regression proof.
- Stage nothing in `plans/PENDING_CHANGELOG.md` yet (bullets are staged at each milestone
  completion, per its header discipline — see M9).

### M1 — Task zero: FlushSync genuinely force-unsliced

The defect: `RuitkReconciler.h:49` documents "synchronously and unsliced" but
`FlushSync` (:173-183) just calls `Tick()`, which re-reads `ruitk.TimeSlicing` (:247) —
`bWasSlicing` is captured (:178) and voided (:181). With slicing ON, a FlushSync can park
mid-pass and return with uncommitted WIP. Every mount surface depends on FlushSync
(§2 rows) — under the M8 flip this becomes a first-frame correctness bug, so it lands FIRST.

- Implement P-06 (`bForceSyncPass` + run-to-quiescence loop). Delete the dead
  `bWasSlicing` lines; update the :49 doc comment to describe the mechanism.
- **New regression test (`Ruitk.Core.FlushSyncUnderSlicing`, in
  `Source/RuitkHostTests/Private/RuitkCoreTests2.cpp` or a new file):** set
  `ruitk.TimeSlicing` true + `ruitk.FrameBudgetMs` tiny (e.g. 0.001) so a plain Tick MUST
  park; mount a wide tree; dirty it; call `FlushSync()` once; assert the tree is fully
  committed (no parked WIP, `IsMounted`, expected leaf count, effects flushed) and that the
  CVar still reads true afterwards. This test FAILS on the current code — write it first,
  watch it fail, then fix (dev-process).
- Sweep the FlushSync caller list (§2 rows) — no caller changes expected, but confirm none
  compensates for the old behavior (e.g. loops FlushSync).

**Done-when:** new test green against the fix, red against stash-of-fix; FULL BATTERY green.

### M2 — `FRuitkScheduler`: lanes, budget, dedup, batched effects

- New `RuitkCore/Public/RuitkScheduler.h` + `Private/RuitkScheduler.cpp` per P-01/P-02.
  Port `ruitk-unity/Runtime/Core/RenderScheduler.cs` faithfully:
  - lanes High/Normal/Low/Idle; per-lane queue + key-tracker dedup (:59-85);
  - `PumpFrame()` = the LateUpdate flow (:116-164): Low-cancel when High+Low non-empty at
    frame start (count it), High, Normal-only-if-High-drained (else escalation count), Low,
    Idle gate (nothing ran + queues empty + elapsed < budget/2) with budget/2 sub-budget
    (:149-161, :177), then UNBUDGETED batched-effects flush (:162, :225-243), frame count;
  - `ExecuteQueue` budget check BEFORE dequeue, cumulative from frame start (:166-204);
  - `EnqueueBatchedEffect`, `PumpNow()` unbudgeted full drain (:214-223), `BeginBatch`/
    `EndBatch` deferral of non-High enqueues (:54-58, :88-114), metrics struct (:245-258);
  - budget source: `FRuitkConfig::FrameBudgetMs()` (semantic re-point — the DEFAULT stays
    8.0 until M4; nothing reads it under defaults-off, so behavior is unchanged).
- Wire the pump: host layer owns the instance; `RuitkSlateHost` pumps once per frame in
  its existing PreTick seam; the mock host exposes `PumpSchedulerFrame()` for tests (P-03).
  The reconciler does NOT use it yet (M3) — this milestone is the scheduler alone.
- **New tests (`Ruitk.Scheduler.*`, new file
  `Source/RuitkHostTests/Private/RuitkSchedulerTest.cpp`, fake clock):** lane order;
  High-starves-Normal escalation count; **entire-Low-queue cancel when High+Low non-empty
  at frame start** (and: Low enqueued mid-frame after High drained is NOT cancelled);
  Idle runs only on an otherwise-idle frame under budget/2, and stops at budget/2;
  per-lane dedup by key (double enqueue runs once; re-enqueue after run runs again);
  budget cut-off mid-queue resumes next frame; batched effects flush even on a
  budget-exhausted frame (unbudgeted); PumpNow drains everything; BeginBatch/EndBatch
  defers non-High enqueues; metrics counters exact.

**Done-when:** `Ruitk.Scheduler` suite green with a fake clock (no sleeps, no flakes);
FULL BATTERY green (nothing else changed behavior).

### M3 — Defer-don't-restart + sliced render passes (semantics port, defaults unchanged)

- Implement P-11 exactly (delete restart machinery; defer queue for mid-flight; replay-once
  guard; superseded redirect + detached bail; render-depth-25 guard).
- Rebuild the slicing path on the scheduler: when `IsTimeSlicing()` (and not
  `bForceSyncPass`), a render pass is a **self-re-enqueueing Slice action** on the Normal
  lane keyed to the reconciler (P-02): each Slice runs units until the **NEW
  `ruitk.TimeSliceMs` quantum** (CVar added now, default 2.0, `FRuitkConfig::TimeSliceMs()`
  beside :30-31) — checked AFTER each unit, no preemption (FiberReconciler.cs:444-455) —
  then re-enqueues itself if work remains (:405-424). Two slices can run in one frame
  inside the scheduler budget. When slicing is OFF: **scheduler bypass, synchronous
  single-pass** (the current Tick shape, minus restart machinery) — contract knob 1.
- Mount stays ALWAYS synchronous regardless of the knob (FiberReconciler.cs:125-129;
  `Render()` keeps its direct synchronous pass; assert in a test).
- Passive-effect flush moves onto the scheduler's batched-effects lane where the host
  pump exists; the mock-host path keeps the direct call (flush stays UNBUDGETED either way).
- Update the stale header comment `RuitkReconciler.h:19-20` (restart language) and the
  file-top doc block.
- **New tests:** defer semantics (`Ruitk.Core.Defer*`): setState mid-pass does NOT restart
  (render counts prove single continuation); N updates during one pass coalesce into ONE
  follow-up render; commit-time updates same queue, same coalescing; superseded-tree
  update redirects (no ghost/duplicate children — port the Unity comment's scenario);
  detached-fiber update warns and bails without crash; setState-during-render depth-25
  guard fires the error log and does not hang; park/resume (`Ruitk.Core.SliceParkResume`):
  slicing on, tiny quantum, tree commits over multiple pumped frames, THEN a mid-park
  update defers and lands in the follow-up, and Unmount-mid-park leaks nothing
  (slab `NumLive` back to zero).

**Done-when:** all new tests green **with `ruitk.TimeSlicing` BOTH off and on** (run the
new suites twice via a CVar-flipping fixture); FULL BATTERY green at unchanged defaults;
M1's FlushSync regression test still green (it now also covers "drain the queued Slice").

### M4 — The knob surface: CVar split + `URuitkSettings` + ini story

- `ruitk.TimeSliceMs` (landed M3) gets its `URuitkSettings` row: `TimeSliceMs` UPROPERTY
  (Performance category, ConsoleVariable meta, clamp `0.1..16`, Units=Milliseconds) +
  `PushSettingsToCVars` row (beside cpp:95-96).
- `ruitk.FrameBudgetMs`: default **8.0 → 4.0** in BOTH the CVar (:20) and the ctor
  (cpp:13) — they must stay equal or the diff-guard push misfires (RuitkSettings.h:25-27
  rationale); help text + tooltip rewritten to the scheduler meaning; P-05 one-shot editor
  notice added to `PushSettingsToCVars`. (Behavior at defaults still unchanged —
  `TimeSlicing` is still false, nothing consumes the budget yet.)
- `ruitk.TimeSlicing` help text updated (scheduler bypass wording). Default NOT flipped yet.
- Settings window: verify (don't code) both new rows appear in the nomad tab — IDetailsView
  over the same CDO (§2 settings-window row).
- Extend `Source/RuitkHostTests/Private/RuitkSettingsTest.cpp`: push/diff-guard behavior
  for the new properties, defaults equality (property ctor == CVar default) for ALL rows —
  the equality test is what makes a future default drift a test failure, not a silent bug.

**Done-when:** FULL BATTERY green; settings defaults-equality test locks the new pairs.

### M5 — Strict diagnostics: implement the dead knob

Evidence of deadness (all verified): `State->bIsRendering` written :479/:504 and never
read; `Ruitk::IsRendering()` (RuitkCoreMisc.cpp:135-138) has zero callers;
`ruitk.StrictDiagnostics` (:36-43) gates nothing.

- **Warning 1 — state-update-during-render:** in `ScheduleUpdateOnFiber`'s mid-flight path
  (post-M3, where :148-151 used to be): if the target fiber's state says a render is on
  the stack for THAT component (`State->bIsRendering` — finally consumed) and
  `FRuitkConfig::IsStrictDiagnosticsEnabled()`, emit
  `[Ruitk][strict] %s: state update during render — move it into an effect or event
  handler` via the hoisted dedup helper (P-10), keyed per component. The update itself
  still defers exactly as M3 built (warn, don't change behavior).
- **Warning 2 — missing dependency array:** in `UseEffectImpl`/the layout twin
  (RuitkContext.h:165/:186/:197): when the no-deps "run every render" form is used and
  strict diagnostics is on, `WarnOnce` (keyed on the hook slot)
  `[Ruitk][strict] %s: effect has no dependency array — it re-runs every render; pass
  deps ([] for run-once)`. Emitted at most once per component (WarnOnce semantics), so
  legitimate every-render effects cost one line per session.
- Both warnings also `FRuitkDiagnostics::Emit` (test-capture path :52-56).
- `Ruitk::IsRendering()` gets its first real caller or a comment update — do not leave the
  aspirational API dangling undocumented (whichever way, record it in the code comment).
- **New tests (`Ruitk.Core.StrictDiagnostics*`):** with capture on + knob on:
  setState-in-render component produces EXACTLY ONE warning across many renders (dedup);
  no-deps effect produces exactly one; knob OFF produces zero; strict_mode double-invoke
  does not double the warnings (diagnostics count once — the knob-7 clause); shipping
  default is off (compile-time default row already covered by the M4 equality test).

**Done-when:** FULL BATTERY green; warnings appear in `FRuitkDiagnostics::Messages` under
capture, never in shipping defaults.

### M6 — Environment label

- Implement P-07: CVar `ruitk.Environment` (int 0/1/2, default 0=auto) +
  `Ruitk::GetEnvironment()` + `ERuitkEnvironment` in `RuitkCoreMisc.h`;
  `FRuitkContext::GetEnvironment()` accessor (no hook slot);
  `URuitkSettings::Environment` enum UPROPERTY (Development category,
  ConsoleVariable meta) + push row; settings equality test row.
- Docs snippet (M9 page work): "branch your OWN components on it; the library never does."
- **New tests:** auto resolves development in the test build (non-shipping); explicit
  override 2 → Production visible through `Ctx.GetEnvironment()` in a rendered component;
  changing it does NOT itself re-render (read-only surface, no subscription).
- **Grep gate (Done-when):** `GetEnvironment` has no callers inside
  `Plugins/ReactiveUIToolkit/Source/` outside RuitkCoreMisc/RuitkContext surface +
  tests — the library-never-branches clause, enforced.

**Done-when:** FULL BATTERY green + the grep gate above.

### M7 — Trace ladder + diff tracing

- Implement P-08: `ruitk.TraceLevel` (int 0/1/2) + `ruitk.DiffTracing` (bool) CVars +
  `FRuitkConfig` accessors; `LogRuitkTrace` category; Basic structural lines at the five
  anchored sites (placement :1261, deletion :1277, update :1272, commit :1169, replacement
  decision :722-725); Verbose per-element/per-hook detail; diff-decision lines
  (SUBTREE-SKIP verdicts :396 region, props-equal, reconciliation tier) on
  `DiffTracing OR Verbose`. Early-out before any formatting when everything is off.
- `URuitkSettings`: `TraceLevel` enum + `bDiffTracing` UPROPERTYs (Development category) +
  push rows + equality-test rows. The nomad tab picks both up automatically.
- `stat Ruitk` and the `FRuitkDiagnostics` counters untouched (verify by diff).
- **New tests (`Ruitk.Core.Trace*`):** capture-based — None emits nothing; Basic emits
  exactly the structural set for a scripted mount/update/delete/replace scenario (assert
  counts per kind, not exact strings beyond the stable prefix); Verbose ⊃ Basic;
  DiffTracing alone (level None) emits ONLY diff-decision lines; default-off = silent.

**Done-when:** FULL BATTERY green; a manual editor spot-check of `LogRuitkTrace` output is
listed for the owner's next `field-test-editor` session (not a merge gate).

### M8 — THE FLIP + coupling fixes + no-regression proof

- Flip `ruitk.TimeSlicing` default **false → true** in BOTH the CVar (:16) and the ctor
  (cpp:12) — same-value discipline as M4. Post-flip contract state: slicing on,
  quantum 2.0, scheduler budget 4.0 — the family defaults.
- **Coupling sweep** (the "same campaign" ruling): run the FULL BATTERY and the demo
  gallery headless (`Ruitk.Demos`, `Ruitk.Acceptance`, `Ruitk.Boot`) under the new
  defaults; every failure is a coupling bug to FIX here — expected suspects, pre-audited:
  tests that assume commit-after-one-Tick (must FlushSync or pump), the item-model row
  roots and ComboBox/ListView/TreeView FlushSync sites (§2 — covered by M1, verify),
  HMR `RefreshLiveRoots` (UetkxHmrController.cpp:426), editor preview (UetkxPreview.cpp:91).
  Mount paths are immune by design (mount always sync).
- **Bench comparison at defaults (merge gate):** re-run the M0 bench set under the NEW
  defaults; append rows to `plans/BENCH_BASELINES.md` (same machine/config annotations);
  the PR carries before/after. Gate: no regression beyond noise on the standard
  scenarios — a first frame is identical (sync mount); steady-state update cost must not
  regress; if a bench shows the slicing park adding latency to a scenario the family calls
  interactive, STOP AND ASK (do not quietly widen the budget).
- Loud changelog entry drafted now (staged M9): the flip, the FrameBudgetMs semantic
  change + 8.0-saver note (P-05), the new knobs.

**Done-when:** FULL BATTERY green under new defaults **and** one full run with
`ruitk.TimeSlicing=false` proving the bypass path stays green (both worlds, every suite);
bench rows recorded.

### M9 — Settings/docs/changelog close-out

- **Docs site (`RuitkUnrealDocs~`):** update every page that names the CVar set —
  `src/pages/Debugging/DebuggingPage.tsx`, `src/pages/Concepts/ConceptsPage.tsx`,
  `src/pages/KnownIssues/KnownIssuesPage.tsx` (+ `src/docs.tsx` if a route changes):
  the 10-knob table (family-canonical names + Unreal spellings + defaults), the scheduler
  frame-flow description, defer-don't-restart semantics (replacing any restart language),
  the trace ladder, the environment surface, the FrameBudgetMs migration note.
  `npm run build && npm run lint` + `node scripts/docs-drift.mjs`.
- **PENDING_CHANGELOG discipline (staged AT EXECUTION, per the ledger's header):** at each
  milestone completion M1-M8, stage its bullet in `plans/PENDING_CHANGELOG.md` in the
  ledger's `- [lane A|B|C] [artifact] summary (ref)` format — do NOT write the real lanes
  mid-campaign; `release-process` §0 drains the ledger at release. Minimum set: [A] FlushSync
  fix; [A] scheduler + defer-don't-restart (loud, with the flip); [A] TimeSliceMs/
  FrameBudgetMs split + migration note; [A] strict diagnostics wired; [A] trace
  ladder + diff tracing; [A] environment; [C] Discord family-parity summary. Lane A
  drains to root `CHANGELOG.md` + the **byte-identical**
  `Plugins/ReactiveUIToolkit/CHANGELOG.md` mirror (`scripts/verify-mirror.mjs`).
- **Version:** P-12 (bump.mjs next minor at release; owner-gated Publish per house flow).
- **Ledgers:** `plans/TECH_DEBT.md` — add entries for any accepted caveat (e.g. lanes
  implemented without High/Low producers, P-04); close none silently.
- **Family sync:** this leg's knob table + message prefixes + trace-event set are inputs to
  the sibling legs' parity plans — hand the §3 table upstream to the owner when done (no
  cross-repo edits from this repo).

**Done-when:** every §7 command green; §10 checklist all boxed.

---

## §6 — New-test roster (all land in `Source/RuitkHostTests/Private/`)

| Suite prefix | File | Covers | Milestone |
|---|---|---|---|
| `Ruitk.Core.FlushSyncUnderSlicing` | `RuitkCoreTests2.cpp` (or new) | force-unsliced FlushSync regression | M1 |
| `Ruitk.Scheduler.*` | `RuitkSchedulerTest.cpp` (NEW) | lanes, cancel, escalation, idle budget/2, dedup, batching, PumpNow, metrics | M2 |
| `Ruitk.Core.Defer*` | `RuitkDeferTest.cpp` (NEW) | no-restart, one-coalesced-replay, commit-queue, superseded redirect, detached bail, depth-25 | M3 |
| `Ruitk.Core.SliceParkResume` | `RuitkDeferTest.cpp` | park/resume across frames, update-while-parked, unmount-while-parked leak check | M3 |
| `Ruitk.Settings.*` (extend) | `RuitkSettingsTest.cpp` | push/diff-guard + defaults-equality for every new/changed row | M4/M6/M7 |
| `Ruitk.Core.StrictDiagnostics*` | `RuitkStrictDiagnosticsTest.cpp` (NEW) | both warnings, dedup, off-by-default, strict_mode-counts-once | M5 |
| `Ruitk.Core.Environment*` | `RuitkStrictDiagnosticsTest.cpp` or new | auto/override resolution, read-only surface | M6 |
| `Ruitk.Core.Trace*` | `RuitkTraceTest.cpp` (NEW) | ladder content sets, OR-switch, default-silent | M7 |

House rules: every suite prints per-section markers; capture-based assertions use
`FRuitkDiagnostics` capture, never log scraping; CVar-flipping fixtures RESTORE prior
values in all paths (other suites run in the same process).

## §7 — Full ordered verify block (copy-paste, run last — and after every milestone where noted)

```bat
:: engine-free gates
node scripts/verify-mirror.mjs
node ide-extensions/scripts/changelog.mjs verify
node scripts/check-headers.mjs
node scripts/lint-skills.mjs
node scripts/docs-drift.mjs
node scripts/check-machine-paths.mjs
node scripts/corpus-hash.mjs --check

:: build + markup drift + FULL battery (parse report\index.json, never exit codes)
<Engine>\Engine\Build\BatchFiles\Build.bat RuitkUnrealDemoEditor Win64 Development -Project=<abs>\RuitkUnrealDemo.uproject -WaitMutex
<Engine>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe <abs>\RuitkUnrealDemo.uproject -run=RuitkCompile -check
<Engine>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe <abs>\RuitkUnrealDemo.uproject -ExecCmds="Automation RunTests Ruitk; Quit" -unattended -nopause -nosplash -nullrhi -log -stdout -FullStdOutLogOutput -ReportExportPath=<scratch>\report

:: docs
cd RuitkUnrealDocs~ && npm ci && npm run build && npm run lint
```

M8 additionally runs the battery TWICE (defaults, then `ruitk.TimeSlicing=false` via
`-ExecCmds="ruitk.TimeSlicing 0; Automation RunTests Ruitk; Quit"`).

## §8 — Already conforming — no-ops, with evidence (state, do not "improve")

| Contract item | Evidence | Verdict |
|---|---|---|
| strict_mode (knob 7) | `ruitk.StrictMode` false + shipping force-off (`RuitkCoreMisc.cpp:45-48`, :70-77); double-invoke with first-result-discard `RuitkReconciler.cpp:565-570`; effects not double-invoked (effects registered per final RunOnce state, flushed once in commit); diagnostics count once (`OnRender()` outside RunOnce, :579) | **NO-OP — this leg IS the family reference**; M5 adds only the counts-once test |
| host_node_pool (knob 4) | `ruitk.HostNodePool` true (:23-25); Slate-only leaf pool; `PoolCapPerType = 256` **const** (`RuitkSlateHost.h:123`, enforcement cpp:231) | NO-OP; cap stays a const per ruling |
| hook_validation defaults (knob 5) | dev-true/shipping-false compiled defaults (:27-34); P-09 tri-state realization | NO-OP |
| Error-boundary latch | D-10 cooperative latch (`RuitkCoreMisc.cpp:108-143`, consume+unwind `RuitkReconciler.cpp:572-577`) — structural boundaries + latch, no exceptions | NO-OP (and the exceptionControlFlow ban is moot here — UE ships without exceptions) |
| Settings ship into builds / ini storage | `URuitkSettings` `config=Game, defaultconfig` → the PROJECT's `DefaultGame.ini`, which IS staged into packaged builds; push at `ECVF_SetByProjectSetting`, diff-guarded (RuitkSettings.h:7-27) | NO-OP — the "all settings ship, defaults off/production" ruling is already the shipped design |

## §9 — Guardrails (what NOT to do)

**Never:**
- Commit or push without an explicit owner ask; no `Co-Authored-By`; no branch off master.
- Port `UseExceptionBoundaryFlow` / any exceptionControlFlow analog
  (ruitk-unity/Shared/Core/Diagnostics/DiagnosticsConfig.cs:32 exists — it is NOT part of
  the contract; the removal ruling stands).
- Weaken the StrictMode shipping force-off (:70-77) — the settings page must not create a
  shipping-on path either (it can't: the accessor wins).
- Turn `PoolCapPerType` (256) or the slab `PageSize` (256) into knobs — caps stay consts.
- Make mount sliced/async — mount is ALWAYS synchronous, every leg.
- Resurrect any Unity-UI-Toolkit-style pooled-component scheme (the 2025-11-17 removal);
  the Slate leaf pool is not that and stays as-is.
- Keep, hide, or half-keep the restart machinery "just in case" — P-11 REPLACES it;
  dead flags are how the next FlushSync-class bug is born.
- Rename or re-default any existing `ruitk.*` CVar beyond what §3 specifies.
- Touch `.uetkx` grammar/diagnostics/corpus (`family-corpus.hash` must not move).
- Hand-edit generated `.inl`/`.gen.cpp`/goldens (nothing here should touch them at all —
  a `RuitkCompile -check` diff during this campaign is by definition a bug in the change).
- Let a milestone end with the battery red, a new suite skipped, or a bench row missing
  its machine/config context.
- Write a drive-absolute personal path in ANY tracked file
  (`node scripts/check-machine-paths.mjs` gates it; machine facts go in `.ruitk-local.json`).

**Error-signature table:**

| Signature | Likely cause / fix |
|---|---|
| A suite hangs after M3 | a test relied on synchronous commit-after-Tick with slicing on — pump frames or FlushSync in the harness, don't lower the quantum globally |
| First frame empty in a demo after M8 | a mount surface bypassed FlushSync or a new surface skipped it — check §2 caller rows, not the reconciler |
| Depth-25 error storm in tests | a test intentionally loops setState-during-render expecting the old 25-RESTART abort — port the assertion to the new depth-guard message |
| Warnings duplicated per render | dedup helper keyed on message instead of (component, key) — P-10 |
| Settings page shows stale default | ctor default and CVar default drifted — the M4 equality test names the pair |
| `verify-mirror.mjs` red | edited root CHANGELOG.md without recopying the plugin mirror byte-identically |
| Battery green exit code but failures | you read the exit code — parse `report\index.json` |

## §10 — Close-out checklist (every box is a merge gate)

- [x] M1 FlushSync force-unsliced + regression test — DONE 2026-07-31 (fail-first verified; battery 134/134)
- [x] M2 `FRuitkScheduler` + `Ruitk.Scheduler` suite (fake clock) — DONE 2026-07-31 (12 tests, battery 146/146)
- [ ] M3 defer-don't-restart + sliced Slice actions + depth-25 + park/resume tests, both-worlds green
- [ ] M4 `ruitk.TimeSliceMs` + FrameBudgetMs re-point (4.0) + settings rows + equality tests + P-05 notice
- [ ] M5 strict diagnostics: both warnings, deduped, `[Ruitk][strict]` prefix, tests
- [ ] M6 environment: CVar + `Ctx.GetEnvironment()` + settings row + grep gate
- [ ] M7 trace ladder + `LogRuitkTrace` + diff-tracing OR-switch + tests; `stat Ruitk` untouched
- [ ] M8 default flip + coupling sweep + bench before/after in BENCH_BASELINES.md (no regression)
- [ ] M9 docs pages + PENDING_CHANGELOG bullets staged per milestone + version plan + TECH_DEBT entries
- [ ] §7 full verify block green end-to-end
- [ ] STOP-AND-ASK items resolved with the owner (below), none guessed

**STOP AND ASK the owner (do not guess):**
1. Branch cut point if the unified-settings campaign has not merged at start (banner).
2. Any M8 bench scenario where default slicing measurably hurts an interactive path.
3. Whether the sibling legs adopt `[<brand>][strict]` as the family prefix shape (P-10) —
   coordination only; this leg proceeds with `[Ruitk][strict]` regardless.
4. Any conflict discovered between this plan and the §1 contract text — the contract wins,
   but the conflict gets recorded here first.

## §11 — Reference reading list (read BEFORE M2; re-read the exact ranges at each milestone)

- `ruitk-unity/Runtime/Core/RenderScheduler.cs` — the whole file (261 lines); the
  scheduler you are porting. Key ranges: :20 (budget 4.0), :54-114 (dedup+batch),
  :116-164 (frame flow), :166-204 (budget), :214-243 (PumpNow/effects), :245-258 (metrics).
- `ruitk-unity/Shared/Core/Fiber/FiberReconciler.cs` — :23-31 (guard flags, queue,
  TimeSliceMs), :125-129 (sync mount), :238-325 (schedule paths: redirect/bail/defer),
  :356-424 (dispatch, WorkLoop, Slice), :429-472 (deadline loop), :860-911 (commit tail +
  replay-once).
- `ruitk-unity/Shared/Core/Fiber/FiberFunctionComponent.cs` — :14-18, :130-164 (depth guard
  around the render invoke).
- `ruitk-unity/Shared/Core/Diagnostics/DiagnosticsConfig.cs` — :11-26 (trace ladder +
  diff-tracing switch; :32 is the thing you must NOT port).
- `ruitk-unity/Shared/Core/Hooks.cs` — :1241 (Verbose-gated per-hook detail precedent).
- This repo: `plans/MASTER_PLAN.md` §4 (canonical run), `plans/PENDING_CHANGELOG.md` header
  (staging discipline), `plans/BENCH_BASELINES.md` header (bench row rules),
  `plans/archive/ES_MODULES_EXECUTION_PLAN.md` (the campaign-shape exemplar this plan
  follows), `.claude/skills/dev-process` + `release-process` + `test-run` skills.
