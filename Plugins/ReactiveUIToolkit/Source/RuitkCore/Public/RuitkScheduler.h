// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// FRuitkScheduler — the family render scheduler (ruitk-unity Runtime/Core/RenderScheduler.cs,
// ported per FAMILY_PARITY_PLAN P-01/P-02): four priority lanes with a per-frame budget,
// per-lane enqueue dedup, a frame-end batched-effects flush, and batch-deferral of non-High
// enqueues.
//
// Faithful frame flow (RenderScheduler.cs:116-164):
//   - High + Low both non-empty at frame START => the ENTIRE Low queue is cancelled
//     (dropped + counted, trackers cleared with it) (:120-128).
//   - High runs; Normal only if High drained (else an escalation is counted) (:131-142).
//   - Low runs; Idle only if no foreground action ran + all three queues empty + elapsed
//     < budget/2 — and Idle gets a budget/2 sub-budget (:149-161, :177).
//   - The batched-effects flush is UNBUDGETED at frame end (:162, :225-243).
//   - Budget is checked BEFORE each dequeue, cumulative from frame start (:166-204).
//     A limit <= 0 means UNLIMITED — the reference's `budgetLimit > 0f` guard.
//
// Engine-local decisions (settled in the plan; do not re-litigate):
//   - P-01: a plain UObject-free RuitkCore class (D-27); time comes from an injectable
//     clock (defaults to FPlatformTime::Seconds) so the mock-host suites drive it with a
//     fake clock — deterministic lane/budget tests, no sleeps.
//   - P-02: C++ TFunctions have no identity (the reference dedups a HashSet<Action>), so
//     Enqueue dedups on an explicit per-lane Key; cancel-Low removes tracker entries too.
//   - Budget source: FRuitkConfig::FrameBudgetMs(), read once per PumpFrame.
//   - No try/catch around actions: UE ships without C++ exceptions — a failing render
//     goes through the D-10 cooperative latch instead.
//   - Effects that queue new batched effects during a flush land in the NEXT flush (the
//     host frame-queue batch rule; the reference would throw on that mutation).

#pragma once

#include "CoreMinimal.h"

/** Scheduler lanes, strongest first (the reference's IScheduler.Priority). */
enum class ERuitkLane : uint8
{
	High = 0,
	Normal = 1,
	Low = 2,
	Idle = 3,
};

class RUITKCORE_API FRuitkScheduler
{
public:
	/** InClockSeconds: monotonic seconds; unset = FPlatformTime::Seconds (P-01). */
	explicit FRuitkScheduler(TFunction<double()> InClockSeconds = TFunction<double()>());

	/** Enqueue Fn on Lane, deduped per lane by Key (P-02): a key already queued on that lane
	 *  is a no-op; the key frees when its action is dequeued, so re-enqueue-after-run runs
	 *  again — the self-re-enqueueing Slice pattern relies on this. During a batch, non-High
	 *  enqueues are deferred to the matching EndBatch (:54-58). Key must be non-null. */
	void Enqueue(const void* Key, TFunction<void()> Fn, ERuitkLane Lane = ERuitkLane::Normal);

	/** Remove Key's queued action from every lane (tracker included) and any batch-deferred
	 *  enqueue carrying it. Engine-local teardown/preempt API with no reference analog: a
	 *  GC'd C# delegate outliving its producer is harmless, a C++ action capturing a dead
	 *  `this` is not (the host — and so the scheduler — outlives every reconciler). NOT the
	 *  frame-start Low-cancel rule: nothing is counted. */
	void Cancel(const void* Key);

	/** Defer non-High enqueues until the matching EndBatch (nestable) (:88-114). */
	void BeginBatch();
	void EndBatch();

	/** Queue an effect for the frame-end unbudgeted flush (:206-212). */
	void EnqueueBatchedEffect(TFunction<void()> Effect);

	/** The once-per-frame pump (the reference's LateUpdate flow) — called from the host's
	 *  frame seam (Slate OnPreTick / the mock host's PumpSchedulerFrame). */
	void PumpFrame();

	/** Unbudgeted FULL drain of all four lanes + batched effects (:214-223) — what
	 *  flush-now surfaces build on. Does not count a frame and never cancels Low. */
	void PumpNow();

	/** Counters (:245-258). */
	struct FMetrics
	{
		int32 Frames = 0;		// PumpFrame calls
		int32 Actions = 0;		// actions executed, all lanes (PumpNow included)
		int32 Escalations = 0;	// frames where High failed to drain (Normal starved)
		int32 LowCancelled = 0; // Low actions dropped by the frame-start cancel rule
		int32 IdleRan = 0;		// Idle actions executed
	};
	const FMetrics& GetMetrics() const { return Metrics; }

	// --- introspection (tests) ---
	int32 NumQueued(ERuitkLane Lane) const { return LaneOf(Lane).Queue.Num(); }
	int32 NumBatchedEffects() const { return BatchedEffects.Num(); }
	int32 GetBatchDepth() const { return BatchDepth; }

private:
	struct FEntry
	{
		const void* Key = nullptr;
		TFunction<void()> Fn;
	};
	struct FLane
	{
		TArray<FEntry> Queue;	   // FIFO; actions may append mid-drain (Slice re-enqueue)
		TSet<const void*> Tracker; // P-02 dedup
	};
	/** A non-High enqueue captured during a batch — kept as data (not a closure) so
	 *  Cancel(Key) can reach it (:88-114 + the Cancel note above). */
	struct FDeferredEnqueue
	{
		const void* Key = nullptr;
		ERuitkLane Lane = ERuitkLane::Normal;
		TFunction<void()> Fn;
	};

	FLane& LaneOf(ERuitkLane Lane) { return Lanes[static_cast<int32>(Lane)]; }
	const FLane& LaneOf(ERuitkLane Lane) const { return Lanes[static_cast<int32>(Lane)]; }

	/** Run Lane until empty or the cumulative budget from FrameStartSec is exhausted
	 *  (checked BEFORE each dequeue); BudgetLimitSec <= 0 = unlimited. Returns the number
	 *  of actions executed. */
	int32 ExecuteQueue(FLane& Lane, double FrameStartSec, double BudgetLimitSec);
	void FlushBatchedEffects();
	double Now() const;

	FLane Lanes[4];
	TArray<TFunction<void()>> BatchedEffects;
	TArray<FDeferredEnqueue> DeferredBatchEnqueues;
	int32 BatchDepth = 0;
	TFunction<double()> Clock;
	FMetrics Metrics;
};
