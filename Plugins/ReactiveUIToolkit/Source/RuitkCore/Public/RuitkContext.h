// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// FRuitkContext — the render context handed to every component: the 23 family hooks as
// member functions over the current fiber's FRuitkComponentState (hooks.gd's static `_cur`
// replaced by an explicit reference — D-05; a debug thread-local still asserts in-render
// use, since capturing Ctx in a stored lambda compiles and corrupts cursors at runtime —
// the honest limitation the plan records).
//
// RULES OF HOOKS: top level of the render function only — stable call order every render.
// The positional-slot model depends on it; the dev validator (ruitk.HookValidation) diagnoses
// violations.

#pragma once

#include "CoreMinimal.h"
#include "RuitkNode.h"
#include "RuitkComponentState.h"
#include "RuitkFiber.h"
#include "RuitkContextHandle.h"
#include "RuitkCoreMisc.h"
#include "RuitkHostConfig.h"

class FRuitkReconciler;

/** UseSafeArea result (pixels). */
struct FRuitkSafeArea
{
	float Left = 0, Top = 0, Right = 0, Bottom = 0;
};

namespace Ruitk
{
	/** The warn-once dedup core (P-10): ONE mechanism — the component's DiagWarned key set —
	 *  with two entry points: FRuitkContext::WarnOnce (hook-site warnings, a context on the
	 *  stack) and the reconciler's strict set-state-during-render site (no context exists
	 *  there). The message is built lazily so deduped repeat calls never pay the Printf. */
	RUITKCORE_API void DiagWarnOnce(FRuitkComponentState& State, FName Key, TFunctionRef<FString()> BuildMsg);
} // namespace Ruitk

class RUITKCORE_API FRuitkContext
{
public:
	FRuitkContext(const TSharedRef<FRuitkComponentState>& InState, FRuitkFiber& InFiber, FRuitkReconciler& InReconciler,
				  IRuitkHostConfig& InHost)
		: StateShared(InState), State(InState.Get()), Fiber(InFiber), Reconciler(InReconciler), Host(InHost)
	{
	}

	// Non-copyable, non-movable: hardening against storing/capturing the context (D-05).
	FRuitkContext(const FRuitkContext&) = delete;
	FRuitkContext& operator=(const FRuitkContext&) = delete;

	// ── state ────────────────────────────────────────────────────────────────────────

	/** auto [Value, SetValue] = Ctx.UseState(0); — setter identity stable; equal-set bails. */
	template <typename T> TTuple<T, TRuitkSetter<T>> UseState(T Initial = T())
	{
		Record(ERuitkHookKind::State);
		const int32 i = State.HookIndex++;
		State.EnsureCellShape(i, TRuitkStateCell<T>::StaticTypeHash(), ERuitkHookKind::State); // TB-13
		if (i >= State.Hooks.Num())
		{
			// TD-019: on a migrating HMR reset the reconciler stashed the old cell's exported value
			// at this slot — seed the fresh FRuitkValue cell from it (the interpreter's cell type)
			// instead of the interp Init, so numeric/string/bool/text state survives the first swap.
			if constexpr (std::is_same_v<T, FRuitkValue>)
			{
				if (State.MigratedState.IsValidIndex(i) && !State.MigratedState[i].IsNull())
				{
					Initial = State.MigratedState[i];
				}
			}
			State.Hooks.Emplace(MakeUnique<TRuitkStateCell<T>>(MoveTemp(Initial)));
		}
		TRuitkStateCell<T>* Cell = static_cast<TRuitkStateCell<T>*>(State.Hooks[i].Get());
		return MakeTuple(Cell->Value, TRuitkSetter<T>(StateWeak(), i));
	}

