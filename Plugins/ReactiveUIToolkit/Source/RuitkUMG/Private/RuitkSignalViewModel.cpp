// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuitkSignalViewModel.h"

const ::UE::FieldNotification::FFieldId URuitkSignalViewModel::FFieldNotificationClassDescriptor::Int(TEXT("Int"), 0);
const ::UE::FieldNotification::FFieldId URuitkSignalViewModel::FFieldNotificationClassDescriptor::Float(TEXT("Float"),
																										1);
const ::UE::FieldNotification::FFieldId URuitkSignalViewModel::FFieldNotificationClassDescriptor::Bool(TEXT("Bool"), 2);
const ::UE::FieldNotification::FFieldId URuitkSignalViewModel::FFieldNotificationClassDescriptor::Text(TEXT("Text"), 3);

void URuitkSignalViewModel::FFieldNotificationClassDescriptor::ForEachField(
	const UClass*, TFunctionRef<bool(::UE::FieldNotification::FFieldId)> Callback) const
{
	if (Callback(Int) && Callback(Float) && Callback(Bool))
	{
		Callback(Text);
	}
}

void URuitkSignalViewModel::SetInt(int32 InValue)
{
	if (Int != InValue)
	{
		Int = InValue;
		BroadcastFieldValueChanged(FFieldNotificationClassDescriptor::Int);
	}
}

void URuitkSignalViewModel::SetFloat(float InValue)
{
	if (Float != InValue)
	{
		Float = InValue;
		BroadcastFieldValueChanged(FFieldNotificationClassDescriptor::Float);
	}
}

void URuitkSignalViewModel::SetBool(bool InValue)
{
	if (Bool != InValue)
	{
		Bool = InValue;
		BroadcastFieldValueChanged(FFieldNotificationClassDescriptor::Bool);
	}
}

void URuitkSignalViewModel::SetText(const FText& InValue)
{
	if (!Text.EqualTo(InValue))
	{
		Text = InValue;
		BroadcastFieldValueChanged(FFieldNotificationClassDescriptor::Text);
	}
}

void URuitkSignalViewModel::Set(const FRuitkValue& Value)
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
		break; // Null / Vector2 / Color / Opaque have no generic field target
	}
}

FDelegateHandle URuitkSignalViewModel::AddFieldValueChangedDelegate(::UE::FieldNotification::FFieldId InFieldId,
																	FFieldValueChangedDelegate InNewDelegate)
{
	return Delegates.Add(this, InFieldId, MoveTemp(InNewDelegate));
}

bool URuitkSignalViewModel::RemoveFieldValueChangedDelegate(::UE::FieldNotification::FFieldId InFieldId,
															FDelegateHandle InHandle)
{
	return Delegates.RemoveFrom(this, InFieldId, InHandle).bRemoved;
}

int32 URuitkSignalViewModel::RemoveAllFieldValueChangedDelegates(FDelegateUserObjectConst InUserObject)
{
	return Delegates.RemoveAll(this, InUserObject).RemoveCount;
}

int32 URuitkSignalViewModel::RemoveAllFieldValueChangedDelegates(::UE::FieldNotification::FFieldId InFieldId,
																 FDelegateUserObjectConst InUserObject)
{
	return Delegates.RemoveAll(this, InFieldId, InUserObject).RemoveCount;
}

const ::UE::FieldNotification::IClassDescriptor& URuitkSignalViewModel::GetFieldNotificationDescriptor() const
{
	static FFieldNotificationClassDescriptor Descriptor;
	return Descriptor;
}

void URuitkSignalViewModel::BroadcastFieldValueChanged(::UE::FieldNotification::FFieldId InFieldId)
{
	Delegates.Broadcast(this, InFieldId);
}
