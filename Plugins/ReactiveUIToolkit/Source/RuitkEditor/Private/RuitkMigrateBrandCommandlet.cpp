// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
#include "RuitkMigrateBrandCommandlet.h"

#include "HAL/FileManager.h"
#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuitkMigrateBrand, Log, All);

namespace
{
	/** One ordered rewrite record. Patterns are ICU regexes; replacements are literal text
	 *  (no capture refs — the table enumerates every concrete pair instead, like the repo's
	 *  own conversion did). Order is load-bearing: longest-token-first prevents substring
	 *  clobbering, and the case-class `RUITK` rule must run after the PascalCase one. */
	struct FBrandRule
	{
		const TCHAR* Pattern;
		const TCHAR* Replacement;
	};

	// The §7.C/§7.D rule set, plus the boundary-blocked families the repo conversion needed
	// (`LogRui*` categories, `CVarRui*`/`GRui*` globals) — all idempotency-guarded.
	static const FBrandRule GBrandRules[] = {
		// C1 — module tokens, longest first.
		{ TEXT("\\bReactiveUIMVVMBridge\\b"), TEXT("RuitkMVVMBridge") },
		{ TEXT("\\bReactiveUICommonUI\\b"), TEXT("RuitkCommonUI") },
		{ TEXT("\\bReactiveUIToolchain\\b"), TEXT("RuitkToolchain") },
		{ TEXT("\\bReactiveUIEditor\\b"), TEXT("RuitkEditor") },
		{ TEXT("\\bReactiveUIInterp\\b"), TEXT("RuitkInterp") },
		{ TEXT("\\bReactiveUISlate\\b"), TEXT("RuitkSlate") },
		{ TEXT("\\bReactiveUICore\\b"), TEXT("RuitkCore") },
		{ TEXT("\\bReactiveUIUMG\\b"), TEXT("RuitkUMG") },
		// C2 — the 9 export macros.
		{ TEXT("\\bREACTIVEUISLATE_API\\b"), TEXT("RUITKSLATE_API") },
		{ TEXT("\\bREACTIVEUICORE_API\\b"), TEXT("RUITKCORE_API") },
		{ TEXT("\\bREACTIVEUIINTERP_API\\b"), TEXT("RUITKINTERP_API") },
		{ TEXT("\\bREACTIVEUIUMG_API\\b"), TEXT("RUITKUMG_API") },
		{ TEXT("\\bREACTIVEUITOOLCHAIN_API\\b"), TEXT("RUITKTOOLCHAIN_API") },
		{ TEXT("\\bREACTIVEUICOMMONUI_API\\b"), TEXT("RUITKCOMMONUI_API") },
		{ TEXT("\\bREACTIVEUIMVVMBRIDGE_API\\b"), TEXT("RUITKMVVMBRIDGE_API") },
		{ TEXT("\\bREACTIVEUIEDITOR_API\\b"), TEXT("RUITKEDITOR_API") },
		{ TEXT("\\bRUIDEMO_API\\b"), TEXT("RUITKDEMO_API") },
		// C5/C6 — plugin paths (both separators), the .uplugin filename, the .uproject ref.
		{ TEXT("Plugins/ReactiveUI/"), TEXT("Plugins/ReactiveUIToolkit/") },
		{ TEXT("Plugins\\\\ReactiveUI\\\\"), TEXT("Plugins\\ReactiveUIToolkit\\") },
		{ TEXT("\\bReactiveUI\\.uplugin\\b"), TEXT("ReactiveUIToolkit.uplugin") },
		{ TEXT("\"Name\": \"ReactiveUI\""), TEXT("\"Name\": \"ReactiveUIToolkit\"") },
		// D1/D2 — the seven type prefixes.
		{ TEXT("\\bFRui(?!tk)"), TEXT("FRuitk") },
		{ TEXT("\\bURui(?!tk)"), TEXT("URuitk") },
		{ TEXT("\\bSRui(?!tk)"), TEXT("SRuitk") },
		{ TEXT("\\bTRui(?!tk)"), TEXT("TRuitk") },
		{ TEXT("\\bIRui(?!tk)"), TEXT("IRuitk") },
		{ TEXT("\\bERui(?!tk)"), TEXT("ERuitk") },
		{ TEXT("\\bARui(?!tk)"), TEXT("ARuitk") },
		// D3 — the namespace.
		{ TEXT("\\bRUI::"), TEXT("Ruitk::") },
		{ TEXT("\\bnamespace RUI\\b"), TEXT("namespace Ruitk") },
		// Boundary-blocked families (word-char prefixes defeat \b).
		{ TEXT("LogRui(?!tk)"), TEXT("LogRuitk") },
		{ TEXT("CVarRui(?!tk)"), TEXT("CVarRuitk") },
		{ TEXT("GRui(?!tk)(?=[A-Z])"), TEXT("GRuitk") },
		// D5 — the bare-Rui blanket.
		{ TEXT("\\bRui(?!tk)"), TEXT("Ruitk") },
		// D6 — case-class: PascalCase (commandlet family) first, then ALL-CAPS macros.
		{ TEXT("\\bRUI(?=[A-Z][a-z])"), TEXT("Ruitk") },
		{ TEXT("\\bRUI(?!TK)(?=[A-Z_])"), TEXT("RUITK") },
		// Console variables + baked codegen constants (0.15 identity ruling): user ini/source
		// lines keep working under the new names; hook-sig constants regenerate via
		// RuitkCompile anyway — this rule just keeps a not-yet-regenerated tree consistent.
		{ TEXT("\\brui\\.(?=[A-Z])"), TEXT("ruitk.") },
		{ TEXT("_RUI_HOOK_SIG"), TEXT("_RUITK_HOOK_SIG") },
	};