	/** auto [Value, Dispatch] = Ctx.UseReducer<T, FAction>(Reducer, Initial); — dispatch
	 *  identity stable; the reducer closure is refreshed every render (family parity). */
	template <typename T, typename TAction>
	TTuple<T, TFunction<void(TAction)>> UseReducer(TFunction<T(const T&, const TAction&)> Reducer, T Initial = T())
	{
		Record(ERuitkHookKind::Reducer);
		const int32 i = State.HookIndex++;
		State.EnsureCellShape(i, TRuitkReducerCell<T, TAction>::StaticTypeHash(), ERuitkHookKind::Reducer); // TB-13
		if (i >= State.Hooks.Num())
		{
			State.Hooks.Emplace(MakeUnique<TRuitkReducerCell<T, TAction>>(MoveTemp(Initial)));
		}
		TRuitkReducerCell<T, TAction>* Cell = static_cast<TRuitkReducerCell<T, TAction>*>(State.Hooks[i].Get());
		Cell->Reducer = MoveTemp(Reducer);
		TWeakPtr<FRuitkComponentState> Weak = StateWeak();
		TFunction<void(TAction)> Dispatch = [Weak, i](TAction Action)
		{
			TSharedPtr<FRuitkComponentState> S = Weak.Pin();
			if (!S.IsValid() || i >= S->Hooks.Num() ||
				S->Hooks[i]->TypeHash() !=
					TRuitkReducerCell<T,
									  TAction>::StaticTypeHash()) // torn down / reshaped (TB-13) — ignore late dispatch
			{
				return;
			}
			TRuitkReducerCell<T, TAction>* C = static_cast<TRuitkReducerCell<T, TAction>*>(S->Hooks[i].Get());
			T Next = C->Reducer(C->Value, Action);
			if (C->Value == Next)
			{
				return;
			}
			C->Value = MoveTemp(Next);
			S->NotifyStateUpdated();
		};
		return MakeTuple(Cell->Value, MoveTemp(Dispatch));
	}

	/** Stable box; mutating Current never re-renders. */
	template <typename T> TSharedRef<TRuitkRef<T>> UseRef(T Initial = T())
	{
		Record(ERuitkHookKind::Ref);
		const int32 i = State.HookIndex++;
		State.EnsureCellShape(i, TRuitkRefCell<T>::StaticTypeHash(), ERuitkHookKind::Ref); // TB-13
		if (i >= State.Hooks.Num())
		{
			State.Hooks.Emplace(MakeUnique<TRuitkRefCell<T>>(MoveTemp(Initial)));
		}
		return static_cast<TRuitkRefCell<T>*>(State.Hooks[i].Get())->Box;
	}

	/** Recompute only when deps change (shallow — Ruitk::Deps semantics). */
	template <typename T> const T& UseMemo(TFunction<T()> Factory, FRuitkDeps Deps)
	{
		Record(ERuitkHookKind::Memo);
		const int32 i = State.HookIndex++;
		State.EnsureCellShape(i, TRuitkMemoCell<T>::StaticTypeHash(), ERuitkHookKind::Memo); // TB-13
		if (i >= State.Hooks.Num())
		{
			TUniquePtr<TRuitkMemoCell<T>> Cell = MakeUnique<TRuitkMemoCell<T>>();
			Cell->Value = Factory();
			Cell->LastDeps = MoveTemp(Deps);
			State.Hooks.Emplace(MoveTemp(Cell));
			return static_cast<TRuitkMemoCell<T>*>(State.Hooks[i].Get())->Value;
		}
		TRuitkMemoCell<T>* Cell = static_cast<TRuitkMemoCell<T>*>(State.Hooks[i].Get());
		if (Ruitk::DepsChanged(Cell->LastDeps, Deps))
		{
			Cell->Value = Factory();
			Cell->LastDeps = MoveTemp(Deps);
		}
		return Cell->Value;
	}

	/** A stable FRuitkCallback while deps are unchanged (memo-friendly event props). */
	FRuitkCallback UseCallback(TFunction<void(const FRuitkValue&)> Fn, FRuitkDeps Deps)
	{
		return UseMemo<FRuitkCallback>([&Fn]() { return FRuitkCallback::Create(MoveTemp(Fn)); }, MoveTemp(Deps));
	}
	FRuitkCallback UseCallback(TFunction<void()> Fn, FRuitkDeps Deps)
	{
		return UseMemo<FRuitkCallback>([&Fn]() { return FRuitkCallback::Create(MoveTemp(Fn)); }, MoveTemp(Deps));
	}

