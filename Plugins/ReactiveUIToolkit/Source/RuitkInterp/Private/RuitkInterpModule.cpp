// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuitkInterp, Log, All);

class FRuitkInterpModule : public IModuleInterface
{
public:
	virtual void StartupModule() override { UE_LOG(LogRuitkInterp, Verbose, TEXT("RuitkInterp module started")); }

	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FRuitkInterpModule, RuitkInterp)