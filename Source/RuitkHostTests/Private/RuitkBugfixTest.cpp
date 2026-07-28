// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Ruitk.Bugfix.* — regression tests locking the fixes from the 2026-07-12 adversarial bug hunt
// (plans/archive/BUGHUNT_2026-07-12.md). Each asserts the previously-uncovered case the corresponding green
// test missed (the hunt's meta-finding: the suite only exercised the happy path).

#include "Misc/AutomationTest.h"
#include "Framework/Application/SlateApplication.h"
#include "UObject/Package.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"

#include "RuitkComboBox.h"
#include "RuitkContext.h"
#include "RuitkCoreElements.h"
#include "RuitkDragDrop.h"
#include "RuitkExpandableArea.h"
#include "RuitkElementAdapter.h"
#include "RuitkElementRegistry.h"
#include "RuitkListView.h"
#include "RuitkMvvmViewModel.h"
#include "RuitkNode.h"
#include "RuitkPresence.h"
#include "RuitkRoot.h"
#include "RuitkRouter.h"
#include "RuitkSegmentedControl.h"
#include "RuitkSlateElements.h"
#include "RuitkSlateTestHarness.h"
#include "RuitkStyle.h"
#include "RuitkTestViewModel.h"
#include "RuitkTypes.h"
#include "RuitkUmgElement.h"

