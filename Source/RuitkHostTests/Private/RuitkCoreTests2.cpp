// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// core_test.gd port, part 2: hook semantics, signals, Suspense, the error latch, fragments,
// reuse_by_slot, time slicing, diagnostics counters.

#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "RuitkMockHost.h"
#include "RuitkContext.h"
#include "RuitkSignal.h"
#include "RuitkCoreElements.h"

#if WITH_DEV_AUTOMATION_TESTS

#define RUITK_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace CoreTest2
{
	static int32 Renders = 0;
	static int32 MemoCalls = 0;
	static TRuitkSetter<int32> IntSetter;
	static TFunction<void(FString)> Dispatch;
	static TArray<FString> Order;
	static FString Seen;
	static int32 SeenInt = 0;

	static void Reset()
	{
		Renders = 0;
		MemoCalls = 0;
		IntSetter = TRuitkSetter<int32>();
		Dispatch = nullptr;
		Order.Reset();
		Seen.Empty();
		SeenInt = 0;
	}
} // namespace CoreTest2

// â”€â”€ Reducer + memo
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static FRuitkNodeArray ReducerMemoComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [Value, Disp] = Ctx.UseReducer<int32, FString>(
		[](const int32& S, const FString& A) { return A == TEXT("inc") ? S + 1 : (A == TEXT("dec") ? S - 1 : S); }, 10);
	CoreTest2::Dispatch = Disp;
	const int32 V = Value;
	const int32 Doubled = Ctx.UseMemo<int32>(
		[V]()
		{
			++CoreTest2::MemoCalls;
			return V * 2;
		},
		Ruitk::Deps(V));
	return {Ruitk::TextBlock(FString::Printf(TEXT("%d/%d"), V, Doubled))};
}
RUITK_COMPONENT(ReducerMemoComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkCoreReducerMemoTest, "Ruitk.Core.ReducerAndMemo", RUITK_TEST_FLAGS)
bool FRuitkCoreReducerMemoTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	FRuitkTestHarness H;
	H.Mount(Ruitk::FC(&ReducerMemoComp));
	TestEqual(TEXT("initial reducer+memo"), H.TextAt(0), FString(TEXT("10/20")));
	TestEqual(TEXT("memo computed once"), CoreTest2::MemoCalls, 1);

	CoreTest2::Dispatch(TEXT("inc"));
	H.Pump();
	TestEqual(TEXT("after inc"), H.TextAt(0), FString(TEXT("11/22")));
	TestEqual(TEXT("memo recomputed on dep change"), CoreTest2::MemoCalls, 2);
	return true;
}

// â”€â”€ Layout-before-passive ordering
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static FRuitkNodeArray LayoutOrderComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	Ctx.UseLayoutEffect([]() { CoreTest2::Order.Add(TEXT("layout")); }, Ruitk::Deps());
	Ctx.UseEffect([]() { CoreTest2::Order.Add(TEXT("passive")); }, Ruitk::Deps());
	return {Ruitk::TextBlock(TEXT("x"))};
}
RUITK_COMPONENT(LayoutOrderComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkCoreLayoutEffectTest, "Ruitk.Core.LayoutEffectOrder", RUITK_TEST_FLAGS)
bool FRuitkCoreLayoutEffectTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	FRuitkTestHarness H;
	H.Mount(Ruitk::FC(&LayoutOrderComp));
	TestEqual(TEXT("layout before passive"), FString::Join(CoreTest2::Order, TEXT(",")),
			  FString(TEXT("layout,passive")));
	return true;
}

