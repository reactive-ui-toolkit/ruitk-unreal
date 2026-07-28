// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// `-run=RuitkCompile [-full] [-check]` — the CLI/CI entry to the .uetkx compiler.
//   (default) incremental sweep of Source/ + Plugins/ (stale files only)
//   -full     recompile everything
//   -check    the CI drift gate: NO writes; exits non-zero when any committed .inl or
//             aggregator differs from a fresh in-memory compile, or any source has errors.
// Thin glue: all semantics live in FUetkxDriver (tested by Ruitk.Uetkx.Driver).

#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "RuitkCompileCommandlet.generated.h"

UCLASS()
class URuitkCompileCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	URuitkCompileCommandlet()
	{
		IsClient = false;
		IsServer = false;
		IsEditor = true;
		LogToConsole = true;
	}

	virtual int32 Main(const FString& Params) override;

	/** The sweep roots: <Project>/Source and <Project>/Plugins. */
	static TArray<FString> DefaultRoots();
};
