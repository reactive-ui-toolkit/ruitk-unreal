// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuitkContext.h"
#include "RuitkReconciler.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuitkCoreHooks, Log, All);

const TCHAR* RuitkHookKindName(ERuitkHookKind Kind)
{
	switch (Kind)
	{
	case ERuitkHookKind::State:
		return TEXT("state");
	case ERuitkHookKind::Reducer:
		return TEXT("reducer");
	case ERuitkHookKind::Ref:
		return TEXT("ref");
	case ERuitkHookKind::Memo:
		return TEXT("memo");
	case ERuitkHookKind::Deferred:
		return TEXT("deferred");
	case ERuitkHookKind::Transition:
		return TEXT("transition");
	case ERuitkHookKind::Stable:
		return TEXT("stable");
	case ERuitkHookKind::SafeArea:
		return TEXT("safe_area");
	case ERuitkHookKind::Signal:
		return TEXT("signal");
	case ERuitkHookKind::Tween:
		return TEXT("tween");
	case ERuitkHookKind::TweenValue:
		return TEXT("tween_value");
	case ERuitkHookKind::Animate:
		return TEXT("animate");
	case ERuitkHookKind::Sfx:
		return TEXT("sfx");
	case ERuitkHookKind::Effect:
		return TEXT("effect");
	case ERuitkHookKind::LayoutEffect:
		return TEXT("layout_effect");
	}
	return TEXT("?");
}

TWeakPtr<FRuitkComponentState> FRuitkContext::StateWeak() const
{
	return StateShared;
}

void FRuitkContext::Record(ERuitkHookKind Kind)
{
	// TB-13: the HMR session needs the flattened hook sequence too — shape-change detection
	// across a Live Coding patch is what drives the family reset rule.
	if (FRuitkConfig::IsHookValidationEnabled() || Ruitk::IsHmrHookTracking())
	{
		State.HookLog.Add(Kind);
	}
}

void FRuitkContext::WarnOnce(FName Key, const FString& Msg)
{
	if (State.DiagWarned.Contains(Key))
	{
		return;
	}
	State.DiagWarned.Add(Key);
	FRuitkDiagnostics::Emit(Msg);
	UE_LOG(LogRuitkCoreHooks, Warning, TEXT("%s"), *Msg);
}

void FRuitkContext::NotifyEffects()
{
	Reconciler.NotifyEffectKinds(Fiber, !State.Effects.IsEmpty(), !State.LayoutEffects.IsEmpty());
}

void FRuitkContext::ReconcilerOnProvidedChanged(const void* Key)
{
	Reconciler.OnProvidedValueChanged(Fiber, Key);
}

void FRuitkContext::StubSlot(ERuitkHookKind Kind, const TCHAR* HookName, const TCHAR* Owner)
{
	Record(Kind);
	const int32 i = State.HookIndex++;
	State.EnsureCellShape(i, FRuitkMarkerCell::StaticTypeHash(), Kind); // TB-13
	if (i >= State.Hooks.Num())
	{
		State.Hooks.Emplace(MakeUnique<FRuitkMarkerCell>(Kind));
	}
	FRuitkMarkerCell* Cell = static_cast<FRuitkMarkerCell*>(State.Hooks[i].Get());
	if (!Cell->bWarned)
	{
		Cell->bWarned = true;
		WarnOnce(FName(HookName),
				 FString::Printf(TEXT("[Ruitk] %s is a Phase-owned stub (fully wired in %s; the v1 ship gate "
									  "forbids stubs). It consumes its hook slot but does nothing yet."),
								 HookName, Owner));
	}
}

// ── UseSfx sink (RuitkHooksInternal.h) ───────────────────────────────────────────────────────

namespace
{
	TFunction<void(FName, const FRuitkValue&)>& SfxSinkRef()
	{
		static TFunction<void(FName, const FRuitkValue&)> Sink;
		return Sink;
	}
} // namespace

namespace Ruitk
{
	void SetSfxSink(TFunction<void(FName Bus, const FRuitkValue& Payload)> Sink)
	{
		SfxSinkRef() = MoveTemp(Sink);
	}

	void DispatchSfx(FName Bus, const FRuitkValue& Payload)
	{
		if (const TFunction<void(FName, const FRuitkValue&)>& Sink = SfxSinkRef())
		{
			Sink(Bus, Payload);
		}
	}
} // namespace Ruitk
