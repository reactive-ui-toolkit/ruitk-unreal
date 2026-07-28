// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TRuitkSignal<T> — reactive value stores OUTSIDE the component tree (signal_store.gd /
// signal_registry.gd), plus the UseSignal/UseSignalKey hooks implemented with React's
// useSyncExternalStore discipline (D-08.5): render reads the snapshot; SUBSCRIPTION happens
// in a passive effect with a post-subscribe re-check — restart-safe by construction, which
// fixes the Godot port's subscribe-during-render leak window.

#pragma once

#include "CoreMinimal.h"
#include "RuitkContext.h"

/** Type-erased base so the FName registry can hold heterogeneous signals. */
class RUITKCORE_API FRuitkSignalBase
{
public:
	virtual ~FRuitkSignalBase() = default;
};

template <typename T> class TRuitkSignal final : public FRuitkSignalBase
{
public:
	explicit TRuitkSignal(T Initial = T()) : Value(MoveTemp(Initial)) {}

	const T& Get() const { return Value; }

	/** Set + notify on change. Change detection: operator== (value semantics — C++ arrays/
	 *  maps are values; wrap in TSharedPtr for identity semantics, §5 deps decision). */
	void Set(T NewValue)
	{
		if (Value == NewValue)
		{
			return;
		}
		Value = MoveTemp(NewValue);
		// Copy: a subscriber may unsubscribe during notify (family rule).
		TArray<TPair<int32, TFunction<void()>>> Copy = Subs;
		for (const TPair<int32, TFunction<void()>>& Sub : Copy)
		{
			Sub.Value();
		}
	}

	/** Functional update. */
	void Update(TFunction<T(const T&)> Fn) { Set(Fn(Value)); }

	/** Subscribe; returns an unsubscribe function. */
	TFunction<void()> Subscribe(TFunction<void()> OnChanged)
	{
		const int32 Id = NextSubId++;
		Subs.Emplace(Id, MoveTemp(OnChanged));
		TWeakPtr<int32> Alive = AliveToken;
		return [this, Id, Alive]()
		{
			if (Alive.IsValid()) // signal itself may be gone — the token guards the `this`
			{
				Subs.RemoveAll([Id](const TPair<int32, TFunction<void()>>& S) { return S.Key == Id; });
			}
		};
	}

	int32 NumSubscribers() const { return Subs.Num(); }

private:
	T Value;
	TArray<TPair<int32, TFunction<void()>>> Subs;
	int32 NextSubId = 0;
	TSharedRef<int32> AliveToken = MakeShared<int32>(0);
};

/** Process-wide FName-keyed shared signals (signal_registry.gd). Keyed signals OUTLIVE the
 *  components that read them — that is the point (shared app state). Runtime type check on
 *  key collision (family: the registry is honest about misuse, never silent). */
namespace Ruitk
{
	RUITKCORE_API TSharedPtr<FRuitkSignalBase>* FindOrAddSignalSlot(FName Key);
	RUITKCORE_API TSharedPtr<FRuitkSignalBase> TryGetSignal(FName Key);
	RUITKCORE_API bool HasSignal(FName Key);
	/** Drop all keyed signals (subscribers NOT notified) — full session reset. */
	RUITKCORE_API void ClearSignals();

	template <typename T> TSharedRef<TRuitkSignal<T>> GetOrCreateSignal(FName Key, T Initial = T())
	{
		TSharedPtr<FRuitkSignalBase>* Slot = FindOrAddSignalSlot(Key);
		if (!Slot->IsValid())
		{
			*Slot = MakeShared<TRuitkSignal<T>>(MoveTemp(Initial));
		}
		return StaticCastSharedRef<TRuitkSignal<T>>(Slot->ToSharedRef());
	}
} // namespace Ruitk

// ─────────────────────────────────────────────────────────────────────────────────────────
// The signal hooks. Cell holds the SNAPSHOT + the live unsubscribe (dtor releases it —
// teardown-by-destructor, the C++ half of _dispose_fiber_state).
// ─────────────────────────────────────────────────────────────────────────────────────────

template <typename TSelected> struct TRuitkSignalCell final : IRuitkHookCell
{
	TSelected Value{};
	TFunction<void()> Unsub;
	const void* BoundSignal = nullptr; // identity of the subscribed signal (re-subscribe detection)

	virtual ~TRuitkSignalCell() override
	{
		if (Unsub)
		{
			Unsub();
		}
	}
	virtual ERuitkHookKind GetKind() const override { return ERuitkHookKind::Signal; }
	RUITK_HOOK_CELL_TYPE()
};

