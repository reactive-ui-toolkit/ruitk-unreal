// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// ReactiveUI.Core.Registry — FILE_SCOPED_EXPORTS (FS-05): the named-factory registry keys by
// FILE-QUALIFIED ids while the designer edges speak short names. Pins the resolution contract:
// exact FQN always hits; a short name hits when exactly ONE `::<Name>` tail matches; several
// matches are AMBIGUOUS (never silent first-wins); enumeration is sorted; replace-on-
// re-register (the Live Coding path) is preserved.

#include "Misc/AutomationTest.h"
#include "RuiCoreElements.h"
#include "RuiNode.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuiRegistryTest, "ReactiveUI.Core.Registry",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuiRegistryTest::RunTest(const FString&)
{
	const FName FqnA(TEXT("RuiUetkx_RegProbe_DirA_Panel::RegProbePanel"));
	const FName FqnB(TEXT("RuiUetkx_RegProbe_DirB_Panel::RegProbePanel"));
	const FName FqnUnique(TEXT("RuiUetkx_RegProbe_DirA_Panel::RegProbeUnique"));
	RUI::RegisterNamedFactory(FqnA, []() { return RUI::Fragment({}); });
	RUI::RegisterNamedFactory(FqnB, []() { return RUI::Fragment({}); });
	RUI::RegisterNamedFactory(FqnUnique, []() { return RUI::Fragment({}); });

	// Exact FQN: always a Hit, even while the short name is ambiguous.
	{
		FName Key;
		TestEqual(TEXT("exact FQN hits"), (int32)RUI::ResolveNamed(FqnA, Key), (int32)RUI::EResolveNamed::Hit);
		TestEqual(TEXT("exact FQN resolves to itself"), Key, FqnA);
		TestTrue(TEXT("HasNamedFactory accepts the FQN"), RUI::HasNamedFactory(FqnA));
	}

	// Unique short name: one tail match — the designer-edge convenience path.
	{
		FName Key;
		TestEqual(TEXT("unique short name hits"),
				  (int32)RUI::ResolveNamed(FName(TEXT("RegProbeUnique")), Key), (int32)RUI::EResolveNamed::Hit);
		TestEqual(TEXT("short name resolves to the FQN"), Key, FqnUnique);
		TestTrue(TEXT("HasNamedFactory accepts the unique short name"),
				 RUI::HasNamedFactory(FName(TEXT("RegProbeUnique"))));
	}

	// Ambiguous short name: two files export it — candidates listed, no first-wins.
	{
		FName Key;
		TArray<FName> Candidates;
		TestEqual(TEXT("two-file short name is AMBIGUOUS"),
				  (int32)RUI::ResolveNamed(FName(TEXT("RegProbePanel")), Key, &Candidates),
				  (int32)RUI::EResolveNamed::Ambiguous);
		TestEqual(TEXT("both candidates named"), Candidates.Num(), 2);
		TestTrue(TEXT("candidates carry the qualified ids"),
				 Candidates.Contains(FqnA) && Candidates.Contains(FqnB));
		TestFalse(TEXT("HasNamedFactory rejects the ambiguous short name"),
				  RUI::HasNamedFactory(FName(TEXT("RegProbePanel"))));
	}

	// Miss: unknown short name, and an explicit qualification that matches nothing.
	{
		FName Key;
		TestEqual(TEXT("unknown short name misses"),
				  (int32)RUI::ResolveNamed(FName(TEXT("RegProbeNothing")), Key), (int32)RUI::EResolveNamed::Miss);
		TestEqual(TEXT("unknown qualification misses (no suffix fallback through ::)"),
				  (int32)RUI::ResolveNamed(FName(TEXT("RuiUetkx_Nowhere::RegProbeUnique")), Key),
				  (int32)RUI::EResolveNamed::Miss);
	}

	// Enumeration: sorted, carries the probes.
	{
		TArray<FName> All;
		RUI::GetRegisteredFactoryNames(All);
		TestTrue(TEXT("enumeration carries the probes"),
				 All.Contains(FqnA) && All.Contains(FqnB) && All.Contains(FqnUnique));
		for (int32 i = 1; i < All.Num(); ++i)
		{
			if (!(All[i - 1].LexicalLess(All[i]) || All[i - 1] == All[i]))
			{
				AddError(TEXT("enumeration is not sorted"));
				break;
			}
		}
	}

	// Replace-on-re-register (Live Coding): same key, new factory — still ONE registration.
	{
		RUI::RegisterNamedFactory(FqnUnique, []() { return RUI::Fragment({}); });
		FName Key;
		TestEqual(TEXT("re-registration keeps the single Hit"),
				  (int32)RUI::ResolveNamed(FName(TEXT("RegProbeUnique")), Key), (int32)RUI::EResolveNamed::Hit);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
