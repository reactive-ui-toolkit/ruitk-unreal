// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

using UnrealBuildTool;

// Editor-only surface (Phases 3-4, 8): the compile-on-save watcher (three triggers + busy
// guard/deadman + MessageLog "Ruitk" de-dup), RuitkCompile/RuitkExportSchema/RuitkContractDump
// commandlets, .uetkx asset actions (browser visibility, New Component, open-external), and
// later the Inspector tab. Deps added with the code, per D-27:
//   CoreUObject, Engine, UnrealEd, DirectoryWatcher, Projects, RuitkCore,
//   RuitkToolchain, RuitkInterp.
public class RuitkEditor : ModuleRules
{
	public RuitkEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",		 // commandlet UCLASSes
			"Engine",			 // UCommandlet base
			"Projects",			 // sweep roots
			"RuitkCore",	 // FRuitkNode / FRuitkRoot (the preview panel, TD-006)
			"RuitkSlate",	 // FRuitkRoot mount surface (the preview panel, TD-006)
			"RuitkUMG",		 // URuitkSettings CDO — the Runtime section of the Settings window
			"RuitkInterp",	 // FUetkxFileScan parser (preview scan — HMR v2 deleted the interpreter)
			"RuitkToolchain", // FUetkxDriver / FUetkxCodegen
			"InputCore",			 // SEditableTextBox in the preview panel
			"WorkspaceMenuStructure", // group the preview/HMR tabs under Window > Tools (grouped entries)
			"ToolMenus",			  // the top-level RuitkUetkx main-menu (HMR v2 Phase 2)
			"DeveloperSettings",	  // URuitkUetkxEditorSettings (HMR v2 Phase 3)
			"PropertyEditor",		  // IDetailsView — the Settings window (SRuitkSettingsPanel)
			"UnrealEd",				  // FEditorDelegates PIE hooks for "Follow Play" (HMR v2)
			"DirectoryWatcher",		 // watcher trigger 1
			"Slate",			 // window-activation trigger
			"SlateCore",
			"MessageLog",  // the "Ruitk" dock listing
		});

		// Live Coding is win64-only: Linux/Mac installed engines ship no UnrealEditor-LiveCoding
		// library, so an unconditional dependency dies at link (`-lUnrealEditor-LiveCoding` not
		// found). The consumer (FUetkxHmrController) guards every use with WITH_LIVE_CODING and
		// degrades to its "Live Coding module is not available" path off Windows.
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDependencyModuleNames.Add("LiveCoding"); // FUetkxHmrController drives the Live Coding session (HMR v2, D-HMR-8)
		}
	}
}