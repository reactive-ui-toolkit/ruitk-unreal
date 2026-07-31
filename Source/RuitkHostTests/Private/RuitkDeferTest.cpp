// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Ruitk.Core.Defer* + Ruitk.Core.Slice* — the M3 defer-don't-restart suite (FAMILY_PARITY_PLAN
// §5/P-11): setState mid-pass/mid-park/mid-commit DEFERS and replays post-commit as ONE
// coalesced follow-up (never restarts the pass); superseded-tree targets redirect; detached
// fibers warn and bail; runaway render-phase cascades hit the render-depth-25 guard; sliced
// passes park on the ruitk.TimeSliceMs quantum and resume across scheduler pumps; mount stays
// ALWAYS synchronous; Unmount mid-park leaks nothing.
//
// Both-worlds discipline (plan M3 Done-when): every defer semantic runs with ruitk.TimeSlicing
// OFF and ON via FScopedWorld — the mock host pumps its scheduler inside PumpFrame (the Slate
// PreTick seam mirror), so the same pump calls drive both worlds. Park tests advance the mock
// clock from INSIDE component renders — deterministic slicing, no sleeps.

#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "RuitkMockHost.h"
#include "RuitkContext.h"
#include "RuitkCoreElements.h"

#if WITH_DEV_AUTOMATION_TESTS

#define RUITK_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace DeferTest
{
	static int32 RendersA = 0;
	static int32 RendersB = 0;
	static int32 RendersC = 0;
	static TRuitkSetter<int32> SetterB;		  // NoRestartB's state setter (event-path trigger)
	static TRuitkSetter<int32> SetterE;		  // CoalesceE's setter (fired from CoalesceB's render)
	static TRuitkSetter<int32> Setter4;		  // SlowChild #4's local setter (mid-park trigger)
	static TRuitkSetter<bool> SetterShow;	  // ToggleParent's show/hide (detached-bail)
	static bool bFireDuringRender = false;	  // one-shot: B setStates during its own render
	static bool bBurstDuringRender = false;	  // one-shot: CoalesceB fires 3 setStates mid-render
	static FRuitkMockHost* GHost = nullptr;	  // park tests advance GHost->MockTimeSeconds
	static double AdvanceMsPerChild = 0.0;	  // clock cost of one SlowChild render
	static FRuitkFiber* StaleFiber = nullptr; // detached-bail capture

	static void Reset()
	{
		RendersA = RendersB = RendersC = 0;
		SetterB = TRuitkSetter<int32>();
		SetterE = TRuitkSetter<int32>();
		Setter4 = TRuitkSetter<int32>();
		SetterShow = TRuitkSetter<bool>();
		bFireDuringRender = false;
		bBurstDuringRender = false;
		GHost = nullptr;
		AdvanceMsPerChild = 0.0;
		StaleFiber = nullptr;
	}

	/** Pin ruitk.TimeSlicing / ruitk.TimeSliceMs / ruitk.FrameBudgetMs for one world;
	 *  restores prior values in all paths (other suites share the process). */
	struct FScopedWorld
	{
		IConsoleVariable* Slicing;
		IConsoleVariable* Quantum;
		IConsoleVariable* Budget;
		bool bPrevSlicing;
		float PrevQuantum;
		float PrevBudget;
		explicit FScopedWorld(bool bSliced, float QuantumMs = 0.0f, float BudgetMs = -1.0f)
			: Slicing(IConsoleManager::Get().FindConsoleVariable(TEXT("ruitk.TimeSlicing"))),
			  Quantum(IConsoleManager::Get().FindConsoleVariable(TEXT("ruitk.TimeSliceMs"))),
			  Budget(IConsoleManager::Get().FindConsoleVariable(TEXT("ruitk.FrameBudgetMs")))
		{
			bPrevSlicing = Slicing->GetBool();
			PrevQuantum = Quantum->GetFloat();
			PrevBudget = Budget->GetFloat();
			Slicing->Set(bSliced);
			Quantum->Set(QuantumMs);
			if (BudgetMs >= 0.0f)
			{
				Budget->Set(BudgetMs);
			}
		}
		~FScopedWorld()
		{
			Slicing->Set(bPrevSlicing);
			Quantum->Set(PrevQuantum);
			Budget->Set(PrevBudget);
		}
	};
} // namespace DeferTest

