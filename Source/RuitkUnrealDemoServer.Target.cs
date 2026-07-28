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
		// Latest, matching the Game/Editor targets: a V5 pin makes 5.7+'s UBT reject the target
		// ("modifies UndefinedIdentifierWarningLevel: Off != Error" — shared build environment;
		// Unique is illegal on installed engines). On 5.6, Latest == V5, so nothing changes there.
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("RuitkDemo");
	}
}