// â”€â”€ State equality semantics (Â§5: value-equality for value types â€” DOCUMENTED divergence
//    from Godot's ref-equality on fresh-but-equal collections)
//    â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static FRuitkNodeArray EqualityComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	++CoreTest2::Renders;
	auto [Arr, SetArr] = Ctx.UseState<TArray<int32>>(TArray<int32>{1, 2, 3});
	CoreTest2::SeenInt = Arr.Num();
	// expose via a lambda the test can call
	CoreTest2::Dispatch = [SetArr](FString Cmd)
	{
		if (Cmd == TEXT("equal"))
		{
			SetArr(TArray<int32>{1, 2, 3}); // equal by VALUE -> bails (C++ semantics)
		}
		else
		{
			SetArr(TArray<int32>{1, 2, 3, 4});
		}
	};
	return {Ruitk::TextBlock(FString::FromInt(Arr.Num()))};
}
RUITK_COMPONENT(EqualityComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkCoreEqualityTest, "Ruitk.Core.StateEqualitySemantics", RUITK_TEST_FLAGS)
bool FRuitkCoreEqualityTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	FRuitkTestHarness H;
	H.Mount(Ruitk::FC(&EqualityComp));
	TestEqual(TEXT("mounted"), CoreTest2::Renders, 1);

	AddInfo(TEXT("[equality] value-equal set BAILS (documented Â§5 divergence: C++ containers are values)"));
	CoreTest2::Dispatch(TEXT("equal"));
	H.Pump();
	TestEqual(TEXT("no re-render on value-equal set"), CoreTest2::Renders, 1);

	AddInfo(TEXT("[equality] a different value re-renders"));
	CoreTest2::Dispatch(TEXT("diff"));
	H.Pump();
	TestEqual(TEXT("re-rendered"), CoreTest2::Renders, 2);
	TestEqual(TEXT("new size seen"), CoreTest2::SeenInt, 4);
	return true;
}

// â”€â”€ Signals: basic + unmount unsubscribes + keyed + selector re-bind
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static TSharedPtr<TRuitkSignal<int32>> GSigInt;

static FRuitkNodeArray SignalComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	++CoreTest2::Renders;
	CoreTest2::SeenInt = Ruitk::UseSignal<int32>(Ctx, GSigInt.ToSharedRef());
	return {Ruitk::TextBlock(FString::FromInt(CoreTest2::SeenInt))};
}
RUITK_COMPONENT(SignalComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkCoreSignalTest, "Ruitk.Core.Signal", RUITK_TEST_FLAGS)
bool FRuitkCoreSignalTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	GSigInt = MakeShared<TRuitkSignal<int32>>(0);
	{
		FRuitkTestHarness H;
		H.Mount(Ruitk::FC(&SignalComp));
		H.Pump(); // effect subscribes post-commit
		TestEqual(TEXT("initial value"), CoreTest2::SeenInt, 0);
		TestEqual(TEXT("one render"), CoreTest2::Renders, 1);
		TestEqual(TEXT("subscribed in effect"), GSigInt->NumSubscribers(), 1);

		GSigInt->Set(5);
		H.Pump();
		TestEqual(TEXT("signal update propagated"), CoreTest2::SeenInt, 5);
		TestEqual(TEXT("re-rendered on change"), CoreTest2::Renders, 2);

		H.Reconciler->Unmount();
	}
	TestEqual(TEXT("unmount unsubscribed"), GSigInt->NumSubscribers(), 0);
	const int32 RendersAfter = CoreTest2::Renders;
	GSigInt->Set(99);
	TestEqual(TEXT("no re-render after unmount"), CoreTest2::Renders, RendersAfter);
	GSigInt.Reset();
	return true;
}

