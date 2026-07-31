// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Ruitk.Core.Trace* — the M7 trace-ladder suite (FAMILY_PARITY_PLAN §5/P-08, family knobs
// 8 + 9): `ruitk.TraceLevel` gates CONTENT (Basic = structural events at the commit-phase
// sites — placements, deletions, node replacements, commit summaries; per-element Update
// lines are Verbose-only, the family Basic set — conformance C2; Verbose also adds
// per-element detail on each structural line + per-hook detail) and `ruitk.DiffTracing` is
// the INDEPENDENT OR-switch for reconciler diff-decision logs (bailout/subtree-skip verdicts,
// child-reconciliation tiers) — diff lines emit when DiffTracing OR Verbose. The gate matrix
// is the family truth table (Unity TraceGateTests, its M5) in executable form, and the
// behavioral pins run ONE scripted mount→update→delete→replace scenario and assert exact
// per-kind counts, in BOTH worlds (`ruitk.TimeSlicing` off and on — the structural counts and
// the render-phase lines must not depend on park/resume).
//
// Assertions are capture-based (FRuitkDiagnostics::Messages), never log-scraped; trace lines
// log at Log verbosity on the dedicated LogRuitkTrace category, so no AddExpectedError is
// needed. CVar pins land at ECVF_SetByProjectSetting (the M5 rung lesson — a SetByCode pin
// would out-prioritize the URuitkSettings push for the rest of the process) and restore prior
// VALUES in all paths; the Ruitk.Umg.Settings parity rows rely on that.

#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "RuitkMockHost.h"
#include "RuitkContext.h"
#include "RuitkCoreElements.h"

#if WITH_DEV_AUTOMATION_TESTS

#define RUITK_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace TraceTest
{
	static int32 Renders = 0;
	static TRuitkSetter<int32> SetPhase;

	static void Reset()
	{
		Renders = 0;
		SetPhase = TRuitkSetter<int32>();
	}

	/** Pin ruitk.TraceLevel + ruitk.DiffTracing (ECVF_SetByProjectSetting — the M5 rung
	 *  lesson) and turn on message capture; restores prior values in all paths (other suites
	 *  share the process, and the settings parity row compares VALUES). */
	struct FScopedTraceWorld
	{
		IConsoleVariable* Level;
		IConsoleVariable* Diff;
		int32 PrevLevel;
		bool bPrevDiff;
		bool bPrevCapture;
		FScopedTraceWorld(int32 InLevel, bool bInDiff)
			: Level(IConsoleManager::Get().FindConsoleVariable(TEXT("ruitk.TraceLevel"))),
			  Diff(IConsoleManager::Get().FindConsoleVariable(TEXT("ruitk.DiffTracing")))
		{
			PrevLevel = Level->GetInt();
			bPrevDiff = Diff->GetBool();
			Level->Set(InLevel, ECVF_SetByProjectSetting);
			Diff->Set(bInDiff, ECVF_SetByProjectSetting);
			bPrevCapture = FRuitkDiagnostics::bCapture;
			FRuitkDiagnostics::bCapture = true;
			FRuitkDiagnostics::ClearMessages();
		}
		~FScopedTraceWorld()
		{
			Level->Set(PrevLevel, ECVF_SetByProjectSetting);
			Diff->Set(bPrevDiff, ECVF_SetByProjectSetting);
			FRuitkDiagnostics::bCapture = bPrevCapture;
			FRuitkDiagnostics::ClearMessages();
		}
	};

	/** Pin ruitk.TimeSlicing/ruitk.TimeSliceMs for a world (the M3 fixture shape — quantum 0
	 *  makes every unit park, the maximally-different render-phase world); restores prior
	 *  values in all paths. */
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

	static int32 CountPrefix(const TCHAR* Prefix)
	{
		int32 N = 0;
		for (const FString& M : FRuitkDiagnostics::Messages)
		{
			if (M.StartsWith(Prefix, ESearchCase::CaseSensitive))
			{
				++N;
			}
		}
		return N;
	}

	static int32 CountExact(const TCHAR* Line)
	{
		int32 N = 0;
		for (const FString& M : FRuitkDiagnostics::Messages)
		{
			if (M.Equals(Line, ESearchCase::CaseSensitive))
			{
				++N;
			}
		}
		return N;
	}

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
} // namespace TraceTest

