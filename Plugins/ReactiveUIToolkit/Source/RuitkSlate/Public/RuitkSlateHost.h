// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// FRuitkSlateHost — IRuitkHostConfig over real Slate widgets (D-11): the ONLY runtime seam
// between the reconciler and concrete Slate APIs. Generic by construction: everything
// widget-specific routes through the adapter registry. FRuitkSlateNode is what an
// FRuitkHostHandle points at on this host.
//
// Scheduling: RequestFrame callbacks queue and drain once per Slate OnPreTick (registered
// lazily, unregistered on destruction). NEVER executed synchronously — a hook setter must
// not re-enter the reconciler from its own call stack (the mid-render restart contract).

#pragma once

#include "CoreMinimal.h"
#include "RuitkElementAdapter.h"
#include "RuitkEventProxy.h"
#include "RuitkHostConfig.h"
#include "RuitkScheduler.h"
#include "Widgets/SWidget.h"

/** One mounted widget: the concrete payload behind FRuitkHostHandle on the Slate host. */
struct RUITKSLATE_API FRuitkSlateNode
{
	TSharedPtr<SWidget> Widget;
	TSharedPtr<FRuitkEventProxy> Proxy; // minted only when the adapter binds events
	IRuitkElementAdapter* Adapter = nullptr;
	FRuitkElementTypeId Type;

	/** Latest committed slot.* props (owned copy — parents re-apply on reorder/updates). */
	TSharedPtr<FRuitkStyleDict> SlotProps;

	/** The EFFECTIVE style dict last applied (classes merged + inline) — the diff base. */
	TSharedPtr<FRuitkStyleDict> AppliedStyle;

	/** The parent NODE while attached (child ops + slot-prop updates go via its adapter). */
	TWeakPtr<FRuitkSlateNode> ParentNode;

	/** MultiSlot child NODES in slot order — the bookkeeping a construct-only widget REPLACEMENT
	 *  needs to re-parent children WITH their slot.* props into the rebuilt widget (TD-011). Kept
	 *  in sync by Insert/Remove/ReorderChild; dead weak refs are compacted on read. */
	TArray<TWeakPtr<FRuitkSlateNode>> ChildNodes;

	/** SingleContent bookkeeping: the current content child (warn_capacity semantics). */
	TWeakPtr<SWidget> ContentChild;
	bool bWarnedCapacity = false;
};

namespace Ruitk::Slate
{
	/** Resolve a captured `Ref` handle to its live widget, typed (P2: the ScrollToEnd-class
	 *  imperative escape hatch — `WidgetFromHandle<SScrollBox>(H)->ScrollToEnd()`). The CALLER
	 *  names the type; guard with `Widget->GetType()` when the ref could point elsewhere. */
	template <typename TWidget> TSharedPtr<TWidget> WidgetFromHandle(const FRuitkHostHandle& Handle)
	{
		const FRuitkSlateNode* Node = static_cast<const FRuitkSlateNode*>(Handle.Get());
		return (Node != nullptr && Node->Widget.IsValid()) ? StaticCastSharedPtr<TWidget>(Node->Widget)
														   : TSharedPtr<TWidget>();
	}
} // namespace Ruitk::Slate

class RUITKSLATE_API FRuitkSlateHost final : public IRuitkHostConfig
{
public:
	FRuitkSlateHost();
	virtual ~FRuitkSlateHost() override;

	FRuitkSlateHost(const FRuitkSlateHost&) = delete;
	FRuitkSlateHost& operator=(const FRuitkSlateHost&) = delete;

	/** Wrap an existing panel widget (the mount surface's root) as a host node so the
	 *  reconciler can parent top-level children into it. */
	FRuitkHostHandle WrapExternalPanel(const TSharedRef<SWidget>& Panel, FRuitkElementTypeId PanelType);

	static FRuitkSlateNode* Resolve(const FRuitkHostHandle& Handle)
	{
		return static_cast<FRuitkSlateNode*>(Handle.Get());
	}

	// ── IRuitkHostConfig ────────────────────────────────────────────────────────────────────
	virtual FRuitkHostHandle CreateInstance(FRuitkElementTypeId Type, const FRuitkPropsBase& Props) override;
	virtual void CommitUpdate(const FRuitkHostHandle& Node, FRuitkElementTypeId Type, const FRuitkPropsBase* OldProps,
							  const FRuitkPropsBase& NewProps) override;
	virtual void ReleaseInstance(const FRuitkHostHandle& Node, FRuitkElementTypeId Type,
								 const TSharedPtr<const FRuitkPropsBase>& LastProps, bool bWasChildless) override;
	virtual void OnBeforeCommit() override;
	virtual void OnAfterCommit() override;
	virtual void InsertChild(const FRuitkHostHandle& Parent, const FRuitkHostHandle& Child, int32 Index) override;
	virtual void RemoveChild(const FRuitkHostHandle& Parent, const FRuitkHostHandle& Child) override;
	virtual void ReorderChildren(const FRuitkHostHandle& Parent, const TArray<FRuitkHostHandle>& Ordered) override;
	virtual FRuitkElementTypeId GetTextElementType() const override;
	virtual void RequestFrame(TFunction<void()> Callback) override;
	virtual void GetSafeArea(float& OutLeft, float& OutTop, float& OutRight, float& OutBottom) const override;
	/** P-03: this host owns the scheduler instance and pumps it once per Slate PreTick.
	 *  Obtaining it arms the PreTick seam (same lazy registration as RequestFrame). */
	virtual FRuitkScheduler* GetScheduler() override;

	/** Drain the frame queue now (tests / FlushSync surfaces — same batch rule as PreTick:
	 *  callbacks queued while draining run on the NEXT drain). */
	void PumpFrameQueue();

	int32 NumPooled(FRuitkElementTypeId Type) const;

private:
	void OnSlatePreTick(float DeltaTime);
	void EnsurePreTickRegistered();
	static void RemoveChildFromParent(FRuitkSlateNode& Parent, const TSharedRef<SWidget>& ChildWidget);

	/** TD-011: a construct-only prop changed — rebuild the widget (reusing the event proxy),
	 *  re-parent its children with their slot props, and swap it into the parent's slot in place.
	 *  Returns the new widget, or the existing one if replacement was not possible (no parent). */
	void ReplaceWidget(FRuitkSlateNode& Node, const FRuitkPropsBase* OldProps, const FRuitkPropsBase& NewProps);

	TArray<TFunction<void()>> FrameQueue;
	FDelegateHandle PreTickHandle;
	bool bPreTickRegistered = false;
	bool bWarnedNoSlateApp = false;

	/** The frame scheduler (M2, P-01/P-03): default engine clock; pumped in OnSlatePreTick
	 *  right after the frame-queue drain, once per frame. */
	FRuitkScheduler Scheduler;

	/** GO-05 node pool: detached event-less leaves, stashed with their last props + applied
	 *  style, diffed on reuse. Per-type cap; drained with the host. */
	struct FPoolEntry
	{
		TSharedPtr<SWidget> Widget;
		TSharedPtr<const FRuitkPropsBase> LastProps;
		TSharedPtr<FRuitkStyleDict> AppliedStyle;
	};
	TMap<FRuitkElementTypeId, TArray<FPoolEntry>> Pool;
	static constexpr int32 PoolCapPerType = 256;

	/** Focus capture between the commit fences (per-commit; cleared after restore). */
	TWeakPtr<SWidget> FocusedBeforeCommit;
};
