// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-012 tail — SSegmentedControl, the labelled tab-bar / radio-group selector. `Labels` bakes the
// segments (SSegmentedControl has no clear-children API, so a label-set change is CONSTRUCT-only —
// the reconstruct mask replaces the widget); `SelectedIndex` is a CONTROLLED runtime prop applied
// skip-when-equal against the widget's live value (D-16). OnSelectionChanged fires the segment index
// when the user picks one. Headless-safe: SetSelectedIndex/GetSelectedIndex are directly exercisable.

#pragma once

#include "CoreMinimal.h"
#include "RuitkNode.h"
#include "RuitkPropsBase.h"
#include "Widgets/SCompoundWidget.h"

template <typename OptionType> class SSegmentedControl;

/** SSegmentedControl<int32> (Leaf): one text segment per `Labels` entry (value = index).
 *  `SelectedIndex` is the controlled selection; OnSelectionChanged fires the picked index. */
struct RUITKSLATE_API FRuitkSegmentedControlProps final : public FRuitkPropsBase
{
	RUITK_PROP(TArray<FString>, Labels, 0) // construct-only (segments bake)
	RUITK_PROP(int32, SelectedIndex, 1)	 // controlled runtime
	RUITK_PROP_EVENT(OnSelectionChanged, 2)
	RUITK_PROPS_BODY(FRuitkSegmentedControlProps, RUITK_EQ(Labels) RUITK_EQ(SelectedIndex) RUITK_EQ(OnSelectionChanged))
};

/** Wraps SSegmentedControl<int32> with a stable callback holder + the controlled-selection surface. */
class RUITKSLATE_API SRuitkSegmentedControl final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRuitkSegmentedControl) : _InitialIndex(0) {}
	SLATE_ARGUMENT(TArray<FString>, Labels)
	SLATE_ARGUMENT(int32, InitialIndex)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetSelectedIndex(int32 Index);
	int32 GetSelectedIndex() const;
	int32 NumSegments() const;
	void SetOnSelectionChanged(FRuitkCallback InCb) { OnSelectionChanged = MoveTemp(InCb); }

private:
	void HandleValueChanged(int32 Value);

	TSharedPtr<SSegmentedControl<int32>> Control;
	FRuitkCallback OnSelectionChanged;
};

namespace Ruitk::Slate
{
	RUITKSLATE_API FRuitkElementTypeId SegmentedControlType();

	/** A labelled segmented selector (tab bar). Labels bake the segments; SelectedIndex is controlled. */
	RUITKSLATE_API FRuitkNode SegmentedControl(FRuitkSegmentedControlProps Props = FRuitkSegmentedControlProps(),
												  FRuitkKey Key = FRuitkKey());

	namespace Detail
	{
		void RegisterSegmentedControlAdapter();
	}
} // namespace Ruitk::Slate
