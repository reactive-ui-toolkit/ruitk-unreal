// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "SRuitkSettingsPanel.h"

#include "DetailsViewArgs.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "RuitkSettings.h"
#include "RuitkUetkxCommands.h"
#include "RuitkUetkxEditorSettings.h"
#include "SRuitkShortcutRecorderRow.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "UObject/UnrealType.h"
#include "UetkxHmrController.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "RuitkUetkx"

void SRuitkSettingsPanel::Construct(const FArguments&)
{
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsArgs;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsArgs.bAllowSearch = false; // ~13 properties across both panels — two search boxes is clutter
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.bShowOptions = false;
	DetailsArgs.bShowScrollBar = false; // the window's own SScrollBox scrolls both sections as one page

	TSharedRef<IDetailsView> RuntimeView = PropertyEditor.CreateDetailView(DetailsArgs);
	RuntimeView->SetObject(GetMutableDefault<URuitkSettings>());
	RuntimeView->OnFinishedChangingProperties().AddSP(this, &SRuitkSettingsPanel::OnRuntimeSettingsChanged);

	TSharedRef<IDetailsView> EditorView = PropertyEditor.CreateDetailView(DetailsArgs);
	EditorView->SetObject(GetMutableDefault<URuitkUetkxEditorSettings>());
	EditorView->OnFinishedChangingProperties().AddSP(this, &SRuitkSettingsPanel::OnEditorSettingsChanged);

	const FSlateFontInfo SectionFont = FCoreStyle::GetDefaultFontStyle("Bold", 11);
	const FSlateFontInfo NoteFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);
	const FSlateColor NoteColor(FLinearColor(0.6f, 0.6f, 0.6f));

	// The two HMR commands were registered at module startup (FRuitkUetkxCommands::Register), well
	// before any tab can spawn.
	const FRuitkUetkxCommands& Commands = FRuitkUetkxCommands::Get();

	ChildSlot
		[SNew(SBox)
			 .MinDesiredWidth(600) // opens at ~600x500 when floating; still user-resizable
			 .MinDesiredHeight(500)
				 [SNew(SBorder)
					  .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					  .Padding(12)
						  [SNew(SScrollBox) +
						   SScrollBox::Slot()
							   [SNew(SVerticalBox)
								// ── Runtime ──────────────────────────────────────────────────────────
								+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
									  [SNew(STextBlock).Font(SectionFont).Text(LOCTEXT("RuntimeSection", "Runtime"))] +
								SVerticalBox::Slot().AutoHeight().Padding(
									0, 0, 0, 4)[SNew(STextBlock)
													.Font(NoteFont)
													.AutoWrapText(true)
													.ColorAndOpacity(NoteColor)
													.Text(LOCTEXT("RuntimeSectionNote",
																  "The six ruitk.* reconciler CVars — edits apply live "
																  "and persist to your project's DefaultGame.ini."))] +
								SVerticalBox::Slot().AutoHeight()[RuntimeView]
								// ── Editor / Hot Reload ──────────────────────────────────────────────
								+ SVerticalBox::Slot().AutoHeight().Padding(
									  0, 14, 0, 2)[SNew(STextBlock)
													   .Font(SectionFont)
													   .Text(LOCTEXT("EditorSection", "Editor / Hot Reload"))] +
								SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
									[SNew(STextBlock)
										 .Font(NoteFont)
										 .AutoWrapText(true)
										 .ColorAndOpacity(NoteColor)
										 .Text(LOCTEXT("EditorSectionNote",
													   "The .uetkx Hot Reload loop — watched roots, debounce, "
													   "notifications, follow-PIE. Per-user (not committed)."))] +
								SVerticalBox::Slot().AutoHeight()[EditorView]
								// ── keyboard shortcuts (chords live in the binding manager, not config) ──
								+ SVerticalBox::Slot().AutoHeight().Padding(
									  0, 14, 0, 2)[SNew(STextBlock)
													   .Font(SectionFont)
													   .Text(LOCTEXT("ShortcutsSection", "Keyboard Shortcuts"))] +
								SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
									[SNew(STextBlock)
										 .Font(NoteFont)
										 .AutoWrapText(true)
										 .ColorAndOpacity(NoteColor)
										 .Text(LOCTEXT("ShortcutsSectionNote",
													   "Default unbound; also editable in Editor Preferences ▸ "
													   "Keyboard Shortcuts (one shared store)."))] +
								SVerticalBox::Slot().AutoHeight().Padding(
									0, 1)[SNew(SRuitkShortcutRecorderRow)
											  .Label(LOCTEXT("ToggleHmrShort", "Toggle HMR"))
											  .Command(Commands.ToggleHmr)] +
								SVerticalBox::Slot().AutoHeight().Padding(
									0, 1)[SNew(SRuitkShortcutRecorderRow)
											  .Label(LOCTEXT("ToggleWindowShort", "Open Window"))
											  .Command(Commands.ToggleHmrWindow)]]]]];
}

void SRuitkSettingsPanel::OnRuntimeSettingsChanged(const FPropertyChangedEvent& Event)
{
	// The details view already ran URuitkSettings::PostEditChangeProperty — the live CVar push at
	// ECVF_SetByProjectSetting (RuitkSettings.cpp). Persist exactly the way Project Settings
	// persists a defaultconfig class — FSettingsSection::Save() routes CLASS_DefaultConfig objects
	// to TryUpdateDefaultConfigFile (Engine Developer/Settings/Private/SettingsSection.cpp:227-229)
	// → the project's DefaultGame.ini, which packaged builds ship.
	GetMutableDefault<URuitkSettings>()->TryUpdateDefaultConfigFile();
}

void SRuitkSettingsPanel::OnEditorSettingsChanged(const FPropertyChangedEvent& Event)
{
	// Persist the way the HMR window's checkboxes always persisted this class: SaveConfig() — the
	// per-user editor layer (config=EditorPerProjectUser; the layer that wins on load).
	URuitkUetkxEditorSettings* Settings = GetMutableDefault<URuitkUetkxEditorSettings>();
	Settings->SaveConfig();
	if (Event.GetPropertyName() == GET_MEMBER_NAME_CHECKED(URuitkUetkxEditorSettings, bHideLiveCodingConsole))
	{
		// The checkbox handler did this too: start/stop the window hider immediately if HMR is active.
		FUetkxHmrController::Get().RefreshConsoleHiderState();
	}
}

#undef LOCTEXT_NAMESPACE
