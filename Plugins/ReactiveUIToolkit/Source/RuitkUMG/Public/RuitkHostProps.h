// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-028 — the designer/Blueprint → component channel for the "ours in theirs" door.
// URuitkHostWidget publishes its BP-set initial props (a name→string map) and its optional
// FieldNotify viewmodel into the hosted tree through this context; components read them with
// the hooks below. Same mechanism as the CommonUI activation seam (plain Reactive UI Toolkit context —
// provider node + Use* hooks), so it is headless-testable without a UMG designer in the loop.
//
//   URuitkHostWidget      --provides-->  FRuitkHostPropsState (via HostPropsProvider)
//   your component      --reads----->  UseHostProp / UseHostProps / UseHostViewModel
//
// The viewmodel arrives as a plain UObject* — hand it straight to Ruitk::Umg::UseField:
//   UObject* Vm = Ruitk::Umg::UseHostViewModel(Ctx);
//   const int32 Health = Ruitk::Umg::UseField<int32>(Ctx, Vm, "Health", 0);

#pragma once

#include "CoreMinimal.h"
#include "RuitkContextHandle.h" // TRuitkContext
#include "RuitkNode.h"
#include "UObject/WeakObjectPtr.h"

class FRuitkContext;

/** What a hosted tree can learn from its URuitkHostWidget host. */
struct RUITKUMG_API FRuitkHostPropsState
{
	/** Designer/BP-set initial props (stringly typed — the generic channel; parse in the component). */
	TMap<FName, FString> Props;
	/** The host-provided FieldNotify viewmodel (weak — the host UPROPERTY owns the strong ref). */
	TWeakObjectPtr<UObject> ViewModel;

	bool operator==(const FRuitkHostPropsState& Other) const
	{
		return ViewModel == Other.ViewModel && Props.OrderIndependentCompareEqual(Other.Props);
	}
	bool operator!=(const FRuitkHostPropsState& Other) const { return !(*this == Other); }
};

namespace Ruitk::Umg
{
	/** The context handle carrying host props to descendants (default = empty / no viewmodel). */
	RUITKUMG_API TRuitkContext<FRuitkHostPropsState>& HostPropsContext();

	/** The full host-props state at this point in the tree. */
	RUITKUMG_API FRuitkHostPropsState UseHostProps(FRuitkContext& Ctx);

	/** Sugar: one named host prop, or `Default` when the host didn't set it. */
	RUITKUMG_API FString UseHostProp(FRuitkContext& Ctx, FName Name, FString Default = FString());

	/** Sugar: the host-provided viewmodel (nullptr when unset/collected) — feed it to UseField. */
	RUITKUMG_API UObject* UseHostViewModel(FRuitkContext& Ctx);

	/** Provide `State` to `Children` (what URuitkHostWidget wraps the hosted component in). */
	RUITKUMG_API FRuitkNode HostPropsProvider(FRuitkHostPropsState State,
											  TArray<FRuitkNode> Children = TArray<FRuitkNode>(),
											  FRuitkKey Key = FRuitkKey());
} // namespace Ruitk::Umg
