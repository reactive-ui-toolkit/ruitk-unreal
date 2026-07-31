// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Ruitk.Scheduler.* — the FRuitkScheduler suite (FAMILY_PARITY_PLAN M2). Everything runs on
// a FAKE clock (P-01: injectable TFunction<double()>): actions "cost" time by advancing the
// clock variable, so lane/budget/idle semantics are asserted deterministically — no sleeps,
// no flakes. The frame-budget CVar is pinned to the family default (4.0) per test and
// RESTORED in all paths (other suites share the process).

#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "RuitkScheduler.h"

#if WITH_DEV_AUTOMATION_TESTS

#define RUITK_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace SchedulerTest
{
	/** Pin ruitk.FrameBudgetMs for the test body; restores the prior value on destruction. */
	struct FScopedBudget
	{
		IConsoleVariable* Var;
		float Prev;
		explicit FScopedBudget(float ValueMs)
			: Var(IConsoleManager::Get().FindConsoleVariable(TEXT("ruitk.FrameBudgetMs")))
		{
			Prev = Var->GetFloat();
			Var->Set(ValueMs);
		}
		~FScopedBudget() { Var->Set(Prev); }
	};

	constexpr double Ms = 0.001; // fake-clock unit
} // namespace SchedulerTest

// ── Lane order: High > Normal > Low across frames; Idle only on an idle frame ─────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkSchedulerLaneOrderTest, "Ruitk.Scheduler.LaneOrder", RUITK_TEST_FLAGS)
bool FRuitkSchedulerLaneOrderTest::RunTest(const FString&)
{
	SchedulerTest::FScopedBudget Budget(4.0f);
	double Clock = 0.0;
	FRuitkScheduler S([&Clock]() { return Clock; });
	TArray<FString> Order;
	int32 KeyA = 0, KeyB = 0, KeyC = 0, KeyD = 0, KeyI = 0;

	// Frame 1: High before Normal (enqueued in reverse).
	S.Enqueue(&KeyB, [&Order]() { Order.Add(TEXT("B")); }, ERuitkLane::Normal);
	S.Enqueue(&KeyA, [&Order]() { Order.Add(TEXT("A")); }, ERuitkLane::High);
	S.Enqueue(&KeyI, [&Order]() { Order.Add(TEXT("I")); }, ERuitkLane::Idle);
	S.PumpFrame();
	TestEqual(TEXT("frame 1: High then Normal, Idle held (foreground ran)"), FString::Join(Order, TEXT(",")),
			  FString(TEXT("A,B")));

	// Frame 2: Normal before Low (no High at frame start -> no cancel).
	S.Enqueue(&KeyC, [&Order]() { Order.Add(TEXT("C")); }, ERuitkLane::Low);
	S.Enqueue(&KeyD, [&Order]() { Order.Add(TEXT("D")); }, ERuitkLane::Normal);
	S.PumpFrame();
	TestEqual(TEXT("frame 2: Normal then Low"), FString::Join(Order, TEXT(",")), FString(TEXT("A,B,D,C")));

	// Frame 3: otherwise idle -> Idle finally runs.
	S.PumpFrame();
	TestEqual(TEXT("frame 3: Idle ran on the idle frame"), FString::Join(Order, TEXT(",")),
			  FString(TEXT("A,B,D,C,I")));
	TestEqual(TEXT("no Low was cancelled"), S.GetMetrics().LowCancelled, 0);
	TestEqual(TEXT("IdleRan counted"), S.GetMetrics().IdleRan, 1);
	return true;
}

