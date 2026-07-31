// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuitkCoreMisc.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"

DEFINE_STAT(STAT_RuitkRenders);
DEFINE_STAT(STAT_RuitkCommits);
DEFINE_STAT(STAT_RuitkPlacements);
DEFINE_STAT(STAT_RuitkUpdates);
DEFINE_STAT(STAT_RuitkDeletions);

// ── CVars (ruitk.*, dotted PascalCase — D-14) ──────────────────────────────────────────────

static TAutoConsoleVariable<bool>
	CVarRuitkTimeSlicing(TEXT("ruitk.TimeSlicing"), false,
						 TEXT("Run Reactive UI Toolkit render passes as time-sliced actions on the frame scheduler "
							  "(commit stays atomic). Off = scheduler bypass: fully synchronous single-pass renders."));

static TAutoConsoleVariable<float>
	CVarRuitkFrameBudgetMs(TEXT("ruitk.FrameBudgetMs"), 4.0f,
						   TEXT("The scheduler's per-frame budget in ms, cumulative across lanes "
								"(render slices, idle work; the frame-end batched-effects flush is "
								"unbudgeted). Per-slice length is ruitk.TimeSliceMs. NOTE: before the "
								"scheduler (family-parity M2-M4) this was the single render-phase "
								"budget with default 8.0."));

static TAutoConsoleVariable<float>
	CVarRuitkTimeSliceMs(TEXT("ruitk.TimeSliceMs"), 2.0f,
						 TEXT("Render-phase quantum in ms when ruitk.TimeSlicing is on: a pass runs units of "
							  "work until the quantum elapses (checked after each unit), then parks — resuming "
							  "the same frame if the scheduler budget allows, else next frame."));

static TAutoConsoleVariable<bool>
	CVarRuitkHostNodePool(TEXT("ruitk.HostNodePool"), true,
						  TEXT("Recycle childless leaf widgets across keyed-list churn (GO-05). Off to A/B."));

static TAutoConsoleVariable<bool>
	CVarRuitkHookValidation(TEXT("ruitk.HookValidation"),
#if UE_BUILD_SHIPPING
							false,
#else
							true,
#endif
							TEXT("Hook-order mismatch detection (hooks in branches/loops desync slots)."));

static TAutoConsoleVariable<bool>
	CVarRuitkStrictDiagnostics(TEXT("ruitk.StrictDiagnostics"),
#if UE_BUILD_SHIPPING
							   false,
#else
							   true,
#endif
							   TEXT("Misuse warnings, prefixed [Ruitk][strict] and deduped per component: state "
									"updates during a component's own render + effects registered with no "
									"dependency array (Ruitk::EveryCommit())."));

static TAutoConsoleVariable<bool>
	CVarRuitkStrictMode(TEXT("ruitk.StrictMode"), false,
						TEXT("Dev double-render: render functions run twice, first result "
							 "discarded (flushes impure renders and stale captures)."));

bool FRuitkConfig::IsTimeSlicing()
{
	return CVarRuitkTimeSlicing.GetValueOnGameThread();
}
float FRuitkConfig::FrameBudgetMs()
{
	return CVarRuitkFrameBudgetMs.GetValueOnGameThread();
}
float FRuitkConfig::TimeSliceMs()
{
	return CVarRuitkTimeSliceMs.GetValueOnGameThread();
}
bool FRuitkConfig::IsHostNodePoolEnabled()
{
	return CVarRuitkHostNodePool.GetValueOnGameThread();
}
bool FRuitkConfig::IsHookValidationEnabled()
{
	return CVarRuitkHookValidation.GetValueOnGameThread();
}
bool FRuitkConfig::IsStrictDiagnosticsEnabled()
{
	return CVarRuitkStrictDiagnostics.GetValueOnGameThread();
}
bool FRuitkConfig::IsStrictModeEnabled()
{
#if UE_BUILD_SHIPPING
	return false; // never in shipping, regardless of the CVar
#else
	return CVarRuitkStrictMode.GetValueOnGameThread();
#endif
}

// ── Diagnostics ──────────────────────────────────────────────────────────────────────────

bool FRuitkDiagnostics::bEnabled = false;
bool FRuitkDiagnostics::bCapture = false;
TArray<FString> FRuitkDiagnostics::Messages;
int32 FRuitkDiagnostics::Renders = 0;
int32 FRuitkDiagnostics::Commits = 0;
int32 FRuitkDiagnostics::Placements = 0;
int32 FRuitkDiagnostics::Updates = 0;
int32 FRuitkDiagnostics::Deletions = 0;

void FRuitkDiagnostics::Emit(const FString& Msg)
{
	if (bCapture)
	{
		Messages.Add(Msg);
	}
}

void FRuitkDiagnostics::ClearMessages()
{
	Messages.Empty();
}

void FRuitkDiagnostics::Reset()
{
	Renders = Commits = Placements = Updates = Deletions = 0;
}

// ── Render-error latch (D-10) ────────────────────────────────────────────────────────────

namespace
{
	// Game-thread only (checkf'd at the public entry points) — plain statics suffice and
	// keep the latch visible in a debugger.
	TOptional<FString> GRuitkRenderFailure;
	bool bGRuitkIsRendering = false;
} // namespace

namespace Ruitk
{
	void FailRender(const FString& Reason)
	{
		if (!GRuitkRenderFailure.IsSet()) // first failure wins (nested failures are fallout)
		{
			GRuitkRenderFailure = Reason;
		}
	}

	TOptional<FString> ConsumeRenderFailure()
	{
		TOptional<FString> Out = MoveTemp(GRuitkRenderFailure);
		GRuitkRenderFailure.Reset();
		return Out;
	}

	bool IsRendering()
	{
		return bGRuitkIsRendering;
	}
	void SetRendering(bool bInRendering)
	{
		bGRuitkIsRendering = bInRendering;
	}
} // namespace Ruitk