// ── No-restart proof: a mid-render setState continues the pass (siblings render ONCE) ─────

static FRuitkNodeArray NoRestartA(FRuitkContext&, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	++DeferTest::RendersA;
	return {Ruitk::TextBlock(TEXT("a"))};
}
RUITK_COMPONENT(NoRestartA)

static FRuitkNodeArray NoRestartB(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	++DeferTest::RendersB;
	auto [V, SetV] = Ctx.UseState<int32>(0);
	DeferTest::SetterB = SetV;
	if (DeferTest::bFireDuringRender && V == 0)
	{
		SetV(1); // setState DURING render: must defer, never restart the pass (P-11)
	}
	return {Ruitk::TextBlock(FString::Printf(TEXT("b%d"), V))};
}
RUITK_COMPONENT(NoRestartB)

static FRuitkNodeArray NoRestartC(FRuitkContext&, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	++DeferTest::RendersC;
	return {Ruitk::TextBlock(TEXT("c"))};
}
RUITK_COMPONENT(NoRestartC)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkDeferNoRestartTest, "Ruitk.Core.DeferNoRestart", RUITK_TEST_FLAGS)
bool FRuitkDeferNoRestartTest::RunTest(const FString&)
{
	// The mid-render setState is exactly what strict diagnostics (M5) warns about — expected
	// (and asserted ≥1) here; the warning's own suite is RuitkStrictDiagnosticsTest.cpp.
	AddExpectedError(TEXT("State update scheduled during render"), EAutomationExpectedErrorFlags::Contains, 0);
	for (const bool bSliced : {false, true})
	{
		AddInfo(FString::Printf(TEXT("[no-restart] world: %s"), bSliced ? TEXT("sliced") : TEXT("sync")));
		DeferTest::Reset();
		DeferTest::bFireDuringRender = true;
		DeferTest::FScopedWorld World(bSliced, /*QuantumMs=*/0.0f);
		FRuitkTestHarness H;
		H.Mount(Ruitk::Fragment({Ruitk::FC(&NoRestartA), Ruitk::FC(&NoRestartB), Ruitk::FC(&NoRestartC)}));
		// Mount is synchronous; the mid-render setState deferred — the FIRST commit carries b0.
		TestEqual(TEXT("mount committed the in-flight pass (no restart)"), H.TextAt(1), FString(TEXT("b0")));
		TestTrue(TEXT("settles"), H.Host.PumpUntilIdle(64));
		// A restart-from-root would have re-rendered the mount siblings; the deferred replay
		// re-renders ONLY B, once.
		TestEqual(TEXT("A rendered once"), DeferTest::RendersA, 1);
		TestEqual(TEXT("C rendered once"), DeferTest::RendersC, 1);
		TestEqual(TEXT("B rendered twice (pass + ONE coalesced follow-up)"), DeferTest::RendersB, 2);
		TestEqual(TEXT("follow-up committed b1"), H.TextAt(1), FString(TEXT("b1")));
	}
	DeferTest::Reset();
	return true;
}

// ── Replay-once coalescing: N setStates during ONE pass -> ONE follow-up commit ───────────

static FRuitkNodeArray CoalesceE(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	++DeferTest::RendersA; // reuse counter A for E
	auto [V, SetV] = Ctx.UseState<int32>(0);
	DeferTest::SetterE = SetV;
	return {Ruitk::TextBlock(FString::Printf(TEXT("e%d"), V))};
}
RUITK_COMPONENT(CoalesceE)