static FRuitkNodeArray SignalKeyCompA(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	++CoreTest2::Renders;
	CoreTest2::SeenInt = Ruitk::UseSignalKey<int32>(Ctx, FName(TEXT("counter")), 10);
	return {Ruitk::TextBlock(FString::FromInt(CoreTest2::SeenInt))};
}
RUITK_COMPONENT(SignalKeyCompA)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkCoreSignalKeyTest, "Ruitk.Core.SignalKey", RUITK_TEST_FLAGS)
bool FRuitkCoreSignalKeyTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	Ruitk::ClearSignals();
	FRuitkTestHarness H1, H2;
	H1.Mount(Ruitk::FC(&SignalKeyCompA));
	H2.Mount(Ruitk::FC(&SignalKeyCompA));
	H1.Pump();
	H2.Pump();
	TestEqual(TEXT("both keyed readers mounted"), CoreTest2::Renders, 2);
	TSharedRef<TRuitkSignal<int32>> Sig = Ruitk::GetOrCreateSignal<int32>(FName(TEXT("counter")), 10);
	TestEqual(TEXT("shared keyed signal carries initial"), Sig->Get(), 10);

	Sig->Set(20);
	H1.Pump();
	H2.Pump();
	TestEqual(TEXT("both readers re-rendered"), CoreTest2::Renders, 4);
	TestEqual(TEXT("readers see 20"), CoreTest2::SeenInt, 20);
	Ruitk::ClearSignals();
	return true;
}

static TSharedPtr<TRuitkSignal<TMap<FString, int32>>> GSigMap;

static FRuitkNodeArray SignalRebindComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [Key, SetKey] = Ctx.UseState<FString>(FString(TEXT("a")));
	CoreTest2::Dispatch = [SetKey](FString K) { SetKey(K); };
	const FString K = Key;
	CoreTest2::SeenInt = Ruitk::UseSignal<TMap<FString, int32>, int32>(
		Ctx, GSigMap.ToSharedRef(), [K](const TMap<FString, int32>& M) { return M.FindRef(K); });
	return {Ruitk::TextBlock(FString::FromInt(CoreTest2::SeenInt))};
}
RUITK_COMPONENT(SignalRebindComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkCoreSignalRebindTest, "Ruitk.Core.SignalSelectorRebind", RUITK_TEST_FLAGS)
bool FRuitkCoreSignalRebindTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	TMap<FString, int32> Init;
	Init.Add(TEXT("a"), 10);
	Init.Add(TEXT("b"), 20);
	GSigMap = MakeShared<TRuitkSignal<TMap<FString, int32>>>(Init);
	FRuitkTestHarness H;
	H.Mount(Ruitk::FC(&SignalRebindComp));
	H.Pump();
	TestEqual(TEXT("selects 'a' = 10"), CoreTest2::SeenInt, 10);

	CoreTest2::Dispatch(TEXT("b")); // selector key changes -> hook re-selects on re-render
	H.Pump();
	TestEqual(TEXT("re-bound selector reads 'b' = 20"), CoreTest2::SeenInt, 20);
	GSigMap.Reset();
	return true;
}

// â”€â”€ MemoEquals (V.memo parity): custom comparer forces bail despite changed props â”€â”€â”€â”€â”€â”€â”€â”€â”€

struct FLabelPropsFwd final : public FRuitkPropsBase
{
	RUITK_PROP(int32, V, 0)
	RUITK_PROPS_BODY(FLabelPropsFwd, RUITK_EQ(V))
};

static FRuitkNodeArray MemoEqChildComp(FRuitkContext&, const FLabelPropsFwd&, const TArray<FRuitkNode>&)
{
	++CoreTest2::Renders;
	return {Ruitk::TextBlock(TEXT("x"))};
}
RUITK_COMPONENT(MemoEqChildComp)

static FRuitkNodeArray MemoEqParentComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [S, SetS] = Ctx.UseState<int32>(0);
	CoreTest2::IntSetter = SetS;
	FLabelPropsFwd P;
	P.SetV(S);
	P.MemoEquals = [](const FRuitkPropsBase&, const FRuitkPropsBase&) { return true; }; // "always equal"
	return {Ruitk::FC(&MemoEqChildComp, MoveTemp(P))};
}
RUITK_COMPONENT(MemoEqParentComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkCoreMemoEqTest, "Ruitk.Core.MemoEquals", RUITK_TEST_FLAGS)
bool FRuitkCoreMemoEqTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	FRuitkTestHarness H;
	H.Mount(Ruitk::FC(&MemoEqParentComp));
	TestEqual(TEXT("memo child rendered once"), CoreTest2::Renders, 1);
	CoreTest2::IntSetter(1);
	H.Pump();
	TestEqual(TEXT("memo child did NOT re-render (custom comparer)"), CoreTest2::Renders, 1);
	return true;
}

