// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuitkPresence.h"

#include "RuitkContext.h"
#include "RuitkContextHandle.h"
#include "RuitkCoreMisc.h"
#include "RuitkHostConfig.h"
#include "RuitkPropsBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuiPresence, Log, All);

// ─────────────────────────────────────────────────────────────────────────────────────────
// The single context handle both the boundary provides on and UsePresence reads. A file-local
// static so the Key() (its Core shared-ptr address) is identical for provide + read.
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace
{
	const TRuitkContext<FRuitkPresenceState>& PresenceContext()
	{
		static const TRuitkContext<FRuitkPresenceState> Handle(FRuitkPresenceState{true, FRuitkCallback()},
														   FName(TEXT("RuitkPresence")));
		return Handle;
	}

	/** Positional-index fallback key for an unkeyed child. MUST use the reconciler's namespaced
	 *  sentinel (a control-char FName that can never equal a user key), not a raw FRuitkKey(Index) —
	 *  the latter is byte-identical to a user's integer key FRuitkKey(Index), so an unkeyed child and an
	 *  integer-keyed child at the same value would collapse to one identity (bughunt B1: silent child
	 *  drop / mis-attributed exit tracking). Mirrors FiberKey/VNodeKey in RuitkReconciler.cpp. */
	FRuitkKey KeyForChild(const FRuitkNode& Child, int32 Index)
	{
		return Child.Key.IsSet() ? Child.Key : FRuitkKey(FName(*FString::Printf(TEXT("\x01idx%d"), Index)));
	}

	// ── PresenceChild: wraps ONE kept child, owns its bPresent context + exit timeout ──────

	struct FRuitkPresenceChildProps final : public FRuitkPropsBase
	{
		RUITK_PROP(bool, bPresent, 0)
		RUITK_PROP(float, MaxExitSeconds, 1)
		RUITK_PROP_EVENT(OnExited, 2) // the boundary's "drop my key" — fired by NotifyDone or timeout
		RUITK_PROPS_BODY(FRuitkPresenceChildProps, RUITK_EQ(bPresent) RUITK_EQ(MaxExitSeconds))
	};

	/** Self-re-arming host-clock timeout: fires OnExited once elapsed >= MaxSeconds, unless the
	 *  Cancel flag (flipped by the effect cleanup on unmount / re-entry) got set first. */
	void PollExitTimeout(IRuitkHostConfig& Host, TSharedRef<bool> Cancel, double StartTime, float MaxSeconds,
						 FRuitkCallback OnExited)
	{
		if (*Cancel)
		{
			return;
		}
		if (Host.GetTimeSeconds() - StartTime >= MaxSeconds)
		{
			OnExited.Execute();
			return;
		}
		Host.RequestFrame([&Host, Cancel, StartTime, MaxSeconds, OnExited]()
						  { PollExitTimeout(Host, Cancel, StartTime, MaxSeconds, OnExited); });
	}

	FRuitkNodeArray PresenceChildComp(FRuitkContext& Ctx, const FRuitkPresenceChildProps& Props,
									const TArray<FRuitkNode>& Children)
	{
		const bool bPresent = Props.bPresent;
		const FRuitkCallback OnExited = Props.OnExited;

		// Once-guard so NotifyDone AND the timeout can't double-fire the drop. Reset whenever the
		// child is present again (a re-entry re-arms a future exit).
		TSharedRef<TRuitkRef<bool>> DoneRef = Ctx.UseRef<bool>(false);
		if (bPresent)
		{
			DoneRef->Current = false;
		}

		// Stable identity (so the provided context only "changes" when bPresent flips, not every
		// render) but the body — and thus the captured latest OnExited — refreshes each render.
		FRuitkCallback NotifyDone = Ctx.UseStableCallback(
			[DoneRef, OnExited]()
			{
				if (DoneRef->Current)
				{
					return;
				}
				DoneRef->Current = true;
				OnExited.Execute();
			});

		Ctx.ProvideContext(PresenceContext(), FRuitkPresenceState{bPresent, NotifyDone});

		// Timeout fence: while exiting, force the drop after MaxExitSeconds if nobody notifies.
		IRuitkHostConfig* Host = &Ctx.GetHost();
		const float MaxExit = Props.MaxExitSeconds;
		Ctx.UseEffect(
			[bPresent, Host, MaxExit, OnExited]() -> FRuitkEffectCleanup
			{
				if (bPresent)
				{
					return FRuitkEffectCleanup();
				}
				TSharedRef<bool> Cancel = MakeShared<bool>(false);
				PollExitTimeout(*Host, Cancel, Host->GetTimeSeconds(), MaxExit, OnExited);
				return [Cancel]() { *Cancel = true; };
			},
			Ruitk::Deps(bPresent ? 0 : 1));

		return Children; // render the wrapped child unchanged (context reaches it as our descendant)
	}
	RUITK_COMPONENT(PresenceChildComp)

	// ── Presence: the boundary that remembers exiting keys and keeps them mounted ──────────

	struct FRuitkPresenceProps final : public FRuitkPropsBase
	{
		RUITK_PROP(float, MaxExitSeconds, 0)
		RUITK_PROPS_BODY(FRuitkPresenceProps, RUITK_EQ(MaxExitSeconds))
	};

	/** One remembered child: its key, its last-seen vnode, and whether it is currently leaving. */
	struct FPresenceSlot
	{
		FRuitkKey Key;
		FRuitkNode Vnode;
		bool bExiting = false;
	};

	FRuitkNodeArray PresenceComp(FRuitkContext& Ctx, const FRuitkPresenceProps& Props, const TArray<FRuitkNode>& Children)
	{
		TSharedRef<TRuitkRef<TArray<FPresenceSlot>>> SlotsRef = Ctx.UseRef<TArray<FPresenceSlot>>();
		// A version bump is the ONLY reason Presence itself re-renders (a completed exit drops a
		// key); entering an exit is driven by the PARENT re-rendering with new children.
		TTuple<int32, TRuitkSetter<int32>> Version = Ctx.UseState<int32>(0);
		const TRuitkSetter<int32>& SetVersion = Version.Value;
		// Unconditional (rules of hooks): a one-shot latch for the unkeyed-child warning below.
		TSharedRef<TRuitkRef<bool>> Warned = Ctx.UseRef<bool>(false);

		// Incoming key -> its index in Children (insertion order preserved).
		TMap<FRuitkKey, int32> IncomingIndex;
		IncomingIndex.Reserve(Children.Num());
		bool bAnyUnkeyed = false;
		for (int32 i = 0; i < Children.Num(); ++i)
		{
			if (!Children[i].Key.IsSet())
			{
				bAnyUnkeyed = true;
			}
			IncomingIndex.Add(KeyForChild(Children[i], i), i);
		}
		if (bAnyUnkeyed && !Warned->Current)
		{
			Warned->Current = true;
			UE_LOG(LogRuiPresence, Warning,
				   TEXT("Ruitk::Presence child without a key: exit tracking falls back to position. "
						"Key every direct child of <Presence>."));
		}

		TArray<FPresenceSlot>& Slots = SlotsRef->Current;
		TArray<FPresenceSlot> Next;
		Next.Reserve(FMath::Max(Slots.Num(), Children.Num()));
		TSet<FRuitkKey> Emitted;

		// 1. Existing slots keep their order. Present -> refresh vnode + clear exiting (this also
		//    CANCELS an in-flight exit on re-entry). Absent -> mark exiting, keep the old vnode.
		for (FPresenceSlot& Slot : Slots)
		{
			if (const int32* Idx = IncomingIndex.Find(Slot.Key))
			{
				Slot.Vnode = Children[*Idx];
				Slot.bExiting = false;
			}
			else
			{
				Slot.bExiting = true;
			}
			Emitted.Add(Slot.Key);
			Next.Add(Slot);
		}
		// 2. Brand-new incoming keys append in incoming order.
		for (int32 i = 0; i < Children.Num(); ++i)
		{
			const FRuitkKey Key = KeyForChild(Children[i], i);
			if (!Emitted.Contains(Key))
			{
				Emitted.Add(Key);
				Next.Add(FPresenceSlot{Key, Children[i], false});
			}
		}
		Slots = MoveTemp(Next);

		// 3. Render each slot through a PresenceChild wrapper (keyed by the child's key so the
		//    fiber — and the child's tween state — is preserved for the whole enter/exit/re-entry).
		const float MaxExit = Props.MaxExitSeconds;
		FRuitkNodeArray Out;
		Out.Reserve(Slots.Num());
		for (const FPresenceSlot& Slot : Slots)
		{
			const FRuitkKey Key = Slot.Key;
			FRuitkPresenceChildProps ChildProps;
			ChildProps.SetbPresent(!Slot.bExiting);
			ChildProps.SetMaxExitSeconds(MaxExit);
			ChildProps.SetOnExited(FRuitkCallback::Create(
				[SlotsRef, SetVersion, Key]()
				{
					TArray<FPresenceSlot>& Live = SlotsRef->Current;
					const int32 Removed = Live.RemoveAll([&Key](const FPresenceSlot& S) { return S.Key == Key; });
					if (Removed > 0)
					{
						SetVersion([](const int32& V) { return V + 1; });
					}
				}));

			Out.Add(Ruitk::FC<FRuitkPresenceChildProps>(&PresenceChildComp, MoveTemp(ChildProps),
													TArray<FRuitkNode>{Slot.Vnode}, Key));
		}
		return Out;
	}
	RUITK_COMPONENT(PresenceComp)
} // namespace

FRuitkNode Ruitk::Presence(TArray<FRuitkNode> Children, float MaxExitSeconds, FRuitkKey Key)
{
	FRuitkPresenceProps Props;
	Props.SetMaxExitSeconds(MaxExitSeconds);
	return Ruitk::FC<FRuitkPresenceProps>(&PresenceComp, MoveTemp(Props), MoveTemp(Children), Key);
}

FRuitkPresenceState UsePresence(FRuitkContext& Ctx)
{
	return Ctx.UseContext(PresenceContext());
}
