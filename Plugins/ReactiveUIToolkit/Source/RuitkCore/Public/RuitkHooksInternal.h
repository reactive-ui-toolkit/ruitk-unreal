// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Hook storage internals: dependency arrays, type-erased hook cells, setter handles.
// Ports hooks.gd's positional-slot model — one array of cells with THREE separate cursors
// (state hooks / passive effects / layout effects), so effect insertion can never shift
// state slots. \internal — public only so the host-project test suites can introspect.

#pragma once

#include "CoreMinimal.h"
#include "RuitkTypes.h"

class FRuitkComponentState;

// ─────────────────────────────────────────────────────────────────────────────────────────
// Dependencies (§5 decision, replacing the family's per-language equality quirks):
// VALUE equality for value kinds, IDENTITY for shared refs. Godot deep-compares deps
// ([G-09]); React identity-compares each. C++ deps are built explicitly via Ruitk::Deps(...),
// so the kind split is visible at the call site instead of implicit in the language.
// ─────────────────────────────────────────────────────────────────────────────────────────

struct FRuitkDep
{
	bool bIdentity = false;
	const void* Ptr = nullptr; // identity kind
	FRuitkValue Value;		   // value kind

	bool operator==(const FRuitkDep& Other) const
	{
		if (bIdentity != Other.bIdentity)
		{
			return false;
		}
		return bIdentity ? Ptr == Other.Ptr : Value == Other.Value;
	}
};

/** TOptional-wrapped: unset = "no deps" = run every commit (family: deps == null).
 *  An EMPTY set array = mount-only (family: []). */
using FRuitkDeps = TOptional<TArray<FRuitkDep>>;

namespace Ruitk
{
	namespace Private
	{
		template <typename T> struct TIsSharedPtrLike : std::false_type
		{
		};
		template <typename T, ESPMode M> struct TIsSharedPtrLike<TSharedPtr<T, M>> : std::true_type
		{
		};
		template <typename T, ESPMode M> struct TIsSharedPtrLike<TSharedRef<T, M>> : std::true_type
		{
		};

		/** Single template + if-constexpr dispatch: overload sets recursing into each other
		 *  trip C++ two-phase lookup (an overload can't see ones declared after it). */
		template <typename... TArgs> void AddDep(TArray<FRuitkDep>& Out, const TArgs&... Args)
		{
			(
				[&Out](const auto& Head)
				{
					using THead = std::decay_t<decltype(Head)>;
					FRuitkDep D;
					if constexpr (TIsSharedPtrLike<THead>::value)
					{
						D.bIdentity = true;
						D.Ptr = &*Head; // TSharedPtr::Get() / TSharedRef deref — object identity
					}
					else if constexpr (std::is_same_v<THead, const TCHAR*> || std::is_same_v<THead, TCHAR*>)
					{
						// String literals are VALUES, not identities (address compare would be
						// a per-TU footgun).
						D.Value = FRuitkValue(Head);
					}
					else if constexpr (std::is_pointer_v<THead>)
					{
						D.bIdentity = true;
						D.Ptr = Head;
					}
					else
					{
						D.Value = FRuitkValue(Head); // value kinds via FRuitkValue's constructors
					}
					Out.Add(D);
				}(Args),
				...);
		}
	} // namespace Private

	/** Build a deps array: Ruitk::Deps(Count, Name, SomeSharedPtr). Empty call = mount-only. */
	template <typename... TArgs> FRuitkDeps Deps(const TArgs&... Args)
	{
		TArray<FRuitkDep> Out;
		Out.Reserve(sizeof...(Args));
		Private::AddDep(Out, Args...);
		return FRuitkDeps(MoveTemp(Out));
	}

	/** No-deps sentinel: the effect runs every commit (family deps == null). */
	inline FRuitkDeps EveryCommit()
	{
		return FRuitkDeps();
	}

