// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuitkTreeView.h"

#include "RuitkElementAdapter.h"
#include "RuitkRoot.h"

// ─────────────────────────────────────────────────────────────────────────────────────────────
// SRuitkTreeRow — one generated row = one reconciler sub-root (the SRuitkListRow pattern).
// ─────────────────────────────────────────────────────────────────────────────────────────────

class SRuitkTreeRow : public STableRow<TSharedPtr<FRuitkValue>>
{
public:
	SLATE_BEGIN_ARGS(SRuitkTreeRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&, const TSharedRef<STableViewBase>& OwnerTable,
				   const TWeakPtr<SRuitkTreeView>& InOwner, const TSharedPtr<FRuitkValue>& InItem)
	{
		Owner = InOwner;
		Item = InItem;

		FRuitkNode Node;
		if (const TSharedPtr<SRuitkTreeView> Pinned = Owner.Pin())
		{
			Node = Pinned->BuildNodeFor(Item);
		}
		RowRoot = FRuitkRoot::Create(MoveTemp(Node));
		RowRoot->FlushSync();

		STableRow<TSharedPtr<FRuitkValue>>::Construct(
			STableRow<TSharedPtr<FRuitkValue>>::FArguments()[RowRoot->GetWidget()], OwnerTable);
	}

	virtual ~SRuitkTreeRow() override
	{
		if (RowRoot.IsValid())
		{
			RowRoot->Unmount();
			RowRoot.Reset();
		}
	}

	void Rebuild()
	{
		if (!RowRoot.IsValid())
		{
			return;
		}
		FRuitkNode Node;
		if (const TSharedPtr<SRuitkTreeView> Pinned = Owner.Pin())
		{
			Node = Pinned->BuildNodeFor(Item);
		}
		RowRoot->Update(MoveTemp(Node));
		RowRoot->FlushSync();
	}

private:
	TWeakPtr<SRuitkTreeView> Owner;
	TSharedPtr<FRuitkValue> Item;
	TSharedPtr<FRuitkRoot> RowRoot;
};

// ─────────────────────────────────────────────────────────────────────────────────────────────
// SRuitkTreeView
// ─────────────────────────────────────────────────────────────────────────────────────────────

void SRuitkTreeView::Construct(const FArguments& InArgs)
{
	if (InArgs._Columns.Num() > 0)
	{
		Header = SNew(SHeaderRow);
		for (const FRuitkHeaderColumn& Column : InArgs._Columns)
		{
			Header->AddColumn(SHeaderRow::Column(Column.Id).DefaultLabel(Column.Label).FillWidth(Column.FillWidth));
		}
	}

	STreeView<FItemType>::FArguments TreeArgs;
	TreeArgs.TreeItemsSource(&Items)
		.OnGenerateRow(STreeView<FItemType>::FOnGenerateRow::CreateSP(this, &SRuitkTreeView::HandleGenerateRow))
		.OnGetChildren(STreeView<FItemType>::FOnGetChildren::CreateSP(this, &SRuitkTreeView::HandleGetChildren))
		.OnSelectionChanged(
			STreeView<FItemType>::FOnSelectionChanged::CreateSP(this, &SRuitkTreeView::HandleSelectionChanged))
		.OnExpansionChanged(
			STreeView<FItemType>::FOnExpansionChanged::CreateSP(this, &SRuitkTreeView::HandleExpansionChanged))
		.SelectionMode(TAttribute<ESelectionMode::Type>::CreateLambda([this]() { return SelectionModeValue; }));
	if (Header.IsValid())
	{
		TreeArgs.HeaderRow(Header);
	}
	TreeWidget = SNew(STreeView<FItemType>);
	TreeWidget->Construct(TreeArgs);

	ChildSlot[TreeWidget.ToSharedRef()];
}

void SRuitkTreeView::SetItems(TArray<FItemType> InItems)
{
	Items = MoveTemp(InItems);
	TreeWidget->RequestTreeRefresh();
}

