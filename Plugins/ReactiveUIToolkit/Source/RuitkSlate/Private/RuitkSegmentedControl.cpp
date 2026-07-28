// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-012 tail — SSegmentedControl wrapper. Segments are baked from Labels at construction (value =
// index); SelectedIndex drives SetValue skip-when-equal; OnValueChanged forwards the picked index.

#include "RuitkSegmentedControl.h"

#include "RuitkElementAdapter.h"
#include "Widgets/Input/SSegmentedControl.h"

void SRuitkSegmentedControl::Construct(const FArguments& InArgs)
{
	SAssignNew(Control, SSegmentedControl<int32>).OnValueChanged(this, &SRuitkSegmentedControl::HandleValueChanged);
	for (int32 i = 0; i < InArgs._Labels.Num(); ++i)
	{
		Control->AddSlot(i, /*bRebuildChildren*/ false).Text(FText::FromString(InArgs._Labels[i]));
	}
	Control->RebuildChildren();
	if (InArgs._Labels.Num() > 0)
	{
		const int32 Initial = FMath::Clamp(InArgs._InitialIndex, 0, InArgs._Labels.Num() - 1);
		// bUpdateChildren=TRUE so the segment highlight follows the controlled value (bughunt B8): the
		// value attribute is unbound, so the check states are static snapshots — without the refresh they
		// stay Unchecked and no controlled/initial selection ever visually highlights (only user clicks do).
		Control->SetValue(Initial, /*bUpdateChildren*/ true);
	}
	ChildSlot[Control.ToSharedRef()];
}

void SRuitkSegmentedControl::SetSelectedIndex(int32 Index)
{
	// Controlled skip-when-equal (D-16): the widget's own click lands on an equal value.
	if (Control.IsValid() && Index >= 0 && Index < Control->NumSlots() && Control->GetValue() != Index)
	{
		Control->SetValue(Index, /*bUpdateChildren*/ true); // refresh the highlight (bughunt B8)
	}
}

int32 SRuitkSegmentedControl::GetSelectedIndex() const
{
	return Control.IsValid() ? Control->GetValue() : INDEX_NONE;
}

int32 SRuitkSegmentedControl::NumSegments() const
{
	return Control.IsValid() ? Control->NumSlots() : 0;
}

void SRuitkSegmentedControl::HandleValueChanged(int32 Value)
{
	if (OnSelectionChanged.IsBound())
	{
		OnSelectionChanged.Execute(FRuitkValue(Value));
	}
}

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Adapter (Leaf; Labels construct-only)
// ─────────────────────────────────────────────────────────────────────────────────────────────

class FRuitkSegmentedControlAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }
	virtual bool IsPoolable() const override { return false; }

	// Segments bake from Labels at construction (no clear API) — a label-set change replaces the widget.
	virtual uint64 GetReconstructMask() const override { return (1ull << FRuitkSegmentedControlProps::Labels_Bit); }

	virtual bool ConstructOnlyChanged(const FRuitkPropsBase& Old, const FRuitkPropsBase& New) const override
	{
		const FRuitkSegmentedControlProps& O = static_cast<const FRuitkSegmentedControlProps&>(Old);
		const FRuitkSegmentedControlProps& N = static_cast<const FRuitkSegmentedControlProps&>(New);
		// Has-bit gated (SEP-REBUILD-1 class): removing Labels is not a construct-only change.
		return N.HasLabels() && (!O.HasLabels() || !(O.Labels == N.Labels));
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props, const TSharedPtr<FRuitkEventProxy>&) override
	{
		const FRuitkSegmentedControlProps& P = static_cast<const FRuitkSegmentedControlProps&>(Props);
		return SNew(SRuitkSegmentedControl).Labels(P.Labels).InitialIndex(P.HasSelectedIndex() ? P.SelectedIndex : 0);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SRuitkSegmentedControl& W = static_cast<SRuitkSegmentedControl&>(Widget);
		const FRuitkSegmentedControlProps& N = static_cast<const FRuitkSegmentedControlProps&>(New);
		const FRuitkSegmentedControlProps* O = static_cast<const FRuitkSegmentedControlProps*>(Old);
		if (N.HasSelectedIndex() && (O == nullptr || !O->HasSelectedIndex() || !(N.SelectedIndex == O->SelectedIndex)))
		{
			W.SetSelectedIndex(N.SelectedIndex);
		}
		if (N.HasOnSelectionChanged())
		{
			W.SetOnSelectionChanged(N.OnSelectionChanged);
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Type, factory, registration
// ─────────────────────────────────────────────────────────────────────────────────────────────

namespace Ruitk::Slate
{
	FRuitkElementTypeId SegmentedControlType()
	{
		return Ruitk::InternElementType(FName(TEXT("SegmentedControl")));
	}

	FRuitkNode SegmentedControl(FRuitkSegmentedControlProps Props, FRuitkKey Key)
	{
		FRuitkNode Node;
		Node.Kind = ERuitkNodeKind::Host;
		Node.ElementType = SegmentedControlType();
		Node.Props = MakeShared<FRuitkSegmentedControlProps>(MoveTemp(Props));
		Node.Key = Key;
		return Node;
	}

	namespace Detail
	{
		void RegisterSegmentedControlAdapter()
		{
			RegisterAdapter(SegmentedControlType(), MakeUnique<FRuitkSegmentedControlAdapter>());
		}
	} // namespace Detail
} // namespace Ruitk::Slate
