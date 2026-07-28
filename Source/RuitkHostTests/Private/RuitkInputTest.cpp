// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Ruitk.Slate.Shortcut — TD-004 keyboard-shortcut half: the FRuitkShortcut chord matcher and
// the UseShortcut hook lifecycle (register on mount → fire on the chord → unregister on unmount).
// Ruitk.Slate.Focus — TD-022 focus extensions: UseFocus ref/Focus()/IsFocused() round-trip.

#include "Misc/AutomationTest.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "RuitkContext.h"
#include "RuitkCoreElements.h"
#include "RuitkInput.h"
#include "RuitkNode.h"
#include "RuitkRoot.h"
#include "RuitkSlateElements.h"
#include "RuitkSlateHost.h"
#include "Widgets/SWindow.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ShortcutTest
{
	static int32 GFires = 0;

	static FRuitkNodeArray ShortcutComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
	{
		Ruitk::Slate::FRuitkShortcut Chord;
		Chord.Key = EKeys::S;
		Chord.bCtrl = true;
		Ruitk::Slate::UseShortcut(Ctx, Chord, []() { ++GFires; });
		return {Ruitk::TextBlock(FString(TEXT("shortcut host")))};
	}
	RUITK_COMPONENT(ShortcutComp)

	static FKeyEvent MakeKey(FKey Key, bool bCtrl)
	{
		const FModifierKeysState Mods(false, false, bCtrl, false, false, false, false, false, false);
		return FKeyEvent(Key, Mods, 0, /*bIsRepeat*/ false, 0, 0);
	}
} // namespace ShortcutTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkShortcutTest, "Ruitk.Slate.Shortcut",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkShortcutTest::RunTest(const FString&)
{
	using namespace Ruitk::Slate;

	// ── the chord matcher (pure) ──────────────────────────────────────────────────────────────
	FRuitkShortcut CtrlS;
	CtrlS.Key = EKeys::S;
	CtrlS.bCtrl = true;
	TestTrue(TEXT("Ctrl+S matches the Ctrl+S chord"), CtrlS.Matches(ShortcutTest::MakeKey(EKeys::S, true)));
	TestFalse(TEXT("S alone does NOT match Ctrl+S"), CtrlS.Matches(ShortcutTest::MakeKey(EKeys::S, false)));
	TestFalse(TEXT("Ctrl+A does NOT match Ctrl+S"), CtrlS.Matches(ShortcutTest::MakeKey(EKeys::A, true)));

	FRuitkShortcut ShiftS;
	ShiftS.Key = EKeys::S;
	ShiftS.bShift = true;
	TestNotEqual(TEXT("different chords have different dep keys"), CtrlS.DepKey(), ShiftS.DepKey());

	// ── the hook: register on mount, fire on the chord, unregister on unmount ──────────────────
	if (FSlateApplication::IsInitialized())
	{
		ShortcutTest::GFires = 0;
		TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(Ruitk::FC(&ShortcutTest::ShortcutComp));
		Root->FlushSync(); // commit + run effects (registers the pre-processor)

		FSlateApplication::Get().ProcessKeyDownEvent(ShortcutTest::MakeKey(EKeys::S, true));
		TestEqual(TEXT("Ctrl+S fired the shortcut while mounted"), ShortcutTest::GFires, 1);

		FSlateApplication::Get().ProcessKeyDownEvent(ShortcutTest::MakeKey(EKeys::A, true));
		TestEqual(TEXT("a non-matching chord does not fire"), ShortcutTest::GFires, 1);

		Root->Unmount(); // cleanup runs → unregisters the pre-processor
		FSlateApplication::Get().ProcessKeyDownEvent(ShortcutTest::MakeKey(EKeys::S, true));
		TestEqual(TEXT("the shortcut no longer fires after unmount"), ShortcutTest::GFires, 1);
	}

	return true;
}

// ── TD-022 focus extensions: UseFocus ─────────────────────────────────────────────────────

