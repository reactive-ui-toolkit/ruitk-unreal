// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Ruitk.Umg + Ruitk.Mvvm — the Phase-6 interop seams, headless:
//   our-inside-theirs: URuitkHostWidget mounts a registered component and unmounts on
//     ReleaseSlateResources (live-root count proves the teardown);
//   theirs-inside-ours: Ruitk::Umg::UserWidget hosts a UUserWidget via SObjectWidget;
//   world teardown: URuitkWorldSubsystem unmounts every root on world death (the PIE-end /
//     level-travel contract);
//   their-data-feeding-ours: UseField re-renders on FieldNotify broadcasts, unbinds on
//     unmount, and reads the default quietly once the VM is gone (stale-VM policy).

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "RuitkAssetBrush.h"
#include "RuitkCoreElements.h"
#include "RuitkFieldHooks.h"
#include "RuitkHostProps.h"
#include "RuitkHostWidget.h"
#include "RuitkMarshal.h"
#include "RuitkNode.h"
#include "RuitkReconciler.h"
#include "RuitkRoot.h"
#include "RuitkSignalViewModel.h"
#include "RuitkSlateElements.h"
#include "RuitkTestViewModel.h"
#include "RuitkUmgElement.h"
#include "RuitkWorldSubsystem.h"
#include "Slate/SObjectWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UmgTest
{
	static URuitkTestViewModel* GViewModel = nullptr;

	static FRuitkNodeArray FieldReaderComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
	{
		const int32 Score = Ruitk::Umg::UseField<int32>(Ctx, GViewModel, FName(TEXT("Score")), -1);
		return {Ruitk::TextBlock(FString::Printf(TEXT("Score: %d"), Score))};
	}
	RUITK_COMPONENT(FieldReaderComp)

	static bool ContainsText(SWidget& RootWidget, const FString& Needle)
	{
		if (RootWidget.GetType() == FName(TEXT("STextBlock")) &&
			static_cast<STextBlock&>(RootWidget).GetText().ToString().Contains(Needle))
		{
			return true;
		}
		FChildren* Children = RootWidget.GetChildren();
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			if (ContainsText(Children->GetChildAt(i).Get(), Needle))
			{
				return true;
			}
		}
		return false;
	}

	static int32 CountLiveReconcilers()
	{
		int32 Count = 0;
		FRuitkReconciler::ForEachLive([&Count](FRuitkReconciler&) { ++Count; });
		return Count;
	}
} // namespace UmgTest

