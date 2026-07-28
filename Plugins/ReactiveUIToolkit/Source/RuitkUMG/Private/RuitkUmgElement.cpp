// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuitkUmgElement.h"

#include "Blueprint/UserWidget.h"
#include "RuitkElementAdapter.h"
#include "RuitkMarshal.h"
#include "RuitkSlateHost.h"
#include "Slate/SObjectWidget.h"
#include "UObject/UnrealType.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

int32 Ruitk::Umg::ApplyPropMap(UUserWidget* Widget, const FRuitkStyleDict& WidgetProps)
{
	if (Widget == nullptr || WidgetProps.Num() == 0)
	{
		return 0;
	}
	// One conversion table for every seam: the per-property dispatch lives in RuitkMarshal
	// (MarshalToProperty — B13 kind-validation rules preserved there verbatim).
	int32 Applied = 0;
	for (const TPair<FName, FRuitkValue>& Pair : WidgetProps)
	{
		if (Ruitk::Umg::MarshalToProperty(Widget, Pair.Key, Pair.Value))
		{
			++Applied;
		}
	}
	// Push the new values into the widget's Slate representation — only once it has one (a hosted
	// widget after TakeWidget). Skips safely for a bare, un-constructed widget (direct tool/test use).
	if (Applied > 0 && Widget->GetCachedWidget().IsValid())
	{
		Widget->SynchronizeProperties();
	}
	return Applied;
}

namespace
{
	class FRuitkUmgAdapter final : public IRuitkElementAdapter
	{
	public:
		virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }

		virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& PropsBase,
												 const TSharedPtr<FRuitkEventProxy>&) override
		{
			const FRuitkUmgProps& Props = static_cast<const FRuitkUmgProps&>(PropsBase);
			UWorld* World = Props.World.Get();
			UClass* WidgetClass = Props.WidgetClass.Get();
			if (!World || !WidgetClass)
			{
				return SNew(STextBlock).Text(NSLOCTEXT("Ruitk", "UmgMissing", "<UMG: class/world unset>"));
			}
			UUserWidget* Widget = ::CreateWidget<UUserWidget>(World, TSubclassOf<UUserWidget>(WidgetClass));
			if (!Widget)
			{
				return SNew(STextBlock).Text(NSLOCTEXT("Ruitk", "UmgFailed", "<UMG: CreateWidget failed>"));
			}
			// Apply the initial prop map, then TakeWidget -> SObjectWidget (holds the strong UObject
			// ref: the hosted widget lives exactly as long as its Slate representation, UMG's contract).
			Ruitk::Umg::ApplyPropMap(Widget, Props.WidgetProps);
			return Widget->TakeWidget();
		}

		virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
		{
			// Re-apply the prop map on change: recover the hosted UUserWidget from its SObjectWidget.
			const FRuitkUmgProps& N = static_cast<const FRuitkUmgProps&>(New);
			const FRuitkUmgProps* O = static_cast<const FRuitkUmgProps*>(Old);
			if (!N.HasWidgetProps())
			{
				return;
			}
			if (O != nullptr && O->HasWidgetProps() && N.WidgetProps.OrderIndependentCompareEqual(O->WidgetProps))
			{
				return;
			}
			if (Widget.GetType() == FName(TEXT("SObjectWidget")))
			{
				if (UUserWidget* Hosted = static_cast<SObjectWidget&>(Widget).GetWidgetObject())
				{
					Ruitk::Umg::ApplyPropMap(Hosted, N.WidgetProps);
				}
			}
		}

		virtual uint64 GetReconstructMask() const override
		{
			return 0b11; // WidgetClass + World are construct-only; WidgetProps applies in place
		}

		virtual bool ConstructOnlyChanged(const FRuitkPropsBase& Old, const FRuitkPropsBase& New) const override
		{
			const FRuitkUmgProps& O = static_cast<const FRuitkUmgProps&>(Old);
			const FRuitkUmgProps& N = static_cast<const FRuitkUmgProps&>(New);
			return !(O.WidgetClass == N.WidgetClass) || !(O.World == N.World);
		}

		virtual bool IsPoolable() const override { return false; } // carries a live UObject
	};

	FRuitkElementTypeId UmgElementType()
	{
		static const FRuitkElementTypeId Id = Ruitk::InternElementType(FName(TEXT("UmgUserWidget")));
		return Id;
	}
} // namespace

namespace Ruitk::Umg
{
	void RegisterUmgAdapters()
	{
		static bool bOnce = false;
		if (bOnce)
		{
			return;
		}
		bOnce = true;
		Ruitk::Slate::RegisterAdapter(UmgElementType(), MakeUnique<FRuitkUmgAdapter>());
	}

	static FRuitkNode MakeUmgNode(TSubclassOf<UUserWidget> WidgetClass, UWorld* World, FRuitkStyleDict WidgetProps,
								FRuitkKey Key)
	{
		RegisterUmgAdapters();
		FRuitkUmgProps Props;
		Props.SetWidgetClass(MoveTemp(WidgetClass));
		Props.SetWorld(World);
		if (WidgetProps.Num() > 0)
		{
			Props.SetWidgetProps(MoveTemp(WidgetProps));
		}
		FRuitkNode Node;
		Node.Kind = ERuitkNodeKind::Host;
		Node.ElementType = UmgElementType();
		Node.Props = MakeShared<FRuitkUmgProps>(MoveTemp(Props));
		Node.Key = Key;
		return Node;
	}

	FRuitkNode UserWidget(TSubclassOf<UUserWidget> WidgetClass, UWorld* World, FRuitkKey Key)
	{
		return MakeUmgNode(MoveTemp(WidgetClass), World, FRuitkStyleDict(), Key);
	}

	FRuitkNode UserWidget(TSubclassOf<UUserWidget> WidgetClass, UWorld* World, FRuitkStyleDict WidgetProps, FRuitkKey Key)
	{
		return MakeUmgNode(MoveTemp(WidgetClass), World, MoveTemp(WidgetProps), Key);
	}
} // namespace Ruitk::Umg
