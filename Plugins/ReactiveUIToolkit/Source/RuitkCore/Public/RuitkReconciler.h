// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// FRuitkReconciler — the fiber reconciler, ported from reconciler.gd with the D-08 React
// adoptions the Godot port hasn't made yet:
//
//   render phase  -> begin work descending (reconcile children, run components with
//                    bailout; SUBTREE-SKIP: a clean fiber with no dirty descendants
//                    returns null and skips its whole subtree — bSubtreeHasUpdates is
//                    CONSUMED here, the family's known missing piece), then complete
//                    ascending (host-node effects; provider stacks pop here, including
//                    on every skip path). Post-order singly-linked effect list (17-style —
//                    right for a synchronous renderer).
//   commit phase  -> deletions -> placement/update/portal-retarget/layout effects ->
//                    reorder (structural frames only) -> current<->WIP swap -> two-pass
//                    passive flush -> deferred-update replay.
//
// Child reconciliation: fast-leaf-list (+ GO-09 reuse_by_slot) -> keys-stable positional ->
// full keyed mark-and-sweep with a persistent key map (GO-08). Updates coalesce to one
// host RequestFrame; setState mid-render restarts from the root (25-restart guard);
// setState mid-commit defers and replays. Double-buffered fibers from the slab (D-06).

#pragma once

#include "CoreMinimal.h"
#include "RuitkNode.h"
#include "RuitkFiber.h"
#include "RuitkHostConfig.h"
#include "RuitkCoreMisc.h"
#include "RuitkContextHandle.h"

class RUITKCORE_API FRuitkReconciler
{
public:
	/** Host outlives the reconciler (the root object owns both — mount-surface contract). */
	FRuitkReconciler(IRuitkHostConfig& InHost, FRuitkHostHandle InRootContainer);
	~FRuitkReconciler();

	FRuitkReconciler(const FRuitkReconciler&) = delete;
	FRuitkReconciler& operator=(const FRuitkReconciler&) = delete;

	/** Mount / replace the root vnode. Initial render is always synchronous (no empty
	 *  first frame). */
	void Render(FRuitkNode RootNode);

	/** Mark a fiber dirty and coalesce a re-render (hook setters land here). */
	void ScheduleUpdateOnFiber(FRuitkFiber* Fiber);
	void RequestUpdate();

	/** Run any pending work NOW, synchronously and unsliced (tests, HMR, mount surfaces).
	 *  "Unsliced" is enforced, not assumed (P-06): bForceSyncPass is raised for the duration,
	 *  every slicing decision reads IsTimeSlicing() && !bForceSyncPass, and passes run to
	 *  quiescence — a commit's deferred-update replay that schedules a follow-up pass is run
	 *  too, so no parked WIP survives the call. */
	void FlushSync();

	/** HMR: mark EVERY function fiber dirty (defeats bailout caches — component definitions
	 *  may have been swapped under their ComponentIds) and coalesce a re-render. */
	void HmrRefreshAll();

	/** Every live reconciler (mounted roots register in ctor/dtor) — how HMR reaches the
	 *  running UIs without plumbing. Game-thread only. */
	static void ForEachLive(TFunctionRef<void(FRuitkReconciler&)> Fn);

	/** Tear down: cleanups, refs nulled, host nodes released, fibers freed. */
	void Unmount();

	bool IsMounted() const { return RootCurrent != nullptr; }

	// --- introspection (tests / ruitk.Stats / the Inspector) ---
	FRuitkFiber* GetRootFiber() const { return RootCurrent; }
	int32 NumLiveFibers() const { return Slab.NumLive(); }
	IRuitkHostConfig& GetHost() { return Host; }

	// --- internal API for the hooks layer (FRuitkContext) — \internal ---
	void NotifyEffectKinds(FRuitkFiber& Fiber, bool bHasPassive, bool bHasLayout);
	static const IRuitkProvidedValue* ResolveProvidedOnCommitted(const FRuitkFiber* From, const void* Key);
	void OnProvidedValueChanged(FRuitkFiber& ProviderFiber, const void* Key);

private:
	// --- scheduling ---
	void EnsureTick();
	void Tick();

	// --- render phase ---
	void BeginRender();
	FRuitkFiber* PerformUnit(FRuitkFiber* Fiber);
	FRuitkFiber* BeginFunction(FRuitkFiber* Fiber);
	FRuitkFiber* BeginErrorBoundary(FRuitkFiber* Fiber);
	void RenderComponent(FRuitkFiber* Fiber); // output lands in State->LastOutput (shared once)
	void CompleteWork(FRuitkFiber* Fiber);
	void PushProvidedContext(FRuitkFiber* Fiber);
	void PopProvidedContext(FRuitkFiber* Fiber);

