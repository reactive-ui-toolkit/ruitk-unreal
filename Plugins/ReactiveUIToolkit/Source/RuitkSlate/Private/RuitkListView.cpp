// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-022 — item-model views. The virtualization comes from Slate's SListView / STileView; the
// reactive half is SRuitkListRow, a table row that owns a per-row FRuitkRoot sub-root. Each row is an
// independent little reconciler: it renders RenderItem(item, index) into its own widget tree, and
// when the parent hands a fresh RenderItem closure the row re-renders that sub-root in place (no
// widget churn — the SListView row is reused, only its content is re-reconciled).

#include "RuitkListView.h"

#include "RuitkElementAdapter.h"
#include "RuitkRoot.h"
#include "RuitkSlateLog.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STileView.h"

// ─────────────────────────────────────────────────────────────────────────────────────────────
// SRuitkListRow — one generated row; owns a detached FRuitkRoot rendering the item's subtree.
// ─────────────────────────────────────────────────────────────────────────────────────────────

class SRuitkListRow : public STableRow<TSharedPtr<FRuitkValue>>
{
public:
	SLATE_BEGIN_ARGS(SRuitkListRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&, const TSharedRef<STableViewBase>& OwnerTable,
				   const TWeakPtr<SRuitkListView>& InOwner, const TSharedPtr<FRuitkValue>& InItem)
	{
		Owner = InOwner;
		Item = InItem;

		FRuitkNode Node;
		if (const TSharedPtr<SRuitkListView> Pinned = Owner.Pin())
		{
			Node = Pinned->BuildNodeFor(Item);
		}
		RowRoot = FRuitkRoot::Create(MoveTemp(Node));
		RowRoot->FlushSync();

		STableRow<TSharedPtr<FRuitkValue>>::Construct(
			STableRow<TSharedPtr<FRuitkValue>>::FArguments()[RowRoot->GetWidget()], OwnerTable);
	}

	virtual ~SRuitkListRow() override
	{
		// Explicit teardown so the sub-root's effect cleanups run deterministically when the list
		// recycles/drops the row (before the shared ref would otherwise unwind).
		if (RowRoot.IsValid())
		{
			RowRoot->Unmount();
			RowRoot.Reset();
		}
	}

	/** Re-run the CURRENT renderer against this row's item and re-reconcile the sub-root in place. */
	void Rebuild()
	{
		if (!RowRoot.IsValid())
		{
			return;
		}
		FRuitkNode Node;
		if (const TSharedPtr<SRuitkListView> Pinned = Owner.Pin())
		{
			Node = Pinned->BuildNodeFor(Item);
		}
		RowRoot->Update(MoveTemp(Node));
		RowRoot->FlushSync();
	}

private:
	TWeakPtr<SRuitkListView> Owner;
	TSharedPtr<FRuitkValue> Item;
	TSharedPtr<FRuitkRoot> RowRoot;
};

// ─────────────────────────────────────────────────────────────────────────────────────────────
// SRuitkListView — wraps SListView/STileView and owns the row bookkeeping.
// ─────────────────────────────────────────────────────────────────────────────────────────────

void SRuitkListView::Construct(const FArguments& InArgs)
{
	ViewKind = InArgs._ViewKind;

	if (ViewKind == ERuitkItemViewKind::Tile)
	{
		ListWidget =
			SNew(STileView<FItemType>)
				.ItemWidth(InArgs._ItemWidth)
				.ItemHeight(InArgs._ItemHeight)
				.ListItemsSource(&Items)
				.OnGenerateTile(this, &SRuitkListView::HandleGenerateRow)
				.SelectionMode(TAttribute<ESelectionMode::Type>::CreateLambda([this]() { return SelectionModeValue; }))
				.OnSelectionChanged(this, &SRuitkListView::HandleSelectionChanged);
	}
	else
	{
		ListWidget =
			SNew(SListView<FItemType>)
				.ListItemsSource(&Items)
				.OnGenerateRow(this, &SRuitkListView::HandleGenerateRow)
				.SelectionMode(TAttribute<ESelectionMode::Type>::CreateLambda([this]() { return SelectionModeValue; }))
				.OnSelectionChanged(this, &SRuitkListView::HandleSelectionChanged);
	}

	ChildSlot[ListWidget.ToSharedRef()];
}

void SRuitkListView::SetItems(TArray<FItemType> InItems)
{
	Items = MoveTemp(InItems);
	if (ListWidget.IsValid())
	{
		ListWidget->RequestListRefresh();
	}
}

