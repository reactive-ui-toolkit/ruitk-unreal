// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuitkSlateHost.h"

#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericApplication.h"
#include "RuitkCoreElements.h"
#include "RuitkCoreMisc.h"
#include "RuitkSlateLog.h"
#include "RuitkStyle.h"
#include "Widgets/SNullWidget.h"

// ─────────────────────────────────────────────────────────────────────────────────────────
// Adapter registry
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace
{
	FCriticalSection GAdapterLock;
	TMap<FRuitkElementTypeId, TUniquePtr<IRuitkElementAdapter>> GAdapters;
} // namespace

namespace Ruitk::Slate
{
	void RegisterAdapter(FRuitkElementTypeId Type, TUniquePtr<IRuitkElementAdapter> Adapter)
	{
		checkf(Type.IsValid(), TEXT("ReactiveUI: cannot register an adapter for the invalid element type"));
		FScopeLock Lock(&GAdapterLock);
		GAdapters.Add(Type, MoveTemp(Adapter)); // replace on re-registration (Live Coding)
	}

	FRuitkElementTypeId RegisterAdapter(FName TagName, TUniquePtr<IRuitkElementAdapter> Adapter)
	{
		const FRuitkElementTypeId Type = Ruitk::InternElementType(TagName);
		RegisterAdapter(Type, MoveTemp(Adapter));
		return Type;
	}

	IRuitkElementAdapter* FindAdapter(FRuitkElementTypeId Type)
	{
		FScopeLock Lock(&GAdapterLock);
		const TUniquePtr<IRuitkElementAdapter>* Found = GAdapters.Find(Type);
		return Found ? Found->Get() : nullptr;
	}

	void ForEachAdapter(const TFunctionRef<void(FRuitkElementTypeId, IRuitkElementAdapter&)> Visit)
	{
		FScopeLock Lock(&GAdapterLock);
		for (const TPair<FRuitkElementTypeId, TUniquePtr<IRuitkElementAdapter>>& Pair : GAdapters)
		{
			Visit(Pair.Key, *Pair.Value);
		}
	}
} // namespace Ruitk::Slate

// ─────────────────────────────────────────────────────────────────────────────────────────
// FRuitkSlateHost
// ─────────────────────────────────────────────────────────────────────────────────────────

FRuitkSlateHost::FRuitkSlateHost() = default;

FRuitkSlateHost::~FRuitkSlateHost()
{
	if (bPreTickRegistered && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnPreTick().Remove(PreTickHandle);
	}
	// Queued frames die with the host (the mount surface tears reconciler + host down
	// together; the reconciler's Tick self-guards anyway).
	FrameQueue.Reset();
}

FRuitkHostHandle FRuitkSlateHost::WrapExternalPanel(const TSharedRef<SWidget>& Panel, FRuitkElementTypeId PanelType)
{
	IRuitkElementAdapter* Adapter = Ruitk::Slate::FindAdapter(PanelType);
	checkf(Adapter != nullptr, TEXT("ReactiveUI: no adapter registered for the mount panel type"));
	TSharedRef<FRuitkSlateNode> Node = MakeShared<FRuitkSlateNode>();
	Node->Widget = Panel;
	Node->Adapter = Adapter;
	Node->Type = PanelType;
	return Node;
}

