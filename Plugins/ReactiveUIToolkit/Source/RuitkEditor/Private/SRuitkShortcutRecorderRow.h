// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// One rebindable-shortcut recorder row (label + "click, then press a key combo" button + clear) —
// extracted from SRuitkUetkxHmrPanel when the settings moved into the Reactive UI Toolkit Settings
// window (the one-settings-window design), so any panel can reuse it instead of duplicating the
// capture logic. Self-contained: the row itself takes keyboard focus while recording (Esc cancels,
// losing focus cancels). It edits the command's ACTIVE chord via FInputBindingManager — the single
// store (EditorKeyBindings.ini, shared with Editor Preferences ▸ Keyboard Shortcuts); nothing is
// persisted here.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FUICommandInfo;

class SRuitkShortcutRecorderRow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRuitkShortcutRecorderRow) : _LabelWidth(84.0f) {}
	SLATE_ARGUMENT(FText, Label)							  // row label ("Toggle HMR", …)
	SLATE_ARGUMENT(TSharedPtr<class FUICommandInfo>, Command) // the command whose chord this row edits
	SLATE_ARGUMENT(float, LabelWidth)						  // fixed label column, so stacked rows align
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// Keyboard capture while recording.
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
	virtual void OnFocusLost(const FFocusEvent& FocusEvent) override;

private:
	FText GetShortcutText() const;
	FReply OnRecordClicked();
	FReply OnClearClicked();

	TSharedPtr<FUICommandInfo> Command;
	bool bRecording = false; // this row is capturing the next key press
};
