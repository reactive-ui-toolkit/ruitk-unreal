// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-012 tail — SExpandableArea wrapper. Two persistent SBox holders back the construct-time
// HeaderContent/BodyContent named slots; the reconciler reparents role-tagged children into them.
// Expansion is controlled (SetExpanded skip-when-equal against the live state; OnExpansionChanged
// forwards the user's toggle).

#include "RuitkExpandableArea.h"

#include "RuitkElementAdapter.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/SNullWidget.h"

void SRuitkExpandableArea::Construct(const FArguments& InArgs)
{
	SAssignNew(HeaderBox, SBox);
	SAssignNew(BodyBox, SBox);

	// clang-format off
	ChildSlot
	[
		SAssignNew(Area, SExpandableArea)
		.InitiallyCollapsed(!InArgs._InitiallyExpanded)
		.OnAreaExpansionChanged(this, &SRuitkExpandableArea::HandleExpansionChanged)
		.HeaderContent()
		[
			HeaderBox.ToSharedRef()
		]
		.BodyContent()
		[
			BodyBox.ToSharedRef()
		]
	];
	// clang-format on
}

void SRuitkExpandableArea::SetExpanded(bool bExpanded)
{
	// Self-notifying skip (D-16): the widget's own toggle lands on an equal value.
	if (Area.IsValid() && Area->IsExpanded() != bExpanded)
	{
		// SExpandableArea::SetExpanded broadcasts OnAreaExpansionChanged; suppress our forward of it for
		// this PROGRAMMATIC (controlled) change so OnExpansionChanged fires only on a genuine user toggle
		// (bughunt B9 — controlled-component feedback footgun).
		bApplyingExpansion = true;
		Area->SetExpanded(bExpanded);
		bApplyingExpansion = false;
	}
}

bool SRuitkExpandableArea::IsExpanded() const
{
	return Area.IsValid() && Area->IsExpanded();
}

void SRuitkExpandableArea::SetRoleContent(FName Role, const TSharedPtr<SWidget>& Content)
{
	const TSharedRef<SWidget> Widget = Content.IsValid() ? Content.ToSharedRef() : SNullWidget::NullWidget;
	if (Role == FName(TEXT("header")))
	{
		if (HeaderBox.IsValid())
		{
			HeaderBox->SetContent(Widget);
		}
	}
	else if (BodyBox.IsValid())
	{
		BodyBox->SetContent(Widget);
	}
}

void SRuitkExpandableArea::ClearContent(const TSharedRef<SWidget>& Content)
{
	auto BoxHolds = [&Content](const TSharedPtr<SBox>& Box) -> bool
	{
		if (!Box.IsValid())
		{
			return false;
		}
		FChildren* Children = Box->GetChildren();
		return Children != nullptr && Children->Num() > 0 && &Children->GetChildAt(0).Get() == &Content.Get();
	};
	if (BoxHolds(HeaderBox))
	{
		HeaderBox->SetContent(SNullWidget::NullWidget);
	}
	else if (BoxHolds(BodyBox))
	{
		BodyBox->SetContent(SNullWidget::NullWidget);
	}
}

TSharedPtr<SWidget> SRuitkExpandableArea::GetRoleContent(FName Role) const
{
	const TSharedPtr<SBox>& Box = (Role == FName(TEXT("header"))) ? HeaderBox : BodyBox;
	if (!Box.IsValid())
	{
		return nullptr;
	}
	FChildren* Children = Box->GetChildren();
	if (Children == nullptr || Children->Num() == 0)
	{
		return nullptr;
	}
	TSharedRef<SWidget> Content = Children->GetChildAt(0);
	return Content == SNullWidget::NullWidget ? nullptr : TSharedPtr<SWidget>(Content);
}