void SRuitkTreeView::SetRenderer(TSharedPtr<FRuitkItemRenderer> InRenderer)
{
	Renderer = MoveTemp(InRenderer);
	for (int32 i = LiveRows.Num() - 1; i >= 0; --i)
	{
		if (const TSharedPtr<SRuitkTreeRow> Row = LiveRows[i].Pin())
		{
			Row->Rebuild();
		}
		else
		{
			LiveRows.RemoveAtSwap(i);
		}
	}
}

void SRuitkTreeView::SetChildAccessor(TSharedPtr<FRuitkChildAccessor> InAccessor)
{
	ChildAccessor = MoveTemp(InAccessor);
	TreeWidget->RequestTreeRefresh();
}

void SRuitkTreeView::SetExpandedItems(const TArray<FItemType>& InExpanded)
{
	bApplyingExpansion = true;
	for (const FItemType& Item : KnownExpanded) // collapse what left the controlled set
	{
		if (!InExpanded.Contains(Item))
		{
			TreeWidget->SetItemExpansion(Item, false);
		}
	}
	for (const FItemType& Item : InExpanded)
	{
		TreeWidget->SetItemExpansion(Item, true);
	}
	KnownExpanded = InExpanded;
	bApplyingExpansion = false;
}

void SRuitkTreeView::SetSelectionMode(ESelectionMode::Type InMode)
{
	SelectionModeValue = InMode;
}

void SRuitkTreeView::SetOnSelectionChanged(FRuitkCallback InCallback)
{
	OnSelectionChanged = MoveTemp(InCallback);
}

void SRuitkTreeView::SetOnExpansionChanged(FRuitkCallback InCallback)
{
	OnExpansionChanged = MoveTemp(InCallback);
}

FRuitkNode SRuitkTreeView::BuildNodeFor(const FItemType& Item) const
{
	if (!Renderer.IsValid() || !(*Renderer) || !Item.IsValid())
	{
		return FRuitkNode();
	}
	const int32 Index = Items.IndexOfByKey(Item); // root index; nested rows get INDEX_NONE
	return (*Renderer)(*Item, Index);
}

void SRuitkTreeView::TrackRow(const TSharedRef<SRuitkTreeRow>& Row)
{
	LiveRows.RemoveAll([](const TWeakPtr<SRuitkTreeRow>& W) { return !W.IsValid(); });
	LiveRows.Add(Row);
}

void SRuitkTreeView::ForceGenerateRows(FVector2D ViewportSize)
{
	const FGeometry Geometry = FGeometry::MakeRoot(ViewportSize, FSlateLayoutTransform());
	TreeWidget->Tick(Geometry, 0.0, 0.016f);
	TreeWidget->Tick(Geometry, 0.016, 0.016f);
}

int32 SRuitkTreeView::NumGeneratedRows() const
{
	int32 Count = 0;
	for (const TWeakPtr<SRuitkTreeRow>& Row : LiveRows)
	{
		Count += Row.IsValid() ? 1 : 0;
	}
	return Count;
}

TSharedRef<ITableRow> SRuitkTreeView::HandleGenerateRow(FItemType Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedRef<SRuitkTreeRow> Row = SNew(SRuitkTreeRow, OwnerTable, SharedThis(this), Item);
	TrackRow(Row);
	return Row;
}

void SRuitkTreeView::HandleGetChildren(FItemType Item, TArray<FItemType>& OutChildren)
{
	if (ChildAccessor.IsValid() && (*ChildAccessor) && Item.IsValid())
	{
		OutChildren = (*ChildAccessor)(*Item);
	}
}

void SRuitkTreeView::HandleSelectionChanged(FItemType Item, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo != ESelectInfo::Direct && OnSelectionChanged.IsBound() && Item.IsValid())
	{
		OnSelectionChanged.Execute(*Item);
	}
}

void SRuitkTreeView::HandleExpansionChanged(FItemType Item, bool bExpanded)
{
	if (!bApplyingExpansion && OnExpansionChanged.IsBound())
	{
		OnExpansionChanged.Execute(FRuitkValue(bExpanded));
	}
}

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Adapter + factory
// ─────────────────────────────────────────────────────────────────────────────────────────────

