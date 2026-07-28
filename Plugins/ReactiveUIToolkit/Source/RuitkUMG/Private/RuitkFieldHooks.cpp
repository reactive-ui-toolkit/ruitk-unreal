// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuitkFieldHooks.h"

namespace
{
	/** The subscription cell payload — the DESTRUCTOR unbinds (the family contract: hook
	 *  cells release external subscriptions in ~cell, so unmount needs no extra plumbing). */
	struct FRuitkFieldSub
	{
		TWeakInterfacePtr<INotifyFieldValueChanged> Vm;
		UE::FieldNotification::FFieldId FieldId;
		FDelegateHandle Handle;

		void Unbind()
		{
			if (INotifyFieldValueChanged* Live = Vm.Get())
			{
				if (Handle.IsValid())
				{
					Live->RemoveFieldValueChangedDelegate(FieldId, Handle);
				}
			}
			Handle.Reset();
			Vm = TWeakInterfacePtr<INotifyFieldValueChanged>();
			FieldId = UE::FieldNotification::FFieldId();
		}

		~FRuitkFieldSub() { Unbind(); }
	};
} // namespace

namespace Ruitk::Umg::Private
{
	bool UseFieldSubscription(FRuitkContext& Ctx, UObject* ViewModel, FName FieldName)
	{
		// a bump-counter state slot: the broadcast increments it, scheduling this component
		auto [Tick, SetTick] = Ctx.UseState<int32>(0);
		TRuitkSetter<int32> Bump = SetTick;
		TSharedRef<TRuitkRef<FRuitkFieldSub>> Sub = Ctx.UseRef<FRuitkFieldSub>();

		INotifyFieldValueChanged* Notify = Cast<INotifyFieldValueChanged>(ViewModel);
		UE::FieldNotification::FFieldId WantId;
		if (Notify && ViewModel)
		{
			WantId = Notify->GetFieldNotificationDescriptor().GetField(ViewModel->GetClass(), FieldName);
		}

		FRuitkFieldSub& Held = Sub->Current;
		const bool bSameTarget = Held.Vm.Get() == Notify && Held.FieldId == WantId && Held.Handle.IsValid();
		if (!bSameTarget)
		{
			Held.Unbind();
			if (Notify && WantId.IsValid())
			{
				Held.Vm = TWeakInterfacePtr<INotifyFieldValueChanged>(ViewModel);
				Held.FieldId = WantId;
				Held.Handle = Notify->AddFieldValueChangedDelegate(
					WantId, INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateLambda(
								[Bump](UObject*, UE::FieldNotification::FFieldId)
								{ Bump([](const int32& Value) { return Value + 1; }); }));
			}
		}
		return Held.Handle.IsValid();
	}
} // namespace Ruitk::Umg::Private