	/** Shallow deps comparison (family _deps_changed): unset on either side => changed. */
	RUITKCORE_API bool DepsChanged(const FRuitkDeps& Prev, const FRuitkDeps& Next);
} // namespace Ruitk

// ─────────────────────────────────────────────────────────────────────────────────────────
// Hook cells
// ─────────────────────────────────────────────────────────────────────────────────────────

/** Hook kinds — doubles as the hook-order validation signature alphabet (hooks.gd _record).
 *  Effect/LayoutEffect don't consume state slots (own cursors) but ARE order-relevant, so
 *  they log too. */
enum class ERuitkHookKind : uint8
{
	State,
	Reducer,
	Ref,
	Memo,
	Deferred,
	Transition,
	Stable,
	SafeArea,
	Signal,
	Tween,
	TweenValue,
	Animate,
	Sfx,
	Effect,
	LayoutEffect,
};

RUITKCORE_API const TCHAR* RuitkHookKindName(ERuitkHookKind Kind);

/** Type-erased hook slot. Destructors double as teardown (a signal cell's dtor
 *  unsubscribes) — the C++ answer to _dispose_fiber_state's explicit unsub pass. */
// TB-13 hardening — a per-instantiation TYPE identity for hook cells. Accessors used to
// static_cast a slot to the concrete cell type unchecked; when the hook list changes under a
// LIVE fiber (an HMR edit, or a rules-of-hooks violation) that cast type-confuses and is
// memory-unsafe (the 2026-07-24 crash: a State cell destructed as a Memo cell). The hash is
// CONTENT-based (the compiler-spelled signature string), so it is stable across DLLs and
// Live Coding patches — unlike a static's address, which would differ per patch and falsely
// reset on every stable-shape patch.
#if defined(_MSC_VER)
#define RUITK_CELL_SIG __FUNCSIG__
#else
#define RUITK_CELL_SIG __PRETTY_FUNCTION__
#endif
#define RUITK_HOOK_CELL_TYPE()                                                                                         \
	static uint32 StaticTypeHash()                                                                                     \
	{                                                                                                                  \
		static const uint32 CellTypeHash = FCrc::StrCrc32(RUITK_CELL_SIG);                                             \
		return CellTypeHash;                                                                                           \
	}                                                                                                                  \
	virtual uint32 TypeHash() const override                                                                           \
	{                                                                                                                  \
		return StaticTypeHash();                                                                                       \
	}

struct IRuitkHookCell
{
	virtual ~IRuitkHookCell() = default;
	virtual ERuitkHookKind GetKind() const = 0;
	/** TB-13: exact concrete-type identity (see RUITK_HOOK_CELL_TYPE) — accessors verify it
	 *  BEFORE downcasting; a mismatch truncates the cell tail instead of corrupting memory. */
	virtual uint32 TypeHash() const = 0;

	/** TB-17 — the HMR rule "preserve state, recompute DERIVATIONS": a Live Coding patch may
	 *  have replaced the code that derives a cached value (a UseMemo factory), so derived
	 *  caches must follow the code. Called on every cell at the first render after a patch;
	 *  memo-family cells unset their remembered deps (unset ⇒ DepsChanged ⇒ the freshly
	 *  patched factory re-runs); user STATE (State/Ref/Reducer) stays untouched. */
	virtual void HmrInvalidateDerived() {}

	/** Export this cell's value as an FRuitkValue when the payload type round-trips through the
	 *  variant (TD-019). Only STATE cells whose T is FRuitkValue-constructible answer true; the
	 *  compiled→interp HMR swap uses this to MIGRATE typed state into the interpreter's
	 *  FRuitkValue cells instead of hard-resetting it. false = not migratable (re-init from Init). */
	virtual bool ExportRuitkValue(FRuitkValue& Out) const { return false; }
};

