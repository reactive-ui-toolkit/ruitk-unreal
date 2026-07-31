// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Port of reconciler.gd (read in full as the spec) with the D-08 adoptions. Faithful-first:
// section order and comments track the original so future syncs diff cleanly. Documented
// divergences: (1) SUBTREE-SKIP bailout consumes bSubtreeHasUpdates (React
// bailoutOnAlreadyFinishedWork — the WIP adopts the CURRENT child chain wholesale, exactly
// React's no-clone fast path); (2) no raw-string child normalization (C++ arrays are
// homogeneous — Ruitk::TextBlock is explicit; the family's flatten becomes a no-op); (3) refs
// follow the React lifecycle (attach on placement, detach on deletion — D-08.4), not
// call-every-commit; (4) minimal-move reordering is HOST-side (the Phase 2 spike decides
// the Slate strategy) — the core marks structural frames and calls ReorderChildren, same
// contract as the family's enforce-order.

#include "RuitkReconciler.h"
#include "RuitkContext.h"
#include "RuitkCoreElements.h"
#include "RuitkElementRegistry.h" // GetElementTypeName — Verbose trace detail (M7)
#include "RuitkScheduler.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuitkReconciler, Log, All);

const TArray<FRuitkNode> FRuitkReconciler::EmptyChildren;

namespace
{
	TArray<FRuitkReconciler*>& LiveReconcilers()
	{
		static TArray<FRuitkReconciler*> Instances;
		return Instances;
	}

	// ── Verbose structural-line detail (M7/P-08: element type / ComponentId / key) ────────
	// Only ever called behind Ruitk::TraceDetail() — Basic keeps the bare per-kind line and
	// pays no formatting.

	FString TraceKeySuffix(const FRuitkKey& Key)
	{
		switch (Key.Kind)
		{
		case FRuitkKey::EKind::Int:
			return FString::Printf(TEXT(" key=%lld"), Key.IntValue);
		case FRuitkKey::EKind::Name:
			return FString::Printf(TEXT(" key=%s"), *Key.NameValue.ToString());
		default:
			return FString();
		}
	}

	FString TraceFiberLabel(const FRuitkFiber* Fiber)
	{
		switch (Fiber->Tag)
		{
		case ERuitkFiberTag::Host:
			return Ruitk::GetElementTypeName(Fiber->ElementType).ToString();
		case ERuitkFiberTag::Function:
			return Fiber->ComponentId.ToString();
		case ERuitkFiberTag::Fragment:
			return FString(TEXT("Fragment"));
		case ERuitkFiberTag::Portal:
			return FString(TEXT("Portal"));
		case ERuitkFiberTag::ErrorBoundary:
			return FString(TEXT("ErrorBoundary"));
		default:
			return FString(TEXT("Root"));
		}
	}

	FString TraceVNodeLabel(const FRuitkNode& VNode)
	{
		switch (VNode.Kind)
		{
		case ERuitkNodeKind::Host:
			return Ruitk::GetElementTypeName(VNode.ElementType).ToString();
		case ERuitkNodeKind::Function:
			return VNode.ComponentId.ToString();
		case ERuitkNodeKind::Portal:
			return FString(TEXT("Portal"));
		case ERuitkNodeKind::ErrorBoundary:
			return FString(TEXT("ErrorBoundary"));
		default:
			return FString(TEXT("Fragment"));
		}
	}
} // namespace

FRuitkReconciler::FRuitkReconciler(IRuitkHostConfig& InHost, FRuitkHostHandle InRootContainer)
	: Host(InHost), RootContainer(MoveTemp(InRootContainer))
{
	FRuitkFiber* Root = Slab.Acquire();
	Root->Tag = ERuitkFiberTag::Root;
	Root->Node = RootContainer;
	RootCurrent = Root;
	LiveReconcilers().Add(this);
}

FRuitkReconciler::~FRuitkReconciler()
{
	LiveReconcilers().Remove(this);
	if (RootCurrent != nullptr)
	{
		Unmount();
	}
}

