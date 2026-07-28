// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Logging/LogMacros.h"
#include "RuitkCultureSync.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuitkCore, Log, All);

// Phase 1 (MASTER_PLAN §3) fills this module with the reconciler. The startup banner below is
// load-bearing already: the Ruitk.Boot suite and the packaged-fidelity test (fresh project,
// enable plugin, expect the banner in the log) both key off it — the exact analogue of the Godot
// repo's "a silent Output means the plugin is NOT running" rule.
class FRuitkCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FString VersionName = TEXT("unknown");
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ReactiveUIToolkit")))
		{
			VersionName = Plugin->GetDescriptor().VersionName;
		}
		UE_LOG(LogRuitkCore, Display, TEXT("Reactive UI Toolkit %s loaded (RuitkCore)"), *VersionName);

		// Culture-change → root re-render (Phase 7 localization): live roots re-render when the
		// text revision bumps, healing anything a component baked under the previous culture.
		Ruitk::RegisterCultureSync();
	}

	virtual void ShutdownModule() override { Ruitk::UnregisterCultureSync(); }
};

IMPLEMENT_MODULE(FRuitkCoreModule, RuitkCore)
