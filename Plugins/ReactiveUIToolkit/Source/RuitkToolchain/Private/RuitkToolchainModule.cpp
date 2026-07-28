// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "UetkxToolchainLog.h"

DEFINE_LOG_CATEGORY(LogRuitkToolchain);

class FRuitkToolchainModule : public IModuleInterface
{
public:
	virtual void StartupModule() override { UE_LOG(LogRuitkToolchain, Verbose, TEXT("RuitkToolchain module started")); }

	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FRuitkToolchainModule, RuitkToolchain)