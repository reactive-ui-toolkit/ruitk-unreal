// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "SRuitkUetkxHmrPanel.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/InputBindingManager.h"
#include "Framework/Commands/UICommandInfo.h"
#include "HAL/PlatformMemory.h"
#include "Logging/LogMacros.h"
#include "RuitkUetkxCommands.h"
#include "RuitkUetkxEditorSettings.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "UetkxHmrController.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "RuitkUetkx"

DEFINE_LOG_CATEGORY_STATIC(LogRuitkHmrPanel, Log, All);

namespace
{
	FText RamToText(uint64 Bytes)
	{
		return FText::AsNumber(static_cast<int64>(Bytes / (1024 * 1024)));
	}
} // namespace

void SRuitkUetkxHmrPanel::Construct(const FArguments&)
{
	BaselineRamBytes = FPlatformMemory::GetStats().UsedPhysical;

	FUetkxHmrController& Controller = FUetkxHmrController::Get();
	StatusChangedHandle = Controller.OnStatusChanged.AddRaw(this, &SRuitkUetkxHmrPanel::OnControllerStatusChanged);

	const FSlateFontInfo StatFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);
	const FSlateFontInfo HeadFont = FCoreStyle::GetDefaultFontStyle("Bold", 11);

	auto StatRow = [StatFont](const FText& Label, TAttribute<FText> Value) -> TSharedRef<SWidget>
	{
		return SNew(SHorizontalBox) +
			   SHorizontalBox::Slot().AutoWidth().Padding(
				   0, 0, 8, 0)[SNew(SBox).WidthOverride(64)[SNew(STextBlock).Font(StatFont).Text(Label)]] +
			   SHorizontalBox::Slot().FillWidth(1.0f)[SNew(STextBlock).Font(StatFont).Text(Value)];
	};

	ChildSlot[SNew(SBox)
				  .MinDesiredWidth(600) // opens at ~600x500 when floating; still user-resizable
				  .MinDesiredHeight(500)
					  [SNew(SBorder)
						   .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						   .Padding(12)
							   [SNew(SVerticalBox)
								// ── Start / Stop ─────────────────────────────────────────────────────────
								+ SVerticalBox::Slot().AutoHeight().Padding(
									  0, 0, 0, 8)[SNew(SButton)
													  .HAlign(HAlign_Center)
													  .VAlign(VAlign_Center)
													  .ContentPadding(FMargin(16, 6))
													  .OnClicked(this, &SRuitkUetkxHmrPanel::OnToggleClicked)
														  [SNew(STextBlock)
															   .Font(HeadFont)
															   .Text(this, &SRuitkUetkxHmrPanel::GetToggleLabel)]]
								// ── ACTIVE / Idle ────────────────────────────────────────────────────────
								+ SVerticalBox::Slot()
									  .AutoHeight()
									  .HAlign(HAlign_Center)
									  .Padding(0, 0, 0,
											   10)[SNew(STextBlock)
													   .Font(HeadFont)
													   .ColorAndOpacity(this, &SRuitkUetkxHmrPanel::GetStateColor)
													   .Text(this, &SRuitkUetkxHmrPanel::GetStateText)]
								// ── stats ────────────────────────────────────────────────────────────────
								+ SVerticalBox::Slot().AutoHeight().Padding(0, 1)[StatRow(
									  LOCTEXT("Watched", "Watched"),
									  FText::FromString(TEXT("Source/**/*.uetkx, Plugins/**/*.uetkx")))] +
								SVerticalBox::Slot().AutoHeight().Padding(
									0, 1)[StatRow(LOCTEXT("Swaps", "Swaps"),
												  TAttribute<FText>(this, &SRuitkUetkxHmrPanel::GetSwapsText))] +
								SVerticalBox::Slot().AutoHeight().Padding(
									0, 1)[StatRow(LOCTEXT("Errors", "Errors"),
												  TAttribute<FText>(this, &SRuitkUetkxHmrPanel::GetErrorsText))] +
								SVerticalBox::Slot().AutoHeight().Padding(
									0, 1)[StatRow(LOCTEXT("Last", "Last"),
												  TAttribute<FText>(this, &SRuitkUetkxHmrPanel::GetLastText))] +
								SVerticalBox::Slot().AutoHeight().Padding(
									0, 1)[StatRow(LOCTEXT("Ram", "RAM"),
												  TAttribute<FText>(this, &SRuitkUetkxHmrPanel::GetRamText))]
								// ── settings ─────────────────────────────────────────────────────────────
								+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 1)
									  [SNew(SCheckBox)
										   .IsChecked(this, &SRuitkUetkxHmrPanel::IsNotificationsChecked)
										   .OnCheckStateChanged(this, &SRuitkUetkxHmrPanel::OnNotificationsChanged)
											   [SNew(STextBlock)
													.Font(StatFont)
													.Text(LOCTEXT("ShowNotifs", "Show swap notifications"))]] +
								SVerticalBox::Slot().AutoHeight().Padding(
									0, 1)[SNew(SCheckBox)
											  .IsChecked(this, &SRuitkUetkxHmrPanel::IsVerboseChecked)
											  .OnCheckStateChanged(this, &SRuitkUetkxHmrPanel::OnVerboseChanged)
												  [SNew(STextBlock)
													   .Font(StatFont)
													   .Text(LOCTEXT("Verbose", "Verbose watcher trace"))]] +
								SVerticalBox::Slot().AutoHeight().Padding(
									0, 1)[SNew(SCheckBox)
											  .IsChecked(this, &SRuitkUetkxHmrPanel::IsHideConsoleChecked)
											  .OnCheckStateChanged(this, &SRuitkUetkxHmrPanel::OnHideConsoleChanged)
												  [SNew(STextBlock)
													   .Font(StatFont)
													   .Text(LOCTEXT("HideConsole", "Hide the Live Coding console "
																					"while HMR is active"))]] +
								SVerticalBox::Slot().AutoHeight().Padding(
									0, 1)[SNew(SCheckBox)
											  .IsChecked(this, &SRuitkUetkxHmrPanel::IsFollowPieChecked)
											  .OnCheckStateChanged(this, &SRuitkUetkxHmrPanel::OnFollowPieChanged)
												  [SNew(STextBlock)
													   .Font(StatFont)
													   .Text(LOCTEXT("FollowPie", "Follow Play: start HMR on Play, "
																				  "stop on Stop"))]]
								// ── rebindable shortcuts (default unbound) ─────────────────────────────────
								+ SVerticalBox::Slot().AutoHeight().Padding(
									  0, 6, 0, 1)[BuildShortcutRow(0, LOCTEXT("ToggleHmrShort", "Toggle HMR"))] +
								SVerticalBox::Slot().AutoHeight().Padding(
									0, 1)[BuildShortcutRow(1, LOCTEXT("ToggleWindowShort", "Open Window"))]
								// ── warning ──────────────────────────────────────────────────────────────
								+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 4)
									  [SNew(STextBlock)
										   .Font(StatFont)
										   .AutoWrapText(true)
										   .ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.75f, 0.20f)))
										   .Text(LOCTEXT("BuildPauseWarning", "⚠ External builds pause while HMR is "
																			  "active. Stop to build normally."))]
								// ── recent errors ────────────────────────────────────────────────────────
								+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 2)
									  [SNew(STextBlock).Font(HeadFont).Text(LOCTEXT("RecentErrors", "Recent Errors"))] +
								SVerticalBox::Slot().FillHeight(1.0f)[SNew(SBox).MinDesiredHeight(
									60)[SNew(SScrollBox) +
										SScrollBox::Slot()[SAssignNew(ErrorListBox, SVerticalBox)]]]]]];

	RebuildErrorList();
}