const ::UE::FieldNotification::FFieldId URuitkTestViewModel::FFieldNotificationClassDescriptor::Score(TEXT("Score"), 0);

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkUmgTest, "Ruitk.Umg",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkUmgTest::RunTest(const FString&)
{
	AddExpectedError(TEXT("is not a registered component"), EAutomationExpectedErrorFlags::Contains, 0);
	const int32 Baseline = UmgTest::CountLiveReconcilers();

	// ── our UI inside theirs: URuitkHostWidget ──────────────────────────────────────────────
	{
		URuitkHostWidget* Host = NewObject<URuitkHostWidget>(GetTransientPackage());
		Host->ComponentName = FName(TEXT("HelloWorld")); // a compiled .uetkx gallery component
		TSharedRef<SWidget> Widget = Host->TakeWidget();
		TestEqual(TEXT("host mounted one root"), UmgTest::CountLiveReconcilers(), Baseline + 1);
		TestTrue(TEXT("compiled component renders inside UMG"),
				 UmgTest::ContainsText(Widget.Get(), TEXT("Hello, world!")));
		Host->ReleaseSlateResources(true);
		TestEqual(TEXT("ReleaseSlateResources unmounts"), UmgTest::CountLiveReconcilers(), Baseline);

		URuitkHostWidget* Unknown = NewObject<URuitkHostWidget>(GetTransientPackage());
		Unknown->ComponentName = FName(TEXT("NoSuchComponent"));
		TestTrue(TEXT("unknown component -> placeholder, no crash"),
				 UmgTest::ContainsText(Unknown->TakeWidget().Get(), TEXT("not a registered component")));
	}

	// ── world teardown contract: URuitkWorldSubsystem ───────────────────────────────────────
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("RuitkUmgTestWorld"));
		if (TestNotNull(TEXT("test world"), World))
		{
			URuitkWorldSubsystem* Subsystem = World->GetSubsystem<URuitkWorldSubsystem>();
			if (TestNotNull(TEXT("subsystem"), Subsystem))
			{
				const int32 Handle = Subsystem->MountNamed(FName(TEXT("HelloWorld")));
				TestTrue(TEXT("mounted"), Handle != INDEX_NONE);
				TestEqual(TEXT("one live root"), Subsystem->NumLiveRoots(), 1);
				TestEqual(TEXT("unknown name refuses"), Subsystem->MountNamed(FName(TEXT("NoSuchComponent"))),
						  (int32)INDEX_NONE);
			}
			World->DestroyWorld(false);
			TestEqual(TEXT("world death unmounted every root (PIE-end contract)"), UmgTest::CountLiveReconcilers(),
					  Baseline);
		}
	}

	// ── theirs inside ours: Ruitk::Umg::UserWidget ──────────────────────────────────────────
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("RuitkUmgHostWorld"));
		if (TestNotNull(TEXT("umg world"), World))
		{
			TSharedRef<FRuitkRoot> Root =
				FRuitkRoot::Create(Ruitk::Umg::UserWidget(URuitkTestUserWidget::StaticClass(), World));
			Root->FlushSync();
			TestTrue(TEXT("hosted UMG widget produced a Slate child"),
					 Root->GetWidget()->GetRootPanel()->GetChildren()->Num() > 0);
			Root->Unmount();
			World->DestroyWorld(false);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkMvvmTest, "Ruitk.Mvvm",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkMvvmTest::RunTest(const FString&)
{
	URuitkTestViewModel* Vm = NewObject<URuitkTestViewModel>(GetTransientPackage());
	Vm->AddToRoot();
	UmgTest::GViewModel = Vm;

	TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(Ruitk::FC(&UmgTest::FieldReaderComp));
	Root->FlushSync();
	SWidget& RootWidget = Root->GetWidget().Get();
	TestTrue(TEXT("initial field read"), UmgTest::ContainsText(RootWidget, TEXT("Score: 0")));

	Vm->SetScore(42);
	Root->FlushSync();
	TestTrue(TEXT("FieldNotify broadcast re-rendered"), UmgTest::ContainsText(RootWidget, TEXT("Score: 42")));

	Vm->SetScore(42); // equal set: no broadcast — nothing to assert beyond not crashing
	Root->FlushSync();

	// stale-VM policy: gone VM reads the caller default, quietly. (HmrRefreshAll defeats the
	// props-equality bailout — a plain root update correctly serves the cached output.)
	UmgTest::GViewModel = nullptr;
	Root->GetReconciler().HmrRefreshAll();
	Root->FlushSync();
	TestTrue(TEXT("stale VM reads default"), UmgTest::ContainsText(RootWidget, TEXT("Score: -1")));

	// unbind-on-unmount: a broadcast after unmount must not crash or resurrect anything
	UmgTest::GViewModel = Vm;
	Root->GetReconciler().HmrRefreshAll();
	Root->FlushSync();
	Root->Unmount();
	Vm->SetScore(77);
	TestEqual(TEXT("unmounted root stays down"), Root->GetReconciler().NumLiveFibers(), 0);

	Vm->RemoveFromRoot();
	UmgTest::GViewModel = nullptr;
	return true;
}

// ── TD-022 / D-17: asset brushes — GC-rooted texture/material brushes on SImage ────────────

namespace UmgTest
{
	static TSharedPtr<FSlateBrush> GBrush;

	static FRuitkNodeArray AssetImageComp(FRuitkContext&, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
	{
		FRuitkImageProps P;
		P.SetImage(GBrush);
		P.SetDesiredSizeOverride(FVector2D(16.0f, 16.0f));
		return {Ruitk::Slate::Image(MoveTemp(P))};
	}
	RUITK_COMPONENT(AssetImageComp)
} // namespace UmgTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkAssetBrushTest, "Ruitk.Umg.AssetBrush",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkAssetBrushTest::RunTest(const FString&)
{
	const int32 Baseline = Ruitk::Umg::NumTrackedAssetBrushes();

	UTexture2D* Texture = UTexture2D::CreateTransient(4, 4);
	if (!TestNotNull(TEXT("transient texture created"), Texture))
	{
		return false;
	}
	FWeakObjectPtr WeakTexture(Texture);

	UmgTest::GBrush = Ruitk::Umg::MakeAssetBrush(Texture, FVector2D(16.0f, 16.0f), FLinearColor::White);
	if (!TestTrue(TEXT("brush built"), UmgTest::GBrush.IsValid()))
	{
		return false;
	}
	TestTrue(TEXT("brush resource is the texture"), UmgTest::GBrush->GetResourceObject() == Texture);
	TestTrue(TEXT("tracked count grew"), Ruitk::Umg::NumTrackedAssetBrushes() > Baseline);

	// The Image adapter applies the brush onto a real SImage. Scoped so the host (and its GO-05
	// node pool, which stashes the released widget's props — and thus a brush ref) is destroyed
	// before the final baseline check.
	{
		TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(Ruitk::FC(&UmgTest::AssetImageComp));
		Root->FlushSync();
		TSharedRef<SWidget> ImageW = Root->GetWidget()->GetRootPanel()->GetChildren()->GetChildAt(0);
		TestEqual(TEXT("SImage mounted"), ImageW->GetType(), FName(TEXT("SImage")));

		// D-17: the texture survives GC while the brush is live (the FGCObject root references it).
		Texture = nullptr;
		CollectGarbage(RF_NoFlags, /*bPerformFullPurge*/ true);
		TestTrue(TEXT("texture survives GC while its brush is live"), WeakTexture.IsValid());

		Root->Unmount();
	}

	// Release: the brush drops, the root compacts the dead entry, the asset is free again.
	UmgTest::GBrush.Reset();
	CollectGarbage(RF_NoFlags, /*bPerformFullPurge*/ true);
	TestEqual(TEXT("tracked count returns to baseline after release"), Ruitk::Umg::NumTrackedAssetBrushes(), Baseline);
	return true;
}

// ── TD-021: the REVERSE MVVM bridge — URuitkSignalViewModel (ours feeding theirs) ────────────

namespace UmgTest
{
	static URuitkSignalViewModel* GSignalVm = nullptr;

	static FRuitkNodeArray SignalReaderComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
	{
		const int32 N = Ruitk::Umg::UseField<int32>(Ctx, GSignalVm, FName(TEXT("Int")), -1);
		return {Ruitk::TextBlock(FString::Printf(TEXT("N:%d"), N))};
	}
	RUITK_COMPONENT(SignalReaderComp)
} // namespace UmgTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkReverseBridgeTest, "Ruitk.Mvvm.ReverseBridge",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkReverseBridgeTest::RunTest(const FString&)
{
	URuitkSignalViewModel* Vm = NewObject<URuitkSignalViewModel>();
	Vm->AddToRoot();

	// ── the bridge broadcasts on change and skips when equal ──────────────────────────────────
	int32 IntBroadcasts = 0;
	const FDelegateHandle Handle = Vm->AddFieldValueChangedDelegate(
		URuitkSignalViewModel::FFieldNotificationClassDescriptor::Int,
		INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateLambda(
			[&IntBroadcasts](UObject*, ::UE::FieldNotification::FFieldId) { ++IntBroadcasts; }));

	Vm->SetInt(5);
	TestEqual(TEXT("Int field updated"), Vm->Int, 5);
	TestEqual(TEXT("broadcast fired once"), IntBroadcasts, 1);
	Vm->SetInt(5); // equal set
	TestEqual(TEXT("equal set did not broadcast"), IntBroadcasts, 1);
	Vm->RemoveFieldValueChangedDelegate(URuitkSignalViewModel::FFieldNotificationClassDescriptor::Int, Handle);

	// ── Set(FRuitkValue) routes by kind ─────────────────────────────────────────────────────────
	Vm->Set(FRuitkValue(3.5f));
	TestEqual(TEXT("float routed"), Vm->Float, 3.5f);
	Vm->Set(FRuitkValue(true));
	TestTrue(TEXT("bool routed"), Vm->Bool);
	Vm->Set(FRuitkValue(FText::FromString(TEXT("hello"))));
	TestEqual(TEXT("text routed"), Vm->Text.ToString(), FString(TEXT("hello")));
	Vm->Set(FRuitkValue(FString(TEXT("world"))));
	TestEqual(TEXT("string routed to text"), Vm->Text.ToString(), FString(TEXT("world")));

	// ── round-trip: Ruitk writes the VM -> VM broadcasts -> a UseField consumer re-renders ──────
	UmgTest::GSignalVm = Vm;
	Vm->SetInt(5);
	TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(Ruitk::FC(&UmgTest::SignalReaderComp));
	Root->FlushSync();
	SWidget& RootWidget = *Root->GetWidget();
	TestTrue(TEXT("consumer reads the field"), UmgTest::ContainsText(RootWidget, TEXT("N:5")));

	Vm->SetInt(9);
	Root->FlushSync();
	TestTrue(TEXT("consumer re-rendered on the reverse-bridge broadcast"),
			 UmgTest::ContainsText(RootWidget, TEXT("N:9")));

	Root->Unmount();
	UmgTest::GSignalVm = nullptr;
	Vm->RemoveFromRoot();
	return true;
}

// ── Ruitk.Umg.Lifecycle — Remount, MountNode, and the hosted-widget GC contract (audit §13)

namespace UmgLifecycleTest
{
	// Depth-first: find the first SObjectWidget and return its backing UUserWidget.
	static UUserWidget* FindHostedUserWidget(SWidget& Widget)
	{
		if (Widget.GetTypeAsString() == TEXT("SObjectWidget"))
		{
			return static_cast<SObjectWidget&>(Widget).GetWidgetObject();
		}
		FChildren* Children = Widget.GetChildren();
		for (int32 i = 0; Children && i < Children->Num(); ++i)
		{
			if (UUserWidget* Found = FindHostedUserWidget(Children->GetChildAt(i).Get()))
			{
				return Found;
			}
		}
		return nullptr;
	}
} // namespace UmgLifecycleTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkUmgLifecycleTest, "Ruitk.Umg.Lifecycle",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkUmgLifecycleTest::RunTest(const FString&)
{
	const int32 Baseline = UmgTest::CountLiveReconcilers();

	// ── URuitkHostWidget::Remount — re-resolves without leaking a root ─────────────────────────
	{
		URuitkHostWidget* Host = NewObject<URuitkHostWidget>(GetTransientPackage());
		Host->ComponentName = FName(TEXT("HelloWorld"));
		TSharedRef<SWidget> Widget = Host->TakeWidget();
		TestEqual(TEXT("mounted one root"), UmgTest::CountLiveReconcilers(), Baseline + 1);

		Host->ComponentName = FName(TEXT("SimpleCounter"));
		Host->Remount();
		TestEqual(TEXT("Remount did not leak a root"), UmgTest::CountLiveReconcilers(), Baseline + 1);
		TestTrue(TEXT("Remount re-resolved to the new component"),
				 UmgTest::ContainsText(Host->TakeWidget().Get(), TEXT("Count1: 0")));
		TestEqual(TEXT("re-TakeWidget still one root"), UmgTest::CountLiveReconcilers(), Baseline + 1);

		Host->ReleaseSlateResources(true);
		TestEqual(TEXT("released"), UmgTest::CountLiveReconcilers(), Baseline);
	}

	// ── URuitkWorldSubsystem::MountNode — node-based mounting (only MountNamed was tested) ──────
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("RuitkUmgLifecycleWorld"));
		if (TestNotNull(TEXT("test world"), World))
		{
			URuitkWorldSubsystem* Subsystem = World->GetSubsystem<URuitkWorldSubsystem>();
			if (TestNotNull(TEXT("subsystem"), Subsystem))
			{
				const int32 Handle = Subsystem->MountNode(Ruitk::TextBlock(TEXT("NODE-MOUNT")));
				TestTrue(TEXT("MountNode returns a handle"), Handle != INDEX_NONE);
				TestEqual(TEXT("one live root"), Subsystem->NumLiveRoots(), 1);
				Subsystem->UnmountHandle(Handle);
				TestEqual(TEXT("UnmountHandle removed it"), Subsystem->NumLiveRoots(), 0);
			}
			World->DestroyWorld(false);
		}
	}

	// ── hosted UUserWidget lifetime — alive across a full GC purge while mounted ─────────────
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("RuitkUmgGcWorld"));
		if (TestNotNull(TEXT("gc world"), World))
		{
			TSharedRef<FRuitkRoot> Root =
				FRuitkRoot::Create(Ruitk::Umg::UserWidget(URuitkTestUserWidget::StaticClass(), World));
			Root->FlushSync();

			TWeakObjectPtr<UUserWidget> Hosted = UmgLifecycleTest::FindHostedUserWidget(Root->GetWidget().Get());
			if (TestTrue(TEXT("found the hosted SObjectWidget's UUserWidget"), Hosted.IsValid()))
			{
				CollectGarbage(RF_NoFlags, /*bPerformFullPurge*/ true);
				TestTrue(TEXT("hosted widget survives a full GC purge while mounted"), Hosted.IsValid());

				Root->Unmount();
				CollectGarbage(RF_NoFlags, /*bPerformFullPurge*/ true);
				TestFalse(TEXT("hosted widget is collectable after unmount"), Hosted.IsValid());
			}
			World->DestroyWorld(false);
		}
	}
	return true;
}

