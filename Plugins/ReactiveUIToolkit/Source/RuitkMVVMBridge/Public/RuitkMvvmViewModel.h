// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-021 — the MVVM-plugin-coupled reverse bridge + global-collection registration. URuitkSignalViewModel
// (RuitkUMG) is the plugin-INDEPENDENT reverse bridge (hand-rolled FieldNotification). This is its
// MVVM-plugin sibling: URuitkMvvmViewModel derives UMVVMViewModelBase, so it can be REGISTERED IN THE MVVM
// GLOBAL VIEWMODEL COLLECTION and resolved by any UMG view binding by context name — the "globally
// bindable, ours feeding theirs" story that needs the ModelViewViewModel plugin. Ruitk writes it via typed
// setters or Set(FRuitkValue); the MVVM base handles skip-when-equal + FieldNotify broadcast.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Templates/SubclassOf.h"
#include "RuitkMvvmViewModel.generated.h"

struct FRuitkValue;
class UGameInstance;

UCLASS(BlueprintType, meta = (DisplayName = "Reactive UI Toolkit MVVM ViewModel"))
class RUITKMVVMBRIDGE_API URuitkMvvmViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Reactive UI Toolkit")
	int32 IntValue = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Reactive UI Toolkit")
	float FloatValue = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Reactive UI Toolkit")
	bool BoolValue = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Reactive UI Toolkit")
	FText TextValue;

	void SetInt(int32 InValue);
	void SetFloat(float InValue);
	void SetBool(bool InValue);
	void SetText(const FText& InValue);

	/** Route an FRuitkValue to the field matching its kind (Ruitk state -> the bound field). */
	void Set(const FRuitkValue& Value);
};

namespace Ruitk::Mvvm
{
	/** Register `ViewModel` in the game instance's MVVM GLOBAL viewmodel collection under `ContextName`
	 *  (context class = the viewmodel's class). Any UMG view can then bind it globally. False if the
	 *  MVVM game subsystem is unavailable or the add was rejected (duplicate context). */
	RUITKMVVMBRIDGE_API bool RegisterGlobalViewModel(UGameInstance* GameInstance, FName ContextName,
														  UMVVMViewModelBase* ViewModel);

	/** Resolve a globally-registered viewmodel by context name (+ optional class); null if absent. */
	RUITKMVVMBRIDGE_API UMVVMViewModelBase*
	FindGlobalViewModel(UGameInstance* GameInstance, FName ContextName,
						TSubclassOf<UMVVMViewModelBase> ContextClass = nullptr);
} // namespace Ruitk::Mvvm
