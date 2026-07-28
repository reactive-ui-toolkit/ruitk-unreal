// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Ruitk.Widgets.Batch3 — WIDGET_COMPLETION_PLAN wave 1: the eight mechanical leaves,
// the fully-masked-construct-only class (ColorBlock/gradients/Hyperlink replace in place on
// any prop change — the first widgets whose WHOLE surface rides TD-011), the TD-012 riders,
// and the TD-011 adapter meta-gate over the whole registry.

#include "Misc/AutomationTest.h"
#include "RuitkContext.h"
#include "RuitkElementAdapter.h"
#include "RuitkElementRegistry.h"
#include "RuitkRoot.h"
#include "RuitkSlateElements.h"
#include "RuitkSlateHost.h"
#include "Widgets/SInvalidationPanel.h"

#if WITH_DEV_AUTOMATION_TESTS

#define RUITK_B3_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace B3Test
{
	static TFunction<void(int32)> IntSetter;

	static TSharedPtr<SWidget> RootChild(FRuitkRoot& Root)
	{
		FChildren* Children = Root.GetWidget()->GetRootPanel()->GetChildren();
		return Children->Num() > 0 ? TSharedPtr<SWidget>(Children->GetChildAt(0)) : nullptr;
	}
} // namespace B3Test

// ── mount all eight + live-setter and reconstruct behavior ─────────────────────────────────

static FRuitkNodeArray B3GalleryComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [Mode, SetMode] = Ctx.UseState<int32>(0);
	B3Test::IntSetter = SetMode;

	FRuitkColorBlockProps ColorP;
	ColorP.SetColor(Mode == 0 ? FLinearColor::Red : FLinearColor::Green);
	ColorP.SetSize(FVector2D(24.0, 24.0));

	FRuitkSimpleGradientProps GradP;
	GradP.SetStartColor(FLinearColor::Black);
	GradP.SetEndColor(FLinearColor::White);

	FRuitkComplexGradientProps CGradP;
	CGradP.SetGradientColors({FLinearColor::Red, FLinearColor::Green, FLinearColor::Blue});

	FRuitkHyperlinkProps LinkP;
	LinkP.SetText(FText::FromString(TEXT("docs")));

	FRuitkBackgroundBlurProps BlurP;
	BlurP.SetBlurStrength(Mode == 0 ? 2.0f : 5.0f);

	FRuitkInvalidationPanelProps InvalP;
	InvalP.SetbCanCache(true);

	return {Ruitk::Slate::VerticalBox(
		FRuitkVerticalBoxProps(),
		{Ruitk::Slate::ColorBlock(MoveTemp(ColorP)), Ruitk::Slate::SimpleGradient(MoveTemp(GradP)),
		 Ruitk::Slate::ComplexGradient(MoveTemp(CGradP)), Ruitk::Slate::Hyperlink(MoveTemp(LinkP)),
		 Ruitk::Slate::EnableBox(FRuitkEnableBoxProps(), {Ruitk::TextBlock(TEXT("enabled-island"))}),
		 Ruitk::Slate::ScissorRectBox(FRuitkScissorRectBoxProps(), {Ruitk::TextBlock(TEXT("clipped"))}),
		 Ruitk::Slate::BackgroundBlur(MoveTemp(BlurP), {Ruitk::TextBlock(TEXT("blurred-behind"))}),
		 Ruitk::Slate::InvalidationPanel(MoveTemp(InvalP), {Ruitk::TextBlock(TEXT("cached"))})})};
}
RUITK_COMPONENT(B3GalleryComp)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkWidgetsBatch3Test, "Ruitk.Widgets.Batch3", RUITK_B3_TEST_FLAGS)
bool FRuitkWidgetsBatch3Test::RunTest(const FString&)
{
	TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(Ruitk::FC(&B3GalleryComp));
	TSharedPtr<SWidget> Panel = B3Test::RootChild(*Root);
	if (!TestTrue(TEXT("panel mounted"), Panel.IsValid()))
	{
		return false;
	}
	FChildren* Kids = Panel->GetChildren();
	TestEqual(TEXT("eight widgets mounted"), Kids->Num(), 8);
	TestEqual(TEXT("SColorBlock"), Kids->GetChildAt(0)->GetType(), FName(TEXT("SColorBlock")));
	TestEqual(TEXT("SSimpleGradient"), Kids->GetChildAt(1)->GetType(), FName(TEXT("SSimpleGradient")));
	TestEqual(TEXT("SComplexGradient"), Kids->GetChildAt(2)->GetType(), FName(TEXT("SComplexGradient")));
	TestEqual(TEXT("SHyperlink"), Kids->GetChildAt(3)->GetType(), FName(TEXT("SHyperlink")));
	TestEqual(TEXT("SEnableBox"), Kids->GetChildAt(4)->GetType(), FName(TEXT("SEnableBox")));
	TestEqual(TEXT("SScissorRectBox"), Kids->GetChildAt(5)->GetType(), FName(TEXT("SScissorRectBox")));
	TestEqual(TEXT("SBackgroundBlur"), Kids->GetChildAt(6)->GetType(), FName(TEXT("SBackgroundBlur")));
	TestEqual(TEXT("SInvalidationPanel"), Kids->GetChildAt(7)->GetType(), FName(TEXT("SInvalidationPanel")));

	// Containers actually hold their children.
	TestEqual(TEXT("EnableBox holds content"), Kids->GetChildAt(4)->GetChildren()->Num(), 1);
	TestEqual(TEXT("BackgroundBlur holds content"), Kids->GetChildAt(6)->GetChildren()->Num(), 1);

	SWidget* ColorBefore = &Kids->GetChildAt(0).Get();
	SWidget* BlurBefore = &Kids->GetChildAt(6).Get();

	AddInfo(TEXT("[b3] construct-only prop change REPLACES the masked leaf; setter widget stays"));
	B3Test::IntSetter(1);
	Root->FlushSync();
	TestTrue(TEXT("ColorBlock REPLACED on Color change (fully-masked TD-011 class)"),
			 &Panel->GetChildren()->GetChildAt(0).Get() != ColorBefore);
	TestEqual(TEXT("replacement is still an SColorBlock"), Panel->GetChildren()->GetChildAt(0)->GetType(),
			  FName(TEXT("SColorBlock")));
	TestTrue(TEXT("BackgroundBlur KEPT on BlurStrength change (live setter)"),
			 &Panel->GetChildren()->GetChildAt(6).Get() == BlurBefore);
	return true;
}