// ── Low-cancel: High + Low both waiting at frame start drops the ENTIRE Low queue ─────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkSchedulerLowCancelTest, "Ruitk.Scheduler.LowCancel", RUITK_TEST_FLAGS)
bool FRuitkSchedulerLowCancelTest::RunTest(const FString&)
{
	SchedulerTest::FScopedBudget Budget(4.0f);
	double Clock = 0.0;
	FRuitkScheduler S([&Clock]() { return Clock; });
	int32 HighRan = 0, LowRan = 0;
	int32 KeyH = 0, KeyL1 = 0, KeyL2 = 0;

	S.Enqueue(&KeyH, [&HighRan]() { ++HighRan; }, ERuitkLane::High);
	S.Enqueue(&KeyL1, [&LowRan]() { ++LowRan; }, ERuitkLane::Low);
	S.Enqueue(&KeyL2, [&LowRan]() { ++LowRan; }, ERuitkLane::Low);
	S.PumpFrame();
	TestEqual(TEXT("High ran"), HighRan, 1);
	TestEqual(TEXT("entire Low queue cancelled, nothing ran"), LowRan, 0);
	TestEqual(TEXT("cancel counted per action"), S.GetMetrics().LowCancelled, 2);
	TestEqual(TEXT("Low queue empty after cancel"), S.NumQueued(ERuitkLane::Low), 0);

	// Tracker entries were removed with the cancel: the same key re-enqueues cleanly.
	S.Enqueue(&KeyL1, [&LowRan]() { ++LowRan; }, ERuitkLane::Low);
	S.PumpFrame();
	TestEqual(TEXT("re-enqueued Low key runs (tracker was cleared by the cancel)"), LowRan, 1);
	TestEqual(TEXT("no further cancels"), S.GetMetrics().LowCancelled, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkSchedulerLowMidFrameTest, "Ruitk.Scheduler.LowMidFrameNotCancelled",
								 RUITK_TEST_FLAGS)
bool FRuitkSchedulerLowMidFrameTest::RunTest(const FString&)
{
	SchedulerTest::FScopedBudget Budget(4.0f);
	double Clock = 0.0;
	FRuitkScheduler S([&Clock]() { return Clock; });
	bool bLowRan = false;
	int32 KeyH = 0, KeyL = 0;

	// Low enqueued MID-FRAME (from the High action, after the frame-start cancel check):
	// it is NOT cancelled and runs in the same frame's Low phase.
	S.Enqueue(&KeyH,
			  [&S, &bLowRan, &KeyL]() { S.Enqueue(&KeyL, [&bLowRan]() { bLowRan = true; }, ERuitkLane::Low); },
			  ERuitkLane::High);
	S.PumpFrame();
	TestTrue(TEXT("mid-frame Low ran the same frame"), bLowRan);
	TestEqual(TEXT("nothing was cancelled"), S.GetMetrics().LowCancelled, 0);
	return true;
}

// ── Escalation: High that fails to drain starves Normal and is counted ────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkSchedulerEscalationTest, "Ruitk.Scheduler.Escalation", RUITK_TEST_FLAGS)
bool FRuitkSchedulerEscalationTest::RunTest(const FString&)
{
	using namespace SchedulerTest;
	FScopedBudget Budget(4.0f);
	double Clock = 0.0;
	FRuitkScheduler S([&Clock]() { return Clock; });
	TArray<FString> Order;
	int32 KeyH1 = 0, KeyH2 = 0, KeyN = 0;

	S.Enqueue(&KeyH1,
			  [&]()
			  {
				  Order.Add(TEXT("H1"));
				  Clock += 5 * Ms; // blows the 4 ms budget
			  },
			  ERuitkLane::High);
	S.Enqueue(&KeyH2, [&Order]() { Order.Add(TEXT("H2")); }, ERuitkLane::High);
	S.Enqueue(&KeyN, [&Order]() { Order.Add(TEXT("N")); }, ERuitkLane::Normal);

	S.PumpFrame();
	TestEqual(TEXT("frame 1: only H1 fit the budget"), FString::Join(Order, TEXT(",")), FString(TEXT("H1")));
	TestEqual(TEXT("escalation counted (High not drained -> Normal starved)"), S.GetMetrics().Escalations, 1);
	TestEqual(TEXT("H2 still queued"), S.NumQueued(ERuitkLane::High), 1);
	TestEqual(TEXT("N still queued"), S.NumQueued(ERuitkLane::Normal), 1);

	S.PumpFrame();
	TestEqual(TEXT("frame 2: High drains then Normal runs"), FString::Join(Order, TEXT(",")),
			  FString(TEXT("H1,H2,N")));
	TestEqual(TEXT("no second escalation"), S.GetMetrics().Escalations, 1);
	return true;
}

// ── Idle gate + budget/2 sub-budget ───────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkSchedulerIdleTest, "Ruitk.Scheduler.IdleBudget", RUITK_TEST_FLAGS)
bool FRuitkSchedulerIdleTest::RunTest(const FString&)
{
	using namespace SchedulerTest;
	FScopedBudget Budget(4.0f); // half-budget = 2 ms
	double Clock = 0.0;
	FRuitkScheduler S([&Clock]() { return Clock; });
	int32 IdleRuns = 0;
	int32 KeyN = 0, KeyJ1 = 0, KeyJ2 = 0;

	// Frame 1: a frame with foreground work never runs Idle (J1 stays queued).
	S.Enqueue(&KeyJ1, [&IdleRuns]() { ++IdleRuns; }, ERuitkLane::Idle);
	S.Enqueue(&KeyN, []() {}, ERuitkLane::Normal);
	S.PumpFrame();
	TestEqual(TEXT("Idle held while foreground ran"), IdleRuns, 0);

	// Frame 2: an otherwise-idle frame runs Idle. The budget check is BEFORE each dequeue,
	// so J1 (cheap) runs at elapsed 0 and J2 is dequeued at elapsed 0 too — its 3 ms cost
	// lands AFTER it ran, past the 2 ms sub-budget.
	S.Enqueue(&KeyJ2,
			  [&]()
			  {
				  ++IdleRuns;
				  Clock += 3 * Ms; // over the 2 ms half-budget
			  },
			  ERuitkLane::Idle);
	S.PumpFrame();
	TestEqual(TEXT("idle frame ran both queued idle actions (check precedes dequeue)"), IdleRuns, 2);

	// Frame 3: the sub-budget cutoff mid-queue — a costly idle action blocks the NEXT entry.
	int32 KeyJ3 = 0, KeyJ4 = 0;
	bool bJ3Ran = false, bJ4Ran = false;
	S.Enqueue(&KeyJ3, [&bJ3Ran]() { bJ3Ran = true; }, ERuitkLane::Idle);
	S.Enqueue(&KeyJ1,
			  [&]()
			  {
				  ++IdleRuns;
				  Clock += 3 * Ms;
			  },
			  ERuitkLane::Idle);
	S.Enqueue(&KeyJ4, [&bJ4Ran]() { bJ4Ran = true; }, ERuitkLane::Idle);
	S.PumpFrame(); // J3 (0 < 2 ms), J1 (0 < 2 ms, then +3 ms), J4 blocked (3 > 2)
	TestTrue(TEXT("J3 ran under the sub-budget"), bJ3Ran);
	TestEqual(TEXT("J1 ran and exhausted the sub-budget"), IdleRuns, 3);
	TestFalse(TEXT("J4 blocked by the budget/2 cutoff"), bJ4Ran);
	TestEqual(TEXT("J4 still queued for the next idle frame"), S.NumQueued(ERuitkLane::Idle), 1);

	// Frame 4: J4 lands.
	S.PumpFrame();
	TestTrue(TEXT("J4 ran on the next idle frame"), bJ4Ran);
	TestEqual(TEXT("IdleRan metric counts every idle action (J1,J2 + J3,J1 + J4)"), S.GetMetrics().IdleRan, 5);
	return true;
}

// ── Per-lane dedup by key ─────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkSchedulerDedupTest, "Ruitk.Scheduler.DedupByKey", RUITK_TEST_FLAGS)
bool FRuitkSchedulerDedupTest::RunTest(const FString&)
{
	SchedulerTest::FScopedBudget Budget(4.0f);
	double Clock = 0.0;
	FRuitkScheduler S([&Clock]() { return Clock; });
	int32 Runs = 0, OtherLaneRuns = 0;
	int32 Key = 0;

	// Double enqueue with the same key on the same lane: runs once.
	S.Enqueue(&Key, [&Runs]() { ++Runs; }, ERuitkLane::Normal);
	S.Enqueue(&Key, [&Runs]() { ++Runs; }, ERuitkLane::Normal);
	TestEqual(TEXT("second enqueue deduped"), S.NumQueued(ERuitkLane::Normal), 1);
	// The SAME key on a DIFFERENT lane is independent (per-lane trackers).
	S.Enqueue(&Key, [&OtherLaneRuns]() { ++OtherLaneRuns; }, ERuitkLane::High);
	S.PumpFrame();
	TestEqual(TEXT("ran once on Normal"), Runs, 1);
	TestEqual(TEXT("ran once on High (independent tracker)"), OtherLaneRuns, 1);

	// Re-enqueue after run: the key freed on dequeue, so it runs again.
	S.Enqueue(&Key, [&Runs]() { ++Runs; }, ERuitkLane::Normal);
	S.PumpFrame();
	TestEqual(TEXT("re-enqueue after run runs again"), Runs, 2);
	TestEqual(TEXT("actions metric exact"), S.GetMetrics().Actions, 3);
	return true;
}

// ── Self-re-enqueue (the Slice pattern): two quanta can run inside one frame budget ───────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkSchedulerSliceReentryTest, "Ruitk.Scheduler.SliceReentry", RUITK_TEST_FLAGS)
bool FRuitkSchedulerSliceReentryTest::RunTest(const FString&)
{
	using namespace SchedulerTest;
	FScopedBudget Budget(4.0f);
	double Clock = 0.0;
	FRuitkScheduler S([&Clock]() { return Clock; });
	int32 Slices = 0;
	int32 Key = 0;

	// A ~2 ms "slice" that re-enqueues itself while work remains: with a 4 ms budget the
	// SAME frame runs it twice — the check is BEFORE each dequeue, so slices start at
	// elapsed 0 and 2.5 ms, and the third (5 ms) is blocked. 2.5 keeps every comparison
	// safely off the 4.0 boundary — the semantics under test are "check before dequeue,
	// cumulative", not FP tie-breaking.
	TFunction<void()> Slice = [&]()
	{
		++Slices;
		Clock += 2.5 * Ms;
		if (Slices < 5)
		{
			S.Enqueue(&Key, Slice, ERuitkLane::Normal);
		}
	};
	S.Enqueue(&Key, Slice, ERuitkLane::Normal);
	S.PumpFrame();
	TestEqual(TEXT("two ~2 ms slices fit the 4 ms budget in one frame"), Slices, 2);
	TestEqual(TEXT("continuation parked for the next frame"), S.NumQueued(ERuitkLane::Normal), 1);
	S.PumpFrame();
	TestEqual(TEXT("next frame runs two more"), Slices, 4);
	S.PumpFrame();
	TestEqual(TEXT("last slice completes"), Slices, 5);
	TestEqual(TEXT("no stale queue entries"), S.NumQueued(ERuitkLane::Normal), 0);
	return true;
}

// ── Budget cut-off mid-queue resumes next frame (Normal lane, no escalation) ──────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkSchedulerBudgetCutoffTest, "Ruitk.Scheduler.BudgetCutoff", RUITK_TEST_FLAGS)
bool FRuitkSchedulerBudgetCutoffTest::RunTest(const FString&)
{
	using namespace SchedulerTest;
	FScopedBudget Budget(4.0f);
	double Clock = 0.0;
	FRuitkScheduler S([&Clock]() { return Clock; });
	TArray<FString> Order;
	int32 K1 = 0, K2 = 0, K3 = 0;

	auto Costly = [&](const TCHAR* Name)
	{
		Order.Add(Name);
		Clock += 3 * Ms;
	};
	S.Enqueue(&K1, [&Costly]() { Costly(TEXT("N1")); }, ERuitkLane::Normal);
	S.Enqueue(&K2, [&Costly]() { Costly(TEXT("N2")); }, ERuitkLane::Normal);
	S.Enqueue(&K3, [&Order]() { Order.Add(TEXT("N3")); }, ERuitkLane::Normal);

	S.PumpFrame(); // N1 (0<4), N2 (3<4), N3 blocked (6>4)
	TestEqual(TEXT("frame 1 ran to the cumulative budget"), FString::Join(Order, TEXT(",")), FString(TEXT("N1,N2")));
	TestEqual(TEXT("remainder parked"), S.NumQueued(ERuitkLane::Normal), 1);
	TestEqual(TEXT("Normal overrun is NOT an escalation (High-only rule)"), S.GetMetrics().Escalations, 0);

	S.PumpFrame();
	TestEqual(TEXT("frame 2 resumes where it parked"), FString::Join(Order, TEXT(",")), FString(TEXT("N1,N2,N3")));
	return true;
}

// ── Batched effects: unbudgeted frame-end flush, even on an exhausted frame ───────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkSchedulerBatchedEffectsTest, "Ruitk.Scheduler.BatchedEffects",
								 RUITK_TEST_FLAGS)