SRuitkUetkxHmrPanel::~SRuitkUetkxHmrPanel()
{
	if (StatusChangedHandle.IsValid())
	{
		FUetkxHmrController::Get().OnStatusChanged.Remove(StatusChangedHandle);
	}
}

FReply SRuitkUetkxHmrPanel::OnToggleClicked()
{
	FUetkxHmrController& Controller = FUetkxHmrController::Get();
	if (Controller.IsActive())
	{
		Controller.Stop();
	}
	else
	{
		FString Error;
		if (!Controller.Start(Error))
		{
			UE_LOG(LogRuitkHmrPanel, Warning, TEXT("[RUI HMR] could not start: %s"), *Error);
		}
	}
	return FReply::Handled();
}

FText SRuitkUetkxHmrPanel::GetToggleLabel() const
{
	return FUetkxHmrController::Get().IsActive() ? LOCTEXT("StopHmr", "■  Stop HMR")
												 : LOCTEXT("StartHmr", "●  Start HMR");
}

FText SRuitkUetkxHmrPanel::GetStateText() const
{
	const FUetkxHmrController& Controller = FUetkxHmrController::Get();
	if (!Controller.IsActive())
	{
		return LOCTEXT("Idle", "Idle");
	}
	return Controller.IsCompiling() ? LOCTEXT("Compiling", "● ACTIVE  (compiling…)") : LOCTEXT("Active", "● ACTIVE");
}

FSlateColor SRuitkUetkxHmrPanel::GetStateColor() const
{
	return FUetkxHmrController::Get().IsActive() ? FSlateColor(FLinearColor(0.30f, 0.85f, 0.35f))
												 : FSlateColor(FLinearColor(0.55f, 0.55f, 0.55f));
}

