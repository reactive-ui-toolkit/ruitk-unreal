// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "RuiSlateElements.h"
#include "RuiSlateLog.h"

DEFINE_LOG_CATEGORY(LogRuiSlate);

class FRuitkSlateModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		RUI::Slate::RegisterBuiltinAdapters();
		UE_LOG(LogRuiSlate, Verbose, TEXT("RuitkSlate module started"));
	}

	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FRuitkSlateModule, RuitkSlate)