// ── RuitkMarshal — the single FRuitkValue ↔ UPROPERTY conversion table (research-promised helper) ──

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkMarshalTest, "Ruitk.Umg.Marshal",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkMarshalTest::RunTest(const FString&)
{
	URuitkSignalViewModel* Vm = NewObject<URuitkSignalViewModel>();
	Vm->AddToRoot();

	// write: every supported category, with kind coercion where documented
	TestTrue(TEXT("int written"), Ruitk::Umg::MarshalToProperty(Vm, FName(TEXT("Int")), FRuitkValue(7)));
	TestEqual(TEXT("int landed"), Vm->Int, 7);
	TestTrue(TEXT("float<-int coerces"), Ruitk::Umg::MarshalToProperty(Vm, FName(TEXT("Float")), FRuitkValue(3)));
	TestEqual(TEXT("float landed"), Vm->Float, 3.0f);
	TestTrue(TEXT("bool written"), Ruitk::Umg::MarshalToProperty(Vm, FName(TEXT("Bool")), FRuitkValue(true)));
	TestTrue(TEXT("bool landed"), Vm->Bool);
	TestTrue(TEXT("text<-string coerces"),
			 Ruitk::Umg::MarshalToProperty(Vm, FName(TEXT("Text")), FRuitkValue(FString(TEXT("hello")))));
	TestEqual(TEXT("text landed"), Vm->Text.ToString(), FString(TEXT("hello")));

	// mismatches are skipped, never mangled (B13 rules)
	TestFalse(TEXT("bool into int refuses"), Ruitk::Umg::MarshalToProperty(Vm, FName(TEXT("Int")), FRuitkValue(true)));
	TestEqual(TEXT("int untouched by the refusal"), Vm->Int, 7);
	TestFalse(TEXT("missing property refuses"),
			  Ruitk::Umg::MarshalToProperty(Vm, FName(TEXT("NoSuchProp")), FRuitkValue(1)));
	TestFalse(TEXT("null object refuses"), Ruitk::Umg::MarshalToProperty(nullptr, FName(TEXT("Int")), FRuitkValue(1)));

	// read: round-trips with the kind following the property type
	FRuitkValue Out;
	TestTrue(TEXT("int read"), Ruitk::Umg::MarshalFromProperty(Vm, FName(TEXT("Int")), Out));
	TestTrue(TEXT("int kind + value"), Out.Kind == FRuitkValue::EKind::Int && Out.IntValue == 7);
	TestTrue(TEXT("text read"), Ruitk::Umg::MarshalFromProperty(Vm, FName(TEXT("Text")), Out));
	TestTrue(TEXT("text kind + value"),
			 Out.Kind == FRuitkValue::EKind::Text && Out.TextValue.ToString() == TEXT("hello"));
	TestFalse(TEXT("missing property read refuses"), Ruitk::Umg::MarshalFromProperty(Vm, FName(TEXT("NoSuchProp")), Out));

	Vm->RemoveFromRoot();
	return true;
}