namespace
{
	ESelectionMode::Type TreeSelectionModeOf(FName V)
	{
		return V == FName(TEXT("single"))		  ? ESelectionMode::Single
			   : V == FName(TEXT("singleToggle")) ? ESelectionMode::SingleToggle
			   : V == FName(TEXT("multi"))		  ? ESelectionMode::Multi
												  : ESelectionMode::None;
	}

	class FRuitkTreeViewAdapter final : public IRuitkElementAdapter
	{
	public:
		virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }
		virtual bool HasEvents() const override { return true; } // callbacks flow via setters

		virtual uint64 GetReconstructMask() const override { return 1ull << FRuitkTreeViewProps::Columns_Bit; }

		virtual bool ConstructOnlyChanged(const FRuitkPropsBase& Old, const FRuitkPropsBase& New) const override
		{
			const FRuitkTreeViewProps& O = static_cast<const FRuitkTreeViewProps&>(Old);
			const FRuitkTreeViewProps& N = static_cast<const FRuitkTreeViewProps&>(New);
			return N.HasColumns() && (!O.HasColumns() || !(O.Columns == N.Columns));
		}

		virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props,
												 const TSharedPtr<FRuitkEventProxy>&) override
		{
			const FRuitkTreeViewProps& P = static_cast<const FRuitkTreeViewProps&>(Props);
			return SNew(SRuitkTreeView).Columns(P.Columns);
		}

		virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
		{
			SRuitkTreeView& W = static_cast<SRuitkTreeView&>(Widget);
			const FRuitkTreeViewProps& N = static_cast<const FRuitkTreeViewProps&>(New);
			const FRuitkTreeViewProps* O = static_cast<const FRuitkTreeViewProps*>(Old);
			if (N.HasSelectionMode() &&
				(O == nullptr || !O->HasSelectionMode() || !(N.SelectionMode == O->SelectionMode)))
			{
				W.SetSelectionMode(TreeSelectionModeOf(N.SelectionMode));
			}
			if (N.HasOnSelectionChanged())
			{
				W.SetOnSelectionChanged(N.OnSelectionChanged);
			}
			if (N.HasOnExpansionChanged())
			{
				W.SetOnExpansionChanged(N.OnExpansionChanged);
			}
			if (N.HasGetChildren() && (O == nullptr || !O->HasGetChildren() || !(N.GetChildren == O->GetChildren)))
			{
				W.SetChildAccessor(N.GetChildren);
			}
			if (N.HasRenderItem() && (O == nullptr || !O->HasRenderItem() || !(N.RenderItem == O->RenderItem)))
			{
				W.SetRenderer(N.RenderItem);
			}
			if (N.HasItems() && (O == nullptr || !O->HasItems() || !(N.Items == O->Items)))
			{
				W.SetItems(N.Items);
			}
			if (N.HasExpandedItems() &&
				(O == nullptr || !O->HasExpandedItems() || !(N.ExpandedItems == O->ExpandedItems)))
			{
				W.SetExpandedItems(N.ExpandedItems);
			}
		}
	};
} // namespace

namespace Ruitk::Slate
{
	FRuitkElementTypeId TreeViewType()
	{
		return Ruitk::InternElementType(FName(TEXT("TreeView")));
	}

	FRuitkNode TreeView(FRuitkTreeViewProps Props, FRuitkKey Key)
	{
		FRuitkNode Node;
		Node.Kind = ERuitkNodeKind::Host;
		Node.ElementType = TreeViewType();
		Node.Props = MakeShared<FRuitkTreeViewProps>(MoveTemp(Props));
		Node.Children = Ruitk::MakeChildren(TArray<FRuitkNode>());
		Node.Key = Key;
		return Node;
	}

	TSharedPtr<FRuitkChildAccessor> MakeChildAccessor(FRuitkChildAccessor Fn)
	{
		return MakeShared<FRuitkChildAccessor>(MoveTemp(Fn));
	}

	namespace Detail
	{
		void RegisterTreeViewAdapter()
		{
			RegisterAdapter(TreeViewType(), MakeUnique<FRuitkTreeViewAdapter>());
		}
	} // namespace Detail
} // namespace Ruitk::Slate
