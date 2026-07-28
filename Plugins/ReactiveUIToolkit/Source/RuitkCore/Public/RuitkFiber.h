// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// FRuitkFiber — a node in the persistent work tree, current/WIP paired via Alternate
// (double buffering, D-06). Fibers live in the reconciler-owned SLAB (FRuitkFiberSlab):
// fixed addresses, free-list reuse, RAW intra-tree pointers, zero ref counting inside the
// tree. The Godot port's "sever cycles explicitly" generalizes here to "no cycles exist" —
// only three ownership edges leave the slab (host handle, shared state, shared props).

#pragma once

#include "CoreMinimal.h"
#include "RuitkNode.h"
#include "RuitkComponentState.h"

enum class ERuitkFiberTag : uint8
{
	Function,
	Host,
	Fragment,
	Portal,
	ErrorBoundary,
	Root,
};

/** Effect flags — what commit must do (family bitmask, incl. the PortalRetarget addition). */
enum ERuitkEffect : uint8
{
	RuitkEffect_None = 0,
	RuitkEffect_Placement = 1 << 0,
	RuitkEffect_Update = 1 << 1,
	RuitkEffect_Deletion = 1 << 2,
	RuitkEffect_Layout = 1 << 3,
	RuitkEffect_Passive = 1 << 4,
	RuitkEffect_PortalRetarget = 1 << 5,
};

struct RUITKCORE_API FRuitkFiber
{
	FRuitkFiber() = default;
	// Never copied/moved: fibers are slab-owned with RAW pointers into them everywhere
	// (Parent/Child/Sibling/Alternate, state back-pointers) — address stability is the
	// contract (D-06).
	FRuitkFiber(const FRuitkFiber&) = delete;
	FRuitkFiber& operator=(const FRuitkFiber&) = delete;

	// --- tree (raw, slab-owned) ---
	FRuitkFiber* Parent = nullptr;
	FRuitkFiber* Child = nullptr;
	FRuitkFiber* Sibling = nullptr;
	int32 Index = 0;

	// --- identity ---
	ERuitkFiberTag Tag = ERuitkFiberTag::Host;
	FRuitkKey Key;
	FRuitkElementTypeId ElementType;			// HOST
	FName ComponentId;						// FUNCTION (registry identity, D-05)
	TSharedPtr<FRuitkComponentInvoke> Invoke; // FUNCTION

	// --- props ---
	TSharedPtr<const FRuitkPropsBase> Props; // committed (null = never rendered)
	TSharedPtr<const FRuitkPropsBase> PendingProps;
	FRuitkChildren InputChildren; // child vnodes to reconcile (shared list)

	// --- host ---
	FRuitkHostHandle Node;

	// --- portal ---
	FRuitkPortalHandle PortalTarget;

	// --- error boundary (structural, D-10) ---
	bool bEbActive = false;
	FString EbLastError;
	FRuitkKey EbResetKey;
	TSharedPtr<FRuitkNode> EbFallback;
	TFunction<void(const FString&)> EbOnError;
	FRuitkChildren EbChildren;

	// --- reconciliation / double buffer ---
	FRuitkFiber* Alternate = nullptr;
	uint8 EffectTag = RuitkEffect_None;
	FRuitkFiber* NextEffect = nullptr; // singly-linked post-order effect list
	bool bHasDeletions = false;		 // this fiber recorded deletions this pass
	bool bMatchedPass = false;		 // full-keyed mark-and-sweep (GO-08)

	// --- context ---
	/** Values THIS fiber provides (keyed by context-handle identity). Type-erased holder +
	 *  the typed compare/propagate closures live with the context implementation. */
	TSharedPtr<TMap<const void*, TSharedPtr<void>>> ProvidedContext;
	bool bReadsContext = false;

	// --- bailout / dirty tracking ---
	bool bHasPendingUpdate = false;
	/** Written on schedule AND CONSUMED by the subtree-skip bailout (D-08.1) — the one the
	 *  Godot port wrote but never consumed. */
	bool bSubtreeHasUpdates = false;

	// --- function-component state (SHARED across alternates) ---
	TSharedPtr<FRuitkComponentState> State;

	bool IsPortal() const { return Tag == ERuitkFiberTag::Portal; }
	bool IsRoot() const { return Tag == ERuitkFiberTag::Root; }