// ── UseOwnedViewModel — create-and-own for the component lifetime (research-promised hook) ──

namespace UmgTest
{
	static URuitkSignalViewModel* GOwnedVmSeen = nullptr;
	static int32 GOwnedVmRenders = 0;

	static FRuitkNodeArray OwnedVmComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
	{
		URuitkSignalViewModel* Vm = Ruitk::Umg::UseOwnedViewModel<URuitkSignalViewModel>(Ctx);
		GOwnedVmSeen = Vm;
		++GOwnedVmRenders;
		const int32 N = Ruitk::Umg::UseField<int32>(Ctx, Vm, FName(TEXT("Int")), -1);
		return {Ruitk::TextBlock(FString::Printf(TEXT("Owned:%d"), N))};
	}
	RUITK_COMPONENT(OwnedVmComp)
} // namespace UmgTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkOwnedViewModelTest, "Ruitk.Mvvm.OwnedViewModel",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkOwnedViewModelTest::RunTest(const FString&)
{
	UmgTest::GOwnedVmSeen = nullptr;
	UmgTest::GOwnedVmRenders = 0;

	TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(Ruitk::FC(&UmgTest::OwnedVmComp));
	Root->FlushSync();
	URuitkSignalViewModel* Vm = UmgTest::GOwnedVmSeen;
	if (!TestNotNull(TEXT("hook created a viewmodel on first render"), Vm))
	{
		return false;
	}
	TWeakObjectPtr<URuitkSignalViewModel> Weak(Vm);
	TestTrue(TEXT("component reads its own VM"), UmgTest::ContainsText(*Root->GetWidget(), TEXT("Owned:0")));

	// identity is stable across re-renders, and the VM survives a full GC purge while mounted
	Vm->SetInt(4);
	Root->FlushSync();
	TestTrue(TEXT("same instance on re-render"), UmgTest::GOwnedVmSeen == Vm);
	TestTrue(TEXT("broadcast re-rendered through UseField"),
			 UmgTest::ContainsText(*Root->GetWidget(), TEXT("Owned:4")));
	CollectGarbage(RF_NoFlags, /*bPerformFullPurge*/ true);
	TestTrue(TEXT("owned VM survives GC while mounted"), Weak.IsValid());

	// unmount releases ownership — the VM becomes collectable
	Root->Unmount();
	UmgTest::GOwnedVmSeen = nullptr;
	CollectGarbage(RF_NoFlags, /*bPerformFullPurge*/ true);
	TestFalse(TEXT("owned VM collectable after unmount"), Weak.IsValid());
	return true;
}

