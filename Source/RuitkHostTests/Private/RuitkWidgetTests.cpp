// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Ruitk.Widgets.* — contract coverage for the batch-2 widgets: right Slate type
// mounted, prop rows applied (via engine getters where they exist), events through the
// real bound delegates, and the two D-16 controlled-input rules (editable-text caret
// skip-when-equal, self-notifying setter skips).

#include "Misc/AutomationTest.h"
#include "RuitkContext.h"
#include "RuitkRoot.h"
#include "RuitkSlateElements.h"
#include "RuitkSlateHost.h"
#include "SRuitkCanvas.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/SRichTextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

#define RUITK_WIDGET_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace WidgetTest
{
	static TFunction<void(int32)> IntSetter;
	static TSharedPtr<FString> Events;
	static FRuitkHostHandle CapturedNode;

	static void Reset()
	{
		IntSetter = nullptr;
		Events = MakeShared<FString>();
		CapturedNode.Reset();
	}

	static TSharedPtr<SWidget> RootChild(FRuitkRoot& Root, int32 Index = 0)
	{
		FChildren* Children = Root.GetWidget()->GetRootPanel()->GetChildren();
		return Children->Num() > Index ? TSharedPtr<SWidget>(Children->GetChildAt(Index)) : nullptr;
	}
} // namespace WidgetTest

// ── layout leaves + containers: Border(Box(Spacer/Image/ProgressBar)) ─────────────────────

static FRuitkNodeArray WidgetsLayoutComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [Pct, SetPct] = Ctx.UseState<int32>(25);
	WidgetTest::IntSetter = SetPct;

	FRuitkBorderProps BorderProps;
	BorderProps.SetPadding(FMargin(8.0f));
	BorderProps.SetBorderBackgroundColor(FLinearColor::Red);
	BorderProps.SetHAlign(FName(TEXT("center")));

	FRuitkBoxProps BoxProps;
	BoxProps.SetWidthOverride(240.0f);
	BoxProps.SetHeightOverride(120.0f);

	FRuitkSpacerProps SpacerProps;
	SpacerProps.SetSize(FVector2D(10.0f, 20.0f));

	FRuitkImageProps ImageProps;
	ImageProps.SetColorAndOpacity(FLinearColor::Green);
	ImageProps.SetDesiredSizeOverride(FVector2D(32.0f, 32.0f));

	FRuitkProgressBarProps BarProps;
	BarProps.SetPercent(Pct / 100.0f);

	return {Ruitk::Slate::Border(
		MoveTemp(BorderProps),
		{Ruitk::Slate::Box(
			MoveTemp(BoxProps),
			{Ruitk::Slate::VerticalBox(FRuitkVerticalBoxProps(), {Ruitk::Slate::Spacer(MoveTemp(SpacerProps)),
															  Ruitk::Slate::Image(MoveTemp(ImageProps)),
															  Ruitk::Slate::ProgressBar(MoveTemp(BarProps))})})})};
}
RUITK_COMPONENT(WidgetsLayoutComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkWidgetsLayoutTest, "Ruitk.Widgets.Layout", RUITK_WIDGET_TEST_FLAGS)
bool FRuitkWidgetsLayoutTest::RunTest(const FString&)
{
	WidgetTest::Reset();
	TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(Ruitk::FC(&WidgetsLayoutComp));
	TSharedPtr<SWidget> BorderW = WidgetTest::RootChild(*Root);
	if (!TestTrue(TEXT("border mounted"), BorderW.IsValid()))
	{
		return false;
	}
	TestEqual(TEXT("SBorder type"), BorderW->GetType(), FName(TEXT("SBorder")));

	TSharedRef<SWidget> BoxW = BorderW->GetChildren()->GetChildAt(0);
	TestEqual(TEXT("SBox type"), BoxW->GetType(), FName(TEXT("SBox")));
	TSharedRef<SWidget> PanelW = BoxW->GetChildren()->GetChildAt(0);
	TestEqual(TEXT("inner panel"), PanelW->GetType(), FName(TEXT("SVerticalBox")));
	TestEqual(TEXT("three leaves"), PanelW->GetChildren()->Num(), 3);
	TestEqual(TEXT("spacer type"), PanelW->GetChildren()->GetChildAt(0)->GetType(), FName(TEXT("SSpacer")));
	TestEqual(TEXT("image type"), PanelW->GetChildren()->GetChildAt(1)->GetType(), FName(TEXT("SImage")));
	TestEqual(TEXT("progress type"), PanelW->GetChildren()->GetChildAt(2)->GetType(), FName(TEXT("SProgressBar")));

	// The box sizes the layout: prepass then check desired size honors the overrides.
	BorderW->SlatePrepass(1.0f);
	TestEqual(TEXT("SBox width override"), BoxW->GetDesiredSize().X, 240.0f);
	TestEqual(TEXT("SBox height override"), BoxW->GetDesiredSize().Y, 120.0f);

	// Regression (owner playtest): box-panel slots must be AUTO-size by default — Slate's
	// own default is FILL, which squeezes every row (clipped titles, crushed inputs). An
	// auto VerticalBox's desired height must be at least the sum of its children's desired heights.
	float ChildSum = 0.0f;
	for (int32 i = 0; i < PanelW->GetChildren()->Num(); ++i)
	{
		ChildSum += PanelW->GetChildren()->GetChildAt(i)->GetDesiredSize().Y;
	}
	TestTrue(TEXT("auto slots: panel desired height covers all children"),
			 PanelW->GetDesiredSize().Y >= ChildSum - 0.5f);
	return true;
}

