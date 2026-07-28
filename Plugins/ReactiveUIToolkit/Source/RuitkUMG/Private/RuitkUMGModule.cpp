// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuitkUmg, Log, All);

class FRuitkUMGModule : public IModuleInterface
{
public:
	virtual void StartupModule() override { UE_LOG(LogRuitkUmg, Verbose, TEXT("RuitkUMG module started")); }

	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FRuitkUMGModule, RuitkUMG)