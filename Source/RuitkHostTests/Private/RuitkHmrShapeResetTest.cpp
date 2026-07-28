// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Ruitk.Hooks.HmrShapeReset — TB-13: the family HMR rule "state preserved on a stable
// hook shape, RESET on a real shape change". v1's interpreter enforced it via its AST
// signature; v2 (Live-Coding patches) detects it at render time: while the HMR session is
// tracking, a component whose FLATTENED hook sequence differs across a generation boundary
// disposes its cells and re-renders clean — instead of positional reads silently serving a
// neighbor's value. A shape change WITHOUT a generation bump stays a rules-of-hooks user
// error (ruitk.HookValidation) and must NOT reset.

#include "Misc/AutomationTest.h"
#include "RuitkContext.h"
#include "RuitkNode.h"
#include "RuitkReconciler.h"
#include "RuitkRoot.h"
#include "RuitkSlateElements.h"
#include "RuitkSlateHost.h"

#if WITH_DEV_AUTOMATION_TESTS

#define RUITK_HMRSHAPE_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace HmrShapeTest
{
	static int32 Mode = 0; // 0: [State, State]  1: [Memo, State, State]  (the DoomFace-ish shapes)
	static int32 SeenA = -1;
	static int32 SeenB = -1;
	static TFunction<void(int32)> SetAOut;
	static TFunction<void(int32)> SetBOut;

	static FRuitkNodeArray ShapeComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
	{
		if (Mode == 1)
		{
			// the added hook BEFORE the states — the classic ordinal-shift shape
			Ctx.UseMemo<int32>([]() { return 42; }, Ruitk::Deps());
		}
		auto [A, SetA] = Ctx.UseState<int32>(1);
		auto [B, SetB] = Ctx.UseState<int32>(2);
		SeenA = A;
		SeenB = B;
		SetAOut = [SetA](int32 V) { SetA(V); };
		SetBOut = [SetB](int32 V) { SetB(V); };
		return {Ruitk::TextBlock(FString::Printf(TEXT("%d/%d"), A, B))};
	}
	RUITK_COMPONENT(ShapeComp)
} // namespace HmrShapeTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkHmrShapeResetTest, "Ruitk.Hooks.HmrShapeReset", RUITK_HMRSHAPE_TEST_FLAGS)
bool FRuitkHmrShapeResetTest::RunTest(const FString&)
{
	using namespace HmrShapeTest;
	// Case 3 DELIBERATELY provokes the rules-of-hooks path — its [Hooks][order] error logs
	// are the expected diagnostic (validation is on in editor test runs), not a failure.
	AddExpectedError(TEXT("[Hooks][order]"), EAutomationExpectedErrorFlags::Contains, 0);
	Mode = 0;
	Ruitk::SetHmrHookTracking(true);

	TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(Ruitk::FC(&ShapeComp));
	TestEqual(TEXT("defaults at mount"), SeenA, 1);
	SetAOut(5);
	SetBOut(9);
	Root->FlushSync();
	TestEqual(TEXT("A driven"), SeenA, 5);
	TestEqual(TEXT("B driven"), SeenB, 9);

	// 1) cosmetic patch (same hook shape) across a generation boundary — state SURVIVES
	Ruitk::BumpHmrGeneration();
	FRuitkReconciler::ForEachLive([](FRuitkReconciler& R) { R.HmrRefreshAll(); });
	Root->FlushSync();
	TestEqual(TEXT("stable shape: A preserved across the patch"), SeenA, 5);
	TestEqual(TEXT("stable shape: B preserved across the patch"), SeenB, 9);

	// 2) the hook LIST changes across a generation boundary — clean RESET (the family rule),
	//    never a neighbor's value.
	Mode = 1;
	Ruitk::BumpHmrGeneration();
	FRuitkReconciler::ForEachLive([](FRuitkReconciler& R) { R.HmrRefreshAll(); });
	Root->FlushSync(); // detection render (reads misaligned cells) + schedules the reset render
	Root->FlushSync(); // the clean re-render
	TestEqual(TEXT("shape change: A reset to its default"), SeenA, 1);
	TestEqual(TEXT("shape change: B reset to its default"), SeenB, 2);

	// 3) drive again, then change shape back WITHOUT a generation bump: a rules-of-hooks
	//    violation, not an edit. The MEMORY-SAFETY tail truncation still rebuilds the
	//    mismatched cells (values reset — the only safe option; the 2026-07-24 crash was
	//    this exact cast served unchecked), but it is reported as the [Hooks][order] user
	//    error, never the HMR reset line. Behavior we pin: no crash, defaults, no dangling.
	SetAOut(7);
	Root->FlushSync();
	TestEqual(TEXT("re-driven under the new shape"), SeenA, 7);
	Mode = 0;
	FRuitkReconciler::ForEachLive([](FRuitkReconciler& R) { R.HmrRefreshAll(); });
	Root->FlushSync();
	Root->FlushSync();
	TestEqual(TEXT("rules-of-hooks shape break: cells rebuilt safely (no type confusion)"), SeenA, 1);

	Ruitk::SetHmrHookTracking(false);
	SetAOut = nullptr;
	SetBOut = nullptr;
	return true;
}