FRuitkHostHandle FRuitkSlateHost::CreateInstance(FRuitkElementTypeId Type, const FRuitkPropsBase& Props)
{
	IRuitkElementAdapter* Adapter = Ruitk::Slate::FindAdapter(Type);
	if (Adapter == nullptr)
	{
		UE_LOG(LogRuitkSlate, Error, TEXT("[ReactiveUI] no adapter registered for element '%s' — rendering a null slot"),
			   *Ruitk::GetElementTypeName(Type).ToString());
		TSharedRef<FRuitkSlateNode> Null = MakeShared<FRuitkSlateNode>();
		Null->Widget = SNullWidget::NullWidget;
		Null->Type = Type;
		return Null;
	}

	TSharedRef<FRuitkSlateNode> Node = MakeShared<FRuitkSlateNode>();
	Node->Adapter = Adapter;
	Node->Type = Type;

	// GO-05 pool hit: reuse the detached widget, diff against its stashed last props.
	const FRuitkPropsBase* DiffBase = nullptr;
	TSharedPtr<const FRuitkPropsBase> StashedProps; // keeps DiffBase alive through ApplyDiff
	TSharedPtr<FRuitkStyleDict> StashedStyle;
	if (TArray<FPoolEntry>* Bucket = Pool.Find(Type); Bucket != nullptr && !Bucket->IsEmpty())
	{
		FPoolEntry Entry = Bucket->Pop(EAllowShrinking::No);
		Node->Widget = Entry.Widget;
		StashedProps = Entry.LastProps;
		StashedStyle = Entry.AppliedStyle;
		DiffBase = StashedProps.Get();
	}
	else
	{
		if (Adapter->HasEvents())
		{
			Node->Proxy = MakeShared<FRuitkEventProxy>(); // before CreateWidget: SLATE_EVENT args bind there
		}
		Node->Widget = Adapter->CreateWidget(Props, Node->Proxy);
	}

	Adapter->ApplyDiff(*Node->Widget, DiffBase, Props); // full apply / diff-on-reuse — one code path
	if (Node->Proxy.IsValid())
	{
		Adapter->SyncEventHandlers(*Node->Proxy, Props);
	}
	TSharedPtr<FRuitkStyleDict> Effective = Ruitk::Slate::BuildEffectiveStyle(Props.Classes, Props.Style);
	if (Effective.IsValid() || StashedStyle.IsValid())
	{
		Ruitk::Slate::ApplyStyleDiff(*Node->Widget, Adapter, StashedStyle.Get(), Effective.Get());
	}
	Node->AppliedStyle = Effective;
	if (Props.SlotProps.IsValid())
	{
		Node->SlotProps = MakeShared<FRuitkStyleDict>(*Props.SlotProps);
	}
	return Node;
}

void FRuitkSlateHost::CommitUpdate(const FRuitkHostHandle& Handle, FRuitkElementTypeId Type, const FRuitkPropsBase* OldProps,
								 const FRuitkPropsBase& NewProps)
{
	FRuitkSlateNode* Node = Resolve(Handle);
	if (Node == nullptr || !Node->Widget.IsValid() || Node->Adapter == nullptr)
	{
		return;
	}

	// TD-011: a construct-only prop that actually CHANGED VALUE requires rebuilding the widget.
	// The mask marks the bits; ConstructOnlyChanged is the precise trigger (a mask bit set on both
	// sides with an unchanged value must not force a rebuild). ReplaceWidget rebuilds + re-parents
	// children + swaps into the parent slot, then re-applies props/style/events — so we are DONE.
	const uint64 Mask = Node->Adapter->GetReconstructMask();
	if (Mask != 0 && OldProps != nullptr)
	{
		const uint64 TouchedBits = (OldProps->SetBits | NewProps.SetBits) & Mask;
		if (TouchedBits != 0 && !NewProps.Equals(*OldProps) && Node->Adapter->ConstructOnlyChanged(*OldProps, NewProps))
		{
			ReplaceWidget(*Node, OldProps, NewProps);
			return;
		}
	}

	Node->Adapter->ApplyDiff(*Node->Widget, OldProps, NewProps);
	if (Node->Proxy.IsValid())
	{
		Node->Adapter->SyncEventHandlers(*Node->Proxy, NewProps);
	}

	// Style layer (D-13): diff the EFFECTIVE dict against what this node last applied —
	// removed keys reset, and a style-only tweak never touches the widget's structure.
	TSharedPtr<FRuitkStyleDict> Effective = Ruitk::Slate::BuildEffectiveStyle(NewProps.Classes, NewProps.Style);
	const bool bStyleSame = (!Effective.IsValid() && !Node->AppliedStyle.IsValid()) ||
							(Effective.IsValid() && Node->AppliedStyle.IsValid() &&
							 Effective->OrderIndependentCompareEqual(*Node->AppliedStyle));
	if (!bStyleSame)
	{
		Ruitk::Slate::ApplyStyleDiff(*Node->Widget, Node->Adapter, Node->AppliedStyle.Get(), Effective.Get());
		Node->AppliedStyle = Effective;
	}

	// slot.* changes re-apply through the parent's adapter.
	const bool bOldHasSlot = OldProps != nullptr && OldProps->SlotProps.IsValid();
	const bool bNewHasSlot = NewProps.SlotProps.IsValid();
	const bool bSlotChanged =
		(bOldHasSlot != bNewHasSlot) ||
		(bNewHasSlot && bOldHasSlot && !NewProps.SlotProps->OrderIndependentCompareEqual(*OldProps->SlotProps));
	if (bSlotChanged)
	{
		Node->SlotProps =
			bNewHasSlot ? TSharedPtr<FRuitkStyleDict>(MakeShared<FRuitkStyleDict>(*NewProps.SlotProps)) : nullptr;
		if (TSharedPtr<FRuitkSlateNode> Parent = Node->ParentNode.Pin())
		{
			if (Parent->Widget.IsValid() && Parent->Adapter != nullptr)
			{
				Parent->Adapter->UpdateChildSlotProps(*Parent->Widget, Node->Widget.ToSharedRef(),
													  Node->SlotProps.Get());
			}
		}
	}
}

