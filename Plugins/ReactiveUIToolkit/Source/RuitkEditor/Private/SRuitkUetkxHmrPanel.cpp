// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "SRuitkUetkxHmrPanel.h"

#include "Framework/Docking/TabManager.h"
#include "HAL/PlatformMemory.h"
#include "Logging/LogMacros.h"
#include "RuitkUetkxEditorSettings.h"
#include "RuitkUetkxMenu.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "UetkxHmrController.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SHyperlink.h"
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
								+ SVerticalBox::Slot().AutoHeight().Padding(
									  0, 1)[StatRow(LOCTEXT("Watched", "Watched"),
													TAttribute<FText>(this, &SRuitkUetkxHmrPanel::GetWatchedText))] +
								SVerticalBox::Slot().AutoHeight().Padding(
									0, 1)[StatRow(LOCTEXT("Swaps", "Swaps"),
												  TAttribute<FText>(this, &SRuitkUetkxHmrPanel::GetSwapsText))] +
								SVerticalBox::Slot().AutoHeight().Padding(
									0, 1)[StatRow(LOCTEXT("Errors", "Errors"),
												  TAttribute<FText>(this, &SRuitkUetkxHmrPanel::GetErrorsText))] +
								SVerticalBox::Slot().AutoHeight().Padding(
									0, 1)[StatRow(LOCTEXT("Last", "Last"),
												  TAttribute<FText>(this, &SRuitkUetkxHmrPanel::GetLastText))] +
								SVerticalBox::Slot().AutoHeight().Padding(0, 1)[StatRow(
									LOCTEXT("Ram", "RAM"), TAttribute<FText>(this, &SRuitkUetkxHmrPanel::GetRamText))]
								// ── settings live in the one settings window (SRuitkSettingsPanel) ───────
								+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 1)
									  [SNew(SHyperlink)
										   .Text(LOCTEXT("AllSettings", "All settings…"))
										   .ToolTipText(LOCTEXT("AllSettingsTip",
																"Open the Reactive UI Toolkit Settings window — "
																"watched roots, debounce, notifications, shortcuts, "
																"and every other runtime + editor option."))
										   .OnNavigate(this, &SRuitkUetkxHmrPanel::OnOpenAllSettings)]
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

FText SRuitkUetkxHmrPanel::GetWatchedText() const
{
	// The ACTUAL roots from the settings CDO (the watcher reads the same property) — not a
	// hardcoded echo of the defaults, which went stale the moment a user edited Watched roots.
	const URuitkUetkxEditorSettings* Settings = GetDefault<URuitkUetkxEditorSettings>();
	TArray<FString> Globs;
	Globs.Reserve(Settings->WatchedRoots.Num());
	for (const FString& Root : Settings->WatchedRoots)
	{
		Globs.Add(Root / TEXT("**/*.uetkx"));
	}
	if (Globs.Num() == 0)
	{
		return LOCTEXT("NoWatchedRoots", "(no watched roots)");
	}
	return FText::FromString(FString::Join(Globs, TEXT(", ")));
}

FText SRuitkUetkxHmrPanel::GetRamText() const
{
	const uint64 Now = FPlatformMemory::GetStats().UsedPhysical;
	const int64 DeltaMB = (static_cast<int64>(Now) - static_cast<int64>(BaselineRamBytes)) / (1024 * 1024);
	return FText::FromString(FString::Printf(TEXT("%s MB (%+lld since open)"), *RamToText(Now).ToString(), DeltaMB));
}

void SRuitkUetkxHmrPanel::OnOpenAllSettings()
{
	// The one-settings-window design: every option — runtime + editor, plus the shortcut recorders —
	// lives in the Reactive UI Toolkit Settings window; open it (same tab the plugin menu opens).
	FGlobalTabmanager::Get()->TryInvokeTab(RuitkUetkxTabs::Settings);
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
