// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Per-function-component hook & effect storage — the port of component_state.gd. SHARED
// across a fiber's alternates by TSharedPtr (never copied), so hook state survives the
// current<->WIP double-buffer swap; `Fiber` is re-pointed to the live fiber each render
// (D-06's ownership edge #2: the back-pointer is RAW and nulled on teardown).

#pragma once

#include "CoreMinimal.h"
#include "RuitkNode.h"
#include "RuitkHooksInternal.h"

struct FRuitkFiber;

class RUITKCORE_API FRuitkComponentState
{
public:
	FRuitkComponentState() = default;
	// Never copied: state is SHARED across alternates by pointer (component_state.gd's
	// contract) — a copy would silently fork hook slots. Also required mechanically: the
	// dllexport'd class would otherwise instantiate an implicit copy-assign over the
	// move-only hook-cell array.
	FRuitkComponentState(const FRuitkComponentState&) = delete;
	FRuitkComponentState& operator=(const FRuitkComponentState&) = delete;

	/** Back-pointer to the LIVE fiber (re-pointed by the reconciler; raw by design — the
	 *  slab owns fibers, state must never extend their life). */
	FRuitkFiber* Fiber = nullptr;

	/** Positional hook slots + cursor (reset each render). */
	TArray<TUniquePtr<IRuitkHookCell>> Hooks;
	int32 HookIndex = 0;

	/** Passive + layout effects, each with their OWN cursor (insertion of an effect can
	 *  never shift state slots — the family's three-cursor rule). */
	TArray<FRuitkEffect> Effects;
	int32 EffectIndex = 0;
	TArray<FRuitkEffect> LayoutEffects;
	int32 LayoutIndex = 0;

	/** Context reads recorded this render, OUT of the slot array (useContext consumes no
	 *  hook slot). Each entry re-checks "did my context change?" against the live tree. */
	struct FContextDep
	{
		const void* Key = nullptr;						// context handle identity
		TFunction<bool(const FRuitkFiber*)> HasChanged; // resolve + compare captured value
	};
	TArray<FContextDep> ContextDeps;

	/** () -> reconciler.ScheduleUpdateOnFiber(state->Fiber). Cleared on teardown. */
	TFunction<void()> OnStateUpdated;

	bool bIsRendering = false;

	/** Cached render output — reused verbatim on bailout, as the SAME shared list (pointer
	 *  identity is what lets grandchildren bail via children_same; a fresh copy per bailed
	 *  render would silently defeat descendant bailouts). Why vnodes are cross-frame; see
	 *  FRuitkNode's lifetime note. */
	FRuitkChildren LastOutput;

	// --- dev diagnostics (ruitk.HookValidation) ---
	TArray<ERuitkHookKind> HookLog; // this render (transient)
	uint32 HmrGenerationStamp = 0;	// TB-13: the HMR generation this state last rendered under
	TArray<ERuitkHookKind>
		HmrShapeSnapshot; // TB-13: the PREVIOUS render's shape (validation's
						  // HookSignatures stays first-primed to keep nagging — two consumers, two baselines)
	TArray<ERuitkHookKind> HookSignatures; // primed first render
	bool bHookOrderPrimed = false;
	TSet<FName> DiagWarned; // warn-once keys

	/** The definition-override generation this state last rendered under (HMR). A fiber
	 *  seeing a NEWER generation with bResetState pending runs a deliberate state reset. */
	uint32 HmrGeneration = 0;

	/** TD-019: one-shot state migration across the compiled→interp HMR swap. When the reconciler
	 *  resets hooks for a same-shape representation swap it snapshots the old cells' exported
	 *  FRuitkValues here (by hook slot); the next render's UseState seeds fresh FRuitkValue cells from
	 *  it instead of the interpreter's Init, so numeric/string/bool/text state survives the FIRST
	 *  save too. Cleared after the migrating render. Null entries (non-migratable slots) re-init. */
	TArray<FRuitkValue> MigratedState;

