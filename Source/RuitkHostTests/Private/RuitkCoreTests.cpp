// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Port of tests/core_test.gd + update_test.gd against the mock host. Sections print via
// AddInfo so hangs name their culprit (house rule). Godot tests that exercise HOST features
// (react events on real controls, item models, classes/stylesheets, custom draw, node pool
// internals, tree/root-node) port with the Slate host in Phases 8/9 suites; the router suite
// is post-v1 (TD-001). C++-semantics adaptations are commented inline (§5 deviations).

#include "Misc/AutomationTest.h"
#include "RuitkMockHost.h"
#include "RuitkContext.h"
#include "RuitkSignal.h"
#include "RuitkCoreElements.h"

#if WITH_DEV_AUTOMATION_TESTS

#define RUITK_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// Shared test-component plumbing: statics reset per test (components must be free functions
// for registry identity — D-05).
namespace CoreTestState
{
	static int32 RenderCountA = 0;
	static int32 RenderCountB = 0;
	static int32 RenderCountC = 0;
	static TRuitkSetter<int32> SetterA;
	static TRuitkSetter<int32> SetterB;
	static TArray<FString> Log;

	static void ResetAll()
	{
		RenderCountA = RenderCountB = RenderCountC = 0;
		SetterA = TRuitkSetter<int32>();
		SetterB = TRuitkSetter<int32>();
		Log.Reset();
	}
} // namespace CoreTestState

// ─────────────────────────────────────────────────────────────────────────────────────────
// Update path (update_test.gd)
// ─────────────────────────────────────────────────────────────────────────────────────────

