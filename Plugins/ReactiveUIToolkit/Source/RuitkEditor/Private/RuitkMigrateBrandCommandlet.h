// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
#pragma once

// 0.15.0 rebrand codemod (family "Reactive UI Toolkit"): rewrites a USER project from the
// pre-0.15 brand identifiers to the 0.15 ones — the SAME record-driven rule set the repo's own
// conversion used (module tokens, export macros, plugin paths, the seven type prefixes,
// `Ruitk::` namespace, the bare-`Rui*` blanket, the `RUITK_*` macro family, the `.uproject`
// plugin name). Idempotent by construction: every record is guarded (`(?!tk)` / `(?!TK)`), so
// a second run rewrites nothing. Modeled on the record-driven `URuitkMigrateEsModulesCommandlet`.
//
//   UnrealEditor-Cmd <Project>.uproject -run=RuitkMigrateBrand [-dry] [-root=<dir>]
//
// Walks {.uetkx,.h,.cpp,.inl,.cs,.uproject} under the project root (default: the project dir),
// skipping Plugins/, Binaries/, Intermediate/, Saved/, DerivedDataCache/ and dot-dirs — the
// 0.15 plugin ships already-converted; only USER sources need rewriting. `-dry` reports the
// would-be changes without writing. After running, paste the MIGRATION-0.15.md [CoreRedirects]
// block into DefaultEngine.ini, resave Blueprints referencing renamed reflected types, and
// verify with `-run=RuitkCompile -check` (exit 0).

#include "Commandlets/Commandlet.h"
#include "RuitkMigrateBrandCommandlet.generated.h"

UCLASS()
class URuitkMigrateBrandCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	URuitkMigrateBrandCommandlet()
	{
		IsClient = false;
		IsServer = false;
		IsEditor = true;
		LogToConsole = true;
	}

	virtual int32 Main(const FString& Params) override;
};
