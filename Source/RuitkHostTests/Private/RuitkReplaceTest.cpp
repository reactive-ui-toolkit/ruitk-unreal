// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Ruitk.Slate.Replace — TD-011: a construct-only prop change REPLACES the widget in place
// (rebuild + re-parent children + swap into the parent slot + keep the event proxy), instead of
// the old warn-and-ignore. No shipped widget carries a reconstruct mask yet, so this drives the
// host directly with a minimal test adapter whose `Flavor` prop is construct-only.

#include "Misc/AutomationTest.h"
#include "RuitkElementAdapter.h"
#include "RuitkEventProxy.h"
#include "RuitkPropsBase.h"
#include "RuitkSlateHost.h"
#include "Widgets/SBoxPanel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ReplaceTest
{
	// A construct-only `Flavor` (mask bit 0) + a plain runtime `Rev` (bit 1).
	struct FTestReconProps : public FRuitkPropsBase
	{
		RUITK_PROP(FName, Flavor, 0)
		RUITK_PROP(int32, Rev, 1)
		RUITK_PROPS_BODY(FTestReconProps, RUITK_EQ(Flavor) RUITK_EQ(Rev))
	};

	// MultiSlot adapter over SVerticalBox; Flavor is construct-only, so a Flavor change forces a
	// rebuild. HasEvents() so the host mints a proxy we can prove is REUSED across the swap.
	class FTestReconAdapter : public IRuitkElementAdapter
	{
	public:
		int32 CreateCount = 0;

		virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::MultiSlot; }
		virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
		{
			++CreateCount;
			return SNew(SVerticalBox);
		}
		virtual void ApplyDiff(SWidget&, const FRuitkPropsBase*, const FRuitkPropsBase&) override {}
		virtual uint64 GetReconstructMask() const override { return (1ull << FTestReconProps::Flavor_Bit); }
		virtual bool ConstructOnlyChanged(const FRuitkPropsBase& Old, const FRuitkPropsBase& New) const override
		{
			// Has-gated like production adapters (SEP-REBUILD-1; enforced by Contract.AdapterMasks).
			const FTestReconProps& O = static_cast<const FTestReconProps&>(Old);
			const FTestReconProps& N = static_cast<const FTestReconProps&>(New);
			return N.HasFlavor() && (!O.HasFlavor() || O.Flavor != N.Flavor);
		}
		virtual bool HasEvents() const override { return true; }
		virtual void SyncEventHandlers(FRuitkEventProxy&, const FRuitkPropsBase&) override {}

		virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32 Index,
								 const FRuitkStyleDict*) override
		{
			SVerticalBox& VB = static_cast<SVerticalBox&>(Parent);
			if (Index < 0 || Index >= VB.NumSlots())
			{
				VB.AddSlot()[Child];
			}
			else
			{
				VB.InsertSlot(Index)[Child];
			}
		}
		virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) override
		{
			static_cast<SVerticalBox&>(Parent).RemoveSlot(Child);
		}
	};
} // namespace ReplaceTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuitkReplaceTest, "Ruitk.Slate.Replace",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRuitkReplaceTest::RunTest(const FString&)
{
	using namespace ReplaceTest;

	FRuitkSlateHost Host;

	// The adapters (one instance each so CreateCount is observable per role).
	auto RootAdapterOwned = MakeUnique<FTestReconAdapter>();
	auto NodeAdapterOwned = MakeUnique<FTestReconAdapter>();
	FTestReconAdapter* NodeAdapter = NodeAdapterOwned.Get();
	const FRuitkElementTypeId RootType =
		Ruitk::Slate::RegisterAdapter(FName(TEXT("TestReconRoot")), MoveTemp(RootAdapterOwned));
	const FRuitkElementTypeId NodeType =
		Ruitk::Slate::RegisterAdapter(FName(TEXT("TestRecon")), MoveTemp(NodeAdapterOwned));

	// Parent panel (mount surface) + the reconstruct node under it.
	TSharedRef<SVerticalBox> RootPanel = SNew(SVerticalBox);
	const FRuitkHostHandle ParentH = Host.WrapExternalPanel(RootPanel, RootType);

	FTestReconProps P0;
	P0.SetFlavor(FName(TEXT("A")));
	P0.SetRev(1);
	const FRuitkHostHandle NodeH = Host.CreateInstance(NodeType, P0);
	Host.InsertChild(ParentH, NodeH, -1);

	// Two children under the reconstruct node.
	FTestReconProps CP;
	const FRuitkHostHandle ChildA = Host.CreateInstance(NodeType, CP);
	const FRuitkHostHandle ChildB = Host.CreateInstance(NodeType, CP);
	Host.InsertChild(NodeH, ChildA, -1);
	Host.InsertChild(NodeH, ChildB, -1);

	FRuitkSlateNode* Node = FRuitkSlateHost::Resolve(NodeH);
	TestNotNull(TEXT("node resolves"), Node);
	SWidget* OldWidget = Node->Widget.Get();
	FRuitkEventProxy* OldProxy = Node->Proxy.Get();
	SWidget* ChildAW = FRuitkSlateHost::Resolve(ChildA)->Widget.Get();
	SWidget* ChildBW = FRuitkSlateHost::Resolve(ChildB)->Widget.Get();
	TestEqual(TEXT("node has 2 children before"), static_cast<SVerticalBox*>(OldWidget)->NumSlots(), 2);
	const int32 CreatesBefore = NodeAdapter->CreateCount;

	// ── construct-only change (Flavor A->B): the widget is REPLACED ───────────────────────────
	{
		FTestReconProps P1;
		P1.SetFlavor(FName(TEXT("B")));
		P1.SetRev(1);
		Host.CommitUpdate(NodeH, NodeType, &P0, P1);

		TestTrue(TEXT("a NEW widget was constructed"), NodeAdapter->CreateCount == CreatesBefore + 1);
		TestNotEqual(TEXT("widget pointer changed (replacement)"), (void*)Node->Widget.Get(), (void*)OldWidget);
		TestEqual(TEXT("event proxy REUSED (bind-once-swap-inner)"), (void*)Node->Proxy.Get(), (void*)OldProxy);

		SVerticalBox* NewVB = static_cast<SVerticalBox*>(Node->Widget.Get());
		FChildren* NewKids = NewVB->GetChildren();
		TestEqual(TEXT("both children re-parented into the new widget"), NewKids->Num(), 2);
		TestEqual(TEXT("child A identity preserved"), (void*)&NewKids->GetChildAt(0).Get(), (void*)ChildAW);
		TestEqual(TEXT("child B identity preserved"), (void*)&NewKids->GetChildAt(1).Get(), (void*)ChildBW);
		// the children's parent link still points at the same node
		TestEqual(TEXT("child A still parented to the node"),
				  (void*)FRuitkSlateHost::Resolve(ChildA)->ParentNode.Pin().Get(), (void*)Node);
	}

	// ── runtime-only change (Rev 1->2, Flavor unchanged): NO replacement ─────────────────────
	{
		SWidget* StableWidget = Node->Widget.Get();
		const int32 CreatesNow = NodeAdapter->CreateCount;
		FTestReconProps P1;
		P1.SetFlavor(FName(TEXT("B")));
		P1.SetRev(1);
		FTestReconProps P2;
		P2.SetFlavor(FName(TEXT("B")));
		P2.SetRev(2);
		Host.CommitUpdate(NodeH, NodeType, &P1, P2);

		TestEqual(TEXT("no new widget for a runtime-only change"), NodeAdapter->CreateCount, CreatesNow);
		TestEqual(TEXT("widget pointer stable (no replacement)"), (void*)Node->Widget.Get(), (void*)StableWidget);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