static FRuitkNodeArray CoalesceB(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	++DeferTest::RendersB;
	auto [X, SetX] = Ctx.UseState<int32>(0);
	auto [Y, SetY] = Ctx.UseState<int32>(0);
	DeferTest::SetterB = SetX;
	if (DeferTest::bBurstDuringRender && X <= 0) // the trigger sets X=-1; the follow-up sees X=1
	{
		SetX(1);			   // own state #1
		SetY(2);			   // own state #2
		DeferTest::SetterE(7); // a SIBLING's state — all three coalesce into ONE follow-up
	}
	return {Ruitk::TextBlock(FString::Printf(TEXT("b%d/%d"), X, Y))};
}
RUITK_COMPONENT(CoalesceB)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkDeferCoalesceTest, "Ruitk.Core.DeferCoalesce", RUITK_TEST_FLAGS)
bool FRuitkDeferCoalesceTest::RunTest(const FString&)
{
	// CoalesceB's own-state burst fires mid-render — the M5 strict warning is expected here.
	AddExpectedError(TEXT("State update scheduled during render"), EAutomationExpectedErrorFlags::Contains, 0);
	for (const bool bSliced : {false, true})
	{
		AddInfo(FString::Printf(TEXT("[coalesce] world: %s"), bSliced ? TEXT("sliced") : TEXT("sync")));
		DeferTest::Reset();
		DeferTest::FScopedWorld World(bSliced, /*QuantumMs=*/0.0f);
		FRuitkTestHarness H;
		H.Mount(Ruitk::Fragment({Ruitk::FC(&CoalesceE), Ruitk::FC(&CoalesceB)}));
		TestTrue(TEXT("mount settles"), H.Host.PumpUntilIdle(64));

		FRuitkDiagnostics::bEnabled = true;
		FRuitkDiagnostics::Reset();
		DeferTest::bBurstDuringRender = true;
		DeferTest::SetterB(-1); // trigger a pass; B's render then bursts 3 setStates mid-pass
		TestTrue(TEXT("burst settles"), H.Host.PumpUntilIdle(64));
		// Trigger pass + exactly ONE coalesced follow-up — not one per setState.
		TestEqual(TEXT("3 mid-pass setStates -> 2 commits total"), FRuitkDiagnostics::Commits, 2);
		TestEqual(TEXT("B's states landed"), H.TextAt(1), FString(TEXT("b1/2")));
		TestEqual(TEXT("sibling E's state landed"), H.TextAt(0), FString(TEXT("e7")));
		FRuitkDiagnostics::bEnabled = false;
	}
	DeferTest::Reset();
	return true;
}

// ── Commit-time updates: a layout-effect setState defers and replays once ─────────────────

static FRuitkNodeArray CommitTimeComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	++DeferTest::RendersB;
	auto [V, SetV] = Ctx.UseState<int32>(0);
	const int32 Now = V;
	Ctx.UseLayoutEffect(
		[Now, SetV]()
		{
			if (Now == 0)
			{
				SetV(1); // fires DURING commit (bIsCommitting) — same queue, same coalescing
			}
		},
		Ruitk::Deps(Now));
	return {Ruitk::TextBlock(FString::Printf(TEXT("v%d"), V))};
}
RUITK_COMPONENT(CommitTimeComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkDeferCommitTimeTest, "Ruitk.Core.DeferCommitTime", RUITK_TEST_FLAGS)
bool FRuitkDeferCommitTimeTest::RunTest(const FString&)
{
	for (const bool bSliced : {false, true})
	{
		AddInfo(FString::Printf(TEXT("[commit-time] world: %s"), bSliced ? TEXT("sliced") : TEXT("sync")));
		DeferTest::Reset();
		DeferTest::FScopedWorld World(bSliced, /*QuantumMs=*/0.0f);
		FRuitkTestHarness H;
		H.Mount(Ruitk::FC(&CommitTimeComp));
		TestTrue(TEXT("settles"), H.Host.PumpUntilIdle(64));
		TestEqual(TEXT("layout-effect update landed"), H.TextAt(0), FString(TEXT("v1")));
		TestEqual(TEXT("exactly one follow-up render"), DeferTest::RendersB, 2);
	}
	DeferTest::Reset();
	return true;
}

// ── The slow tree: each child render advances the mock clock — deterministic parking ──────

struct FSlowChildProps final : public FRuitkPropsBase
{
	RUITK_PROP(int32, Index, 0)
	RUITK_PROP(int32, Frame, 1)
	RUITK_PROPS_BODY(FSlowChildProps, RUITK_EQ(Index) RUITK_EQ(Frame))
};

