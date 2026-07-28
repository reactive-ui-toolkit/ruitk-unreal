// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-021 — the CommonUI activation seam, Ruitk side. A Reactive UI Toolkit tree hosted inside a CommonUI
// activatable needs to REACT to activation (is this screen the active one on the stack?) and to the
// current input method (mouse/keyboard vs gamepad vs touch — CommonUI's whole reason for being). We
// surface both through a context the host provides and hooks the tree reads:
//
//   URuitkActivatableScreen  --provides-->  FRuitkActivationState (via ActivationProvider)
//   your component         --reads----->  UseActivation / UseIsActive / UseInputMethod
//
// The mechanism is plain Reactive UI Toolkit context (no UObject dependency), so it is fully unit-testable
// headless; the UObject (RuitkActivatableScreen.h) is only the CommonUI-side data source that flips
// the state and re-renders.

#pragma once

#include "CoreMinimal.h"
#include "RuitkContextHandle.h" // TRuitkContext
#include "RuitkNode.h"

class FRuitkContext;

/** The input device family currently driving the UI (mirrors CommonUI's ECommonInputType). */
enum class ERuitkInputMethod : uint8
{
	MouseAndKeyboard,
	Gamepad,
	Touch,
};

/** What a hosted Reactive UI Toolkit tree can learn about its CommonUI screen. */
struct RUITKCOMMONUI_API FRuitkActivationState
{
	bool bActive = false; // is this the active screen on its activatable stack?
	ERuitkInputMethod InputMethod = ERuitkInputMethod::MouseAndKeyboard;

	bool operator==(const FRuitkActivationState& Other) const
	{
		return bActive == Other.bActive && InputMethod == Other.InputMethod;
	}
	bool operator!=(const FRuitkActivationState& Other) const { return !(*this == Other); }
};

/** TD-029 — the screen's desired-focus registry. The hosted tree designates its initial gamepad
 *  focus with UseDesiredFocus; the screen's CommonUI `GetDesiredFocusTarget()` contract then has
 *  somewhere to land (URuitkActivatableScreen returns itself and forwards the received focus by
 *  invoking `FocusDesired`). Owned by the screen; reaches the tree via FocusTargetContext. */
struct RUITKCOMMONUI_API FRuitkFocusTargetRegistry
{
	/** Moves focus to the designated widget (typically a Ruitk::Slate::UseFocus handle's Focus).
	 *  Unset when nothing is designated. */
	TFunction<void()> FocusDesired;

	bool HasTarget() const { return static_cast<bool>(FocusDesired); }
};

namespace Ruitk::CommonUI
{
	/** The context handle carrying activation state to descendants (default = inactive / M&K). */
	RUITKCOMMONUI_API TRuitkContext<FRuitkActivationState>& ActivationContext();

	/** Read the full activation state at this point in the tree. */
	RUITKCOMMONUI_API FRuitkActivationState UseActivation(FRuitkContext& Ctx);

	/** Sugar: is the hosting screen active? */
	RUITKCOMMONUI_API bool UseIsActive(FRuitkContext& Ctx);

	/** Sugar: the current input method. */
	RUITKCOMMONUI_API ERuitkInputMethod UseInputMethod(FRuitkContext& Ctx);

	/** Provide `State` to `Children` (what the activatable host wraps the component in). */
	RUITKCOMMONUI_API FRuitkNode ActivationProvider(FRuitkActivationState State,
													TArray<FRuitkNode> Children = TArray<FRuitkNode>(),
													FRuitkKey Key = FRuitkKey());

	// ── TD-029: desired focus ─────────────────────────────────────────────────────────────

	/** The context carrying the screen's focus registry (null outside an activatable host). */
	RUITKCOMMONUI_API TRuitkContext<TSharedPtr<FRuitkFocusTargetRegistry>>& FocusTargetContext();

	/** Designate this component's widget as the screen's desired (gamepad) focus target.
	 *  Pass a callable that moves focus there — typically `Handle.Focus` from
	 *  `Ruitk::Slate::UseFocus` (whose `Ref` you attached to the widget's props). Latest call
	 *  per commit wins; the designation clears on unmount. Call unconditionally (hook rules);
	 *  outside a screen (no registry in context) it is a quiet no-op. */
	RUITKCOMMONUI_API void UseDesiredFocus(FRuitkContext& Ctx, TFunction<void()> FocusAction);

	/** Provide `Registry` to `Children` (what URuitkActivatableScreen wraps the component in). */
	RUITKCOMMONUI_API FRuitkNode FocusTargetProvider(TSharedPtr<FRuitkFocusTargetRegistry> Registry,
													 TArray<FRuitkNode> Children = TArray<FRuitkNode>(),
													 FRuitkKey Key = FRuitkKey());
} // namespace Ruitk::CommonUI