void FRuitkReconciler::ForEachLive(TFunctionRef<void(FRuitkReconciler&)> Fn)
{
	// snapshot: Fn may mount/unmount (HMR refresh triggers renders)
	TArray<FRuitkReconciler*> Snapshot = LiveReconcilers();
	for (FRuitkReconciler* Reconciler : Snapshot)
	{
		if (LiveReconcilers().Contains(Reconciler))
		{
			Fn(*Reconciler);
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Scheduling
// ─────────────────────────────────────────────────────────────────────────────────────────

void FRuitkReconciler::Render(FRuitkNode RootNode)
{
	checkf(IsInGameThread(), TEXT("Ruitk: Render must run on the game thread (D-15)"));
	if (RootCurrent == nullptr)
	{
		return; // torn down — a render after unmount is a no-op, not a crash [audit]
	}
	// Initial / top-level mount is ALWAYS synchronous, regardless of ruitk.TimeSlicing (the
	// family rule, FiberReconciler.cs:125-129 — no empty first frame). A parked sliced pass
	// and its queued Slice action are pre-empted: the new root vnode supersedes them.
	bTickPending = false;
	CancelQueuedSlice();
	RootVNode = MoveTemp(RootNode);
	RootCurrent->bHasPendingUpdate = true;
	bWorkActive = false; // force a fresh BeginRender from the new root vnode (reclaims WIP)
	// A setState during a mount render DEFERS like any mid-flight update (P-11) and replays
	// as a queued follow-up after this commit; a render FAILURE still rebuilds from the root
	// inside DoWork — mount promises a coherent first frame, and a boundary fallback must
	// never lose to a half-built tree (documented divergence from the Godot commit-and-
	// re-tick shape).
	TGuardValue<bool> ForceSync(bForceSyncPass, true);
	DoWork(/*bViaScheduler=*/false);
}

void FRuitkReconciler::ScheduleUpdateOnFiber(FRuitkFiber* Fiber)
{
	ScheduleUpdateInternal(Fiber, /*bScheduleWork=*/true);
}

void FRuitkReconciler::ScheduleUpdateInternal(FRuitkFiber* Fiber, bool bScheduleWork)
{
	checkf(
		IsInGameThread(),
		TEXT("Ruitk: state updates must run on the game thread (D-15; use Ruitk::PostToGameThread from async code)"));
	if (RootCurrent == nullptr)
	{
		return; // torn down — ignore late setState/effect callbacks [audit]
	}
	FRuitkFiber* Target = Fiber ? Fiber : RootCurrent;

	// Membership walk BEFORE touching any flags (FiberReconciler.cs:204-300): a fiber inside
	// a subtree deleted this pass bails silently — its deletion is already committed intent;
	// a walk that tops out anywhere but a root we own means a detached/stale fiber — warn
	// and bail rather than mark garbage. A walk that tops out at the SUPERSEDED root
	// (RootCurrent->Alternate — the home of deferred updates captured mid-pass and replayed
	// post-commit) falls through: the both-twin marking below re-marks the pending flags on
	// the live counterparts, which IS the reference's redirect re-mark (:254-281).
	FRuitkFiber* Top = Target;
	for (FRuitkFiber* F = Target; F != nullptr; F = F->Parent)
	{
		if ((F->EffectTag & RuitkEffect_Deletion) != 0)
		{
			return; // deleted mid-pass — the update dies with the subtree (:236-239)
		}
		Top = F;
	}
	const bool bKnownRoot = (Top == RootCurrent) || (Top == WipRoot) ||
							(RootCurrent->Alternate != nullptr && Top == RootCurrent->Alternate);
	if (!bKnownRoot)
	{
		UE_LOG(LogRuitkReconciler, Warning,
			   TEXT("[Ruitk] update on a detached fiber — ignored (a released/unmounted component's setter?)"));
		return;
	}

	// Mark the target AND its alternate twin, and set the subtree flag on every ancestor AND
	// its twin (React's markUpdateLaneFromFiberToRoot parity). A shared state's Fiber may point
	// at whichever buffer last rendered THIS component — but a bailed-out ancestor is reached
	// through the OTHER buffer next pass (ReconcileFiber copies the flag from the committed
	// side). Marking both twins guarantees the flag survives onto the WIP regardless of which
	// buffer the async setState (frame/timer callback) happened to land on. [audit: async
	// setState through a bailing intermediate — TD-003 Presence exposed it].
	Target->bHasPendingUpdate = true;
	if (Target->Alternate != nullptr)
	{
		Target->Alternate->bHasPendingUpdate = true;
	}
	for (FRuitkFiber* P = Target->Parent; P != nullptr; P = P->Parent)
	{
		P->bSubtreeHasUpdates = true;
		if (P->Alternate != nullptr)
		{
			P->Alternate->bSubtreeHasUpdates = true;
		}
	}
	if (!bScheduleWork)
	{
		return; // deferred replay: CommitRoot's tail schedules ONCE after the drain (P-11(b))
	}
	if (bIsCommitting)
	{
		// Mutating the WIP mid-commit would corrupt the tree being committed — defer.
		DeferredUpdates.Add(Target);
		return;
	}
	// Mid-flight (a synchronous pass on this stack, or a sliced pass parked between quanta):
	// DEFER, never restart (P-11(a), FiberReconciler.cs:311-325). Restarting on every update
	// starves large trees under sustained per-frame updates — the pass never reaches commit —
	// and the aborted walk's fresh fibers leak. The deferred update replays from CommitRoot's
	// tail, coalescing everything that arrived during the pass into ONE follow-up render.
	if (bWorkActive && !bReplayingDeferred)
	{
		DeferredUpdates.Add(Target);
		if (Ruitk::IsRendering())
		{
			bDeferredFromRender = true; // a render-phase setState — feeds the depth ladder
			// Strict diagnostics, family warning 1 (M5): the component set its OWN state while
			// its render was on the stack (Target->State->bIsRendering — the per-component
			// discriminator; a parent's setter fired from a child's render is the legal
			// lift-state-up shape and stays silent). Warn only — the defer above IS the
			// behavior. Deduped per component (P-10 core); StrictMode's second invoke dedups
			// through the same set (diagnostics count once). The post-commit REPLAY can never
			// reach this line: replayed updates re-mark with bScheduleWork=false and return
			// above, and replay runs outside render anyway (CommitRoot's tail).
			if (FRuitkConfig::IsStrictDiagnosticsEnabled() && Target->State.IsValid() && Target->State->bIsRendering)
			{
				Ruitk::DiagWarnOnce(*Target->State, FName(TEXT("strict-setstate-in-render")),
									[Target]() -> FString
									{
										return FString::Printf(
											TEXT("[Ruitk][strict] %s: state update during render — move it into an "
												 "effect or event handler"),
											*Target->ComponentId.ToString());
									});
			}
		}
		return;
	}
	EnsureWork();
}

void FRuitkReconciler::RequestUpdate()
{
	ScheduleUpdateOnFiber(RootCurrent);
}

void FRuitkReconciler::EnsureTick()
{
	if (bTickPending)
	{
		return;
	}
	bTickPending = true;
	Host.RequestFrame([this]() { Tick(); });
	// NOTE on lifetime: the mount surface owns reconciler + host and tears both down
	// together; Unmount() flips RootCurrent null and Tick() self-guards. The mock host
	// drops queued frames on destruction; the Slate host unregisters its OnPreTick.
}

void FRuitkReconciler::FlushSync()
{
	// P-06: "synchronously and unsliced" is enforced, not assumed — while bForceSyncPass is
	// raised, every slicing decision reads false, so a pass below can never park. Loop to
	// quiescence: the commit's deferred-update replay (CommitRoot's tail) may schedule a
	// follow-up pass via EnsureTick — run that too before returning, mirroring the family
	// scheduler's PumpNow full drain (RenderScheduler.cs:214-223) and the sync-mode replay
	// (FiberReconciler.cs:901-905). Bounded: an effect-driven setState loop must not spin
	// forever here — leave the residual work queued (EnsureTick is already pending) and log.
	TGuardValue<bool> ForceSync(bForceSyncPass, true);
	// Claim this reconciler's queued Slice action (M3): its work drains synchronously below,
	// and the cancelled action must not fire later against an already-quiescent tree.
	if (bSliceQueued)
	{
		CancelQueuedSlice();
		bTickPending = true; // hand the claimed work to the sync loop
	}
	// A render-phase setState cascade resolves via the MaxRenderDepth guard (26 passes),
	// so this cap must sit ABOVE it — it then only catches effect-driven loops.
	constexpr int32 MaxFlushPasses = MaxRenderDepth + 7;
	int32 Passes = 0;
	while (bTickPending || bWorkActive || !PendingPassive.IsEmpty())
	{
		if (++Passes > MaxFlushPasses)
		{
			UE_LOG(LogRuitkReconciler, Error,
				   TEXT("[Ruitk] FlushSync did not reach quiescence after %d passes (setState loop in an "
						"effect?); remaining work stays scheduled."),
				   MaxFlushPasses);
			break;
		}
		if (!PendingPassive.IsEmpty())
		{
			// A prior sliced commit parked its passive flush on the scheduler's frame-end
			// lane — flush it first; its setStates then join this drain. (The parked action
			// still fires at frame end and finds PendingPassive empty — a no-op.)
			FlushPassive();
			continue;
		}
		bTickPending = false;
		Tick();
	}
}

void FRuitkReconciler::HmrRefreshAll()
{
	if (!RootCurrent)
	{
		return;
	}
	// Every function fiber goes dirty — a definition may have been swapped under any
	// ComponentId, and props-equality bailouts would otherwise keep serving stale output.
	// ScheduleUpdateOnFiber also maintains the ancestor subtree flags + coalesced tick.
	FRuitkFiber* Fiber = RootCurrent;
	while (Fiber)
	{
		if (Fiber->State.IsValid())
		{
			ScheduleUpdateOnFiber(Fiber);
		}
		if (Fiber->Child)
		{
			Fiber = Fiber->Child;
			continue;
		}
		while (Fiber && !Fiber->Sibling)
		{
			Fiber = Fiber->Parent;
		}
		Fiber = Fiber ? Fiber->Sibling : nullptr;
	}
}

bool FRuitkReconciler::ShouldUseScheduler()
{
	// ORDER MATTERS: GetScheduler() may arm the host's frame pump (the Slate host registers
	// its PreTick seam) — it must not be touched while slicing is off, so untouched-defaults
	// behavior stays byte-equivalent.
	return FRuitkConfig::IsTimeSlicing() && !bForceSyncPass && Host.GetScheduler() != nullptr;
}

void FRuitkReconciler::EnsureWork()
{
	if (ShouldUseScheduler())
	{
		EnqueueSlice();
	}
	else
	{
		EnsureTick();
	}
}

void FRuitkReconciler::EnqueueSlice()
{
	if (bSliceQueued)
	{
		return; // one queued Slice at a time (the scheduler's key dedup mirrors this)
	}
	FRuitkScheduler* Scheduler = Host.GetScheduler();
	if (Scheduler == nullptr)
	{
		EnsureTick();
		return;
	}
	bSliceQueued = true;
	// Self-re-enqueueing Slice on the Normal lane, keyed to this reconciler (P-02/P-04,
	// FiberReconciler.cs:405-424). Two quanta can run inside one frame budget. The weak
	// token guards teardown races (the host-owned scheduler outlives us); Unmount/dtor also
	// Cancel(this) for cleanliness.
	TWeakPtr<int32> Token = LifeToken;
	Scheduler->Enqueue(
		this,
		[this, Token]()
		{
			if (Token.IsValid())
			{
				RunSlice();
			}
		},
		ERuitkLane::Normal);
}

void FRuitkReconciler::RunSlice()
{
	bSliceQueued = false;
	if (!ShouldUseScheduler())
	{
		Tick(); // slicing flipped off since the enqueue — run the synchronous shape instead
		return;
	}
	DoWork(/*bViaScheduler=*/true);
}

void FRuitkReconciler::CancelQueuedSlice()
{
	if (!bSliceQueued)
	{
		return;
	}
	bSliceQueued = false;
	if (FRuitkScheduler* Scheduler = Host.GetScheduler())
	{
		Scheduler->Cancel(this);
	}
}

void FRuitkReconciler::Tick()
{
	bTickPending = false;
	DoWork(/*bViaScheduler=*/false);
}

void FRuitkReconciler::DoWork(bool bViaScheduler)
{
	if (!RootVNode.IsSet() || RootCurrent == nullptr)
	{
		bWorkActive = false;
		return;
	}
	if (!bWorkActive)
	{
		BeginRender();
		bWorkActive = true;
	}

	// The two family axes (contract §1): the QUANTUM (ruitk.TimeSliceMs) is checked inside
	// the pass AFTER each unit — no preemption (FiberReconciler.cs:429-472); the per-frame
	// budget across lanes belongs to the SCHEDULER (PumpFrame). Host time is the clock so
	// the mock host's settable clock drives park/resume deterministically. Slicing off (or
	// FlushSync/mount): synchronous single-pass — the scheduler is bypassed entirely.
	const bool bSliced = FRuitkConfig::IsTimeSlicing() && !bForceSyncPass;
	const double QuantumSec = static_cast<double>(FRuitkConfig::TimeSliceMs()) / 1000.0;
	const double Start = Host.GetTimeSeconds();
	int32 ErrorRestarts = 0;
	while (NextUnit != nullptr)
	{
		NextUnit = PerformUnit(NextUnit);
		if (bPassPoisoned)
		{
			// A render failure activated a boundary (HandleRenderFailure): abandon the
			// poisoned WIP and rebuild from the root NOW so the fallback lands in THIS
			// commit. Bounded: a rebuild happens only when a boundary NEWLY activates
			// (active ones can't re-capture), capped for the mount-path adopt-miss loop.
			bPassPoisoned = false;
			if (++ErrorRestarts > MaxErrorRestarts)
			{
				UE_LOG(LogRuitkReconciler, Error,
					   TEXT("[Ruitk] Too many error-boundary rebuilds (%d). Abandoning the pass."), MaxErrorRestarts);
				bWorkActive = false;
				NextUnit = nullptr;
				return; // abandoned WIP is reclaimed by the next BeginRender / Unmount
			}
			BeginRender();
			continue;
		}
		if (bSliced && (Host.GetTimeSeconds() - Start) >= QuantumSec)
		{
			break; // quantum exhausted — park
		}
	}

	if (NextUnit == nullptr)
	{
		bWorkActive = false;
		CommitRoot();
	}
	else if (bViaScheduler)
	{
		EnqueueSlice(); // park: next quantum (same frame if the scheduler budget allows)
	}
	else
	{
		EnsureTick(); // park: resume on the next host frame (sliced, no scheduler)
	}
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Render phase
// ─────────────────────────────────────────────────────────────────────────────────────────

void FRuitkReconciler::BeginRender()
{
	// Reclaim an abandoned pass first (restart / aborted mount): its fresh WIP fibers would
	// otherwise leak in the slab, and shared component states could keep pointing at them.
	// (WipRoot is nulled on commit, so non-null here always means "abandoned".)
	if (WipRoot != nullptr)
	{
		ReleaseAbandonedChildren(WipRoot);
	}

	FirstEffect = nullptr;
	LastEffect = nullptr;
	Deletions.Reset();
	ReorderSet.Reset();
	// PendingPassive is NOT reset here: it fills during COMMIT (never during a render pass,
	// so an abandoned pass cannot have touched it) and, in the sliced world, survives until
	// the scheduler's frame-end batched flush — a follow-up pass beginning before that flush
	// must not wipe the still-pending effects (M3).
	// An aborted (error-restarted) pass may have left provider stacks pushed — pop them all.
	while (!ProviderStack.IsEmpty())
	{
		PopProvidedContext(ProviderStack.Last());
	}

	// Reuse the root's ping-pong buddy (double buffering) instead of allocating. [perf #1]
	FRuitkFiber* Wip = RootCurrent->Alternate;
	if (Wip == nullptr)
	{
		Wip = Slab.Acquire();
		RootCurrent->Alternate = Wip;
	}
	Wip->Alternate = RootCurrent;
	Wip->Tag = ERuitkFiberTag::Root;
	Wip->Node = RootContainer;
	Wip->Child = nullptr;
	Wip->Sibling = nullptr;
	Wip->Parent = nullptr;
	Wip->EffectTag = RuitkEffect_None;
	Wip->NextEffect = nullptr;
	Wip->bHasDeletions = false;
	Wip->InputChildren = MakeShared<const TArray<FRuitkNode>>(TArray<FRuitkNode>{RootVNode.GetValue()});
	Wip->bHasPendingUpdate = RootCurrent->bHasPendingUpdate;
	Wip->bSubtreeHasUpdates = RootCurrent->bSubtreeHasUpdates;
	WipRoot = Wip;
	NextUnit = Wip;
}

FRuitkFiber* FRuitkReconciler::PerformUnit(FRuitkFiber* Fiber)
{
	// begin-work (inlined)
	FRuitkFiber* Next = nullptr;
	switch (Fiber->Tag)
	{
	case ERuitkFiberTag::Function:
		Next = BeginFunction(Fiber);
		break;
	case ERuitkFiberTag::ErrorBoundary:
		Next = BeginErrorBoundary(Fiber);
		break;
	default:
	{
		// ROOT / HOST / FRAGMENT / PORTAL: reconcile declared children.
		// Leaf fast-path: nothing declared now AND nothing before -> skip entirely. [perf]
		FRuitkFiber* Alt = Fiber->Alternate;
		const bool bNoDeclared = !Fiber->InputChildren.IsValid() || Fiber->InputChildren->IsEmpty();
		if (bNoDeclared && (Alt == nullptr || Alt->Child == nullptr))
		{
			Next = nullptr;
		}
		else if (ReconcileChildren(Fiber, OldFirst(Alt), Fiber->InputChildren))
		{
			Next = nullptr; // fast-list handled the children in place; don't descend
		}
		else
		{
			Next = Fiber->Child;
		}
		break;
	}
	}

	if (Next != nullptr)
	{
		// Descending into children: providers push their values for the subtree (popped in
		// the ascend loop below — INCLUDING skip paths, the D-08.3 correctness trap).
		PushProvidedContext(Fiber);
		return Next;
	}

	// no child -> complete this fiber, then sibling / climb.
	FRuitkFiber* F = Fiber;
	while (F != nullptr)
	{
		CompleteWork(F);
		if (F->Sibling != nullptr)
		{
			return F->Sibling;
		}
		F = F->Parent;
	}
	return nullptr;
}

FRuitkFiber* FRuitkReconciler::BeginFunction(FRuitkFiber* Fiber)
{
	if (Fiber->State.IsValid())
	{
		Fiber->State->Fiber = Fiber; // re-point the shared state at the live fiber
	}
	FRuitkFiber* Alt = Fiber->Alternate;

	const bool bPropsEqual = PropsEqual(Fiber);
	const bool bContextOk = !Fiber->bReadsContext || !HasContextChanged(Fiber);
	const bool bChildrenSame = ChildrenSame(Alt ? Alt->InputChildren : FRuitkChildren(), Fiber->InputChildren);
	const bool bCanBail = !Fiber->bHasPendingUpdate && bContextOk && bPropsEqual && bChildrenSame;

	// ── SUBTREE-SKIP (D-08.1, React bailoutOnAlreadyFinishedWork) ──────────────────────
	// Clean fiber + clean subtree -> adopt the CURRENT child chain wholesale and skip the
	// entire subtree (no clones — React's fast path; the shared children stay validly
	// paired with their alternates for future passes).
	if (bCanBail && !Fiber->bSubtreeHasUpdates && Alt != nullptr)
	{
		if (Ruitk::TraceDiff()) // M7/P-08 diff-decision log
		{
			Ruitk::TraceEmit(
				FString::Printf(TEXT("[Ruitk][diff] Component %s: subtree-skip"), *Fiber->ComponentId.ToString()));
		}
		Fiber->Props = Fiber->PendingProps;
		Fiber->Child = Alt->Child;
		return nullptr;
	}

	FRuitkChildren OutChildren;
	if (bCanBail && Fiber->State.IsValid())
	{
		if (Ruitk::TraceDiff()) // M7/P-08 diff-decision log
		{
			Ruitk::TraceEmit(FString::Printf(TEXT("[Ruitk][diff] Component %s: bailout (children re-reconciled)"),
											 *Fiber->ComponentId.ToString()));
		}
		OutChildren = Fiber->State->LastOutput; // SAME shared list — grandchildren can bail
	}
	else
	{
		if (Ruitk::TraceDiff()) // M7/P-08 diff-decision log — the props-equal verdict, spelled out
		{
			Ruitk::TraceEmit(FString::Printf(
				TEXT(
					"[Ruitk][diff] Component %s: render (pending=%d props-equal=%d context-clean=%d children-same=%d)"),
				*Fiber->ComponentId.ToString(), Fiber->bHasPendingUpdate ? 1 : 0, bPropsEqual ? 1 : 0,
				bContextOk ? 1 : 0, bChildrenSame ? 1 : 0));
		}
		Fiber->bHasPendingUpdate = false;
		RenderComponent(Fiber);
		OutChildren = Fiber->State.IsValid() ? Fiber->State->LastOutput : FRuitkChildren();
	}
	Fiber->bSubtreeHasUpdates = false;
	Fiber->Props = Fiber->PendingProps;

	if (ReconcileChildren(Fiber, OldFirst(Alt), OutChildren))
	{
		return nullptr;
	}
	return Fiber->Child;
}

void FRuitkReconciler::RenderComponent(FRuitkFiber* Fiber)
{
	TSharedPtr<FRuitkComponentState>& State = Fiber->State;
	check(State.IsValid());

	// Runaway guard (P-11(d), FiberFunctionComponent.cs:140-155): past MaxRenderDepth
	// consecutive render-phase-update follow-ups, the component renders NOTHING for the
	// pass — no setState fires, the cascade breaks, and the quiet commit resets the ladder.
	// LogError names the component; no crash, no hang.
	if (RenderDepth > MaxRenderDepth)
	{
		UE_LOG(LogRuitkReconciler, Error,
			   TEXT("[Ruitk] Maximum render depth (%d) exceeded in '%s' — a component may be calling setState "
					"unconditionally during render. Rendering nothing for this pass."),
			   MaxRenderDepth, *Fiber->ComponentId.ToString());
		State->LastOutput = Ruitk::MakeChildren(FRuitkNodeArray());
		return;
	}

	// HMR: a live definition override for this ComponentId replaces the compiled Invoke; a
	// new generation with bResetState runs the deliberate hook-shape reset ONCE per state.
	TSharedPtr<FRuitkComponentInvoke> InvokeOverride;
	{
		Ruitk::FRuitkComponentOverride Override = Ruitk::FindComponentOverride(Fiber->ComponentId);
		if (Override.Invoke.IsValid())
		{
			InvokeOverride = Override.Invoke;
			if (State->HmrGeneration != Override.Generation)
			{
				if (Override.bResetState && State->Hooks.Num() > 0)
				{
					if (Override.bMigrateState)
					{
						// TD-019: same-shape representation swap — snapshot exportable state BEFORE the
						// reset so this render's UseState re-seeds from it. Pack by STATE-ORDINAL, not
						// absolute hook slot (bughunt HMR-1): the interp materializes only UseState cells,
						// so its i-th UseState reads MigratedState[i] by state order. A non-State slot
						// (Ref/Memo/Reducer/effect) has no interp cell and must NOT consume an index; a
						// non-exportable State cell still consumes one (as Null) to keep the rest aligned.
						State->MigratedState.Reset();
						for (int32 h = 0; h < State->Hooks.Num(); ++h)
						{
							if (!State->Hooks[h].IsValid() || State->Hooks[h]->GetKind() != ERuitkHookKind::State)
							{
								continue;
							}
							FRuitkValue Exported;
							State->MigratedState.Add(State->Hooks[h]->ExportRuitkValue(Exported) ? MoveTemp(Exported)
																								 : FRuitkValue());
						}
					}
					State->HmrResetHooks(); // reset (family rule); MigratedState survives for re-seed
				}
				State->HmrGeneration = Override.Generation;
			}
		}
	}

	auto RunOnce = [&]() -> FRuitkNodeArray
	{
		// _begin
		State->HookIndex = 0;
		State->EffectIndex = 0;
		State->LayoutIndex = 0;
		State->ContextDeps.Reset();
		State->bIsRendering = true;
		if (FRuitkConfig::IsHookValidationEnabled() || Ruitk::IsHmrHookTracking())
		{
			State->HookLog.Reset();
		}
		if (Ruitk::IsHmrHookTracking() && Ruitk::HmrGeneration() != State->HmrGenerationStamp)
		{
			// TB-17 — first render after a Live Coding patch: the code that DERIVES cached
			// values may have changed, so memo-family caches invalidate (their factories
			// re-run this render); user state (State/Ref/Reducer) stays untouched. "Preserve
			// state, recompute derivations" — the TB-15 lesson at the hook level.
			for (const TUniquePtr<IRuitkHookCell>& Cell : State->Hooks)
			{
				Cell->HmrInvalidateDerived();
			}
		}
		Ruitk::SetRendering(true);

		FRuitkContext Ctx(State.ToSharedRef(), *Fiber, *this, Host);
		const FRuitkComponentInvoke& InvokeFn = InvokeOverride.IsValid() ? *InvokeOverride : *Fiber->Invoke;
		FRuitkNodeArray Result = InvokeFn(Ctx, Fiber->PendingProps.Get(),
										  Fiber->InputChildren.IsValid() ? *Fiber->InputChildren : EmptyChildren);

		// _end
		Ruitk::SetRendering(false);
		State->bIsRendering = false;
		if (FRuitkConfig::IsHookValidationEnabled() || Ruitk::IsHmrHookTracking())
		{
			const uint32 Gen = Ruitk::HmrGeneration();
			if (!State->bHookOrderPrimed)
			{
				State->HookSignatures = State->HookLog;
				State->bHookOrderPrimed = true;
			}
			else if (Ruitk::IsHmrHookTracking() && State->HmrShapeSnapshot.Num() > 0 &&
					 State->HmrShapeSnapshot != State->HookLog && Gen != State->HmrGenerationStamp)
			{
				// TB-13 — the family rule: state is preserved on a STABLE hook shape and RESET
				// on a real shape change. The shape moved exactly across a Live-Coding patch
				// boundary (generation bump), so this is an EDIT to the hook list, not a
				// rules-of-hooks violation: dispose the cells (effect cleanups included) and
				// re-render clean instead of letting positional reads serve a neighbor's value.
				const FString Msg = FString::Printf(
					TEXT("[Ruitk][HMR] %s: hook shape changed by the edit (%d -> %d hooks) — state reset"),
					*Fiber->ComponentId.ToString(), State->HmrShapeSnapshot.Num(), State->HookLog.Num());
				FRuitkDiagnostics::Emit(Msg);
				UE_LOG(LogRuitkReconciler, Display, TEXT("%s"), *Msg);
				State->HmrResetHooks();		  // also un-primes — the re-render primes the NEW shape
				ScheduleUpdateOnFiber(Fiber); // re-render reads clean defaults
			}
			else if (FRuitkConfig::IsHookValidationEnabled())
			{
				const TArray<ERuitkHookKind>& Prev = State->HookSignatures;
				const TArray<ERuitkHookKind>& Now = State->HookLog;
				const int32 N = FMath::Min(Prev.Num(), Now.Num());
				for (int32 i = 0; i < N; ++i)
				{
					if (Prev[i] != Now[i])
					{
						const FString Msg = FString::Printf(
							TEXT("[Hooks][order] %s: hook #%d changed '%s' -> '%s' across renders — hooks must run in "
								 "the same order every render."),
							*Fiber->ComponentId.ToString(), i, RuitkHookKindName(Prev[i]), RuitkHookKindName(Now[i]));
						FRuitkDiagnostics::Emit(Msg);
						UE_LOG(LogRuitkReconciler, Error, TEXT("%s"), *Msg);
						break;
					}
				}
				if (Prev.Num() != Now.Num())
				{
					const FString Msg = FString::Printf(TEXT("[Hooks][order] %s: hook count changed %d -> %d across "
															 "renders — a hook is being called conditionally."),
														*Fiber->ComponentId.ToString(), Prev.Num(), Now.Num());
					FRuitkDiagnostics::Emit(Msg);
					UE_LOG(LogRuitkReconciler, Error, TEXT("%s"), *Msg);
				}
			}
			State->HmrGenerationStamp = Gen;
			if (Ruitk::IsHmrHookTracking())
			{
				State->HmrShapeSnapshot = State->HookLog; // the NEXT render compares against THIS one
			}
		}
		return Result;
	};

	FRuitkNodeArray Result = RunOnce();
	if (FRuitkConfig::IsStrictModeEnabled())
	{
		Result = RunOnce(); // double-invoke, first result discarded (impure-render flusher)
	}
	State->MigratedState.Reset(); // TD-019: one-shot — consumed by this render's UseState calls

	// Cooperative error latch (D-10): a failed render unwinds to the nearest boundary.
	if (TOptional<FString> Failure = Ruitk::ConsumeRenderFailure())
	{
		HandleRenderFailure(Fiber, Failure.GetValue());
		Result.Reset();
	}

	FRuitkDiagnostics::OnRender();
	State->LastOutput = Ruitk::MakeChildren(MoveTemp(Result)); // shared ONCE; bailouts reuse it
	if (!State->Effects.IsEmpty())
	{
		Fiber->EffectTag |= RuitkEffect_Passive;
	}
	if (!State->LayoutEffects.IsEmpty())
	{
		Fiber->EffectTag |= RuitkEffect_Layout;
	}
}

FRuitkFiber* FRuitkReconciler::BeginErrorBoundary(FRuitkFiber* Fiber)
{
	// A mount-pass failure recorded its activation by key-path (the WIP fiber that carried
	// it was abandoned with that pass) — re-adopt it before deciding what to render.
	if (!PendingEbActivations.IsEmpty())
	{
		AdoptPendingEbActivation(Fiber);
	}
	// Structural boundary (family): renders the fallback while active; ResetKey change
	// clears. The latch (HandleRenderFailure) is what activates it without exceptions.
	// A missing alternate is a MOUNT, not a reset — there is nothing to clear, and a
	// just-adopted mount-pass activation must survive.
	FRuitkFiber* Alt = Fiber->Alternate;
	const bool bResetRequested = (Alt != nullptr) && !(Alt->EbResetKey == Fiber->EbResetKey);
	if (bResetRequested)
	{
		Fiber->bEbActive = false;
		Fiber->EbLastError.Empty();
	}
	FRuitkChildren Children;
	if (Fiber->bEbActive && !bResetRequested)
	{
		if (Fiber->EbFallback.IsValid())
		{
			Children = MakeShared<const TArray<FRuitkNode>>(TArray<FRuitkNode>{*Fiber->EbFallback});
		}
	}
	else
	{
		Children = Fiber->EbChildren;
	}
	if (ReconcileChildren(Fiber, OldFirst(Alt), Children))
	{
		return nullptr;
	}
	return Fiber->Child;
}

void FRuitkReconciler::HandleRenderFailure(FRuitkFiber* FailedFiber, const FString& Reason)
{
	// Find the nearest boundary above the failed component (WIP chain — pre-commit, safe).
	// An ALREADY-ACTIVE boundary can't capture again this pass (its fallback is what just
	// failed) — skip upward, or the failure loops forever (React's captured-boundary rule).
	for (FRuitkFiber* F = FailedFiber; F != nullptr; F = F->Parent)
	{
		if (F->Tag == ERuitkFiberTag::ErrorBoundary && !F->bEbActive)
		{
			F->bEbActive = true;
			F->EbLastError = Reason;
			if (F->Alternate != nullptr)
			{
				// The committed twin carries the activation into the restart pass.
				F->Alternate->bEbActive = true;
				F->Alternate->EbLastError = Reason;
			}
			else
			{
				// Mount-pass boundary: the WIP fiber is abandoned with this pass and has no
				// committed twin, so record the activation by key-path for the rebuild pass
				// to re-adopt (BeginErrorBoundary). If an ancestor renders differently and
				// the path misses, the child just fails again and re-records — self-healing,
				// bounded by MaxErrorRestarts (DoWork).
				FPendingEbActivation& Pending = PendingEbActivations.AddDefaulted_GetRef();
				Pending.Reason = Reason;
				for (const FRuitkFiber* P = F; P != nullptr && P->Tag != ERuitkFiberTag::Root; P = P->Parent)
				{
					Pending.Path.Add(FiberKey(P));
				}
			}
			if (F->EbOnError)
			{
				F->EbOnError(Reason);
			}
			// Poison the pass: DoWork abandons the WIP (never committed; BeginRender
			// reclaims it) and rebuilds from the root so the boundary renders its fallback.
			// This is the ERROR path's rebuild — setState never poisons a pass (P-11).
			bPassPoisoned = true;
			UE_LOG(LogRuitkReconciler, Error, TEXT("[Ruitk] render failed: %s (caught by error boundary)"), *Reason);
			return;
		}
	}
	UE_LOG(LogRuitkReconciler, Error, TEXT("[Ruitk] render failed with no error boundary above: %s"), *Reason);
}

void FRuitkReconciler::AdoptPendingEbActivation(FRuitkFiber* BoundaryFiber)
{
	TArray<FRuitkKey> Path;
	for (const FRuitkFiber* P = BoundaryFiber; P != nullptr && P->Tag != ERuitkFiberTag::Root; P = P->Parent)
	{
		Path.Add(FiberKey(P));
	}
	for (int32 i = 0; i < PendingEbActivations.Num(); ++i)
	{
		if (PendingEbActivations[i].Path == Path)
		{
			BoundaryFiber->bEbActive = true;
			BoundaryFiber->EbLastError = PendingEbActivations[i].Reason;
			PendingEbActivations.RemoveAt(i);
			return;
		}
	}
}

void FRuitkReconciler::PushProvidedContext(FRuitkFiber* Fiber)
{
	if (!Fiber->ProvidedContext.IsValid() || Fiber->ProvidedContext->IsEmpty())
	{
		return;
	}
	for (const TPair<const void*, TSharedPtr<void>>& Pair : *Fiber->ProvidedContext)
	{
		static_cast<IRuitkProvidedValue*>(Pair.Value.Get())->PushOnRenderStack();
	}
	ProviderStack.Push(Fiber);
}

void FRuitkReconciler::PopProvidedContext(FRuitkFiber* Fiber)
{
	if (!ProviderStack.IsEmpty() && ProviderStack.Last() == Fiber)
	{
		for (const TPair<const void*, TSharedPtr<void>>& Pair : *Fiber->ProvidedContext)
		{
			static_cast<IRuitkProvidedValue*>(Pair.Value.Get())->PopFromRenderStack();
		}
		ProviderStack.Pop(EAllowShrinking::No);
	}
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Child reconciliation
// ─────────────────────────────────────────────────────────────────────────────────────────

FRuitkFiber* FRuitkReconciler::ReconcileFiber(FRuitkFiber* ParentFiber, FRuitkFiber* OldFiber, const FRuitkNode& VNode,
											  int32 Index)
{
	const bool bReuse = (OldFiber != nullptr) && OldFiber->Matches(VNode);
	FRuitkFiber* Fiber;
	if (bReuse)
	{
		Fiber = OldFiber->Alternate;
		if (Fiber == nullptr)
		{
			Fiber = Slab.Acquire();
		}
		Fiber->Alternate = OldFiber;
		OldFiber->Alternate = Fiber;
	}
	else
	{
		Fiber = Slab.Acquire();
		Fiber->Alternate = nullptr;
		if (OldFiber != nullptr)
		{
			// Structural trace (M7/P-08): the replacement decision — same slot, Matches false,
			// so the old subtree is torn down and a fresh one is built (delete + place follow).
			if (Ruitk::TraceStructural())
			{
				Ruitk::TraceEmit(Ruitk::TraceDetail()
									 ? FString::Printf(TEXT("[Ruitk][trace] Replace %s -> %s"),
													   *TraceFiberLabel(OldFiber), *TraceVNodeLabel(VNode))
									 : FString(TEXT("[Ruitk][trace] Replace")));
			}
			DeleteFiber(ParentFiber, OldFiber);
		}
	}

	// --- render-scoped fields (reset every render — the buddy holds stale data) ---
	Fiber->Parent = ParentFiber;
	Fiber->Child = nullptr;
	Fiber->Sibling = nullptr;
	Fiber->Index = Index;
	Fiber->EffectTag = RuitkEffect_None;
	Fiber->NextEffect = nullptr;
	Fiber->bHasDeletions = false;
	Fiber->Key = VNode.Key;
	Fiber->PendingProps = VNode.Props;
	Fiber->InputChildren = VNode.Children;
	Fiber->Tag = FRuitkFiber::TagForNode(VNode);
	if (VNode.Kind == ERuitkNodeKind::Host)
	{
		Fiber->ElementType = VNode.ElementType;
		Fiber->ComponentId = NAME_None;
		Fiber->Invoke.Reset();
		Fiber->PortalTarget.Reset();
	}
	else
	{
		Fiber->ElementType = FRuitkElementTypeId();
		Fiber->ComponentId = (VNode.Kind == ERuitkNodeKind::Function) ? VNode.ComponentId : NAME_None;
		Fiber->Invoke = (VNode.Kind == ERuitkNodeKind::Function) ? VNode.Invoke : nullptr;
		Fiber->PortalTarget = (VNode.Kind == ERuitkNodeKind::Portal) ? VNode.PortalTarget : nullptr;
	}
	if (VNode.Kind == ERuitkNodeKind::ErrorBoundary)
	{
		Fiber->EbFallback = VNode.EbFallback;
		Fiber->EbOnError = VNode.EbOnError;
		Fiber->EbResetKey = VNode.EbResetKey;
		Fiber->EbChildren = VNode.Children;
	}

	if (bReuse)
	{
		// carry committed baseline + live node/state/context from the current fiber
		Fiber->Node = OldFiber->Node;
		Fiber->State = OldFiber->State;
		Fiber->Props = OldFiber->Props;
		Fiber->bReadsContext = OldFiber->bReadsContext;
		Fiber->bHasPendingUpdate = OldFiber->bHasPendingUpdate;
		Fiber->bSubtreeHasUpdates = OldFiber->bSubtreeHasUpdates;
		// Carry provided context (DUPLICATED so provider change-detection vs the alternate
		// works, and a bailed-out provider keeps providing). [audit C1]
		if (OldFiber->ProvidedContext.IsValid())
		{
			TSharedRef<TMap<const void*, TSharedPtr<void>>> Dup = MakeShared<TMap<const void*, TSharedPtr<void>>>();
			for (const TPair<const void*, TSharedPtr<void>>& Pair : *OldFiber->ProvidedContext)
			{
				Dup->Add(Pair.Key, static_cast<IRuitkProvidedValue*>(Pair.Value.Get())->Duplicate());
			}
			Fiber->ProvidedContext = Dup;
		}
		else
		{
			Fiber->ProvidedContext.Reset();
		}
		if (Fiber->Tag == ERuitkFiberTag::ErrorBoundary)
		{
			Fiber->bEbActive = OldFiber->bEbActive;
			Fiber->EbLastError = OldFiber->EbLastError;
		}
	}
	else
	{
		Fiber->Node.Reset();
		Fiber->State.Reset();
		Fiber->Props.Reset();
		Fiber->bReadsContext = false;
		Fiber->bHasPendingUpdate = false;
		Fiber->bSubtreeHasUpdates = false;
		Fiber->ProvidedContext.Reset();
		Fiber->bEbActive = false;
	}

	if (Fiber->Tag == ERuitkFiberTag::Function && !Fiber->State.IsValid())
	{
		TSharedRef<FRuitkComponentState> NewState = MakeShared<FRuitkComponentState>();
		NewState->Fiber = Fiber;
		TWeakPtr<FRuitkComponentState> Weak = NewState;
		NewState->OnStateUpdated = [this, Weak]()
		{
			TSharedPtr<FRuitkComponentState> S = Weak.Pin();
			if (S.IsValid())
			{
				ScheduleUpdateOnFiber(S->Fiber);
			}
		};
		Fiber->State = NewState;
	}
	return Fiber;
}

bool FRuitkReconciler::ReconcileChildren(FRuitkFiber* ParentFiber, FRuitkFiber* OldFirstFiber,
										 const FRuitkChildren& ChildVNodes)
{
	const TArray<FRuitkNode>& VNodes = NormalizedChildren(ChildVNodes);

	// FAST-LIST PATH (+ GO-09 reuse_by_slot; see TryFastLeafList).
	const bool bReuseBySlot = ParentFiber->PendingProps.IsValid() && ParentFiber->PendingProps->bReuseBySlot;
	if (OldFirstFiber != nullptr && !VNodes.IsEmpty() &&
		TryFastLeafList(ParentFiber, OldFirstFiber, VNodes, bReuseBySlot))
	{
		if (Ruitk::TraceDiff()) // M7/P-08 diff-decision log — child-reconciliation tier
		{
			Ruitk::TraceEmit(FString::Printf(TEXT("[Ruitk][diff] Children fast-leaf (%d)"), VNodes.Num()));
		}
		return true;
	}

	ParentFiber->Child = nullptr;
	if (VNodes.IsEmpty())
	{
		FRuitkFiber* Oc = OldFirstFiber;
		while (Oc != nullptr)
		{
			FRuitkFiber* Nxt = Oc->Sibling;
			DeleteFiber(ParentFiber, Oc);
			Oc = Nxt;
		}
		return false;
	}

	FRuitkFiber* Prev = nullptr;
	bool bStructural = false;
	if (AnyKeyed(VNodes))
	{
		// FAST PATH: positionally-stable keyed list — no key map. [perf P2]
		if (KeysStable(OldFirstFiber, VNodes))
		{
			if (Ruitk::TraceDiff()) // M7/P-08 diff-decision log — child-reconciliation tier
			{
				Ruitk::TraceEmit(FString::Printf(TEXT("[Ruitk][diff] Children keys-stable (%d)"), VNodes.Num()));
			}
			FRuitkFiber* Ocs = OldFirstFiber;
			for (int32 i = 0; i < VNodes.Num(); ++i)
			{
				FRuitkFiber* Cf = ReconcileFiber(ParentFiber, Ocs, VNodes[i], i);
				if (Prev == nullptr)
				{
					ParentFiber->Child = Cf;
				}
				else
				{
					Prev->Sibling = Cf;
				}
				Prev = Cf;
				Ocs = Ocs->Sibling;
			}
			return false; // stable -> no structural change -> no reorder
		}
		// Full keyed mark-and-sweep with the persistent key map (GO-08). Unkeyed children
		// get NAMESPACED positional keys (FRuitkKey int with a reserved marker cannot collide
		// with user keys because user int keys and positional keys live in the same space —
		// so positional sentinels use FName "\x01idx%d"-style names, family [audit M1]).
		if (Ruitk::TraceDiff()) // M7/P-08 diff-decision log — child-reconciliation tier
		{
			Ruitk::TraceEmit(FString::Printf(TEXT("[Ruitk][diff] Children full-keyed (%d)"), VNodes.Num()));
		}
		KeyMap.Reset();
		FRuitkFiber* Ock = OldFirstFiber;
		while (Ock != nullptr)
		{
			KeyMap.Add(FiberKey(Ock), Ock);
			Ock->bMatchedPass = false;
			Ock = Ock->Sibling;
		}
		for (int32 i = 0; i < VNodes.Num(); ++i)
		{
			const FRuitkNode& Vn = VNodes[i];
			FRuitkFiber* OldMatch = KeyMap.FindRef(VNodeKey(Vn, i));
			if (OldMatch != nullptr && (OldMatch->bMatchedPass || !OldMatch->Matches(Vn)))
			{
				OldMatch = nullptr;
			}
			if (OldMatch != nullptr)
			{
				OldMatch->bMatchedPass = true;
				if (OldMatch->Index != i)
				{
					bStructural = true; // moved
				}
			}
			else
			{
				bStructural = true; // new placement
			}
			FRuitkFiber* Cf = ReconcileFiber(ParentFiber, OldMatch, Vn, i);
			if (Prev == nullptr)
			{
				ParentFiber->Child = Cf;
			}
			else
			{
				Prev->Sibling = Cf;
			}
			Prev = Cf;
		}
		FRuitkFiber* Ocd = OldFirstFiber;
		while (Ocd != nullptr)
		{
			FRuitkFiber* Nxtd = Ocd->Sibling;
			if (!Ocd->bMatchedPass)
			{
				DeleteFiber(ParentFiber, Ocd);
			}
			Ocd = Nxtd;
		}
		KeyMap.Reset();
	}
	else
	{
		// index (positional) path
		if (Ruitk::TraceDiff()) // M7/P-08 diff-decision log — child-reconciliation tier
		{
			Ruitk::TraceEmit(FString::Printf(TEXT("[Ruitk][diff] Children positional (%d)"), VNodes.Num()));
		}
		FRuitkFiber* Oci = OldFirstFiber;
		for (int32 i = 0; i < VNodes.Num(); ++i)
		{
			FRuitkFiber* OldMatch = Oci;
			if (OldMatch == nullptr || !OldMatch->Matches(VNodes[i]))
			{
				bStructural = true;
			}
			FRuitkFiber* Cf = ReconcileFiber(ParentFiber, OldMatch, VNodes[i], i);
			if (Prev == nullptr)
			{
				ParentFiber->Child = Cf;
			}
			else
			{
				Prev->Sibling = Cf;
			}
			Prev = Cf;
			if (Oci != nullptr)
			{
				Oci = Oci->Sibling;
			}
		}
		while (Oci != nullptr)
		{
			FRuitkFiber* Nxti = Oci->Sibling;
			DeleteFiber(ParentFiber, Oci);
			Oci = Nxti;
		}
	}

	if (bStructural)
	{
		MarkReorder(ParentFiber); // only when the SET changed [perf #2]
	}
	return false;
}

bool FRuitkReconciler::TryFastLeafList(FRuitkFiber* ParentFiber, FRuitkFiber* OldFirstFiber,
									   const TArray<FRuitkNode>& VNodes, bool bIgnoreKeys)
{
	const int32 N = VNodes.Num();
	// 1. Eligibility scan (read-only).
	FRuitkFiber* Oc = OldFirstFiber;
	for (int32 i = 0; i < N; ++i)
	{
		if (Oc == nullptr)
		{
			return false;
		}
		const FRuitkNode& Vn = VNodes[i];
		if (Vn.Kind != ERuitkNodeKind::Host || Oc->Tag != ERuitkFiberTag::Host ||
			!(Oc->ElementType == Vn.ElementType) || (!bIgnoreKeys && !(Oc->Key == Vn.Key)))
		{
			return false;
		}
		if (Oc->Child != nullptr || Vn.NumChildren() != 0)
		{
			return false; // leaves on both sides only
		}
		Oc = Oc->Sibling;
	}
	if (Oc != nullptr)
	{
		return false; // old list longer -> count changed
	}
	// 2. Reconcile in place (no buddy swap, no per-child begin/complete).
	ParentFiber->Child = OldFirstFiber;
	Oc = OldFirstFiber;
	for (int32 i = 0; i < N; ++i)
	{
		const FRuitkNode& Vn = VNodes[i];
		if (bIgnoreKeys)
		{
			Oc->Key = Vn.Key; // adopt the new key so the slot stays fast next frame
		}
		Oc->Parent = ParentFiber;
		Oc->Index = i;
		Oc->EffectTag = RuitkEffect_None;
		Oc->NextEffect = nullptr;
		Oc->InputChildren = Vn.Children;
		const TSharedPtr<const FRuitkPropsBase>& Np = Vn.Props;
		Oc->PendingProps = Np;
		const bool bChanged = (Np != Oc->Props) && !(Np.IsValid() && Oc->Props.IsValid() && Np->Equals(*Oc->Props));
		if (bChanged)
		{
			Oc->EffectTag = RuitkEffect_Update;
			AppendEffect(Oc);
		}
		Oc = Oc->Sibling;
	}
	return true;
}

bool FRuitkReconciler::KeysStable(FRuitkFiber* OldFirstFiber, const TArray<FRuitkNode>& VNodes) const
{
	FRuitkFiber* Oc = OldFirstFiber;
	for (int32 i = 0; i < VNodes.Num(); ++i)
	{
		if (Oc == nullptr)
		{
			return false;
		}
		const FRuitkNode& Vn = VNodes[i];
		if (Oc->Key.IsSet() || Vn.Key.IsSet())
		{
			if (!(Oc->Key == Vn.Key))
			{
				return false;
			}
		}
		else if (Oc->Index != i)
		{
			return false; // both unkeyed -> must be the same position
		}
		if (!Oc->Matches(Vn))
		{
			return false;
		}
		Oc = Oc->Sibling;
	}
	return Oc == nullptr; // exactly the same length
}

void FRuitkReconciler::DeleteFiber(FRuitkFiber* ParentFiber, FRuitkFiber* OldFiber)
{
	OldFiber->EffectTag |= RuitkEffect_Deletion;
	ParentFiber->bHasDeletions = true;
	Deletions.Add(OldFiber);
	MarkReorder(ParentFiber);
}

void FRuitkReconciler::MarkReorder(FRuitkFiber* ParentFiber)
{
	for (FRuitkFiber* F = ParentFiber; F != nullptr; F = F->Parent)
	{
		if (F->IsPortal() || F->Node.IsValid())
		{
			ReorderSet.Add(F);
			return;
		}
	}
}

bool FRuitkReconciler::HasContextChanged(const FRuitkFiber* Fiber) const
{
	if (!Fiber->State.IsValid())
	{
		return false;
	}
	for (const FRuitkComponentState::FContextDep& Dep : Fiber->State->ContextDeps)
	{
		// Resolve against the COMMITTED tree via the alternate chain (Godot semantics).
		if (Dep.HasChanged && Dep.HasChanged(Fiber->Alternate ? Fiber->Alternate : Fiber))
		{
			return true;
		}
	}
	return false;
}

void FRuitkReconciler::AppendEffect(FRuitkFiber* Fiber)
{
	Fiber->NextEffect = nullptr;
	if (FirstEffect == nullptr)
	{
		FirstEffect = Fiber;
	}
	else
	{
		LastEffect->NextEffect = Fiber;
	}
	LastEffect = Fiber;
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Complete phase
// ─────────────────────────────────────────────────────────────────────────────────────────

void FRuitkReconciler::CompleteWork(FRuitkFiber* Fiber)
{
	switch (Fiber->Tag)
	{
	case ERuitkFiberTag::Host:
		if (!Fiber->Node.IsValid())
		{
			check(Fiber->PendingProps.IsValid());
			Fiber->Node = Host.CreateInstance(Fiber->ElementType, *Fiber->PendingProps);
			Fiber->Props = Fiber->PendingProps;
			Fiber->EffectTag |= RuitkEffect_Placement;
		}
		else if (!PropsEqual(Fiber))
		{
			Fiber->EffectTag |= RuitkEffect_Update;
		}
		break;
	case ERuitkFiberTag::Portal:
		if (Fiber->Alternate != nullptr && Fiber->Alternate->PortalTarget != Fiber->PortalTarget)
		{
			Fiber->EffectTag |= RuitkEffect_PortalRetarget;
			ReorderSet.Add(Fiber); // re-assert order at the new target [audit M6]
		}
		break;
	default:
		break;
	}
	if (Fiber->EffectTag != RuitkEffect_None)
	{
		AppendEffect(Fiber);
	}
	PopProvidedContext(Fiber); // providers pop on ascend — every path
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Commit phase
// ─────────────────────────────────────────────────────────────────────────────────────────

void FRuitkReconciler::CommitRoot()
{
	bIsCommitting = true;
	FRuitkDiagnostics::OnCommit();
	// Per-commit structural counts for the trace summary (M7/P-08) — trace-only bookkeeping,
	// unconditional int increments (the diagnostics counters and `stat Ruitk` stay untouched).
	TraceCommitPlacements = 0;
	TraceCommitUpdates = 0;
	TraceCommitDeletions = 0;
	++TraceCommitSeq;
	Host.OnBeforeCommit(); // focus capture fence (host mutations start here)

	for (FRuitkFiber* D : Deletions)
	{
		CommitDeletion(D);
	}
	Deletions.Reset();

	FRuitkFiber* F = FirstEffect;
	while (F != nullptr)
	{
		const uint8 Tag = F->EffectTag;
		if (Tag & RuitkEffect_Placement)
		{
			CommitPlacement(F);
		}
		if (Tag & RuitkEffect_Update)
		{
			CommitUpdate(F);
		}
		if (Tag & RuitkEffect_PortalRetarget)
		{
			CommitPortalRetarget(F);
		}
		if (Tag & RuitkEffect_Layout)
		{
			CommitLayoutEffects(F);
		}
		if (Tag & RuitkEffect_Passive)
		{
			PendingPassive.Add(F);
		}
		FRuitkFiber* Nxt = F->NextEffect;
		F->EffectTag = RuitkEffect_None;
		F->NextEffect = nullptr;
		F = Nxt;
	}
	FirstEffect = nullptr;
	LastEffect = nullptr;

	for (FRuitkFiber* Hp : ReorderSet)
	{
		EnforceChildOrder(Hp);
	}
	ReorderSet.Reset();
	Host.OnAfterCommit(); // focus restore fence (host mutations end here)

	// Structural trace (M7/P-08): the commit summary — all host mutations for this commit are
	// done at this point, so the per-kind counts are final.
	if (Ruitk::TraceStructural())
	{
		Ruitk::TraceEmit(
			FString::Printf(TEXT("[Ruitk][trace] Commit #%d: %d placement(s), %d update(s), %d deletion(s)"),
							TraceCommitSeq, TraceCommitPlacements, TraceCommitUpdates, TraceCommitDeletions));
	}

	RootCurrent = WipRoot;
	WipRoot = nullptr;			  // non-null WipRoot now always means "abandoned pass" (BeginRender reclaims)
	PendingEbActivations.Reset(); // activations that never found their boundary are stale now
	bIsCommitting = false;

	// Passive effects: direct two-pass flush in the synchronous world; in the sliced world
	// the flush rides the scheduler's frame-end batched-effects lane, UNBUDGETED (the
	// reference's post-frame passive timing — FiberReconciler.cs:830-859 /
	// RenderScheduler.cs:225-243). PendingPassive stays a member either way so ReleaseFiber
	// can scrub entries of fibers deleted before the flush fires (slab memory recycles).
	FRuitkScheduler* Scheduler = ShouldUseScheduler() ? Host.GetScheduler() : nullptr;
	if (Scheduler != nullptr && !PendingPassive.IsEmpty())
	{
		TWeakPtr<int32> Token = LifeToken;
		Scheduler->EnqueueBatchedEffect(
			[this, Token]()
			{
				if (Token.IsValid())
				{
					FlushPassive();
				}
			});
	}
	else
	{
		FlushPassive();
	}

	// Deferred replay — ONE coalesced follow-up (P-11(b), FiberReconciler.cs:884-909):
	// re-mark every queued target under the replay guard WITHOUT scheduling per item, then
	// schedule ONCE. A pass whose defers included a render-phase setState climbs the
	// RenderDepth ladder (the MaxRenderDepth guard consumes it); any quiet commit resets it.
	if (DeferredUpdates.IsEmpty())
	{
		RenderDepth = 0;
		bDeferredFromRender = false;
		return;
	}
	{
		TGuardValue<bool> Replaying(bReplayingDeferred, true);
		TArray<FRuitkFiber*> Deferred = MoveTemp(DeferredUpdates);
		DeferredUpdates.Reset();
		for (FRuitkFiber* Target : Deferred)
		{
			ScheduleUpdateInternal(Target, /*bScheduleWork=*/false);
		}
	}
	if (bDeferredFromRender)
	{
		++RenderDepth;
	}
	else
	{
		RenderDepth = 0;
	}
	bDeferredFromRender = false;
	EnsureWork();
}

void FRuitkReconciler::CommitPlacement(FRuitkFiber* Fiber)
{
	if (!Fiber->Node.IsValid())
	{
		return;
	}
	bool bViaPortal = false;
	FRuitkPortalHandle Portal;
	FRuitkHostHandle ParentNode = HostParentNode(Fiber, bViaPortal, Portal);
	// APPEND (index -1) + let the structural-frame reorder assert exact order — the family
	// model (add_child then enforce order). Fiber->Index is the index among sibling VNODES,
	// NOT the flattened host index (fragments/components collapse), so it must not be used
	// as an insertion position.
	if (bViaPortal)
	{
		Host.InsertPortalChild(Portal, Fiber->Node, INDEX_NONE);
	}
	else
	{
		Host.InsertChild(ParentNode, Fiber->Node, INDEX_NONE);
	}
	// React ref lifecycle (D-08.4): attach after placement.
	if (Fiber->PendingProps.IsValid() && Fiber->PendingProps->Ref)
	{
		Fiber->PendingProps->Ref(Fiber->Node);
	}
	FRuitkDiagnostics::OnPlacement();
	++TraceCommitPlacements;
	if (Ruitk::TraceStructural()) // M7/P-08 structural event (Basic bare; Verbose adds detail)
	{
		Ruitk::TraceEmit(Ruitk::TraceDetail() ? FString::Printf(TEXT("[Ruitk][trace] Placement %s%s"),
																*TraceFiberLabel(Fiber), *TraceKeySuffix(Fiber->Key))
											  : FString(TEXT("[Ruitk][trace] Placement")));
	}
}

void FRuitkReconciler::CommitUpdate(FRuitkFiber* Fiber)
{
	if (!Fiber->Node.IsValid() || !Fiber->PendingProps.IsValid())
	{
		return;
	}
	Host.CommitUpdate(Fiber->Node, Fiber->ElementType, Fiber->Props.Get(), *Fiber->PendingProps);
	Fiber->Props = Fiber->PendingProps;
	FRuitkDiagnostics::OnUpdate();
	++TraceCommitUpdates;
	if (Ruitk::TraceStructural()) // M7/P-08 structural event
	{
		Ruitk::TraceEmit(Ruitk::TraceDetail() ? FString::Printf(TEXT("[Ruitk][trace] Update %s%s"),
																*TraceFiberLabel(Fiber), *TraceKeySuffix(Fiber->Key))
											  : FString(TEXT("[Ruitk][trace] Update")));
	}
}

void FRuitkReconciler::CommitDeletion(FRuitkFiber* Fiber)
{
	FRuitkDiagnostics::OnDeletion();
	++TraceCommitDeletions;
	if (Ruitk::TraceStructural()) // M7/P-08 structural event — one line per removed subtree root
	{
		Ruitk::TraceEmit(Ruitk::TraceDetail() ? FString::Printf(TEXT("[Ruitk][trace] Deletion %s%s"),
																*TraceFiberLabel(Fiber), *TraceKeySuffix(Fiber->Key))
											  : FString(TEXT("[Ruitk][trace] Deletion")));
	}
	NullRefsRecursive(Fiber);
	RunCleanupsRecursive(Fiber);
	DetachPortalChildren(Fiber); // portal content lives under targets, not this subtree
	ReleaseHostNodes(Fiber);
	ReleaseFiberTree(Fiber);
}

void FRuitkReconciler::NullRefsRecursive(FRuitkFiber* Fiber)
{
	// React detach: refs cleared on unmount so callback refs never dangle. [audit C2]
	if (Fiber->Props.IsValid() && Fiber->Props->Ref)
	{
		Fiber->Props->Ref(FRuitkHostHandle());
	}
	for (FRuitkFiber* C = Fiber->Child; C != nullptr; C = C->Sibling)
	{
		NullRefsRecursive(C);
	}
}

void FRuitkReconciler::CommitPortalRetarget(FRuitkFiber* Fiber)
{
	if (!Fiber->PortalTarget.IsValid())
	{
		return;
	}
	TArray<FRuitkHostHandle> Ordered;
	CollectHostChildren(Fiber, Ordered);
	for (const FRuitkHostHandle& Child : Ordered)
	{
		// Old target detach happens host-side inside InsertPortalChild's re-parent; the
		// portal is in the reorder set (see CompleteWork) so exact order is asserted after.
		Host.InsertPortalChild(Fiber->PortalTarget, Child, INDEX_NONE);
	}
}

void FRuitkReconciler::CommitLayoutEffects(FRuitkFiber* Fiber)
{
	if (!Fiber->State.IsValid())
	{
		return;
	}
	for (FRuitkEffect& E : Fiber->State->LayoutEffects)
	{
		if (!E.bEverRan || Ruitk::DepsChanged(E.LastDeps, E.Deps))
		{
			if (E.Cleanup)
			{
				E.Cleanup();
				E.Cleanup = nullptr;
			}
			E.Cleanup = E.Factory ? E.Factory() : FRuitkEffectCleanup();
			E.LastDeps = E.Deps;
			E.bEverRan = true;
		}
	}
}

void FRuitkReconciler::FlushPassive()
{
	// Two passes across all collected fibers: every cleanup first, then every setup.
	for (FRuitkFiber* Fiber : PendingPassive)
	{
		if (!Fiber->State.IsValid())
		{
			continue;
		}
		for (FRuitkEffect& E : Fiber->State->Effects)
		{
			if ((!E.bEverRan || Ruitk::DepsChanged(E.LastDeps, E.Deps)) && E.Cleanup)
			{
				E.Cleanup();
				E.Cleanup = nullptr;
			}
		}
	}
	for (FRuitkFiber* Fiber : PendingPassive)
	{
		if (!Fiber->State.IsValid())
		{
			continue;
		}
		for (FRuitkEffect& E : Fiber->State->Effects)
		{
			if (!E.bEverRan || Ruitk::DepsChanged(E.LastDeps, E.Deps))
			{
				E.Cleanup = E.Factory ? E.Factory() : FRuitkEffectCleanup();
				E.LastDeps = E.Deps;
				E.bEverRan = true;
			}
		}
	}
	PendingPassive.Reset();
}

void FRuitkReconciler::RunCleanups(FRuitkFiber* Fiber)
{
	if (!Fiber->State.IsValid())
	{
		return;
	}
	for (TArray<FRuitkEffect>* Arr : {&Fiber->State->Effects, &Fiber->State->LayoutEffects})
	{
		for (FRuitkEffect& E : *Arr)
		{
			if (E.Cleanup)
			{
				E.Cleanup();
				E.Cleanup = nullptr;
			}
		}
	}
}

void FRuitkReconciler::RunCleanupsRecursive(FRuitkFiber* Fiber)
{
	RunCleanups(Fiber);
	for (FRuitkFiber* C = Fiber->Child; C != nullptr; C = C->Sibling)
	{
		RunCleanupsRecursive(C);
	}
	DisposeFiberState(Fiber);
}

void FRuitkReconciler::DisposeFiberState(FRuitkFiber* Fiber)
{
	if (Fiber->State.IsValid())
	{
		Fiber->State->Dispose(); // cell dtors release external subscriptions
		Fiber->State.Reset();
	}
}

void FRuitkReconciler::EnforceChildOrder(FRuitkFiber* ParentFiber)
{
	FRuitkHostHandle PNode;
	bool bPortal = false;
	if (ParentFiber->IsPortal())
	{
		PNode = ParentFiber->PortalTarget;
		bPortal = true;
	}
	else if (ParentFiber->Node.IsValid())
	{
		PNode = ParentFiber->Node;
	}
	if (!PNode.IsValid())
	{
		return;
	}
	TArray<FRuitkHostHandle> Ordered;
	CollectHostChildren(ParentFiber, Ordered);
	(void)bPortal;
	Host.ReorderChildren(PNode, Ordered);
}

void FRuitkReconciler::CollectHostChildren(FRuitkFiber* Fiber, TArray<FRuitkHostHandle>& Out) const
{
	for (FRuitkFiber* C = Fiber->Child; C != nullptr; C = C->Sibling)
	{
		if (C->Tag == ERuitkFiberTag::Portal)
		{
			continue; // portal children live under the portal target, not here
		}
		if (C->Node.IsValid())
		{
			Out.Add(C->Node);
		}
		else
		{
			CollectHostChildren(C, Out);
		}
	}
}

void FRuitkReconciler::DetachPortalChildren(FRuitkFiber* Fiber)
{
	if (Fiber->IsPortal() && Fiber->PortalTarget.IsValid())
	{
		// A portal's host children live under the TARGET — outside whatever host subtree a
		// deletion/unmount is about to release, and (for external targets) outside this tree
		// entirely. Remove them explicitly or they leak in the target panel — the
		// RemovePortalChild seam exists for exactly this (audit 2026-07-14: first Portal test
		// caught the miss). CollectHostChildren skips nested portal children; the recursion
		// below detaches those against their own targets.
		TArray<FRuitkHostHandle> PortalKids;
		CollectHostChildren(Fiber, PortalKids);
		for (const FRuitkHostHandle& Kid : PortalKids)
		{
			Host.RemovePortalChild(Fiber->PortalTarget, Kid);
		}
	}
	for (FRuitkFiber* C = Fiber->Child; C != nullptr; C = C->Sibling)
	{
		DetachPortalChildren(C);
	}
}

void FRuitkReconciler::ReleaseHostNodes(FRuitkFiber* Fiber)
{
	if (Fiber->Node.IsValid() && !Fiber->IsRoot())
	{
		const bool bChildless = (Fiber->Child == nullptr);
		Host.ReleaseInstance(Fiber->Node, Fiber->ElementType, Fiber->Props, bChildless);
		return; // the host released/pooled the whole subtree root; children released with it
	}
	for (FRuitkFiber* C = Fiber->Child; C != nullptr; C = C->Sibling)
	{
		ReleaseHostNodes(C);
	}
}

FRuitkHostHandle FRuitkReconciler::HostParentNode(FRuitkFiber* Fiber, bool& bOutViaPortal,
												  FRuitkPortalHandle& OutPortal) const
{
	bOutViaPortal = false;
	for (FRuitkFiber* P = Fiber->Parent; P != nullptr; P = P->Parent)
	{
		if (P->IsPortal() && P->PortalTarget.IsValid())
		{
			bOutViaPortal = true;
			OutPortal = P->PortalTarget;
			return nullptr;
		}
		if (P->Node.IsValid())
		{
			return P->Node;
		}
	}
	return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Context plumbing (untyped halves — typed halves live in FRuitkContext templates)
// ─────────────────────────────────────────────────────────────────────────────────────────

void FRuitkReconciler::NotifyEffectKinds(FRuitkFiber& Fiber, bool bHasPassive, bool bHasLayout)
{
	if (bHasPassive)
	{
		Fiber.EffectTag |= RuitkEffect_Passive;
	}
	if (bHasLayout)
	{
		Fiber.EffectTag |= RuitkEffect_Layout;
	}
}

const IRuitkProvidedValue* FRuitkReconciler::ResolveProvidedOnCommitted(const FRuitkFiber* From, const void* Key)
{
	for (const FRuitkFiber* F = From; F != nullptr; F = F->Parent)
	{
		if (F->ProvidedContext.IsValid())
		{
			if (const TSharedPtr<void>* Found = F->ProvidedContext->Find(Key))
			{
				return static_cast<const IRuitkProvidedValue*>(Found->Get());
			}
		}
	}
	return nullptr;
}

void FRuitkReconciler::OnProvidedValueChanged(FRuitkFiber& ProviderFiber, const void* Key)
{
	// Eager propagation over the COMMITTED subtree (Godot _propagate_context_change): mark
	// consumers of Key dirty so they re-render through bailouts; intermediate ancestors get
	// the subtree flag; stop at shadowing providers.
	FRuitkFiber* Alt = ProviderFiber.Alternate;
	if (Alt == nullptr || Alt->Child == nullptr)
	{
		return;
	}

	struct FWalker
	{
		const void* Key;
		bool Walk(FRuitkFiber* First)
		{
			bool bAny = false;
			for (FRuitkFiber* F = First; F != nullptr; F = F->Sibling)
			{
				if (F->ProvidedContext.IsValid() && F->ProvidedContext->Contains(Key))
				{
					continue; // shadowed below this provider
				}
				bool bSelfMarked = false;
				if (F->bReadsContext && F->State.IsValid())
				{
					for (const FRuitkComponentState::FContextDep& Dep : F->State->ContextDeps)
					{
						if (Dep.Key == Key)
						{
							F->bHasPendingUpdate = true;
							bSelfMarked = true;
							bAny = true;
							break;
						}
					}
				}
				if (F->Child != nullptr)
				{
					if (Walk(F->Child))
					{
						bAny = true;
						if (!bSelfMarked)
						{
							F->bSubtreeHasUpdates = true;
						}
					}
				}
			}
			return bAny;
		}
	};
	FWalker Walker{Key};
	Walker.Walk(Alt->Child);
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────────────────

bool FRuitkReconciler::AnyKeyed(const TArray<FRuitkNode>& VNodes)
{
	for (const FRuitkNode& Vn : VNodes)
	{
		if (Vn.Key.IsSet())
		{
			return true;
		}
	}
	return false;
}

FRuitkKey FRuitkReconciler::FiberKey(const FRuitkFiber* F)
{
	if (F->Key.IsSet())
	{
		return F->Key;
	}
	// Namespaced positional key: control-char-prefixed name — can never equal a user key
	// (user FName keys can't contain \x01; user int keys are a different FRuitkKey kind).
	return FRuitkKey(FName(*FString::Printf(TEXT("\x01idx%d"), F->Index)));
}

FRuitkKey FRuitkReconciler::VNodeKey(const FRuitkNode& VNode, int32 Index)
{
	if (VNode.Key.IsSet())
	{
		return VNode.Key;
	}
	return FRuitkKey(FName(*FString::Printf(TEXT("\x01idx%d"), Index)));
}

bool FRuitkReconciler::ChildrenSame(const FRuitkChildren& A, const FRuitkChildren& B)
{
	// Pointer identity of shared child lists == the family's vnode-identity children check.
	const bool bAEmpty = !A.IsValid() || A->IsEmpty();
	const bool bBEmpty = !B.IsValid() || B->IsEmpty();
	if (bAEmpty && bBEmpty)
	{
		return true;
	}
	return A == B;
}

bool FRuitkReconciler::PropsEqual(const FRuitkFiber* Fiber) const
{
	if (!Fiber->Props.IsValid())
	{
		return false; // never rendered
	}
	if (Fiber->PendingProps == Fiber->Props) // identity fast-path (memoized props) [perf P3]
	{
		return true;
	}
	if (!Fiber->PendingProps.IsValid())
	{
		return false;
	}
	if (Fiber->PendingProps->MemoEquals) // V.memo custom comparer
	{
		return Fiber->PendingProps->MemoEquals(*Fiber->Props, *Fiber->PendingProps);
	}
	return Fiber->PendingProps->Equals(*Fiber->Props);
}

const TArray<FRuitkNode>& FRuitkReconciler::NormalizedChildren(const FRuitkChildren& Children) const
{
	// The family flattened nested arrays and auto-wrapped raw strings; C++ child lists are
	// flat + homogeneous by construction (Ruitk::TextBlock is explicit), so this is a passthrough.
	return Children.IsValid() ? *Children : EmptyChildren;
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// Teardown
// ─────────────────────────────────────────────────────────────────────────────────────────

void FRuitkReconciler::ReleaseFiber(FRuitkFiber* Fiber)
{
	// The release choke point (M3): fibers are slab memory, not GC objects — queues holding
	// raw fiber pointers are scrubbed BEFORE the memory recycles. A deferred update whose
	// fiber dies but whose twin survives (abandoned-pass reclaim) REDIRECTS to the twin —
	// the flags were twin-marked at schedule time, and keeping an entry keeps the follow-up
	// wake-up. No surviving twin = the reference's deleted-fiber bail (silent).
	if (!DeferredUpdates.IsEmpty())
	{
		for (int32 i = DeferredUpdates.Num() - 1; i >= 0; --i)
		{
			if (DeferredUpdates[i] == Fiber)
			{
				if (Fiber->Alternate != nullptr)
				{
					DeferredUpdates[i] = Fiber->Alternate;
				}
				else
				{
					DeferredUpdates.RemoveAt(i);
				}
			}
		}
	}
	if (!PendingPassive.IsEmpty())
	{
		PendingPassive.Remove(Fiber); // its effects died with it (cleanups already ran)
	}
	Slab.Release(Fiber);
}

void FRuitkReconciler::ReleaseFiberTree(FRuitkFiber* Fiber)
{
	if (Fiber == nullptr)
	{
		return;
	}
	FRuitkFiber* C = Fiber->Child;
	while (C != nullptr)
	{
		FRuitkFiber* Nxt = C->Sibling;
		ReleaseFiberTree(C);
		C = Nxt;
	}
	FRuitkFiber* Alt = Fiber->Alternate;
	if (Alt != nullptr)
	{
		// Sever the pair FIRST: both twins die here, so neither may serve as a deferred-
		// update redirect target for the other (ReleaseFiber would hand out dead memory).
		Alt->Alternate = nullptr;
		Fiber->Alternate = nullptr;
	}
	ReleaseFiber(Fiber); // ResetForReuse severs everything
	if (Alt != nullptr)
	{
		// Release the buddy too (its children are buddies of ours, already handled).
		ReleaseFiber(Alt);
	}
}

void FRuitkReconciler::ReleaseAbandonedChildren(FRuitkFiber* Parent)
{
	FRuitkFiber* Child = Parent->Child;
	Parent->Child = nullptr;
	// COMMITTED-CHAIN adoption: this WIP fiber shares the committed twin's child chain
	// (same objects, not copies) — via SUBTREE-SKIP (children still parented to the twin)
	// or via the fast-leaf-list, which additionally REPARENTED the committed leaves onto
	// the WIP fiber. Keep the chain, and repair Parent back to the committed twin so the
	// committed tree stays self-consistent (ScheduleUpdateOnFiber walks Parent between
	// passes — it must never climb into a reclaimed WIP chain).
	if (Child != nullptr && Parent->Alternate != nullptr && Parent->Alternate->Child == Child)
	{
		for (FRuitkFiber* C = Child; C != nullptr; C = C->Sibling)
		{
			if (C->Parent == Parent)
			{
				C->Parent = Parent->Alternate;
			}
		}
		return;
	}
	while (Child != nullptr)
	{
		FRuitkFiber* Next = Child->Sibling;
		if (Child->Parent != Parent)
		{
			break; // defense-in-depth: any other shared-chain flavor is not ours to free
		}
		ReleaseAbandonedChildren(Child);
		if (Child->Alternate != nullptr)
		{
			// Sever the pairing from the committed side; it re-pairs fresh next pass.
			Child->Alternate->Alternate = nullptr;
		}
		if (Child->State.IsValid() && Child->State->Fiber == Child)
		{
			// A shared state must never keep pointing at a released fiber (setters would
			// misdirect ScheduleUpdateOnFiber) — repoint at the committed twin, if any.
			Child->State->Fiber = Child->Alternate;
		}
		ReleaseFiber(Child);
		Child = Next;
	}
}

void FRuitkReconciler::Unmount()
{
	if (RootCurrent == nullptr)
	{
		return;
	}
	bTickPending = false;
	CancelQueuedSlice(); // a parked Slice action must not fire against the torn-down tree
	NextUnit = nullptr;
	bWorkActive = false;
	DeferredUpdates.Reset();
	PendingPassive.Reset(); // a sliced commit's parked frame-end flush dies with the tree
	if (WipRoot != nullptr)
	{
		ReleaseAbandonedChildren(WipRoot); // an abandoned pass dies with the root
	}
	PendingEbActivations.Reset();
	for (FRuitkFiber* C = RootCurrent->Child; C != nullptr; C = C->Sibling)
	{
		NullRefsRecursive(C);
		RunCleanupsRecursive(C);
		DetachPortalChildren(C); // portal content lives under targets, not this subtree
		ReleaseHostNodes(C);
	}
	ReleaseFiberTree(RootCurrent);
	RootCurrent = nullptr;
	WipRoot = nullptr;
	NextUnit = nullptr;
	RootVNode.Reset();
}