static FRuitkNodeArray SlowChild(FRuitkContext& Ctx, const FSlowChildProps& Props, const TArray<FRuitkNode>&)
{
	if (DeferTest::GHost != nullptr)
	{
		DeferTest::GHost->MockTimeSeconds += DeferTest::AdvanceMsPerChild * 0.001;
	}
	auto [Local, SetLocal] = Ctx.UseState<int32>(0);
	if (Props.Index == 4)
	{
		DeferTest::Setter4 = SetLocal;
	}
	return {Ruitk::TextBlock(FString::Printf(TEXT("s%d f%d l%d"), Props.Index, Props.Frame, Local))};
}
RUITK_COMPONENT(SlowChild)

static FRuitkNodeArray SlowHead(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [Frame, SetFrame] = Ctx.UseState<int32>(0);
	DeferTest::SetterB = SetFrame;
	TArray<FRuitkNode> Items;
	for (int32 i = 0; i < 5; ++i)
	{
		FSlowChildProps P;
		P.SetIndex(i);
		P.SetFrame(Frame);
		Items.Add(Ruitk::FC(&SlowChild, MoveTemp(P), {}, FRuitkKey(i)));
	}
	return {RuitkTest::Box(RuitkTest::BoxProps(TEXT("list")), MoveTemp(Items))};
}
RUITK_COMPONENT(SlowHead)

// ── Park/resume across frames + Unmount-mid-park leak check ───────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkSliceParkResumeTest, "Ruitk.Core.SliceParkResume", RUITK_TEST_FLAGS)
bool FRuitkSliceParkResumeTest::RunTest(const FString&)
{
	DeferTest::Reset();
	// Quantum 1 ms, scheduler budget 2 ms, each child costs 1 ms: the update pass MUST span
	// multiple pumped frames (~2 slices per frame, 5 children + head).
	DeferTest::FScopedWorld World(/*bSliced=*/true, /*QuantumMs=*/1.0f, /*BudgetMs=*/2.0f);
	FRuitkTestHarness H;
	DeferTest::GHost = &H.Host;
	DeferTest::AdvanceMsPerChild = 1.0;

	H.Mount(Ruitk::FC(&SlowHead)); // mount is always synchronous — even on the slow tree
	FMockNode* List = H.ChildAt(0);
	if (!TestNotNull(TEXT("list mounted synchronously"), List))
	{
		DeferTest::Reset();
		return false;
	}
	TestEqual(TEXT("5 children mounted synchronously"), List->Children.Num(), 5);

	AddInfo(TEXT("[park/resume] a sliced update spans frames; commit stays atomic"));
	DeferTest::SetterB(1); // Frame 0 -> 1: every child re-renders (props change)
	H.Host.PumpFrame();	   // first scheduler frame: the pass parks mid-tree
	TestTrue(TEXT("pass is parked after one frame"), H.Host.SchedulerHasWork());
	TestEqual(TEXT("commit is atomic: old labels while parked"), List->Children[0]->TextOf(),
			  FString(TEXT("s0 f0 l0")));

	AddInfo(TEXT("[park/resume] a mid-park update defers and lands in the ONE follow-up"));
	DeferTest::Setter4(1); // arrives while parked — defers (never discards the pass)
	TestTrue(TEXT("pass + follow-up complete"), H.Host.PumpUntilIdle(64));
	TestEqual(TEXT("still exactly 5 children (no ghosts/duplicates)"), List->Children.Num(), 5);
	TestEqual(TEXT("frame landed on child 0"), List->Children[0]->TextOf(), FString(TEXT("s0 f1 l0")));
	TestEqual(TEXT("frame landed on child 4 + mid-park local update"), List->Children[4]->TextOf(),
			  FString(TEXT("s4 f1 l1")));

	AddInfo(TEXT("[park/resume] Unmount mid-park leaks nothing"));
	DeferTest::SetterB(2);
	H.Host.PumpFrame(); // park again
	H.Reconciler->Unmount();
	TestEqual(TEXT("slab NumLive back to zero"), H.Reconciler->NumLiveFibers(), 0);
	TestFalse(TEXT("queued Slice cancelled at unmount"), H.Host.SchedulerHasWork());
	TestTrue(TEXT("post-unmount pumps are inert"), H.Host.PumpUntilIdle(8));

	DeferTest::Reset();
	return true;
}