	/** Family useImperativeHandle (P2, WIDGET_COMPLETION_PLAN): a child publishes a
	 *  custom handle object into a parent-owned ref box (the parent passes the box down as
	 *  a prop), rebuilt when Deps change and CLEARED on unmount. THandle must be
	 *  default-constructible (the cleared state). Both siblings ship this hook; Unity's
	 *  media controllers (Play/Pause/Seek) are the canonical use. */
	template <typename THandle>
	void UseImperativeHandle(const TSharedRef<TRuitkRef<THandle>>& TargetRef, TFunction<THandle()> Factory,
							 FRuitkDeps Deps)
	{
		const THandle& Handle = UseMemo<THandle>(MoveTemp(Factory), Deps); // copy of Deps — Effect owns the move
		THandle Copy = Handle;
		TSharedRef<TRuitkRef<THandle>> Target = TargetRef;
		UseEffect(
			[Target, Copy]() -> FRuitkEffectCleanup
			{
				Target->Current = Copy;
				return [Target]() { Target->Current = THandle(); };
			},
			MoveTemp(Deps));
	}

	/** = UseMemo (family alias). */
	template <typename T> const T& UseImperativeHandle(TFunction<T()> Factory, FRuitkDeps Deps)
	{
		return UseMemo<T>(MoveTemp(Factory), MoveTemp(Deps));
	}

	// ── effects ──────────────────────────────────────────────────────────────────────

	/** Passive: after commit, two-pass (all cleanups, then all setups). Deps: Ruitk::Deps(...)
	 *  = when changed; Ruitk::Deps() = mount-only; Ruitk::EveryCommit() = every commit.
	 *  Accepts a lambda returning either void or an FRuitkEffectCleanup (if-constexpr dispatch
	 *  — twin TFunction overloads are ambiguous for lambdas). */
	template <typename TFn> void UseEffect(TFn&& Effect, FRuitkDeps Deps)
	{
		UseEffectImpl(WrapEffect(Forward<TFn>(Effect)), MoveTemp(Deps));
	}

	/** Layout: synchronously during commit (pre-paint), cleanup-then-setup per fiber. */
	template <typename TFn> void UseLayoutEffect(TFn&& Effect, FRuitkDeps Deps)
	{
		UseLayoutEffectImpl(WrapEffect(Forward<TFn>(Effect)), MoveTemp(Deps));
	}

	void UseEffectImpl(TFunction<FRuitkEffectCleanup()> Effect, FRuitkDeps Deps, bool bWarnNoDeps = true)
	{
		Record(ERuitkHookKind::Effect); // own cursor; logged for order validation like any hook
		const int32 i = State.EffectIndex++;
		// Strict diagnostics, family warning 2 (M5): unset Deps — Ruitk::EveryCommit(), the
		// family's "no dependency array" (Unity warns on null deps the same way, Hooks.cs:551-575)
		// — re-runs the effect every render. Legal, but usually a forgotten deps argument, so it
		// costs one warning per component+slot per session (DiagWarnOnce; StrictMode's double
		// invoke dedups through the same set, the diagnostics-count-once clause). Library-owned
		// every-commit effects opt out via InternalUseEffect — the message names the USER's
		// component, which must not be blamed for library plumbing it cannot change.
		if (bWarnNoDeps && !Deps.IsSet() && FRuitkConfig::IsStrictDiagnosticsEnabled())
		{
			Ruitk::DiagWarnOnce(State, FName(TEXT("strict-nodeps-effect"), i + 1),
								[this]() -> FString
								{
									return FString::Printf(
										TEXT("[Ruitk][strict] %s: effect has no dependency array — it re-runs every "
											 "render; pass deps (Ruitk::Deps() for run-once)"),
										*Fiber.ComponentId.ToString());
								});
		}
		if (i >= State.Effects.Num())
		{
			State.Effects.AddDefaulted();
		}
		State.Effects[i].Factory = MoveTemp(Effect);
		State.Effects[i].Deps = MoveTemp(Deps);
		NotifyEffects();
	}

