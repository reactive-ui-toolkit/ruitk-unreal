// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// The mock host: IRuitkHostConfig over plain test nodes — what makes the ENTIRE reconciler
// testable headlessly (the react-test-renderer seam, D-11). Also the test harness and the
// test elements the ported core suites render.

#pragma once

#include "CoreMinimal.h"
#include "RuitkHostConfig.h"
#include "RuitkElementRegistry.h"
#include "RuitkReconciler.h"
#include "RuitkScheduler.h"
#include "RuitkCoreElements.h"

/** A mock host node: enough structure to assert identity, order, props, and lifecycle. */
struct FMockNode
{
	FName Tag;
	/** Raw view of the latest committed props — the fiber owns them (kept alive while
	 *  committed); updated on every CommitUpdate. Test-read only. */
	const FRuitkPropsBase* Props = nullptr;
	TArray<TSharedPtr<FMockNode>> Children;
	FMockNode* Parent = nullptr;
	int32 UpdateCount = 0;
	bool bReleased = false;

	template <typename T> const T* PropsAs() const { return static_cast<const T*>(Props); }

	FString TextOf() const
	{
		const FRuitkTextBlockProps* T = PropsAs<FRuitkTextBlockProps>();
		return T ? T->Text.ToString() : FString();
	}
};

class FRuitkMockHost final : public IRuitkHostConfig
{
public:
	TSharedRef<FMockNode> Root = MakeShared<FMockNode>();
	int32 CreatedCount = 0;
	int32 ReleasedCount = 0;
	FRuitkSafeArea SafeArea{1, 2, 3, 4};

	static FMockNode* Cast(const FRuitkHostHandle& H) { return static_cast<FMockNode*>(H.Get()); }

	virtual FRuitkHostHandle CreateInstance(FRuitkElementTypeId Type, const FRuitkPropsBase& Props) override
	{
		TSharedRef<FMockNode> Node = MakeShared<FMockNode>();
		Node->Tag = Ruitk::GetElementTypeName(Type);
		Node->Props = &Props;
		++CreatedCount;
		return Node;
	}

	virtual void CommitUpdate(const FRuitkHostHandle& Node, FRuitkElementTypeId, const FRuitkPropsBase*,
							  const FRuitkPropsBase& NewProps) override
	{
		FMockNode* N = Cast(Node);
		N->Props = &NewProps;
		++N->UpdateCount;
	}

	virtual void ReleaseInstance(const FRuitkHostHandle& Node, FRuitkElementTypeId,
								 const TSharedPtr<const FRuitkPropsBase>&, bool) override
	{
		FMockNode* N = Cast(Node);
		N->bReleased = true;
		++ReleasedCount;
		if (N->Parent != nullptr)
		{
			RemoveFrom(N->Parent, N);
		}
	}

	virtual void InsertChild(const FRuitkHostHandle& Parent, const FRuitkHostHandle& Child, int32 Index) override
	{
		FMockNode* P = Parent.IsValid() ? Cast(Parent) : &Root.Get();
		TSharedPtr<FMockNode> C = StaticCastSharedPtr<FMockNode>(Child);
		if (C->Parent != nullptr)
		{
			RemoveFrom(C->Parent, C.Get());
		}
		C->Parent = P;
		if (Index < 0 || Index >= P->Children.Num())
		{
			P->Children.Add(C);
		}
		else
		{
			P->Children.Insert(C, Index);
		}
	}

	virtual void RemoveChild(const FRuitkHostHandle& Parent, const FRuitkHostHandle& Child) override
	{
		FMockNode* P = Parent.IsValid() ? Cast(Parent) : &Root.Get();
		RemoveFrom(P, Cast(Child));
	}

	virtual void ReorderChildren(const FRuitkHostHandle& Parent, const TArray<FRuitkHostHandle>& Ordered) override
	{
		FMockNode* P = Parent.IsValid() ? Cast(Parent) : &Root.Get();
		TArray<TSharedPtr<FMockNode>> NewOrder;
		NewOrder.Reserve(Ordered.Num());
		for (const FRuitkHostHandle& H : Ordered)
		{
			FMockNode* Want = Cast(H);
			for (const TSharedPtr<FMockNode>& Existing : P->Children)
			{
				if (Existing.Get() == Want)
				{
					NewOrder.Add(Existing);
					break;
				}
			}
		}
		// Keep any host children the reconciler doesn't know (none in practice) at the end.
		for (const TSharedPtr<FMockNode>& Existing : P->Children)
		{
			if (!NewOrder.Contains(Existing))
			{
				NewOrder.Add(Existing);
			}
		}
		P->Children = MoveTemp(NewOrder);
	}

	virtual FRuitkElementTypeId GetTextElementType() const override { return Ruitk::TextBlockElementType(); }

	virtual void RequestFrame(TFunction<void()> Callback) override { FrameQueue.Add(MoveTemp(Callback)); }

	virtual void GetSafeArea(float& L, float& T, float& R, float& B) const override
	{
		L = SafeArea.Left;
		T = SafeArea.Top;
		R = SafeArea.Right;
		B = SafeArea.Bottom;
	}

	/** Settable clock — the tween hooks read host time, so tests advance deterministically. */
	double MockTimeSeconds = 0.0;
	virtual double GetTimeSeconds() const override { return MockTimeSeconds; }