bool FRuitkSchedulerBatchedEffectsTest::RunTest(const FString&)
{
	using namespace SchedulerTest;
	FScopedBudget Budget(4.0f);
	double Clock = 0.0;
	FRuitkScheduler S([&Clock]() { return Clock; });
	bool bEffectRan = false, bMidFrameEffectRan = false;
	int32 KeyN = 0;

	S.EnqueueBatchedEffect([&bEffectRan]() { bEffectRan = true; });
	S.Enqueue(&KeyN,
			  [&]()
			  {
				  Clock += 10 * Ms; // exhaust the whole frame budget
				  S.EnqueueBatchedEffect([&bMidFrameEffectRan]() { bMidFrameEffectRan = true; });
			  },
			  ERuitkLane::Normal);
	S.PumpFrame();
	TestTrue(TEXT("pre-queued effect flushed despite the exhausted budget (unbudgeted)"), bEffectRan);
	TestTrue(TEXT("mid-frame effect flushed the same frame"), bMidFrameEffectRan);
	TestEqual(TEXT("no effects left"), S.NumBatchedEffects(), 0);
	return true;
}

// ── PumpNow: unbudgeted full drain of every lane + effects ────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkSchedulerPumpNowTest, "Ruitk.Scheduler.PumpNow", RUITK_TEST_FLAGS)
bool FRuitkSchedulerPumpNowTest::RunTest(const FString&)
{
	using namespace SchedulerTest;
	FScopedBudget Budget(4.0f);
	double Clock = 0.0;
	FRuitkScheduler S([&Clock]() { return Clock; });
	int32 Ran = 0;
	bool bEffectRan = false;
	int32 KH = 0, KN = 0, KL = 0, KI = 0;

	auto Costly = [&]()
	{
		++Ran;
		Clock += 10 * Ms; // each action alone would exhaust a frame budget
	};
	S.Enqueue(&KH, Costly, ERuitkLane::High);
	S.Enqueue(&KN, Costly, ERuitkLane::Normal);
	S.Enqueue(&KL, Costly, ERuitkLane::Low);
	S.Enqueue(&KI, Costly, ERuitkLane::Idle);
	S.EnqueueBatchedEffect([&bEffectRan]() { bEffectRan = true; });

	S.PumpNow();
	TestEqual(TEXT("all four lanes drained regardless of budget"), Ran, 4);
	TestTrue(TEXT("batched effects flushed"), bEffectRan);
	TestEqual(TEXT("PumpNow does not count a frame"), S.GetMetrics().Frames, 0);
	TestEqual(TEXT("PumpNow cancels nothing"), S.GetMetrics().LowCancelled, 0);
	return true;
}

