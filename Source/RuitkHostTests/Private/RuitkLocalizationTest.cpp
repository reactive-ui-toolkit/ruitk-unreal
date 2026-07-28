// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Localization (MASTER_PLAN Phase 7 item 2): the culture-change → root re-render mechanism
// (RuitkCultureSync) and a tripwire on the committed gather output (the RuitkDemo localization
// target must keep gathering the markup's NSLOCTEXT entries from the committed *.uetkx.inl —
// masks live in Config/Localization/RuitkDemo_Gather.ini; regenerate via
//   UnrealEditor-Cmd <proj> -run=GatherText -config=Config/Localization/RuitkDemo_Gather.ini).

#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RuitkContext.h"
#include "RuitkCoreElements.h"
#include "RuitkCultureSync.h"
#include "RuitkMockHost.h"

#if WITH_DEV_AUTOMATION_TESTS

#define RUITK_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace LocTestState
{
	static int32 RenderCount = 0;
} // namespace LocTestState

static FRuitkNodeArray LocProbeComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	++LocTestState::RenderCount;
	// The FText below is what a compiled .uetkx literal becomes (self-namespaced NSLOCTEXT) —
	// lazy, so it re-resolves on culture change; the re-render heals anything baked around it.
	return {Ruitk::TextBlock(NSLOCTEXT("Uetkx.LocProbe", "LocProbe_0", "Localized probe"))};
}
RUITK_COMPONENT(LocProbeComp)

// ─────────────────────────────────────────────────────────────────────────────────────────
// The refresh mechanism: every live root re-renders when the handler fires.
// ─────────────────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkLocRefreshTest, "Ruitk.Loc.CultureRefreshMechanism", RUITK_TEST_FLAGS)
bool FRuitkLocRefreshTest::RunTest(const FString&)
{
	LocTestState::RenderCount = 0;
	FRuitkTestHarness H;

	AddInfo(TEXT("[loc] 1/2 mount probe"));
	H.Mount(Ruitk::FC(&LocProbeComp));
	TestEqual(TEXT("initial render count == 1"), LocTestState::RenderCount, 1);

	AddInfo(TEXT("[loc] 2/2 refresh handler -> one coalesced re-render"));
	Ruitk::RefreshAllRootsForCultureChange();
	H.Pump();
	TestEqual(TEXT("refresh re-rendered the live root"), LocTestState::RenderCount, 2);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// The wiring: a REAL culture change (text-revision bump) reaches the reconciler.
// ─────────────────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkLocCultureChangeTest, "Ruitk.Loc.CultureChangeRerenders", RUITK_TEST_FLAGS)
bool FRuitkLocCultureChangeTest::RunTest(const FString&)
{
	LocTestState::RenderCount = 0;
	// Unit suites do not run StartupModule (house rule) — subscribe here; Register is idempotent
	// so this coexists with the module's own registration when the whole battery runs.
	Ruitk::RegisterCultureSync();

	FRuitkTestHarness H;
	H.Mount(Ruitk::FC(&LocProbeComp));
	TestEqual(TEXT("initial render count == 1"), LocTestState::RenderCount, 1);

	FInternationalization& I18N = FInternationalization::Get();
	const FString OriginalCulture = I18N.GetCurrentCulture()->GetName();
	const FString OtherCulture = OriginalCulture.StartsWith(TEXT("fr")) ? TEXT("de") : TEXT("fr");

	AddInfo(FString::Printf(TEXT("[loc] culture switch %s -> %s"), *OriginalCulture, *OtherCulture));
	if (!I18N.SetCurrentCulture(OtherCulture))
	{
		AddWarning(
			FString::Printf(TEXT("culture '%s' unavailable on this machine — wiring not exercised"), *OtherCulture));
		return true;
	}
	H.Pump();
	const int32 AfterSwitch = LocTestState::RenderCount;

	// Restore FIRST so a failing assertion can't leave the process in a foreign culture.
	I18N.SetCurrentCulture(OriginalCulture);
	H.Pump();

	TestTrue(TEXT("culture change re-rendered the live root"), AfterSwitch >= 2);
	TestTrue(TEXT("restore re-rendered again"), LocTestState::RenderCount > AfterSwitch);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Gather output tripwire: the committed manifest carries the markup's Uetkx.* namespaces.
// ─────────────────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkLocGatherManifestTest, "Ruitk.Loc.GatherManifest", RUITK_TEST_FLAGS)
bool FRuitkLocGatherManifestTest::RunTest(const FString&)
{
	const FString ManifestPath =
		FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Localization/RuitkDemo/RuitkDemo.manifest"));
	FString Manifest;
	if (!TestTrue(TEXT("RuitkDemo.manifest exists (run the GatherText commandlet — see this file's header)"),
				  FFileHelper::LoadFileToString(Manifest, *ManifestPath)))
	{
		return false;
	}
	// The .uetkx codegen self-namespaces every literal as "Uetkx.<Basename>"; the manifest nests
	// the dotted form as a "Uetkx" parent namespace with per-file subnamespaces.
	TestTrue(TEXT("manifest gathered markup text (the 'Uetkx' namespace is present)"),
			 Manifest.Contains(TEXT("\"Uetkx\"")));
	// And the plugin's own runtime strings gather too (palette category, design-time labels).
	TestTrue(TEXT("manifest gathered plugin strings (the 'Ruitk' namespace is present)"),
			 Manifest.Contains(TEXT("\"Ruitk\"")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