	void UseLayoutEffectImpl(TFunction<FRuitkEffectCleanup()> Effect, FRuitkDeps Deps)
	{
		Record(ERuitkHookKind::LayoutEffect);
		const int32 i = State.LayoutIndex++;
		// Strict diagnostics, family warning 2 — the layout twin (own cursor, own key family).
		if (!Deps.IsSet() && FRuitkConfig::IsStrictDiagnosticsEnabled())
		{
			Ruitk::DiagWarnOnce(State, FName(TEXT("strict-nodeps-layout"), i + 1),
								[this]() -> FString
								{
									return FString::Printf(
										TEXT("[Ruitk][strict] %s: layout effect has no dependency array — it re-runs "
											 "every render; pass deps (Ruitk::Deps() for run-once)"),
										*Fiber.ComponentId.ToString());
								});
		}
		if (i >= State.LayoutEffects.Num())
		{
			State.LayoutEffects.AddDefaulted();
		}
		State.LayoutEffects[i].Factory = MoveTemp(Effect);
		State.LayoutEffects[i].Deps = MoveTemp(Deps);
		NotifyEffects();
	}

	// ── context ──────────────────────────────────────────────────────────────────────

	/** Nearest provided value (O(1) via the handle's render stack — D-08.3), or the
	 *  handle's default. Consumes NO hook slot; records the read for change re-rendering. */
	template <typename T> const T& UseContext(const TRuitkContext<T>& Handle)
	{
		Fiber.bReadsContext = true;
		const TArray<const T*>& Stack = Handle.GetCore().RenderStack;
		const T& Current = Stack.IsEmpty() ? Handle.GetDefault() : *Stack.Last();

		FRuitkComponentState::FContextDep Dep;
		Dep.Key = Handle.Key();
		Dep.HasChanged = [Handle, Old = Current](const FRuitkFiber* F) -> bool
		{
			const T* Now = ResolveTyped(Handle, F);
			const T& NowRef = Now ? *Now : Handle.GetDefault();
			return !(NowRef == Old);
		};
		State.ContextDeps.Add(MoveTemp(Dep));
		return Current;
	}

	/** Provide `Value` under `Handle` to this component's subtree (hook-style provider —
	 *  family API, no wrapper element). On change, marks consuming descendants dirty so
	 *  they re-render through bailouts (eager propagation). */
	template <typename T> void ProvideContext(const TRuitkContext<T>& Handle, T Value)
	{
		if (!Fiber.ProvidedContext.IsValid())
		{
			Fiber.ProvidedContext = MakeShared<TMap<const void*, TSharedPtr<void>>>();
		}
		TSharedPtr<TRuitkProvidedValue<T>> Holder = MakeShared<TRuitkProvidedValue<T>>(Handle, MoveTemp(Value));

		bool bChanged = true;
		if (const FRuitkFiber* Alt = Fiber.Alternate)
		{
			if (Alt->ProvidedContext.IsValid())
			{
				if (const TSharedPtr<void>* Prev = Alt->ProvidedContext->Find(Handle.Key()))
				{
					const IRuitkProvidedValue* PrevHolder = static_cast<const IRuitkProvidedValue*>(Prev->Get());
					bChanged = !Holder->ValueEquals(*PrevHolder);
				}
			}
		}
		Fiber.ProvidedContext->Add(Handle.Key(), Holder);
		if (bChanged)
		{
			ReconcilerOnProvidedChanged(Handle.Key());
		}
	}

	// ── concurrency (family semantics, D-08 kept-from-Godot list) ─────────────────────

	/** Returns the PREVIOUS value on the render where `Value` changes, then commits the
	 *  target on a deferred next-frame tick (one extra re-render). */
	template <typename T> T UseDeferredValue(T Value, FRuitkDeps Deps = FRuitkDeps())
	{
		Record(ERuitkHookKind::Deferred);
		const int32 i = State.HookIndex++;
		State.EnsureCellShape(i, TRuitkDeferredCell<T>::StaticTypeHash(), ERuitkHookKind::Deferred); // TB-13
		if (i >= State.Hooks.Num())
		{
			TUniquePtr<TRuitkDeferredCell<T>> Cell = MakeUnique<TRuitkDeferredCell<T>>();
			Cell->Value = Value;
			Cell->Target = Value;
			Cell->Deps = MoveTemp(Deps);
			State.Hooks.Emplace(MoveTemp(Cell));
			return Value;
		}
		TRuitkDeferredCell<T>* Cell = static_cast<TRuitkDeferredCell<T>*>(State.Hooks[i].Get());
		bool bChanged;
		if (Deps.IsSet())
		{
			bChanged = Ruitk::DepsChanged(Cell->Deps, Deps);
			Cell->Deps = MoveTemp(Deps);
		}
		else
		{
			bChanged = !(Cell->Value == Value);
		}
		if (bChanged && !(Cell->Value == Value))
		{
			Cell->Target = Value;
			if (!Cell->bPending)
			{
				Cell->bPending = true;
				ScheduleDeferredCommit<T>(i);
			}
		}
		return Cell->Value;
	}