static FRuitkNodeArray UpdateTestComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [V, SetV] = Ctx.UseState<int32>(0);
	++CoreTestState::RenderCountA;
	CoreTestState::SetterA = SetV;
	return {Ruitk::TextBlock(FString::Printf(TEXT("v=%d"), V))};
}
RUITK_COMPONENT(UpdateTestComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkCoreUpdateTest, "Ruitk.Update.CoalescedDiff", RUITK_TEST_FLAGS)
bool FRuitkCoreUpdateTest::RunTest(const FString&)
{
	using namespace CoreTestState;
	ResetAll();
	FRuitkTestHarness H;

	AddInfo(TEXT("[update] 1/3 initial render"));
	H.Mount(Ruitk::FC(&UpdateTestComp));
	TestEqual(TEXT("initial render count == 1"), RenderCountA, 1);
	FMockNode* Label = H.ChildAt(0);
	if (!TestNotNull(TEXT("label mounted"), Label))
	{
		return false;
	}
	TestEqual(TEXT("initial text"), Label->TextOf(), FString(TEXT("v=0")));

	AddInfo(TEXT("[update] 2/3 setState -> one re-render, node REUSED"));
	SetterA(5);
	H.Pump();
	TestEqual(TEXT("one coalesced re-render"), RenderCountA, 2);
	TestTrue(TEXT("host node REUSED (diff, not recreate)"), H.ChildAt(0) == Label);
	TestEqual(TEXT("updated text"), Label->TextOf(), FString(TEXT("v=5")));

	AddInfo(TEXT("[update] 3/3 two sets in one frame coalesce"));
	SetterA(10);
	SetterA(11);
	H.Pump();
	TestEqual(TEXT("two sets coalesce to one render"), RenderCountA, 3);
	TestEqual(TEXT("final text"), Label->TextOf(), FString(TEXT("v=11")));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Effects (deps + cleanup ordering + unmount)
// ─────────────────────────────────────────────────────────────────────────────────────────

static FRuitkNodeArray EffectsComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [Count, SetCount] = Ctx.UseState<int32>(0);
	auto [Other, SetOther] = Ctx.UseState<int32>(0);
	CoreTestState::SetterA = SetCount;
	CoreTestState::SetterB = SetOther;
	const int32 Cur = Count;
	Ctx.UseEffect(
		[Cur]() -> FRuitkEffectCleanup
		{
			CoreTestState::Log.Add(FString::Printf(TEXT("setup:%d"), Cur));
			return [Cur]() { CoreTestState::Log.Add(FString::Printf(TEXT("cleanup:%d"), Cur)); };
		},
		Ruitk::Deps(Count));
	return {Ruitk::TextBlock(TEXT("x"))};
}
RUITK_COMPONENT(EffectsComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkCoreEffectsTest, "Ruitk.Core.Effects", RUITK_TEST_FLAGS)
bool FRuitkCoreEffectsTest::RunTest(const FString&)
{
	using namespace CoreTestState;
	ResetAll();
	{
		FRuitkTestHarness H;
		AddInfo(TEXT("[effects] mount"));
		H.Mount(Ruitk::FC(&EffectsComp));
		TestEqual(TEXT("effect runs on mount"), FString::Join(Log, TEXT(",")), FString(TEXT("setup:0")));

		AddInfo(TEXT("[effects] dep change -> cleanup then setup"));
		SetterA(1);
		H.Pump();
		TestEqual(TEXT("cleanup->setup on dep change"), FString::Join(Log, TEXT(",")),
				  FString(TEXT("setup:0,cleanup:0,setup:1")));

		AddInfo(TEXT("[effects] unrelated state -> effect skipped"));
		SetterB(99);
		H.Pump();
		TestEqual(TEXT("effect skipped when deps unchanged"), FString::Join(Log, TEXT(",")),
				  FString(TEXT("setup:0,cleanup:0,setup:1")));

		AddInfo(TEXT("[effects] unmount runs cleanup"));
		H.Reconciler->Unmount();
	}
	TestEqual(TEXT("cleanup on unmount"), FString::Join(Log, TEXT(",")),
			  FString(TEXT("setup:0,cleanup:0,setup:1,cleanup:1")));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Bailout + SUBTREE-SKIP (D-08.1 — the adopted React mechanism, asserted directly)
// ─────────────────────────────────────────────────────────────────────────────────────────

struct FLabelProps final : public FRuitkPropsBase
{
	RUITK_PROP(FString, LabelText, 0)
	RUITK_PROPS_BODY(FLabelProps, RUITK_EQ(LabelText))
};

static FRuitkNodeArray BailChildComp(FRuitkContext&, const FLabelProps& Props, const TArray<FRuitkNode>&)
{
	++CoreTestState::RenderCountB;
	return {Ruitk::TextBlock(Props.LabelText)};
}
RUITK_COMPONENT(BailChildComp)

static FRuitkNodeArray BailParentComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	++CoreTestState::RenderCountA;
	auto [S, SetS] = Ctx.UseState<int32>(0);
	CoreTestState::SetterA = SetS;
	FLabelProps ChildProps;
	ChildProps.SetLabelText(TEXT("static"));
	return {
		Ruitk::TextBlock(FString::Printf(TEXT("count %d"), S)),
		Ruitk::FC(&BailChildComp, MoveTemp(ChildProps)),
	};
}
RUITK_COMPONENT(BailParentComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkCoreBailoutTest, "Ruitk.Core.Bailout", RUITK_TEST_FLAGS)
bool FRuitkCoreBailoutTest::RunTest(const FString&)
{
	using namespace CoreTestState;
	ResetAll();
	FRuitkTestHarness H;
	H.Mount(Ruitk::FC(&BailParentComp));
	TestEqual(TEXT("initial parent renders"), RenderCountA, 1);
	TestEqual(TEXT("initial child renders"), RenderCountB, 1);

	SetterA(1);
	H.Pump();
	TestEqual(TEXT("parent re-rendered"), RenderCountA, 2);
	TestEqual(TEXT("child BAILED (equal props)"), RenderCountB, 1);
	return true;
}

// A → B(static passthrough) → C(leaf): bumping A re-renders A; B bails on Equals (fresh but
// equal props); B's cached output hands C IDENTICAL vnodes -> C takes the SUBTREE-SKIP path.
static FRuitkNodeArray SkipLeafComp(FRuitkContext&, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	++CoreTestState::RenderCountC;
	return {Ruitk::TextBlock(TEXT("leaf"))};
}
RUITK_COMPONENT(SkipLeafComp)