	// --- child reconciliation ---
	FRuitkFiber* ReconcileFiber(FRuitkFiber* ParentFiber, FRuitkFiber* OldFiber, const FRuitkNode& VNode, int32 Index);
	/** Returns true when the fast-leaf-list path fully handled the children in place. */
	bool ReconcileChildren(FRuitkFiber* ParentFiber, FRuitkFiber* OldFirst, const FRuitkChildren& ChildVNodes);
	bool TryFastLeafList(FRuitkFiber* ParentFiber, FRuitkFiber* OldFirst, const TArray<FRuitkNode>& VNodes,
						 bool bIgnoreKeys);
	bool KeysStable(FRuitkFiber* OldFirst, const TArray<FRuitkNode>& VNodes) const;
	void DeleteFiber(FRuitkFiber* ParentFiber, FRuitkFiber* OldFiber);
	void MarkReorder(FRuitkFiber* ParentFiber);
	bool HasContextChanged(const FRuitkFiber* Fiber) const;
	void AppendEffect(FRuitkFiber* Fiber);

	// --- commit phase ---
	void CommitRoot();
	void CommitPlacement(FRuitkFiber* Fiber);
	void CommitUpdate(FRuitkFiber* Fiber);
	void CommitDeletion(FRuitkFiber* Fiber);
	void CommitPortalRetarget(FRuitkFiber* Fiber);
	void CommitLayoutEffects(FRuitkFiber* Fiber);
	void FlushPassive();
	void RunCleanups(FRuitkFiber* Fiber);
	void RunCleanupsRecursive(FRuitkFiber* Fiber);
	void DisposeFiberState(FRuitkFiber* Fiber);
	void NullRefsRecursive(FRuitkFiber* Fiber);
	void EnforceChildOrder(FRuitkFiber* ParentFiber);
	void CollectHostChildren(FRuitkFiber* Fiber, TArray<FRuitkHostHandle>& Out) const;
	void ReleaseHostNodes(FRuitkFiber* Fiber);
	void DetachPortalChildren(FRuitkFiber* Fiber);
	FRuitkHostHandle HostParentNode(FRuitkFiber* Fiber, bool& bOutViaPortal, FRuitkPortalHandle& OutPortal) const;

	// --- error latch (D-10) ---
	void HandleRenderFailure(FRuitkFiber* FailedFiber, const FString& Reason);
	void AdoptPendingEbActivation(FRuitkFiber* BoundaryFiber);

	// --- tree lifecycle ---
	void ReleaseFiberTree(FRuitkFiber* Fiber);
	/** Reclaim the fresh WIP fibers of an abandoned pass (restart / aborted mount). */
	void ReleaseAbandonedChildren(FRuitkFiber* Parent);

	// --- helpers ---
	static FRuitkFiber* OldFirst(FRuitkFiber* Fiber) { return Fiber ? Fiber->Child : nullptr; }
	static bool AnyKeyed(const TArray<FRuitkNode>& VNodes);
	static FRuitkKey FiberKey(const FRuitkFiber* F);
	static FRuitkKey VNodeKey(const FRuitkNode& VNode, int32 Index);
	static bool ChildrenSame(const FRuitkChildren& A, const FRuitkChildren& B);
	bool PropsEqual(const FRuitkFiber* Fiber) const;
	const TArray<FRuitkNode>& NormalizedChildren(const FRuitkChildren& Children) const;

private:
	IRuitkHostConfig& Host;
	FRuitkHostHandle RootContainer;
	FRuitkFiberSlab Slab;

	TOptional<FRuitkNode> RootVNode;
	FRuitkFiber* RootCurrent = nullptr;

	FRuitkFiber* WipRoot = nullptr;
	FRuitkFiber* NextUnit = nullptr;

	FRuitkFiber* FirstEffect = nullptr;
	FRuitkFiber* LastEffect = nullptr;
	TArray<FRuitkFiber*> Deletions;
	TSet<FRuitkFiber*> ReorderSet;
	TArray<FRuitkFiber*> PendingPassive;

	/** Persistent full-keyed map (GO-08): cleared per call, never re-allocated per frame. */
	TMap<FRuitkKey, FRuitkFiber*> KeyMap;

	/** Providers pushed this pass (parallel stack so ascend/skip paths pop exactly what
	 *  descend pushed, even across restarts). */
	TArray<FRuitkFiber*> ProviderStack;

	/** A mount-pass render failure records its boundary activation by key-path — the WIP
	 *  fiber that carried it is abandoned with the pass and has no committed twin, so the
	 *  activation must survive OUTSIDE the fiber tree until the restart pass re-adopts it
	 *  (deterministic positional keys make the path stable across identical passes). */
	struct FPendingEbActivation
	{
		TArray<FRuitkKey> Path; // boundary-up-to-root key path
		FString Reason;
	};
	TArray<FPendingEbActivation> PendingEbActivations;

	bool bIsCommitting = false;
	TArray<FRuitkFiber*> DeferredUpdates;
	bool bWorkActive = false;
	/** Raised only inside FlushSync (scoped): forces every slicing decision to false so the
	 *  in-flight pass cannot park (P-06 — the ":unsliced" contract of FlushSync). */
	bool bForceSyncPass = false;
	bool bRestart = false;
	bool bTickPending = false;
	int32 RestartCount = 0;

	static constexpr int32 MaxRestarts = 25;

	/** Scratch for NormalizedChildren's empty case. */
	static const TArray<FRuitkNode> EmptyChildren;
};
