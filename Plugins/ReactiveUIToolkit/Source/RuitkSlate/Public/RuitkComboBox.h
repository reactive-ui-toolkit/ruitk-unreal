// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-012 tail — SComboBox<TSharedPtr<FRuitkValue>>, the dropdown selector. Same render-prop shape as
// ListView (Options + a RenderOption closure), reused for BOTH the collapsed selected-display and the
// generated dropdown rows — each is its own FRuitkRoot sub-root. SelectedIndex is CONTROLLED;
// OnSelectionChanged fires the picked index (only on a user pick, never on the programmatic set).
//
// C++-FIRST (the render closure isn't markup-expressible — like ListView). The dropdown rows generate
// only when the menu is open under geometry, so OpenMenu()/NumGeneratedRows() let the suite drive and
// verify the menu headless via the interaction harness.

#pragma once

#include "CoreMinimal.h"
#include "RuitkListView.h" // FRuitkItemRenderer + MakeItemRenderer
#include "RuitkNode.h"
#include "RuitkPropsBase.h"
#include "Widgets/SCompoundWidget.h"

template <typename OptionType> class SComboBox;
class FRuitkRoot;
class SBox;

/** SComboBox (Leaf; options are DATA). Options + RenderOption render both the selected display and the
 *  dropdown rows; SelectedIndex is controlled; OnSelectionChanged fires the picked index. */
struct RUITKSLATE_API FRuitkComboBoxProps final : public FRuitkPropsBase
{
	RUITK_PROP(TArray<TSharedPtr<FRuitkValue>>, Options, 0)
	RUITK_PROP(TSharedPtr<FRuitkItemRenderer>, RenderOption, 1)
	RUITK_PROP(int32, SelectedIndex, 2)
	RUITK_PROP_EVENT(OnSelectionChanged, 3)
	RUITK_PROPS_BODY(FRuitkComboBoxProps,
					 RUITK_EQ(Options) RUITK_EQ(RenderOption) RUITK_EQ(SelectedIndex) RUITK_EQ(OnSelectionChanged))
};

/** Wraps SComboBox<TSharedPtr<FRuitkValue>> with the render-prop plumbing + controlled selection. */
class RUITKSLATE_API SRuitkComboBox final : public SCompoundWidget
{
public:
	using FItemType = TSharedPtr<FRuitkValue>;

	SLATE_BEGIN_ARGS(SRuitkComboBox) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SRuitkComboBox() override;

	void SetOptions(TArray<FItemType> InOptions);
	void SetRenderer(TSharedPtr<FRuitkItemRenderer> InRenderer);
	void SetSelectedIndex(int32 Index);
	void SetOnSelectionChanged(FRuitkCallback InCb) { OnSelectionChanged = MoveTemp(InCb); }

	int32 GetSelectedIndex() const { return SelectedIndex; }

	/** Open the dropdown (pushes the menu). Pair with the harness's menu tick to generate rows. */
	void OpenMenu();
	/** How many dropdown-row sub-roots are currently live (after the menu has generated). */
	int32 NumGeneratedRows() const;
	/** The widget currently shown as the collapsed selected display (its sub-root's tree). */
	TSharedPtr<SWidget> GetSelectedContent() const;

	/** The backing SComboBox (tests drive a user-style selection through it, e.g. ESelectInfo::Direct). */
	TSharedPtr<SComboBox<FItemType>> GetComboWidget() const { return Combo; }

private:
	TSharedRef<SWidget> HandleGenerateRow(FItemType Item);
	void HandleSelectionChanged(FItemType Item, ESelectInfo::Type SelectInfo);
	/** Fresh row generation each open: unmount the prior open's row sub-roots (runs their cleanups)
	 *  instead of leaking them for the widget's lifetime (bughunt IW-2). */
	void HandleMenuOpening();
	void UnmountRowRoots();
	FRuitkNode BuildNodeFor(const FItemType& Item) const;
	void RefreshSelectedDisplay();

	TArray<FItemType> Options;
	TSharedPtr<FRuitkItemRenderer> Renderer;
	int32 SelectedIndex = INDEX_NONE;
	FRuitkCallback OnSelectionChanged;

	TSharedPtr<SComboBox<FItemType>> Combo;
	TSharedPtr<SBox> SelectedHolder;
	TSharedPtr<FRuitkRoot> SelectedRoot;
	TArray<TSharedPtr<FRuitkRoot>> RowRoots;
	/** Set while WE drive Combo->SetSelectedItem, so HandleSelectionChanged suppresses OnSelectionChanged
	 *  for our own programmatic set (bughunt B6 — ESelectInfo::Direct is NOT a reliable programmatic-vs-user
	 *  signal; SComboBox emits Direct for genuine user commits too). */
	bool bApplyingSelection = false;
};

namespace Ruitk::Slate
{
	RUITKSLATE_API FRuitkElementTypeId ComboBoxType();

	/** A dropdown selector. Hold Options stably; RenderOption renders the selected + each row. */
	RUITKSLATE_API FRuitkNode ComboBox(FRuitkComboBoxProps Props = FRuitkComboBoxProps(), FRuitkKey Key = FRuitkKey());

	namespace Detail
	{
		void RegisterComboBoxAdapter();
	}
} // namespace Ruitk::Slate