// ── TD-011 meta-gate: the whole adapter registry honors the reconstruct-mask contract ──────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkAdapterMaskContractTest, "Ruitk.Contract.AdapterMasks", RUITK_B3_TEST_FLAGS)
bool FRuitkAdapterMaskContractTest::RunTest(const FString&)
{
	// Force full registration (Boot normally does this; keep the test order-independent).
	Ruitk::Slate::RegisterBuiltinAdapters();

	int32 Total = 0, Masked = 0;
	Ruitk::Slate::ForEachAdapter(
		[&](FRuitkElementTypeId Type, IRuitkElementAdapter& Adapter)
		{
			++Total;
			const uint64 Mask = Adapter.GetReconstructMask();
			if (Mask == 0)
			{
				return;
			}
			++Masked;
			// The precise gate must be side-effect-free and FALSE for identical inputs — a
			// ConstructOnlyChanged that fires on equal props would rebuild every commit
			// (state/focus loss), the exact bug class TD-011 exists to prevent. Empty props
			// carry no Has-bits, so any correct Has-gated implementation returns false.
			const FRuitkEmptyProps A, B;
			TestFalse(FString::Printf(TEXT("adapter '%s': ConstructOnlyChanged(empty, empty) must be false"),
									  *Ruitk::GetElementTypeName(Type).ToString()),
					  Adapter.ConstructOnlyChanged(A, B));
		});
	AddInfo(FString::Printf(TEXT("[masks] %d adapters, %d with reconstruct masks"), Total, Masked));
	TestTrue(TEXT("registry populated"), Total > 30);
	TestTrue(TEXT("the masked class exists (Separator + the wave-1 construct-only leaves)"), Masked >= 5);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