void SRuitkExpandableArea::HandleExpansionChanged(bool bExpanded)
{
	// Only a genuine USER toggle forwards the event; a programmatic (controlled) change is suppressed
	// via bApplyingExpansion (bughunt B9).
	if (!bApplyingExpansion && OnExpansionChanged.IsBound())
	{
		OnExpansionChanged.Execute(FRuitkValue(bExpanded));
	}
}

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Adapter (two role slots)
// ─────────────────────────────────────────────────────────────────────────────────────────────

namespace
{
	/** The child's role ("header" | "body"); absent/other -> "body". */
	FName RoleOf(const FRuitkStyleDict* SlotProps)
	{
		if (SlotProps != nullptr)
		{
			if (const FRuitkValue* V = SlotProps->Find(FName(TEXT("slot.role"))))
			{
				return V->Kind == FRuitkValue::EKind::Name ? V->NameValue : FName(*V->StringValue);
			}
		}
		return FName(TEXT("body"));
	}
} // namespace

class FRuitkExpandableAreaAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::MultiSlot; }
	virtual bool IsPoolable() const override { return false; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props, const TSharedPtr<FRuitkEventProxy>&) override
	{
		const FRuitkExpandableAreaProps& P = static_cast<const FRuitkExpandableAreaProps&>(Props);
		return SNew(SRuitkExpandableArea).InitiallyExpanded(P.HasbIsExpanded() ? P.bIsExpanded : true);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SRuitkExpandableArea& W = static_cast<SRuitkExpandableArea&>(Widget);
		const FRuitkExpandableAreaProps& N = static_cast<const FRuitkExpandableAreaProps&>(New);
		const FRuitkExpandableAreaProps* O = static_cast<const FRuitkExpandableAreaProps*>(Old);
		if (N.HasbIsExpanded() && (O == nullptr || !O->HasbIsExpanded() || !(N.bIsExpanded == O->bIsExpanded)))
		{
			W.SetExpanded(N.bIsExpanded);
		}
		if (N.HasOnExpansionChanged())
		{
			W.SetOnExpansionChanged(N.OnExpansionChanged);
		}
	}

	virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32,
							 const FRuitkStyleDict* SlotProps) override
	{
		static_cast<SRuitkExpandableArea&>(Parent).SetRoleContent(RoleOf(SlotProps), Child);
	}

	virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) override
	{
		static_cast<SRuitkExpandableArea&>(Parent).ClearContent(Child);
	}

	virtual void ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
								 TFunctionRef<const FRuitkStyleDict*(const TSharedRef<SWidget>&)> SlotPropsOf) override
	{
		SRuitkExpandableArea& W = static_cast<SRuitkExpandableArea&>(Parent);
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			W.SetRoleContent(RoleOf(SlotPropsOf(Child)), Child);
		}
	}

	virtual void UpdateChildSlotProps(SWidget& Parent, const TSharedRef<SWidget>& Child,
									  const FRuitkStyleDict* SlotProps) override
	{
		SRuitkExpandableArea& W = static_cast<SRuitkExpandableArea&>(Parent);
		W.ClearContent(Child); // role may have changed — drop then re-route
		W.SetRoleContent(RoleOf(SlotProps), Child);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Type, factory, registration
// ─────────────────────────────────────────────────────────────────────────────────────────────

namespace Ruitk::Slate
{
	FRuitkElementTypeId ExpandableAreaType()
	{
		return Ruitk::InternElementType(FName(TEXT("ExpandableArea")));
	}

	FRuitkNode ExpandableArea(FRuitkExpandableAreaProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		FRuitkNode Node;
		Node.Kind = ERuitkNodeKind::Host;
		Node.ElementType = ExpandableAreaType();
		Node.Props = MakeShared<FRuitkExpandableAreaProps>(MoveTemp(Props));
		Node.Children = Ruitk::MakeChildren(MoveTemp(Children));
		Node.Key = Key;
		return Node;
	}

	namespace Detail
	{
		void RegisterExpandableAreaAdapter()
		{
			RegisterAdapter(ExpandableAreaType(), MakeUnique<FRuitkExpandableAreaAdapter>());
		}
	} // namespace Detail
} // namespace Ruitk::Slate
