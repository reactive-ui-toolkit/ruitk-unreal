// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Ruitk.Slate.DragDrop — TD-004 drag-and-drop half: the FRuitkDragDropOp payload carrier, the
// SRuitkDropTarget accept/reject + hover + drop-fire logic, and the SRuitkDragSource begin-drag path.
// Slate DnD overrides are directly callable, so the suite drives a synthetic drop (a hand-built
// FDragDropEvent over our op) without a live Slate drag loop.

#include "Misc/AutomationTest.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/DragAndDrop.h"
#include "Input/Events.h"
#include "RuitkDragDrop.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DragDropTest
{
	static int32 GDropped = -99;
	static int32 GEntered = -99;
	static bool GLeft = false;
	static int32 GDragStarted = -99;
	static int32 GDragEnded = -1; // -1 unset, 0 = not handled, 1 = handled
} // namespace DragDropTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkDragDropTest, "Ruitk.Slate.DragDrop",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkDragDropTest::RunTest(const FString&)
{
	using namespace Ruitk::Slate;
	using namespace DragDropTest;

	if (!FSlateApplication::IsInitialized())
	{
		AddInfo(TEXT("[dragdrop] Slate not initialized — operation construction needs it; skipped."));
		return true;
	}

	// ── the operation carries payload + type; OnEnded fires once with the handled flag ─────────
	{
		GDragEnded = -1;
		int32 EndCalls = 0;
		TSharedRef<FRuitkDragDropOp> Op = FRuitkDragDropOp::New(FName(TEXT("card")), FRuitkValue(7),
																[&EndCalls](bool bHandled)
																{
																	++EndCalls;
																	GDragEnded = bHandled ? 1 : 0;
																});
		TestEqual(TEXT("op carries the type"), Op->DragType, FName(TEXT("card")));
		TestEqual(TEXT("op carries the payload"), static_cast<int32>(Op->Payload.IntValue), 7);

		FPointerEvent Pointer;
		Op->OnDrop(true, Pointer);
		Op->OnDrop(true, Pointer); // second call must NOT re-fire
		TestEqual(TEXT("OnEnded fires exactly once"), EndCalls, 1);
		TestEqual(TEXT("OnEnded saw the handled flag"), GDragEnded, 1);
	}

	// ── drop target: accepts a matching type, fires OnDrop with the payload ─────────────────────
	{
		GDropped = -99;
		TSharedRef<SRuitkDropTarget> Target = SNew(SRuitkDropTarget);
		Target->SetAcceptTypes({FName(TEXT("card"))});
		Target->SetOnDrop(
			FRuitkCallback::Create([](const FRuitkValue& V) { GDropped = static_cast<int32>(V.IntValue); }));

		FPointerEvent Pointer;
		FDragDropEvent Accepted(Pointer, FRuitkDragDropOp::New(FName(TEXT("card")), FRuitkValue(42)));
		const FReply R = Target->OnDrop(FGeometry(), Accepted);
		TestTrue(TEXT("accepted drop is handled"), R.IsEventHandled());
		TestEqual(TEXT("OnDrop received the payload"), GDropped, 42);
	}

	// ── drop target: rejects a non-accepted type (no fire, unhandled) ───────────────────────────
	{
		GDropped = -99;
		TSharedRef<SRuitkDropTarget> Target = SNew(SRuitkDropTarget);
		Target->SetAcceptTypes({FName(TEXT("card"))});
		Target->SetOnDrop(
			FRuitkCallback::Create([](const FRuitkValue& V) { GDropped = static_cast<int32>(V.IntValue); }));

		FPointerEvent Pointer;
		FDragDropEvent Rejected(Pointer, FRuitkDragDropOp::New(FName(TEXT("token")), FRuitkValue(99)));
		const FReply R = Target->OnDrop(FGeometry(), Rejected);
		TestFalse(TEXT("rejected drop is unhandled"), R.IsEventHandled());
		TestEqual(TEXT("OnDrop did not fire for a rejected type"), GDropped, -99);
	}

	// ── drop target: empty AcceptTypes accepts anything; enter/leave toggle hover ───────────────
	{
		GEntered = -99;
		GLeft = false;
		TSharedRef<SRuitkDropTarget> Target = SNew(SRuitkDropTarget);
		// no SetAcceptTypes -> accept any
		Target->SetOnDragEnter(
			FRuitkCallback::Create([](const FRuitkValue& V) { GEntered = static_cast<int32>(V.IntValue); }));
		Target->SetOnDragLeave(FRuitkCallback::Create([](const FRuitkValue&) { GLeft = true; }));

		FPointerEvent Pointer;
		FDragDropEvent Enter(Pointer, FRuitkDragDropOp::New(FName(TEXT("anything")), FRuitkValue(5)));
		Target->OnDragEnter(FGeometry(), Enter);
		TestTrue(TEXT("hover is set on enter"), Target->IsOver());
		TestEqual(TEXT("OnDragEnter received the payload"), GEntered, 5);

		Target->OnDragLeave(Enter);
		TestFalse(TEXT("hover clears on leave"), Target->IsOver());
		TestTrue(TEXT("OnDragLeave fired"), GLeft);
	}

	// ── drag source: OnDragDetected begins the operation and fires OnDragStart with the payload ──
	{
		GDragStarted = -99;
		TSharedRef<SRuitkDragSource> Source = SNew(SRuitkDragSource);
		Source->SetDragType(FName(TEXT("card")));
		Source->SetPayload(FRuitkValue(123));
		Source->SetOnDragStart(
			FRuitkCallback::Create([](const FRuitkValue& V) { GDragStarted = static_cast<int32>(V.IntValue); }));

		FPointerEvent Pointer;
		const FReply R = Source->OnDragDetected(FGeometry(), Pointer);
		TestTrue(TEXT("drag-detected is handled"), R.IsEventHandled());
		TestEqual(TEXT("OnDragStart fired with the payload"), GDragStarted, 123);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