	/** TB-13 hardening — call BEFORE downcasting the cell at Slot: verifies the stored cell
	 *  is EXACTLY the concrete type (and kind) the caller expects. A mismatch means the hook
	 *  list changed under this live fiber (an HMR edit, or a rules-of-hooks violation): the
	 *  TAIL from Slot on is truncated — cells before Slot were already served this render
	 *  (their references must stay valid), the mismatched ones rebuild fresh via the normal
	 *  allocate-on-miss path. The render-tail detector then decides between the HMR clean
	 *  reset and the rules-of-hooks error report. Without this, the old unchecked cast
	 *  destructed garbage (the 2026-07-24 crash). */
	void EnsureCellShape(int32 Slot, uint32 ExpectedTypeHash, ERuitkHookKind ExpectedKind)
	{
		if (Slot < Hooks.Num() &&
			(Hooks[Slot]->TypeHash() != ExpectedTypeHash || Hooks[Slot]->GetKind() != ExpectedKind))
		{
			Hooks.SetNum(Slot); // cell dtors release subscriptions (the signal-cell contract)
		}
	}

	/** The HMR hook-shape reset: run effect cleanups, then drop every hook slot so the next
	 *  render re-initializes (cell destructors release external subscriptions). Scheduling
	 *  wiring (OnStateUpdated/Fiber) and LastOutput stay — the fiber is still mounted. */
	void HmrResetHooks()
	{
		for (FRuitkEffect& Effect : LayoutEffects)
		{
			if (Effect.Cleanup)
			{
				Effect.Cleanup();
				Effect.Cleanup = nullptr;
			}
		}
		for (FRuitkEffect& Effect : Effects)
		{
			if (Effect.Cleanup)
			{
				Effect.Cleanup();
				Effect.Cleanup = nullptr;
			}
		}
		Hooks.Empty();
		Effects.Empty();
		LayoutEffects.Empty();
		ContextDeps.Empty();
		HookIndex = 0;
		EffectIndex = 0;
		LayoutIndex = 0;
		bHookOrderPrimed = false;
		HookLog.Reset();
		HookSignatures.Reset();
		HmrShapeSnapshot.Reset();
	}

	/** Deliberate full teardown (unmount / HMR hook-shape reset). Cell destructors release
	 *  external subscriptions (signal cells unsubscribe in ~cell); effects' cleanups must
	 *  have been run by the reconciler BEFORE this (family order). */
	void Dispose()
	{
		Hooks.Empty();
		Effects.Empty();
		LayoutEffects.Empty();
		ContextDeps.Empty();
		LastOutput.Reset();
		OnStateUpdated = nullptr;
		Fiber = nullptr;
	}

	/** Notify the reconciler (setters/dispatch/signal-fire path). */
	void NotifyStateUpdated()
	{
		if (OnStateUpdated)
		{
			OnStateUpdated();
		}
	}
};

// ── TRuitkSetter implementation (needs FRuitkComponentState) ─────────────────────────────────

template <typename T> void TRuitkSetter<T>::operator()(T NewValue) const
{
	TSharedPtr<FRuitkComponentState> S = State.Pin();
	if (!S.IsValid() || Slot >= S->Hooks.Num() ||
		S->Hooks[Slot]->TypeHash() !=
			TRuitkStateCell<T>::StaticTypeHash()) // torn down / reshaped (TB-13) — ignore late calls [audit C3]
	{
		return;
	}
	TRuitkStateCell<T>* Cell = static_cast<TRuitkStateCell<T>*>(S->Hooks[Slot].Get());
	if (Cell->Value == NewValue) // equal-set bails (no re-render)
	{
		return;
	}
	Cell->Value = MoveTemp(NewValue);
	S->NotifyStateUpdated();
}

template <typename T> void TRuitkSetter<T>::operator()(TFunction<T(const T&)> Updater) const
{
	TSharedPtr<FRuitkComponentState> S = State.Pin();
	if (!S.IsValid() || Slot >= S->Hooks.Num() ||
		S->Hooks[Slot]->TypeHash() != TRuitkStateCell<T>::StaticTypeHash()) // torn down / reshaped (TB-13)
	{
		return;
	}
	TRuitkStateCell<T>* Cell = static_cast<TRuitkStateCell<T>*>(S->Hooks[Slot].Get());
	T Next = Updater(Cell->Value);
	if (Cell->Value == Next)
	{
		return;
	}
	Cell->Value = MoveTemp(Next);
	S->NotifyStateUpdated();
}
