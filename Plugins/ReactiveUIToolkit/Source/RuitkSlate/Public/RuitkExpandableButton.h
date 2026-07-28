// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ExpandableButton (WIDGET_COMPLETION_PLAN wave 2) — SExpandableButton on the SRuitkExpandableArea
// wrapper pattern: the engine widget's three content slots are CONSTRUCT-ONLY named slots, so
// the wrapper feeds them persistent SBox holders and the reconciler reparents role-tagged
// children into the holders (`slot.role = "collapsed" | "expanded" | body`). Texts + expansion
// are engine ATTRIBUTES with no setters — the whole prop surface rides the reconstruct mask.

#pragma once

#include "CoreMinimal.h"
#include "RuitkElementAdapter.h"
#include "RuitkSlateElements.h"
#include "Widgets/Input/SExpandableButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SCompoundWidget.h"

/** Props: label texts + controlled expansion (all masked — no engine setters) + the two
 *  FReply click events. */
struct RUITKSLATE_API FRuitkExpandableButtonProps final : public FRuitkPropsBase
{
	RUITK_PROP(FText, CollapsedText, 0)
	RUITK_PROP(FText, ExpandedText, 1)
	RUITK_PROP(bool, bIsExpanded, 2)
	RUITK_PROP_EVENT(OnExpansionClicked, 3)
	RUITK_PROP_EVENT(OnCloseClicked, 4)

	virtual bool Equals(const FRuitkPropsBase& OtherBase) const override
	{
		const FRuitkExpandableButtonProps& Other = static_cast<const FRuitkExpandableButtonProps&>(OtherBase);
		auto TextEq = [](const FText& A, const FText& B) { return A.IdenticalTo(B) || A.ToString() == B.ToString(); };
		return BaseFieldsEqual(Other) && TextEq(CollapsedText, Other.CollapsedText) &&
			   TextEq(ExpandedText, Other.ExpandedText) && bIsExpanded == Other.bIsExpanded &&
			   OnExpansionClicked == Other.OnExpansionClicked && OnCloseClicked == Other.OnCloseClicked;
	}
};

/** Wraps SExpandableButton with three SBox holders the reconciler reparents children into. */
class RUITKSLATE_API SRuitkExpandableButton final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRuitkExpandableButton) : _IsExpanded(true) {}
	SLATE_ARGUMENT(FText, CollapsedText)
	SLATE_ARGUMENT(FText, ExpandedText)
	SLATE_ARGUMENT(bool, IsExpanded)
	SLATE_EVENT(FOnClicked, OnExpansionClicked)
	SLATE_EVENT(FOnClicked, OnCloseClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Route a child into a holder by role ("collapsed" | "expanded" | anything-else = body). */
	void SetRoleContent(FName Role, const TSharedPtr<SWidget>& Content);
	/** Clear whichever holder currently shows `Content`. */
	void ClearContent(const TSharedRef<SWidget>& Content);

private:
	TSharedPtr<SExpandableButton> Button;
	TSharedPtr<SBox> CollapsedBox;
	TSharedPtr<SBox> ExpandedBox;
	TSharedPtr<SBox> BodyBox;
};

namespace Ruitk::Slate
{
	RUITKSLATE_API FRuitkElementTypeId ExpandableButtonType();

	/** An expanding button. Optional children carry `slot.role="collapsed"` / `"expanded"`
	 *  (button faces); any other child is the expanded body content. */
	RUITKSLATE_API FRuitkNode ExpandableButton(FRuitkExpandableButtonProps Props = FRuitkExpandableButtonProps(),
											   TArray<FRuitkNode> Children = TArray<FRuitkNode>(),
											   FRuitkKey Key = FRuitkKey());

	namespace Detail
	{
		void RegisterExpandableButtonAdapter();
	}
} // namespace Ruitk::Slate
