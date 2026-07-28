// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Ruitk.Widgets.SuggestionTextBox — TD-012 tail: the autocomplete field. Verifies the real
// suggestion BEHAVIOUR (the substring filter that decides which suggestions show — exactly what the
// widget's OnShowingSuggestions returns) and the controlled text round-trip through the live widget.

#include "Misc/AutomationTest.h"
#include "Framework/Application/SlateApplication.h"
#include "RuitkElementAdapter.h"
#include "RuitkElementRegistry.h"
#include "RuitkSlateTestHarness.h"
#include "RuitkSuggestionTextBox.h"
#include "RuitkTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkSuggestionTextBoxTest, "Ruitk.Widgets.SuggestionTextBox",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkSuggestionTextBoxTest::RunTest(const FString&)
{
	IRuitkElementAdapter* Adapter = Ruitk::Slate::FindAdapter(Ruitk::Slate::SuggestionTextBoxType());
	if (!TestNotNull(TEXT("SuggestionTextBox adapter registered"), Adapter))
	{
		return false;
	}

	FRuitkSuggestionTextBoxProps Props;
	Props.SetSuggestions({TEXT("apple"), TEXT("apricot"), TEXT("banana"), TEXT("Grape")});
	int32 Changed = 0;
	FString LastChanged;
	Props.SetOnTextChanged(FRuitkCallback::Create(
		[&Changed, &LastChanged](const FRuitkValue& V)
		{
			++Changed;
			LastChanged = V.TextValue.ToString();
		}));

	TSharedRef<SWidget> Widget = Adapter->CreateWidget(Props, nullptr);
	SRuitkSuggestionTextBox& Sug = static_cast<SRuitkSuggestionTextBox&>(Widget.Get());
	Adapter->ApplyDiff(Sug, nullptr, Props);

	// ── the suggestion filter (the real behaviour behind the dropdown) ──────────────────────────
	// 'ap' is a case-insensitive SUBSTRING: apple, apricot, and Gr[ap]e — but not banana.
	TArray<FString> Ap = Sug.ComputeSuggestions(TEXT("ap"));
	TestEqual(TEXT("'ap' matches three candidates (incl. Grape)"), Ap.Num(), 3);
	TestTrue(TEXT("'ap' -> apple"), Ap.Contains(TEXT("apple")));
	TestTrue(TEXT("'ap' -> apricot"), Ap.Contains(TEXT("apricot")));
	TestTrue(TEXT("'ap' -> Grape (substring gr-AP-e)"), Ap.Contains(TEXT("Grape")));
	TestFalse(TEXT("'ap' does NOT match banana"), Ap.Contains(TEXT("banana")));

	// case-insensitive
	TArray<FString> Grape = Sug.ComputeSuggestions(TEXT("grape"));
	TestEqual(TEXT("case-insensitive match on 'grape'"), Grape.Num(), 1);
	TestTrue(TEXT("'grape' -> Grape"), Grape.Contains(TEXT("Grape")));

	// substring, not just prefix
	TArray<FString> Nan = Sug.ComputeSuggestions(TEXT("nan"));
	TestTrue(TEXT("substring 'nan' -> banana"), Nan.Contains(TEXT("banana")));

	// empty input -> no suggestions
	TestEqual(TEXT("empty input yields no suggestions"), Sug.ComputeSuggestions(TEXT("")).Num(), 0);

	// ── controlled text round-trip through the live widget ──────────────────────────────────────
	if (FSlateApplication::IsInitialized())
	{
		RuitkTest::FTestWindow Win(Widget);
		if (Win.IsValid())
		{
			Win.PumpGeometry();
			FRuitkSuggestionTextBoxProps WithText = Props;
			WithText.SetText(FText::FromString(TEXT("apple")));
			Adapter->ApplyDiff(Sug, &Props, WithText);
			TestEqual(TEXT("controlled text set on the widget"), Sug.GetText().ToString(), FString(TEXT("apple")));
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
