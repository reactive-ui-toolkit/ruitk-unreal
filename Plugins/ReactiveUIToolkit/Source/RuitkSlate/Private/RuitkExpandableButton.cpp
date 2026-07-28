// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuitkExpandableButton.h"

#include "RuitkEventProxy.h"
#include "Widgets/SNullWidget.h"

void SRuitkExpandableButton::Construct(const FArguments& InArgs)
{
	SAssignNew(CollapsedBox, SBox);
	SAssignNew(ExpandedBox, SBox);
	SAssignNew(BodyBox, SBox);

	// clang-format off
	ChildSlot
	[
		SAssignNew(Button, SExpandableButton)
		.CollapsedText(InArgs._CollapsedText)
		.ExpandedText(InArgs._ExpandedText)
		.IsExpanded(InArgs._IsExpanded)
		.OnExpansionClicked(InArgs._OnExpansionClicked)
		.OnCloseClicked(InArgs._OnCloseClicked)
		.CollapsedButtonContent()
		[
			CollapsedBox.ToSharedRef()
		]
		.ExpandedButtonContent()
		[
			ExpandedBox.ToSharedRef()
		]
		.ExpandedChildContent()
		[
			BodyBox.ToSharedRef()
		]
	];
	// clang-format on
}

void SRuitkExpandableButton::SetRoleContent(FName Role, const TSharedPtr<SWidget>& Content)
{
	const TSharedRef<SWidget> W = Content.IsValid() ? Content.ToSharedRef() : SNullWidget::NullWidget;
	if (Role == FName(TEXT("collapsed")))
	{
		CollapsedBox->SetContent(W);
	}
	else if (Role == FName(TEXT("expanded")))
	{
		ExpandedBox->SetContent(W);
	}
	else
	{
		BodyBox->SetContent(W);
	}
}

void SRuitkExpandableButton::ClearContent(const TSharedRef<SWidget>& Content)
{
	for (const TSharedPtr<SBox>& Holder : {CollapsedBox, ExpandedBox, BodyBox})
	{
		if (Holder->GetChildren()->Num() > 0 && &Holder->GetChildren()->GetChildAt(0).Get() == &Content.Get())
		{
			Holder->SetContent(SNullWidget::NullWidget);
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────────────────

namespace
{
	FName ButtonRoleOf(const FRuitkStyleDict* SlotProps)
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

	class FRuitkExpandableButtonAdapter final : public IRuitkElementAdapter
	{
	public:
		virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::MultiSlot; }
		virtual bool HasEvents() const override { return true; }

		virtual uint64 GetReconstructMask() const override
		{
			return (1ull << FRuitkExpandableButtonProps::CollapsedText_Bit) |
				   (1ull << FRuitkExpandableButtonProps::ExpandedText_Bit) |
				   (1ull << FRuitkExpandableButtonProps::bIsExpanded_Bit);
		}

		virtual bool ConstructOnlyChanged(const FRuitkPropsBase& Old, const FRuitkPropsBase& New) const override
		{
			const FRuitkExpandableButtonProps& O = static_cast<const FRuitkExpandableButtonProps&>(Old);
			const FRuitkExpandableButtonProps& N = static_cast<const FRuitkExpandableButtonProps&>(New);
			auto TextChanged = [](bool bNewHas, bool bOldHas, const FText& OldV, const FText& NewV)
			{ return bNewHas && (!bOldHas || !(NewV.IdenticalTo(OldV) || NewV.ToString() == OldV.ToString())); };
			return TextChanged(N.HasCollapsedText(), O.HasCollapsedText(), O.CollapsedText, N.CollapsedText) ||
				   TextChanged(N.HasExpandedText(), O.HasExpandedText(), O.ExpandedText, N.ExpandedText) ||
				   (N.HasbIsExpanded() && (!O.HasbIsExpanded() || O.bIsExpanded != N.bIsExpanded));
		}

		virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props,
												 const TSharedPtr<FRuitkEventProxy>& Proxy) override
		{
			const FRuitkExpandableButtonProps& P = static_cast<const FRuitkExpandableButtonProps&>(Props);
			return SNew(SRuitkExpandableButton)
				.CollapsedText(P.CollapsedText)
				.ExpandedText(P.ExpandedText)
				.IsExpanded(!P.HasbIsExpanded() || P.bIsExpanded)
				.OnExpansionClicked(
					FOnClicked::CreateSP(Proxy.ToSharedRef(), &FRuitkEventProxy::HandleReply,
										 static_cast<int32>(FRuitkExpandableButtonProps::OnExpansionClicked_Bit)))
				.OnCloseClicked(
					FOnClicked::CreateSP(Proxy.ToSharedRef(), &FRuitkEventProxy::HandleReply,
										 static_cast<int32>(FRuitkExpandableButtonProps::OnCloseClicked_Bit)));
		}

		virtual void SyncEventHandlers(FRuitkEventProxy& Proxy, const FRuitkPropsBase& New) override
		{
			const FRuitkExpandableButtonProps& N = static_cast<const FRuitkExpandableButtonProps&>(New);
			Proxy.SetHandler(static_cast<int32>(FRuitkExpandableButtonProps::OnExpansionClicked_Bit),
							 N.OnExpansionClicked);
			Proxy.SetHandler(static_cast<int32>(FRuitkExpandableButtonProps::OnCloseClicked_Bit), N.OnCloseClicked);
		}

		virtual void ApplyDiff(SWidget&, const FRuitkPropsBase*, const FRuitkPropsBase&) override {} // all masked

		virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32,
								 const FRuitkStyleDict* SlotProps) override
		{
			static_cast<SRuitkExpandableButton&>(Parent).SetRoleContent(ButtonRoleOf(SlotProps), Child);
		}

		virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) override
		{
			static_cast<SRuitkExpandableButton&>(Parent).ClearContent(Child);
		}

		virtual void
		ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
						TFunctionRef<const FRuitkStyleDict*(const TSharedRef<SWidget>&)> SlotPropsOf) override
		{
			SRuitkExpandableButton& W = static_cast<SRuitkExpandableButton&>(Parent);
			for (const TSharedRef<SWidget>& Child : Ordered)
			{
				W.SetRoleContent(ButtonRoleOf(SlotPropsOf(Child)), Child);
			}
		}

		virtual void UpdateChildSlotProps(SWidget& Parent, const TSharedRef<SWidget>& Child,
										  const FRuitkStyleDict* SlotProps) override
		{
			static_cast<SRuitkExpandableButton&>(Parent).SetRoleContent(ButtonRoleOf(SlotProps), Child);
		}
	};
} // namespace

namespace Ruitk::Slate
{
	FRuitkElementTypeId ExpandableButtonType()
	{
		return Ruitk::InternElementType(FName(TEXT("ExpandableButton")));
	}

	FRuitkNode ExpandableButton(FRuitkExpandableButtonProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		FRuitkNode Node;
		Node.Kind = ERuitkNodeKind::Host;
		Node.ElementType = ExpandableButtonType();
		Node.Props = MakeShared<FRuitkExpandableButtonProps>(MoveTemp(Props));
		Node.Children = Ruitk::MakeChildren(MoveTemp(Children));
		Node.Key = Key;
		return Node;
	}

	namespace Detail
	{
		void RegisterExpandableButtonAdapter()
		{
			RegisterAdapter(ExpandableButtonType(), MakeUnique<FRuitkExpandableButtonAdapter>());
		}
	} // namespace Detail
} // namespace Ruitk::Slate
