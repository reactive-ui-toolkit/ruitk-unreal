// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Config CVars, diagnostics counters, and the cooperative render-error latch — the small
// always-on core services (config.gd + diagnostics.gd + the D-10 latch, one header).

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"

// ─────────────────────────────────────────────────────────────────────────────────────────
// STATGROUP_Ruitk (D-14) — `stat Ruitk` shows per-frame reconciler activity.
// ─────────────────────────────────────────────────────────────────────────────────────────

DECLARE_STATS_GROUP(TEXT("Ruitk"), STATGROUP_Ruitk, STATCAT_Advanced);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Renders"), STAT_RuitkRenders, STATGROUP_Ruitk, RUITKCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Commits"), STAT_RuitkCommits, STATGROUP_Ruitk, RUITKCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Placements"), STAT_RuitkPlacements, STATGROUP_Ruitk, RUITKCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Updates"), STAT_RuitkUpdates, STATGROUP_Ruitk, RUITKCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Deletions"), STAT_RuitkDeletions, STATGROUP_Ruitk, RUITKCORE_API);

// ─────────────────────────────────────────────────────────────────────────────────────────
// Config (ruitk.* CVars — dotted PascalCase per D-14; defaults mirror config.gd)
// ─────────────────────────────────────────────────────────────────────────────────────────

/** The trace-content ladder (family knob 8, M7/P-08) — what the tracer emits, resolved from
 *  the `ruitk.TraceLevel` int CVar (0/1/2; out-of-range collapses to None, never garbage). */
enum class ERuitkTraceLevel : uint8
{
	None,
	Basic,
	Verbose
};

struct RUITKCORE_API FRuitkConfig
{
	/** Run render passes as time-sliced scheduler actions (commit stays atomic). Off =
	 *  scheduler bypass, synchronous single-pass — the family time_slicing knob. */
	static bool IsTimeSlicing();
	/** The scheduler's per-frame budget, cumulative across lanes (family frame_budget_ms,
	 *  default 4.0 — was the single-axis render budget, default 8.0, before M2-M4). */
	static float FrameBudgetMs();
	/** Render-phase quantum (family knob time_slice_ms, default 2.0): a sliced pass runs
	 *  units of work until the quantum elapses — checked AFTER each unit, no preemption
	 *  (FiberReconciler.cs:31, :444-455) — then parks. Only read when IsTimeSlicing(). */
	static float TimeSliceMs();

	/** Host-node pooling opt-out (A/B measurement). */
	static bool IsHostNodePoolEnabled();

	/** Hook-order mismatch detection (dev builds default-on). */
	static bool IsHookValidationEnabled();
	/** The two family misuse warnings, prefixed `[Ruitk][strict]` and deduped per component
	 *  (family strict_diagnostics, M5): state update during a component's own render + effect
	 *  registered with no dependency array (dev builds default-on). */
	static bool IsStrictDiagnosticsEnabled();

	/** StrictMode double-render (dev-only; flushes impure renders + stale captures, D-14). */
	static bool IsStrictModeEnabled();

	/** Trace-content level (family trace_level, default None — M7/P-08): Basic = structural
	 *  events at the commit-phase sites (placements, updates, deletions, node replacements,
	 *  commit summaries); Verbose adds per-element detail on each structural line + per-hook
	 *  detail (effect captures, state writes) and implies diff tracing. Content only — the
	 *  transport is the LogRuitkTrace category (see Ruitk::TraceEmit). */
	static ERuitkTraceLevel TraceLevel();
	/** Reconciler diff-decision logs (family diff_tracing, default false): bailout /
	 *  SUBTREE-SKIP verdicts and child-reconciliation tiers. An INDEPENDENT OR-switch —
	 *  diff lines emit when this is on OR TraceLevel is Verbose (Ruitk::TraceDiff). */
	static bool IsDiffTracingEnabled();
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// Environment label (family knob 10, P-07) — a READ-ONLY tag for USER components
// ─────────────────────────────────────────────────────────────────────────────────────────

/** The resolved environment label. Components read it via FRuitkContext::GetEnvironment()
 *  (or Ruitk::GetEnvironment() outside a render) to branch THEIR OWN dev-vs-prod concerns
 *  (debug overlays, verbose panels). The library itself NEVER branches on it — that clause
 *  is grep-gated in the family-parity plan (M6). */
enum class ERuitkEnvironment : uint8
{
	Development,
	Production
};

namespace Ruitk
{
	/** Resolve `ruitk.Environment` (0=auto, 1=development, 2=production; out-of-range = auto).
	 *  Auto is the contract's "editor-or-debug → development" in UE terms: development in any
	 *  non-shipping build (editor included), production in Shipping. Read-only surface — no
	 *  subscription; changing the CVar does not re-render anything by itself. */
	RUITKCORE_API ERuitkEnvironment GetEnvironment();
} // namespace Ruitk

// ─────────────────────────────────────────────────────────────────────────────────────────
// Diagnostics counters (diagnostics.gd) — cheap, opt-in, test-assertable
// ─────────────────────────────────────────────────────────────────────────────────────────

struct RUITKCORE_API FRuitkDiagnostics
{
	static bool bEnabled;