	/** ICU-regex replace-all with a literal replacement. Returns the number of matches. */
	int32 ApplyRule(FString& Text, const FBrandRule& Rule)
	{
		const FRegexPattern Pattern(Rule.Pattern);
		FRegexMatcher Matcher(Pattern, Text);
		FString Out;
		Out.Reserve(Text.Len());
		int32 Cursor = 0, Count = 0;
		while (Matcher.FindNext())
		{
			const int32 B = Matcher.GetMatchBeginning();
			const int32 E = Matcher.GetMatchEnding();
			Out += Text.Mid(Cursor, B - Cursor);
			Out += Rule.Replacement;
			Cursor = E;
			++Count;
		}
		if (Count == 0)
		{
			return 0;
		}
		Out += Text.Mid(Cursor);
		Text = MoveTemp(Out);
		return Count;
	}

	bool ShouldSkip(const FString& Path)
	{
		// The 0.15 plugin ships converted; build output and VCS internals are never sources.
		static const TCHAR* Skips[] = { TEXT("/Plugins/"),	  TEXT("/Binaries/"), TEXT("/Intermediate/"),
										TEXT("/Saved/"),	  TEXT("/DerivedDataCache/"), TEXT("/.git/") };
		for (const TCHAR* Skip : Skips)
		{
			if (Path.Contains(Skip))
			{
				return true;
			}
		}
		return false;
	}
} // namespace

int32 URuitkMigrateBrandCommandlet::Main(const FString& Params)
{
	const bool bDry = FParse::Param(*Params, TEXT("dry"));
	FString Root = FPaths::ProjectDir();
	FParse::Value(*Params, TEXT("root="), Root);
	Root = FPaths::ConvertRelativePathToFull(Root);

	static const TCHAR* Extensions[] = { TEXT("*.uetkx"), TEXT("*.h"), TEXT("*.cpp"),
										 TEXT("*.inl"),	  TEXT("*.cs"), TEXT("*.uproject") };
	TArray<FString> Files;
	for (const TCHAR* Ext : Extensions)
	{
		TArray<FString> Found;
		IFileManager::Get().FindFilesRecursive(Found, *Root, Ext, /*Files=*/true, /*Dirs=*/false);
		Files.Append(Found);
	}
	Files.Sort();

	int32 FilesChanged = 0, TotalRewrites = 0;
	for (const FString& File : Files)
	{
		const FString Normalized = File.Replace(TEXT("\\"), TEXT("/"));
		if (ShouldSkip(Normalized))
		{
			continue;
		}
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *File))
		{
			continue;
		}
		int32 FileRewrites = 0;
		for (const FBrandRule& Rule : GBrandRules)
		{
			FileRewrites += ApplyRule(Text, Rule);
		}
		if (FileRewrites == 0)
		{
			continue;
		}
		++FilesChanged;
		TotalRewrites += FileRewrites;
		if (bDry)
		{
			UE_LOG(LogRuitkMigrateBrand, Display, TEXT("[dry] %s: %d rewrite(s)"), *Normalized, FileRewrites);
		}
		else if (!FFileHelper::SaveStringToFile(Text, *File,
												FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			UE_LOG(LogRuitkMigrateBrand, Error, TEXT("%s: could not write"), *Normalized);
			return 1;
		}
	}

	UE_LOG(LogRuitkMigrateBrand, Display,
		   TEXT("RuitkMigrateBrand%s: %d file(s) rewritten (%d replacement(s)). Next: paste the "
				"MIGRATION-0.15.md [CoreRedirects] block, resave Blueprints referencing renamed "
				"types, then verify with -run=RuitkCompile -check."),
		   bDry ? TEXT(" (dry)") : TEXT(""), FilesChanged, TotalRewrites);
	return 0;
}