namespace Ruitk
{
	/** UseSignal with selector: re-renders when the SELECTED slice changes. */
	template <typename T, typename TSelected>
	TSelected UseSignal(FRuitkContext& Ctx, const TSharedRef<TRuitkSignal<T>>& Sig, TFunction<TSelected(const T&)> Selector)
	{
		TRuitkSignalCell<TSelected>* Cell = Ctx.template AcquireCell<TRuitkSignalCell<TSelected>>(ERuitkHookKind::Signal);
		const bool bFirst = (Cell->BoundSignal == nullptr);

		// Render reads the snapshot directly from the store (never stale).
		Cell->Value = Selector(Sig->Get());

		// Subscribe in an EFFECT keyed on the signal's identity; the effect body re-checks
		// the snapshot (the useSyncExternalStore tear-window re-check).
		TWeakPtr<FRuitkComponentState> Weak = Ctx.StateWeak();
		const int32 SlotIndex = Ctx.GetState().HookIndex - 1;
		TSharedRef<TRuitkSignal<T>> SigCopy = Sig;
		TFunction<TSelected(const T&)> SelCopy = Selector;
		Ctx.InternalUseEffect(
			[Weak, SlotIndex, SigCopy, SelCopy]() -> FRuitkEffectCleanup
			{
				auto ReadAndMaybeNotify = [Weak, SlotIndex, SigCopy, SelCopy]()
				{
					TSharedPtr<FRuitkComponentState> S = Weak.Pin();
					if (!S.IsValid() || SlotIndex >= S->Hooks.Num() ||
						S->Hooks[SlotIndex]->TypeHash() != TRuitkSignalCell<TSelected>::StaticTypeHash())
					{
						return;
					}
					TRuitkSignalCell<TSelected>* C = static_cast<TRuitkSignalCell<TSelected>*>(S->Hooks[SlotIndex].Get());
					TSelected Now = SelCopy(SigCopy->Get());
					if (!(C->Value == Now))
					{
						C->Value = MoveTemp(Now);
						S->NotifyStateUpdated();
					}
				};

				TSharedPtr<FRuitkComponentState> S = Weak.Pin();
				if (!S.IsValid() || SlotIndex >= S->Hooks.Num() ||
					S->Hooks[SlotIndex]->TypeHash() != TRuitkSignalCell<TSelected>::StaticTypeHash())
				{
					return FRuitkEffectCleanup();
				}
				TRuitkSignalCell<TSelected>* C = static_cast<TRuitkSignalCell<TSelected>*>(S->Hooks[SlotIndex].Get());
				if (C->Unsub) // re-subscribing (signal instance changed)
				{
					C->Unsub();
				}
				C->BoundSignal = &SigCopy.Get();
				C->Unsub = SigCopy->Subscribe(ReadAndMaybeNotify);
				ReadAndMaybeNotify(); // tear-window re-check: value may have moved between render and effect
				return FRuitkEffectCleanup(
					[Weak, SlotIndex]()
					{
						TSharedPtr<FRuitkComponentState> S2 = Weak.Pin();
						if (!S2.IsValid() || SlotIndex >= S2->Hooks.Num() ||
							S2->Hooks[SlotIndex]->TypeHash() != TRuitkSignalCell<TSelected>::StaticTypeHash())
						{
							return;
						}
						TRuitkSignalCell<TSelected>* C2 =
							static_cast<TRuitkSignalCell<TSelected>*>(S2->Hooks[SlotIndex].Get());
						if (C2->Unsub)
						{
							C2->Unsub();
							C2->Unsub = nullptr;
							C2->BoundSignal = nullptr;
						}
					});
			},
			Ruitk::Deps(&SigCopy.Get()));
		(void)bFirst;
		return Cell->Value;
	}

	/** UseSignal without selector: the whole value. */
	template <typename T> T UseSignal(FRuitkContext& Ctx, const TSharedRef<TRuitkSignal<T>>& Sig)
	{
		return UseSignal<T, T>(Ctx, Sig, [](const T& V) { return V; });
	}

	/** Process-wide keyed signal (created lazily; shared by every reader of the key). */
	template <typename T> T UseSignalKey(FRuitkContext& Ctx, FName Key, T Initial = T())
	{
		return UseSignal<T>(Ctx, GetOrCreateSignal<T>(Key, MoveTemp(Initial)));
	}
} // namespace Ruitk