// ── TD-028: the designer/BP → component channel — InitialProps + ViewModel through context ──

namespace UmgTest
{
	static FRuitkNodeArray HostPropReaderComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
	{
		const FString Title = Ruitk::Umg::UseHostProp(Ctx, FName(TEXT("Title")), TEXT("<default>"));
		UObject* Vm = Ruitk::Umg::UseHostViewModel(Ctx);
		const int32 N = Ruitk::Umg::UseField<int32>(Ctx, Vm, FName(TEXT("Int")), -1);
		return {Ruitk::TextBlock(FString::Printf(TEXT("T:%s N:%d"), *Title, N))};
	}
	RUITK_COMPONENT(HostPropReaderComp)
} // namespace UmgTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkUmgHostPropsTest, "Ruitk.Umg.HostProps",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkUmgHostPropsTest::RunTest(const FString&)
{
	Ruitk::RegisterNamedFactory(FName(TEXT("RuitkHostPropReader")), []() { return Ruitk::FC(&UmgTest::HostPropReaderComp); });

	URuitkSignalViewModel* Vm = NewObject<URuitkSignalViewModel>();
	Vm->AddToRoot();
	Vm->SetInt(5);

	// ── BP-set props + VM reach the hosted component ───────────────────────────────────────
	URuitkHostWidget* Host = NewObject<URuitkHostWidget>(GetTransientPackage());
	Host->ComponentName = FName(TEXT("RuitkHostPropReader"));
	Host->InitialProps.Add(FName(TEXT("Title")), TEXT("Hello"));
	Host->ViewModel = Vm;
	TSharedRef<SWidget> Widget = Host->TakeWidget();
	TestTrue(TEXT("initial props + viewmodel reached the component"),
			 UmgTest::ContainsText(Widget.Get(), TEXT("T:Hello N:5")));

	// ── SynchronizeProperties forwards edits into the LIVE tree (no remount) ─────────────────
	Host->InitialProps.Add(FName(TEXT("Title")), TEXT("Renamed"));
	Vm->SetInt(9);
	Host->SynchronizeProperties();
	TestTrue(TEXT("SynchronizeProperties re-published props + VM state in place"),
			 UmgTest::ContainsText(Widget.Get(), TEXT("T:Renamed N:9")));

	Host->ReleaseSlateResources(true);

	// ── nothing set → quiet defaults ─────────────────────────────────────────────────────────
	URuitkHostWidget* Bare = NewObject<URuitkHostWidget>(GetTransientPackage());
	Bare->ComponentName = FName(TEXT("RuitkHostPropReader"));
	TestTrue(TEXT("unset host props read the caller defaults"),
			 UmgTest::ContainsText(Bare->TakeWidget().Get(), TEXT("T:<default> N:-1")));
	Bare->ReleaseSlateResources(true);

	Vm->RemoveFromRoot();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