// ── controlled editable text: the D-16 caret rule ─────────────────────────────────────────

static FRuitkNodeArray WidgetsEditComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [Gen, SetGen] = Ctx.UseState<int32>(0);
	WidgetTest::IntSetter = SetGen;
	TSharedPtr<FString> Log = WidgetTest::Events;

	FRuitkEditableTextBoxProps Props;
	Props.SetText(FText::FromString(Gen == 0 ? TEXT("alpha") : TEXT("beta")));
	Props.SetHintText(FText::FromString(TEXT("type here")));
	Props.SetOnTextChanged(
		FRuitkCallback::Create([Log](const FRuitkValue& V) { *Log += TEXT("chg:") + V.TextValue.ToString() + TEXT(";"); }));
	Props.SetOnTextCommitted(FRuitkCallback::Create([Log](const FRuitkValue& V)
												  { *Log += TEXT("commit:") + V.TextValue.ToString() + TEXT(";"); }));
	Props.Ref = [](const FRuitkHostHandle& H) { WidgetTest::CapturedNode = H; };
	return {Ruitk::Slate::EditableTextBox(MoveTemp(Props))};
}
RUITK_COMPONENT(WidgetsEditComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkWidgetsEditableTest, "Ruitk.Widgets.EditableText", RUITK_WIDGET_TEST_FLAGS)
bool FRuitkWidgetsEditableTest::RunTest(const FString&)
{
	WidgetTest::Reset();
	TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(Ruitk::FC(&WidgetsEditComp));
	TSharedPtr<SWidget> W = WidgetTest::RootChild(*Root);
	if (!TestTrue(TEXT("editable mounted"), W.IsValid()))
	{
		return false;
	}
	SEditableTextBox& Edit = static_cast<SEditableTextBox&>(*W);
	TestEqual(TEXT("initial text applied"), Edit.GetText().ToString(), FString(TEXT("alpha")));

	AddInfo(TEXT("[caret] user-typed text that matches state is NOT re-set"));
	// Simulate the typing round-trip: the widget already holds the value the state will
	// re-render with. The adapter must skip SetText (compares against the WIDGET).
	Edit.SetText(FText::FromString(TEXT("beta")));
	// SetText is SELF-NOTIFYING (the D-16 premise): the real OnTextChanged delegate fired
	// through the proxy into the user closure.
	TestEqual(TEXT("programmatic SetText notified through the proxy"), *WidgetTest::Events, FString(TEXT("chg:beta;")));
	WidgetTest::Events->Empty();
	WidgetTest::IntSetter(1); // state now renders "beta" too
	Root->FlushSync();
	TestEqual(TEXT("text still beta"), Edit.GetText().ToString(), FString(TEXT("beta")));
	TestEqual(TEXT("the equal-value re-render did NOT re-notify (caret rule)"), *WidgetTest::Events, FString());

	AddInfo(TEXT("[events] the real bound delegates reach the swapped closures"));
	FRuitkSlateNode* Node = FRuitkSlateHost::Resolve(WidgetTest::CapturedNode);
	if (!TestNotNull(TEXT("ref captured the slate node"), Node) ||
		!TestTrue(TEXT("proxy exists"), Node->Proxy.IsValid()))
	{
		return false;
	}
	Node->Proxy->HandleText(FText::FromString(TEXT("x")),
							static_cast<int32>(FRuitkEditableTextBoxProps::OnTextChanged_Bit));
	Node->Proxy->HandleTextCommit(FText::FromString(TEXT("y")), ETextCommit::OnEnter,
								  static_cast<int32>(FRuitkEditableTextBoxProps::OnTextCommitted_Bit));
	TestEqual(TEXT("both handlers fired"), *WidgetTest::Events, FString(TEXT("chg:x;commit:y;")));
	return true;
}

