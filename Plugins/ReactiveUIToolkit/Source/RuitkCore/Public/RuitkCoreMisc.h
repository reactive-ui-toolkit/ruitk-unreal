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

struct RUITKCORE_API FRuitkConfig
{
	/** Chunk the render phase across frames on a budget (commit stays atomic). Off by
	 *  default — synchronous renders are simplest and fast for normal UIs. */
	static bool IsTimeSlicing();
	static float FrameBudgetMs();
	/** Render-phase quantum (family knob time_slice_ms, default 2.0): a sliced pass runs
	 *  units of work until the quantum elapses — checked AFTER each unit, no preemption
	 *  (FiberReconciler.cs:31, :444-455) — then parks. Only read when IsTimeSlicing(). */
	static float TimeSliceMs();

	/** Host-node pooling opt-out (A/B measurement). */
	static bool IsHostNodePoolEnabled();

	/** Hook-order validation + set-during-render warnings (dev builds default-on). */
	static bool IsHookValidationEnabled();
	static bool IsStrictDiagnosticsEnabled();

	/** StrictMode double-render (dev-only; flushes impure renders + stale captures, D-14). */
	static bool IsStrictModeEnabled();
};

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

	/** True while a component render is on the stack (debug in-render assert support). */
	RUITKCORE_API bool IsRendering();
	RUITKCORE_API void SetRendering(bool bInRendering);
} // namespace Ruitk

#define RUITK_RENDER_FAIL(Fmt, ...)                                                                                    \
	Ruitk::FailRender(FString::Printf(TEXT("%s(%d): ") Fmt, TEXT(__FILE__), __LINE__, ##__VA_ARGS__))
