// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-012 tail — SNumericEntryBox<float> wrapper. The controlled value lives in a member the widget's
// Value attribute reads; OnValueChanged/OnValueCommitted forward the float payload.

#include "RuitkNumericEntryBox.h"

#include "RuitkElementAdapter.h"
#include "Widgets/Input/SNumericEntryBox.h"

void SRuitkNumericEntryBox::Construct(const FArguments& InArgs)
{
	MinValue = InArgs._MinValue;
	MaxValue = InArgs._MaxValue;
	// clang-format off
	ChildSlot
	[
		SAssignNew(Entry, SNumericEntryBox<float>)
		.AllowSpin(false)
		.MinValue(InArgs._MinValue)
		.MaxValue(InArgs._MaxValue)
		.Value(this, &SRuitkNumericEntryBox::GetOptionalValue)
		.OnValueChanged(this, &SRuitkNumericEntryBox::HandleValueChanged)
		.OnValueCommitted(this, &SRuitkNumericEntryBox::HandleValueCommitted)
	];
	// clang-format on
}

float SRuitkNumericEntryBox::Clamp(float InValue) const
{
	float V = InValue;
	if (MinValue.IsSet())
	{
		V = FMath::Max(V, MinValue.GetValue());
	}
	if (MaxValue.IsSet())
	{
		V = FMath::Min(V, MaxValue.GetValue());
	}
	return V;
}

void SRuitkNumericEntryBox::SetValue(float InValue)
{
	const float Clamped = Clamp(InValue);
	// Controlled skip-when-equal (D-16): the field's own edit round-trips to an equal value.
	if (!FMath::IsNearlyEqual(CurrentValue, Clamped))
	{
		CurrentValue = Clamped;
	}
}

void SRuitkNumericEntryBox::HandleValueChanged(float InValue)
{
	// Enforce the documented Min/Max bounds on typed input before forwarding (bughunt IW-1): with
	// AllowSpin(false) the engine never clamps, so an unclamped value would reach the parent state.
	const float Clamped = Clamp(InValue);
	if (OnValueChangedCb.IsBound())
	{
		OnValueChangedCb.Execute(FRuitkValue(Clamped));
	}
}

void SRuitkNumericEntryBox::HandleValueCommitted(float InValue, ETextCommit::Type)
{
	const float Clamped = Clamp(InValue);
	CurrentValue = Clamped; // reflect the clamp in the field immediately (the display reads CurrentValue)
	if (OnValueCommittedCb.IsBound())
	{
		OnValueCommittedCb.Execute(FRuitkValue(Clamped));
	}
}

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Adapter (Leaf)
// ─────────────────────────────────────────────────────────────────────────────────────────────

class FRuitkNumericEntryBoxAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }
	virtual bool IsPoolable() const override { return false; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props, const TSharedPtr<FRuitkEventProxy>&) override
	{
		const FRuitkNumericEntryBoxProps& P = static_cast<const FRuitkNumericEntryBoxProps&>(Props);
		return SNew(SRuitkNumericEntryBox)
			.MinValue(P.HasMinValue() ? TOptional<float>(P.MinValue) : TOptional<float>())
			.MaxValue(P.HasMaxValue() ? TOptional<float>(P.MaxValue) : TOptional<float>());
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SRuitkNumericEntryBox& W = static_cast<SRuitkNumericEntryBox&>(Widget);
		const FRuitkNumericEntryBoxProps& N = static_cast<const FRuitkNumericEntryBoxProps&>(New);
		const FRuitkNumericEntryBoxProps* O = static_cast<const FRuitkNumericEntryBoxProps*>(Old);
		// Bounds first, so a same-render Value re-application clamps against the new bounds (bughunt IW-1:
		// ApplyDiff previously never re-applied Min/Max, so a runtime bound change was inert).
		if (N.HasMinValue() && (O == nullptr || !O->HasMinValue() || !FMath::IsNearlyEqual(N.MinValue, O->MinValue)))
		{
			W.SetMinValue(TOptional<float>(N.MinValue));
		}
		if (N.HasMaxValue() && (O == nullptr || !O->HasMaxValue() || !FMath::IsNearlyEqual(N.MaxValue, O->MaxValue)))
		{
			W.SetMaxValue(TOptional<float>(N.MaxValue));
		}
		if (N.HasValue() && (O == nullptr || !O->HasValue() || !FMath::IsNearlyEqual(N.Value, O->Value)))
		{
			W.SetValue(N.Value);
		}
		if (N.HasOnValueChanged())
		{
			W.SetOnValueChanged(N.OnValueChanged);
		}
		if (N.HasOnValueCommitted())
		{
			W.SetOnValueCommitted(N.OnValueCommitted);
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Type, factory, registration
// ─────────────────────────────────────────────────────────────────────────────────────────────

namespace Ruitk::Slate
{
	FRuitkElementTypeId NumericEntryBoxType()
	{
		return Ruitk::InternElementType(FName(TEXT("NumericEntryBox")));
	}

	FRuitkNode NumericEntryBox(FRuitkNumericEntryBoxProps Props, FRuitkKey Key)
	{
		FRuitkNode Node;
		Node.Kind = ERuitkNodeKind::Host;
		Node.ElementType = NumericEntryBoxType();
		Node.Props = MakeShared<FRuitkNumericEntryBoxProps>(MoveTemp(Props));
		Node.Key = Key;
		return Node;
	}

	namespace Detail
	{
		void RegisterNumericEntryBoxAdapter()
		{
			RegisterAdapter(NumericEntryBoxType(), MakeUnique<FRuitkNumericEntryBoxAdapter>());
		}
	} // namespace Detail
} // namespace Ruitk::Slate
