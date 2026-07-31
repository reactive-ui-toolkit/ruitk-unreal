// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-021 — the activation context mechanism. ActivationProvider is a plain Reactive UI Toolkit component that
// provides FRuitkActivationState to its children; the Use* hooks read it. No UObject dependency here —
// this is the fully headless-testable half of the CommonUI seam.

#include "RuitkActivation.h"

#include "RuitkContext.h"

TRuitkContext<FRuitkActivationState>& Ruitk::CommonUI::ActivationContext()
{
	static TRuitkContext<FRuitkActivationState> Ctx(FRuitkActivationState{}, FName(TEXT("RuitkActivation")));
	return Ctx;
}

namespace
{
	/** Props for the provider component: the state to publish. Equality drives re-provision — a state
	 *  change re-renders the provider, which re-provides the context and dirties consumers. */
	struct FRuitkActivationProviderProps final : public FRuitkPropsBase
	{
		FRuitkActivationState State;

		virtual bool Equals(const FRuitkPropsBase& Other) const override
		{
			const FRuitkActivationProviderProps& O = static_cast<const FRuitkActivationProviderProps&>(Other);
			return BaseFieldsEqual(Other) && State == O.State;
		}
	};

	FRuitkNodeArray ActivationProviderComp(FRuitkContext& Ctx, const FRuitkActivationProviderProps& Props,
										   const TArray<FRuitkNode>& Children)
	{
		Ctx.ProvideContext(Ruitk::CommonUI::ActivationContext(), Props.State);
		return FRuitkNodeArray(Children);
	}
	RUITK_COMPONENT(ActivationProviderComp)
} // namespace

FRuitkActivationState Ruitk::CommonUI::UseActivation(FRuitkContext& Ctx)
{
	return Ctx.UseContext(ActivationContext());
}

bool Ruitk::CommonUI::UseIsActive(FRuitkContext& Ctx)
{
	return UseActivation(Ctx).bActive;
}

ERuitkInputMethod Ruitk::CommonUI::UseInputMethod(FRuitkContext& Ctx)
{
	return UseActivation(Ctx).InputMethod;
}

FRuitkNode Ruitk::CommonUI::ActivationProvider(FRuitkActivationState State, TArray<FRuitkNode> Children, FRuitkKey Key)
{
	FRuitkActivationProviderProps Props;
	Props.State = State;
	return Ruitk::FC(&ActivationProviderComp, MoveTemp(Props), MoveTemp(Children), Key);
}

// ── TD-029: desired focus ─────────────────────────────────────────────────────────────────

TRuitkContext<TSharedPtr<FRuitkFocusTargetRegistry>>& Ruitk::CommonUI::FocusTargetContext()
{
	static TRuitkContext<TSharedPtr<FRuitkFocusTargetRegistry>> Ctx(TSharedPtr<FRuitkFocusTargetRegistry>(),
																	FName(TEXT("RuitkFocusTarget")));
	return Ctx;
}

namespace
{
	struct FRuitkFocusTargetProviderProps final : public FRuitkPropsBase
	{
		TSharedPtr<FRuitkFocusTargetRegistry> Registry;

		virtual bool Equals(const FRuitkPropsBase& Other) const override
		{
			const FRuitkFocusTargetProviderProps& O = static_cast<const FRuitkFocusTargetProviderProps&>(Other);
			return BaseFieldsEqual(Other) && Registry == O.Registry;
		}
	};

	FRuitkNodeArray FocusTargetProviderComp(FRuitkContext& Ctx, const FRuitkFocusTargetProviderProps& Props,
											const TArray<FRuitkNode>& Children)
	{
		Ctx.ProvideContext(Ruitk::CommonUI::FocusTargetContext(), Props.Registry);
		return FRuitkNodeArray(Children);
	}
	RUITK_COMPONENT(FocusTargetProviderComp)
} // namespace

void Ruitk::CommonUI::UseDesiredFocus(FRuitkContext& Ctx, TFunction<void()> FocusAction)
{
	const TSharedPtr<FRuitkFocusTargetRegistry> Registry = Ctx.UseContext(FocusTargetContext());
	// One effect slot, re-run every commit: the render phase stays pure, the LATEST action wins
	// (it may capture fresh state), and the cleanup clears the designation on unmount. The weak
	// capture never extends the screen-owned registry's lifetime. Internal path: this is
	// library plumbing — the strict no-deps warning (M5) must not blame the user's component
	// for an every-commit effect it did not write.
	TWeakPtr<FRuitkFocusTargetRegistry> Weak = Registry;
	Ctx.InternalUseEffect(
		[Weak, Action = MoveTemp(FocusAction)]() -> TFunction<void()>
		{
			if (const TSharedPtr<FRuitkFocusTargetRegistry> Pinned = Weak.Pin())
			{
				Pinned->FocusDesired = Action;
			}
			return [Weak]()
			{
				if (const TSharedPtr<FRuitkFocusTargetRegistry> Pinned = Weak.Pin())
				{
					Pinned->FocusDesired = nullptr;
				}
			};
		},
		Ruitk::EveryCommit());
}

FRuitkNode Ruitk::CommonUI::FocusTargetProvider(TSharedPtr<FRuitkFocusTargetRegistry> Registry,
												TArray<FRuitkNode> Children, FRuitkKey Key)
{
	FRuitkFocusTargetProviderProps Props;
	Props.Registry = MoveTemp(Registry);
	return Ruitk::FC(&FocusTargetProviderComp, MoveTemp(Props), MoveTemp(Children), Key);
}
