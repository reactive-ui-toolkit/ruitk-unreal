// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Ruitk.Widgets.Batch3c — WIDGET_COMPLETION_PLAN waves 3+4: mount every protocol widget,
// pin its concrete Slate type, and exercise the two new slot models (anchors + fractions).

#include "Misc/AutomationTest.h"
#include "RuitkContext.h"
#include "RuitkListView.h"
#include "RuitkRoot.h"
#include "RuitkSlateElements.h"
#include "RuitkSlateHost.h"
#include "RuitkStyle.h"
#include "RuitkTreeView.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SSplitter.h"

#if WITH_DEV_AUTOMATION_TESTS

#define RUITK_B4_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace B4Test
{
	static TSharedPtr<SWidget> RootChild(FRuitkRoot& Root)
	{
		FChildren* Children = Root.GetWidget()->GetRootPanel()->GetChildren();
		return Children->Num() > 0 ? TSharedPtr<SWidget>(Children->GetChildAt(0)) : nullptr;
	}

	static FRuitkNode WithSlotDict(FRuitkNode Node, std::initializer_list<TPair<FName, FRuitkValue>> Pairs)
	{
		TSharedRef<FRuitkStyleDict> Dict = MakeShared<FRuitkStyleDict>();
		for (const TPair<FName, FRuitkValue>& Pair : Pairs)
		{
			Dict->Add(Pair.Key, Pair.Value);
		}
		TSharedRef<FRuitkPropsBase> Props = ConstCastSharedRef<FRuitkPropsBase>(Node.Props.ToSharedRef());
		Props->SlotProps = Dict;
		return Node;
	}
} // namespace B4Test

