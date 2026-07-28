// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-021 — "ours feeding theirs": the REVERSE MVVM bridge. URuitkSignalViewModel is a FieldNotify
// UObject whose fields Ruitk code writes (from an effect or event) and which broadcasts on change,
// so a UMG widget bound to it updates when Ruitk state moves. It implements INotifyFieldValueChanged
// directly over the engine FieldNotification module — NO ModelViewViewModel-plugin dependency, so
// it works whether or not that plugin is enabled (UseField already reads it the other direction).
//
// It exposes a small generic field set (Int/Float/Bool/Text). Ruitk pushes a value with the typed
// setters or Set(FRuitkValue) (routes by kind); each setter skips-when-equal and broadcasts.

#pragma once

#include "CoreMinimal.h"
#include "FieldNotificationDelegate.h"
#include "FieldNotificationId.h"
#include "INotifyFieldValueChanged.h"
#include "RuitkTypes.h"
#include "UObject/Object.h"

#include "RuitkSignalViewModel.generated.h"

UCLASS(BlueprintType)
class RUITKUMG_API URuitkSignalViewModel : public UObject, public INotifyFieldValueChanged
{
	GENERATED_BODY()

public:
	// The generic bindable fields (a UMG widget binds to these by name).
	UPROPERTY(BlueprintReadOnly, Category = "ReactiveUI")
	int32 Int = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ReactiveUI")
	float Float = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ReactiveUI")
	bool Bool = false;

	UPROPERTY(BlueprintReadOnly, Category = "ReactiveUI")
	FText Text;

	/** Typed setters — skip-when-equal, broadcast on change. */
	void SetInt(int32 InValue);
	void SetFloat(float InValue);
	void SetBool(bool InValue);
	void SetText(const FText& InValue);

	/** Push an FRuitkValue into the matching field by kind (the Ruitk->VM bridge entry point). */
	void Set(const FRuitkValue& Value);

	// ── INotifyFieldValueChanged (over the engine FieldNotification module) ─────────────────

	struct RUITKUMG_API FFieldNotificationClassDescriptor : public ::UE::FieldNotification::IClassDescriptor
	{
		static const ::UE::FieldNotification::FFieldId Int;
		static const ::UE::FieldNotification::FFieldId Float;
		static const ::UE::FieldNotification::FFieldId Bool;
		static const ::UE::FieldNotification::FFieldId Text;
		virtual void ForEachField(const UClass* Class,
								  TFunctionRef<bool(::UE::FieldNotification::FFieldId)> Callback) const override;
	};

	virtual FDelegateHandle AddFieldValueChangedDelegate(::UE::FieldNotification::FFieldId InFieldId,
														 FFieldValueChangedDelegate InNewDelegate) override;
	virtual bool RemoveFieldValueChangedDelegate(::UE::FieldNotification::FFieldId InFieldId,
												 FDelegateHandle InHandle) override;
	virtual int32 RemoveAllFieldValueChangedDelegates(FDelegateUserObjectConst InUserObject) override;
	virtual int32 RemoveAllFieldValueChangedDelegates(::UE::FieldNotification::FFieldId InFieldId,
													  FDelegateUserObjectConst InUserObject) override;
	virtual const ::UE::FieldNotification::IClassDescriptor& GetFieldNotificationDescriptor() const override;
	virtual void BroadcastFieldValueChanged(::UE::FieldNotification::FFieldId InFieldId) override;

private:
	::UE::FieldNotification::FFieldMulticastDelegate Delegates;
};
