// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuitkUetkxCommands.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Logging/LogMacros.h"
#include "RuitkUetkxMenu.h"
#include "Styling/AppStyle.h"
#include "UetkxHmrController.h"

#define LOCTEXT_NAMESPACE "RuitkUetkx"

DEFINE_LOG_CATEGORY_STATIC(LogRuitkCmd, Log, All);

FRuitkUetkxCommands::FRuitkUetkxCommands()
	: TCommands<FRuitkUetkxCommands>(TEXT("RuitkUetkx"), LOCTEXT("ContextDesc", "RuitkUetkx"), NAME_None,
										FAppStyle::GetAppStyleSetName())
{
}

void FRuitkUetkxCommands::RegisterCommands()
{
	// Default UNBOUND — FInputChord() is empty, so nothing fires until the owner binds it.
	UI_COMMAND(ToggleHmr, "Toggle HMR", "Start or stop the RuitkUetkx Hot Reload mode.",
			   EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ToggleHmrWindow, "Toggle HMR Window", "Open or close the RuitkUetkx Hot Reload window.",
			   EUserInterfaceActionType::Button, FInputChord());
}

void FRuitkUetkxCommands::ExecuteToggleHmr()
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
			UE_LOG(LogRuitkCmd, Warning, TEXT("[RuitkUetkx] Toggle HMR — start failed: %s"), *Error);
		}
	}
}

void FRuitkUetkxCommands::ExecuteToggleHmrWindow()
{
	const FTabId TabId(RuitkUetkxTabs::HmrWindow);
	if (TSharedPtr<SDockTab> Existing = FGlobalTabmanager::Get()->FindExistingLiveTab(TabId))
	{
		Existing->RequestCloseTab();
	}
	else
	{
		FGlobalTabmanager::Get()->TryInvokeTab(RuitkUetkxTabs::HmrWindow);
	}
}

bool FRuitkUetkxInputProcessor::HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& KeyEvent)
{
	// Auto-repeat while the chord is held must not re-fire (it would rapid-toggle the mode).
	if (KeyEvent.IsRepeat())
	{
		return false;
	}
	// Ignore bare modifier presses — wait for the real key that completes the chord.
	const FKey Key = KeyEvent.GetKey();
	if (Key.IsModifierKey())
	{
		return false;
	}
	const FInputChord Chord(Key, KeyEvent.IsShiftDown(), KeyEvent.IsControlDown(), KeyEvent.IsAltDown(),
							KeyEvent.IsCommandDown());

	const FRuitkUetkxCommands& Commands = FRuitkUetkxCommands::Get();
	if (Commands.ToggleHmr.IsValid() && Commands.ToggleHmr->HasActiveChord(Chord))
	{
		FRuitkUetkxCommands::ExecuteToggleHmr();
		return true; // consume — this was our shortcut
	}
	if (Commands.ToggleHmrWindow.IsValid() && Commands.ToggleHmrWindow->HasActiveChord(Chord))
	{
		FRuitkUetkxCommands::ExecuteToggleHmrWindow();
		return true;
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