void SRuitkListView::SetRenderer(TSharedPtr<FRuitkItemRenderer> InRenderer)
{
	Renderer = MoveTemp(InRenderer);
	// The reactive path: every live row re-runs the new closure against its own sub-root.
	int32 Live = 0;
	for (int32 i = LiveRows.Num() - 1; i >= 0; --i)
	{
		if (const TSharedPtr<SRuitkListRow> Row = LiveRows[i].Pin())
		{
			Row->Rebuild();
			++Live;
		}
		else
		{
			LiveRows.RemoveAtSwap(i);
		}
	}
}

void SRuitkListView::SetSelectionMode(ESelectionMode::Type InMode)
{
	SelectionModeValue = InMode;
}

void SRuitkListView::SetOnSelectionChanged(FRuitkCallback InCallback)
{
	OnSelectionChanged = MoveTemp(InCallback);
}

FRuitkNode SRuitkListView::BuildNodeFor(const FItemType& Item) const
{
	if (!Item.IsValid() || !Renderer.IsValid() || !(*Renderer))
	{
		return FRuitkNode(); // empty fragment — renders nothing
	}
	const int32 Index = Items.IndexOfByKey(Item);
	return (*Renderer)(*Item, Index);
}

void SRuitkListView::TrackRow(const TSharedRef<SRuitkListRow>& Row)
{
	for (int32 i = LiveRows.Num() - 1; i >= 0; --i)
	{
		if (!LiveRows[i].IsValid())
		{
			LiveRows.RemoveAtSwap(i);
		}
	}
	LiveRows.Add(Row);
}

void SRuitkListView::ForceGenerateRows(FVector2D ViewportSize)
{
	if (!ListWidget.IsValid())
	{
		return;
	}
	const FGeometry Geometry = FGeometry::MakeRoot(ViewportSize, FSlateLayoutTransform());
	ListWidget->RequestListRefresh();
	// Two ticks: the first arranges/measures the panel, the second regenerates rows to fill it.
	ListWidget->SlatePrepass(1.0f);
	ListWidget->Tick(Geometry, 0.0, 0.0f);
	ListWidget->SlatePrepass(1.0f);
	ListWidget->Tick(Geometry, 0.0, 0.0f);
}

int32 SRuitkListView::NumGeneratedRows() const
{
	int32 Live = 0;
	for (const TWeakPtr<SRuitkListRow>& Row : LiveRows)
	{
		if (Row.IsValid())
		{
			++Live;
		}
	}
	return Live;
}

TSharedRef<ITableRow> SRuitkListView::HandleGenerateRow(FItemType Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedRef<SRuitkListRow> Row = SNew(SRuitkListRow, OwnerTable, TWeakPtr<SRuitkListView>(SharedThis(this)), Item);
	TrackRow(Row);
	return Row;
}

void SRuitkListView::HandleSelectionChanged(FItemType Item, ESelectInfo::Type SelectInfo)
{
	if (!OnSelectionChanged.IsBound())
	{
		return;
	}
	const int32 Index = Item.IsValid() ? Items.IndexOfByKey(Item) : INDEX_NONE;
	OnSelectionChanged.Execute(FRuitkValue(Index));
}

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Shared setter application (both views share Items/RenderItem/SelectionMode/OnSelectionChanged).
// ─────────────────────────────────────────────────────────────────────────────────────────────

namespace
{
	ESelectionMode::Type SelectionModeOf(FName V)
	{
		if (V == FName(TEXT("single")))
		{
			return ESelectionMode::Single;
		}
		if (V == FName(TEXT("singleToggle")))
		{
			return ESelectionMode::SingleToggle;
		}
		if (V == FName(TEXT("multi")))
		{
			return ESelectionMode::Multi;
		}
		return ESelectionMode::None;
	}

	/** Apply the four shared item-view props; Old==nullptr applies everything set. */
	template <typename TProps> void ApplyItemViewShared(SRuitkListView& W, const TProps* O, const TProps& N)
	{
		if (N.HasItems() && (O == nullptr || !O->HasItems() || !(N.Items == O->Items)))
		{
			W.SetItems(N.Items);
		}
		if (N.HasRenderItem() && (O == nullptr || !O->HasRenderItem() || !(N.RenderItem == O->RenderItem)))
		{
			W.SetRenderer(N.RenderItem);
		}
		if (N.HasSelectionMode() && (O == nullptr || !O->HasSelectionMode() || !(N.SelectionMode == O->SelectionMode)))
		{
			W.SetSelectionMode(SelectionModeOf(N.SelectionMode));
		}
		// Event: identity-swap every commit (cheap; the widget forwards to the current callback).
		if (N.HasOnSelectionChanged())
		{
			W.SetOnSelectionChanged(N.OnSelectionChanged);
		}
	}
} // namespace

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Adapters
// ─────────────────────────────────────────────────────────────────────────────────────────────

class FRuitkListViewAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }

	// Live rows + sub-roots are per-instance state — never pool this widget.
	virtual bool IsPoolable() const override { return false; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SRuitkListView).ViewKind(ERuitkItemViewKind::List);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SRuitkListView& W = static_cast<SRuitkListView&>(Widget);
		ApplyItemViewShared(W, static_cast<const FRuitkListViewProps*>(Old), static_cast<const FRuitkListViewProps&>(New));
	}
};

class FRuitkTileViewAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }
	virtual bool IsPoolable() const override { return false; }

	// Tile cell size is construct-only (STileView bakes its panel around it) — a change rebuilds.
	virtual uint64 GetReconstructMask() const override
	{
		return (1ull << FRuitkTileViewProps::ItemWidth_Bit) | (1ull << FRuitkTileViewProps::ItemHeight_Bit);
	}

	virtual bool ConstructOnlyChanged(const FRuitkPropsBase& Old, const FRuitkPropsBase& New) const override
	{
		const FRuitkTileViewProps& O = static_cast<const FRuitkTileViewProps&>(Old);
		const FRuitkTileViewProps& N = static_cast<const FRuitkTileViewProps&>(New);
		// Has-bit gated (SEP-REBUILD-1 class): removing a cell-size prop is not a construct-only change.
		const bool bW = N.HasItemWidth() && (!O.HasItemWidth() || !(O.ItemWidth == N.ItemWidth));
		const bool bH = N.HasItemHeight() && (!O.HasItemHeight() || !(O.ItemHeight == N.ItemHeight));
		return bW || bH;
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props, const TSharedPtr<FRuitkEventProxy>&) override
	{
		const FRuitkTileViewProps& P = static_cast<const FRuitkTileViewProps&>(Props);
		return SNew(SRuitkListView)
			.ViewKind(ERuitkItemViewKind::Tile)
			.ItemWidth(P.HasItemWidth() ? P.ItemWidth : 128.0f)
			.ItemHeight(P.HasItemHeight() ? P.ItemHeight : 128.0f);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SRuitkListView& W = static_cast<SRuitkListView&>(Widget);
		ApplyItemViewShared(W, static_cast<const FRuitkTileViewProps*>(Old), static_cast<const FRuitkTileViewProps&>(New));
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Types, factories, registration
// ─────────────────────────────────────────────────────────────────────────────────────────────

namespace Ruitk::Slate
{
	FRuitkElementTypeId ListViewType()
	{
		return Ruitk::InternElementType(FName(TEXT("ListView")));
	}
	FRuitkElementTypeId TileViewType()
	{
		return Ruitk::InternElementType(FName(TEXT("TileView")));
	}

	FRuitkNode ListView(FRuitkListViewProps Props, FRuitkKey Key)
	{
		FRuitkNode Node;
		Node.Kind = ERuitkNodeKind::Host;
		Node.ElementType = ListViewType();
		Node.Props = MakeShared<FRuitkListViewProps>(MoveTemp(Props));
		Node.Key = Key;
		return Node;
	}

	FRuitkNode TileView(FRuitkTileViewProps Props, FRuitkKey Key)
	{
		FRuitkNode Node;
		Node.Kind = ERuitkNodeKind::Host;
		Node.ElementType = TileViewType();
		Node.Props = MakeShared<FRuitkTileViewProps>(MoveTemp(Props));
		Node.Key = Key;
		return Node;
	}

	TSharedPtr<FRuitkItemRenderer> MakeItemRenderer(FRuitkItemRenderer Fn)
	{
		return MakeShared<FRuitkItemRenderer>(MoveTemp(Fn));
	}

	namespace Detail
	{
		void RegisterItemViewAdapters()
		{
			RegisterAdapter(ListViewType(), MakeUnique<FRuitkListViewAdapter>());
			RegisterAdapter(TileViewType(), MakeUnique<FRuitkTileViewAdapter>());
		}
	} // namespace Detail
} // namespace Ruitk::Slate
