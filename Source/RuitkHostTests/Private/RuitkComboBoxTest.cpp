// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Ruitk.Widgets.ComboBox — TD-012 tail: the dropdown selector. Verifies the render-prop
// selected-display sub-root (read back through the widget tree), controlled selection, AND the real
// dropdown: opens the menu and ticks its list via the interaction harness so the option-row sub-roots
// generate — the menu-stack path that made this widget "interactive-only" before the harness.

#include "Misc/AutomationTest.h"
#include "Framework/Application/SlateApplication.h"
#include "RuitkComboBox.h"
#include "RuitkElementAdapter.h"
#include "RuitkElementRegistry.h"
#include "RuitkListView.h" // MakeItemRenderer
#include "RuitkCoreElements.h"
#include "RuitkSlateTestHarness.h"
#include "RuitkTypes.h"
#include "Layout/Geometry.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ComboBoxTest
{
	static FString SelectedText(SRuitkComboBox& Combo)
	{
		TSharedPtr<SWidget> Content = Combo.GetSelectedContent();
		if (!Content.IsValid())
		{
			return FString();
		}
		SWidget* Text = RuitkTest::FindDescendantByType(*Content, FName(TEXT("STextBlock")));
		return Text != nullptr ? static_cast<STextBlock*>(Text)->GetText().ToString() : FString();
	}
} // namespace ComboBoxTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkComboBoxTest, "Ruitk.Widgets.ComboBox",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkComboBoxTest::RunTest(const FString&)
{
	using namespace Ruitk::Slate;
	using namespace ComboBoxTest;

	IRuitkElementAdapter* Adapter = FindAdapter(ComboBoxType());
	if (!TestNotNull(TEXT("ComboBox adapter registered"), Adapter))
	{
		return false;
	}

	TArray<TSharedPtr<FRuitkValue>> Options;
	Options.Add(MakeShared<FRuitkValue>(10));
	Options.Add(MakeShared<FRuitkValue>(20));
	Options.Add(MakeShared<FRuitkValue>(30));

	auto Renderer =
		MakeItemRenderer([](const FRuitkValue& V, int32) -> FRuitkNode
						 { return Ruitk::TextBlock(FString::Printf(TEXT("opt-%d"), static_cast<int32>(V.IntValue))); });

	FRuitkComboBoxProps Props;
	Props.SetOptions(Options);
	Props.SetRenderOption(Renderer);
	Props.SetSelectedIndex(1);

	TSharedRef<SWidget> Widget = Adapter->CreateWidget(Props, nullptr);
	SRuitkComboBox& Combo = static_cast<SRuitkComboBox&>(Widget.Get());
	Adapter->ApplyDiff(Combo, nullptr, Props);

	// ── the selected-display sub-root shows RenderOption(options[1]) ─────────────────────────────
	TestEqual(TEXT("selected index is 1"), Combo.GetSelectedIndex(), 1);
	TestEqual(TEXT("selected display renders option 1"), SelectedText(Combo), FString(TEXT("opt-20")));

	// Controlled selection moves the display.
	FRuitkComboBoxProps Pick2 = Props;
	Pick2.SetSelectedIndex(2);
	Adapter->ApplyDiff(Combo, &Props, Pick2);
	TestEqual(TEXT("controlled selection moved display to option 2"), SelectedText(Combo), FString(TEXT("opt-30")));

	// ── the real dropdown: open the menu and tick its list so the row sub-roots generate ────────
	if (FSlateApplication::IsInitialized())
	{
		RuitkTest::FTestWindow Win(Widget);
		if (Win.IsValid())
		{
			Win.PumpGeometry();
			Combo.OpenMenu();
			FSlateApplication::Get().Tick(); // let the menu-stack push realize
			Win.PumpGeometry();

			// SComboBox's dropdown list is an SComboListType (an SListView subclass) living in the
			// pushed menu window; tick it with geometry so the visible option rows generate.
			const FGeometry Geometry = FGeometry::MakeRoot(FVector2D(200, 600), FSlateLayoutTransform());
			for (int32 Pass = 0; Pass < 3; ++Pass)
			{
				if (SWidget* List = RuitkTest::FindInAllWindowsByType(FName(TEXT("SComboListType"))))
				{
					List->SlatePrepass(1.0f);
					List->Tick(Geometry, 0.0, 0.0f);
				}
			}
			TestEqual(TEXT("all three option rows generated in the dropdown"), Combo.NumGeneratedRows(), 3);
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