	/** [bIsPending(false), StartTransition] — synchronous no-op, faithful to the family. */
	TTuple<bool, TFunction<void(TFunction<void()>)>> UseTransition()
	{
		Record(ERuitkHookKind::Transition);
		const int32 i = State.HookIndex++;
		State.EnsureCellShape(i, FRuitkTransitionCell::StaticTypeHash(), ERuitkHookKind::Transition); // TB-13
		if (i >= State.Hooks.Num())
		{
			State.Hooks.Emplace(MakeUnique<FRuitkTransitionCell>());
		}
		return MakeTuple(false, TFunction<void(TFunction<void()>)>(
									[](TFunction<void()> Action)
									{
										if (Action)
										{
											Action();
										}
									}));
	}

	// ── stable callbacks (identity never changes; body refreshed every render) ─────────

	FRuitkCallback UseStableCallback(TFunction<void()> Fn)
	{
		return UseStableAction(TFunction<void(const FRuitkValue&)>(
			[Inner = MoveTemp(Fn)](const FRuitkValue&)
			{
				if (Inner)
				{
					Inner();
				}
			}));
	}
	FRuitkCallback UseStableFunc(TFunction<void()> Fn) { return UseStableCallback(MoveTemp(Fn)); }

	FRuitkCallback UseStableAction(TFunction<void(const FRuitkValue&)> Fn)
	{
		Record(ERuitkHookKind::Stable);
		const int32 i = State.HookIndex++;
		State.EnsureCellShape(i, FRuitkStableCell::StaticTypeHash(), ERuitkHookKind::Stable); // TB-13
		if (i >= State.Hooks.Num())
		{
			TUniquePtr<FRuitkStableCell> Cell = MakeUnique<FRuitkStableCell>();
			TSharedRef<TFunction<void(const FRuitkValue&)>> Inner = Cell->Inner;
			Cell->Wrapper = FRuitkCallback::Create(TFunction<void(const FRuitkValue&)>(
				[Inner](const FRuitkValue& Arg)
				{
					if (*Inner)
					{
						(*Inner)(Arg);
					}
				}));
			State.Hooks.Emplace(MoveTemp(Cell));
		}
		FRuitkStableCell* Cell = static_cast<FRuitkStableCell*>(State.Hooks[i].Get());
		*Cell->Inner = MoveTemp(Fn);
		return Cell->Wrapper;
	}

	// ── platform ─────────────────────────────────────────────────────────────────────

	/** Device safe-area insets — host-supplied (mock test value → Slate FDisplayMetrics →
	 *  CommonUI override; the D-27 owner chain). */
	FRuitkSafeArea UseSafeArea()
	{
		Record(ERuitkHookKind::SafeArea);
		const int32 i = State.HookIndex++;
		State.EnsureCellShape(i, FRuitkMarkerCell::StaticTypeHash(), ERuitkHookKind::SafeArea); // TB-13
		if (i >= State.Hooks.Num())
		{
			State.Hooks.Emplace(MakeUnique<FRuitkMarkerCell>(ERuitkHookKind::SafeArea));
		}
		FRuitkSafeArea Out;
		Host.GetSafeArea(Out.Left, Out.Top, Out.Right, Out.Bottom);
		return Out;
	}

	// ── animation (Phase 7) ───────────────────────────────────────────────────────────

	/**
	 * Animate towards Target, re-rendering every frame until settled (a self-re-arming
	 * RequestFrame chain — the Suspense poll-driver pattern). Retargeting mid-flight starts
	 * from the CURRENT value (no snap). First render primes at Target (no mount animation;
	 * use UseAnimate for enter transitions). Time comes from the host clock (deterministic
	 * under the mock host).
	 */
	template <typename T>
	T UseTweenValue(const T& Target, float DurationSec = 0.25f, ERuitkEase Ease = ERuitkEase::InOut)
	{
		return TweenSlot<T>(ERuitkHookKind::TweenValue, Target, DurationSec, Ease);
	}