void FRuitkSlateHost::ReleaseInstance(const FRuitkHostHandle& Handle, FRuitkElementTypeId Type,
									const TSharedPtr<const FRuitkPropsBase>& LastProps, bool bWasChildless)
{
	FRuitkSlateNode* Node = Resolve(Handle);
	if (Node == nullptr)
	{
		return;
	}
	// Detach if the reconciler's deletion path didn't already (defensive — Release promises
	// the node is done).
	if (TSharedPtr<FRuitkSlateNode> Parent = Node->ParentNode.Pin())
	{
		if (Parent->Widget.IsValid() && Parent->Adapter != nullptr && Node->Widget.IsValid())
		{
			RemoveChildFromParent(*Parent, Node->Widget.ToSharedRef());
		}
	}
	Node->ParentNode.Reset();
	if (Node->Proxy.IsValid())
	{
		Node->Proxy->ClearAll(); // user closures must not outlive the node
	}

	// GO-05: stash detached event-less childless leaves for diff-on-reuse.
	if (FRuitkConfig::IsHostNodePoolEnabled() && bWasChildless && Node->Widget.IsValid() && Node->Adapter != nullptr &&
		Node->Adapter->IsPoolable())
	{
		TArray<FPoolEntry>& Bucket = Pool.FindOrAdd(Type);
		if (Bucket.Num() < PoolCapPerType)
		{
			FPoolEntry& Entry = Bucket.AddDefaulted_GetRef();
			Entry.Widget = Node->Widget;
			Entry.LastProps = LastProps;
			Entry.AppliedStyle = Node->AppliedStyle;
		}
	}
	Node->Widget.Reset();
}

void FRuitkSlateHost::OnBeforeCommit()
{
	FocusedBeforeCommit.Reset();
	if (FSlateApplication::IsInitialized())
	{
		// Capture the focused widget (cursor user); restore after mutations if the commit's
		// detach/attach churn stole it but the widget survived (critique gap 6).
		FocusedBeforeCommit = FSlateApplication::Get().GetUserFocusedWidget(FSlateApplication::CursorUserIndex);
	}
}

void FRuitkSlateHost::OnAfterCommit()
{
	TSharedPtr<SWidget> Focused = FocusedBeforeCommit.Pin();
	FocusedBeforeCommit.Reset();
	if (!Focused.IsValid() || !FSlateApplication::IsInitialized())
	{
		return;
	}
	FSlateApplication& App = FSlateApplication::Get();
	if (App.GetUserFocusedWidget(FSlateApplication::CursorUserIndex) != Focused)
	{
		App.SetUserFocus(FSlateApplication::CursorUserIndex, Focused, EFocusCause::SetDirectly);
	}
}

int32 FRuitkSlateHost::NumPooled(FRuitkElementTypeId Type) const
{
	const TArray<FPoolEntry>* Bucket = Pool.Find(Type);
	return Bucket != nullptr ? Bucket->Num() : 0;
}