	/** Message capture for headless tests (validators emit here when on). */
	static bool bCapture;
	static TArray<FString> Messages;
	static void Emit(const FString& Msg);
	static void ClearMessages();

	static int32 Renders; // component render-fn invocations (excludes bailouts)
	static int32 Commits;
	static int32 Placements;
	static int32 Updates;
	static int32 Deletions;

	static void Reset();
	static void OnRender()
	{
		INC_DWORD_STAT(STAT_RuitkRenders);
		if (bEnabled)
		{
			++Renders;
		}
	}
	static void OnCommit()
	{
		INC_DWORD_STAT(STAT_RuitkCommits);
		if (bEnabled)
		{
			++Commits;
		}
	}
	static void OnPlacement()
	{
		INC_DWORD_STAT(STAT_RuitkPlacements);
		if (bEnabled)
		{
			++Placements;
		}
	}
	static void OnUpdate()
	{
		INC_DWORD_STAT(STAT_RuitkUpdates);
		if (bEnabled)
		{
			++Updates;
		}
	}
	static void OnDeletion()
	{
		INC_DWORD_STAT(STAT_RuitkDeletions);
		if (bEnabled)
		{
			++Deletions;
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// Trace ladder (family knobs 8 + 9, M7/P-08) — content gates + the single transport.
// Content is gated HERE (what is emitted); transport is the dedicated LogRuitkTrace category
// (defined with Ruitk::TraceEmit — one `log LogRuitkTrace off` silences the tracer without
// touching the plugin's other categories). `stat Ruitk` and the FRuitkDiagnostics counters
// are a separate, untouched surface.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkComponentState;

namespace Ruitk
{
	/** Content gates — the family truth table (Unity TraceGateTests §6): structural ⇐ level
	 *  != None; detail ⇐ level == Verbose; diff ⇐ DiffTracing OR Verbose. ALWAYS check a gate
	 *  before building a message — when everything is off no trace call ever reaches a
	 *  Printf (the P-08 cheap-early-out rule). */
	FORCEINLINE bool TraceStructural()
	{
		return FRuitkConfig::TraceLevel() != ERuitkTraceLevel::None;
	}
	FORCEINLINE bool TraceDetail()
	{
		return FRuitkConfig::TraceLevel() == ERuitkTraceLevel::Verbose;
	}
	FORCEINLINE bool TraceDiff()
	{
		return FRuitkConfig::IsDiffTracingEnabled() || TraceDetail();
	}

	/** THE transport (P-08): every trace line — structural `[Ruitk][trace]` and diff
	 *  `[Ruitk][diff]` alike — goes to the LogRuitkTrace category, plus
	 *  FRuitkDiagnostics::Emit under capture (headless suites assert on capture, never
	 *  log-scrape — the house rule). */
	RUITKCORE_API void TraceEmit(const FString& Line);

	/** Verbose per-hook write line (`[Ruitk][trace] Hook …` — the Hooks.cs:1241 precedent).
	 *  Out-of-line so the header-inline setter/dispatch sites need no complete FRuitkFiber;
	 *  gate with TraceDetail() BEFORE calling. Kind is a literal ("state"/"reducer"). */
	RUITKCORE_API void TraceHookWrite(const FRuitkComponentState& State, int32 Slot, const TCHAR* Kind);
} // namespace Ruitk

// ─────────────────────────────────────────────────────────────────────────────────────────
// Cooperative render-error latch (D-10). UE ships without C++ exceptions, so there is no
// throw path: a failing render CALLS Ruitk::FailRender(...) and returns whatever it can; the
// reconciler checks the latch after each component render and unwinds the WIP subtree to
// the nearest error boundary (safe pre-commit — double buffering keeps `current` intact).
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace Ruitk
{
	/** Signal "this render failed" (usable via the RUITK_RENDER_FAIL macro for file/line). */
	RUITKCORE_API void FailRender(const FString& Reason);

	/** Reconciler-side: consume the latch (returns unset optional when no failure). */
	RUITKCORE_API TOptional<FString> ConsumeRenderFailure();

	/** True while a component render is on the stack. Consumers: the reconciler's render-phase
	 *  defer discriminator (ScheduleUpdateInternal's depth ladder) and — refined per component
	 *  via FRuitkComponentState::bIsRendering — the strict set-state-during-render warning. */
	RUITKCORE_API bool IsRendering();
	RUITKCORE_API void SetRendering(bool bInRendering);
} // namespace Ruitk

#define RUITK_RENDER_FAIL(Fmt, ...)                                                                                    \
	Ruitk::FailRender(FString::Printf(TEXT("%s(%d): ") Fmt, TEXT(__FILE__), __LINE__, ##__VA_ARGS__))
