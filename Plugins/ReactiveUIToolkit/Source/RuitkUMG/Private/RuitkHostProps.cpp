// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-028 — the host-props context mechanism (see RuitkHostProps.h). Mirrors the CommonUI
// activation seam: a provider component publishes the state; equality gates re-provision.

#include "RuitkHostProps.h"

#include "RuitkContext.h"

TRuitkContext<FRuitkHostPropsState>& Ruitk::Umg::HostPropsContext()
{
	static TRuitkContext<FRuitkHostPropsState> Ctx(FRuitkHostPropsState{}, FName(TEXT("RuitkHostProps")));
	return Ctx;
}

namespace
{
	struct FRuitkHostPropsProviderProps final : public FRuitkPropsBase
	{
		FRuitkHostPropsState State;

		virtual bool Equals(const FRuitkPropsBase& Other) const override
		{
			const FRuitkHostPropsProviderProps& O = static_cast<const FRuitkHostPropsProviderProps&>(Other);
			return BaseFieldsEqual(Other) && State == O.State;
		}
	};

	FRuitkNodeArray HostPropsProviderComp(FRuitkContext& Ctx, const FRuitkHostPropsProviderProps& Props,
										  const TArray<FRuitkNode>& Children)
	{
		Ctx.ProvideContext(Ruitk::Umg::HostPropsContext(), Props.State);
		return FRuitkNodeArray(Children);
	}
	RUITK_COMPONENT(HostPropsProviderComp)
} // namespace

FRuitkHostPropsState Ruitk::Umg::UseHostProps(FRuitkContext& Ctx)
{
	return Ctx.UseContext(HostPropsContext());
}

FString Ruitk::Umg::UseHostProp(FRuitkContext& Ctx, FName Name, FString Default)
{
	const FRuitkHostPropsState State = UseHostProps(Ctx);
	if (const FString* Found = State.Props.Find(Name))
	{
		return *Found;
	}
	return Default;
}

UObject* Ruitk::Umg::UseHostViewModel(FRuitkContext& Ctx)
{
	return UseHostProps(Ctx).ViewModel.Get();
}

FRuitkNode Ruitk::Umg::HostPropsProvider(FRuitkHostPropsState State, TArray<FRuitkNode> Children, FRuitkKey Key)
{
	FRuitkHostPropsProviderProps Props;
	Props.State = MoveTemp(State);
	return Ruitk::FC(&HostPropsProviderComp, MoveTemp(Props), MoveTemp(Children), Key);
}
