// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Typed context handles — the family's RuitkContext (object-identity keys + default value)
// upgraded with React's O(1) read path (D-08.3): each handle carries a RENDER-PHASE value
// stack; provider fibers push on descend and pop on ascend (including bailout-skip paths —
// the pop lives in the reconciler's ascend loop, the classic correctness trap). Bailout
// re-checks and eager propagation still walk the COMMITTED tree via the fibers' provided
// maps (Godot semantics, which is what makes subtree-skip safe).

#pragma once

#include "CoreMinimal.h"

struct FRuitkFiber;

/** Type-erased provided value stored on a provider fiber. One per (fiber, context). */
class IRuitkProvidedValue
{
public:
	virtual ~IRuitkProvidedValue() = default;

	/** Push/pop this value on its handle's render stack (reconciler descend/ascend). */
	virtual void PushOnRenderStack() = 0;
	virtual void PopFromRenderStack() = 0;

	/** Value-equality vs another provided holder of the SAME context (change detection for
	 *  eager propagation). */
	virtual bool ValueEquals(const IRuitkProvidedValue& Other) const = 0;

	/** The handle's identity key. */
	virtual const void* GetContextKey() const = 0;

	/** Carry for fiber reuse (providers keep providing across bailouts — [audit C1]). */
	virtual TSharedPtr<IRuitkProvidedValue> Duplicate() const = 0;
};

template <typename T> class TRuitkContextCore
{
public:
	explicit TRuitkContextCore(T InDefault, FName InName) : Default(MoveTemp(InDefault)), DebugName(InName) {}

	T Default;
	FName DebugName;

	/** Render-phase provider stack (game-thread only, like everything render). */
	TArray<const T*> RenderStack;
};

/**
 * The context handle — create once (static/module scope), share everywhere:
 *
 *   static const TRuitkContext<FMyTheme> ThemeCtx(FMyTheme{...}, TEXT("Theme"));
 *   Ctx.ProvideContext(ThemeCtx, Theme);          // in the provider component
 *   const FMyTheme& Theme = Ctx.UseContext(ThemeCtx);  // in consumers
 */
template <typename T> class TRuitkContext
{
public:
	explicit TRuitkContext(T InDefault = T(), FName InName = NAME_None)
		: Core(MakeShared<TRuitkContextCore<T>>(MoveTemp(InDefault), InName))
	{
	}

	/** Identity key (object identity of the shared core — distinct handles never collide). */
	const void* Key() const { return &Core.Get(); }

	const T& GetDefault() const { return Core->Default; }

	TRuitkContextCore<T>& GetCore() const { return Core.Get(); }

private:
	TSharedRef<TRuitkContextCore<T>> Core;
};

/** The typed provided-value holder (created by ProvideContext). */
template <typename T> class TRuitkProvidedValue final : public IRuitkProvidedValue
{
public:
	// Initializer order matches DECLARATION order (Value before Handle) — C5038-clean.
	TRuitkProvidedValue(const TRuitkContext<T>& InHandle, T InValue) : Value(MoveTemp(InValue)), Handle(InHandle) {}

	T Value;

	virtual void PushOnRenderStack() override { Handle.GetCore().RenderStack.Push(&Value); }
	virtual void PopFromRenderStack() override { Handle.GetCore().RenderStack.Pop(EAllowShrinking::No); }

	virtual bool ValueEquals(const IRuitkProvidedValue& Other) const override
	{
		// Same-context invariant (keys matched before calling) — the cast is sound.
		return Value == static_cast<const TRuitkProvidedValue<T>&>(Other).Value;
	}

	virtual const void* GetContextKey() const override { return Handle.Key(); }

	virtual TSharedPtr<IRuitkProvidedValue> Duplicate() const override
	{
		return MakeShared<TRuitkProvidedValue<T>>(Handle, Value);
	}

	const TRuitkContext<T> Handle;
};
