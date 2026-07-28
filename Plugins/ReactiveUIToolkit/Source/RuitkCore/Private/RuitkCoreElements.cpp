// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuitkCoreElements.h"
#include "RuitkElementRegistry.h"

namespace Ruitk
{
	FRuitkElementTypeId TextBlockElementType()
	{
		static FRuitkElementTypeId Id = InternElementType(FName(TEXT("TextBlock")));
		return Id;
	}

	FRuitkNode TextBlock(FText InText, FRuitkKey Key)
	{
		TSharedRef<FRuitkTextBlockProps> Props = MakeShared<FRuitkTextBlockProps>();
		Props->SetText(MoveTemp(InText));
		FRuitkNode Node;
		Node.Kind = ERuitkNodeKind::Host;
		Node.ElementType = TextBlockElementType();
		Node.Props = Props;
		Node.Key = Key;
		return Node;
	}

	FRuitkNode TextBlock(const FString& InText, FRuitkKey Key)
	{
		return TextBlock(FText::FromString(InText), Key);
	}

	// ── Suspense (family polyfill: fallback until IsReady, poll-per-frame driver) ────────

	FRuitkNodeArray SuspenseComponent(FRuitkContext& Ctx, const FRuitkSuspenseProps& Props, const TArray<FRuitkNode>& Children)
	{
		auto [bReady, SetReady] = Ctx.UseState<bool>(false);

		// One driver per readiness source: poll each frame via the host's RequestFrame
		// until ready or torn down (the token dies with the cleanup).
		TFunction<bool()> IsReady = Props.IsReady;
		IRuitkHostConfig* Host = &Ctx.GetHost();
		Ctx.UseEffect(
			[bReady = bReady, SetReady, IsReady, Host]() -> FRuitkEffectCleanup
			{
				if (bReady || !IsReady)
				{
					return FRuitkEffectCleanup();
				}
				if (IsReady()) // already satisfied — become ready synchronously
				{
					SetReady(true);
					return FRuitkEffectCleanup();
				}
				TSharedRef<bool> Cancelled = MakeShared<bool>(false);
				// Self-re-arming per-frame poll.
				TSharedRef<TFunction<void()>> Poll = MakeShared<TFunction<void()>>();
				*Poll = [Cancelled, IsReady, SetReady, Host, Poll]()
				{
					if (*Cancelled)
					{
						return;
					}
					if (IsReady())
					{
						SetReady(true);
						return;
					}
					Host->RequestFrame(*Poll);
				};
				Host->RequestFrame(*Poll);
				return FRuitkEffectCleanup([Cancelled]() { *Cancelled = true; });
			},
			Ruitk::Deps(bReady)); // re-arm when readiness flips (and tear down the stale driver)

		if (bReady)
		{
			return FRuitkNodeArray(Children);
		}
		if (Props.Fallback.IsValid())
		{
			return FRuitkNodeArray{*Props.Fallback};
		}
		return FRuitkNodeArray();
	}

	FRuitkNode Suspense(TFunction<bool()> IsReady, FRuitkNode Fallback, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		FRuitkSuspenseProps Props;
		Props.IsReady = MoveTemp(IsReady);
		Props.Fallback = MakeShared<FRuitkNode>(MoveTemp(Fallback));
		return FC(&SuspenseComponent, MoveTemp(Props), MoveTemp(Children), Key);
	}
} // namespace Ruitk

// Direct registration (the RUITK_COMPONENT macro can't token-paste a qualified name).
static const FName GRuiSuspenseComponentId =
	Ruitk::RegisterComponentId((void*)&Ruitk::SuspenseComponent, FName(TEXT("RUI.Suspense")));