// â”€â”€ Suspense: fallback until IsReady flips
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static TSharedPtr<bool> GReadyFlag;

static FRuitkNodeArray SuspenseHostComp(FRuitkContext&, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	TSharedPtr<bool> Flag = GReadyFlag;
	return {Ruitk::Suspense([Flag]() { return Flag.IsValid() && *Flag; }, Ruitk::TextBlock(TEXT("loading")),
							{Ruitk::TextBlock(TEXT("loaded"))})};
}
RUITK_COMPONENT(SuspenseHostComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkCoreSuspenseTest, "Ruitk.Core.Suspense", RUITK_TEST_FLAGS)
bool FRuitkCoreSuspenseTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	GReadyFlag = MakeShared<bool>(false);
	FRuitkTestHarness H;
	H.Mount(Ruitk::FC(&SuspenseHostComp));
	H.Pump(); // effect arms the poll driver
	TestEqual(TEXT("fallback initially"), H.TextAt(0), FString(TEXT("loading")));

	*GReadyFlag = true;
	H.Pump(4); // poll fires -> SetReady -> re-render
	TestEqual(TEXT("children after ready"), H.TextAt(0), FString(TEXT("loaded")));
	GReadyFlag.Reset();
	return true;
}

// â”€â”€ Error boundary + the cooperative latch (D-10)
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static TSharedPtr<bool> GShouldFail;
static TSharedPtr<FString> GCaught;

static FRuitkNodeArray FailingComp(FRuitkContext&, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	if (GShouldFail.IsValid() && *GShouldFail)
	{
		RUITK_RENDER_FAIL(TEXT("boom"));
		return {};
	}
	return {Ruitk::TextBlock(TEXT("fine"))};
}
RUITK_COMPONENT(FailingComp)

static FRuitkNodeArray BoundaryHostComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [ResetKey, SetResetKey] = Ctx.UseState<int32>(0);
	CoreTest2::IntSetter = SetResetKey;
	return {Ruitk::ErrorBoundary(Ruitk::TextBlock(TEXT("fallback")), {Ruitk::FC(&FailingComp)}, FRuitkKey(ResetKey),
								 [](const FString& Reason)
								 {
									 if (GCaught.IsValid())
									 {
										 *GCaught = Reason;
									 }
								 })};
}
RUITK_COMPONENT(BoundaryHostComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkCoreErrorBoundaryTest, "Ruitk.Core.ErrorBoundary", RUITK_TEST_FLAGS)
bool FRuitkCoreErrorBoundaryTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	GShouldFail = MakeShared<bool>(true);
	GCaught = MakeShared<FString>();
	AddExpectedError(TEXT("render failed"), EAutomationExpectedErrorFlags::Contains, 0);

	FRuitkTestHarness H;
	H.Mount(Ruitk::FC(&BoundaryHostComp));
	H.Pump(4); // failure -> restart -> boundary renders fallback
	TestEqual(TEXT("boundary shows fallback"), H.TextAt(0), FString(TEXT("fallback")));
	TestTrue(TEXT("OnError received the reason"), GCaught->Contains(TEXT("boom")));

	AddInfo(TEXT("[boundary] reset-key change recovers"));
	*GShouldFail = false;
	CoreTest2::IntSetter(1); // new reset key -> boundary clears and re-renders children
	H.Pump(4);
	TestEqual(TEXT("children after reset"), H.TextAt(0), FString(TEXT("fine")));
	GShouldFail.Reset();
	GCaught.Reset();
	return true;
}

