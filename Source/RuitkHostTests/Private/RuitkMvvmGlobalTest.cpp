// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Ruitk.Mvvm.GlobalCollection — TD-021: the MVVM-plugin global viewmodel-collection registration.
// A URuitkMvvmViewModel (a UMVVMViewModelBase) is registered in the game instance's global collection and
// resolved back by context name; Ruitk writes route to the right field via Set(FRuitkValue), and a
// FieldNotify listener confirms the broadcast — the "globally bindable, ours feeding theirs" path.

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "INotifyFieldValueChanged.h" // UE 5.7 deprecated the FieldNotification/IFieldValueChanged.h path
#include "RuitkMvvmViewModel.h"
#include "RuitkTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkMvvmGlobalTest, "Ruitk.Mvvm.GlobalCollection",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkMvvmGlobalTest::RunTest(const FString&)
{
	if (GEngine == nullptr)
	{
		AddInfo(TEXT("[mvvm] no GEngine — global-collection test skipped."));
		return true;
	}

	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->AddToRoot();
	GameInstance->InitializeStandalone();

	URuitkMvvmViewModel* Vm = NewObject<URuitkMvvmViewModel>(GameInstance);

	// ── register in the global collection + resolve back by context name ─────────────────────────
	const bool bRegistered = Ruitk::Mvvm::RegisterGlobalViewModel(GameInstance, FName(TEXT("PlayerStats")), Vm);
	TestTrue(TEXT("viewmodel registered in the global collection"), bRegistered);

	UMVVMViewModelBase* Found =
		Ruitk::Mvvm::FindGlobalViewModel(GameInstance, FName(TEXT("PlayerStats")), URuitkMvvmViewModel::StaticClass());
	TestTrue(TEXT("resolves back the SAME instance by context name"), Found == Vm);

	TestNull(
		TEXT("an unregistered context name resolves to null"),
		Ruitk::Mvvm::FindGlobalViewModel(GameInstance, FName(TEXT("NoSuchContext")), URuitkMvvmViewModel::StaticClass()));

	// ── Ruitk writes route by kind + broadcast a FieldNotify change ────────────────────────────────
	int32 Broadcasts = 0;
	Vm->AddFieldValueChangedDelegate(URuitkMvvmViewModel::FFieldNotificationClassDescriptor::IntValue,
									 INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateLambda(
										 [&Broadcasts](UObject*, UE::FieldNotification::FFieldId) { ++Broadcasts; }));

	Vm->Set(FRuitkValue(42));
	TestEqual(TEXT("Set(int) routed to IntValue"), Vm->IntValue, 42);
	TestEqual(TEXT("IntValue change broadcast once"), Broadcasts, 1);

	Vm->Set(FRuitkValue(42)); // equal -> skip, no broadcast
	TestEqual(TEXT("equal set does not re-broadcast"), Broadcasts, 1);

	Vm->Set(FRuitkValue(FString(TEXT("hello"))));
	TestEqual(TEXT("Set(string) routed to TextValue"), Vm->TextValue.ToString(), FString(TEXT("hello")));

	Vm->Set(FRuitkValue(true));
	TestTrue(TEXT("Set(bool) routed to BoolValue"), Vm->BoolValue);

	GameInstance->Shutdown();
	GameInstance->RemoveFromRoot();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
