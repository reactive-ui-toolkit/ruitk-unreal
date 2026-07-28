// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Ruitk.Widgets.SegmentedControl — TD-012 tail: labelled tab-bar selector. Adapter-driven:
// proves segment baking from Labels, controlled SelectedIndex (skip-when-equal), and the
// construct-only Labels reconstruct-mask gate (a label-set change forces a widget replacement).

#include "Misc/AutomationTest.h"
#include "RuitkElementAdapter.h"
#include "RuitkElementRegistry.h"
#include "RuitkSegmentedControl.h"
#include "RuitkTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkSegmentedControlTest, "Ruitk.Widgets.SegmentedControl",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkSegmentedControlTest::RunTest(const FString&)
{
	IRuitkElementAdapter* Adapter = Ruitk::Slate::FindAdapter(Ruitk::Slate::SegmentedControlType());
	if (!TestNotNull(TEXT("SegmentedControl adapter registered"), Adapter))
	{
		return false;
	}

	FRuitkSegmentedControlProps Props;
	Props.SetLabels({TEXT("Easy"), TEXT("Normal"), TEXT("Hard")});
	Props.SetSelectedIndex(1);

	TSharedRef<SWidget> Widget = Adapter->CreateWidget(Props, nullptr);
	SRuitkSegmentedControl& Seg = static_cast<SRuitkSegmentedControl&>(Widget.Get());
	Adapter->ApplyDiff(Seg, nullptr, Props);

	TestEqual(TEXT("one segment per label"), Seg.NumSegments(), 3);
	TestEqual(TEXT("initial selection is index 1"), Seg.GetSelectedIndex(), 1);

	// Controlled selection: a new SelectedIndex drives SetValue.
	FRuitkSegmentedControlProps Pick2 = Props;
	Pick2.SetSelectedIndex(2);
	Adapter->ApplyDiff(Seg, &Props, Pick2);
	TestEqual(TEXT("controlled prop moved selection to index 2"), Seg.GetSelectedIndex(), 2);

	// The reconstruct mask covers Labels: same labels -> no rebuild; changed labels -> rebuild.
	TestFalse(TEXT("same labels -> no reconstruct"), Adapter->ConstructOnlyChanged(Props, Pick2));

	FRuitkSegmentedControlProps Relabel;
	Relabel.SetLabels({TEXT("On"), TEXT("Off")});
	Relabel.SetSelectedIndex(0);
	TestTrue(TEXT("changed labels -> reconstruct"), Adapter->ConstructOnlyChanged(Props, Relabel));

	// A fresh widget from the new labels bakes the new segment count.
	TSharedRef<SWidget> W2 = Adapter->CreateWidget(Relabel, nullptr);
	SRuitkSegmentedControl& Seg2 = static_cast<SRuitkSegmentedControl&>(W2.Get());
	TestEqual(TEXT("relabelled widget has two segments"), Seg2.NumSegments(), 2);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