// â”€â”€ Fragment flattening order
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static FRuitkNodeArray FragmentComp(FRuitkContext&, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	return {RuitkTest::Box(RuitkTest::BoxProps(TEXT("list")),
						   {
							   Ruitk::TextBlock(TEXT("a")),
							   Ruitk::Fragment({Ruitk::TextBlock(TEXT("b")), Ruitk::TextBlock(TEXT("c"))}),
							   Ruitk::TextBlock(TEXT("d")),
						   })};
}
RUITK_COMPONENT(FragmentComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkCoreFragmentTest, "Ruitk.Core.Fragment", RUITK_TEST_FLAGS)
bool FRuitkCoreFragmentTest::RunTest(const FString&)
{
	FRuitkTestHarness H;
	H.Mount(Ruitk::FC(&FragmentComp));
	FMockNode* List = H.ChildAt(0);
	if (!TestEqual(TEXT("fragment flattens to 4 children"), List->Children.Num(), 4))
	{
		return false;
	}
	TArray<FString> Texts;
	for (const TSharedPtr<FMockNode>& C : List->Children)
	{
		Texts.Add(C->TextOf());
	}
	TestEqual(TEXT("order a,b,c,d"), FString::Join(Texts, TEXT(",")), FString(TEXT("a,b,c,d")));
	return true;
}

// â”€â”€ GO-09 reuse_by_slot: full key churn, zero node churn
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static FRuitkNodeArray ReuseBySlotComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [Frame, SetFrame] = Ctx.UseState<int32>(0);
	CoreTest2::IntSetter = SetFrame;
	TArray<FRuitkNode> Items;
	for (int32 i = 0; i < 5; ++i)
	{
		// EVERY key changes every render â€” the keyed path would delete+recreate all 5.
		Items.Add(RuitkTest::Box(RuitkTest::BoxProps(FString::Printf(TEXT("v%d"), i + Frame), i + Frame), {},
								 FRuitkKey(FString::Printf(TEXT("k%d_%d"), i, Frame))));
	}
	FTestBoxProps ContainerProps = RuitkTest::BoxProps(TEXT("container"));
	ContainerProps.bReuseBySlot = true;
	return {RuitkTest::Box(MoveTemp(ContainerProps), MoveTemp(Items))};
}
RUITK_COMPONENT(ReuseBySlotComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkCoreReuseBySlotTest, "Ruitk.Core.ReuseBySlot", RUITK_TEST_FLAGS)
bool FRuitkCoreReuseBySlotTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	FRuitkTestHarness H;
	H.Mount(Ruitk::FC(&ReuseBySlotComp));
	FMockNode* Container = H.ChildAt(0);
	if (!TestEqual(TEXT("5 slots"), Container->Children.Num(), 5))
	{
		return false;
	}
	FMockNode* N0 = Container->Children[0].Get();
	FMockNode* N4 = Container->Children[4].Get();

	FRuitkDiagnostics::bEnabled = true;
	FRuitkDiagnostics::Reset();
	CoreTest2::IntSetter(1);
	H.Pump();
	TestTrue(TEXT("nodes REUSED across full key change"),
			 Container->Children[0].Get() == N0 && Container->Children[4].Get() == N4);
	TestTrue(TEXT("ZERO mount/unmount churn"), FRuitkDiagnostics::Placements == 0 && FRuitkDiagnostics::Deletions == 0);
	TestEqual(TEXT("reused node's props updated in place"), N0->PropsAs<FTestBoxProps>()->Value, 1);
	FRuitkDiagnostics::bEnabled = false;
	return true;
}

