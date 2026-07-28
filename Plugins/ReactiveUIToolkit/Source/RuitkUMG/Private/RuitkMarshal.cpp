// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// The single FRuitkValue ↔ UPROPERTY conversion table (see RuitkMarshal.h). The write half is the
// prop-map bridge's former inner loop, promoted verbatim (bughunt B13's kind-validation rules
// preserved); the read half mirrors it for the reverse direction.

#include "RuitkMarshal.h"

#include "UObject/StrProperty.h" // 5.7 forward-declares FStrProperty through UnrealType.h (TB-30)
#include "UObject/TextProperty.h"
#include "UObject/UnrealType.h"

bool Ruitk::Umg::MarshalToProperty(UObject* Object, FName PropertyName, const FRuitkValue& Value)
{
	if (Object == nullptr)
	{
		return false;
	}
	FProperty* Prop = Object->GetClass()->FindPropertyByName(PropertyName);
	if (Prop == nullptr)
	{
		return false;
	}
	void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Object);
	const FRuitkValue& V = Value;

	// Dispatch on the DESTINATION property type but validate against V.Kind (bughunt B13): FRuitkValue
	// stores each kind in a separate zero-defaulted member, so reading (e.g.) V.FloatValue on an
	// Int-kind value silently yields 0.0. Numeric props coerce Int<->Float; every other category
	// requires a compatible kind, else nothing is written (the documented "mismatches are skipped").
	using EKind = FRuitkValue::EKind;
	const bool bNumeric = (V.Kind == EKind::Int || V.Kind == EKind::Float);
	const double NumD = (V.Kind == EKind::Float) ? V.FloatValue : static_cast<double>(V.IntValue);
	const int64 NumI = (V.Kind == EKind::Float) ? static_cast<int64>(V.FloatValue) : V.IntValue;
	const bool bStringy = (V.Kind == EKind::String || V.Kind == EKind::Text || V.Kind == EKind::Name);
	auto AsString = [&V]() -> FString
	{
		return V.Kind == EKind::Text ? V.TextValue.ToString()
									 : (V.Kind == EKind::Name ? V.NameValue.ToString() : V.StringValue);
	};

	if (FIntProperty* IntProp = CastField<FIntProperty>(Prop))
	{
		return bNumeric && (IntProp->SetPropertyValue(ValuePtr, static_cast<int32>(NumI)), true);
	}
	if (FInt64Property* Int64Prop = CastField<FInt64Property>(Prop))
	{
		return bNumeric && (Int64Prop->SetPropertyValue(ValuePtr, NumI), true);
	}
	if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
	{
		return bNumeric && (FloatProp->SetPropertyValue(ValuePtr, static_cast<float>(NumD)), true);
	}
	if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
	{
		return bNumeric && (DoubleProp->SetPropertyValue(ValuePtr, NumD), true);
	}
	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
	{
		return (V.Kind == EKind::Bool) && (BoolProp->SetPropertyValue(ValuePtr, V.BoolValue), true);
	}
	if (FStrProperty* StrProp = CastField<FStrProperty>(Prop))
	{
		return bStringy && (StrProp->SetPropertyValue(ValuePtr, AsString()), true);
	}
	if (FTextProperty* TextProp = CastField<FTextProperty>(Prop))
	{
		return bStringy && (TextProp->SetPropertyValue(ValuePtr, V.Kind == EKind::Text ? V.TextValue
																					   : FText::FromString(AsString())),
							true);
	}
	if (FNameProperty* NameProp = CastField<FNameProperty>(Prop))
	{
		return bStringy &&
			   (NameProp->SetPropertyValue(ValuePtr, V.Kind == EKind::Name ? V.NameValue : FName(*AsString())), true);
	}
	return false; // an unsupported property type — skipped, not an error
}

bool Ruitk::Umg::MarshalFromProperty(const UObject* Object, FName PropertyName, FRuitkValue& OutValue)
{
	if (Object == nullptr)
	{
		return false;
	}
	const FProperty* Prop = Object->GetClass()->FindPropertyByName(PropertyName);
	if (Prop == nullptr)
	{
		return false;
	}
	const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Object);

	if (const FIntProperty* IntProp = CastField<FIntProperty>(Prop))
	{
		OutValue = FRuitkValue(static_cast<int64>(IntProp->GetPropertyValue(ValuePtr)));
		return true;
	}
	if (const FInt64Property* Int64Prop = CastField<FInt64Property>(Prop))
	{
		OutValue = FRuitkValue(Int64Prop->GetPropertyValue(ValuePtr));
		return true;
	}
	if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
	{
		OutValue = FRuitkValue(static_cast<double>(FloatProp->GetPropertyValue(ValuePtr)));
		return true;
	}
	if (const FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
	{
		OutValue = FRuitkValue(DoubleProp->GetPropertyValue(ValuePtr));
		return true;
	}
	if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
	{
		OutValue = FRuitkValue(BoolProp->GetPropertyValue(ValuePtr));
		return true;
	}
	if (const FStrProperty* StrProp = CastField<FStrProperty>(Prop))
	{
		OutValue = FRuitkValue(StrProp->GetPropertyValue(ValuePtr));
		return true;
	}
	if (const FTextProperty* TextProp = CastField<FTextProperty>(Prop))
	{
		OutValue = FRuitkValue(TextProp->GetPropertyValue(ValuePtr));
		return true;
	}
	if (const FNameProperty* NameProp = CastField<FNameProperty>(Prop))
	{
		OutValue = FRuitkValue(NameProp->GetPropertyValue(ValuePtr));
		return true;
	}
	return false;
}
