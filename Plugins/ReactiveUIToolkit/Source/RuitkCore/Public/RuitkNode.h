// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// FRuitkNode — the virtual node: a lightweight description of a piece of UI, diffed by the
// reconciler against the persistent fiber tree. Mirrors the family's five kinds
// (HOST/FUNCTION/FRAGMENT/PORTAL/ERROR_BOUNDARY).
//
// LIFETIME (differs from the plan's original "per-frame arena" idea — superseded with
// reasoning): vnodes are NOT single-frame. The bailout path reuses a component's cached
// last render output (FRuitkComponentState::LastOutput) across frames, and committed props
// live on fibers until replaced — so both nodes and props are shared-ptr persistent, and
// only the FIBERS get slab allocation. (MASTER_PLAN Phase 1 note updated at commit.)

#pragma once

#include "CoreMinimal.h"
#include "RuitkTypes.h"
#include "RuitkPropsBase.h"

struct FRuitkNode;
class FRuitkContext;

/** A component's render function output. */
using FRuitkNodeArray = TArray<FRuitkNode>;

/**
 * Type-erased component invoker: created by Ruitk::FC from a typed free function; carries the
 * typed props and calls the function with them. The INVOKER is per-vnode; the component's
 * IDENTITY for reconciliation is the registered FName (D-05 — raw fn pointers break across
 * Live Coding relocations, so identity lives in the registry, never in the pointer).
 */
using FRuitkComponentInvoke =
	TFunction<FRuitkNodeArray(FRuitkContext&, const FRuitkPropsBase*, const TArray<FRuitkNode>&)>;

/** The five node kinds (family parity). */
enum class ERuitkNodeKind : uint8
{
	Host,
	Function,
	Fragment,
	Portal,
	ErrorBoundary,
};

/** Shared, immutable child list. SHARED (not by-value) because fibers keep referencing a
 *  node's children across frames for the bailout children-identity comparison — a value
 *  copy per fiber would be the exact allocation churn D-06 exists to avoid, and a view
 *  would dangle when a re-render replaces the cached output that owns it. Pointer equality
 *  of two child lists == the family's vnode-identity children_same check. */
using FRuitkChildren = TSharedPtr<const TArray<FRuitkNode>>;

struct RUITKCORE_API FRuitkNode
{
	ERuitkNodeKind Kind = ERuitkNodeKind::Fragment;

	/** HOST: the interned element type (adapter registry key). */
	FRuitkElementTypeId ElementType;

	/** FUNCTION: registered identity + the typed invoker. */
	FName ComponentId;
	TSharedPtr<FRuitkComponentInvoke> Invoke;

	/** Props (shared, immutable once built; pointer identity = memo fast path). */
	TSharedPtr<const FRuitkPropsBase> Props;

	/** Children (shared; null = none). */
	FRuitkChildren Children;

	FRuitkKey Key;

	/** PORTAL: opaque host target. */
	FRuitkPortalHandle PortalTarget;

	// --- ERROR_BOUNDARY fields (props-as-fields; boundaries are structural — D-10) ---
	TSharedPtr<FRuitkNode> EbFallback;
	TFunction<void(const FString&)> EbOnError;
	FRuitkKey EbResetKey;

	int32 NumChildren() const { return Children.IsValid() ? Children->Num() : 0; }

	/** Structural equality is NOT defined — nodes compare by (Kind, identity fields) inside
	 *  the reconciler only; children lists compare by POINTER (see FRuitkChildren). */
};

namespace Ruitk
{
	/** Build a shared child list (the factories' common path). */
	RUITKCORE_API FRuitkChildren MakeChildren(TArray<FRuitkNode> InChildren);
} // namespace Ruitk