static FRuitkNodeArray SkipMidComp(FRuitkContext&, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	++CoreTestState::RenderCountB;
	return {Ruitk::FC(&SkipLeafComp)};
}
RUITK_COMPONENT(SkipMidComp)

static FRuitkNodeArray SkipTopComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	++CoreTestState::RenderCountA;
	auto [S, SetS] = Ctx.UseState<int32>(0);
	CoreTestState::SetterA = SetS;
	return {
		Ruitk::TextBlock(FString::Printf(TEXT("top %d"), S)),
		Ruitk::FC(&SkipMidComp),
	};
}
RUITK_COMPONENT(SkipTopComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkCoreSubtreeSkipTest, "Ruitk.Core.SubtreeSkip", RUITK_TEST_FLAGS)
bool FRuitkCoreSubtreeSkipTest::RunTest(const FString&)
{
	using namespace CoreTestState;
	ResetAll();
	FRuitkTestHarness H;
	H.Mount(Ruitk::FC(&SkipTopComp));
	TestEqual(TEXT("mount: top"), RenderCountA, 1);
	TestEqual(TEXT("mount: mid"), RenderCountB, 1);
	TestEqual(TEXT("mount: leaf"), RenderCountC, 1);
	FMockNode* LeafText = H.ChildAt(1); // [topText, leafText] flattened under root

	SetterA(1);
	H.Pump();
	TestEqual(TEXT("top re-rendered"), RenderCountA, 2);
	TestEqual(TEXT("mid bailed"), RenderCountB, 1);
	TestEqual(TEXT("leaf skipped entirely (subtree-skip)"), RenderCountC, 1);
	TestTrue(TEXT("leaf host node untouched"), H.ChildAt(1) == LeafText && LeafText->UpdateCount == 0);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Keyed reorder + identity + removal
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace CoreTestState
{
	static TRuitkSetter<TArray<FString>> KeySetter;
}

static FRuitkNodeArray KeyedComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [Ids, SetIds] = Ctx.UseState<TArray<FString>>(TArray<FString>{TEXT("a"), TEXT("b"), TEXT("c")});
	CoreTestState::KeySetter = SetIds;
	TArray<FRuitkNode> Items;
	for (const FString& Id : Ids)
	{
		Items.Add(RuitkTest::Box(RuitkTest::BoxProps(Id), {}, FRuitkKey(Id)));
	}
	return {RuitkTest::Box(RuitkTest::BoxProps(TEXT("list")), MoveTemp(Items))};
}
RUITK_COMPONENT(KeyedComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkCoreKeyedTest, "Ruitk.Core.KeyedReorder", RUITK_TEST_FLAGS)
bool FRuitkCoreKeyedTest::RunTest(const FString&)
{
	using namespace CoreTestState;
	ResetAll();
	FRuitkTestHarness H;
	H.Mount(Ruitk::FC(&KeyedComp));
	FMockNode* List = H.ChildAt(0);
	if (!TestNotNull(TEXT("list mounted"), List) || !TestEqual(TEXT("3 children"), List->Children.Num(), 3))
	{
		return false;
	}
	FMockNode* A = List->Children[0].Get();
	FMockNode* B = List->Children[1].Get();
	FMockNode* C = List->Children[2].Get();

	AddInfo(TEXT("[keyed] reorder c,a,b preserves identity"));
	KeySetter(TArray<FString>{TEXT("c"), TEXT("a"), TEXT("b")});
	H.Pump();
	TestTrue(TEXT("c first"), List->Children[0].Get() == C);
	TestTrue(TEXT("a second"), List->Children[1].Get() == A);
	TestTrue(TEXT("b third"), List->Children[2].Get() == B);

	AddInfo(TEXT("[keyed] removal keeps survivors' identity"));
	KeySetter(TArray<FString>{TEXT("c"), TEXT("b")});
	H.Pump();
	TestEqual(TEXT("2 children after removal"), List->Children.Num(), 2);
	TestTrue(TEXT("c,b remain with identity"), List->Children[0].Get() == C && List->Children[1].Get() == B);
	TestTrue(TEXT("a released"), A->bReleased);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Context: typed handles, defaults, distinct identity, change propagation, survives bailout
// ─────────────────────────────────────────────────────────────────────────────────────────

static TRuitkContext<FString> GThemeCtx(FString(TEXT("fallback")), FName(TEXT("Theme")));

namespace CoreTestState
{
	static FString SeenContext;
	static TRuitkSetter<FString> ThemeSetter;
} // namespace CoreTestState

static FRuitkNodeArray CtxConsumerComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	++CoreTestState::RenderCountB;
	auto [S, SetS] = Ctx.UseState<int32>(0); // gives the test a handle to force re-render
	CoreTestState::SetterB = SetS;
	CoreTestState::SeenContext = Ctx.UseContext(GThemeCtx);
	return {Ruitk::TextBlock(CoreTestState::SeenContext)};
}
RUITK_COMPONENT(CtxConsumerComp)