FText SRuitkUetkxHmrPanel::GetSwapsText() const
{
	return FText::AsNumber(FUetkxHmrController::Get().GetStatus().Swaps);
}

FText SRuitkUetkxHmrPanel::GetErrorsText() const
{
	return FText::AsNumber(FUetkxHmrController::Get().GetStatus().Errors);
}

FText SRuitkUetkxHmrPanel::GetLastText() const
{
	const FUetkxHmrStatus& Status = FUetkxHmrController::Get().GetStatus();
	if (Status.LastReason.IsEmpty())
	{
		return LOCTEXT("None", "—");
	}
	return FText::FromString(FString::Printf(TEXT("%s (%.0f ms)"), *Status.LastReason, Status.LastMs));
}

FText SRuitkUetkxHmrPanel::GetRamText() const
{
	const uint64 Now = FPlatformMemory::GetStats().UsedPhysical;
	const int64 DeltaMB = (static_cast<int64>(Now) - static_cast<int64>(BaselineRamBytes)) / (1024 * 1024);
	return FText::FromString(FString::Printf(TEXT("%s MB (%+lld since open)"), *RamToText(Now).ToString(), DeltaMB));
}

// ── settings checkboxes (bound to URuitkUetkxEditorSettings, persisted immediately) ──────────
ECheckBoxState SRuitkUetkxHmrPanel::IsNotificationsChecked() const
{
	return GetDefault<URuitkUetkxEditorSettings>()->bShowNotifications ? ECheckBoxState::Checked
																		  : ECheckBoxState::Unchecked;
}

void SRuitkUetkxHmrPanel::OnNotificationsChanged(ECheckBoxState NewState)
{
	URuitkUetkxEditorSettings* Settings = GetMutableDefault<URuitkUetkxEditorSettings>();
	Settings->bShowNotifications = (NewState == ECheckBoxState::Checked);
	Settings->SaveConfig();
}

ECheckBoxState SRuitkUetkxHmrPanel::IsVerboseChecked() const
{
	return GetDefault<URuitkUetkxEditorSettings>()->bVerboseWatcher ? ECheckBoxState::Checked
																	   : ECheckBoxState::Unchecked;
}

void SRuitkUetkxHmrPanel::OnVerboseChanged(ECheckBoxState NewState)
{
	URuitkUetkxEditorSettings* Settings = GetMutableDefault<URuitkUetkxEditorSettings>();
	Settings->bVerboseWatcher = (NewState == ECheckBoxState::Checked);
	Settings->SaveConfig();
}

ECheckBoxState SRuitkUetkxHmrPanel::IsHideConsoleChecked() const
{
	return GetDefault<URuitkUetkxEditorSettings>()->bHideLiveCodingConsole ? ECheckBoxState::Checked
																			  : ECheckBoxState::Unchecked;
}

void SRuitkUetkxHmrPanel::OnHideConsoleChanged(ECheckBoxState NewState)
{
	URuitkUetkxEditorSettings* Settings = GetMutableDefault<URuitkUetkxEditorSettings>();
	Settings->bHideLiveCodingConsole = (NewState == ECheckBoxState::Checked);
	Settings->SaveConfig();
	FUetkxHmrController::Get().RefreshConsoleHiderState(); // start/stop the window hider immediately if active
}

ECheckBoxState SRuitkUetkxHmrPanel::IsFollowPieChecked() const
{
	return GetDefault<URuitkUetkxEditorSettings>()->bFollowPie ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SRuitkUetkxHmrPanel::OnFollowPieChanged(ECheckBoxState NewState)
{
	URuitkUetkxEditorSettings* Settings = GetMutableDefault<URuitkUetkxEditorSettings>();
	Settings->bFollowPie = (NewState == ECheckBoxState::Checked);
	Settings->SaveConfig();
}

// ── in-window shortcut recorder (edits the command's active chord — the single source of truth) ──
TSharedPtr<FUICommandInfo> SRuitkUetkxHmrPanel::CommandFor(int32 RecordIndex) const
{
	const FRuitkUetkxCommands& Commands = FRuitkUetkxCommands::Get();
	return RecordIndex == 0 ? Commands.ToggleHmr : Commands.ToggleHmrWindow;
}

FText SRuitkUetkxHmrPanel::GetShortcutText(int32 RecordIndex) const
{
	if (RecordingIndex == RecordIndex)
	{
		return LOCTEXT("PressAKey", "press a key…");
	}
	TSharedPtr<FUICommandInfo> Command = CommandFor(RecordIndex);
	const FText Chord = Command.IsValid() ? Command->GetInputText() : FText::GetEmpty();
	return Chord.IsEmpty() ? LOCTEXT("None", "None") : Chord;
}

FReply SRuitkUetkxHmrPanel::OnRecordClicked(int32 RecordIndex)
{
	RecordingIndex = RecordIndex;
	// Route the next key press to this panel so OnKeyDown captures the chord.
	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly);
	return FReply::Handled();
}