// ── Superseded-tree redirect: a defer captured mid-pass replays onto the LIVE tree ────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkDeferSupersededRedirectTest, "Ruitk.Core.DeferSupersededRedirect",
								 RUITK_TEST_FLAGS)
bool FRuitkDeferSupersededRedirectTest::RunTest(const FString&)
{
	DeferTest::Reset();
	DeferTest::FScopedWorld World(/*bSliced=*/true, /*QuantumMs=*/1.0f, /*BudgetMs=*/2.0f);
	FRuitkTestHarness H;
	DeferTest::GHost = &H.Host;
	DeferTest::AdvanceMsPerChild = 1.0;

	H.Mount(Ruitk::FC(&SlowHead));
	FMockNode* List = H.ChildAt(0);
	if (!TestNotNull(TEXT("list mounted"), List))
	{
		DeferTest::Reset();
		return false;
	}

	// Park a full-tree pass, then update child #4 while it is parked. The deferred target is
	// a fiber of the tree the commit SUPERSEDES — replay must redirect to the live twins.
	// The failure mode this guards (the Unity comment's scenario): reconciling from the
	// stale root re-mounts already-placed subtrees as duplicates ("frozen ghost elements").
	FRuitkDiagnostics::bEnabled = true;
	FRuitkDiagnostics::Reset();
	DeferTest::SetterB(1);
	H.Host.PumpFrame(); // parked mid-tree
	DeferTest::Setter4(1);
	TestTrue(TEXT("completes"), H.Host.PumpUntilIdle(64));
	TestEqual(TEXT("NO duplicate children after the redirected replay"), List->Children.Num(), 5);
	TestEqual(TEXT("no ghost placements (updates only)"), FRuitkDiagnostics::Placements, 0);
	TestEqual(TEXT("no deletions"), FRuitkDiagnostics::Deletions, 0);
	TestEqual(TEXT("redirected update landed"), List->Children[4]->TextOf(), FString(TEXT("s4 f1 l1")));
	FRuitkDiagnostics::bEnabled = false;

	DeferTest::Reset();
	return true;
}

// ── Detached-fiber bail: an update on a released fiber warns and is ignored ───────────────

static FRuitkNodeArray DoomedChild(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	DeferTest::StaleFiber = &Ctx.GetFiber(); // captured for the post-deletion poke
	return {Ruitk::TextBlock(TEXT("doomed"))};
}
RUITK_COMPONENT(DoomedChild)

static FRuitkNodeArray ToggleParent(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [bShow, SetShow] = Ctx.UseState<bool>(true);
	DeferTest::SetterShow = SetShow;
	if (bShow)
	{
		return {Ruitk::FC(&DoomedChild)};
	}
	return {Ruitk::TextBlock(TEXT("empty"))};
}
RUITK_COMPONENT(ToggleParent)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkDeferDetachedBailTest, "Ruitk.Core.DeferDetachedBail", RUITK_TEST_FLAGS)
bool FRuitkDeferDetachedBailTest::RunTest(const FString&)
{
	DeferTest::Reset();
	AddExpectedError(TEXT("detached fiber"), EAutomationExpectedErrorFlags::Contains, 0);
	DeferTest::FScopedWorld World(/*bSliced=*/false);
	FRuitkTestHarness H;
	H.Mount(Ruitk::FC(&ToggleParent));
	TestEqual(TEXT("doomed child mounted"), H.TextAt(0), FString(TEXT("doomed")));
	TestNotNull(TEXT("stale fiber captured"), DeferTest::StaleFiber);

	// Swap the child out: its fiber is deleted and RELEASED back to the slab — the captured
	// pointer is now detached (a released/unmounted component's fiber).
	DeferTest::SetterShow(false);
	TestTrue(TEXT("toggle settles"), H.Host.PumpUntilIdle(16));
	TestEqual(TEXT("child swapped out"), H.TextAt(0), FString(TEXT("empty")));

	// Poke the reconciler with the stale pointer: it must WARN and bail — no crash, no
	// spurious render (the reference's detached-fiber warning, FiberReconciler.cs:282-289).
	FRuitkDiagnostics::bEnabled = true;
	FRuitkDiagnostics::Reset();
	H.Reconciler->ScheduleUpdateOnFiber(DeferTest::StaleFiber);
	TestTrue(TEXT("nothing scheduled by the detached poke"), H.Host.PumpUntilIdle(8));
	TestEqual(TEXT("no commit resulted"), FRuitkDiagnostics::Commits, 0);
	TestEqual(TEXT("tree unchanged"), H.TextAt(0), FString(TEXT("empty")));
	FRuitkDiagnostics::bEnabled = false;

	DeferTest::Reset();
	return true;
}

