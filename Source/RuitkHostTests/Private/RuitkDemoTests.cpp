// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Ruitk.Demos — the demos_test.gd analogue: every gallery entry mounts headlessly and
// unmounts clean (zero live fibers), a real button click drives state through a screen, and
// the shell's menu switches screens (old screen unmounted, new one mounted).

#include "Misc/AutomationTest.h"
#include "RuitkDemoScreens.h"
#include "RuitkRoot.h"
#include "RuitkSlateElements.h"
#include "RuitkSlateHost.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DemoTest
{
	static SWidget* FindByType(SWidget& RootWidget, FName TypeName)
	{
		if (RootWidget.GetType() == TypeName)
		{
			return &RootWidget;
		}
		FChildren* Children = RootWidget.GetChildren();
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			if (SWidget* Found = FindByType(Children->GetChildAt(i).Get(), TypeName))
			{
				return Found;
			}
		}
		return nullptr;
	}

	/** First STextBlock whose text CONTAINS Needle (depth-first). */
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

	/** First SButton whose label contains (or exactly equals) Needle. */
	static SButton* FindButton(SWidget& RootWidget, const FString& Needle, bool bExact = false)
	{
		if (RootWidget.GetType() == FName(TEXT("SButton")))
		{
			FChildren* Kids = RootWidget.GetChildren();
			if (Kids->Num() > 0 && Kids->GetChildAt(0)->GetType() == FName(TEXT("STextBlock")))
			{
				const FString Label = StaticCastSharedRef<STextBlock>(Kids->GetChildAt(0))->GetText().ToString();
				if (bExact ? Label == Needle : Label.Contains(Needle))
				{
					return static_cast<SButton*>(&RootWidget);
				}
			}
		}
		FChildren* Children = RootWidget.GetChildren();
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			if (SButton* Found = FindButton(Children->GetChildAt(i).Get(), Needle, bExact))
			{
				return Found;
			}
		}
		return nullptr;
	}
} // namespace DemoTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkDemosTest, "Ruitk.Demos",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkDemosTest::RunTest(const FString&)
{
	// AcceptanceLab's Ref-attach probe deliberately runs every commit (Ruitk::EveryCommit(),
	// section 9) — the M5 strict no-deps warning is expected once and suppressed.
	AddExpectedError(TEXT("has no dependency array"), EAutomationExpectedErrorFlags::Contains, 0);

	AddInfo(TEXT("[demos] every compiled .uetkx screen self-registered its factory"));
	for (const FName& Name : RuitkDemo::GetCompiledScreenNames())
	{
		// Ruitk::Named falls back to an empty Fragment — a missing registration would
		// otherwise "mount" invisibly. This is the seam between the gallery and the
		// committed generated code (RuitkDemo.Uetkx.gen.cpp).
		TestTrue(FString::Printf(TEXT("'%s' registered"), *Name.ToString()), Ruitk::HasNamedFactory(Name));
	}

	AddInfo(TEXT("[demos] every gallery entry mounts headlessly and unmounts clean"));
	for (const RuitkDemo::FRuitkDemoEntry& Entry : RuitkDemo::GetGalleryEntries())
	{
		TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(Entry.Make());
		Root->FlushSync();
		TestTrue(FString::Printf(TEXT("'%s' mounted something"), *Entry.Name),
				 Root->GetWidget()->GetRootPanel()->GetChildren()->Num() > 0);
		Root->Unmount();
		TestEqual(FString::Printf(TEXT("'%s' unmounted to zero fibers"), *Entry.Name),
				  Root->GetReconciler().NumLiveFibers(), 0);
	}

	AddInfo(TEXT("[demos] a real click drives the counter screen"));
	{
		TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(RuitkDemo::GetGalleryEntries()[1].Make()); // Counter
		Root->FlushSync();
		SWidget& RootWidget = Root->GetWidget().Get();
		// SimpleCounter became the two-counter HMR field-test vehicle (custom hook + UseState
		// + the .style companion) — the pin follows its real content.
		TestTrue(TEXT("starts at 0"), DemoTest::ContainsText(RootWidget, TEXT("Count1: 0")));
		SButton* Plus = DemoTest::FindButton(RootWidget, TEXT("+"));
		if (TestNotNull(TEXT("found the + button"), Plus))
		{
			Plus->SimulateClick();
			Root->FlushSync();
			TestTrue(TEXT("count incremented"), DemoTest::ContainsText(RootWidget, TEXT("Count1: 1")));
		}
	}

	AddInfo(TEXT("[demos] the interop screens render their headless fallbacks (audit Phase 4)"));
	{
		TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(Ruitk::Named(FName(TEXT("UmgHostDemo"))));
		Root->FlushSync();
		TestTrue(TEXT("UmgHostDemo renders the no-world fallback headless"),
				 DemoTest::ContainsText(Root->GetWidget().Get(), TEXT("press Play")));
		Root->Unmount();
	}
	{
		TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(Ruitk::Named(FName(TEXT("CommonUiDemo"))));
		Root->FlushSync();
		SWidget& W = Root->GetWidget().Get();
		TestTrue(TEXT("CommonUiDemo renders the toggle half headless"),
				 DemoTest::ContainsText(W, TEXT("Toggle activation")));
		TestTrue(TEXT("CommonUiDemo renders the real-stack fallback headless"),
				 DemoTest::ContainsText(W, TEXT("press Play")));
		Root->Unmount();
	}

	AddInfo(TEXT("[demos] the shell menu switches screens (remount on switch)"));
	{
		TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(RuitkDemo::GalleryRoot());
		Root->FlushSync();
		SWidget& RootWidget = Root->GetWidget().Get();
		TestTrue(TEXT("starts on Hello World"), DemoTest::ContainsText(RootWidget, TEXT("Hello, world!")));

		SButton* TicTacToe = DemoTest::FindButton(RootWidget, TEXT("Tic Tac Toe"));
		if (TestNotNull(TEXT("found the Tic Tac Toe menu button"), TicTacToe))
		{
			TicTacToe->SimulateClick();
			Root->FlushSync();
			TestTrue(TEXT("switched to Tic Tac Toe"),
					 DemoTest::ContainsText(RootWidget, TEXT("Welcome to Tic Tac Toe!")));
			TestFalse(TEXT("Hello World unmounted"), DemoTest::ContainsText(RootWidget, TEXT("Hello, world!")));

			SButton* Start = DemoTest::FindButton(RootWidget, TEXT("Start Game"));
			if (TestNotNull(TEXT("found Start Game"), Start))
			{
				Start->SimulateClick();
				Root->FlushSync();
				TestTrue(TEXT("game board appeared"), DemoTest::ContainsText(RootWidget, TEXT("Player turn: X")));

				// A move must MARK the board (owner playtest regression: marks not visible).
				SButton* EmptyCell = DemoTest::FindButton(RootWidget, TEXT(" "), /*bExact*/ true);
				if (TestNotNull(TEXT("found an empty cell"), EmptyCell))
				{
					EmptyCell->SimulateClick();
					Root->FlushSync();
					TestTrue(TEXT("the X mark rendered on the board"),
							 DemoTest::ContainsText(RootWidget, TEXT("X")) &&
								 DemoTest::ContainsText(RootWidget, TEXT("Player turn: O")));
				}
			}
		}
		Root->Unmount();
		TestEqual(TEXT("gallery unmounts to zero fibers"), Root->GetReconciler().NumLiveFibers(), 0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
