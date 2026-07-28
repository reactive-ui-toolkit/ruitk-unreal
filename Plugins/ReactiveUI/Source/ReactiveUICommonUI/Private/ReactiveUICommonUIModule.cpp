// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuiCommonUI, Log, All);

class FRuitkCommonUIModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogRuiCommonUI, Verbose, TEXT("RuitkCommonUI module started"));
	}

	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FRuitkCommonUIModule, RuitkCommonUI)