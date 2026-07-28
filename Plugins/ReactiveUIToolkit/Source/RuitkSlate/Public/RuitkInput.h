// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-004 (input APIs, keyboard-shortcut half): Ruitk::Slate::UseShortcut registers a key-chord ->
// callback for a component's lifetime via a Slate input pre-processor, cleaned up on unmount. The
// LATEST callback fires (a stable ref box refreshed each render), and the pre-processor re-registers
// only when the CHORD changes. Drag-and-drop (the other half of TD-004) is tracked separately.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "RuitkTypes.h" // FRuitkHostHandle

class FRuitkContext;
struct FKeyEvent;

namespace Ruitk::Slate
{
	/** A keyboard chord: a key plus required modifier state (all must match exactly). */
	struct RUITKSLATE_API FRuitkShortcut
	{
		FKey Key;
		bool bCtrl = false;
		bool bShift = false;
		bool bAlt = false;
		bool bCmd = false;

		/** True when the event's key + modifier state match this chord exactly. */
		bool Matches(const FKeyEvent& Event) const;

		/** A stable hash of the chord — the UseEffect dep so it re-registers only on a chord change. */
		int32 DepKey() const;
	};

	/** Register `OnTrigger` to fire when `Chord` is pressed, for the calling component's lifetime.
	 *  Requires a running Slate application (a no-op headless without one). */
	RUITKSLATE_API void UseShortcut(FRuitkContext& Ctx, const FRuitkShortcut& Chord, TFunction<void()> OnTrigger);

	// ── TD-022 (focus extensions): programmatic focus over a widget ref ─────────────────────

	/**
	 * A focus handle: attach `Ref` to the target element's `ref=` prop, then call `Focus()` to
	 * move keyboard focus there and `IsFocused()` to query it. The React ref lifecycle keeps the
	 * captured widget in sync (attached on mount, cleared on unmount). Headless-safe (no-ops
	 * without a running Slate application).
	 */
	struct RUITKSLATE_API FRuitkFocusHandle
	{
		TFunction<void(const FRuitkHostHandle&)> Ref;
		TFunction<void()> Focus;
		TFunction<bool()> IsFocused;
	};

	/** Stable focus handle for the calling component (a UseRef under the hood). */
	RUITKSLATE_API FRuitkFocusHandle UseFocus(FRuitkContext& Ctx);

	/** Imperative focus/blur on a mounted host handle (e.g. from an effect or event). */
	RUITKSLATE_API void FocusWidget(const FRuitkHostHandle& Handle);
	RUITKSLATE_API void ClearFocus();
} // namespace Ruitk::Slate