// ── TB-17: "preserve state, recompute derivations" across a patch boundary ────────────────
// A UseMemo with mount-only deps caches forever by design — but after a Live Coding patch
// the FACTORY CODE may have changed, so the first post-patch render must re-run it (the
// owner's matrix-8 find: editing the memo'd title string patched successfully and showed
// the stale cached string). State cells must still survive the same refresh.

namespace HmrMemoTest
{
	static int32 FactorySource = 1; // stands in for "the code the patch replaced"
	static int32 SeenMemo = -1;
	static int32 SeenState = -1;
	static TFunction<void(int32)> SetStateOut;

	static FRuitkNodeArray MemoComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
	{
		const int32 Memoized = Ctx.UseMemo<int32>([]() { return FactorySource; }, Ruitk::Deps());
		auto [S, SetS] = Ctx.UseState<int32>(0);
		SeenMemo = Memoized;
		SeenState = S;
		SetStateOut = [SetS](int32 V) { SetS(V); };
		return {Ruitk::TextBlock(FString::Printf(TEXT("%d/%d"), Memoized, S))};
	}
	RUITK_COMPONENT(MemoComp)
} // namespace HmrMemoTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkHmrMemoInvalidateTest, "Ruitk.Hooks.HmrMemoInvalidate",
								 RUITK_HMRSHAPE_TEST_FLAGS)
bool FRuitkHmrMemoInvalidateTest::RunTest(const FString&)
{
	using namespace HmrMemoTest;
	FactorySource = 1;
	Ruitk::SetHmrHookTracking(true);

	TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(Ruitk::FC(&MemoComp));
	TestEqual(TEXT("memo computed at mount"), SeenMemo, 1);
	SetStateOut(5);
	Root->FlushSync();

	// the "patch": the factory's source changes; WITHOUT a generation boundary the cache
	// legitimately holds (mount-only deps — plain React semantics)
	FactorySource = 2;
	SetStateOut(6);
	Root->FlushSync();
	TestEqual(TEXT("no boundary: memo cache holds (mount-only deps)"), SeenMemo, 1);

	// across the boundary the derivation recomputes; the STATE survives
	Ruitk::BumpHmrGeneration();
	FRuitkReconciler::ForEachLive([](FRuitkReconciler& R) { R.HmrRefreshAll(); });
	Root->FlushSync();
	TestEqual(TEXT("post-patch: memo recomputed from the new code"), SeenMemo, 2);
	TestEqual(TEXT("post-patch: state preserved"), SeenState, 6);

	Ruitk::SetHmrHookTracking(false);
	SetStateOut = nullptr;
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