	/** Float convenience over UseTweenValue. */
	float UseTween(float Target, float DurationSec = 0.25f, ERuitkEase Ease = ERuitkEase::InOut)
	{
		return TweenSlot<float>(ERuitkHookKind::Tween, Target, DurationSec, Ease);
	}

	/** 0..1 progress towards bIn — the enter/exit-animation driver. */
	float UseAnimate(bool bIn, float DurationSec = 0.25f, ERuitkEase Ease = ERuitkEase::InOut)
	{
		return TweenSlot<float>(ERuitkHookKind::Animate, bIn ? 1.0f : 0.0f, DurationSec, Ease);
	}

	/**
	 * A play-sound callback for a named bus. Dispatch goes through the process-wide sfx sink
	 * (Ruitk::SetSfxSink) — the game registers HOW to play (world context, audio assets); an
	 * unset sink is a quiet no-op. The callback payload rides through to the sink.
	 */
	FRuitkCallback UseSfx(FName Bus = NAME_None)
	{
		Record(ERuitkHookKind::Sfx);
		const int32 i = State.HookIndex++;
		State.EnsureCellShape(i, FRuitkMarkerCell::StaticTypeHash(), ERuitkHookKind::Sfx); // TB-13
		if (i >= State.Hooks.Num())
		{
			State.Hooks.Emplace(MakeUnique<FRuitkMarkerCell>(ERuitkHookKind::Sfx));
		}
		return FRuitkCallback::Create([Bus](const FRuitkValue& Payload) { Ruitk::DispatchSfx(Bus, Payload); });
	}

	// ── signals (UseSignal/UseSignalKey live in RuitkSignal.h as templates over this) ───

	/** \internal — the shared tween slot: prime → retarget-from-current → ease → re-arm. */
	template <typename T> T TweenSlot(ERuitkHookKind Kind, const T& Target, float DurationSec, ERuitkEase Ease)
	{
		Record(Kind);
		const int32 i = State.HookIndex++;
		State.EnsureCellShape(i, TRuitkTweenCell<T>::StaticTypeHash(), Kind); // TB-13
		if (i >= State.Hooks.Num())
		{
			TUniquePtr<TRuitkTweenCell<T>> Cell = MakeUnique<TRuitkTweenCell<T>>(Kind);
			Cell->From = Target;
			Cell->To = Target;
			Cell->Current = Target; // prime at target — no mount animation
			State.Hooks.Emplace(MoveTemp(Cell));
		}
		TRuitkTweenCell<T>* Cell = static_cast<TRuitkTweenCell<T>*>(State.Hooks[i].Get());
		const double Now = Host.GetTimeSeconds();
		if (!(Cell->To == Target))
		{
			Cell->From = Cell->Current; // retarget mid-flight: continue from where we are
			Cell->To = Target;
			Cell->StartTime = Now;
			Cell->Duration = FMath::Max(DurationSec, 0.0001f);
			Cell->bActive = true;
		}
		if (Cell->bActive)
		{
			const float Progress =
				FMath::Clamp(static_cast<float>((Now - Cell->StartTime) / Cell->Duration), 0.0f, 1.0f);
			Cell->Current = Ruitk::LerpTween(Cell->From, Cell->To, Ruitk::ApplyEase(Ease, Progress));
			if (Progress >= 1.0f)
			{
				Cell->Current = Cell->To;
				Cell->bActive = false;
			}
			else
			{
				// self-re-arming frame chain: one coalesced re-render per frame while animating
				TWeakPtr<FRuitkComponentState> Weak = StateWeak();
				Host.RequestFrame(
					[Weak]()
					{
						if (TSharedPtr<FRuitkComponentState> Live = Weak.Pin())
						{
							Live->NotifyStateUpdated();
						}
					});
			}
		}
		return Cell->Current;
	}