void FRuitkSlateHost::InsertChild(const FRuitkHostHandle& ParentHandle, const FRuitkHostHandle& ChildHandle, int32 Index)
{
	FRuitkSlateNode* Parent = Resolve(ParentHandle);
	FRuitkSlateNode* Child = Resolve(ChildHandle);
	if (Parent == nullptr || Child == nullptr || !Parent->Widget.IsValid() || !Child->Widget.IsValid() ||
		Parent->Adapter == nullptr)
	{
		return;
	}
	const TSharedRef<SWidget> ChildWidget = Child->Widget.ToSharedRef();
	switch (Parent->Adapter->GetChildKind())
	{
	case ERuitkChildKind::MultiSlot:
		Parent->Adapter->InsertChild(*Parent->Widget, ChildWidget, Index, Child->SlotProps.Get());
		break;
	case ERuitkChildKind::SingleContent:
	{
		TSharedPtr<SWidget> Existing = Parent->ContentChild.Pin();
		if (Existing.IsValid() && Existing != Child->Widget && !Parent->bWarnedCapacity)
		{
			Parent->bWarnedCapacity = true;
			UE_LOG(LogRuitkSlate, Warning,
				   TEXT("[ReactiveUI] '%s' takes ONE child — extra children replace the content (last wins)"),
				   *Ruitk::GetElementTypeName(Parent->Type).ToString());
		}
		Parent->Adapter->SetContent(*Parent->Widget, Child->Widget);
		Parent->ContentChild = Child->Widget;
		break;
	}
	case ERuitkChildKind::Leaf:
		UE_LOG(LogRuitkSlate, Warning, TEXT("[ReactiveUI] '%s' is a leaf — child '%s' dropped"),
			   *Ruitk::GetElementTypeName(Parent->Type).ToString(), *Ruitk::GetElementTypeName(Child->Type).ToString());
		return;
	}
	Child->ParentNode = StaticCastSharedPtr<FRuitkSlateNode>(ParentHandle);

	// TD-011 bookkeeping: mirror the MultiSlot child order so a construct-only REPLACEMENT can
	// re-parent children (with slot props) into the rebuilt widget. Dedupe (a re-insert moves it).
	if (Parent->Adapter->GetChildKind() == ERuitkChildKind::MultiSlot)
	{
		TSharedPtr<FRuitkSlateNode> ChildShared = StaticCastSharedPtr<FRuitkSlateNode>(ChildHandle);
		Parent->ChildNodes.RemoveAll([&ChildShared](const TWeakPtr<FRuitkSlateNode>& W)
									 { return !W.IsValid() || W.Pin() == ChildShared; });
		const int32 At = (Index < 0 || Index > Parent->ChildNodes.Num()) ? Parent->ChildNodes.Num() : Index;
		Parent->ChildNodes.Insert(ChildShared, At);
	}
}

void FRuitkSlateHost::RemoveChild(const FRuitkHostHandle& ParentHandle, const FRuitkHostHandle& ChildHandle)
{
	FRuitkSlateNode* Parent = Resolve(ParentHandle);
	FRuitkSlateNode* Child = Resolve(ChildHandle);
	if (Parent == nullptr || Child == nullptr || !Parent->Widget.IsValid() || !Child->Widget.IsValid() ||
		Parent->Adapter == nullptr)
	{
		return;
	}
	RemoveChildFromParent(*Parent, Child->Widget.ToSharedRef());
	Child->ParentNode.Reset();
	Parent->ChildNodes.RemoveAll(
		[Child](const TWeakPtr<FRuitkSlateNode>& W)
		{
			TSharedPtr<FRuitkSlateNode> P = W.Pin();
			return !P.IsValid() || P.Get() == Child;
		});
}

void FRuitkSlateHost::ReorderChildren(const FRuitkHostHandle& ParentHandle, const TArray<FRuitkHostHandle>& Ordered)
{
	FRuitkSlateNode* Parent = Resolve(ParentHandle);
	if (Parent == nullptr || !Parent->Widget.IsValid() || Parent->Adapter == nullptr ||
		Parent->Adapter->GetChildKind() != ERuitkChildKind::MultiSlot)
	{
		return;
	}
	TArray<TSharedRef<SWidget>> Widgets;
	Widgets.Reserve(Ordered.Num());
	TMap<SWidget*, const FRuitkStyleDict*> SlotPropsByWidget;
	SlotPropsByWidget.Reserve(Ordered.Num());
	for (const FRuitkHostHandle& H : Ordered)
	{
		if (FRuitkSlateNode* N = Resolve(H); N != nullptr && N->Widget.IsValid())
		{
			TSharedRef<SWidget> W = N->Widget.ToSharedRef();
			Widgets.Add(W);
			SlotPropsByWidget.Add(&W.Get(), N->SlotProps.Get());
		}
	}
	Parent->Adapter->ReorderChildren(*Parent->Widget, Widgets,
									 [&SlotPropsByWidget](const TSharedRef<SWidget>& W) -> const FRuitkStyleDict*
									 { return SlotPropsByWidget.FindRef(&W.Get()); });

	// TD-011 bookkeeping: adopt the exact new order.
	Parent->ChildNodes.Reset(Ordered.Num());
	for (const FRuitkHostHandle& H : Ordered)
	{
		if (TSharedPtr<FRuitkSlateNode> N = StaticCastSharedPtr<FRuitkSlateNode>(H); N.IsValid())
		{
			Parent->ChildNodes.Add(N);
		}
	}
}