template <typename T> struct TRuitkStateCell final : IRuitkHookCell
{
	T Value;
	explicit TRuitkStateCell(T InValue) : Value(MoveTemp(InValue)) {}
	virtual ERuitkHookKind GetKind() const override { return ERuitkHookKind::State; }
	RUITK_HOOK_CELL_TYPE()

	virtual bool ExportRuitkValue(FRuitkValue& Out) const override
	{
		// Numeric / bool / string / text / vector2 / color state round-trips; container or
		// opaque state (e.g. TArray<FString>) does not construct an FRuitkValue → stays reset.
		if constexpr (std::is_constructible_v<FRuitkValue, const T&>)
		{
			Out = FRuitkValue(Value);
			return true;
		}
		else
		{
			return false;
		}
	}
};

template <typename T, typename TAction> struct TRuitkReducerCell final : IRuitkHookCell
{
	T Value;
	TFunction<T(const T&, const TAction&)> Reducer; // refreshed every render (family parity)
	explicit TRuitkReducerCell(T InValue) : Value(MoveTemp(InValue)) {}
	virtual ERuitkHookKind GetKind() const override { return ERuitkHookKind::Reducer; }
	RUITK_HOOK_CELL_TYPE()
};

/** UseRef box — stable across renders; mutating Current never re-renders. */
template <typename T> struct TRuitkRef
{
	T Current{};
};

template <typename T> struct TRuitkRefCell final : IRuitkHookCell
{
	TSharedRef<TRuitkRef<T>> Box;
	explicit TRuitkRefCell(T Initial) : Box(MakeShared<TRuitkRef<T>>()) { Box->Current = MoveTemp(Initial); }
	virtual ERuitkHookKind GetKind() const override { return ERuitkHookKind::Ref; }
	RUITK_HOOK_CELL_TYPE()
};

template <typename T> struct TRuitkMemoCell final : IRuitkHookCell
{
	T Value;
	FRuitkDeps LastDeps;
	virtual void HmrInvalidateDerived() override { LastDeps.Reset(); } // TB-17: patched factory re-runs
	virtual ERuitkHookKind GetKind() const override { return ERuitkHookKind::Memo; }
	RUITK_HOOK_CELL_TYPE()
};

template <typename T> struct TRuitkDeferredCell final : IRuitkHookCell
{
	T Value{};
	T Target{};
	FRuitkDeps Deps;
	virtual void HmrInvalidateDerived() override { Deps.Reset(); } // TB-17
	bool bPending = false;
	virtual ERuitkHookKind GetKind() const override { return ERuitkHookKind::Deferred; }
	RUITK_HOOK_CELL_TYPE()
};

struct FRuitkTransitionCell final : IRuitkHookCell
{
	virtual ERuitkHookKind GetKind() const override { return ERuitkHookKind::Transition; }
	RUITK_HOOK_CELL_TYPE()
};

/** Stable-callback cell: the WRAPPER's identity never changes; the inner body is refreshed
 *  every render (useStableCallback/Func/Action). */
struct FRuitkStableCell final : IRuitkHookCell
{
	TSharedRef<TFunction<void(const FRuitkValue&)>> Inner = MakeShared<TFunction<void(const FRuitkValue&)>>();
	FRuitkCallback Wrapper; // minted once, reads Inner
	virtual ERuitkHookKind GetKind() const override { return ERuitkHookKind::Stable; }
	RUITK_HOOK_CELL_TYPE()
};

