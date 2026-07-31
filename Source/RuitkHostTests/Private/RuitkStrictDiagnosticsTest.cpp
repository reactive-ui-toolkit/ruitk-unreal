// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Ruitk.Core.StrictDiagnostics* — the M5 strict-diagnostics suite (FAMILY_PARITY_PLAN §5/P-10):
// ruitk.StrictDiagnostics gates the two family misuse warnings, prefixed [Ruitk][strict] and
// deduped per component through the DiagWarnOnce core — (1) a state update during the
// component's OWN render (the per-component discriminator: a parent's setter fired from a
// child's render is the legal lift-state-up shape and stays silent), and (2) an effect/layout
// effect registered with no dependency array (Ruitk::EveryCommit(); library-owned effects are
// exempt via InternalUseEffect). Warnings observe — the P-11 defer and the effect registration
// behave identically with the knob on or off. StrictMode's double invoke dedups through the
// same per-component key set (the family diagnostics-count-once clause), and the post-commit
// deferred REPLAY can never re-warn by construction: it re-marks with bScheduleWork=false
// (early return above the warn site) and runs outside render (CommitRoot's tail) — the
// cascade tests prove it observationally (N replays, one message).
//
// Assertions are capture-based (FRuitkDiagnostics::Messages), never log-scraped; the log lines
// the warnings also emit are AddExpectedError'd (occurrences=0: assert-present + suppress).
// Shipping-default-off is a compiled default (RuitkCoreMisc.cpp per-build #if) locked by the
// Ruitk.Umg.Settings defaults-equality rows — a WITH_DEV_AUTOMATION_TESTS build cannot
// execute the Shipping branch, so there is nothing more to run here.

#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "RuitkMockHost.h"
#include "RuitkContext.h"
#include "RuitkCoreElements.h"

#if WITH_DEV_AUTOMATION_TESTS

#define RUITK_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace StrictDiagTest
{
	static int32 RendersSelfSet = 0;
	static int32 RendersNoDeps = 0;
	static TRuitkSetter<int32> SetterParent; // PokeParent's setter (fired from PokeChild's render)
	static TRuitkSetter<int32> SetterNoDeps; // NoDepsComp's setter (external re-render trigger)
	static bool bPokeParentDuringRender = false;

	static void Reset()
	{
		RendersSelfSet = 0;
		RendersNoDeps = 0;
		SetterParent = TRuitkSetter<int32>();
		SetterNoDeps = TRuitkSetter<int32>();
		bPokeParentDuringRender = false;
	}

	/** Pin ruitk.StrictDiagnostics (+ ruitk.StrictMode, default-pinned OFF so an ambient value
	 *  can't skew render counts) and turn on message capture; restores prior values in all
	 *  paths (other suites share the process). Sets at ECVF_SetByProjectSetting, NOT the
	 *  default ECVF_SetByCode: ruitk.StrictMode is the Ruitk.Umg.Settings push-test vehicle
	 *  ("no other test touches it") — a SetByCode pin would raise the rung for the rest of the
	 *  process and the settings push (same SetByProjectSetting priority, accepted) would start
	 *  being rejected as lower-priority. */
	struct FScopedStrictWorld
	{
		IConsoleVariable* Strict;
		IConsoleVariable* Mode;
		bool bPrevStrict;
		bool bPrevMode;
		bool bPrevCapture;
		explicit FScopedStrictWorld(bool bStrictOn, bool bStrictModeOn = false)
			: Strict(IConsoleManager::Get().FindConsoleVariable(TEXT("ruitk.StrictDiagnostics"))),
			  Mode(IConsoleManager::Get().FindConsoleVariable(TEXT("ruitk.StrictMode")))
		{
			bPrevStrict = Strict->GetBool();
			bPrevMode = Mode->GetBool();
			Strict->Set(bStrictOn, ECVF_SetByProjectSetting);
			Mode->Set(bStrictModeOn, ECVF_SetByProjectSetting);
			bPrevCapture = FRuitkDiagnostics::bCapture;
			FRuitkDiagnostics::bCapture = true;
			FRuitkDiagnostics::ClearMessages();
		}
		~FScopedStrictWorld()
		{
			Strict->Set(bPrevStrict, ECVF_SetByProjectSetting);
			Mode->Set(bPrevMode, ECVF_SetByProjectSetting);
			FRuitkDiagnostics::bCapture = bPrevCapture;
			FRuitkDiagnostics::ClearMessages();
		}
	};

	/** Pin ruitk.TimeSlicing/ruitk.TimeSliceMs for a world (warning 1 sits on the shared defer
	 *  branch — prove it in both worlds); restores prior values in all paths. */
	struct FScopedSlicing
	{
		IConsoleVariable* Slicing;
		IConsoleVariable* Quantum;
		bool bPrevSlicing;
		float PrevQuantum;
		explicit FScopedSlicing(bool bSliced, float QuantumMs = 0.0f)
			: Slicing(IConsoleManager::Get().FindConsoleVariable(TEXT("ruitk.TimeSlicing"))),
			  Quantum(IConsoleManager::Get().FindConsoleVariable(TEXT("ruitk.TimeSliceMs")))
		{
			bPrevSlicing = Slicing->GetBool();
			PrevQuantum = Quantum->GetFloat();
			Slicing->Set(bSliced);
			Quantum->Set(QuantumMs);
		}
		~FScopedSlicing()
		{
			Slicing->Set(bPrevSlicing);
			Quantum->Set(PrevQuantum);
		}
	};

	static int32 CountContaining(const TCHAR* Needle)
	{
		int32 N = 0;
		for (const FString& M : FRuitkDiagnostics::Messages)
		{
			if (M.Contains(Needle))
			{
				++N;
			}
		}
		return N;
	}
} // namespace StrictDiagTest