	/** \internal — signal hook plumbing (see RuitkSignal.h). */
	template <typename TCell, typename... TCellArgs> TCell* AcquireCell(ERuitkHookKind Kind, TCellArgs&&... Args)
	{
		Record(Kind);
		const int32 i = State.HookIndex++;
		State.EnsureCellShape(i, TCell::StaticTypeHash(), Kind); // TB-13
		if (i >= State.Hooks.Num())
		{
			State.Hooks.Emplace(MakeUnique<TCell>(Forward<TCellArgs>(Args)...));
		}
		return static_cast<TCell*>(State.Hooks[i].Get());
	}

	/** \internal */
	TWeakPtr<FRuitkComponentState> StateWeak() const;
	/** \internal */
	FRuitkComponentState& GetState() { return State; }
	FRuitkFiber& GetFiber() { return Fiber; }
	IRuitkHostConfig& GetHost() { return Host; }
	/** \internal — library-owned effect registration (signal subscriptions, CommonUI focus
	 *  designation, …): identical to UseEffect minus the strict no-deps warning, which names
	 *  the USER's component and must not fire for plumbing the user did not write. */
	void InternalUseEffect(TFunction<FRuitkEffectCleanup()> Effect, FRuitkDeps Deps)
	{
		UseEffectImpl(MoveTemp(Effect), MoveTemp(Deps), /*bWarnNoDeps=*/false);
	}

private:
	/** Normalize an effect lambda (void- or cleanup-returning) into the canonical factory. */
	template <typename TFn> static TFunction<FRuitkEffectCleanup()> WrapEffect(TFn&& Effect)
	{
		if constexpr (std::is_void_v<std::invoke_result_t<TFn>>)
		{
			return TFunction<FRuitkEffectCleanup()>(
				[Fn = Forward<TFn>(Effect)]() -> FRuitkEffectCleanup
				{
					Fn();
					return FRuitkEffectCleanup();
				});
		}
		else
		{
			return TFunction<FRuitkEffectCleanup()>(Forward<TFn>(Effect));
		}
	}

	void Record(ERuitkHookKind Kind);
	void WarnOnce(FName Key, const FString& Msg);
	void NotifyEffects();
	void ReconcilerOnProvidedChanged(const void* Key);
	void StubSlot(ERuitkHookKind Kind, const TCHAR* HookName, const TCHAR* Owner);

	template <typename T> void ScheduleDeferredCommit(int32 SlotIndex)
	{
		TWeakPtr<FRuitkComponentState> Weak = StateWeak();
		Host.RequestFrame(
			[Weak, SlotIndex]()
			{
				TSharedPtr<FRuitkComponentState> S = Weak.Pin();
				if (!S.IsValid() || SlotIndex >= S->Hooks.Num() ||
					S->Hooks[SlotIndex]->TypeHash() !=
						TRuitkDeferredCell<T>::StaticTypeHash()) // TB-13: reshaped — ignore
				{
					return;
				}
				TRuitkDeferredCell<T>* Cell = static_cast<TRuitkDeferredCell<T>*>(S->Hooks[SlotIndex].Get());
				Cell->bPending = false;
				if (!(Cell->Value == Cell->Target))
				{
					Cell->Value = Cell->Target;
					S->NotifyStateUpdated();
				}
			});
	}

	/** Resolve a context on the COMMITTED tree (bailout re-checks; propagation is untyped). */
	template <typename T> static const T* ResolveTyped(const TRuitkContext<T>& Handle, const FRuitkFiber* From);

	TSharedRef<FRuitkComponentState> StateShared;
	FRuitkComponentState& State;
	FRuitkFiber& Fiber;
	FRuitkReconciler& Reconciler;
	IRuitkHostConfig& Host;
};

// ── template definitions needing FRuitkFiber/complete types ────────────────────────────────

template <typename T> const T* FRuitkContext::ResolveTyped(const TRuitkContext<T>& Handle, const FRuitkFiber* From)
{
	for (const FRuitkFiber* F = From; F != nullptr; F = F->Parent)
	{
		if (F->ProvidedContext.IsValid())
		{
			if (const TSharedPtr<void>* Found = F->ProvidedContext->Find(Handle.Key()))
			{
				return &static_cast<const TRuitkProvidedValue<T>*>(Found->Get())->Value;
			}
		}
	}
	return nullptr;
}