// â”€â”€ Time slicing: budget 0 parks per unit; the sliced update still completes
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static FRuitkNodeArray SlicedComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [Frame, SetFrame] = Ctx.UseState<int32>(0);
	CoreTest2::IntSetter = SetFrame;
	TArray<FRuitkNode> Items;
	for (int32 i = 0; i < 8; ++i)
	{
		Items.Add(RuitkTest::Box(RuitkTest::BoxProps(FString::Printf(TEXT("item %d-%d"), i, Frame)), {}, FRuitkKey(i)));
	}
	return {RuitkTest::Box(RuitkTest::BoxProps(TEXT("list")), MoveTemp(Items))};
}
RUITK_COMPONENT(SlicedComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkCoreTimeSlicingTest, "Ruitk.Core.TimeSlicing", RUITK_TEST_FLAGS)
bool FRuitkCoreTimeSlicingTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	IConsoleVariable* Slicing = IConsoleManager::Get().FindConsoleVariable(TEXT("ruitk.TimeSlicing"));
	IConsoleVariable* Quantum = IConsoleManager::Get().FindConsoleVariable(TEXT("ruitk.TimeSliceMs"));
	const bool bPrevSlicing = Slicing->GetBool();
	const float PrevQuantum = Quantum->GetFloat();
	Slicing->Set(true);
	Quantum->Set(0.0f); // quantum exhausts after every unit — maximum slicing (M3 two-axis model)

	{
		FRuitkTestHarness H;
		H.Mount(Ruitk::FC(&SlicedComp)); // mount is always synchronous
		FMockNode* List = H.ChildAt(0);
		TestEqual(TEXT("sliced: initial 8 items"), List->Children.Num(), 8);
		TestEqual(TEXT("sliced: initial label"), List->Children[0]->PropsAs<FTestBoxProps>()->Label,
				  FString(TEXT("item 0-0")));

		CoreTest2::IntSetter(1);
		TestTrue(TEXT("sliced update completes"), H.Host.PumpUntilIdle(200));
		TestEqual(TEXT("first item updated"), List->Children[0]->PropsAs<FTestBoxProps>()->Label,
				  FString(TEXT("item 0-1")));
		TestEqual(TEXT("last item updated"), List->Children[7]->PropsAs<FTestBoxProps>()->Label,
				  FString(TEXT("item 7-1")));
	}

	Slicing->Set(bPrevSlicing);
	Quantum->Set(PrevQuantum);
	return true;
}

// -- FlushSync under slicing: "synchronously and unsliced" must be literal -- with slicing on
//    and a zero quantum the update parks as a queued/sliced Slice action, but ONE FlushSync
//    call claims it and commits everything (P-06/M1, extended by M3 to drain the queued
//    Slice; every mount surface + HMR + the item-model row roots depend on this)
//    -------------

static FRuitkNodeArray FlushSyncWideComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [Frame, SetFrame] = Ctx.UseState<int32>(0);
	CoreTest2::IntSetter = SetFrame;
	const int32 F = Frame;
	Ctx.UseEffect([F]() { CoreTest2::SeenInt = F; }, Ruitk::Deps(F));
	TArray<FRuitkNode> Items;
	for (int32 i = 0; i < 32; ++i)
	{
		Items.Add(RuitkTest::Box(RuitkTest::BoxProps(FString::Printf(TEXT("w %d-%d"), i, Frame)), {}, FRuitkKey(i)));
	}
	return {RuitkTest::Box(RuitkTest::BoxProps(TEXT("wide")), MoveTemp(Items))};
}
RUITK_COMPONENT(FlushSyncWideComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkCoreFlushSyncUnderSlicingTest, "Ruitk.Core.FlushSyncUnderSlicing",
								 RUITK_TEST_FLAGS)