/** Order-validation-only cell (UseSafeArea / UseSfx record their slot). */
struct FRuitkMarkerCell final : IRuitkHookCell
{
	ERuitkHookKind Kind;
	bool bWarned = false; // warn-once for not-yet-wired stubs
	explicit FRuitkMarkerCell(ERuitkHookKind InKind) : Kind(InKind) {}
	virtual ERuitkHookKind GetKind() const override { return Kind; }
	RUITK_HOOK_CELL_TYPE()
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// Animation hooks (UseTween/UseAnimate/UseTweenValue)
// ─────────────────────────────────────────────────────────────────────────────────────────

enum class ERuitkEase : uint8
{
	Linear,
	In,	   // cubic
	Out,   // cubic
	InOut, // cubic
};

/** The tween slot: retarget-from-current, host-clock driven (see FRuitkContext::TweenSlot). */
template <typename T> struct TRuitkTweenCell final : IRuitkHookCell
{
	ERuitkHookKind Kind;
	T From{};
	T To{};
	T Current{};
	double StartTime = 0.0;
	float Duration = 0.25f;
	bool bActive = false;
	explicit TRuitkTweenCell(ERuitkHookKind InKind) : Kind(InKind) {}
	virtual ERuitkHookKind GetKind() const override { return Kind; }
	RUITK_HOOK_CELL_TYPE()
};

namespace Ruitk
{
	inline float ApplyEase(ERuitkEase Ease, float T)
	{
		switch (Ease)
		{
		case ERuitkEase::Linear:
			return T;
		case ERuitkEase::In:
			return T * T * T;
		case ERuitkEase::Out:
		{
			const float Inv = 1.0f - T;
			return 1.0f - Inv * Inv * Inv;
		}
		case ERuitkEase::InOut:
			return T < 0.5f ? 4.0f * T * T * T : 1.0f - FMath::Pow(-2.0f * T + 2.0f, 3.0f) / 2.0f;
		}
		return T;
	}

	inline float LerpTween(float From, float To, float Alpha)
	{
		return FMath::Lerp(From, To, Alpha);
	}
	inline FVector2D LerpTween(const FVector2D& From, const FVector2D& To, float Alpha)
	{
		return FMath::Lerp(From, To, Alpha);
	}
	inline FLinearColor LerpTween(const FLinearColor& From, const FLinearColor& To, float Alpha)
	{
		return FLinearColor::LerpUsingHSV(From, To, Alpha);
	}

	/** The process-wide UseSfx sink: the game registers HOW a bus plays (world context,
	 *  audio assets). Unset = quiet no-op. */
	RUITKCORE_API void SetSfxSink(TFunction<void(FName Bus, const FRuitkValue& Payload)> Sink);
	RUITKCORE_API void DispatchSfx(FName Bus, const FRuitkValue& Payload);
} // namespace Ruitk

// ─────────────────────────────────────────────────────────────────────────────────────────
// Effects (recorded during render, run during commit — reconciler drives)
// ─────────────────────────────────────────────────────────────────────────────────────────

using FRuitkEffectCleanup = TFunction<void()>;

struct FRuitkEffect
{
	TFunction<FRuitkEffectCleanup()> Factory;
	FRuitkDeps Deps;	 // this render's deps
	FRuitkDeps LastDeps; // deps at last run (unset = never ran)
	FRuitkEffectCleanup Cleanup;
	bool bEverRan = false;
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// Setter handle
// ─────────────────────────────────────────────────────────────────────────────────────────

/**
 * The value+setter pair's setter half: a small copyable handle {weak state, slot}. Safe
 * after teardown (weak + slot-count guard — the family's [audit C3]); stable identity ==
 * the (state, slot) pair, so passing setters as props stays memo-friendly.
 * Implementation lives with FRuitkComponentState (needs its definition).
 */
template <typename T> class TRuitkSetter
{
public:
	TRuitkSetter() = default;
	TRuitkSetter(TWeakPtr<FRuitkComponentState> InState, int32 InSlot) : State(MoveTemp(InState)), Slot(InSlot) {}

	void operator()(T NewValue) const;					   // set value
	void operator()(TFunction<T(const T&)> Updater) const; // functional update

	bool operator==(const TRuitkSetter& Other) const { return Slot == Other.Slot && State == Other.State; }

private:
	TWeakPtr<FRuitkComponentState> State;
	int32 Slot = INDEX_NONE;
};