// ── checkbox + slider: self-notifying skips + events ──────────────────────────────────────

static FRuitkNodeArray WidgetsInputComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [State, SetState] = Ctx.UseState<int32>(0);
	WidgetTest::IntSetter = SetState;
	TSharedPtr<FString> Log = WidgetTest::Events;

	FRuitkCheckBoxProps Check;
	Check.SetbIsChecked(State != 0);
	Check.SetOnCheckStateChanged(
		FRuitkCallback::Create([Log](const FRuitkValue& V) { *Log += V.BoolValue ? TEXT("on;") : TEXT("off;"); }));

	FRuitkSliderProps Slide;
	Slide.SetValue(State != 0 ? 0.75f : 0.25f);
	Slide.SetOnValueChanged(
		FRuitkCallback::Create([Log](const FRuitkValue& V) { *Log += FString::Printf(TEXT("v%.2f;"), V.FloatValue); }));

	return {Ruitk::Slate::VerticalBox(
		FRuitkVerticalBoxProps(),
		{Ruitk::Slate::CheckBox(MoveTemp(Check), {Ruitk::TextBlock(TEXT("opt"))}), Ruitk::Slate::Slider(MoveTemp(Slide))})};
}
RUITK_COMPONENT(WidgetsInputComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkWidgetsInputTest, "Ruitk.Widgets.CheckSlider", RUITK_WIDGET_TEST_FLAGS)
bool FRuitkWidgetsInputTest::RunTest(const FString&)
{
	WidgetTest::Reset();
	TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(Ruitk::FC(&WidgetsInputComp));
	TSharedPtr<SWidget> Panel = WidgetTest::RootChild(*Root);
	if (!TestTrue(TEXT("panel mounted"), Panel.IsValid()))
	{
		return false;
	}
	SCheckBox& Check = static_cast<SCheckBox&>(Panel->GetChildren()->GetChildAt(0).Get());
	SSlider& Slide = static_cast<SSlider&>(Panel->GetChildren()->GetChildAt(1).Get());
	TestEqual(TEXT("checkbox type"), Check.GetType(), FName(TEXT("SCheckBox")));
	TestEqual(TEXT("slider type"), Slide.GetType(), FName(TEXT("SSlider")));
	TestFalse(TEXT("unchecked at state 0"), Check.IsChecked());
	TestEqual(TEXT("slider value applied"), Slide.GetValue(), 0.25f);

	AddInfo(TEXT("[inputs] state flip drives both widgets"));
	WidgetTest::IntSetter(1);
	Root->FlushSync();
	TestTrue(TEXT("checked at state 1"), Check.IsChecked());
	TestEqual(TEXT("slider moved"), Slide.GetValue(), 0.75f);
	return true;
}

// ── scrollbox + canvas ────────────────────────────────────────────────────────────────────

