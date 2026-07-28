// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Ruitk.Style.* — the D-13 v1 contract: style keys map to setters (a style-only
// re-render provably does NOT reconstruct the widget — pointer identity is asserted),
// removed style keys RESET to defaults (unlike plain props), classes merge under inline.
// Plus the GO-05 pool: released leaves come back as the SAME widget pointer.

#include "Misc/AutomationTest.h"
#include "RuitkContext.h"
#include "RuitkRoot.h"
#include "RuitkSlateElements.h"
#include "RuitkSlateHost.h"
#include "RuitkStyle.h"
#include "Widgets/IToolTip.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

#define RUITK_STYLE_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace StyleTest
{
	static TFunction<void(int32)> IntSetter;

	static TSharedPtr<SWidget> RootChild(FRuitkRoot& Root)
	{
		FChildren* Children = Root.GetWidget()->GetRootPanel()->GetChildren();
		return Children->Num() > 0 ? TSharedPtr<SWidget>(Children->GetChildAt(0)) : nullptr;
	}

	// SImage keeps GetColorAndOpacityAttribute protected — a data-free peek subclass re-exports
	// it for assertions (the standard Slate test trick; layout-identical, so the cast is safe).
	struct FImageTintPeek : public SImage
	{
		using SImage::GetColorAndOpacityAttribute;
	};

	static FLinearColor ImageTint(const TSharedRef<SWidget>& W)
	{
		return static_cast<FImageTintPeek&>(static_cast<SImage&>(W.Get()))
			.GetColorAndOpacityAttribute()
			.Get()
			.GetSpecifiedColor();
	}

	static FRuitkNode StyledText(const FString& S, TSharedPtr<FRuitkStyleDict> Style, TArray<FName> Classes = {})
	{
		FRuitkNode Node = Ruitk::TextBlock(S);
		TSharedRef<FRuitkTextBlockProps> Props =
			MakeShared<FRuitkTextBlockProps>(static_cast<const FRuitkTextBlockProps&>(*Node.Props));
		Props->Style = MoveTemp(Style);
		Props->Classes = MoveTemp(Classes);
		Node.Props = Props;
		return Node;
	}
} // namespace StyleTest

// ── apply / update / removal-reset + pointer identity ─────────────────────────────────────

static FRuitkNodeArray StyleModesComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [Mode, SetMode] = Ctx.UseState<int32>(0);
	StyleTest::IntSetter = SetMode;
	TSharedPtr<FRuitkStyleDict> Style;
	if (Mode < 2)
	{
		Style = MakeShared<FRuitkStyleDict>();
		Style->Add(FName(TEXT("RenderOpacity")), FRuitkValue(Mode == 0 ? 0.4f : 0.8f));
		if (Mode == 0)
		{
			Style->Add(FName(TEXT("visibility")), FRuitkValue(FName(TEXT("hidden"))));
			Style->Add(FName(TEXT("RenderTranslation")), FRuitkValue(FVector2D(5.0f, 7.0f)));
			Style->Add(FName(TEXT("Clipping")), FRuitkValue(FName(TEXT("clipToBounds"))));
			Style->Add(FName(TEXT("ToolTipText")), FRuitkValue(FText::FromString(TEXT("hover me"))));
		}
	}
	return {
		Ruitk::Slate::VerticalBox(FRuitkVerticalBoxProps(), {StyleTest::StyledText(TEXT("styled"), MoveTemp(Style))})};
}
RUITK_COMPONENT(StyleModesComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkStyleApplyTest, "Ruitk.Style.ApplyAndReset", RUITK_STYLE_TEST_FLAGS)
bool FRuitkStyleApplyTest::RunTest(const FString&)
{
	TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(Ruitk::FC(&StyleModesComp));
	TSharedPtr<SWidget> Panel = StyleTest::RootChild(*Root);
	if (!TestTrue(TEXT("panel mounted"), Panel.IsValid()))
	{
		return false;
	}
	TSharedRef<SWidget> Text = Panel->GetChildren()->GetChildAt(0);
	SWidget* TextPtrBefore = &Text.Get();
	TestEqual(TEXT("opacity applied at mount"), Text->GetRenderOpacity(), 0.4f);
	TestTrue(TEXT("visibility applied"), Text->GetVisibility() == EVisibility::Hidden);
	TestTrue(TEXT("render transform applied"), Text->GetRenderTransform().IsSet());
	TestTrue(TEXT("clipping applied (the Doom framebuffer key)"), Text->GetClipping() == EWidgetClipping::ClipToBounds);
	TestTrue(TEXT("tooltip applied (P1 universal key)"),
			 Text->GetToolTip().IsValid() && !Text->GetToolTip()->IsEmpty());

	AddInfo(TEXT("[style] style-only change: same widget, new values, removed keys reset"));
	StyleTest::IntSetter(1);
	Root->FlushSync();
	TSharedRef<SWidget> TextAfter = Panel->GetChildren()->GetChildAt(0);
	TestTrue(TEXT("POINTER IDENTITY across style-only re-render (D-13 gate)"), &TextAfter.Get() == TextPtrBefore);
	TestEqual(TEXT("opacity updated"), TextAfter->GetRenderOpacity(), 0.8f);
	TestTrue(TEXT("removed visibility RESET to visible"), TextAfter->GetVisibility() == EVisibility::Visible);
	TestFalse(TEXT("removed translation RESET to identity"), TextAfter->GetRenderTransform().IsSet());
	TestTrue(TEXT("removed clipping RESET to inherit"), TextAfter->GetClipping() == EWidgetClipping::Inherit);
	TestTrue(TEXT("removed tooltip RESET to empty"),
			 !TextAfter->GetToolTip().IsValid() || TextAfter->GetToolTip()->IsEmpty());

	AddInfo(TEXT("[style] whole dict removed -> everything resets"));
	StyleTest::IntSetter(2);
	Root->FlushSync();
	TestEqual(TEXT("opacity reset to 1"), TextAfter->GetRenderOpacity(), 1.0f);
	TestTrue(TEXT("still the same widget"), &Panel->GetChildren()->GetChildAt(0).Get() == TextPtrBefore);
	return true;
}