	/** The frame scheduler (M2, P-01/P-03) on the SAME settable clock — lane/budget tests
	 *  advance MockTimeSeconds instead of sleeping. Pumped manually (PumpSchedulerFrame). */
	FRuitkScheduler Scheduler{TFunction<double()>([this]() { return MockTimeSeconds; })};
	virtual FRuitkScheduler* GetScheduler() override { return &Scheduler; }

	/** One scheduler "frame" (the mock's PreTick analog). */
	void PumpSchedulerFrame() { Scheduler.PumpFrame(); }

	/** Any frame callbacks queued? (tween tests assert the chain arms/drains) */
	bool HasQueuedFrames() const { return !FrameQueue.IsEmpty(); }

	/** Anything parked on the scheduler? (sliced render passes live there — M3) */
	bool SchedulerHasWork() const
	{
		return Scheduler.NumQueued(ERuitkLane::High) > 0 || Scheduler.NumQueued(ERuitkLane::Normal) > 0 ||
			   Scheduler.NumQueued(ERuitkLane::Low) > 0 || Scheduler.NumQueued(ERuitkLane::Idle) > 0 ||
			   Scheduler.NumBatchedEffects() > 0;
	}

	/** Run one host "frame", mirroring the Slate PreTick seam (M3): drain the frame
	 *  callbacks queued so far (new ones queue for the next), then pump the scheduler once —
	 *  sliced render passes and the frame-end batched-effects flush live there. */
	void PumpFrame()
	{
		TArray<TFunction<void()>> Batch = MoveTemp(FrameQueue);
		FrameQueue.Reset();
		for (TFunction<void()>& Fn : Batch)
		{
			Fn();
		}
		Scheduler.PumpFrame();
	}

	void Pump(int32 Frames = 2)
	{
		for (int32 i = 0; i < Frames; ++i)
		{
			PumpFrame();
		}
	}

	/** Pump until nothing is queued on either seam (bounded — a runaway loop fails the assert). */
	bool PumpUntilIdle(int32 MaxFrames = 64)
	{
		int32 n = 0;
		while (!FrameQueue.IsEmpty() || SchedulerHasWork())
		{
			if (++n > MaxFrames)
			{
				return false;
			}
			PumpFrame();
		}
		return true;
	}

private:
	static void RemoveFrom(FMockNode* Parent, FMockNode* Child)
	{
		Parent->Children.RemoveAll([Child](const TSharedPtr<FMockNode>& C) { return C.Get() == Child; });
		if (Child->Parent == Parent)
		{
			Child->Parent = nullptr;
		}
	}

	TArray<TFunction<void()>> FrameQueue;
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// Test elements ("Box": a generic container/leaf with a label + value + event)
// ─────────────────────────────────────────────────────────────────────────────────────────

struct FTestBoxProps final : public FRuitkPropsBase
{
	RUITK_PROP(FString, Label, 0)
	RUITK_PROP(int32, Value, 1)
	RUITK_PROP_EVENT(OnPing, 2)
	RUITK_PROPS_BODY(FTestBoxProps, RUITK_EQ(Label) RUITK_EQ(Value))
};

namespace RuitkTest
{
	inline FRuitkElementTypeId BoxType()
	{
		static FRuitkElementTypeId Id = Ruitk::InternElementType(FName(TEXT("Box")));
		return Id;
	}

	inline FRuitkNode Box(FTestBoxProps InProps = FTestBoxProps(), TArray<FRuitkNode> Children = {},
						  FRuitkKey Key = FRuitkKey())
	{
		FRuitkNode Node;
		Node.Kind = ERuitkNodeKind::Host;
		Node.ElementType = BoxType();
		Node.Props = MakeShared<FTestBoxProps>(MoveTemp(InProps));
		Node.Children = Ruitk::MakeChildren(MoveTemp(Children));
		Node.Key = Key;
		return Node;
	}

	inline FTestBoxProps BoxProps(const FString& Label, int32 Value = 0)
	{
		FTestBoxProps P;
		P.SetLabel(Label);
		P.SetValue(Value);
		return P;
	}
} // namespace RuitkTest

/** Everything a core test needs: host + reconciler + pump + tree access. */
struct FRuitkTestHarness
{
	FRuitkMockHost Host;
	TUniquePtr<FRuitkReconciler> Reconciler;

	FRuitkTestHarness() { Reconciler = MakeUnique<FRuitkReconciler>(Host, Host.Root); }

	~FRuitkTestHarness()
	{
		if (Reconciler.IsValid())
		{
			Reconciler->Unmount();
		}
	}

	void Mount(FRuitkNode Node) { Reconciler->Render(MoveTemp(Node)); }
	void Pump(int32 Frames = 2) { Host.Pump(Frames); }

	FMockNode* RootNode() { return &Host.Root.Get(); }
	FMockNode* ChildAt(int32 i) { return Host.Root->Children.IsValidIndex(i) ? Host.Root->Children[i].Get() : nullptr; }

	/** Null-safe text read: a missing node fails its TestEqual instead of crashing the
	 *  whole automation run (a crash loses the report for every suite after it). */
	FString TextAt(int32 i)
	{
		FMockNode* N = ChildAt(i);
		return N ? N->TextOf() : FString(TEXT("<no node>"));
	}
};