namespace FocusTest
{
	static Ruitk::Slate::FRuitkFocusHandle GFocus;
	static FRuitkHostHandle GHandle; // the mounted node, captured for the test to resolve the SWidget

	static FRuitkNodeArray FocusComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
	{
		Ruitk::Slate::FRuitkFocusHandle Focus = Ruitk::Slate::UseFocus(Ctx);
		GFocus = Focus;
		// A Button focuses directly (unlike SEditableTextBox, which delegates to an inner
		// SEditableText — so GetUserFocusedWidget would not match the box the ref captured).
		FRuitkButtonProps Props;
		auto RefFn = Focus.Ref;
		Props.Ref = [RefFn](const FRuitkHostHandle& H)
		{
			GHandle = H;
			if (RefFn)
			{
				RefFn(H);
			}
		};
		return {Ruitk::Slate::Button(MoveTemp(Props), {Ruitk::TextBlock(TEXT("focus me"))})};
	}
	RUITK_COMPONENT(FocusComp)
} // namespace FocusTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkFocusTest, "Ruitk.Slate.Focus",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkFocusTest::RunTest(const FString&)
{
	using namespace FocusTest;

	// The handle is always fully bound, even headless.
	{
		TSharedRef<FRuitkRoot> Root = FRuitkRoot::Create(Ruitk::FC(&FocusComp));
		Root->FlushSync();
		TestTrue(TEXT("Ref bound"), (bool)GFocus.Ref);
		TestTrue(TEXT("Focus bound"), (bool)GFocus.Focus);
		TestTrue(TEXT("IsFocused bound"), (bool)GFocus.IsFocused);
		GFocus.Focus(); // no crash detached / headless
		TestFalse(TEXT("not focused while detached"), GFocus.IsFocused());
	}

	// End-to-end through a real window: Focus() moves keyboard focus to the ref'd widget.
	if (FSlateApplication::IsInitialized())
	{
		TSharedRef<SWindow> Window = SNew(SWindow).ClientSize(FVector2D(200, 120));
		FSlateApplication::Get().AddWindow(Window, /*bShowImmediately*/ false);
		TSharedRef<FRuitkRoot> Root = FRuitkRoot::CreateInWindow(Window, Ruitk::FC(&FocusComp));
		Root->FlushSync();
		Window->SlatePrepass(1.0f);
		FSlateApplication& App = FSlateApplication::Get();

		// The exact widget the ref captured (resolved from the node the ref stored).
		FRuitkSlateNode* Node = FRuitkSlateHost::Resolve(GHandle);
		if (TestNotNull(TEXT("ref stored the mounted node"), Node) &&
			TestTrue(TEXT("node has a widget"), Node->Widget.IsValid()))
		{
			TSharedRef<SWidget> Button = Node->Widget.ToSharedRef();

			// Verify IsFocused tracks Slate: drive focus directly, then read it back through the handle.
			const bool bSet = App.SetUserFocus(FSlateApplication::CursorUserIndex, Button, EFocusCause::SetDirectly);
			if (bSet)
			{
				TestTrue(TEXT("IsFocused true after Slate focus"), GFocus.IsFocused());

				App.ClearUserFocus(FSlateApplication::CursorUserIndex, EFocusCause::SetDirectly);
				TestFalse(TEXT("IsFocused false after clear"), GFocus.IsFocused());

				// The handle's own Focus() must target the SAME widget.
				GFocus.Focus();
				TestTrue(TEXT("handle Focus() focused the ref'd widget"),
						 App.GetUserFocusedWidget(FSlateApplication::CursorUserIndex) == Button);

				Ruitk::Slate::ClearFocus();
				TestFalse(TEXT("ClearFocus cleared it"), GFocus.IsFocused());
			}
			else
			{
				AddInfo(TEXT("[focus] headless Slate refused focus — handle plumbing verified, "
							 "focus round-trip skipped"));
			}
		}

		Root->Unmount();
		FSlateApplication::Get().RequestDestroyWindow(Window);
	}

	GFocus = Ruitk::Slate::FRuitkFocusHandle();
	GHandle.Reset();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