// ── R11: STRING-literal style values parse like slot values (SLOT-1's class, style side) ──
// The toolchain emits every literal markup style value as a String (`RenderOpacity="0.5"`);
// the union-field reads silently gave 0/false/ZeroVector — invisible widgets, disabled
// buttons — with no diagnostic anywhere. Pinned distinguishably: each expected value differs
// from the old silent default.

static FRuitkNodeArray StyleStringFormsComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	TSharedPtr<FRuitkStyleDict> Style = MakeShared<FRuitkStyleDict>();
	Style->Add(FName(TEXT("RenderOpacity")), FRuitkValue(FString(TEXT("0.5"))));
	Style->Add(FName(TEXT("enabled")), FRuitkValue(FString(TEXT("true"))));
	Style->Add(FName(TEXT("RenderTranslation")), FRuitkValue(FString(TEXT("5,7"))));
	return {Ruitk::Slate::VerticalBox(FRuitkVerticalBoxProps(), {StyleTest::StyledText(TEXT("lit"), MoveTemp(Style))})};
}
RUITK_COMPONENT(StyleStringFormsComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkStyleStringFormsTest, "Ruitk.Style.StringLiteralForms", RUITK_STYLE_TEST_FLAGS)
bool FRuitkStyleStringFormsTest::RunTest(const FString&)
{
	TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(Ruitk::FC(&StyleStringFormsComp));
	TSharedPtr<SWidget> Panel = StyleTest::RootChild(*Root);
	if (!TestTrue(TEXT("panel mounted"), Panel.IsValid()))
	{
		return false;
	}
	TSharedRef<SWidget> Text = Panel->GetChildren()->GetChildAt(0);
	TestEqual(TEXT("RenderOpacity=\"0.5\" parses (was silent 0)"), Text->GetRenderOpacity(), 0.5f);
	TestTrue(TEXT("enabled=\"true\" parses (was silent false)"), Text->IsEnabled());
	TestTrue(TEXT("RenderTranslation=\"5,7\" parses (was silent ZeroVector)"),
			 Text->GetRenderTransform().IsSet() &&
				 FVector2D(Text->GetRenderTransform().GetValue().GetTranslation()).Equals(FVector2D(5.0f, 7.0f)));
	return true;
}

// ── classes merge: class applies, inline wins ─────────────────────────────────────────────

