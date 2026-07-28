// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuitkExportSchemaCommandlet.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UetkxCodegen.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuitkExportSchema, Log, All);

int32 URuitkExportSchemaCommandlet::Main(const FString& Params)
{
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ReactiveUIToolkit"), TEXT("schema.json"));
	if (!FFileHelper::SaveStringToFile(FUetkxCodegen::ExportSchemaJson(), *Path,
									   FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogRuitkExportSchema, Error, TEXT("could not write %s"), *Path);
		return 1;
	}
	UE_LOG(LogRuitkExportSchema, Display, TEXT("schema exported: %s"), *Path);
	return 0;
}
