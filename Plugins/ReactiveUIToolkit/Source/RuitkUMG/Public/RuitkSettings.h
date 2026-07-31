// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// The unified runtime settings page — Project Settings ▸ Plugins ▸ Reactive UI Toolkit — over the
// ruitk.* CVars (declared in RuitkCore/Private/RuitkCoreMisc.cpp, read via FRuitkConfig; the
// accessors keep reading the CVars, so this class changes zero call sites).
//
// WHY DEFAULTS LIVE IN C++, NOT A PLUGIN INI: plugin-shipped Config/Default*.ini files are not
// packaged into games (Epic docs), so a plugin ini would silently vanish from shipping builds.
// Instead the constructor carries the defaults and user edits land in the PROJECT's
// DefaultGame.ini (config=Game + defaultconfig), which IS staged — the FMOD/Wwise pattern.
//
// WHY EXPLICIT PUSH, NOT UDeveloperSettingsBackedByCVars: the engine base's data flow is the
// wrong direction for ini persistence. UDeveloperSettingsBackedByCVars::PostInitProperties
// (Engine/Source/Runtime/DeveloperSettings/Private/Engine/DeveloperSettingsBackedByCVars.cpp:12-22)
// only calls ImportConsoleVariableValues() — CVar → property — and only under WITH_EDITOR on the
// CDO; since LoadConfig runs BEFORE PostInitProperties (CoreUObject UObjectGlobals.cpp,
// FObjectInitializer::PostConstructInit: LoadConfig at :4334, PostInitProperties at :4380), the
// import OVERWRITES the ini-persisted user edits with the CVar's compiled default, and in a
// packaged game (no WITH_EDITOR) nothing syncs at all. Export to the CVars exists only in
// PostEditChangeProperty (edit-time). So we go the other way: PostInitProperties on the CDO
// pushes property values ONTO the CVars at ECVF_SetByProjectSetting — in every build config —
// and PostEditChangeProperty pushes live in-editor via the engine's own
// ExportValuesToConsoleVariables (DeveloperSettings.cpp:138-193, same priority). Higher-priority
// sources (SystemSettings/ConsoleVariables ini, command line, console) still win, per the CVar
// priority ladder. The push is diff-guarded, so an untouched CDO — whose constructor defaults
// match the per-build CVar defaults exactly — never touches a CVar and shipping behavior stays
// byte-identical (including FRuitkConfig::IsStrictModeEnabled's shipping force-false).
//
// WHY THIS LIVES IN RuitkUMG, NOT RuitkCore: a UCLASS needs CoreUObject + UnrealHeaderTool, and
// RuitkCore is UObject-free by locked decision (MASTER_PLAN D-27 — "no UObject/CoreUObject";
// the engine-blind core is what the mock-host suites run headlessly). RuitkUMG is the plugin's
// designated "Epic interop, UObject side" Runtime module and always loads with the plugin, so
// the CDO push runs at startup in every target that has the plugin enabled.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RuitkSettings.generated.h"

/** The ruitk.Environment values (int CVar: 0/1/2) — the family `environment` knob's Unreal
 *  spelling. Components read the RESOLVED label via Ctx.GetEnvironment(); the library itself
 *  never branches on it. */
UENUM()
enum class ERuitkEnvironmentSetting : uint8
{
	/** Development in any non-shipping build (editor included), Production in Shipping. */
	Auto = 0,
	Development = 1,
	Production = 2,
};

/**
 * Runtime configuration for the Reactive UI Toolkit reconciler — the ruitk.* console variables as
 * a persistable Project Settings page. Edits apply live in the editor and are written to your
 * project's DefaultGame.ini; console and ini overrides keep the last word.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Reactive UI Toolkit"))
class RUITKUMG_API URuitkSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URuitkSettings();

	//~ UDeveloperSettings
	virtual FName GetCategoryName() const override { return FName(TEXT("Plugins")); }
#if WITH_EDITOR
	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;
#endif

	//~ UObject
	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Push every property whose value differs from its CVar onto the CVar, at
	 *  ECVF_SetByProjectSetting so higher-priority sources (ini/command line/console) still win.
	 *  Runs on the CDO at startup (after config load); callable again after programmatic edits. */
	void PushSettingsToCVars() const;

	/** ruitk.TimeSlicing — run render passes as time-sliced actions on the frame scheduler
	 *  (commit stays atomic). Off = scheduler bypass: fully synchronous single-pass renders. */
	UPROPERTY(config, EditAnywhere, Category = "Performance", meta = (ConsoleVariable = "ruitk.TimeSlicing"))
	bool bTimeSlicing;

	/** ruitk.TimeSliceMs — the render-phase quantum: a sliced pass runs units of work until
	 *  this many milliseconds elapse (checked after each unit), then parks — resuming the same
	 *  frame if the scheduler budget allows, else next frame. Only read when Time Slicing is on. */
	UPROPERTY(config, EditAnywhere, Category = "Performance",
			  meta = (ConsoleVariable = "ruitk.TimeSliceMs", ClampMin = "0.1", UIMin = "0.5", UIMax = "16.0",
					  Units = "Milliseconds"))
	float TimeSliceMs;

	/** ruitk.FrameBudgetMs — the scheduler's per-frame budget, cumulative across lanes (render
	 *  slices, idle work; the frame-end batched-effects flush is unbudgeted). Per-slice length
	 *  is Time Slice Ms. NOTE: before the scheduler this was the single render-phase budget
	 *  with default 8.0 — a saved 8.0 still works (it is simply a more generous budget). */
	UPROPERTY(config, EditAnywhere, Category = "Performance",
			  meta = (ConsoleVariable = "ruitk.FrameBudgetMs", ClampMin = "0.1", UIMin = "1.0", UIMax = "16.0",
					  Units = "Milliseconds"))
	float FrameBudgetMs;

	/** ruitk.HostNodePool — recycle childless leaf widgets across keyed-list churn (GO-05).
	 *  Off to A/B. */
	UPROPERTY(config, EditAnywhere, Category = "Performance", meta = (ConsoleVariable = "ruitk.HostNodePool"))
	bool bHostNodePool;

	/** ruitk.HookValidation — hook-order mismatch detection (hooks in branches/loops desync
	 *  slots). Defaults off in Shipping. */
	UPROPERTY(config, EditAnywhere, Category = "Development", meta = (ConsoleVariable = "ruitk.HookValidation"))
	bool bHookValidation;

	/** ruitk.StrictDiagnostics — misuse warnings, prefixed [Ruitk][strict] and deduped per
	 *  component: state updates during a component's own render + effects registered with no
	 *  dependency array (Ruitk::EveryCommit()). Defaults off in Shipping. */
	UPROPERTY(config, EditAnywhere, Category = "Development", meta = (ConsoleVariable = "ruitk.StrictDiagnostics"))
	bool bStrictDiagnostics;

	/** ruitk.StrictMode — dev double-render: render functions run twice, first result discarded
	 *  (flushes impure renders and stale captures). Ignored in Shipping builds. */
	UPROPERTY(config, EditAnywhere, Category = "Development", meta = (ConsoleVariable = "ruitk.StrictMode"))
	bool bStrictMode;

	/** ruitk.Environment — the environment label surfaced READ-ONLY to components
	 *  (Ctx.GetEnvironment()). Auto = development in any non-shipping build, production in
	 *  Shipping. For YOUR components' dev-vs-prod branches — the library never branches on it. */
	UPROPERTY(config, EditAnywhere, Category = "Development", meta = (ConsoleVariable = "ruitk.Environment"))
	ERuitkEnvironmentSetting Environment;
};