// ── Warning-1 components ──────────────────────────────────────────────────────────────────

/** Sets its own state during render on EVERY render until V reaches 5 — five warn attempts
 *  across the coalesced replay ladder, which the dedup must collapse to ONE message. */
static FRuitkNodeArray SelfSetComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	++StrictDiagTest::RendersSelfSet;
	auto [V, SetV] = Ctx.UseState<int32>(0);
	if (V < 5)
	{
		SetV(V + 1); // state update DURING this component's own render — warns (once), defers (P-11)
	}
	return {Ruitk::TextBlock(FString::Printf(TEXT("v%d"), V))};
}
RUITK_COMPONENT(SelfSetComp)

static FRuitkNodeArray PokeChild(FRuitkContext&, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	if (StrictDiagTest::bPokeParentDuringRender)
	{
		StrictDiagTest::bPokeParentDuringRender = false;
		StrictDiagTest::SetterParent(1); // the PARENT's state, set during the CHILD's render
	}
	return {Ruitk::TextBlock(TEXT("child"))};
}
RUITK_COMPONENT(PokeChild)

static FRuitkNodeArray PokeParent(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [N, SetN] = Ctx.UseState<int32>(0);
	StrictDiagTest::SetterParent = SetN;
	return {Ruitk::TextBlock(FString::Printf(TEXT("p%d"), N)), Ruitk::FC(&PokeChild)};
}
RUITK_COMPONENT(PokeParent)

// ── Warning-2 component ───────────────────────────────────────────────────────────────────

/** Two no-deps effect slots + a deps'd one + the layout twin + the library-internal path:
 *  exactly three warnings (keyed per slot), the deps'd and internal registrations silent. */