static FRuitkNodeArray WidgetsScrollCanvasComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	// Draw fn wrapped ONCE via UseMemo — identity survives re-renders (the D-12 rule).
	const TSharedPtr<FRuitkDrawFn>& Draw = Ctx.UseMemo<TSharedPtr<FRuitkDrawFn>>(
		[]()
		{
			return Ruitk::Slate::MakeDrawFn([](const FGeometry&, FSlateWindowElementList&, int32 LayerId) -> int32
										  { return LayerId; });
		},
		Ruitk::Deps());

	FRuitkCanvasProps CanvasProps;
	CanvasProps.SetDrawFn(Draw);
	CanvasProps.SetCanvasSize(FVector2D(64.0f, 48.0f));

	FRuitkScrollBoxProps ScrollProps;
	ScrollProps.SetOrientation(FName(TEXT("vertical")));

	TArray<FRuitkNode> Items;
	for (int32 i = 0; i < 5; ++i)
	{
		Items.Add(Ruitk::TextBlock(FString::Printf(TEXT("item %d"), i)));
	}
	return {
		Ruitk::Slate::VerticalBox(FRuitkVerticalBoxProps(), {Ruitk::Slate::ScrollBox(MoveTemp(ScrollProps), MoveTemp(Items)),
														 Ruitk::Slate::RuitkCanvas(MoveTemp(CanvasProps))})};
}
RUITK_COMPONENT(WidgetsScrollCanvasComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkWidgetsScrollCanvasTest, "Ruitk.Widgets.ScrollCanvas", RUITK_WIDGET_TEST_FLAGS)
bool FRuitkWidgetsScrollCanvasTest::RunTest(const FString&)
{
	WidgetTest::Reset();
	TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(Ruitk::FC(&WidgetsScrollCanvasComp));
	TSharedPtr<SWidget> Panel = WidgetTest::RootChild(*Root);
	if (!TestTrue(TEXT("panel mounted"), Panel.IsValid()))
	{
		return false;
	}
	TSharedRef<SWidget> Scroll = Panel->GetChildren()->GetChildAt(0);
	TSharedRef<SWidget> Canvas = Panel->GetChildren()->GetChildAt(1);
	TestEqual(TEXT("scrollbox type"), Scroll->GetType(), FName(TEXT("SScrollBox")));
	TestEqual(TEXT("canvas type"), Canvas->GetType(), FName(TEXT("SRuitkCanvas")));

	Canvas->SlatePrepass(1.0f);
	TestTrue(TEXT("canvas desired size"), Canvas->GetDesiredSize() == FVector2D(64.0f, 48.0f));
	return true;
}

// ── batch 2 (Phase 7): WidgetSwitcher + ScaleBox + Throbber + WrapBox ──────────────────────

