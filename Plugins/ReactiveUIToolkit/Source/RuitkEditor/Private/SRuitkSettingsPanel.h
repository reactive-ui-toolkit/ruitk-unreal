// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// The Reactive UI Toolkit Settings window (nomad tab) — the family-wide "one custom settings
// window per leg" design: every setting of the plugin in one place, in sections, opened from the
// Reactive UI Toolkit main menu. Two IDetailsView panels over the SAME settings CDOs the Project
// Settings pages edit (UDeveloperSettings auto-registration stays, so those pages remain as free
// mirrors — zero duplicate UI code):
//   Runtime            — URuitkSettings           (the six ruitk.* CVar-backed reconciler knobs)
//   Editor / Hot Reload — URuitkUetkxEditorSettings (the seven HMR knobs) + the two rebindable
//                         shortcut rows (SRuitkShortcutRecorderRow; the binding manager persists
//                         chords — they are not config properties).
// Each panel persists exactly the way its class already persists — see the
// OnFinishedChangingProperties handlers in the .cpp; no new save path was invented.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

struct FPropertyChangedEvent;

class SRuitkSettingsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRuitkSettingsPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** Runtime panel edit landed → persist URuitkSettings the way Project Settings does. */
	void OnRuntimeSettingsChanged(const FPropertyChangedEvent& Event);

	/** Editor panel edit landed → persist URuitkUetkxEditorSettings the way the HMR window's
	 *  checkboxes always did (+ the live console-hider refresh they performed). */
	void OnEditorSettingsChanged(const FPropertyChangedEvent& Event);
};
