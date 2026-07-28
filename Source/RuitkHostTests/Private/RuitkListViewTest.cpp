// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Ruitk.Widgets.ListView — TD-022 item-model views: the virtualized SListView/STileView with
// per-row FRuitkRoot sub-roots. Rows generate only under an arranged geometry, so the test drives
// generation deterministically via SRuitkListView::ForceGenerateRows, then proves the reactive path:
// re-handing a fresh RenderItem closure re-runs it against every live row's sub-root (no churn).

#include "Misc/AutomationTest.h"
#include "Framework/Application/SlateApplication.h"
#include "RuitkListView.h"
#include "RuitkNode.h"
#include "RuitkRoot.h"
#include "RuitkCoreElements.h"
#include "RuitkSlateElements.h"
#include "Widgets/SOverlay.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ListViewTest
{
	static int32 GRenderCalls = 0;
	static int32 GLastSelectedIndex = -99;

	static TArray<TSharedPtr<FRuitkValue>> MakeItems(std::initializer_list<int32> Values)
	{
		TArray<TSharedPtr<FRuitkValue>> Out;
		for (int32 V : Values)
		{
			Out.Add(MakeShared<FRuitkValue>(V));
		}
		return Out;
	}

	/** Pull the mounted SRuitkListView out of a root (the sole child of the root's overlay panel). */
	static SRuitkListView* ResolveListView(const TSharedRef<FRuitkRoot>& Root)
	{
		TSharedRef<SOverlay> Panel = Root->GetWidget()->GetRootPanel();
		FChildren* Children = Panel->GetChildren();
		if (Children == nullptr || Children->Num() == 0)
		{
			return nullptr;
		}
		return static_cast<SRuitkListView*>(&Children->GetChildAt(0).Get());
	}
} // namespace ListViewTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkListViewTest, "Ruitk.Widgets.ListView",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkListViewTest::RunTest(const FString&)
{
	using namespace Ruitk::Slate;
	using namespace ListViewTest;

	if (!FSlateApplication::IsInitialized())
	{
		AddInfo(TEXT("[listview] Slate not initialized — row-generation checks require it; skipped."));
		return true;
	}

	// Stable item pointers so rows are REUSED across a renderer swap (the reactive path, not churn).
	TArray<TSharedPtr<FRuitkValue>> Items = MakeItems({10, 20, 30});

	// ── generation: three items -> three row sub-roots, renderer invoked once per row ──────────
	GRenderCalls = 0;
	auto R1 = MakeItemRenderer(
		[](const FRuitkValue& V, int32) -> FRuitkNode
		{
			++GRenderCalls;
			return Ruitk::TextBlock(FString::FromInt(static_cast<int32>(V.IntValue)));
		});

	FRuitkListViewProps P1;
	P1.SetItems(Items);
	P1.SetRenderItem(R1);

	TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(ListView(P1));
	Root->FlushSync();

	SRuitkListView* View = ResolveListView(Root);
	if (!TestNotNull(TEXT("SRuitkListView mounted"), View))
	{
		return false;
	}

	View->ForceGenerateRows(FVector2D(300.0f, 2000.0f)); // tall enough to fit all three
	TestEqual(TEXT("all three rows generated"), View->NumGeneratedRows(), 3);
	TestEqual(TEXT("renderer invoked once per row at generation"), GRenderCalls, 3);

	// ── reactivity: a fresh RenderItem closure re-runs against every live row, no row churn ────
	const int32 CallsAfterGen = GRenderCalls;
	auto R2 = MakeItemRenderer(
		[](const FRuitkValue& V, int32) -> FRuitkNode
		{
			++GRenderCalls;
			return Ruitk::TextBlock(FString::FromInt(static_cast<int32>(V.IntValue) * 2));
		});

	FRuitkListViewProps P2;
	P2.SetItems(Items); // SAME pointers -> no regeneration, rows reused
	P2.SetRenderItem(R2);
	Root->Update(ListView(P2));
	Root->FlushSync();

	TestEqual(TEXT("no row churn on renderer swap"), View->NumGeneratedRows(), 3);
	TestEqual(TEXT("each live row re-ran the new closure"), GRenderCalls, CallsAfterGen + 3);

	Root->Unmount();
	TestEqual(TEXT("rows torn down with the root"), View->NumGeneratedRows(), 0);

	// ── selection forwards the selected item's index through OnSelectionChanged ────────────────
	{
		GRenderCalls = 0;
		GLastSelectedIndex = -99;
		TArray<TSharedPtr<FRuitkValue>> SelItems = MakeItems({100, 200});
		auto R3 = MakeItemRenderer([](const FRuitkValue& V, int32) -> FRuitkNode
								   { return Ruitk::TextBlock(FString::FromInt(static_cast<int32>(V.IntValue))); });
		FRuitkListViewProps SP;
		SP.SetItems(SelItems);
		SP.SetRenderItem(R3);
		SP.SetSelectionMode(FName(TEXT("single")));
		SP.SetOnSelectionChanged(FRuitkCallback::Create([](const FRuitkValue& Arg)
														{ GLastSelectedIndex = static_cast<int32>(Arg.IntValue); }));

		TSharedRef<FRuitkRoot> SRoot = FRuitkRoot::Create(ListView(SP));
		SRoot->FlushSync();
		SRuitkListView* SView = ResolveListView(SRoot);
		if (TestNotNull(TEXT("selection view mounted"), SView) && SView->GetListWidget().IsValid())
		{
			SView->GetListWidget()->SetSelection(SelItems[1], ESelectInfo::OnMouseClick);
			TestEqual(TEXT("selection reported the second item's index"), GLastSelectedIndex, 1);
		}
		SRoot->Unmount();
	}

	// ── TileView shares the item model + sub-root rows ────────────────────────────────────────
	{
		GRenderCalls = 0;
		TArray<TSharedPtr<FRuitkValue>> TileItems = MakeItems({1, 2, 3, 4});
		auto TR = MakeItemRenderer(
			[](const FRuitkValue& V, int32) -> FRuitkNode
			{
				++GRenderCalls;
				return Ruitk::TextBlock(FString::FromInt(static_cast<int32>(V.IntValue)));
			});
		FRuitkTileViewProps TP;
		TP.SetItems(TileItems);
		TP.SetRenderItem(TR);
		TP.SetItemWidth(64.0f);
		TP.SetItemHeight(64.0f);

		TSharedRef<FRuitkRoot> TRoot = FRuitkRoot::Create(TileView(TP));
		TRoot->FlushSync();
		SRuitkListView* TView = ResolveListView(TRoot);
		if (TestNotNull(TEXT("SRuitkTileView mounted"), TView))
		{
			TView->ForceGenerateRows(FVector2D(300.0f, 2000.0f));
			TestEqual(TEXT("all four tiles generated"), TView->NumGeneratedRows(), 4);
			TestEqual(TEXT("tile renderer invoked once per tile"), GRenderCalls, 4);
		}
		TRoot->Unmount();
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