static FRuitkNodeArray CtxProviderComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [Theme, SetTheme] = Ctx.UseState<FString>(FString(TEXT("dark")));
	CoreTestState::ThemeSetter = SetTheme;
	Ctx.ProvideContext(GThemeCtx, Theme);
	return {Ruitk::FC(&CtxConsumerComp)};
}
RUITK_COMPONENT(CtxProviderComp)

static FRuitkNodeArray CtxGrandparentComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [S, SetS] = Ctx.UseState<int32>(0);
	CoreTestState::SetterA = SetS;
	return {
		Ruitk::TextBlock(FString::Printf(TEXT("gp %d"), S)),
		Ruitk::FC(&CtxProviderComp),
	};
}
RUITK_COMPONENT(CtxGrandparentComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkCoreContextTest, "Ruitk.Core.Context", RUITK_TEST_FLAGS)
bool FRuitkCoreContextTest::RunTest(const FString&)
{
	using namespace CoreTestState;
	ResetAll();
	SeenContext.Empty();
	{
		AddInfo(TEXT("[context] provider -> consumer + change propagation through bailouts"));
		FRuitkTestHarness H;
		H.Mount(Ruitk::FC(&CtxProviderComp));
		TestEqual(TEXT("consumer sees provided value"), SeenContext, FString(TEXT("dark")));
		TestEqual(TEXT("consumer rendered once"), RenderCountB, 1);

		ThemeSetter(FString(TEXT("light")));
		H.Pump();
		TestEqual(TEXT("consumer sees updated value"), SeenContext, FString(TEXT("light")));
		TestEqual(TEXT("consumer re-rendered on context change"), RenderCountB, 2);
	}
	{
		AddInfo(TEXT("[context] no provider -> handle default"));
		ResetAll();
		SeenContext.Empty();
		FRuitkTestHarness H;
		H.Mount(Ruitk::FC(&CtxConsumerComp));
		TestEqual(TEXT("unprovided handle returns default"), SeenContext, FString(TEXT("fallback")));
	}
	{
		AddInfo(TEXT("[context] distinct handles have distinct identity"));
		TRuitkContext<int32> A(1);
		TRuitkContext<int32> B(1);
		TestTrue(TEXT("identity differs despite equal defaults"), A.Key() != B.Key());
	}
	{
		AddInfo(TEXT("[context] survives provider bailout (carried provided map pushes on descend)"));
		ResetAll();
		SeenContext.Empty();
		FRuitkTestHarness H;
		H.Mount(Ruitk::FC(&CtxGrandparentComp));
		TestEqual(TEXT("initial context"), SeenContext, FString(TEXT("dark")));
		SetterA(1); // grandparent re-renders; provider bails (fresh-but-equal props)
		H.Pump();
		SetterB(1); // force the consumer to re-render and re-read
		H.Pump();
		TestEqual(TEXT("context SURVIVES provider bailout"), SeenContext, FString(TEXT("dark")));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
