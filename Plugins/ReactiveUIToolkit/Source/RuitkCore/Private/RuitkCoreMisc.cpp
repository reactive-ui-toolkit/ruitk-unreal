// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuitkCoreMisc.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"

DEFINE_STAT(STAT_RuiRenders);
DEFINE_STAT(STAT_RuiCommits);
DEFINE_STAT(STAT_RuiPlacements);
DEFINE_STAT(STAT_RuiUpdates);
DEFINE_STAT(STAT_RuiDeletions);

// ── CVars (ruitk.*, dotted PascalCase — D-14) ──────────────────────────────────────────────

static TAutoConsoleVariable<bool>
	CVarRuitkTimeSlicing(TEXT("ruitk.TimeSlicing"), false,
					   TEXT("Chunk the Reactive UI Toolkit render phase across frames on a budget (commit stays atomic)."));

static TAutoConsoleVariable<float>
	CVarRuitkFrameBudgetMs(TEXT("ruitk.FrameBudgetMs"), 8.0f,
						 TEXT("Render-phase work per frame before parking, when ruitk.TimeSlicing is on."));

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
							 TEXT("Warn on state updates during render and similar misuse."));

static TAutoConsoleVariable<bool> CVarRuitkStrictMode(TEXT("ruitk.StrictMode"), false,
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
