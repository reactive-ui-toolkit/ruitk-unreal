// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

// Dedicated-server target. Exists from day one because MASTER_PLAN D-27's server policy is
// CI-verified: Reactive UI Toolkit modules stay present on Server targets (no TargetDenyList — that
// would break user Build.cs), and runtime behavior is gated instead (URuitkSubsystem declines
// to create on dedicated servers; mounts checkf a non-server world).
public class RuitkUnrealDemoServerTarget : TargetRules
{
	public RuitkUnrealDemoServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("RuitkDemo");
	}
}