FRuitkElementTypeId FRuitkSlateHost::GetTextElementType() const
{
	return Ruitk::TextBlockElementType();
}

void FRuitkSlateHost::RequestFrame(TFunction<void()> Callback)
{
	FrameQueue.Add(MoveTemp(Callback));
	EnsurePreTickRegistered();
}

void FRuitkSlateHost::GetSafeArea(float& OutLeft, float& OutTop, float& OutRight, float& OutBottom) const
{
	OutLeft = OutTop = OutRight = OutBottom = 0.0f;
	if (FSlateApplication::IsInitialized())
	{
		FDisplayMetrics Metrics;
		FSlateApplication::Get().GetCachedDisplayMetrics(Metrics);
		OutLeft = Metrics.TitleSafePaddingSize.X;
		OutTop = Metrics.TitleSafePaddingSize.Y;
		OutRight = Metrics.TitleSafePaddingSize.Z;
		OutBottom = Metrics.TitleSafePaddingSize.W;
	}
}

void FRuitkSlateHost::PumpFrameQueue()
{
	TArray<TFunction<void()>> Batch = MoveTemp(FrameQueue);
	FrameQueue.Reset();
	for (TFunction<void()>& Fn : Batch)
	{
		Fn();
	}
}

void FRuitkSlateHost::OnSlatePreTick(float)
{
	if (!FrameQueue.IsEmpty())
	{
		PumpFrameQueue();
	}
}

void FRuitkSlateHost::EnsurePreTickRegistered()
{
	if (bPreTickRegistered)
	{
		return;
	}
	if (!FSlateApplication::IsInitialized())
	{
		if (!bWarnedNoSlateApp)
		{
			bWarnedNoSlateApp = true;
			UE_LOG(LogRuitkSlate, Warning,
				   TEXT("[ReactiveUI] no Slate application — updates queue until PumpFrameQueue()/FlushSync"));
		}
		return;
	}
	PreTickHandle = FSlateApplication::Get().OnPreTick().AddRaw(this, &FRuitkSlateHost::OnSlatePreTick);
	bPreTickRegistered = true;
}

void FRuitkSlateHost::RemoveChildFromParent(FRuitkSlateNode& Parent, const TSharedRef<SWidget>& ChildWidget)
{
	switch (Parent.Adapter->GetChildKind())
	{
	case ERuitkChildKind::MultiSlot:
		Parent.Adapter->RemoveChild(*Parent.Widget, ChildWidget);
		break;
	case ERuitkChildKind::SingleContent:
		if (Parent.ContentChild.Pin() == ChildWidget)
		{
			Parent.Adapter->SetContent(*Parent.Widget, nullptr);
			Parent.ContentChild.Reset();
		}
		break;
	case ERuitkChildKind::Leaf:
		break;
	}
}

