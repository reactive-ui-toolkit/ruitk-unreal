// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// The unified-settings suite: URuitkSettings (Project Settings ▸ Plugins ▸ Reactive UI Toolkit)
// must mirror the six ruitk.* CVars — CDO defaults equal to the FRuitkConfig accessor values in
// the CURRENT build config (per-build default parity, the invariant the diff-guarded push relies
// on), and the explicit-push route must actually carry an edited property onto its CVar at
// ECVF_SetByProjectSetting.

#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "RuitkCoreMisc.h"
#include "RuitkSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

#define RUITK_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkSettingsTest, "Ruitk.Umg.Settings", RUITK_TEST_FLAGS)
bool FRuitkSettingsTest::RunTest(const FString&)
{
	AddInfo(TEXT("[settings] 1/3 CDO exists"));
	const URuitkSettings* Settings = GetDefault<URuitkSettings>();
	if (!TestNotNull(TEXT("URuitkSettings CDO"), Settings))
	{
		return false;
	}

	AddInfo(TEXT("[settings] 2/3 defaults match FRuitkConfig in this build config"));
	// The demo project ships no [/Script/RuitkUMG.RuitkSettings] ini section and nothing else
	// mutates these four CVars mid-suite (Ruitk.Core.TimeSlicing touches TimeSlicing/FrameBudgetMs
	// but restores the defaults), so CDO == accessor proves constructor parity with the per-build
	// CVar defaults in RuitkCoreMisc.cpp.
	TestEqual(TEXT("bTimeSlicing == FRuitkConfig::IsTimeSlicing"), Settings->bTimeSlicing,
			  FRuitkConfig::IsTimeSlicing());
	TestEqual(TEXT("FrameBudgetMs == FRuitkConfig::FrameBudgetMs"), Settings->FrameBudgetMs,
			  FRuitkConfig::FrameBudgetMs());
	TestEqual(TEXT("bHostNodePool == FRuitkConfig::IsHostNodePoolEnabled"), Settings->bHostNodePool,
			  FRuitkConfig::IsHostNodePoolEnabled());
	TestEqual(TEXT("bHookValidation == FRuitkConfig::IsHookValidationEnabled"), Settings->bHookValidation,
			  FRuitkConfig::IsHookValidationEnabled());
	TestEqual(TEXT("bStrictDiagnostics == FRuitkConfig::IsStrictDiagnosticsEnabled"), Settings->bStrictDiagnostics,
			  FRuitkConfig::IsStrictDiagnosticsEnabled());
	// StrictMode: the accessor force-returns false in Shipping regardless of the CVar, but this
	// suite never runs in Shipping (WITH_DEV_AUTOMATION_TESTS), so CDO == accessor holds here too.
	TestEqual(TEXT("bStrictMode == FRuitkConfig::IsStrictModeEnabled"), Settings->bStrictMode,
			  FRuitkConfig::IsStrictModeEnabled());

	AddInfo(TEXT("[settings] 3/3 explicit push propagates an edit to the CVar"));
	// Use ruitk.StrictMode: no other test touches it, so its CVar still sits at constructor
	// priority and a ECVF_SetByProjectSetting push must win. (ruitk.TimeSlicing/FrameBudgetMs are
	// unusable here — Ruitk.Core.TimeSlicing Sets them at the higher ECVF_SetByCode, after which a
	// project-setting-priority push is correctly rejected by the CVar system.)
	IConsoleVariable* StrictMode = IConsoleManager::Get().FindConsoleVariable(TEXT("ruitk.StrictMode"));
	if (!TestNotNull(TEXT("ruitk.StrictMode CVar registered"), StrictMode))
	{
		return false;
	}
	const bool bOriginal = StrictMode->GetBool();
	URuitkSettings* Mutable = GetMutableDefault<URuitkSettings>();
	Mutable->bStrictMode = !bOriginal;
	Mutable->PushSettingsToCVars();
	TestEqual(TEXT("push carried the edited value onto ruitk.StrictMode"), StrictMode->GetBool(), !bOriginal);

	// Restore both sides (same priority, so the restore push is accepted).
	Mutable->bStrictMode = bOriginal;
	Mutable->PushSettingsToCVars();
	TestEqual(TEXT("restore push returned ruitk.StrictMode to its default"), StrictMode->GetBool(), bOriginal);
	TestEqual(TEXT("accessor sees the restored value"), FRuitkConfig::IsStrictModeEnabled(), bOriginal);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
