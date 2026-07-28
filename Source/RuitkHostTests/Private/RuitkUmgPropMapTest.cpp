// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Ruitk.Umg.PropMap — TD-021: the per-class UMG prop-map bridge. A declarative name->FRuitkValue
// map is applied to a hosted UUserWidget's UPROPERTYs by reflection (type-matched), so a hosted widget
// receives Ruitk-driven data without a hand-written binding. Verifies the reflection application
// directly (int/float/bool/text/string, unknown skipped) and end-to-end through the UserWidget element.

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "UObject/Package.h"
#include "RuitkRoot.h"
#include "RuitkTypes.h"
#include "RuitkUmgElement.h"
#include "RuitkTestViewModel.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkUmgPropMapTest, "Ruitk.Umg.PropMap",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkUmgPropMapTest::RunTest(const FString&)
{
	// ── the reflection application, direct (no world needed) ─────────────────────────────────────
	URuitkTestUserWidget* Widget = NewObject<URuitkTestUserWidget>(GetTransientPackage());
	Widget->AddToRoot();

	FRuitkStyleDict Props;
	Props.Add(FName(TEXT("IntValue")), FRuitkValue(42));
	Props.Add(FName(TEXT("FloatValue")), FRuitkValue(3.5f));
	Props.Add(FName(TEXT("BoolValue")), FRuitkValue(true));
	Props.Add(FName(TEXT("TextValue")), FRuitkValue(FText::FromString(TEXT("hello"))));
	Props.Add(FName(TEXT("StringValue")), FRuitkValue(FString(TEXT("world"))));

	const int32 Applied = Ruitk::Umg::ApplyPropMap(Widget, Props);
	TestEqual(TEXT("all five typed properties applied"), Applied, 5);
	TestEqual(TEXT("int reflected"), Widget->IntValue, 42);
	TestTrue(TEXT("float reflected"), FMath::IsNearlyEqual(Widget->FloatValue, 3.5f));
	TestTrue(TEXT("bool reflected"), Widget->BoolValue);
	TestEqual(TEXT("text reflected"), Widget->TextValue.ToString(), FString(TEXT("hello")));
	TestEqual(TEXT("string reflected"), Widget->StringValue, FString(TEXT("world")));

	// unknown property names are skipped, not errors
	FRuitkStyleDict Unknown;
	Unknown.Add(FName(TEXT("NoSuchProperty")), FRuitkValue(1));
	TestEqual(TEXT("unknown property applies nothing"), Ruitk::Umg::ApplyPropMap(Widget, Unknown), 0);

	// a String value coerces into an FText property
	FRuitkStyleDict Coerce;
	Coerce.Add(FName(TEXT("TextValue")), FRuitkValue(FString(TEXT("coerced"))));
	Ruitk::Umg::ApplyPropMap(Widget, Coerce);
	TestEqual(TEXT("string coerces into FText property"), Widget->TextValue.ToString(), FString(TEXT("coerced")));

	Widget->RemoveFromRoot();

	// ── end-to-end through the UserWidget element (mounted in a standalone game instance) ────────
	if (GEngine != nullptr)
	{
		UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
		GameInstance->AddToRoot();
		GameInstance->InitializeStandalone();
		UWorld* World = GameInstance->GetWorld();

		if (TestNotNull(TEXT("standalone world"), World))
		{
			FRuitkStyleDict Mounted;
			Mounted.Add(FName(TEXT("IntValue")), FRuitkValue(7));
			TSharedRef<FRuitkRoot> Root =
				FRuitkRoot::Create(Ruitk::Umg::UserWidget(URuitkTestUserWidget::StaticClass(), World, Mounted));
			Root->FlushSync();
			TestTrue(TEXT("hosted widget produced a Slate child"),
					 Root->GetWidget()->GetRootPanel()->GetChildren()->Num() > 0);
			Root->Unmount();
		}

		GameInstance->Shutdown();
		GameInstance->RemoveFromRoot();
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
