// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Ruitk.CommonUI.Activation — TD-021: the activation-context mechanism (headless): a component
// reads UseIsActive and re-renders when ActivationProvider publishes a new state.
// Ruitk.CommonUI.Screen — the URuitkActivatableScreen UObject end-to-end in a standalone game
// instance: activate -> the hosted Ruitk tree shows ACTIVE; deactivate -> INACTIVE (real CommonUI
// activation driving a real Reactive UI Toolkit re-render).

#include "Misc/AutomationTest.h"
#include "Blueprint/UserWidget.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "RuitkActivatableScreen.h"
#include "RuitkActivation.h"
#include "RuitkContext.h"
#include "RuitkCoreElements.h"
#include "RuitkNode.h"
#include "RuitkRoot.h"
#include "RuitkSlateTestHarness.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CommonUITest
{
	// A probe component that renders its activation state as text — the observable for both suites.
	static FRuitkNodeArray ActiveProbe(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
	{
		const bool bActive = Ruitk::CommonUI::UseIsActive(Ctx);
		return {Ruitk::TextBlock(bActive ? FString(TEXT("ACTIVE")) : FString(TEXT("INACTIVE")))};
	}
	RUITK_COMPONENT(ActiveProbe)

	static FString ProbeText(const TSharedRef<SWidget>& Root)
	{
		SWidget* Text = RuitkTest::FindDescendantByType(Root.Get(), FName(TEXT("STextBlock")));
		return Text != nullptr ? static_cast<STextBlock*>(Text)->GetText().ToString() : FString();
	}

	// Renders the current input method as text — the observable for the B12 device-switch regression.
	static FRuitkNodeArray InputProbe(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
	{
		const ERuitkInputMethod M = Ruitk::CommonUI::UseInputMethod(Ctx);
		const TCHAR* S = M == ERuitkInputMethod::Gamepad ? TEXT("GAMEPAD")
						 : M == ERuitkInputMethod::Touch ? TEXT("TOUCH")
													   : TEXT("MK");
		return {Ruitk::TextBlock(FString(S))};
	}
	RUITK_COMPONENT(InputProbe)
} // namespace CommonUITest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkCommonUIActivationTest, "Ruitk.CommonUI.Activation",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkCommonUIActivationTest::RunTest(const FString&)
{
	using namespace Ruitk::CommonUI;
	using namespace CommonUITest;

	// Inactive by default.
	FRuitkActivationState Inactive;
	TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(ActivationProvider(Inactive, {Ruitk::FC(&ActiveProbe)}));
	Root->FlushSync();
	TestEqual(TEXT("inactive state renders INACTIVE"), ProbeText(Root->GetWidget()), FString(TEXT("INACTIVE")));

	// Publish active -> the consuming component re-renders through the context.
	FRuitkActivationState Active;
	Active.bActive = true;
	Root->Update(ActivationProvider(Active, {Ruitk::FC(&ActiveProbe)}));
	Root->FlushSync();
	TestEqual(TEXT("active state re-renders to ACTIVE"), ProbeText(Root->GetWidget()), FString(TEXT("ACTIVE")));

	// Back to inactive.
	Root->Update(ActivationProvider(Inactive, {Ruitk::FC(&ActiveProbe)}));
	Root->FlushSync();
	TestEqual(TEXT("re-renders back to INACTIVE"), ProbeText(Root->GetWidget()), FString(TEXT("INACTIVE")));

	Root->Unmount();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkCommonUIScreenTest, "Ruitk.CommonUI.Screen",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkCommonUIScreenTest::RunTest(const FString&)
{
	using namespace CommonUITest;

	// Name-mount the probe so the screen can host it.
	Ruitk::RegisterNamedFactory(FName(TEXT("RuitkActiveProbe")), []() { return Ruitk::FC(&ActiveProbe); });

	if (GEngine == nullptr)
	{
		AddInfo(TEXT("[commonui] no GEngine — UObject screen construction skipped."));
		return true;
	}

	// A standalone game instance gives a world + local player so CreateWidget builds a real UUserWidget.
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->AddToRoot();
	GameInstance->InitializeStandalone();

	URuitkActivatableScreen* Screen = CreateWidget<URuitkActivatableScreen>(GameInstance);
	if (TestNotNull(TEXT("activatable screen created"), Screen))
	{
		Screen->ComponentName = FName(TEXT("RuitkActiveProbe"));
		TSharedRef<SWidget> Widget = Screen->TakeWidget();

		TestFalse(TEXT("starts inactive"), Screen->IsScreenActive());
		TestEqual(TEXT("hosted tree starts INACTIVE"), ProbeText(Widget), FString(TEXT("INACTIVE")));

		Screen->ActivateWidget();
		TestTrue(TEXT("ActivateWidget marks the screen active"), Screen->IsScreenActive());
		TestEqual(TEXT("activation re-renders the hosted tree to ACTIVE"), ProbeText(Widget), FString(TEXT("ACTIVE")));

		Screen->DeactivateWidget();
		TestFalse(TEXT("DeactivateWidget clears active"), Screen->IsScreenActive());
		TestEqual(TEXT("deactivation re-renders to INACTIVE"), ProbeText(Widget), FString(TEXT("INACTIVE")));
	}

	GameInstance->Shutdown();
	GameInstance->RemoveFromRoot();
	return true;
}

// ── B12 (bughunt): a live input-method switch re-renders the screen's UseInputMethod consumers ────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkCommonUIInputMethodTest, "Ruitk.CommonUI.InputMethod",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkCommonUIInputMethodTest::RunTest(const FString&)
{
	using namespace CommonUITest;
	Ruitk::RegisterNamedFactory(FName(TEXT("RuitkInputProbe")), []() { return Ruitk::FC(&InputProbe); });
	if (GEngine == nullptr)
	{
		return true;
	}
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->AddToRoot();
	GameInstance->InitializeStandalone();

	URuitkActivatableScreen* Screen = CreateWidget<URuitkActivatableScreen>(GameInstance);
	if (TestNotNull(TEXT("screen created"), Screen))
	{
		Screen->ComponentName = FName(TEXT("RuitkInputProbe"));
		TSharedRef<SWidget> Widget = Screen->TakeWidget();
		Screen->ActivateWidget(); // subscribes to the input-method-changed delegate
		TestEqual(TEXT("B12: starts mouse-and-keyboard"), ProbeText(Widget), FString(TEXT("MK")));

		ULocalPlayer* LocalPlayer = GameInstance->GetLocalPlayerByIndex(0);
		if (LocalPlayer != nullptr)
		{
			if (UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(LocalPlayer))
			{
				Input->SetCurrentInputType(ECommonInputType::Gamepad); // a live device switch
				TestEqual(TEXT("B12: the screen re-rendered to GAMEPAD on the device switch"), ProbeText(Widget),
						  FString(TEXT("GAMEPAD")));
			}
		}
	}

	GameInstance->Shutdown();
	GameInstance->RemoveFromRoot();
	return true;
}

// ── TD-029: the desired-focus seam — UseDesiredFocus designates; the screen answers CommonUI ────

namespace CommonUITest
{
	static int32 GFocusInvocations = 0;

	// Designates itself as the screen's desired focus target (the TD-029 tree-side hook).
	static FRuitkNodeArray FocusProbe(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
	{
		Ruitk::CommonUI::UseDesiredFocus(Ctx, []() { ++GFocusInvocations; });
		return {Ruitk::TextBlock(FString(TEXT("FOCUS-PROBE")))};
	}
	RUITK_COMPONENT(FocusProbe)
} // namespace CommonUITest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkCommonUIDesiredFocusTest, "Ruitk.CommonUI.DesiredFocus",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkCommonUIDesiredFocusTest::RunTest(const FString&)
{
	using namespace Ruitk::CommonUI;
	using namespace CommonUITest;
	GFocusInvocations = 0;

	// ── the mechanism, headless: provider + hook + clear-on-unmount ───────────────────────────
	{
		const TSharedPtr<FRuitkFocusTargetRegistry> Registry = MakeShared<FRuitkFocusTargetRegistry>();
		TestFalse(TEXT("registry starts without a target"), Registry->HasTarget());

		TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(FocusTargetProvider(Registry, {Ruitk::FC(&FocusProbe)}));
		Root->FlushSync();
		if (TestTrue(TEXT("UseDesiredFocus designated a target"), Registry->HasTarget()))
		{
			Registry->FocusDesired();
			TestEqual(TEXT("invoking the registry reaches the designated action"), GFocusInvocations, 1);
		}

		Root->Unmount();
		TestFalse(TEXT("unmount clears the designation"), Registry->HasTarget());
	}

	// ── the screen end-to-end: GetDesiredFocusTarget has somewhere to land ────────────────────
	Ruitk::RegisterNamedFactory(FName(TEXT("RuitkFocusProbe")), []() { return Ruitk::FC(&FocusProbe); });
	Ruitk::RegisterNamedFactory(FName(TEXT("RuitkActiveProbe")), []() { return Ruitk::FC(&ActiveProbe); });
	if (GEngine == nullptr)
	{
		AddInfo(TEXT("[commonui] no GEngine — UObject screen construction skipped."));
		return true;
	}
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->AddToRoot();
	GameInstance->InitializeStandalone();

	URuitkActivatableScreen* Screen = CreateWidget<URuitkActivatableScreen>(GameInstance);
	if (TestNotNull(TEXT("screen created"), Screen))
	{
		Screen->ComponentName = FName(TEXT("RuitkFocusProbe"));
		TSharedRef<SWidget> Widget = Screen->TakeWidget();
		TestTrue(TEXT("hosted tree designated the screen's focus target"), Screen->HasDesiredFocusTarget());
		TestTrue(TEXT("GetDesiredFocusTarget returns the screen (the focus-forwarding landing pad)"),
				 Screen->GetDesiredFocusTarget() == Screen);

		Screen->ReleaseSlateResources(true);
		TestFalse(TEXT("teardown clears the designation"), Screen->HasDesiredFocusTarget());
	}

	// A screen whose component designates nothing keeps the base behavior (no target).
	URuitkActivatableScreen* Plain = CreateWidget<URuitkActivatableScreen>(GameInstance);
	if (TestNotNull(TEXT("plain screen created"), Plain))
	{
		Plain->ComponentName = FName(TEXT("RuitkActiveProbe"));
		TSharedRef<SWidget> Widget = Plain->TakeWidget();
		TestFalse(TEXT("no designation without UseDesiredFocus"), Plain->HasDesiredFocusTarget());
		TestTrue(TEXT("GetDesiredFocusTarget stays the base default (null)"),
				 Plain->GetDesiredFocusTarget() == nullptr);
	}

	GameInstance->Shutdown();
	GameInstance->RemoveFromRoot();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