// ── The scripted scenario component ───────────────────────────────────────────────────────

/** Phase-driven tree under a constant-props Box container — one commit per phase:
 *  phase 0 (mount): Box[a b c]          -> 4 placements
 *  phase 1:         Box[a1 b1 c1]       -> 3 updates (fast-leaf tier)
 *  phase 2:         Box[a1]             -> 2 deletions (positional tier)
 *  phase 3:         Box[Box]            -> 1 replacement decision + 1 placement + 1 deletion
 *  Totals: 5 placements, 3 updates, 3 deletions, 1 replace, 4 commits; 4 renders.
 *  The deps'd effect keeps strict diagnostics silent and gives Verbose one hook-capture line
 *  per render; the three setter calls give Verbose three state-write lines. */
static FRuitkNodeArray TraceLadderComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	++TraceTest::Renders;
	auto [Phase, SetP] = Ctx.UseState<int32>(0);
	TraceTest::SetPhase = SetP;
	Ctx.UseEffect([]() {}, Ruitk::Deps(Phase));
	TArray<FRuitkNode> Kids;
	switch (Phase)
	{
	case 0:
		Kids = {Ruitk::TextBlock(TEXT("a")), Ruitk::TextBlock(TEXT("b")), Ruitk::TextBlock(TEXT("c"))};
		break;
	case 1:
		Kids = {Ruitk::TextBlock(TEXT("a1")), Ruitk::TextBlock(TEXT("b1")), Ruitk::TextBlock(TEXT("c1"))};
		break;
	case 2:
		Kids = {Ruitk::TextBlock(TEXT("a1"))};
		break;
	default:
		Kids = {RuitkTest::Box(RuitkTest::BoxProps(TEXT("leaf")))};
		break;
	}
	return {RuitkTest::Box(RuitkTest::BoxProps(TEXT("area")), MoveTemp(Kids))};
}
RUITK_COMPONENT(TraceLadderComp)

/** Runs the four phases to quiescence in the given world; per-kind counts accumulate in the
 *  ambient capture. Returns false on any pump/shape failure (the caller's asserts then read
 *  as fallout, not the root cause). */
static bool RunTraceScenario(FAutomationTestBase& Test, bool bSliced)
{
	TraceTest::Reset();
	TraceTest::FScopedSlicing World(bSliced);
	FRuitkTestHarness H;
	H.Mount(Ruitk::FC(&TraceLadderComp));
	bool bOk = Test.TestTrue(TEXT("mount settles"), H.Host.PumpUntilIdle(256));
	for (int32 Phase = 1; Phase <= 3; ++Phase)
	{
		TraceTest::SetPhase(Phase);
		bOk &= Test.TestTrue(*FString::Printf(TEXT("phase %d settles"), Phase), H.Host.PumpUntilIdle(256));
	}
	bOk &= Test.TestEqual(TEXT("4 renders (mount + 3 phases)"), TraceTest::Renders, 4);
	FMockNode* Area = H.ChildAt(0);
	bOk &= Test.TestNotNull(TEXT("container committed"), Area);
	if (Area != nullptr)
	{
		bOk &= Test.TestEqual(TEXT("final tree: one replaced child"), Area->Children.Num(), 1);
	}
	return bOk;
}