// ─────────────────────────────────────────────────────────────────────────────────────────
// Component registry (D-05): stable FName identity surviving Live Coding.
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace Ruitk
{
	/** Register/refresh a component id → nothing to store beyond the name's existence; the
	 *  fn-pointer → FName map lets FC() resolve identity fast and RE-RESOLVE after Live
	 *  Coding relocates code (pointers change; names don't). */
	RUITKCORE_API FName RegisterComponentId(void* FnPtr, FName Id);

	/** The registered id for a fn pointer (NAME_None if unregistered — lambda components:
	 *  documented always-re-render semantics via a per-call unique id). */
	RUITKCORE_API FName FindComponentId(void* FnPtr);

	/** Name → zero-arg node factory (default props). Generated .uetkx code self-registers
	 *  its components here; consumers in OTHER translation units (the gallery, previews,
	 *  Phase-4 hot reload) instantiate by name — the generated wrappers themselves are
	 *  TU-local to the aggregator by design. Re-registering a name replaces the factory
	 *  (Live Coding / HMR). */
	RUITKCORE_API bool RegisterNamedFactory(FName Name, TFunction<FRuitkNode()> Factory);

	/** FILE_SCOPED_EXPORTS (FS-05): generated registrations key by the FILE-QUALIFIED id
	 *  (`RuitkUetkx_<path>::<Name>`); the designer edges speak SHORT names. Resolution: an exact
	 *  key always Hits; a short name Hits when exactly ONE registration's `::<Name>` tail
	 *  matches, Misses on none, and is AMBIGUOUS on several (the caller must qualify — never a
	 *  silent first-wins). OutCandidates (optional) collects every tail match for error text. */
	enum class EResolveNamed : uint8
	{
		Hit,
		Miss,
		Ambiguous
	};
	RUITKCORE_API EResolveNamed ResolveNamed(FName NameOrFqn, FName& OutKey, TArray<FName>* OutCandidates = nullptr);

	/** Every registered factory id, lexically sorted — the first enumeration surface (dropdown
	 *  pickers, diagnostics, tests). */
	RUITKCORE_API void GetRegisteredFactoryNames(TArray<FName>& Out);

	/** Instantiate a named component with default props (empty Fragment when unknown; an
	 *  AMBIGUOUS short name renders nothing and error-logs the qualified candidates once). */
	RUITKCORE_API FRuitkNode Named(FName Name);

	/** True when Name resolves to exactly one registration (exact or unique short-name tail). */
	RUITKCORE_API bool HasNamedFactory(FName Name);

	// ── HMR seams (the registries themselves are tiny and shipping-safe — Shipping builds
	//    simply never register anything) ─────────────────────────────────────────────────────

	/** Hook-signature ledger, FName-keyed like every identity map (FILE_SCOPED_EXPORTS: the
	 *  key is the FQN). Interp-era seam: generated code BAKES `__RUITK_HOOK_SIG` constants but
	 *  nothing self-registers them since the interpreter died (HMR v2) — live preserve-vs-reset
	 *  is decided by the reconciler's hook-shape snapshot (TB-13), not this map. Retained as a
	 *  per-identity ledger for tooling/tests (per-FILE key independence is pinned in the Driver
	 *  suite). 0 = unknown. */
	RUITKCORE_API void RegisterHookSignature(FName ComponentId, uint32 Signature);
	RUITKCORE_API uint32 FindHookSignature(FName ComponentId);

	/** TB-13 — HMR hook-shape tracking (the family rule: state preserved on a stable hook
	 *  shape, RESET on a real shape change). The editor's HMR controller arms tracking for
	 *  the session (Start/Stop) and bumps the generation on every Live-Coding patch-complete;
	 *  while armed, every render records the component's FLATTENED hook sequence, and a
	 *  sequence that changed across a generation boundary resets that component's hook state
	 *  (v1's interpreter enforced this via its AST signature; v2 detects it at render time).
	 *  A shape change WITHOUT a generation bump stays what it always was: a rules-of-hooks
	 *  user error (ruitk.HookValidation). */
	RUITKCORE_API void SetHmrHookTracking(bool bActive);
	RUITKCORE_API bool IsHmrHookTracking();
	RUITKCORE_API void BumpHmrGeneration();
	RUITKCORE_API uint32 HmrGeneration();

	/** A live definition override for a ComponentId: the reconciler invokes this INSTEAD of
	 *  the fiber's compiled Invoke. Each Set bumps the generation; bResetState additionally
	 *  disposes hook state the first time each fiber renders under the new generation (hook
	 *  shape changed). bMigrateState (TD-019) makes that reset MIGRATE exported state rather than
	 *  zero it — used for the compiled→interp representation swap where the shape is unchanged.
	 *  Clear returns the component to its compiled definition. */
	RUITKCORE_API void SetComponentOverride(FName ComponentId, TSharedPtr<FRuitkComponentInvoke> Invoke,
											bool bResetState, bool bMigrateState = false);
	RUITKCORE_API void ClearComponentOverride(FName ComponentId);

	struct FRuitkComponentOverride
	{
		TSharedPtr<FRuitkComponentInvoke> Invoke;
		uint32 Generation = 0;
		bool bResetState = false;
		bool bMigrateState = false; // TD-019: reset by MIGRATING exported state, not hard-zeroing it
	};
	/** Snapshot lookup (copy — the registry may be swapped between renders). Unset = empty
	 *  Invoke. */
	RUITKCORE_API FRuitkComponentOverride FindComponentOverride(FName ComponentId);
} // namespace Ruitk