// ── Depth-25: an unconditional setState-during-render logs and stops — no hang ────────────

static FRuitkNodeArray RunawayComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	++DeferTest::RendersB;
	auto [V, SetV] = Ctx.UseState<int32>(0);
	SetV(V + 1); // UNCONDITIONAL setState during render — the classic runaway
	return {Ruitk::TextBlock(FString::Printf(TEXT("r%d"), V))};
}
RUITK_COMPONENT(RunawayComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkDeferDepth25Test, "Ruitk.Core.DeferDepth25", RUITK_TEST_FLAGS)
bool FRuitkDeferDepth25Test::RunTest(const FString&)
{
	for (const bool bSliced : {false, true})
	{
		AddInfo(FString::Printf(TEXT("[depth-25] world: %s"), bSliced ? TEXT("sliced") : TEXT("sync")));
		DeferTest::Reset();
		AddExpectedError(TEXT("Maximum render depth"), EAutomationExpectedErrorFlags::Contains, 0);
		// The unconditional mid-render setState also draws the M5 strict warning (once).
		AddExpectedError(TEXT("State update scheduled during render"), EAutomationExpectedErrorFlags::Contains, 0);
		DeferTest::FScopedWorld World(bSliced, /*QuantumMs=*/0.0f);
		FRuitkTestHarness H;
		H.Mount(Ruitk::FC(&RunawayComp));
		// Does not hang: the cascade is bounded by the depth guard, then goes quiescent.
		TestTrue(TEXT("cascade terminates (no hang)"), H.Host.PumpUntilIdle(200));
		TestTrue(TEXT("bounded render count"),
				 DeferTest::RendersB > 0 && DeferTest::RendersB <= 30); // 25-ish passes, not hundreds
		TestEqual(TEXT("over-depth pass rendered nothing"), H.RootNode()->Children.Num(), 0);
		TestTrue(TEXT("reconciler still mounted and alive"), H.Reconciler->IsMounted());
	}
	DeferTest::Reset();
	return true;
}

// ── Mount is ALWAYS synchronous, sliced world included ────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkSliceMountAlwaysSyncTest, "Ruitk.Core.SliceMountAlwaysSync", RUITK_TEST_FLAGS)
bool FRuitkSliceMountAlwaysSyncTest::RunTest(const FString&)
{
	DeferTest::Reset();
	DeferTest::FScopedWorld World(/*bSliced=*/true, /*QuantumMs=*/0.0f);
	FRuitkTestHarness H;
	DeferTest::GHost = &H.Host;
	DeferTest::AdvanceMsPerChild = 5.0; // way past any quantum — mount must not care
	H.Mount(Ruitk::FC(&SlowHead));
	// No pump has happened: the whole tree must already be committed (no empty first frame).
	FMockNode* List = H.ChildAt(0);
	if (TestNotNull(TEXT("list committed before any pump"), List))
	{
		TestEqual(TEXT("all 5 children committed before any pump"), List->Children.Num(), 5);
	}
	TestFalse(TEXT("nothing parked on the scheduler"), H.Host.SchedulerHasWork());
	DeferTest::Reset();
	return true;
}

