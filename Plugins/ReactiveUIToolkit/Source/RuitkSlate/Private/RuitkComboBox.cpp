// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-012 tail — SComboBox wrapper. The selected display and each dropdown row are FRuitkRoot sub-roots
// built from the shared RenderOption closure. Selection is controlled (SetSelectedItem never re-fires
// OnSelectionChanged — that only fires on a user pick, filtered by ESelectInfo::Direct).

#include "RuitkComboBox.h"

#include "RuitkElementAdapter.h"
#include "RuitkRoot.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

void SRuitkComboBox::Construct(const FArguments&)
{
	SAssignNew(SelectedHolder, SBox);
	// clang-format off
	ChildSlot
	[
		SAssignNew(Combo, SComboBox<FItemType>)
		.OptionsSource(&Options)
		.OnGenerateWidget(this, &SRuitkComboBox::HandleGenerateRow)
		.OnComboBoxOpening(this, &SRuitkComboBox::HandleMenuOpening)
		.OnSelectionChanged(this, &SRuitkComboBox::HandleSelectionChanged)
		[
			SelectedHolder.ToSharedRef()
		]
	];
	// clang-format on
}

SRuitkComboBox::~SRuitkComboBox()
{
	// Tear down every reconciler sub-root this widget owns (rows + the selected display) so their
	// hooks/effects clean up rather than leaking with the widget (bughunt IW-2).
	UnmountRowRoots();
	if (SelectedRoot.IsValid())
	{
		SelectedRoot->Unmount();
		SelectedRoot.Reset();
	}
}

void SRuitkComboBox::SetOptions(TArray<FItemType> InOptions)
{
	Options = MoveTemp(InOptions);
	if (Combo.IsValid())
	{
		// RefreshOptions can prune a now-absent selection and fire OnSelectionChanged(nullptr, Direct);
		// that is an engine-originated prune, not a user pick, so suppress our callback around it
		// (bughunt B6-1 / IW-3 — Direct alone is not a programmatic-vs-user signal).
		bApplyingSelection = true;
		Combo->RefreshOptions();
		bApplyingSelection = false;
	}
	RefreshSelectedDisplay();
}

void SRuitkComboBox::SetRenderer(TSharedPtr<FRuitkItemRenderer> InRenderer)
{
	Renderer = MoveTemp(InRenderer);
	RefreshSelectedDisplay();
}

void SRuitkComboBox::SetSelectedIndex(int32 Index)
{
	if (Index == SelectedIndex)
	{
		return;
	}
	SelectedIndex = Index;
	if (Combo.IsValid() && Options.IsValidIndex(Index))
	{
		bApplyingSelection = true; // suppress our own callback (bughunt B6)
		Combo->SetSelectedItem(Options[Index]);
		bApplyingSelection = false;
	}
	RefreshSelectedDisplay();
}

FRuitkNode SRuitkComboBox::BuildNodeFor(const FItemType& Item) const
{
	if (!Item.IsValid() || !Renderer.IsValid() || !(*Renderer))
	{
		return FRuitkNode();
	}
	return (*Renderer)(*Item, Options.IndexOfByKey(Item));
}

void SRuitkComboBox::RefreshSelectedDisplay()
{
	if (!SelectedHolder.IsValid())
	{
		return;
	}
	const FItemType Selected = Options.IsValidIndex(SelectedIndex) ? Options[SelectedIndex] : nullptr;
	FRuitkNode Node = BuildNodeFor(Selected);
	if (SelectedRoot.IsValid())
	{
		SelectedRoot->Update(MoveTemp(Node));
		SelectedRoot->FlushSync();
	}
	else
	{
		SelectedRoot = FRuitkRoot::Create(MoveTemp(Node));
		SelectedRoot->FlushSync();
	}
	SelectedHolder->SetContent(SelectedRoot->GetWidget());
}

TSharedRef<SWidget> SRuitkComboBox::HandleGenerateRow(FItemType Item)
{
	TSharedRef<FRuitkRoot> Row = FRuitkRoot::Create(BuildNodeFor(Item));
	Row->FlushSync();
	RowRoots.Add(Row);
	return Row->GetWidget();
}

void SRuitkComboBox::HandleSelectionChanged(FItemType Item, ESelectInfo::Type /*SelectInfo*/)
{
	const int32 Index = Item.IsValid() ? Options.IndexOfByKey(Item) : INDEX_NONE;
	SelectedIndex = Index;
	RefreshSelectedDisplay();
	// Fire only for a genuine USER pick — suppressed via the reentrancy flag while WE drive the set,
	// NOT via ESelectInfo (SComboBox emits Direct for user keyboard-close / gamepad-accept too, B6).
	// A user pick always resolves to a valid option; an INDEX_NONE here is an engine-originated prune
	// (the selected option was removed from Options), never a user action — so never report it as one
	// (bughunt IW-3 / B6-1).
	if (!bApplyingSelection && Index != INDEX_NONE && OnSelectionChanged.IsBound())
	{
		OnSelectionChanged.Execute(FRuitkValue(Index));
	}
}

void SRuitkComboBox::HandleMenuOpening()
{
	UnmountRowRoots(); // each open regenerates fresh rows; retire the prior open's sub-roots
}

void SRuitkComboBox::UnmountRowRoots()
{
	for (const TSharedPtr<FRuitkRoot>& Row : RowRoots)
	{
		if (Row.IsValid())
		{
			Row->Unmount();
		}
	}
	RowRoots.Reset();
}

void SRuitkComboBox::OpenMenu()
{
	UnmountRowRoots(); // a fresh open regenerates the visible rows (retire the previous ones)
	if (Combo.IsValid())
	{
		Combo->SetIsOpen(true);
	}
}

int32 SRuitkComboBox::NumGeneratedRows() const
{
	return RowRoots.Num();
}

TSharedPtr<SWidget> SRuitkComboBox::GetSelectedContent() const
{
	return SelectedRoot.IsValid() ? TSharedPtr<SWidget>(SelectedRoot->GetWidget()) : nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Adapter (Leaf; options are data)
// ─────────────────────────────────────────────────────────────────────────────────────────────

class FRuitkComboBoxAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }
	virtual bool IsPoolable() const override { return false; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SRuitkComboBox);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SRuitkComboBox& W = static_cast<SRuitkComboBox&>(Widget);
		const FRuitkComboBoxProps& N = static_cast<const FRuitkComboBoxProps&>(New);
		const FRuitkComboBoxProps* O = static_cast<const FRuitkComboBoxProps*>(Old);
		if (N.HasOptions() && (O == nullptr || !O->HasOptions() || !(N.Options == O->Options)))
		{
			W.SetOptions(N.Options);
		}
		if (N.HasRenderOption() && (O == nullptr || !O->HasRenderOption() || !(N.RenderOption == O->RenderOption)))
		{
			W.SetRenderer(N.RenderOption);
		}
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
	FRuitkElementTypeId ComboBoxType()
	{
		return Ruitk::InternElementType(FName(TEXT("ComboBox")));
	}

	FRuitkNode ComboBox(FRuitkComboBoxProps Props, FRuitkKey Key)
	{
		FRuitkNode Node;
		Node.Kind = ERuitkNodeKind::Host;
		Node.ElementType = ComboBoxType();
		Node.Props = MakeShared<FRuitkComboBoxProps>(MoveTemp(Props));
		Node.Key = Key;
		return Node;
	}

	namespace Detail
	{
		void RegisterComboBoxAdapter()
		{
			RegisterAdapter(ComboBoxType(), MakeUnique<FRuitkComboBoxAdapter>());
		}
	} // namespace Detail
} // namespace Ruitk::Slate