static FRuitkNodeArray WidgetsBatch2Comp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [Page, SetPage] = Ctx.UseState<int32>(0);
	WidgetTest::IntSetter = SetPage;

	FRuitkWidgetSwitcherProps SwitcherProps;
	SwitcherProps.SetWidgetIndex(Page);

	FRuitkScaleBoxProps ScaleProps;
	ScaleProps.SetStretch(FName(TEXT("scaleToFit")));
	ScaleProps.SetStretchDirection(FName(TEXT("downOnly")));

	FRuitkThrobberProps ThrobProps;
	ThrobProps.SetNumPieces(5);
	ThrobProps.SetAnimate(FName(TEXT("verticalAndOpacity")));

	FRuitkWrapBoxProps WrapProps;
	WrapProps.SetOrientation(FName(TEXT("horizontal")));
	WrapProps.SetWrapSize(120.0f);

	return {Ruitk::Slate::VerticalBox(
		FRuitkVerticalBoxProps(),
		{Ruitk::Slate::WidgetSwitcher(
			 MoveTemp(SwitcherProps),
			 {Ruitk::TextBlock(TEXT("page A")), Ruitk::TextBlock(TEXT("page B")), Ruitk::TextBlock(TEXT("page C"))}),
		 Ruitk::Slate::ScaleBox(MoveTemp(ScaleProps), {Ruitk::TextBlock(TEXT("scaled"))}),
		 Ruitk::Slate::Throbber(MoveTemp(ThrobProps)),
		 Ruitk::Slate::WrapBox(MoveTemp(WrapProps),
							 {Ruitk::TextBlock(TEXT("w0")), Ruitk::TextBlock(TEXT("w1")), Ruitk::TextBlock(TEXT("w2"))})})};
}
RUITK_COMPONENT(WidgetsBatch2Comp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkWidgetsBatch2Test, "Ruitk.Widgets.Batch2", RUITK_WIDGET_TEST_FLAGS)
bool FRuitkWidgetsBatch2Test::RunTest(const FString&)
{
	WidgetTest::Reset();
	TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(Ruitk::FC(&WidgetsBatch2Comp));
	TSharedPtr<SWidget> Panel = WidgetTest::RootChild(*Root);
	if (!TestTrue(TEXT("panel mounted"), Panel.IsValid()))
	{
		return false;
	}
	TSharedRef<SWidget> SwitcherW = Panel->GetChildren()->GetChildAt(0);
	TSharedRef<SWidget> ScaleW = Panel->GetChildren()->GetChildAt(1);
	TSharedRef<SWidget> ThrobW = Panel->GetChildren()->GetChildAt(2);
	TSharedRef<SWidget> WrapW = Panel->GetChildren()->GetChildAt(3);

	// Right Slate types mounted.
	TestEqual(TEXT("switcher type"), SwitcherW->GetType(), FName(TEXT("SWidgetSwitcher")));
	TestEqual(TEXT("scalebox type"), ScaleW->GetType(), FName(TEXT("SScaleBox")));
	TestEqual(TEXT("throbber type"), ThrobW->GetType(), FName(TEXT("SThrobber")));
	TestEqual(TEXT("wrapbox type"), WrapW->GetType(), FName(TEXT("SWrapBox")));

	// WidgetSwitcher: three pages, index prop applied.
	SWidgetSwitcher& Switcher = static_cast<SWidgetSwitcher&>(*SwitcherW);
	TestEqual(TEXT("switcher has 3 pages"), Switcher.GetNumWidgets(), 3);
	TestEqual(TEXT("active index applied"), Switcher.GetActiveWidgetIndex(), 0);

	// ScaleBox wraps one child; WrapBox flowed three children.
	TestEqual(TEXT("scalebox has content"), ScaleW->GetChildren()->Num(), 1);
	TestEqual(TEXT("wrapbox has 3 children"), WrapW->GetChildren()->Num(), 3);

	// State flip drives the switcher index (runtime setter path).
	AddInfo(TEXT("[switcher] state flip retargets the active page"));
	WidgetTest::IntSetter(2);
	Root->FlushSync();
	TestEqual(TEXT("active index retargeted"), Switcher.GetActiveWidgetIndex(), 2);
	return true;
}

// ── batch 2b: text inputs + safe containers + Separator (TD-011 reconstruct mask) ─────────

