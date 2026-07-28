// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-012 tail — SNumericEntryBox<float>, the typed numeric field. SNumericEntryBox has NO value
// setter (its Value is a bound attribute), so SRuitkNumericEntryBox holds the controlled value in a
// member the attribute reads — `Value` is applied skip-when-equal against that live value (D-16),
// exactly the controlled-input contract of the editable-text widgets. OnValueChanged/OnValueCommitted
// forward the float payload. Verifiable headless: the displayed value is read from the inner editable
// text via the interaction harness (the entry's value getter reflects the controlled member).

#pragma once

#include "CoreMinimal.h"
#include "RuitkNode.h"
#include "RuitkPropsBase.h"
#include "Widgets/SCompoundWidget.h"

template <typename NumericType> class SNumericEntryBox;

/** SNumericEntryBox<float> (Leaf): a controlled numeric field. Value is applied skip-when-equal;
 *  MinValue/MaxValue bound the typed input; OnValueChanged/OnValueCommitted carry the float. */
struct RUITKSLATE_API FRuitkNumericEntryBoxProps final : public FRuitkPropsBase
{
	RUITK_PROP(float, Value, 0)
	RUITK_PROP(float, MinValue, 1)
	RUITK_PROP(float, MaxValue, 2)
	RUITK_PROP_EVENT(OnValueChanged, 3)
	RUITK_PROP_EVENT(OnValueCommitted, 4)
	RUITK_PROPS_BODY(FRuitkNumericEntryBoxProps,
				   RUITK_EQ(Value) RUITK_EQ(MinValue) RUITK_EQ(MaxValue) RUITK_EQ(OnValueChanged) RUITK_EQ(OnValueCommitted))
};

/** Wraps SNumericEntryBox<float> with the controlled-value member the widget's Value attribute reads. */
class RUITKSLATE_API SRuitkNumericEntryBox final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRuitkNumericEntryBox) {}
	SLATE_ARGUMENT(TOptional<float>, MinValue)
	SLATE_ARGUMENT(TOptional<float>, MaxValue)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetValue(float InValue);
	float GetValue() const { return CurrentValue; }
	void SetMinValue(TOptional<float> InMin) { MinValue = InMin; }
	void SetMaxValue(TOptional<float> InMax) { MaxValue = InMax; }
	void SetOnValueChanged(FRuitkCallback InCb) { OnValueChangedCb = MoveTemp(InCb); }
	void SetOnValueCommitted(FRuitkCallback InCb) { OnValueCommittedCb = MoveTemp(InCb); }

private:
	TOptional<float> GetOptionalValue() const { return CurrentValue; }
	void HandleValueChanged(float InValue);
	void HandleValueCommitted(float InValue, ETextCommit::Type CommitType);
	/** Clamp to [MinValue, MaxValue] when set. AllowSpin(false) routes typed input through the editable-
	 *  text path, which does NOT clamp against the engine's Min/Max — so the wrapper enforces the bounds
	 *  on every forwarded/set value (bughunt IW-1). */
	float Clamp(float InValue) const;

	float CurrentValue = 0.0f;
	TOptional<float> MinValue;
	TOptional<float> MaxValue;
	FRuitkCallback OnValueChangedCb;
	FRuitkCallback OnValueCommittedCb;
	TSharedPtr<SNumericEntryBox<float>> Entry;
};

namespace Ruitk::Slate
{
	RUITKSLATE_API FRuitkElementTypeId NumericEntryBoxType();

	/** A controlled numeric field. Drive `Value` from state; OnValueChanged fires as the user types. */
	RUITKSLATE_API FRuitkNode NumericEntryBox(FRuitkNumericEntryBoxProps Props = FRuitkNumericEntryBoxProps(),
												 FRuitkKey Key = FRuitkKey());

	namespace Detail
	{
		void RegisterNumericEntryBoxAdapter();
	}
} // namespace Ruitk::Slate
