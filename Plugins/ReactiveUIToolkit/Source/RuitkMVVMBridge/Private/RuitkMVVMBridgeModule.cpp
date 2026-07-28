// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuitkMVVMBridge, Log, All);

class FRuitkMVVMBridgeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogRuitkMVVMBridge, Verbose, TEXT("RuitkMVVMBridge module started"));
	}

	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FRuitkMVVMBridgeModule, RuitkMVVMBridge)