// ── BeginBatch/EndBatch: non-High enqueues defer to batch end (nestable) ──────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkSchedulerBatchDeferTest, "Ruitk.Scheduler.BatchDefer", RUITK_TEST_FLAGS)
bool FRuitkSchedulerBatchDeferTest::RunTest(const FString&)
{
	SchedulerTest::FScopedBudget Budget(4.0f);
	double Clock = 0.0;
	FRuitkScheduler S([&Clock]() { return Clock; });
	TArray<FString> Order;
	int32 KeyH = 0, KeyN = 0;

	S.BeginBatch();
	S.Enqueue(&KeyN, [&Order]() { Order.Add(TEXT("N")); }, ERuitkLane::Normal);
	TestEqual(TEXT("Normal deferred during batch"), S.NumQueued(ERuitkLane::Normal), 0);
	S.Enqueue(&KeyH, [&Order]() { Order.Add(TEXT("H")); }, ERuitkLane::High);
	TestEqual(TEXT("High bypasses the batch"), S.NumQueued(ERuitkLane::High), 1);

	S.BeginBatch(); // nested
	S.EndBatch();
	TestEqual(TEXT("inner EndBatch does not release"), S.NumQueued(ERuitkLane::Normal), 0);
	S.EndBatch();
	TestEqual(TEXT("outer EndBatch releases the deferred enqueue"), S.NumQueued(ERuitkLane::Normal), 1);

	S.PumpFrame();
	TestEqual(TEXT("execution order preserved"), FString::Join(Order, TEXT(",")), FString(TEXT("H,N")));
	TestEqual(TEXT("unbalanced EndBatch is a no-op"), S.GetBatchDepth(), 0);
	S.EndBatch();
	TestEqual(TEXT("depth never goes negative"), S.GetBatchDepth(), 0);
	return true;
}