FReply SRuitkUetkxHmrPanel::OnClearShortcut(int32 RecordIndex)
{
	if (TSharedPtr<FUICommandInfo> Command = CommandFor(RecordIndex))
	{
		Command->RemoveActiveChord(EMultipleKeyBindingIndex::Primary);
		FInputBindingManager::Get().NotifyActiveChordChanged(*Command, EMultipleKeyBindingIndex::Primary);
	}
	if (RecordingIndex == RecordIndex)
	{
		RecordingIndex = INDEX_NONE;
	}
	return FReply::Handled();
}

FReply SRuitkUetkxHmrPanel::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	if (RecordingIndex == INDEX_NONE)
	{
		return SCompoundWidget::OnKeyDown(Geometry, KeyEvent);
	}
	const FKey Key = KeyEvent.GetKey();
	if (Key == EKeys::Escape)
	{
		RecordingIndex = INDEX_NONE; // cancel, leave the binding unchanged
		return FReply::Handled();
	}
	if (Key.IsModifierKey())
	{
		return FReply::Handled(); // wait for the real key that completes the chord
	}
	const FInputChord Chord(Key, KeyEvent.IsShiftDown(), KeyEvent.IsControlDown(), KeyEvent.IsAltDown(),
							KeyEvent.IsCommandDown());
	if (TSharedPtr<FUICommandInfo> Command = CommandFor(RecordingIndex))
	{
		Command->SetActiveChord(Chord, EMultipleKeyBindingIndex::Primary);
		FInputBindingManager::Get().NotifyActiveChordChanged(*Command, EMultipleKeyBindingIndex::Primary);
	}
	RecordingIndex = INDEX_NONE;
	return FReply::Handled();
}

TSharedRef<SWidget> SRuitkUetkxHmrPanel::BuildShortcutRow(int32 RecordIndex, const FText& Label)
{
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 9);
	return SNew(SHorizontalBox) +
		   SHorizontalBox::Slot()
			   .AutoWidth()
			   .VAlign(VAlign_Center)
			   .Padding(0, 0, 8, 0)[SNew(SBox).WidthOverride(84)[SNew(STextBlock).Font(Font).Text(Label)]] +
		   SHorizontalBox::Slot().FillWidth(1.0f).VAlign(
			   VAlign_Center)[SNew(SButton)
								  .ToolTipText(LOCTEXT("RecordTip", "Click, then press a key combo (Esc to cancel)."))
								  .OnClicked(this, &SRuitkUetkxHmrPanel::OnRecordClicked,
											 RecordIndex)[SNew(STextBlock)
															  .Font(Font)
															  .Text(this, &SRuitkUetkxHmrPanel::GetShortcutText,
																	RecordIndex)]] +
		   SHorizontalBox::Slot()
			   .AutoWidth()
			   .VAlign(VAlign_Center)
			   .Padding(4, 0, 0,
						0)[SNew(SButton)
							   .ToolTipText(LOCTEXT("ClearTip", "Clear this shortcut (unbind)."))
							   .OnClicked(this, &SRuitkUetkxHmrPanel::OnClearShortcut,
										  RecordIndex)[SNew(STextBlock).Font(Font).Text(LOCTEXT("ClearX", "×"))]];
}

void SRuitkUetkxHmrPanel::RebuildErrorList()
{
	if (!ErrorListBox.IsValid())
	{
		return;
	}
	ErrorListBox->ClearChildren();
	const TArray<FUetkxHmrError>& Errors = FUetkxHmrController::Get().GetRecentErrors();
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 8);
	if (Errors.Num() == 0)
	{
		ErrorListBox->AddSlot().AutoHeight().Padding(
			0, 1)[SNew(STextBlock)
					  .Font(Font)
					  .ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
					  .Text(LOCTEXT("NoErrors", "None."))];
		return;
	}
	for (const FUetkxHmrError& Error : Errors)
	{
		ErrorListBox->AddSlot().AutoHeight().Padding(
			0, 1)[SNew(STextBlock)
					  .Font(Font)
					  .ColorAndOpacity(FSlateColor(FLinearColor(0.90f, 0.45f, 0.40f)))
					  .Text(FText::FromString(FString::Printf(TEXT("%s  %s"), *Error.When, *Error.Summary)))];
	}
}

void SRuitkUetkxHmrPanel::OnControllerStatusChanged()
{
	RebuildErrorList();
}

#undef LOCTEXT_NAMESPACE
