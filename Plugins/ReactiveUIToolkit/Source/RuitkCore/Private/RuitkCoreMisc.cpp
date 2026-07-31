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
	CVarRuitkTimeSlicing(TEXT("ruitk.TimeSlicing"), true, // family default ON since the M8 flip
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

static TAutoConsoleVariable<int32>
	CVarRuitkTraceLevel(TEXT("ruitk.TraceLevel"), 0,
						TEXT("Trace-content level on the LogRuitkTrace category: 0 = off, 1 = Basic "
							 "(structural events: placements, deletions, node replacements, commit "
							 "summaries), 2 = Verbose (adds per-element update lines, per-element and "
							 "per-hook detail, and implies ruitk.DiffTracing)."));

static TAutoConsoleVariable<bool>
	CVarRuitkDiffTracing(TEXT("ruitk.DiffTracing"), false,
						 TEXT("Reconciler diff-decision logs ([Ruitk][diff] ...): bailout/subtree-skip "
							  "verdicts and child-reconciliation tiers (fast-leaf/keys-stable/full-keyed/"
							  "positional). Independent of ruitk.TraceLevel — diff lines emit when this "
							  "is true OR the level is Verbose."));

static TAutoConsoleVariable<int32>
	CVarRuitkEnvironment(TEXT("ruitk.Environment"), 0,
						 TEXT("The environment label surfaced READ-ONLY to components via "
							  "Ctx.GetEnvironment(): 0 = auto (development in any non-shipping build, "
							  "production in Shipping), 1 = development, 2 = production. For YOUR "
							  "components' dev-vs-prod branches - the library never branches on it."));

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
ERuitkTraceLevel FRuitkConfig::TraceLevel()
{
	switch (CVarRuitkTraceLevel.GetValueOnGameThread())
	{
	case 1:
		return ERuitkTraceLevel::Basic;
	case 2:
		return ERuitkTraceLevel::Verbose;
	default:
		// 0 and any out-of-range value: off (the ruitk.Environment precedent — never garbage).
		return ERuitkTraceLevel::None;
	}
}
bool FRuitkConfig::IsDiffTracingEnabled()
{
	return CVarRuitkDiffTracing.GetValueOnGameThread();
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

// ── Trace transport (M7/P-08) ────────────────────────────────────────────────────────────
// ONE dedicated category carries ALL trace output. Content is gated at the call sites
// (Ruitk::TraceStructural/TraceDetail/TraceDiff before any formatting); this category gates
// TRANSPORT — a single `log LogRuitkTrace off` silences/redirects the tracer without touching
// the plugin's other categories.

DEFINE_LOG_CATEGORY_STATIC(LogRuitkTrace, Log, All);

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
	void TraceEmit(const FString& Line)
	{
		FRuitkDiagnostics::Emit(Line); // capture path — headless suites assert here, never log-scrape
		UE_LOG(LogRuitkTrace, Log, TEXT("%s"), *Line);
	}

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

	ERuitkEnvironment GetEnvironment()
	{
		switch (CVarRuitkEnvironment.GetValueOnGameThread())
		{
		case 1:
			return ERuitkEnvironment::Development;
		case 2:
			return ERuitkEnvironment::Production;
		default:
			// auto (and any out-of-range value): the contract's "editor-or-debug → development"
			// in UE terms — development in any non-shipping build, production in Shipping.
#if UE_BUILD_SHIPPING
			return ERuitkEnvironment::Production;
#else
			return ERuitkEnvironment::Development;
#endif
		}
	}
} // namespace Ruitk
