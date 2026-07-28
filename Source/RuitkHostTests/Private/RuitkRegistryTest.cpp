// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Ruitk.Core.Registry — FILE_SCOPED_EXPORTS (FS-05): the named-factory registry keys by
// FILE-QUALIFIED ids while the designer edges speak short names. Pins the resolution contract:
// exact FQN always hits; a short name hits when exactly ONE `::<Name>` tail matches; several
// matches are AMBIGUOUS (never silent first-wins); enumeration is sorted; replace-on-
// re-register (the Live Coding path) is preserved.

#include "Misc/AutomationTest.h"
#include "RuitkCoreElements.h"
#include "RuitkNode.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkRegistryTest, "Ruitk.Core.Registry",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkRegistryTest::RunTest(const FString&)
{
	const FName FqnA(TEXT("RuitkUetkx_RegProbe_DirA_Panel::RegProbePanel"));
	const FName FqnB(TEXT("RuitkUetkx_RegProbe_DirB_Panel::RegProbePanel"));
	const FName FqnUnique(TEXT("RuitkUetkx_RegProbe_DirA_Panel::RegProbeUnique"));
	Ruitk::RegisterNamedFactory(FqnA, []() { return Ruitk::Fragment({}); });
	Ruitk::RegisterNamedFactory(FqnB, []() { return Ruitk::Fragment({}); });
	Ruitk::RegisterNamedFactory(FqnUnique, []() { return Ruitk::Fragment({}); });

	// Exact FQN: always a Hit, even while the short name is ambiguous.
	{
		FName Key;
		TestEqual(TEXT("exact FQN hits"), (int32)Ruitk::ResolveNamed(FqnA, Key), (int32)Ruitk::EResolveNamed::Hit);
		TestEqual(TEXT("exact FQN resolves to itself"), Key, FqnA);
		TestTrue(TEXT("HasNamedFactory accepts the FQN"), Ruitk::HasNamedFactory(FqnA));
	}

	// Unique short name: one tail match — the designer-edge convenience path.
	{
		FName Key;
		TestEqual(TEXT("unique short name hits"), (int32)Ruitk::ResolveNamed(FName(TEXT("RegProbeUnique")), Key),
				  (int32)Ruitk::EResolveNamed::Hit);
		TestEqual(TEXT("short name resolves to the FQN"), Key, FqnUnique);
		TestTrue(TEXT("HasNamedFactory accepts the unique short name"),
				 Ruitk::HasNamedFactory(FName(TEXT("RegProbeUnique"))));
	}

	// Ambiguous short name: two files export it — candidates listed, no first-wins.
	{
		FName Key;
		TArray<FName> Candidates;
		TestEqual(TEXT("two-file short name is AMBIGUOUS"),
				  (int32)Ruitk::ResolveNamed(FName(TEXT("RegProbePanel")), Key, &Candidates),
				  (int32)Ruitk::EResolveNamed::Ambiguous);
		TestEqual(TEXT("both candidates named"), Candidates.Num(), 2);
		TestTrue(TEXT("candidates carry the qualified ids"), Candidates.Contains(FqnA) && Candidates.Contains(FqnB));
		TestFalse(TEXT("HasNamedFactory rejects the ambiguous short name"),
				  Ruitk::HasNamedFactory(FName(TEXT("RegProbePanel"))));
	}

	// Miss: unknown short name, and an explicit qualification that matches nothing.
	{
		FName Key;
		TestEqual(TEXT("unknown short name misses"), (int32)Ruitk::ResolveNamed(FName(TEXT("RegProbeNothing")), Key),
				  (int32)Ruitk::EResolveNamed::Miss);
		TestEqual(TEXT("unknown qualification misses (no suffix fallback through ::)"),
				  (int32)Ruitk::ResolveNamed(FName(TEXT("RuitkUetkx_Nowhere::RegProbeUnique")), Key),
				  (int32)Ruitk::EResolveNamed::Miss);
	}

	// Enumeration: sorted, carries the probes.
	{
		TArray<FName> All;
		Ruitk::GetRegisteredFactoryNames(All);
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
		Ruitk::RegisterNamedFactory(FqnUnique, []() { return Ruitk::Fragment({}); });
		FName Key;
		TestEqual(TEXT("re-registration keeps the single Hit"),
				  (int32)Ruitk::ResolveNamed(FName(TEXT("RegProbeUnique")), Key), (int32)Ruitk::EResolveNamed::Hit);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
