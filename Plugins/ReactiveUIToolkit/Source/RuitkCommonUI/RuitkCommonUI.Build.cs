// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

using UnrealBuildTool;

// CommonUI citizenship (Phase 6): URuitkActivatableScreen, UseActivation, UseInputMethod, the
// UseSafeArea platform override. We live INSIDE CommonUI, never rebuild it (D-25).
// Deps added with the code, per D-27: CoreUObject, Engine, CommonUI (via an OPTIONAL plugin
// reference in the .uplugin -- Phase 6 verifies optional refs gate module loading cleanly and
// falls back to Build.cs conditionals if they do not), RuitkCore, RuitkUMG.
public class RuitkCommonUI : ModuleRules
{
	public RuitkCommonUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UMG",
			"Slate",
			"SlateCore",
			"CommonUI",
			"CommonInput",
			"GameplayTags",
			"RuitkCore",
			"RuitkSlate",
			"RuitkUMG",
		});
	}
}