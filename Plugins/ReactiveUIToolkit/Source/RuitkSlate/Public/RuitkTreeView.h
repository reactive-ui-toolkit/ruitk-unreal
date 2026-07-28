// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-022 closure (WIDGET_COMPLETION_PLAN wave 4) — the hierarchical item-model view.
// STreeView on the SRuitkListView pattern: per-row reconciler sub-roots + a GetChildren
// accessor (the piece the flat FRuitkValue item type couldn't carry) + CONTROLLED expansion
// (ExpandedItems diffed onto SetItemExpansion; OnExpansionChanged reports user toggles) +
// the P5c column protocol (a Columns list builds the SHeaderRow; construct-only).
//
// C++-FIRST like ListView (render closures are not markup-expressible — no .uetkx tag).
// Hold Items/children arrays stably (UseMemo/UseRef); re-hand RenderItem each render.

#pragma once

#include "CoreMinimal.h"
#include "RuitkListView.h" // FRuitkItemRenderer + the item-model conventions
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STreeView.h"

class FRuitkRoot;
class SRuitkTreeRow;

/** Child accessor: item -> its children (return the SAME TSharedPtrs every call — identity
 *  keys the rows and the expansion map). */
using FRuitkChildAccessor = TFunction<TArray<TSharedPtr<FRuitkValue>>(const FRuitkValue&)>;

/** One header column (P5c): construct-only on the tree (a Columns change rebuilds the header). */
struct FRuitkHeaderColumn
{
	FName Id;
	FText Label;
	float FillWidth = 1.0f;

	bool operator==(const FRuitkHeaderColumn& O) const
	{
		return Id == O.Id && FillWidth == O.FillWidth &&
			   (Label.IdenticalTo(O.Label) || Label.ToString() == O.Label.ToString());
	}
};

/** TreeView props. Expansion is CONTROLLED: ExpandedItems (by item identity) diffs onto the
 *  widget; user toggles report via OnExpansionChanged (Value = bool; pair with selection to
 *  know which item — or use the P2 handle for imperative reads). */
struct RUITKSLATE_API FRuitkTreeViewProps final : public FRuitkPropsBase
{
	RUITK_PROP(TArray<TSharedPtr<FRuitkValue>>, Items, 0) // root items
	RUITK_PROP(TSharedPtr<FRuitkItemRenderer>, RenderItem, 1)
	RUITK_PROP(TSharedPtr<FRuitkChildAccessor>, GetChildren, 2)
	RUITK_PROP(TArray<TSharedPtr<FRuitkValue>>, ExpandedItems, 3)
	RUITK_PROP(TArray<FRuitkHeaderColumn>, Columns, 4)
	RUITK_PROP(FName, SelectionMode, 5)
	RUITK_PROP_EVENT(OnSelectionChanged, 6)
	RUITK_PROP_EVENT(OnExpansionChanged, 7)
	RUITK_PROPS_BODY(FRuitkTreeViewProps, RUITK_EQ(Items) RUITK_EQ(RenderItem) RUITK_EQ(GetChildren)
											  RUITK_EQ(ExpandedItems) RUITK_EQ(Columns) RUITK_EQ(SelectionMode)
												  RUITK_EQ(OnSelectionChanged) RUITK_EQ(OnExpansionChanged))
};

/** The concrete tree widget (adapter-driven via the Set*() surface; headless helpers mirror
 *  SRuitkListView's). */
class RUITKSLATE_API SRuitkTreeView : public SCompoundWidget
{
public:
	using FItemType = TSharedPtr<FRuitkValue>;

	SLATE_BEGIN_ARGS(SRuitkTreeView) {}
	SLATE_ARGUMENT(TArray<FRuitkHeaderColumn>, Columns)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetItems(TArray<FItemType> InItems);
	void SetRenderer(TSharedPtr<FRuitkItemRenderer> InRenderer);
	void SetChildAccessor(TSharedPtr<FRuitkChildAccessor> InAccessor);
	/** Controlled expansion: expand exactly this set (collapse everything else known). */
	void SetExpandedItems(const TArray<FItemType>& InExpanded);
	void SetSelectionMode(ESelectionMode::Type InMode);
	void SetOnSelectionChanged(FRuitkCallback InCallback);
	void SetOnExpansionChanged(FRuitkCallback InCallback);

	FRuitkNode BuildNodeFor(const FItemType& Item) const;
	void TrackRow(const TSharedRef<SRuitkTreeRow>& Row);
	void ForceGenerateRows(FVector2D ViewportSize);
	int32 NumGeneratedRows() const;

	TSharedPtr<STreeView<FItemType>> GetTreeWidget() const { return TreeWidget; }

private:
	TSharedRef<class ITableRow> HandleGenerateRow(FItemType Item, const TSharedRef<class STableViewBase>& OwnerTable);
	void HandleGetChildren(FItemType Item, TArray<FItemType>& OutChildren);
	void HandleSelectionChanged(FItemType Item, ESelectInfo::Type SelectInfo);
	void HandleExpansionChanged(FItemType Item, bool bExpanded);

	TSharedPtr<STreeView<FItemType>> TreeWidget;
	TSharedPtr<SHeaderRow> Header;
	TArray<FItemType> Items;
	TSharedPtr<FRuitkItemRenderer> Renderer;
	TSharedPtr<FRuitkChildAccessor> ChildAccessor;
	TArray<FItemType> KnownExpanded; // last controlled set (for collapse diffing)
	FRuitkCallback OnSelectionChanged;
	FRuitkCallback OnExpansionChanged;
	ESelectionMode::Type SelectionModeValue = ESelectionMode::None;
	bool bApplyingExpansion = false; // suppress OnExpansionChanged for programmatic changes
	TArray<TWeakPtr<SRuitkTreeRow>> LiveRows;
};

namespace Ruitk::Slate
{
	RUITKSLATE_API FRuitkElementTypeId TreeViewType();

	/** A virtualized hierarchical tree (TD-022). C++-first, like ListView. */
	RUITKSLATE_API FRuitkNode TreeView(FRuitkTreeViewProps Props = FRuitkTreeViewProps(), FRuitkKey Key = FRuitkKey());

	/** Wrap a child accessor ONCE (UseMemo/UseRef it) — identity participates in props equality. */
	RUITKSLATE_API TSharedPtr<FRuitkChildAccessor> MakeChildAccessor(FRuitkChildAccessor Fn);

	namespace Detail
	{
		void RegisterTreeViewAdapter();
	}
} // namespace Ruitk::Slate