bool FRuitkCoreFlushSyncUnderSlicingTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	CoreTest2::SeenInt = -1; // effect writes 0 on mount -- must be distinguishable
	IConsoleVariable* Slicing = IConsoleManager::Get().FindConsoleVariable(TEXT("ruitk.TimeSlicing"));
	IConsoleVariable* Quantum = IConsoleManager::Get().FindConsoleVariable(TEXT("ruitk.TimeSliceMs"));
	const bool bPrevSlicing = Slicing->GetBool();
	const float PrevQuantum = Quantum->GetFloat();
	Slicing->Set(true);
	Quantum->Set(0.0f); // parks after every unit -- a plain sliced pump cannot finish in one quantum

	{
		FRuitkTestHarness H;
		H.Mount(Ruitk::FC(&FlushSyncWideComp)); // mount is always synchronous
		FMockNode* Wide = H.ChildAt(0);
		if (TestNotNull(TEXT("mounted container"), Wide))
		{
			TestEqual(TEXT("32 leaves mounted"), Wide->Children.Num(), 32);
			TestEqual(TEXT("mount effect flushed"), CoreTest2::SeenInt, 0);

			CoreTest2::IntSetter(1);   // dirty -> queues a Slice action on the scheduler
			H.Reconciler->FlushSync(); // ONE call: must claim the queued Slice and commit, unsliced

			TestEqual(TEXT("first leaf committed by FlushSync"), Wide->Children[0]->PropsAs<FTestBoxProps>()->Label,
					  FString(TEXT("w 0-1")));
			TestEqual(TEXT("last leaf committed by FlushSync"), Wide->Children[31]->PropsAs<FTestBoxProps>()->Label,
					  FString(TEXT("w 31-1")));
			TestEqual(TEXT("passive effects flushed by FlushSync"), CoreTest2::SeenInt, 1);
			TestTrue(TEXT("still mounted"), H.Reconciler->IsMounted());
			TestTrue(TEXT("ruitk.TimeSlicing still reads true after FlushSync"), FRuitkConfig::IsTimeSlicing());

			// No parked WIP survives the call: draining the host queue afterwards changes nothing.
			TestTrue(TEXT("host queue drains"), H.Host.PumpUntilIdle(200));
			TestEqual(TEXT("no further commit was needed"), Wide->Children[31]->PropsAs<FTestBoxProps>()->Label,
					  FString(TEXT("w 31-1")));
		}
	}

	Slicing->Set(bPrevSlicing);
	Quantum->Set(PrevQuantum);
	return true;
}

// â”€â”€ Refs: attach on placement, null on removal (React lifecycle, D-08.4)
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static TSharedPtr<FRuitkHostHandle> GCapturedRef;

static FRuitkNodeArray RefComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [bShow, SetShow] = Ctx.UseState<bool>(true);
	CoreTest2::Dispatch = [SetShow](FString Cmd) { SetShow(Cmd == TEXT("show")); };
	if (bShow)
	{
		FTestBoxProps P = RuitkTest::BoxProps(TEXT("target"));
		P.Ref = [](const FRuitkHostHandle& H)
		{
			if (GCapturedRef.IsValid())
			{
				*GCapturedRef = H;
			}
		};
		return {RuitkTest::Box(MoveTemp(P))};
	}
	return {Ruitk::TextBlock(TEXT("gone"))};
}
RUITK_COMPONENT(RefComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkCoreRefTest, "Ruitk.Core.RefLifecycle", RUITK_TEST_FLAGS)
bool FRuitkCoreRefTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	GCapturedRef = MakeShared<FRuitkHostHandle>();
	FRuitkTestHarness H;
	H.Mount(Ruitk::FC(&RefComp));
	TestTrue(TEXT("ref populated while mounted"), GCapturedRef->IsValid());

	CoreTest2::Dispatch(TEXT("hide"));
	H.Pump();
	TestFalse(TEXT("ref nulled when node removed"), GCapturedRef->IsValid());
	GCapturedRef.Reset();
	return true;
}

// â”€â”€ Deferred value: lags one render, then catches up
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static FRuitkNodeArray DeferredComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [Now, SetNow] = Ctx.UseState<int32>(0);
	CoreTest2::IntSetter = SetNow;
	const int32 Deferred = Ctx.UseDeferredValue<int32>(Now);
	CoreTest2::SeenInt = Deferred;
	CoreTest2::Seen = FString::Printf(TEXT("%d/%d"), Now, Deferred);
	return {Ruitk::TextBlock(CoreTest2::Seen)};
}
RUITK_COMPONENT(DeferredComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkCoreDeferredTest, "Ruitk.Core.DeferredValue", RUITK_TEST_FLAGS)
bool FRuitkCoreDeferredTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	FRuitkTestHarness H;
	H.Mount(Ruitk::FC(&DeferredComp));
	TestEqual(TEXT("initial 0/0"), CoreTest2::Seen, FString(TEXT("0/0")));

	CoreTest2::IntSetter(5);
	H.Host.PumpFrame(); // one frame: urgent renders with stale deferred
	TestEqual(TEXT("urgent updates, deferred lags (5/0)"), CoreTest2::Seen, FString(TEXT("5/0")));

	TestTrue(TEXT("deferred settles"), H.Host.PumpUntilIdle(16));
	TestEqual(TEXT("deferred catches up"), CoreTest2::SeenInt, 5);
	return true;
}

