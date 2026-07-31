// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuitkScheduler.h"
#include "RuitkCoreMisc.h"
#include "HAL/PlatformTime.h"

FRuitkScheduler::FRuitkScheduler(TFunction<double()> InClockSeconds) : Clock(MoveTemp(InClockSeconds))
{
}

double FRuitkScheduler::Now() const
{
	return Clock ? Clock() : FPlatformTime::Seconds();
}

void FRuitkScheduler::Enqueue(const void* Key, TFunction<void()> Fn, ERuitkLane Lane)
{
	if (Key == nullptr || !Fn)
	{
		return;
	}
	// Batch rule (:54-58): non-High enqueues made during a batch land at EndBatch — the
	// whole Enqueue (dedup included) replays then, so the dedup sees EndBatch-time state.
	if (BatchDepth > 0 && Lane != ERuitkLane::High)
	{
		DeferredBatchEnqueues.Add([this, Key, Lane, DeferredFn = MoveTemp(Fn)]() mutable
								  { Enqueue(Key, MoveTemp(DeferredFn), Lane); });
		return;
	}
	FLane& L = LaneOf(Lane);
	bool bAlreadyQueued = false;
	L.Tracker.Add(Key, &bAlreadyQueued);
	if (!bAlreadyQueued)
	{
		L.Queue.Add(FEntry{Key, MoveTemp(Fn)});
	}
}

void FRuitkScheduler::BeginBatch()
{
	++BatchDepth;
}

void FRuitkScheduler::EndBatch()
{
	if (BatchDepth == 0)
	{
		return;
	}
	--BatchDepth;
	if (BatchDepth > 0 || DeferredBatchEnqueues.IsEmpty())
	{
		return;
	}
	TArray<TFunction<void()>> Snapshot = MoveTemp(DeferredBatchEnqueues);
	DeferredBatchEnqueues.Reset();
	for (TFunction<void()>& Replay : Snapshot)
	{
		Replay();
	}
}

void FRuitkScheduler::EnqueueBatchedEffect(TFunction<void()> Effect)
{
	if (Effect)
	{
		BatchedEffects.Add(MoveTemp(Effect));
	}
}

void FRuitkScheduler::PumpFrame()
{
	const double FrameStart = Now();
	const double BudgetSec = static_cast<double>(FRuitkConfig::FrameBudgetMs()) / 1000.0;

	FLane& High = LaneOf(ERuitkLane::High);
	FLane& Normal = LaneOf(ERuitkLane::Normal);
	FLane& Low = LaneOf(ERuitkLane::Low);
	FLane& Idle = LaneOf(ERuitkLane::Idle);

	// Low-cancel rule (:120-128): High + Low both waiting at frame START => the ENTIRE Low
	// queue is dropped and counted, tracker entries removed with it (so a later re-enqueue
	// of the same key is accepted).
	if (!High.Queue.IsEmpty() && !Low.Queue.IsEmpty())
	{
		Metrics.LowCancelled += Low.Queue.Num();
		Low.Queue.Reset();
		Low.Tracker.Reset();
	}

	const int32 HighRan = ExecuteQueue(High, FrameStart, BudgetSec);
	int32 NormalRan = 0;
	if (High.Queue.IsEmpty())
	{
		NormalRan = ExecuteQueue(Normal, FrameStart, BudgetSec);
	}
	else
	{
		++Metrics.Escalations; // High failed to drain inside the budget — Normal starved (:139-142)
	}
	const int32 LowRan = ExecuteQueue(Low, FrameStart, BudgetSec);

	// Idle gate (:149-161): only on an otherwise-idle frame, under half the budget, with a
	// budget/2 sub-budget (:177).
	const bool bRanForeground = HighRan > 0 || NormalRan > 0 || LowRan > 0;
	const bool bQueuesEmpty = High.Queue.IsEmpty() && Normal.Queue.IsEmpty() && Low.Queue.IsEmpty();
	if (!bRanForeground && bQueuesEmpty && (Now() - FrameStart) < BudgetSec * 0.5)
	{
		Metrics.IdleRan += ExecuteQueue(Idle, FrameStart, BudgetSec * 0.5);
	}

	FlushBatchedEffects(); // UNBUDGETED (:162)
	++Metrics.Frames;
}

void FRuitkScheduler::PumpNow()
{
	// Unbudgeted FULL drain (:214-223): flush-now surfaces must leave nothing parked.
	// No Low-cancel, no idle gate, no frame count — this is not a frame.
	const double FrameStart = Now();
	ExecuteQueue(LaneOf(ERuitkLane::High), FrameStart, 0.0);
	ExecuteQueue(LaneOf(ERuitkLane::Normal), FrameStart, 0.0);
	ExecuteQueue(LaneOf(ERuitkLane::Low), FrameStart, 0.0);
	ExecuteQueue(LaneOf(ERuitkLane::Idle), FrameStart, 0.0);
	FlushBatchedEffects();
}

int32 FRuitkScheduler::ExecuteQueue(FLane& Lane, double FrameStartSec, double BudgetLimitSec)
{
	if (Lane.Queue.IsEmpty())
	{
		return 0;
	}
	if (BudgetLimitSec < 0.0)
	{
		BudgetLimitSec = 0.0; // the reference clamp (:178-181); <= 0 = unlimited (:186 guard)
	}
	int32 Executed = 0;
	while (!Lane.Queue.IsEmpty())
	{
		// Budget check BEFORE dequeue, cumulative from frame start (:183-189).
		if (BudgetLimitSec > 0.0 && (Now() - FrameStartSec) > BudgetLimitSec)
		{
			break;
		}
		FEntry Entry = MoveTemp(Lane.Queue[0]);
		Lane.Queue.RemoveAt(0, EAllowShrinking::No);
		Lane.Tracker.Remove(Entry.Key);
		// May re-enqueue itself (the Slice pattern) — the appended entry runs in THIS drain
		// if budget allows, which is how two 2 ms slices fit one 4 ms frame. No try/catch:
		// UE ships without exceptions (D-10 latch covers render failures).
		Entry.Fn();
		++Metrics.Actions;
		++Executed;
	}
	return Executed;
}

void FRuitkScheduler::FlushBatchedEffects()
{
	if (BatchedEffects.IsEmpty())
	{
		return;
	}
	// Swap-out before running: an effect that queues a new batched effect lands in the NEXT
	// flush (the host frame-queue batch rule; the reference's foreach would throw on that).
	TArray<TFunction<void()>> Batch = MoveTemp(BatchedEffects);
	BatchedEffects.Reset();
	for (TFunction<void()>& Effect : Batch)
	{
		Effect();
	}
}