// ── Metrics: exact counters over a scripted scenario ──────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkSchedulerMetricsTest, "Ruitk.Scheduler.Metrics", RUITK_TEST_FLAGS)
bool FRuitkSchedulerMetricsTest::RunTest(const FString&)
{
	using namespace SchedulerTest;
	FScopedBudget Budget(4.0f);
	double Clock = 0.0;
	FRuitkScheduler S([&Clock]() { return Clock; });
	int32 KH1 = 0, KH2 = 0, KN = 0, KL1 = 0, KL2 = 0, KI = 0;

	S.Enqueue(&KH1, [&Clock]() { Clock += 5 * Ms; }, ERuitkLane::High);
	S.Enqueue(&KH2, []() {}, ERuitkLane::High);
	S.Enqueue(&KN, []() {}, ERuitkLane::Normal);
	S.Enqueue(&KL1, []() {}, ERuitkLane::Low);
	S.Enqueue(&KL2, []() {}, ERuitkLane::Low);

	S.PumpFrame(); // cancel L1+L2; H1 runs (busts budget); H2 blocked -> escalation; N starved
	S.PumpFrame(); // H2, N run
	S.PumpFrame(); // empty frame
	S.Enqueue(&KI, []() {}, ERuitkLane::Idle);
	S.PumpFrame(); // idle frame: I runs

	const FRuitkScheduler::FMetrics& M = S.GetMetrics();
	TestEqual(TEXT("frames"), M.Frames, 4);
	TestEqual(TEXT("actions (H1,H2,N,I)"), M.Actions, 4);
	TestEqual(TEXT("escalations"), M.Escalations, 1);
	TestEqual(TEXT("lowCancelled"), M.LowCancelled, 2);
	TestEqual(TEXT("idleRan"), M.IdleRan, 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