	/** Can this fiber be reused for `vnode`? (family matches()) */
	bool Matches(const FRuitkNode& VNode) const
	{
		switch (VNode.Kind)
		{
		case ERuitkNodeKind::Host:
			return Tag == ERuitkFiberTag::Host && ElementType == VNode.ElementType;
		case ERuitkNodeKind::Function:
			return Tag == ERuitkFiberTag::Function && ComponentId == VNode.ComponentId;
		case ERuitkNodeKind::Fragment:
			return Tag == ERuitkFiberTag::Fragment;
		case ERuitkNodeKind::Portal:
			return Tag == ERuitkFiberTag::Portal;
		case ERuitkNodeKind::ErrorBoundary:
			return Tag == ERuitkFiberTag::ErrorBoundary;
		}
		return false;
	}

	static ERuitkFiberTag TagForNode(const FRuitkNode& VNode)
	{
		switch (VNode.Kind)
		{
		case ERuitkNodeKind::Host:
			return ERuitkFiberTag::Host;
		case ERuitkNodeKind::Function:
			return ERuitkFiberTag::Function;
		case ERuitkNodeKind::Fragment:
			return ERuitkFiberTag::Fragment;
		case ERuitkNodeKind::Portal:
			return ERuitkFiberTag::Portal;
		case ERuitkNodeKind::ErrorBoundary:
			return ERuitkFiberTag::ErrorBoundary;
		}
		return ERuitkFiberTag::Host;
	}

	/** Full reset for slab reuse (every field to fresh-fiber state). */
	void ResetForReuse()
	{
		Parent = Child = Sibling = nullptr;
		Index = 0;
		Tag = ERuitkFiberTag::Host;
		Key = FRuitkKey();
		ElementType = FRuitkElementTypeId();
		ComponentId = NAME_None;
		Invoke.Reset();
		Props.Reset();
		PendingProps.Reset();
		InputChildren.Reset();
		Node.Reset();
		PortalTarget.Reset();
		bEbActive = false;
		EbLastError.Empty();
		EbResetKey = FRuitkKey();
		EbFallback.Reset();
		EbOnError = nullptr;
		EbChildren.Reset();
		Alternate = nullptr;
		EffectTag = RuitkEffect_None;
		NextEffect = nullptr;
		bHasDeletions = false;
		bMatchedPass = false;
		ProvidedContext.Reset();
		bReadsContext = false;
		bHasPendingUpdate = false;
		bSubtreeHasUpdates = false;
		State.Reset();
	}
};

/**
 * The fiber slab: paged storage with a free list. Fixed addresses for the lifetime of the
 * reconciler (closures/state back-pointers rely on it), O(1) acquire/release, zero
 * steady-state allocation once warmed (stable trees reuse alternates and never touch the
 * slab at all). Per-reconciler — a torn-down root frees exactly its own pages.
 */
class RUITKCORE_API FRuitkFiberSlab
{
public:
	static constexpr int32 PageSize = 256;

	~FRuitkFiberSlab()
	{
		for (FPage* Page : Pages)
		{
			delete Page;
		}
	}

	FRuitkFiber* Acquire()
	{
		if (FreeList != nullptr)
		{
			FRuitkFiber* Out = FreeList;
			FreeList = Out->Sibling; // free list threads through Sibling
			Out->Sibling = nullptr;
			++LiveCount;
			return Out;
		}
		if (Pages.IsEmpty() || Pages.Last()->Used == PageSize)
		{
			Pages.Add(new FPage());
		}
		FPage* Page = Pages.Last();
		FRuitkFiber* Out = &Page->Fibers[Page->Used++];
		++LiveCount;
		return Out;
	}

	/** Return ONE fiber (caller handles the alternate pair — see reconciler Release()). */
	void Release(FRuitkFiber* Fiber)
	{
		Fiber->ResetForReuse();
		Fiber->Sibling = FreeList;
		FreeList = Fiber;
		--LiveCount;
	}

	int32 NumLive() const { return LiveCount; }
	int32 NumPages() const { return Pages.Num(); }

private:
	struct FPage
	{
		FRuitkFiber Fibers[PageSize];
		int32 Used = 0;
	};

	TArray<FPage*> Pages;
	FRuitkFiber* FreeList = nullptr;
	int32 LiveCount = 0;
};