/**
 * Declare a component's stable identity next to its definition:
 *
 *   FRuitkNodeArray Counter(FRuitkContext& Ctx, const FCounterProps& Props, const TArray<FRuitkNode>& Children);
 *   RUITK_COMPONENT(Counter)
 *
 * The .uetkx codegen emits the same macro; hand-written and generated components are
 * indistinguishable to the reconciler.
 */
#define RUITK_COMPONENT(FnName)                                                                                        \
	static const FName FnName##_RuitkId = Ruitk::RegisterComponentId((void*)&FnName, FName(TEXT(#FnName)));

// ─────────────────────────────────────────────────────────────────────────────────────────
// Node factories — the Ruitk:: builder surface's structural pieces (element builders arrive
// with the host adapters; these are the engine-blind ones).
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace Ruitk
{
	/** The shape of a typed component function. */
	template <typename TProps>
	using TRuitkComponentFn = FRuitkNodeArray (*)(FRuitkContext&, const TProps&, const TArray<FRuitkNode>&);

	/** Function component node from a typed free function (identity = registered FName). */
	template <typename TProps>
	FRuitkNode FC(TRuitkComponentFn<TProps> Fn, TProps InProps = TProps(),
				  TArray<FRuitkNode> InChildren = TArray<FRuitkNode>(), FRuitkKey InKey = FRuitkKey())
	{
		static_assert(std::is_base_of_v<FRuitkPropsBase, TProps>, "component props must derive FRuitkPropsBase");
		FRuitkNode Node;
		Node.Kind = ERuitkNodeKind::Function;
		Node.ComponentId = FindComponentId((void*)Fn);
		if (Node.ComponentId.IsNone())
		{
			// Unregistered (e.g. a local lambda-ish fn): mint a per-pointer id. Identity is
			// then stable for the process but resets across Live Coding — the documented
			// always-may-reset semantics for unregistered components.
			Node.ComponentId = RegisterComponentId((void*)Fn, FName(*FString::Printf(TEXT("__anon_%p"), (void*)Fn)));
		}
		TSharedRef<const TProps> Shared = MakeShared<const TProps>(MoveTemp(InProps));
		Node.Props = Shared;
		Node.Invoke = MakeShared<FRuitkComponentInvoke>(
			[Fn](FRuitkContext& Ctx, const FRuitkPropsBase* Props,
				 const TArray<FRuitkNode>& Children) -> FRuitkNodeArray
			{
				// Invariant: the reconciler only pairs a fiber with vnodes of the SAME
				// ComponentId, and FC always stores TProps for that id — the cast is sound.
				return Fn(Ctx, *static_cast<const TProps*>(Props), Children);
			});
		Node.Children = MakeChildren(MoveTemp(InChildren));
		Node.Key = InKey;
		return Node;
	}

	RUITKCORE_API FRuitkNode Fragment(TArray<FRuitkNode> Children, FRuitkKey Key = FRuitkKey());

	RUITKCORE_API FRuitkNode Portal(FRuitkPortalHandle Target, TArray<FRuitkNode> Children,
									FRuitkKey Key = FRuitkKey());

	/**
	 * Structural error boundary (family semantics, D-10): renders Fallback when activated —
	 * by the cooperative error latch (Ruitk::FailRender) or imperatively — and resets when
	 * ResetKey changes. Not a markup tag (family convention): an escape-hatch call.
	 */
	RUITKCORE_API FRuitkNode ErrorBoundary(FRuitkNode Fallback, TArray<FRuitkNode> Children,
										   FRuitkKey ResetKey = FRuitkKey(),
										   TFunction<void(const FString&)> OnError = nullptr,
										   FRuitkKey Key = FRuitkKey());
} // namespace Ruitk