// ── The gate matrix (pure logic — the family truth table, spelled row by row) ─────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkTraceGateMatrixTest, "Ruitk.Core.TraceGateMatrix", RUITK_TEST_FLAGS)
bool FRuitkTraceGateMatrixTest::RunTest(const FString&)
{
	AddInfo(TEXT("[trace/matrix] (level, diff) -> (structural, detail, diffGate) — six rows"));
	struct FRow
	{
		int32 Level;
		bool bDiff;
		bool bStructural;
		bool bDetail;
		bool bDiffGate;
	};
	const FRow Rows[] = {
		{0, false, false, false, false}, {0, true, false, false, true}, {1, false, true, false, false},
		{1, true, true, false, true},	 {2, false, true, true, true},	{2, true, true, true, true},
	};
	for (const FRow& Row : Rows)
	{
		TraceTest::FScopedTraceWorld World(Row.Level, Row.bDiff);
		const FString Tag = FString::Printf(TEXT("@ level=%d diff=%d"), Row.Level, Row.bDiff ? 1 : 0);
		TestEqual(*(TEXT("structural ") + Tag), Ruitk::TraceStructural(), Row.bStructural);
		TestEqual(*(TEXT("detail ") + Tag), Ruitk::TraceDetail(), Row.bDetail);
		TestEqual(*(TEXT("diff ") + Tag), Ruitk::TraceDiff(), Row.bDiffGate);
	}
	return true;
}

// ── Default-off = silent ──────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkTraceDefaultSilentTest, "Ruitk.Core.TraceDefaultSilent", RUITK_TEST_FLAGS)
bool FRuitkTraceDefaultSilentTest::RunTest(const FString&)
{
	AddInfo(TEXT("[trace/silent] compiled defaults gate everything off (every suite restores values)"));
	TestEqual(TEXT("default TraceLevel is None"), static_cast<int32>(FRuitkConfig::TraceLevel()),
			  static_cast<int32>(ERuitkTraceLevel::None));
	TestFalse(TEXT("default DiffTracing is off"), FRuitkConfig::IsDiffTracingEnabled());
	TestFalse(TEXT("structural gate off at defaults"), Ruitk::TraceStructural());
	TestFalse(TEXT("diff gate off at defaults"), Ruitk::TraceDiff());

	AddInfo(TEXT("[trace/silent] (None, off) pinned: the full scenario emits not one trace/diff line"));
	{
		TraceTest::FScopedTraceWorld World(0, false);
		RunTraceScenario(*this, /*bSliced=*/false);
		TestEqual(TEXT("zero structural/hook lines"), TraceTest::CountPrefix(TEXT("[Ruitk][trace]")), 0);
		TestEqual(TEXT("zero diff lines"), TraceTest::CountPrefix(TEXT("[Ruitk][diff]")), 0);
	}
	TraceTest::Reset();
	return true;
}

// ── Basic: exactly the structural set, bare lines, no diff/hook detail — both worlds ──────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkTraceBasicTest, "Ruitk.Core.TraceBasicStructural", RUITK_TEST_FLAGS)
bool FRuitkTraceBasicTest::RunTest(const FString&)
{
	for (const bool bSliced : {false, true})
	{
		AddInfo(FString::Printf(TEXT("[trace/basic] world: %s"), bSliced ? TEXT("sliced") : TEXT("sync")));
		TraceTest::FScopedTraceWorld World(1, false);
		RunTraceScenario(*this, bSliced);
		TestEqual(TEXT("5 placements"), TraceTest::CountPrefix(TEXT("[Ruitk][trace] Placement")), 5);
		// Update lines are Verbose-only (family Basic excludes them — conformance C2); the
		// commit summaries below still carry the update COUNTS at Basic.
		TestEqual(TEXT("no update lines at Basic"), TraceTest::CountPrefix(TEXT("[Ruitk][trace] Update")), 0);
		TestEqual(TEXT("3 deletions"), TraceTest::CountPrefix(TEXT("[Ruitk][trace] Deletion")), 3);
		TestEqual(TEXT("1 replacement decision"), TraceTest::CountPrefix(TEXT("[Ruitk][trace] Replace")), 1);
		TestEqual(TEXT("4 commit summaries"), TraceTest::CountPrefix(TEXT("[Ruitk][trace] Commit #")), 4);
		// Per-element detail is Verbose-only: at Basic every structural line is the bare kind.
		TestEqual(TEXT("placement lines bare"), TraceTest::CountExact(TEXT("[Ruitk][trace] Placement")), 5);
		TestEqual(TEXT("replace line bare"), TraceTest::CountExact(TEXT("[Ruitk][trace] Replace")), 1);
		TestEqual(TEXT("no per-hook lines at Basic"), TraceTest::CountPrefix(TEXT("[Ruitk][trace] Hook")), 0);
		TestEqual(TEXT("no diff lines at Basic"), TraceTest::CountPrefix(TEXT("[Ruitk][diff]")), 0);
	}
	TraceTest::Reset();
	return true;
}

