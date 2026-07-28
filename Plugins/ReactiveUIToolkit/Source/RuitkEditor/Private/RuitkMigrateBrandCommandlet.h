// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
#pragma once

// 0.15.0 rebrand codemod (family "Reactive UI Toolkit"): rewrites a USER project from the
// pre-0.15 brand identifiers to the 0.15 ones — the SAME record-driven rule set the repo's own
// conversion used (module tokens, export macros, plugin paths, the seven type prefixes,
// `Ruitk::` namespace, the bare-`Rui*` blanket, the `RUITK_*` macro family, the `.uproject`
// plugin name). Modeled on the record-driven `URuitkMigrateEsModulesCommandlet`.
//
// Idempotent: a second run rewrites nothing. Two mechanisms, not one — the `Rui`-stem records
// carry an explicit `(?!tk)` / `(?!TK)` lookahead, while the module tokens, the 9 export macros,
// the path/uplugin/uproject records, the namespace records and `_RUI_HOOK_SIG` are self-guarding
// (their output no longer matches their own pattern). A NEW record must satisfy one or the other.
// The `Rui`-stem records additionally require the next character to be uppercase or `_`
// (`(?=[A-Z_])`), which is what keeps them off ordinary words and user identifiers that merely
// begin with the same letters (`FRuit`, "Ruined save"); the `rui.*` cvar record names its nine
// tokens outright rather than matching an open `rui.` prefix.
//
//   UnrealEditor-Cmd <Project>.uproject -run=RuitkMigrateBrand [-dry] [-root=<dir>]
//
// Walks {.uetkx,.h,.cpp,.inl,.cs,.uproject} under the project root (default: the project dir),
// skipping the two BRAND plugin folders (`Plugins/ReactiveUIToolkit/` ships converted;
// `Plugins/ReactiveUI/` is the retired one, which must stay compilable until deleted) plus
// Binaries/, Intermediate/, Saved/, DerivedDataCache/ and dot-dirs. A user's own project
// plugins under `Plugins/` ARE migrated — they are ordinary user sources. `-dry` reports the
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