static FRuitkNodeArray StyleClassesComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [Mode, SetMode] = Ctx.UseState<int32>(0);
	StyleTest::IntSetter = SetMode;
	TSharedPtr<FRuitkStyleDict> Inline;
	if (Mode == 1)
	{
		Inline = MakeShared<FRuitkStyleDict>();
		Inline->Add(FName(TEXT("RenderOpacity")), FRuitkValue(0.9f)); // inline beats the class
	}
	return {Ruitk::Slate::VerticalBox(FRuitkVerticalBoxProps(),
									  {StyleTest::StyledText(TEXT("classy"), Inline, {FName(TEXT("rui-test-dim"))})})};
}
RUITK_COMPONENT(StyleClassesComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkStyleClassesTest, "Ruitk.Style.Classes", RUITK_STYLE_TEST_FLAGS)
bool FRuitkStyleClassesTest::RunTest(const FString&)
{
	FRuitkStyleDict Dim;
	Dim.Add(FName(TEXT("RenderOpacity")), FRuitkValue(0.25f));
	Ruitk::Slate::RegisterStyleClass(FName(TEXT("rui-test-dim")), MoveTemp(Dim));

	TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(Ruitk::FC(&StyleClassesComp));
	TSharedPtr<SWidget> Panel = StyleTest::RootChild(*Root);
	if (!TestTrue(TEXT("panel mounted"), Panel.IsValid()))
	{
		return false;
	}
	TSharedRef<SWidget> Text = Panel->GetChildren()->GetChildAt(0);
	TestEqual(TEXT("class opacity applied"), Text->GetRenderOpacity(), 0.25f);

	StyleTest::IntSetter(1);
	Root->FlushSync();
	TestEqual(TEXT("inline style wins over the class"), Text->GetRenderOpacity(), 0.9f);
	return true;
}

// ── GO-05 pool: released leaves come back as the same widget ──────────────────────────────

static FRuitkNodeArray StylePoolComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [Count, SetCount] = Ctx.UseState<int32>(3);
	StyleTest::IntSetter = SetCount;
	TArray<FRuitkNode> Rows;
	for (int32 i = 0; i < Count; ++i)
	{
		FRuitkNode Row = Ruitk::TextBlock(FString::Printf(TEXT("row %d"), i));
		Row.Key = FRuitkKey(i);
		Rows.Add(MoveTemp(Row));
	}
	return {Ruitk::Slate::VerticalBox(FRuitkVerticalBoxProps(), MoveTemp(Rows))};
}
RUITK_COMPONENT(StylePoolComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkStylePoolTest, "Ruitk.Style.NodePool", RUITK_STYLE_TEST_FLAGS)
bool FRuitkStylePoolTest::RunTest(const FString&)
{
	TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(Ruitk::FC(&StylePoolComp));
	TSharedPtr<SWidget> Panel = StyleTest::RootChild(*Root);
	if (!TestTrue(TEXT("panel mounted"), Panel.IsValid()))
	{
		return false;
	}
	TSet<SWidget*> OriginalPtrs;
	for (int32 i = 0; i < 3; ++i)
	{
		OriginalPtrs.Add(&Panel->GetChildren()->GetChildAt(i).Get());
	}

	AddInfo(TEXT("[pool] shrink stashes the released leaves"));
	StyleTest::IntSetter(1);
	Root->FlushSync();
	TestEqual(TEXT("one row left"), Panel->GetChildren()->Num(), 1);
	TestEqual(TEXT("two texts pooled"), Root->GetHost().NumPooled(Ruitk::TextBlockElementType()), 2);

	AddInfo(TEXT("[pool] regrow reuses the SAME widgets (diff-on-reuse)"));
	StyleTest::IntSetter(3);
	Root->FlushSync();
	TestEqual(TEXT("three rows again"), Panel->GetChildren()->Num(), 3);
	TestEqual(TEXT("pool drained"), Root->GetHost().NumPooled(Ruitk::TextBlockElementType()), 0);
	int32 Reused = 0;
	for (int32 i = 0; i < 3; ++i)
	{
		if (OriginalPtrs.Contains(&Panel->GetChildren()->GetChildAt(i).Get()))
		{
			++Reused;
		}
	}
	TestEqual(TEXT("all three widgets are the original pointers (1 kept + 2 pooled)"), Reused, 3);
	// And the reused widgets carry the RIGHT text (diff against stashed props applied it).
	for (int32 i = 0; i < 3; ++i)
	{
		TSharedRef<SWidget> W = Panel->GetChildren()->GetChildAt(i);
		TestEqual(TEXT("reused row text"), StaticCastSharedRef<STextBlock>(W)->GetText().ToString(),
				  FString::Printf(TEXT("row %d"), i));
	}
	return true;
}

