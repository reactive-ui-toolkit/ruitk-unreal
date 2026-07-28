// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuitkMvvmViewModel.h"

#include "Engine/GameInstance.h"
#include "MVVMGameSubsystem.h"
#include "RuitkTypes.h" // FRuitkValue
#include "Types/MVVMViewModelCollection.h"
#include "Types/MVVMViewModelContext.h"

void URuitkMvvmViewModel::SetInt(int32 InValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(IntValue, InValue);
}

void URuitkMvvmViewModel::SetFloat(float InValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(FloatValue, InValue);
}

void URuitkMvvmViewModel::SetBool(bool InValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(BoolValue, InValue);
}

void URuitkMvvmViewModel::SetText(const FText& InValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(TextValue, InValue);
}

void URuitkMvvmViewModel::Set(const FRuitkValue& Value)
{
	switch (Value.Kind)
	{
	case FRuitkValue::EKind::Bool:
		SetBool(Value.BoolValue);
		break;
	case FRuitkValue::EKind::Int:
		SetInt(static_cast<int32>(Value.IntValue));
		break;
	case FRuitkValue::EKind::Float:
		SetFloat(static_cast<float>(Value.FloatValue));
		break;
	case FRuitkValue::EKind::Text:
		SetText(Value.TextValue);
		break;
	case FRuitkValue::EKind::String:
		SetText(FText::FromString(Value.StringValue));
		break;
	case FRuitkValue::EKind::Name:
		SetText(FText::FromName(Value.NameValue));
		break;
	default:
		break; // Null / Vector2 / Color / Opaque have no scalar field here
	}
}

namespace
{
	UMVVMViewModelCollectionObject* GlobalCollection(UGameInstance* GameInstance)
	{
		if (GameInstance == nullptr)
		{
			return nullptr;
		}
		if (UMVVMGameSubsystem* Subsystem = GameInstance->GetSubsystem<UMVVMGameSubsystem>())
		{
			return Subsystem->GetViewModelCollection();
		}
		return nullptr;
	}
} // namespace

bool Ruitk::Mvvm::RegisterGlobalViewModel(UGameInstance* GameInstance, FName ContextName, UMVVMViewModelBase* ViewModel)
{
	if (ViewModel == nullptr)
	{
		return false;
	}
	UMVVMViewModelCollectionObject* Collection = GlobalCollection(GameInstance);
	if (Collection == nullptr)
	{
		return false;
	}
	FMVVMViewModelContext Context;
	Context.ContextClass = ViewModel->GetClass();
	Context.ContextName = ContextName;
	const bool bAdded = Collection->AddViewModelInstance(Context, ViewModel);

	// Also register a URuitkMvvmViewModel BASE-class alias for a subclassed viewmodel, so
	// FindGlobalViewModel resolves it whether the caller passes the concrete class or relies on the
	// base-class default (bughunt P1 — the engine's context match is exact class + name, so a subclass
	// registered only under its own class was unresolvable via the default). The name still disambiguates.
	if (bAdded && ViewModel->GetClass() != URuitkMvvmViewModel::StaticClass() && ViewModel->IsA<URuitkMvvmViewModel>())
	{
		FMVVMViewModelContext BaseAlias;
		BaseAlias.ContextClass = URuitkMvvmViewModel::StaticClass();
		BaseAlias.ContextName = ContextName;
		Collection->AddViewModelInstance(BaseAlias, ViewModel); // best-effort; collides only on duplicate name
	}
	return bAdded;
}

UMVVMViewModelBase* Ruitk::Mvvm::FindGlobalViewModel(UGameInstance* GameInstance, FName ContextName,
												   TSubclassOf<UMVVMViewModelBase> ContextClass)
{
	UMVVMViewModelCollectionObject* Collection = GlobalCollection(GameInstance);
	if (Collection == nullptr)
	{
		return nullptr;
	}
	FMVVMViewModelContext Context;
	Context.ContextClass = ContextClass.Get() != nullptr
							   ? ContextClass
							   : TSubclassOf<UMVVMViewModelBase>(URuitkMvvmViewModel::StaticClass());
	Context.ContextName = ContextName;
	return Collection->FindViewModelInstance(Context);
}