static FRuitkNodeArray WidgetsBatch2bComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	// state 0: thin/white · 1: thin/red (runtime color only) · 2: thick/red (construct change)
	auto [Phase, SetPhase] = Ctx.UseState<int32>(0);
	WidgetTest::IntSetter = SetPhase;

	FRuitkMultiLineEditableTextBoxProps MultiProps;
	MultiProps.SetText(FText::FromString(TEXT("line one")));
	MultiProps.SetHintText(FText::FromString(TEXT("notes")));

	FRuitkSearchBoxProps SearchProps;
	SearchProps.SetHintText(FText::FromString(TEXT("search")));

	FRuitkSafeZoneProps SafeProps;
	SafeProps.SetbIsTitleSafe(true);

	FRuitkDPIScalerProps DpiProps;
	DpiProps.SetDPIScale(1.5f);

	FRuitkSeparatorProps SepProps;
	SepProps.SetThickness(Phase >= 2 ? 6.0f : 2.0f);
	SepProps.SetColorAndOpacity(Phase >= 1 ? FLinearColor::Red : FLinearColor::White);

	return {Ruitk::Slate::VerticalBox(FRuitkVerticalBoxProps(),
									{Ruitk::Slate::MultiLineEditableTextBox(MoveTemp(MultiProps)),
									 Ruitk::Slate::SearchBox(MoveTemp(SearchProps)),
									 Ruitk::Slate::SafeZone(MoveTemp(SafeProps), {Ruitk::TextBlock(TEXT("safe"))}),
									 Ruitk::Slate::DPIScaler(MoveTemp(DpiProps), {Ruitk::TextBlock(TEXT("scaled"))}),
									 Ruitk::Slate::Separator(MoveTemp(SepProps))})};
}
RUITK_COMPONENT(WidgetsBatch2bComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkWidgetsBatch2bTest, "Ruitk.Widgets.Batch2b", RUITK_WIDGET_TEST_FLAGS)
bool FRuitkWidgetsBatch2bTest::RunTest(const FString&)
{
	WidgetTest::Reset();
	TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(Ruitk::FC(&WidgetsBatch2bComp));
	TSharedPtr<SWidget> Panel = WidgetTest::RootChild(*Root);
	if (!TestTrue(TEXT("panel mounted"), Panel.IsValid()))
	{
		return false;
	}
	TSharedRef<SWidget> MultiW = Panel->GetChildren()->GetChildAt(0);
	TSharedRef<SWidget> SearchW = Panel->GetChildren()->GetChildAt(1);
	TSharedRef<SWidget> SafeW = Panel->GetChildren()->GetChildAt(2);
	TSharedRef<SWidget> DpiW = Panel->GetChildren()->GetChildAt(3);

	TestEqual(TEXT("multiline type"), MultiW->GetType(), FName(TEXT("SMultiLineEditableTextBox")));
	TestEqual(TEXT("searchbox type"), SearchW->GetType(), FName(TEXT("SSearchBox")));
	TestEqual(TEXT("safezone type"), SafeW->GetType(), FName(TEXT("SSafeZone")));
	TestEqual(TEXT("dpiscaler type"), DpiW->GetType(), FName(TEXT("SDPIScaler")));
	TestEqual(TEXT("multiline initial text"), static_cast<SMultiLineEditableTextBox&>(*MultiW).GetText().ToString(),
			  FString(TEXT("line one")));

	// ── TD-011 in production: the Separator's Thickness is construct-only ────────────────────
	SWidget* Sep0 = &Panel->GetChildren()->GetChildAt(4).Get();
	TestEqual(TEXT("separator type"), Sep0->GetType(), FName(TEXT("SSeparator")));

	AddInfo(TEXT("[reconstruct] a runtime-only change (ColorAndOpacity) must NOT replace the widget"));
	WidgetTest::IntSetter(1);
	Root->FlushSync();
	SWidget* Sep1 = &Panel->GetChildren()->GetChildAt(4).Get();
	TestEqual(TEXT("color-only change reused the same SSeparator"), (void*)Sep1, (void*)Sep0);

	AddInfo(TEXT("[reconstruct] a construct-only change (Thickness) MUST replace the widget"));
	WidgetTest::IntSetter(2);
	Root->FlushSync();
	SWidget* Sep2 = &Panel->GetChildren()->GetChildAt(4).Get();
	TestNotEqual(TEXT("thickness change replaced the SSeparator"), (void*)Sep2, (void*)Sep1);
	TestEqual(TEXT("replacement is still an SSeparator"), Sep2->GetType(), FName(TEXT("SSeparator")));
	return true;
}

// ── batch 2c: SpinBox + UniformWrapPanel + RichTextBlock + Grid/UniformGrid panels ─────────

static FRuitkNode CellBox(const TCHAR* Label, int32 Column, int32 Row)
{
	FRuitkBoxProps BoxProps;
	TSharedRef<FRuitkStyleDict> Slot = MakeShared<FRuitkStyleDict>();
	Slot->Add(FName(TEXT("slot.column")), FRuitkValue(Column));
	Slot->Add(FName(TEXT("slot.row")), FRuitkValue(Row));
	BoxProps.SlotProps = Slot;
	return Ruitk::Slate::Box(MoveTemp(BoxProps), {Ruitk::TextBlock(Label)});
}