static FRuitkNodeArray NoDepsComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	++StrictDiagTest::RendersNoDeps;
	auto [V, SetV] = Ctx.UseState<int32>(0);
	StrictDiagTest::SetterNoDeps = SetV;
	Ctx.UseEffect([]() {}, Ruitk::EveryCommit());		// effect slot 0 — warns once
	Ctx.UseEffect([]() {}, Ruitk::EveryCommit());		// effect slot 1 — its OWN key, warns once
	Ctx.UseEffect([]() {}, Ruitk::Deps(V));				// explicit deps — silent
	Ctx.UseLayoutEffect([]() {}, Ruitk::EveryCommit()); // layout slot 0 — warns once
	// The library-owned path (signal subscriptions, CommonUI focus, …) is exempt by design:
	// the message names the USER's component, which cannot fix plumbing it did not write.
	Ctx.InternalUseEffect([]() -> FRuitkEffectCleanup { return FRuitkEffectCleanup(); }, Ruitk::EveryCommit());
	return {Ruitk::TextBlock(FString::Printf(TEXT("nd%d"), V))};
}
RUITK_COMPONENT(NoDepsComp)

// ── Warning 1: once per component, both worlds; cross-component pokes stay silent ─────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkStrictSetStateInRenderTest, "Ruitk.Core.StrictDiagnosticsSetStateInRender",
								 RUITK_TEST_FLAGS)
bool FRuitkStrictSetStateInRenderTest::RunTest(const FString&)
{
	AddExpectedError(TEXT("state update during render"), EAutomationExpectedErrorFlags::Contains, 0);
	for (const bool bSliced : {false, true})
	{
		AddInfo(FString::Printf(TEXT("[strict/setstate] world: %s"), bSliced ? TEXT("sliced") : TEXT("sync")));
		StrictDiagTest::Reset();
		StrictDiagTest::FScopedStrictWorld StrictWorld(/*bStrictOn=*/true);
		StrictDiagTest::FScopedSlicing World(bSliced);
		FRuitkTestHarness H;
		H.Mount(Ruitk::FC(&SelfSetComp));
		TestTrue(TEXT("cascade settles"), H.Host.PumpUntilIdle(128));
		TestEqual(TEXT("behavior unchanged by the warning (cascade landed)"), H.TextAt(0), FString(TEXT("v5")));
		TestEqual(TEXT("6 renders (mount + 5 coalesced follow-ups)"), StrictDiagTest::RendersSelfSet, 6);
		// Five warn attempts (one per render that set state) — ONE message: dedup across
		// renders, and the five deferred REPLAYS never re-warned (bScheduleWork=false path).
		TestEqual(TEXT("EXACTLY ONE warning across the cascade"),
				  StrictDiagTest::CountContaining(TEXT("state update during render")), 1);
		TestEqual(TEXT("prefixed and component-named"),
				  StrictDiagTest::CountContaining(TEXT("[Ruitk][strict] SelfSetComp")), 1);
	}

	AddInfo(TEXT("[strict/setstate] cross-component poke: the per-component discriminator stays silent"));
	{
		StrictDiagTest::Reset();
		StrictDiagTest::FScopedStrictWorld StrictWorld(/*bStrictOn=*/true);
		StrictDiagTest::FScopedSlicing World(/*bSliced=*/false);
		FRuitkTestHarness H;
		StrictDiagTest::bPokeParentDuringRender = true;
		H.Mount(Ruitk::FC(&PokeParent));
		TestTrue(TEXT("settles"), H.Host.PumpUntilIdle(64));
		TestEqual(TEXT("the parent's update landed"), H.TextAt(0), FString(TEXT("p1")));
		TestEqual(TEXT("no warning: the target's own render was not on the stack"),
				  StrictDiagTest::CountContaining(TEXT("state update during render")), 0);
	}
	StrictDiagTest::Reset();
	return true;
}

