// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "SRuitkShortcutRecorderRow.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/InputBindingManager.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "RuitkUetkx"

void SRuitkShortcutRecorderRow::Construct(const FArguments& InArgs)
{
	Command = InArgs._Command;

	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 9);
	ChildSlot[SNew(SHorizontalBox) +
			  SHorizontalBox::Slot()
				  .AutoWidth()
				  .VAlign(VAlign_Center)
				  .Padding(0, 0, 8, 0)[SNew(SBox).WidthOverride(
					  InArgs._LabelWidth)[SNew(STextBlock).Font(Font).Text(InArgs._Label)]] +
			  SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				  [SNew(SButton)
					   .ToolTipText(LOCTEXT("RecordTip", "Click, then press a key combo (Esc to cancel)."))
					   .OnClicked(this, &SRuitkShortcutRecorderRow::OnRecordClicked)
						   [SNew(STextBlock).Font(Font).Text(this, &SRuitkShortcutRecorderRow::GetShortcutText)]] +
			  SHorizontalBox::Slot()
				  .AutoWidth()
				  .VAlign(VAlign_Center)
				  .Padding(4, 0, 0, 0)[SNew(SButton)
										   .ToolTipText(LOCTEXT("ClearTip", "Clear this shortcut (unbind)."))
										   .OnClicked(this, &SRuitkShortcutRecorderRow::OnClearClicked)
											   [SNew(STextBlock).Font(Font).Text(LOCTEXT("ClearX", "×"))]]];
}

FText SRuitkShortcutRecorderRow::GetShortcutText() const
{
	if (bRecording)
	{
		return LOCTEXT("PressAKey", "press a key…");
	}
	const FText Chord = Command.IsValid() ? Command->GetInputText() : FText::GetEmpty();
	return Chord.IsEmpty() ? LOCTEXT("None", "None") : Chord;
}

FReply SRuitkShortcutRecorderRow::OnRecordClicked()
{
	bRecording = true;
	// Route the next key press to this row so OnKeyDown captures the chord.
	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly);
	return FReply::Handled();
}

FReply SRuitkShortcutRecorderRow::OnClearClicked()
{
	if (Command.IsValid())
	{
		Command->RemoveActiveChord(EMultipleKeyBindingIndex::Primary);
		FInputBindingManager::Get().NotifyActiveChordChanged(*Command, EMultipleKeyBindingIndex::Primary);
	}
	bRecording = false;
	return FReply::Handled();
}

FReply SRuitkShortcutRecorderRow::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	if (!bRecording)
	{
		return SCompoundWidget::OnKeyDown(Geometry, KeyEvent);
	}
	const FKey Key = KeyEvent.GetKey();
	if (Key == EKeys::Escape)
	{
		bRecording = false; // cancel, leave the binding unchanged
		return FReply::Handled();
	}
	if (Key.IsModifierKey())
	{
		return FReply::Handled(); // wait for the real key that completes the chord
	}
	const FInputChord Chord(Key, KeyEvent.IsShiftDown(), KeyEvent.IsControlDown(), KeyEvent.IsAltDown(),
							KeyEvent.IsCommandDown());
	if (Command.IsValid())
	{
		Command->SetActiveChord(Chord, EMultipleKeyBindingIndex::Primary);
		FInputBindingManager::Get().NotifyActiveChordChanged(*Command, EMultipleKeyBindingIndex::Primary);
	}
	bRecording = false;
	return FReply::Handled();
}

void SRuitkShortcutRecorderRow::OnFocusLost(const FFocusEvent& FocusEvent)
{
	SCompoundWidget::OnFocusLost(FocusEvent);
	bRecording = false; // clicking elsewhere can no longer capture — stop advertising "press a key…"
}

#undef LOCTEXT_NAMESPACE