// ── diff_tracing alone: ONLY diff-decision lines (independence) ───────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkTraceDiffAloneTest, "Ruitk.Core.TraceDiffTracingAlone", RUITK_TEST_FLAGS)
bool FRuitkTraceDiffAloneTest::RunTest(const FString&)
{
	AddInfo(TEXT("[trace/diff-alone] level None + diff on: diff lines only"));
	TraceTest::FScopedTraceWorld World(0, true);
	RunTraceScenario(*this, /*bSliced=*/false);
	TestTrue(TEXT("diff lines present"), TraceTest::CountPrefix(TEXT("[Ruitk][diff]")) > 0);
	TestTrue(TEXT("component verdicts present"), TraceTest::CountPrefix(TEXT("[Ruitk][diff] Component")) > 0);
	TestTrue(TEXT("fast-leaf tier hit (phase 1)"),
			 TraceTest::CountContaining(TEXT("[Ruitk][diff] Children fast-leaf")) > 0);
	TestTrue(TEXT("positional tier hit"), TraceTest::CountContaining(TEXT("[Ruitk][diff] Children positional")) > 0);
	TestEqual(TEXT("ZERO structural/hook lines (independence)"), TraceTest::CountPrefix(TEXT("[Ruitk][trace]")), 0);
	TraceTest::Reset();
	return true;
}

// ── Verbose ⊃ Basic: same structural counts + detail + hook lines + the OR-switch ─────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkTraceVerboseTest, "Ruitk.Core.TraceVerboseSuperset", RUITK_TEST_FLAGS)
bool FRuitkTraceVerboseTest::RunTest(const FString&)
{
	for (const bool bSliced : {false, true})
	{
		AddInfo(FString::Printf(TEXT("[trace/verbose] world: %s"), bSliced ? TEXT("sliced") : TEXT("sync")));
		TraceTest::FScopedTraceWorld World(2, false);
		RunTraceScenario(*this, bSliced);
		// Superset: the structural counts are EXACTLY Basic's — detail rides the same lines —
		// plus the Verbose-only per-element Update lines (C2).
		TestEqual(TEXT("5 placements"), TraceTest::CountPrefix(TEXT("[Ruitk][trace] Placement")), 5);
		TestEqual(TEXT("3 updates (Verbose-only)"), TraceTest::CountPrefix(TEXT("[Ruitk][trace] Update")), 3);
		TestEqual(TEXT("3 deletions"), TraceTest::CountPrefix(TEXT("[Ruitk][trace] Deletion")), 3);
		TestEqual(TEXT("1 replacement decision"), TraceTest::CountPrefix(TEXT("[Ruitk][trace] Replace")), 1);
		TestEqual(TEXT("4 commit summaries"), TraceTest::CountPrefix(TEXT("[Ruitk][trace] Commit #")), 4);
		// Per-element detail present (no bare structural lines; the element type is named).
		TestEqual(TEXT("no bare placement lines"), TraceTest::CountExact(TEXT("[Ruitk][trace] Placement")), 0);
		TestTrue(TEXT("placements name the element"),
				 TraceTest::CountContaining(TEXT("[Ruitk][trace] Placement TextBlock")) > 0);
		TestEqual(TEXT("the replace names old -> new"),
				  TraceTest::CountContaining(TEXT("[Ruitk][trace] Replace TextBlock -> Box")), 1);
		// Per-hook detail: one effect capture per render + one line per accepted state write.
		TestEqual(TEXT("4 effect captures"), TraceTest::CountContaining(TEXT("Hook TraceLadderComp: effect captured")),
				  4);
		TestEqual(TEXT("3 state writes"), TraceTest::CountContaining(TEXT("Hook TraceLadderComp: state write")), 3);
		// The OR-switch: Verbose alone lights the diff sites too.
		TestTrue(TEXT("diff lines present at Verbose"), TraceTest::CountPrefix(TEXT("[Ruitk][diff]")) > 0);
	}
	TraceTest::Reset();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