static FRuitkNodeArray WidgetsBatch2cComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [Val, SetVal] = Ctx.UseState<int32>(0);
	WidgetTest::IntSetter = SetVal;

	FRuitkSpinBoxProps SpinProps;
	SpinProps.SetValue(Val == 0 ? 0.25f : 0.75f);
	SpinProps.SetMinValue(0.0f);
	SpinProps.SetMaxValue(1.0f);

	FRuitkRichTextBlockProps RichProps;
	RichProps.SetText(FText::FromString(TEXT("rich <b>text</>")));
	RichProps.SetbAutoWrapText(true);

	return {Ruitk::Slate::VerticalBox(
		FRuitkVerticalBoxProps(),
		{Ruitk::Slate::SpinBox(MoveTemp(SpinProps)),
		 Ruitk::Slate::UniformWrapPanel(FRuitkUniformWrapPanelProps(),
									  {Ruitk::TextBlock(TEXT("u0")), Ruitk::TextBlock(TEXT("u1"))}),
		 Ruitk::Slate::RichTextBlock(MoveTemp(RichProps)),
		 Ruitk::Slate::GridPanel(FRuitkGridPanelProps(), {CellBox(TEXT("g00"), 0, 0), CellBox(TEXT("g11"), 1, 1)}),
		 Ruitk::Slate::UniformGridPanel(FRuitkUniformGridPanelProps(),
									  {CellBox(TEXT("c00"), 0, 0), CellBox(TEXT("c01"), 0, 1)})})};
}
RUITK_COMPONENT(WidgetsBatch2cComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkWidgetsBatch2cTest, "Ruitk.Widgets.Batch2c", RUITK_WIDGET_TEST_FLAGS)
bool FRuitkWidgetsBatch2cTest::RunTest(const FString&)
{
	WidgetTest::Reset();
	TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(Ruitk::FC(&WidgetsBatch2cComp));
	TSharedPtr<SWidget> Panel = WidgetTest::RootChild(*Root);
	if (!TestTrue(TEXT("panel mounted"), Panel.IsValid()))
	{
		return false;
	}
	SSpinBox<float>& Spin = static_cast<SSpinBox<float>&>(Panel->GetChildren()->GetChildAt(0).Get());
	TSharedRef<SWidget> WrapW = Panel->GetChildren()->GetChildAt(1);
	SRichTextBlock& Rich = static_cast<SRichTextBlock&>(Panel->GetChildren()->GetChildAt(2).Get());
	TSharedRef<SWidget> GridW = Panel->GetChildren()->GetChildAt(3);
	TSharedRef<SWidget> UGridW = Panel->GetChildren()->GetChildAt(4);

	TestEqual(TEXT("uniformwrap type"), WrapW->GetType(), FName(TEXT("SUniformWrapPanel")));
	TestEqual(TEXT("richtext type"), Rich.GetType(), FName(TEXT("SRichTextBlock")));
	TestEqual(TEXT("gridpanel type"), GridW->GetType(), FName(TEXT("SGridPanel")));
	TestEqual(TEXT("uniformgrid type"), UGridW->GetType(), FName(TEXT("SUniformGridPanel")));

	TestEqual(TEXT("spinbox value applied"), Spin.GetValue(), 0.25f);
	TestEqual(TEXT("richtext text applied"), Rich.GetText().ToString(), FString(TEXT("rich <b>text</>")));
	TestEqual(TEXT("uniformwrap has 2 children"), WrapW->GetChildren()->Num(), 2);
	TestEqual(TEXT("gridpanel placed 2 cells"), GridW->GetChildren()->Num(), 2);
	TestEqual(TEXT("uniformgrid placed 2 cells"), UGridW->GetChildren()->Num(), 2);

	AddInfo(TEXT("[spinbox] state flip retargets the value (self-notifying skip path)"));
	WidgetTest::IntSetter(1);
	Root->FlushSync();
	TestEqual(TEXT("spinbox value retargeted"), Spin.GetValue(), 0.75f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