// ── FlushSync claims a parked Slice: mid-park FlushSync commits everything ────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkSliceFlushSyncDrainTest, "Ruitk.Core.SliceFlushSyncDrain", RUITK_TEST_FLAGS)
bool FRuitkSliceFlushSyncDrainTest::RunTest(const FString&)
{
	DeferTest::Reset();
	DeferTest::FScopedWorld World(/*bSliced=*/true, /*QuantumMs=*/1.0f, /*BudgetMs=*/2.0f);
	FRuitkTestHarness H;
	DeferTest::GHost = &H.Host;
	DeferTest::AdvanceMsPerChild = 1.0;
	H.Mount(Ruitk::FC(&SlowHead));
	FMockNode* List = H.ChildAt(0);
	if (!TestNotNull(TEXT("list mounted"), List))
	{
		DeferTest::Reset();
		return false;
	}

	DeferTest::SetterB(1);
	H.Host.PumpFrame(); // parked mid-pass with a re-enqueued Slice
	TestTrue(TEXT("parked"), H.Host.SchedulerHasWork());
	H.Reconciler->FlushSync(); // must claim the queued Slice + finish the pass, unsliced
	TestEqual(TEXT("FlushSync committed the parked pass"), List->Children[4]->TextOf(), FString(TEXT("s4 f1 l0")));

	FRuitkDiagnostics::bEnabled = true;
	FRuitkDiagnostics::Reset();
	TestTrue(TEXT("drains"), H.Host.PumpUntilIdle(16));
	TestEqual(TEXT("no further commit after FlushSync"), FRuitkDiagnostics::Commits, 0);
	FRuitkDiagnostics::bEnabled = false;

	DeferTest::Reset();
	return true;
}

// ── Both worlds produce identical trees and host-op counts ────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkDeferBothWorldsTest, "Ruitk.Core.DeferBothWorldsEquivalence", RUITK_TEST_FLAGS)
bool FRuitkDeferBothWorldsTest::RunTest(const FString&)
{
	struct FWorldResult
	{
		int32 Placements = 0;
		int32 Updates = 0;
		int32 Deletions = 0;
		int32 Commits = 0;
		TArray<FString> Texts;
	};
	FWorldResult Results[2];

	for (int32 w = 0; w < 2; ++w)
	{
		const bool bSliced = (w == 1);
		DeferTest::Reset();
		DeferTest::FScopedWorld World(bSliced, /*QuantumMs=*/0.0f);
		FRuitkTestHarness H;
		H.Mount(Ruitk::FC(&SlowHead));
		TestTrue(TEXT("mount settles"), H.Host.PumpUntilIdle(64));

		FRuitkDiagnostics::bEnabled = true;
		FRuitkDiagnostics::Reset();
		DeferTest::SetterB(1); // full-tree update
		TestTrue(TEXT("update settles"), H.Host.PumpUntilIdle(64));
		DeferTest::Setter4(3); // targeted child update
		TestTrue(TEXT("child update settles"), H.Host.PumpUntilIdle(64));

		FWorldResult& R = Results[w];
		R.Placements = FRuitkDiagnostics::Placements;
		R.Updates = FRuitkDiagnostics::Updates;
		R.Deletions = FRuitkDiagnostics::Deletions;
		R.Commits = FRuitkDiagnostics::Commits;
		FMockNode* List = H.ChildAt(0);
		if (List != nullptr)
		{
			for (const TSharedPtr<FMockNode>& C : List->Children)
			{
				R.Texts.Add(C->TextOf());
			}
		}
		FRuitkDiagnostics::bEnabled = false;
	}

	TestEqual(TEXT("identical placements"), Results[1].Placements, Results[0].Placements);
	TestEqual(TEXT("identical updates"), Results[1].Updates, Results[0].Updates);
	TestEqual(TEXT("identical deletions"), Results[1].Deletions, Results[0].Deletions);
	TestEqual(TEXT("identical commits"), Results[1].Commits, Results[0].Commits);
	TestEqual(TEXT("identical final tree"), FString::Join(Results[1].Texts, TEXT("|")),
			  FString::Join(Results[0].Texts, TEXT("|")));
	TestEqual(TEXT("scenario really ran (5 leaves)"), Results[0].Texts.Num(), 5);

	DeferTest::Reset();
	return true;
}

#undef RUITK_TEST_FLAGS

#endif // WITH_DEV_AUTOMATION_TESTS