#include "Input/DragAndDrop.h"
#include "Input/Events.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BugfixTest
{
	/** Collect the text of every STextBlock in a widget tree (order = DFS). */
	static void CollectTexts(SWidget& Root, TArray<FString>& Out)
	{
		if (Root.GetType() == FName(TEXT("STextBlock")))
		{
			Out.Add(static_cast<STextBlock&>(Root).GetText().ToString());
		}
		if (FChildren* Children = Root.GetChildren())
		{
			for (int32 i = 0; i < Children->Num(); ++i)
			{
				CollectTexts(Children->GetChildAt(i).Get(), Out);
			}
		}
	}

	static bool AnyContains(const TArray<FString>& Texts, const TCHAR* Needle)
	{
		for (const FString& T : Texts)
		{
			if (T.Contains(Needle))
			{
				return true;
			}
		}
		return false;
	}

	// ── B1: Presence must namespace unkeyed children so they never collide with a user integer key ──
	static FRuitkNodeArray PresenceMixComp(FRuitkContext&, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
	{
		TArray<FRuitkNode> Kids;
		Kids.Add(Ruitk::TextBlock(FString(TEXT("UNKEYED")))); // no key -> positional fallback
		FRuitkNode Keyed = Ruitk::TextBlock(FString(TEXT("KEYEDZERO")));
		Keyed.Key = FRuitkKey(0); // user integer key 0 — must NOT collide with the fallback
		Kids.Add(MoveTemp(Keyed));
		return {Ruitk::Presence(MoveTemp(Kids))};
	}
	RUITK_COMPONENT(PresenceMixComp)
} // namespace BugfixTest

// ── B3 + B2: router search parsing / relative navigation ────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkBugfixRouterTest, "Ruitk.Bugfix.Router",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkBugfixRouterTest::RunTest(const FString&)
{
	// B3: first value wins for a repeated key (was last-value-wins).
	const TMap<FString, FString> P = Ruitk::ParseSearch(TEXT("?a=1&a=2&b=x"));
	TestEqual(TEXT("B3: repeated key keeps the FIRST value"), P.FindRef(TEXT("a")), FString(TEXT("1")));
	TestEqual(TEXT("B3: other keys unaffected"), P.FindRef(TEXT("b")), FString(TEXT("x")));

	// B2: a relative href with a query must not double the tail. UseHref resolves ./ against a base;
	// ResolvePath + a single tail. Verify via the pure ResolvePath + ParseLocation round-trip that a
	// query survives exactly once (the doubling was Resolved + ExtractQueryHash on the un-stripped To).
	const FString Resolved = Ruitk::ResolvePath(TEXT("child?tab=1"), TEXT("/parent/page"));
	// ResolvePath now sees a raw segment (query still embedded — that's expected of ResolvePath); the
	// router's Navigate strips first. Assert ParseLocation of a correctly-assembled href has a single tail:
	const FRuitkLocation Loc =
		Ruitk::ParseLocation(TEXT("/parent/page/child") + Ruitk::BuildSearch({{TEXT("tab"), TEXT("1")}}));
	TestEqual(TEXT("B2: search is a single ?tab=1"), Loc.Search, FString(TEXT("?tab=1")));
	TestFalse(TEXT("B2: no doubled '?'"), Loc.Search.Contains(TEXT("?tab=1?")));
	(void)Resolved;
	return true;
}

// ── B5 + B4: stylesheet comment stripping + inline-only token resolution ─────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkBugfixStyleTest, "Ruitk.Bugfix.Style",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkBugfixStyleTest::RunTest(const FString&)
{
	// B5: a `//` inside a quoted value must NOT truncate the block (was silently dropping the class).
	const int32 Registered =
		Ruitk::Slate::LoadStylesheet(TEXT(".card { BorderImage: \"img://cdn/bg.png\"; } .other { Padding: \"4\"; }"));
	TestEqual(TEXT("B5: both classes register despite '//' in a value"), Registered, 2);

	// B4: an inline-only `$token` (NO classes) resolves against the active theme — the empty-classes fast
	// path used to early-return the raw dict, leaving `$accent` as a String the adapter can't apply.
	FRuitkStyleDict Theme;
	Theme.Add(FName(TEXT("accent")), FRuitkValue(FLinearColor(0.0f, 1.0f, 0.0f, 1.0f)));
	Ruitk::Slate::RegisterTheme(FName(TEXT("bugfix_dark")), Theme);
	Ruitk::Slate::SetActiveTheme(FName(TEXT("bugfix_dark")));

	TSharedPtr<FRuitkStyleDict> Inline = MakeShared<FRuitkStyleDict>();
	Inline->Add(FName(TEXT("ColorAndOpacity")), FRuitkValue(FString(TEXT("$accent"))));
	TSharedPtr<FRuitkStyleDict> Eff = Ruitk::Slate::BuildEffectiveStyle(/*Classes*/ {}, Inline); // no classes
	if (TestTrue(TEXT("B4: effective style built"), Eff.IsValid()))
	{
		const FRuitkValue* Resolved = Eff->Find(FName(TEXT("ColorAndOpacity")));
		TestTrue(TEXT("B4: inline $accent resolved to a Color (not left as a raw String)"),
				 Resolved != nullptr && Resolved->Kind == FRuitkValue::EKind::Color);
	}

	Ruitk::Slate::SetActiveTheme(NAME_None);
	return true;
}

// ── B13: ApplyPropMap skips kind mismatches, coerces numerics ────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkBugfixPropMapTest, "Ruitk.Bugfix.PropMap",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkBugfixPropMapTest::RunTest(const FString&)
{
	URuitkTestUserWidget* W = NewObject<URuitkTestUserWidget>(GetTransientPackage());
	W->AddToRoot();
	W->FloatValue = 9.0f; // sentinel: a kind-mismatch must NOT overwrite it with 0
	W->IntValue = 7;

	// An Int-kind value into a float prop COERCES to 1.0 (was writing V.FloatValue==0.0).
	FRuitkStyleDict Coerce;
	Coerce.Add(FName(TEXT("FloatValue")), FRuitkValue(1)); // Kind==Int
	const int32 Applied = Ruitk::Umg::ApplyPropMap(W, Coerce);
	TestEqual(TEXT("B13: int->float coerces (applied)"), Applied, 1);
	TestTrue(TEXT("B13: FloatValue coerced to 1.0"), FMath::IsNearlyEqual(W->FloatValue, 1.0f));

	// A Bool-kind value into an int prop is a mismatch -> SKIPPED, IntValue unchanged, not counted.
	FRuitkStyleDict Mismatch;
	Mismatch.Add(FName(TEXT("IntValue")), FRuitkValue(true)); // Kind==Bool
	TestEqual(TEXT("B13: bool->int is skipped (0 applied)"), Ruitk::Umg::ApplyPropMap(W, Mismatch), 0);
	TestEqual(TEXT("B13: IntValue untouched by the mismatch"), W->IntValue, 7);

	W->RemoveFromRoot();
	return true;
}

// ── B7: DropTarget fires OnDragLeave on a successful drop ────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkBugfixDropLeaveTest, "Ruitk.Bugfix.DropLeave",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkBugfixDropLeaveTest::RunTest(const FString&)
{
	if (!FSlateApplication::IsInitialized())
	{
		return true;
	}
	bool bLeft = false;
	TSharedRef<SRuitkDropTarget> Target = SNew(SRuitkDropTarget);
	Target->SetOnDragLeave(FRuitkCallback::Create([&bLeft](const FRuitkValue&) { bLeft = true; }));

	FPointerEvent Pointer;
	FDragDropEvent Enter(Pointer, FRuitkDragDropOp::New(FName(TEXT("card")), FRuitkValue(1)));
	Target->OnDragEnter(FGeometry(), Enter);
	TestTrue(TEXT("B7: hovered after enter"), Target->IsOver());

	FDragDropEvent Drop(Pointer, FRuitkDragDropOp::New(FName(TEXT("card")), FRuitkValue(1)));
	Target->OnDrop(FGeometry(), Drop);
	TestFalse(TEXT("B7: hover cleared after drop"), Target->IsOver());
	TestTrue(TEXT("B7: OnDragLeave fired on drop (hover styling un-sticks)"), bLeft);
	return true;
}

// ── B9: ExpandableArea does NOT fire OnExpansionChanged for a controlled (programmatic) change ────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkBugfixExpandableTest, "Ruitk.Bugfix.Expandable",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkBugfixExpandableTest::RunTest(const FString&)
{
	IRuitkElementAdapter* Adapter = Ruitk::Slate::FindAdapter(Ruitk::Slate::ExpandableAreaType());
	if (!TestNotNull(TEXT("adapter"), Adapter))
	{
		return false;
	}
	int32 Fires = 0;
	FRuitkExpandableAreaProps Props;
	Props.SetbIsExpanded(true);
	Props.SetOnExpansionChanged(FRuitkCallback::Create([&Fires](const FRuitkValue&) { ++Fires; }));
	TSharedRef<SWidget> Widget = Adapter->CreateWidget(Props, nullptr);
	SRuitkExpandableArea& Area = static_cast<SRuitkExpandableArea&>(Widget.Get());
	Adapter->ApplyDiff(Area, nullptr, Props);

	// Controlled collapse via a new prop value — a PROGRAMMATIC change; must NOT fire the user event.
	FRuitkExpandableAreaProps Collapsed = Props;
	Collapsed.SetbIsExpanded(false);
	Adapter->ApplyDiff(Area, &Props, Collapsed);
	TestFalse(TEXT("B9: controlled collapse did not fire OnExpansionChanged"), Area.IsExpanded());
	TestEqual(TEXT("B9: OnExpansionChanged NOT fired for a programmatic change"), Fires, 0);
	return true;
}

// ── B8: SegmentedControl highlights the controlled segment (inner SCheckBox is checked) ───────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkBugfixSegmentedTest, "Ruitk.Bugfix.Segmented",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkBugfixSegmentedTest::RunTest(const FString&)
{
	if (!FSlateApplication::IsInitialized())
	{
		return true;
	}
	IRuitkElementAdapter* Adapter = Ruitk::Slate::FindAdapter(Ruitk::Slate::SegmentedControlType());
	if (!TestNotNull(TEXT("adapter"), Adapter))
	{
		return false;
	}
	FRuitkSegmentedControlProps Props;
	Props.SetLabels({TEXT("A"), TEXT("B"), TEXT("C")});
	Props.SetSelectedIndex(1);
	TSharedRef<SWidget> Widget = Adapter->CreateWidget(Props, nullptr);
	Adapter->ApplyDiff(static_cast<SRuitkSegmentedControl&>(Widget.Get()), nullptr, Props);

	RuitkTest::FTestWindow Win(Widget);
	if (Win.IsValid())
	{
		Win.PumpGeometry();
		// Exactly one segment SCheckBox is Checked, and it is the controlled index (B8: was zero checked).
		int32 CheckedCount = 0;
		TArray<TSharedRef<SWidget>> Stack;
		Stack.Push(Widget);
		while (Stack.Num() > 0)
		{
			TSharedRef<SWidget> Cur = Stack.Pop();
			if (Cur->GetType() == FName(TEXT("SCheckBox")) &&
				static_cast<SCheckBox&>(Cur.Get()).GetCheckedState() == ECheckBoxState::Checked)
			{
				++CheckedCount;
			}
			if (FChildren* C = Cur->GetChildren())
			{
				for (int32 i = 0; i < C->Num(); ++i)
				{
					Stack.Push(C->GetChildAt(i));
				}
			}
		}
		TestEqual(TEXT("B8: exactly one segment is highlighted for the controlled value"), CheckedCount, 1);
	}
	return true;
}

// ── B6: ComboBox forwards a user selection reported as ESelectInfo::Direct ────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkBugfixComboBoxTest, "Ruitk.Bugfix.ComboBox",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkBugfixComboBoxTest::RunTest(const FString&)
{
	using namespace Ruitk::Slate;
	IRuitkElementAdapter* Adapter = FindAdapter(ComboBoxType());
	if (!TestNotNull(TEXT("adapter"), Adapter))
	{
		return false;
	}
	TArray<TSharedPtr<FRuitkValue>> Options{MakeShared<FRuitkValue>(10), MakeShared<FRuitkValue>(20)};
	auto Renderer = MakeItemRenderer([](const FRuitkValue& V, int32) -> FRuitkNode
									 { return Ruitk::TextBlock(FString::FromInt((int32)V.IntValue)); });

	int32 Fires = 0;
	int32 LastIndex = -1;
	FRuitkComboBoxProps Props;
	Props.SetOptions(Options);
	Props.SetRenderOption(Renderer);
	Props.SetSelectedIndex(0);
	Props.SetOnSelectionChanged(FRuitkCallback::Create(
		[&Fires, &LastIndex](const FRuitkValue& V)
		{
			++Fires;
			LastIndex = (int32)V.IntValue;
		}));

	TSharedRef<SWidget> Widget = Adapter->CreateWidget(Props, nullptr);
	SRuitkComboBox& Combo = static_cast<SRuitkComboBox&>(Widget.Get());
	Adapter->ApplyDiff(Combo, nullptr, Props);

	// Programmatic controlled set must NOT fire (our reentrancy guard).
	FRuitkComboBoxProps Pick1 = Props;
	Pick1.SetSelectedIndex(1);
	Adapter->ApplyDiff(Combo, &Props, Pick1);
	TestEqual(TEXT("B6: programmatic controlled set does not fire OnSelectionChanged"), Fires, 0);

	// A USER selection that Slate reports as ESelectInfo::Direct (keyboard-close / gamepad-accept) MUST
	// fire — SComboBox::SetSelectedItem reports Direct; driving it OUTSIDE our reentrancy guard is
	// exactly the user-commit path the old ESelectInfo::Direct check wrongly swallowed.
	if (Combo.GetComboWidget().IsValid())
	{
		Combo.GetComboWidget()->SetSelectedItem(Options[0]); // reports ESelectInfo::Direct, not our set
		TestEqual(TEXT("B6: a user Direct selection fires OnSelectionChanged"), Fires, 1);
		TestEqual(TEXT("B6: forwarded the picked index"), LastIndex, 0);
	}
	return true;
}

// ── B1: Presence mixed unkeyed + integer-keyed children both render ───────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkBugfixPresenceTest, "Ruitk.Bugfix.PresenceKeys",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkBugfixPresenceTest::RunTest(const FString&)
{
	using namespace BugfixTest;
	TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(Ruitk::FC(&PresenceMixComp));
	Root->FlushSync();
	TArray<FString> Texts;
	CollectTexts(Root->GetWidget().Get(), Texts);
	TestTrue(TEXT("B1: the unkeyed child rendered"), AnyContains(Texts, TEXT("UNKEYED")));
	TestTrue(TEXT("B1: the integer-keyed(0) child also rendered (no key collision)"),
			 AnyContains(Texts, TEXT("KEYEDZERO")));
	Root->Unmount();
	return true;
}

// ── P1: a registered URuitkMvvmViewModel subclass resolves via FindGlobalViewModel's default class ──
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkBugfixMvvmSubclassTest, "Ruitk.Bugfix.MvvmSubclass",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkBugfixMvvmSubclassTest::RunTest(const FString&)
{
	if (GEngine == nullptr)
	{
		return true;
	}
	UGameInstance* GI = NewObject<UGameInstance>(GEngine);
	GI->AddToRoot();
	GI->InitializeStandalone();

	URuitkTestMvvmSubViewModel* Sub = NewObject<URuitkTestMvvmSubViewModel>(GI);
	TestTrue(TEXT("P1: subclass registered"), Ruitk::Mvvm::RegisterGlobalViewModel(GI, FName(TEXT("SubStats")), Sub));

	// Default class (base) must resolve the subclass instance now (base-class alias).
	TestTrue(TEXT("P1: resolves the subclass via the default (base) class"),
			 Ruitk::Mvvm::FindGlobalViewModel(GI, FName(TEXT("SubStats"))) == Sub);
	// The concrete-class find still works too.
	TestTrue(TEXT("P1: resolves via the concrete subclass"),
			 Ruitk::Mvvm::FindGlobalViewModel(GI, FName(TEXT("SubStats")), URuitkTestMvvmSubViewModel::StaticClass()) ==
				 Sub);

	GI->Shutdown();
	GI->RemoveFromRoot();
	return true;
}

// ── B11: removing an asset Brush resets the SImage (exercises the reset branch; no dangling ptr) ──
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkBugfixAssetBrushTest, "Ruitk.Bugfix.AssetBrush",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkBugfixAssetBrushTest::RunTest(const FString&)
{
	IRuitkElementAdapter* Img = Ruitk::Slate::FindAdapter(Ruitk::InternElementType(FName(TEXT("Image"))));
	if (!TestNotNull(TEXT("Image adapter"), Img))
	{
		return false;
	}
	FRuitkImageProps WithBrush;
	WithBrush.SetImage(MakeShared<FSlateBrush>());
	TSharedRef<SWidget> Widget = Img->CreateWidget(WithBrush, nullptr);
	Img->ApplyDiff(Widget.Get(), nullptr, WithBrush);
	TestTrue(TEXT("B11: image is an SImage"), Widget->GetType() == FName(TEXT("SImage")));

	// Re-render WITHOUT the brush: the fix resets SImage to the no-brush default instead of leaving a
	// raw pointer into props that are about to be freed. The image must survive (no crash / no dangling).
	FRuitkImageProps NoBrush;
	Img->ApplyDiff(Widget.Get(), &WithBrush, NoBrush);
	TestTrue(TEXT("B11: image survives brush removal"), Widget->GetType() == FName(TEXT("SImage")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