void FRuitkSlateHost::ReplaceWidget(FRuitkSlateNode& Node, const FRuitkPropsBase* OldProps, const FRuitkPropsBase& NewProps)
{
	TSharedPtr<FRuitkSlateNode> Parent = Node.ParentNode.Pin();
	if (!Parent.IsValid() || !Parent->Widget.IsValid() || Parent->Adapter == nullptr)
	{
		// A host node always has a host parent (the wrapped mount panel or another widget). If it
		// somehow does not, we cannot swap it into a slot — apply the runtime diff and warn once.
		UE_LOG(LogRuitkSlate, Warning,
			   TEXT("[ReactiveUI] construct-only change on '%s' could not replace the widget (no host parent) — "
					"runtime props applied, construct-only props stale"),
			   *Ruitk::GetElementTypeName(Node.Type).ToString());
		Node.Adapter->ApplyDiff(*Node.Widget, nullptr, NewProps);
		return;
	}
	IRuitkElementAdapter* Adapter = Node.Adapter;
	const TSharedRef<SWidget> OldWidget = Node.Widget.ToSharedRef();

	// 1. Build the replacement, REUSING the event proxy so bound delegates keep firing
	//    (bind-once-swap-inner: CreateWidget rebinds the new widget's SLATE_EVENT args to the same
	//    proxy, SyncEventHandlers refreshes the inner callbacks).
	const TSharedRef<SWidget> NewWidget = Adapter->CreateWidget(NewProps, Node.Proxy);
	// Runtime props: restore the LAST-committed props first, then apply this render's changes on top.
	// The fresh widget starts at its literal defaults, so a runtime prop the new render OMITS but a
	// prior render set would otherwise reset — violating the family "removed plain props don't reset"
	// rule on the rebuild path (bughunt SEP-REBUILD-1). Construct-only props ride NewProps via CreateWidget.
	if (OldProps != nullptr)
	{
		Adapter->ApplyDiff(*NewWidget, nullptr, *OldProps);
		Adapter->ApplyDiff(*NewWidget, OldProps, NewProps);
	}
	else
	{
		Adapter->ApplyDiff(*NewWidget, nullptr, NewProps);
	}
	if (Node.Proxy.IsValid())
	{
		Adapter->SyncEventHandlers(*Node.Proxy, NewProps);
	}

	// 2. Re-parent the children into the new widget WITH their slot props, order preserved.
	switch (Adapter->GetChildKind())
	{
	case ERuitkChildKind::MultiSlot:
		for (const TWeakPtr<FRuitkSlateNode>& ChildWeak : Node.ChildNodes)
		{
			if (TSharedPtr<FRuitkSlateNode> ChildNode = ChildWeak.Pin();
				ChildNode.IsValid() && ChildNode->Widget.IsValid())
			{
				Adapter->InsertChild(*NewWidget, ChildNode->Widget.ToSharedRef(), -1, ChildNode->SlotProps.Get());
			}
		}
		break;
	case ERuitkChildKind::SingleContent:
		if (TSharedPtr<SWidget> Content = Node.ContentChild.Pin())
		{
			Adapter->SetContent(*NewWidget, Content);
		}
		break;
	case ERuitkChildKind::Leaf:
		break;
	}

	// 3. Adopt this render's slot props (a construct-only change may ride with a slot change), then
	//    swap the new widget into the parent's slot at the SAME index.
	if (NewProps.SlotProps.IsValid())
	{
		Node.SlotProps = MakeShared<FRuitkStyleDict>(*NewProps.SlotProps);
	}
	else
	{
		Node.SlotProps.Reset();
	}
	const int32 Index = Parent->ChildNodes.IndexOfByPredicate([&Node](const TWeakPtr<FRuitkSlateNode>& W)
															  { return W.Pin().Get() == &Node; });
	RemoveChildFromParent(*Parent, OldWidget);
	switch (Parent->Adapter->GetChildKind())
	{
	case ERuitkChildKind::MultiSlot:
		Parent->Adapter->InsertChild(*Parent->Widget, NewWidget, Index, Node.SlotProps.Get());
		break;
	case ERuitkChildKind::SingleContent:
		Parent->Adapter->SetContent(*Parent->Widget, NewWidget);
		Parent->ContentChild = NewWidget;
		break;
	case ERuitkChildKind::Leaf:
		break;
	}

	// 4. Adopt the new widget + re-apply the effective style from scratch (the old style lived on
	//    the discarded widget). The node identity, ChildNodes, and parent link are all unchanged.
	Node.Widget = NewWidget;
	Node.AppliedStyle.Reset();
	TSharedPtr<FRuitkStyleDict> Effective = Ruitk::Slate::BuildEffectiveStyle(NewProps.Classes, NewProps.Style);
	if (Effective.IsValid())
	{
		Ruitk::Slate::ApplyStyleDiff(*NewWidget, Adapter, nullptr, Effective.Get());
	}
	Node.AppliedStyle = Effective;
}