static FRuitkNodeArray B4GalleryComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	// P5a: two anchored children on a ConstraintCanvas.
	FRuitkNode Anchored = B4Test::WithSlotDict(Ruitk::TextBlock(TEXT("anchored")),
											 {{FName(TEXT("Slot.Anchors")), FRuitkValue(FString(TEXT("0.5,0.5")))},
											  {FName(TEXT("Slot.Offset")), FRuitkValue(FString(TEXT("10,10,100,40")))},
											  {FName(TEXT("Slot.ZOrder")), FRuitkValue(3.0f)}});

	// P5b: two panes with fractions.
	FRuitkNode PaneA =
		B4Test::WithSlotDict(Ruitk::TextBlock(TEXT("A")), {{FName(TEXT("Slot.SizeValue")), FRuitkValue(0.3f)}});
	FRuitkNode PaneB =
		B4Test::WithSlotDict(Ruitk::TextBlock(TEXT("B")), {{FName(TEXT("Slot.SizeValue")), FRuitkValue(0.7f)}});

	// D-W4: quadrants route by slot.role.
	FRuitkNode QuadTL = Ruitk::TextBlock(TEXT("TL")); // no role -> topLeft default
	FRuitkNode QuadBR = B4Test::WithSlotDict(Ruitk::TextBlock(TEXT("BR")),
										   {{FName(TEXT("Slot.Role")), FRuitkValue(FName(TEXT("bottomRight")))}});

	// P3: anchor + role="menu" content.
	FRuitkNode MenuContent = B4Test::WithSlotDict(Ruitk::TextBlock(TEXT("popup")),
												{{FName(TEXT("Slot.Role")), FRuitkValue(FName(TEXT("menu")))}});

	FRuitkNumericDropDownProps DropP;
	DropP.SetValues({1.0f, 2.0f, 4.0f});
	DropP.SetLabels({TEXT("one"), TEXT("two"), TEXT("four")});
	DropP.SetValue(2.0f);

	FRuitkBreadcrumbTrailProps CrumbP;
	CrumbP.SetCrumbs({TEXT("root"), TEXT("child"), TEXT("leaf")});

	FRuitkVectorInputBoxProps VecP;
	VecP.SetX(1.0f);
	VecP.SetY(2.0f);
	VecP.SetZ(3.0f);

	FRuitkRotatorInputBoxProps RotP;
	RotP.SetRoll(10.0f);
	RotP.SetPitch(20.0f);
	RotP.SetYaw(30.0f);

	FRuitkTreeViewProps TreeP;
	TreeP.SetColumns({{FName(TEXT("Name")), FText::FromString(TEXT("Name")), 1.0f}});

	return {Ruitk::Slate::VerticalBox(
		FRuitkVerticalBoxProps(),
		{Ruitk::Slate::ConstraintCanvas(FRuitkConstraintCanvasProps(), {MoveTemp(Anchored)}),
		 Ruitk::Slate::Splitter(FRuitkSplitterProps(), {MoveTemp(PaneA), MoveTemp(PaneB)}),
		 Ruitk::Slate::MenuAnchor(FRuitkMenuAnchorProps(), {Ruitk::TextBlock(TEXT("anchor")), MoveTemp(MenuContent)}),
		 Ruitk::Slate::WindowTitleBarArea(FRuitkWindowTitleBarAreaProps(), {Ruitk::TextBlock(TEXT("title"))}),
		 Ruitk::Slate::NumericDropDown(MoveTemp(DropP)), Ruitk::Slate::BreadcrumbTrail(MoveTemp(CrumbP)),
		 Ruitk::Slate::NotificationList(), Ruitk::Slate::LinkedBox(FRuitkLinkedBoxProps(), {Ruitk::TextBlock(TEXT("linked"))}),
		 Ruitk::Slate::VirtualJoystick(), Ruitk::Slate::VectorInputBox(MoveTemp(VecP)),
		 Ruitk::Slate::RotatorInputBox(MoveTemp(RotP)), Ruitk::Slate::TreeView(MoveTemp(TreeP)),
		 Ruitk::Slate::Splitter2x2(FRuitkSplitter2x2Props(), {MoveTemp(QuadTL), MoveTemp(QuadBR)})})};
}
RUITK_COMPONENT(B4GalleryComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkWidgetsBatch3cTest, "Ruitk.Widgets.Batch3c", RUITK_B4_TEST_FLAGS)
bool FRuitkWidgetsBatch3cTest::RunTest(const FString&)
{
	TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(Ruitk::FC(&B4GalleryComp));
	TSharedPtr<SWidget> Panel = B4Test::RootChild(*Root);
	if (!TestTrue(TEXT("panel mounted"), Panel.IsValid()))
	{
		return false;
	}
	FChildren* Kids = Panel->GetChildren();
	TestEqual(TEXT("thirteen widgets mounted"), Kids->Num(), 13);
	TestEqual(TEXT("SConstraintCanvas"), Kids->GetChildAt(0)->GetType(), FName(TEXT("SConstraintCanvas")));
	TestEqual(TEXT("SSplitter"), Kids->GetChildAt(1)->GetType(), FName(TEXT("SSplitter")));
	TestEqual(TEXT("SMenuAnchor"), Kids->GetChildAt(2)->GetType(), FName(TEXT("SMenuAnchor")));
	TestEqual(TEXT("SWindowTitleBarArea"), Kids->GetChildAt(3)->GetType(), FName(TEXT("SWindowTitleBarArea")));
	TestEqual(TEXT("SBreadcrumbTrail type"), Kids->GetChildAt(5)->GetType(), FName(TEXT("SBreadcrumbTrail<FString>")));
	TestEqual(TEXT("SNotificationList"), Kids->GetChildAt(6)->GetType(), FName(TEXT("SNotificationList")));
	TestEqual(TEXT("SLinkedBox"), Kids->GetChildAt(7)->GetType(), FName(TEXT("SLinkedBox")));
	TestEqual(TEXT("SVirtualJoystick"), Kids->GetChildAt(8)->GetType(), FName(TEXT("SVirtualJoystick")));
	TestEqual(TEXT("SRuitkTreeView"), Kids->GetChildAt(11)->GetType(), FName(TEXT("SRuitkTreeView")));

	// P5a: the anchor slot actually carries the parsed values.
	{
		FChildren* CanvasKids = Kids->GetChildAt(0)->GetChildren();
		if (TestEqual(TEXT("canvas holds the anchored child"), CanvasKids->Num(), 1))
		{
			const SConstraintCanvas::FSlot& Slot =
				static_cast<const SConstraintCanvas::FSlot&>(CanvasKids->GetSlotAt(0));
			TestTrue(TEXT("anchors applied"), Slot.GetAnchors().Minimum.Equals(FVector2D(0.5, 0.5)));
			TestEqual(TEXT("zorder applied"), Slot.GetZOrder(), 3.0f);
		}
	}

	// P5b: both panes attached.
	TestEqual(TEXT("splitter holds both panes"), Kids->GetChildAt(1)->GetChildren()->Num(), 2);

	// D-W4: role routing put TL in the top-left quadrant, BR in the bottom-right.
	{
		TSharedPtr<SWidget> Quad = Kids->GetChildAt(12);
		TestEqual(TEXT("SSplitter2x2"), Quad->GetType(), FName(TEXT("SSplitter2x2")));
		SSplitter2x2& Q = static_cast<SSplitter2x2&>(*Quad);
		TestEqual(TEXT("topLeft routed"), Q.GetTopLeftContent()->GetType(), FName(TEXT("STextBlock")));
		TestEqual(TEXT("bottomRight routed"), Q.GetBottomRightContent()->GetType(), FName(TEXT("STextBlock")));
		TestNotEqual(TEXT("topRight empty"), Q.GetTopRightContent()->GetType(), FName(TEXT("STextBlock")));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
