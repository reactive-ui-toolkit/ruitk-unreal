// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// "Theirs inside ours": a UMG UUserWidget hosted as a Ruitk element. The adapter creates the
// widget ONCE per mounted node (CreateWidget + TakeWidget — SObjectWidget holds the strong
// UObject ref, so GC safety rides UMG's own mechanism) and replaces it when the class or
// owner changes (reconstruct mask). The PROP-MAP bridge (TD-021) applies declarative Ruitk props
// to the hosted widget's UPROPERTYs by reflection each commit (name -> FRuitkValue, type-matched),
// so a hosted UUserWidget receives Ruitk-driven data without a hand-written binding.

#pragma once

#include "CoreMinimal.h"
#include "RuitkNode.h"
#include "RuitkPropsBase.h"
#include "RuitkTypes.h" // FRuitkStyleDict
#include "Templates/SubclassOf.h"

class UUserWidget;
class UWorld;

struct RUITKUMG_API FRuitkUmgProps final : public FRuitkPropsBase
{
	RUITK_PROP(TSubclassOf<UUserWidget>, WidgetClass, 0)
	RUITK_PROP(TWeakObjectPtr<UWorld>, World, 1)
	/** name -> value, applied to the hosted widget's matching UPROPERTYs by reflection each commit. */
	RUITK_PROP(FRuitkStyleDict, WidgetProps, 2)

	virtual bool Equals(const FRuitkPropsBase& OtherBase) const override
	{
		const FRuitkUmgProps& Other = static_cast<const FRuitkUmgProps&>(OtherBase);
		if (!BaseFieldsEqual(OtherBase) || !(WidgetClass == Other.WidgetClass) || !(World == Other.World))
		{
			return false;
		}
		if (HasWidgetProps() != Other.HasWidgetProps())
		{
			return false;
		}
		return !HasWidgetProps() || WidgetProps.OrderIndependentCompareEqual(Other.WidgetProps);
	}
};

namespace Ruitk::Umg
{
	/** A UMG widget as a Ruitk node. The widget is created against World (its owning player
	 *  context); replacing WidgetClass replaces the widget. */
	RUITKUMG_API FRuitkNode UserWidget(TSubclassOf<UUserWidget> WidgetClass, UWorld* World,
									   FRuitkKey Key = FRuitkKey());

	/** As above, plus a declarative prop map applied to the hosted widget's UPROPERTYs. */
	RUITKUMG_API FRuitkNode UserWidget(TSubclassOf<UUserWidget> WidgetClass, UWorld* World, FRuitkStyleDict WidgetProps,
									   FRuitkKey Key = FRuitkKey());

	/** Apply a prop map to a widget's UPROPERTYs by reflection (int/float/bool/string/text/name,
	 *  type-matched to the FRuitkValue kind). Unknown names and type mismatches are skipped. Returns
	 *  the number of properties actually set. Public so tools/tests can drive it directly. */
	RUITKUMG_API int32 ApplyPropMap(UUserWidget* Widget, const FRuitkStyleDict& WidgetProps);

	/** Register the adapter (module startup; idempotent). */
	RUITKUMG_API void RegisterUmgAdapters();
} // namespace Ruitk::Umg