// ── widget-specific ColorAndOpacity style key: Image + Separator (Doom regression) ────────
// Markup routes ColorAndOpacity through the style dict; adapters without an ApplyStyleKey
// handler silently dropped it — the Doom viewport's alpha-0 flash quads painted OPAQUE WHITE
// over the whole frame. Asserts apply at mount, live update, and removal-reset for both.

static FRuitkNodeArray StyleTintLeavesComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [Mode, SetMode] = Ctx.UseState<int32>(0);
	StyleTest::IntSetter = SetMode;

	FRuitkImageProps ImageProps;
	FRuitkSeparatorProps SepProps;
	if (Mode < 2)
	{
		const FLinearColor ImageTint = Mode == 0 ? FLinearColor(0.85f, 0.05f, 0.05f, 0.0f) // the Doom hurt flash
												 : FLinearColor(0.95f, 0.85f, 0.2f, 0.35f);
		TSharedRef<FRuitkStyleDict> ImageStyle = MakeShared<FRuitkStyleDict>();
		ImageStyle->Add(FName(TEXT("ColorAndOpacity")), FRuitkValue(ImageTint));
		ImageProps.Style = ImageStyle;

		const FLinearColor SepTint =
			Mode == 0 ? FLinearColor(0.2f, 0.4f, 0.6f, 0.5f) : FLinearColor(0.6f, 0.4f, 0.2f, 1.0f);
		TSharedRef<FRuitkStyleDict> SepStyle = MakeShared<FRuitkStyleDict>();
		SepStyle->Add(FName(TEXT("ColorAndOpacity")), FRuitkValue(SepTint));
		SepProps.Style = SepStyle;
	}
	return {Ruitk::Slate::VerticalBox(FRuitkVerticalBoxProps(), {Ruitk::Slate::Image(MoveTemp(ImageProps)),
																 Ruitk::Slate::Separator(MoveTemp(SepProps))})};
}
RUITK_COMPONENT(StyleTintLeavesComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkStyleTintLeavesTest, "Ruitk.Style.WidgetColorKeys", RUITK_STYLE_TEST_FLAGS)
bool FRuitkStyleTintLeavesTest::RunTest(const FString&)
{
	TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(Ruitk::FC(&StyleTintLeavesComp));
	TSharedPtr<SWidget> Panel = StyleTest::RootChild(*Root);
	if (!TestTrue(TEXT("panel mounted"), Panel.IsValid()))
	{
		return false;
	}
	TSharedRef<SWidget> Image = Panel->GetChildren()->GetChildAt(0);
	TSharedRef<SSeparator> Sep = StaticCastSharedRef<SSeparator>(Panel->GetChildren()->GetChildAt(1));

	TestTrue(TEXT("Image tint applied at mount (alpha 0 — the Doom flash)"),
			 StyleTest::ImageTint(Image) == FLinearColor(0.85f, 0.05f, 0.05f, 0.0f));
	TestTrue(TEXT("Separator tint applied at mount"),
			 Sep->GetColorAndOpacity() == FLinearColor(0.2f, 0.4f, 0.6f, 0.5f));

	AddInfo(TEXT("[style] tint-only change updates in place"));
	StyleTest::IntSetter(1);
	Root->FlushSync();
	TestTrue(TEXT("Image tint updated"), StyleTest::ImageTint(Image) == FLinearColor(0.95f, 0.85f, 0.2f, 0.35f));
	TestTrue(TEXT("Separator tint updated"), Sep->GetColorAndOpacity() == FLinearColor(0.6f, 0.4f, 0.2f, 1.0f));

	AddInfo(TEXT("[style] removed key resets to opaque white (the family rule)"));
	StyleTest::IntSetter(2);
	Root->FlushSync();
	TestTrue(TEXT("Image tint reset"), StyleTest::ImageTint(Image) == FLinearColor::White);
	TestTrue(TEXT("Separator tint reset"), Sep->GetColorAndOpacity() == FLinearColor::White);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