// â”€â”€ SafeArea comes from the host (mock returns 1,2,3,4)
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static FRuitkNodeArray SafeAreaComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	FRuitkSafeArea SA = Ctx.UseSafeArea();
	CoreTest2::Seen = FString::Printf(TEXT("%.0f,%.0f,%.0f,%.0f"), SA.Left, SA.Top, SA.Right, SA.Bottom);
	return {Ruitk::TextBlock(CoreTest2::Seen)};
}
RUITK_COMPONENT(SafeAreaComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkCoreSafeAreaTest, "Ruitk.Core.SafeArea", RUITK_TEST_FLAGS)
bool FRuitkCoreSafeAreaTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	FRuitkTestHarness H;
	H.Mount(Ruitk::FC(&SafeAreaComp));
	TestEqual(TEXT("host-supplied safe area"), CoreTest2::Seen, FString(TEXT("1,2,3,4")));
	return true;
}

// â”€â”€ Diagnostics counters
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static FRuitkNodeArray DiagComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [V, SetV] = Ctx.UseState<int32>(0);
	CoreTest2::IntSetter = SetV;
	return {Ruitk::TextBlock(FString::FromInt(V))};
}
RUITK_COMPONENT(DiagComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkCoreDiagnosticsTest, "Ruitk.Core.Diagnostics", RUITK_TEST_FLAGS)
bool FRuitkCoreDiagnosticsTest::RunTest(const FString&)
{
	CoreTest2::Reset();
	FRuitkDiagnostics::bEnabled = true;
	FRuitkDiagnostics::Reset();
	FRuitkTestHarness H;
	H.Mount(Ruitk::FC(&DiagComp));
	TestTrue(TEXT("counted initial render"), FRuitkDiagnostics::Renders >= 1);
	TestTrue(TEXT("counted placements"), FRuitkDiagnostics::Placements >= 1);
	const int32 R0 = FRuitkDiagnostics::Renders;
	CoreTest2::IntSetter(1);
	H.Pump();
	TestTrue(TEXT("counted update render"), FRuitkDiagnostics::Renders > R0);
	TestTrue(TEXT("counted prop update"), FRuitkDiagnostics::Updates >= 1);
	FRuitkDiagnostics::bEnabled = false;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkFmtTest, "Ruitk.Core.Fmt", RUITK_TEST_FLAGS)
bool FRuitkFmtTest::RunTest(const FString&)
{
	// {}-placeholder interpolation, type-generic + ordered (the .uetkx binding sugar).
	TestEqual(TEXT("int fill"), Ruitk::Fmt(TEXT("Count: {}"), 7).ToString(), FString(TEXT("Count: 7")));
	TestEqual(TEXT("multi + types"), Ruitk::Fmt(TEXT("{} of {} ({})"), 2, 3, true).ToString(),
			  FString(TEXT("2 of 3 (true)")));
	TestEqual(TEXT("string + text args"),
			  Ruitk::Fmt(TEXT("{}-{}"), FString(TEXT("a")), FText::FromString(TEXT("b"))).ToString(),
			  FString(TEXT("a-b")));
	TestEqual(TEXT("escaped brace"), Ruitk::Fmt(TEXT("{{}} {}"), 9).ToString(), FString(TEXT("{} 9")));
	TestEqual(TEXT("no args, no placeholders"), Ruitk::Fmt(TEXT("plain")).ToString(), FString(TEXT("plain")));
	TestEqual(TEXT("float"), Ruitk::Fmt(TEXT("{}"), 1.5f).ToString(), FString(TEXT("1.5")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
