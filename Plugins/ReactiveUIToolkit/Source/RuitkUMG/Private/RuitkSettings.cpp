// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuitkSettings.h"
#include "HAL/IConsoleManager.h"

#define LOCTEXT_NAMESPACE "Ruitk"

URuitkSettings::URuitkSettings()
{
	// Per-build default parity with RuitkCoreMisc.cpp's CVar declarations — an untouched CDO must
	// be a no-op push in EVERY configuration (the diff guard in PushSettingsToCVars relies on it).
	bTimeSlicing = false;
	FrameBudgetMs = 8.0f;
	bHostNodePool = true;
#if UE_BUILD_SHIPPING
	bHookValidation = false;
	bStrictDiagnostics = false;
#else
	bHookValidation = true;
	bStrictDiagnostics = true;
#endif
	bStrictMode = false;
}

#if WITH_EDITOR
FText URuitkSettings::GetSectionText() const
{
	return LOCTEXT("RuitkSettingsSection", "Reactive UI Toolkit");
}

FText URuitkSettings::GetSectionDescription() const
{
	return LOCTEXT("RuitkSettingsSectionDesc",
				   "Runtime configuration for the Reactive UI Toolkit reconciler — the ruitk.* console "
				   "variables, persisted to your project's DefaultGame.ini.");
}
#endif

void URuitkSettings::PostInitProperties()
{
	Super::PostInitProperties();

	// Config values are already in the properties here (LoadConfig runs before PostInitProperties —
	// FObjectInitializer::PostConstructInit, UObjectGlobals.cpp:4334 vs :4380). Push the CDO once
	// per process, in every build config — this is what carries a DefaultGame.ini edit onto the
	// CVars in a packaged game, where the editor-only Import/Export helpers don't exist.
	if (IsTemplate())
	{
		PushSettingsToCVars();
	}
}

#if WITH_EDITOR
void URuitkSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent); // broadcasts OnSettingChanged

	// Live in-editor push via the engine's own meta=(ConsoleVariable) walk — same
	// ECVF_SetByProjectSetting priority as the startup push (DeveloperSettings.cpp:138-193).
	if (PropertyChangedEvent.Property)
	{
		ExportValuesToConsoleVariables(PropertyChangedEvent.Property);
	}
}
#endif

void URuitkSettings::PushSettingsToCVars() const
{
	IConsoleManager& Console = IConsoleManager::Get();

	// Diff-guarded: only a value that actually differs from the CVar is pushed, so untouched
	// defaults never raise a CVar off its ECVF_SetByConstructor rung (and a push a higher-priority
	// source has already claimed is rejected by the CVar system — which is the desired outcome).
	const auto PushBool = [&Console](const TCHAR* Name, bool bValue)
	{
		if (IConsoleVariable* CVar = Console.FindConsoleVariable(Name))
		{
			if (CVar->GetBool() != bValue)
			{
				CVar->Set(bValue ? 1 : 0, ECVF_SetByProjectSetting); // int form, mirroring DeveloperSettings.cpp:161
			}
		}
	};
	const auto PushFloat = [&Console](const TCHAR* Name, float Value)
	{
		if (IConsoleVariable* CVar = Console.FindConsoleVariable(Name))
		{
			if (!FMath::IsNearlyEqual(CVar->GetFloat(), Value))
			{
				CVar->Set(Value, ECVF_SetByProjectSetting);
			}
		}
	};

	PushBool(TEXT("ruitk.TimeSlicing"), bTimeSlicing);
	PushFloat(TEXT("ruitk.FrameBudgetMs"), FrameBudgetMs);
	PushBool(TEXT("ruitk.HostNodePool"), bHostNodePool);
	PushBool(TEXT("ruitk.HookValidation"), bHookValidation);
	PushBool(TEXT("ruitk.StrictDiagnostics"), bStrictDiagnostics);
	PushBool(TEXT("ruitk.StrictMode"), bStrictMode);
}

#undef LOCTEXT_NAMESPACE