// ── Warning 2: keyed per slot, deduped across renders; deps'd + internal paths silent ─────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkStrictNoDepsTest, "Ruitk.Core.StrictDiagnosticsNoDeps", RUITK_TEST_FLAGS)
bool FRuitkStrictNoDepsTest::RunTest(const FString&)
{
	AddExpectedError(TEXT("has no dependency array"), EAutomationExpectedErrorFlags::Contains, 0);
	StrictDiagTest::Reset();
	StrictDiagTest::FScopedStrictWorld StrictWorld(/*bStrictOn=*/true);
	FRuitkTestHarness H;
	H.Mount(Ruitk::FC(&NoDepsComp));
	TestTrue(TEXT("mounts"), H.Host.PumpUntilIdle(64));

	AddInfo(TEXT("[strict/no-deps] one warning per no-deps slot"));
	TestEqual(TEXT("two plain-effect slots warned (per-slot keys)"),
			  StrictDiagTest::CountContaining(TEXT(": effect has no dependency array")), 2);
	TestEqual(TEXT("the layout twin warned"),
			  StrictDiagTest::CountContaining(TEXT(": layout effect has no dependency array")), 1);
	TestEqual(TEXT("deps'd + library-internal registrations stay silent (3 total)"),
			  StrictDiagTest::CountContaining(TEXT("[Ruitk][strict]")), 3);

	AddInfo(TEXT("[strict/no-deps] dedup across renders"));
	StrictDiagTest::SetterNoDeps(1);
	TestTrue(TEXT("re-render 1 settles"), H.Host.PumpUntilIdle(64));
	StrictDiagTest::SetterNoDeps(2);
	TestTrue(TEXT("re-render 2 settles"), H.Host.PumpUntilIdle(64));
	TestEqual(TEXT("3 renders total"), StrictDiagTest::RendersNoDeps, 3);
	TestEqual(TEXT("still exactly 3 warnings"), StrictDiagTest::CountContaining(TEXT("[Ruitk][strict]")), 3);
	StrictDiagTest::Reset();
	return true;
}

// ── Knob off ⇒ silent, behavior identical ────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkStrictOffTest, "Ruitk.Core.StrictDiagnosticsOff", RUITK_TEST_FLAGS)
bool FRuitkStrictOffTest::RunTest(const FString&)
{
	StrictDiagTest::Reset();
	StrictDiagTest::FScopedStrictWorld StrictWorld(/*bStrictOn=*/false);
	FRuitkTestHarness H;
	H.Mount(Ruitk::Fragment({Ruitk::FC(&SelfSetComp), Ruitk::FC(&NoDepsComp)}));
	TestTrue(TEXT("settles"), H.Host.PumpUntilIdle(128));
	TestEqual(TEXT("knob off: zero strict messages"), StrictDiagTest::CountContaining(TEXT("[Ruitk][strict]")), 0);
	TestEqual(TEXT("behavior identical: the cascade still landed"), H.TextAt(0), FString(TEXT("v5")));
	StrictDiagTest::Reset();
	return true;
}

// ── StrictMode double-invoke: diagnostics count once (family knob-7 clause) ───────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkStrictModeCountsOnceTest, "Ruitk.Core.StrictDiagnosticsStrictModeOnce",
								 RUITK_TEST_FLAGS)
bool FRuitkStrictModeCountsOnceTest::RunTest(const FString&)
{
	AddExpectedError(TEXT("state update during render"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedError(TEXT("has no dependency array"), EAutomationExpectedErrorFlags::Contains, 0);
	StrictDiagTest::Reset();
	StrictDiagTest::FScopedStrictWorld StrictWorld(/*bStrictOn=*/true, /*bStrictModeOn=*/true);
	FRuitkTestHarness H;
	H.Mount(Ruitk::Fragment({Ruitk::FC(&SelfSetComp), Ruitk::FC(&NoDepsComp)}));
	TestTrue(TEXT("settles under double-invoke"), H.Host.PumpUntilIdle(128));
	TestEqual(TEXT("warning 1 not doubled by the second invoke"),
			  StrictDiagTest::CountContaining(TEXT("state update during render")), 1);
	TestEqual(TEXT("warning 2 not doubled by the second invoke"),
			  StrictDiagTest::CountContaining(TEXT("has no dependency array")), 3);
	TestEqual(TEXT("4 strict messages total"), StrictDiagTest::CountContaining(TEXT("[Ruitk][strict]")), 4);
	StrictDiagTest::Reset();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
