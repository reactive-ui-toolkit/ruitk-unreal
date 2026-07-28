// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuitkNode.h"
#include "RuitkElementRegistry.h"

// Per-file static category (the LogRuiCoreHooks pattern) — NEVER a second LogRuiCore: the
// module cpp defines that one, and unity builds merge TUs (a duplicate static is C2011).
DEFINE_LOG_CATEGORY_STATIC(LogRuiCoreRegistry, Log, All);

// ─────────────────────────────────────────────────────────────────────────────────────────
// Component registry (D-05)
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace
{
	// TB-21 — the registry-singleton lifetime rule, BOTH halves load-bearing:
	//
	// 1. NEVER a function-local `static T Instance;`. A Live-Coding patch that recompiles the
	//    holder gets its OWN fresh copy in the patch DLL (guard + storage are function-
	//    internal), so patched lookups read an EMPTY registry while the base one still holds
	//    every registration — the gallery rendered null Fragments and asserted.
	// 2. NEVER a plain namespace-scope INSTANCE either. Load-time registrations from OTHER TUs
	//    in this module (RuitkCoreElements.cpp's RUI.Suspense, RuitkRouter.cpp) run during static
	//    init, and cross-TU dynamic-init order is undefined — an unconstructed TMap is an AV
	//    at module load (caught by the battery the first time it was tried).
	//
	// The construct that satisfies both: a ZERO-INITIALIZED namespace-scope pointer (zero-init
	// precedes ALL dynamic init — order-safe) + first-touch allocation (single-threaded under
	// the loader lock; every later call sees non-null). Namespace-scope data keeps BASE
	// storage across Live-Coding patches (the TB-15-proven behavior: initializers never re-run
	// on patch), so a patched RuitkCore keeps pointing at the ORIGINAL registries.
	// Deliberately never freed: process-lifetime registries; destruction order at exit is not
	// a problem we want back.
	struct FRuitkComponentRegistry
	{
		// Fn pointer → registered FName. Pointers may be RE-REGISTERED after Live Coding
		// relocates code (RUITK_COMPONENT's static initializer runs again in the patched
		// module); the NAME is the identity, so re-pointing is exactly the desired behavior.
		TMap<void*, FName> PtrToId;
		FCriticalSection Lock;
	};
	FRuitkComponentRegistry* GComponentRegistry = nullptr;
	FRuitkComponentRegistry& ComponentRegistry()
	{
		if (GComponentRegistry == nullptr)
		{
			GComponentRegistry = new FRuitkComponentRegistry();
		}
		return *GComponentRegistry;
	}

	struct FRuitkElementTypeRegistry
	{
		TMap<FName, uint16> TagToId;
		TArray<FName> IdToTag; // index = id - 1
		FCriticalSection Lock;
	};
	FRuitkElementTypeRegistry* GElementTypeRegistry = nullptr;
	FRuitkElementTypeRegistry& ElementTypeRegistry()
	{
		if (GElementTypeRegistry == nullptr)
		{
			GElementTypeRegistry = new FRuitkElementTypeRegistry();
		}
		return *GElementTypeRegistry;
	}

	struct FRuitkNamedFactoryRegistry
	{
		TMap<FName, TFunction<FRuitkNode()>> Factories;
		FCriticalSection Lock;
	};
	FRuitkNamedFactoryRegistry* GNamedFactoryRegistry = nullptr;
	FRuitkNamedFactoryRegistry& NamedFactoryRegistry()
	{
		if (GNamedFactoryRegistry == nullptr)
		{
			GNamedFactoryRegistry = new FRuitkNamedFactoryRegistry();
		}
		return *GNamedFactoryRegistry;
	}
} // namespace

namespace Ruitk
{
	FName RegisterComponentId(void* FnPtr, FName Id)
	{
		FRuitkComponentRegistry& Reg = ComponentRegistry();
		FScopeLock Guard(&Reg.Lock);
		Reg.PtrToId.Add(FnPtr, Id);
		return Id;
	}

	FName FindComponentId(void* FnPtr)
	{
		FRuitkComponentRegistry& Reg = ComponentRegistry();
		FScopeLock Guard(&Reg.Lock);
		if (const FName* Found = Reg.PtrToId.Find(FnPtr))
		{
			return *Found;
		}
		return NAME_None;
	}

	bool RegisterNamedFactory(FName Name, TFunction<FRuitkNode()> Factory)
	{
		FRuitkNamedFactoryRegistry& Reg = NamedFactoryRegistry();
		FScopeLock Guard(&Reg.Lock);
		Reg.Factories.Add(Name, MoveTemp(Factory)); // replace-on-re-register (Live Coding/HMR)
		return true;
	}

	EResolveNamed ResolveNamed(FName NameOrFqn, FName& OutKey, TArray<FName>* OutCandidates)
	{
		FRuitkNamedFactoryRegistry& Reg = NamedFactoryRegistry();
		FScopeLock Guard(&Reg.Lock);
		// Exact key first — a fully-qualified id always addresses one registration.
		if (Reg.Factories.Contains(NameOrFqn))
		{
			OutKey = NameOrFqn;
			return EResolveNamed::Hit;
		}
		// FILE_SCOPED_EXPORTS (FS-05): generated registrations key by the FILE-QUALIFIED name
		// (`RuitkUetkx_<path>::<Name>`), but the designer edges (URuitkHostWidget.ComponentName,
		// ActivatableScreen, MountNamed, the preview) speak SHORT names. A short name resolves
		// when exactly ONE registration's `::<Short>` tail matches (case-sensitive on the tail —
		// FName's own case-insensitivity already folded the lookup key); several matches are
		// AMBIGUOUS and the caller must qualify — never a silent first-wins.
		const FString Short = NameOrFqn.ToString();
		if (Short.Contains(TEXT("::")))
		{
			return EResolveNamed::Miss; // an explicit qualification that matched nothing
		}
		const FString Tail = TEXT("::") + Short;
		FName Found = NAME_None;
		int32 Matches = 0;
		for (const TPair<FName, TFunction<FRuitkNode()>>& Pair : Reg.Factories)
		{
			if (Pair.Key.ToString().EndsWith(Tail, ESearchCase::IgnoreCase))
			{
				++Matches;
				Found = Pair.Key;
				if (OutCandidates != nullptr)
				{
					OutCandidates->Add(Pair.Key);
				}
			}
		}
		if (Matches == 1)
		{
			OutKey = Found;
			return EResolveNamed::Hit;
		}
		return Matches == 0 ? EResolveNamed::Miss : EResolveNamed::Ambiguous;
	}

	void GetRegisteredFactoryNames(TArray<FName>& Out)
	{
		FRuitkNamedFactoryRegistry& Reg = NamedFactoryRegistry();
		FScopeLock Guard(&Reg.Lock);
		Reg.Factories.GenerateKeyArray(Out);
		Out.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
	}

	FRuitkNode Named(FName Name)
	{
		FName Key;
		TArray<FName> Candidates;
		const EResolveNamed Verdict = ResolveNamed(Name, Key, &Candidates);
		if (Verdict == EResolveNamed::Ambiguous)
		{
			// Never a silent first-wins: name every candidate ONCE per ambiguous name so the
			// designer can paste the qualified id (the on-widget error text does the same).
			// Lock-guarded like every registry structure (mounts can come from async loads).
			// Namespace-scope-equivalent lifetime is not needed here: worst case after a Core
			// patch is one extra log line (TB-21 note above covers the REGISTRIES).
			static TSet<FName> ReportedAmbiguous;
			static FCriticalSection ReportedLock;
			bool bAlready = false;
			{
				FScopeLock Guard(&ReportedLock);
				ReportedAmbiguous.Add(Name, &bAlready);
			}
			if (!bAlready)
			{
				FString List;
				for (const FName& C : Candidates)
				{
					List += (List.IsEmpty() ? TEXT("") : TEXT(", ")) + C.ToString();
				}
				UE_LOG(LogRuiCoreRegistry, Error,
					   TEXT("Ruitk::Named('%s') is AMBIGUOUS — several files export this name; use a qualified id: %s"),
					   *Name.ToString(), *List);
			}
			return Fragment({});
		}
		TFunction<FRuitkNode()> Factory;
		if (Verdict == EResolveNamed::Hit)
		{
			FRuitkNamedFactoryRegistry& Reg = NamedFactoryRegistry();
			FScopeLock Guard(&Reg.Lock);
			if (const TFunction<FRuitkNode()>* Found = Reg.Factories.Find(Key))
			{
				Factory = *Found;
			}
		}
		return Factory ? Factory() : Fragment({});
	}

	bool HasNamedFactory(FName Name)
	{
		FName Key;
		return ResolveNamed(Name, Key) == EResolveNamed::Hit;
	}

	namespace
	{
		// TB-21: the pointer + first-touch idiom, like every registry (note at the top) —
		// patch-stable AND static-init-order-safe.
		struct FRuitkHmrRegistry
		{
			TMap<FName, uint32> HookSignatures;
			TMap<FName, FRuitkComponentOverride> Overrides;
			uint32 NextGeneration = 1;
			FCriticalSection Lock;
		};
		FRuitkHmrRegistry* GHmrRegistryPtr = nullptr;
		FRuitkHmrRegistry& HmrRegistry()
		{
			if (GHmrRegistryPtr == nullptr)
			{
				GHmrRegistryPtr = new FRuitkHmrRegistry();
			}
			return *GHmrRegistryPtr;
		}
	} // namespace

	void RegisterHookSignature(FName ComponentId, uint32 Signature)
	{
		FRuitkHmrRegistry& Reg = HmrRegistry();
		FScopeLock Guard(&Reg.Lock);
		Reg.HookSignatures.Add(ComponentId, Signature);
	}

	// TB-13 — session-scoped HMR hook-shape tracking (armed by the editor controller; consumed
	// by the reconciler's render tail). Atomics: read every render, written from editor events.
	static TAtomic<bool> GHmrHookTracking{false};
	static TAtomic<uint32> GHmrGeneration{0};

	void SetHmrHookTracking(bool bActive)
	{
		GHmrHookTracking = bActive;
	}

	bool IsHmrHookTracking()
	{
		return GHmrHookTracking;
	}

	void BumpHmrGeneration()
	{
		++GHmrGeneration;
	}

	uint32 HmrGeneration()
	{
		return GHmrGeneration;
	}

	uint32 FindHookSignature(FName ComponentId)
	{
		FRuitkHmrRegistry& Reg = HmrRegistry();
		FScopeLock Guard(&Reg.Lock);
		if (const uint32* Found = Reg.HookSignatures.Find(ComponentId))
		{
			return *Found;
		}
		return 0;
	}

	void SetComponentOverride(FName ComponentId, TSharedPtr<FRuitkComponentInvoke> Invoke, bool bResetState,
							  bool bMigrateState)
	{
		FRuitkHmrRegistry& Reg = HmrRegistry();
		FScopeLock Guard(&Reg.Lock);
		FRuitkComponentOverride& Entry = Reg.Overrides.FindOrAdd(ComponentId);
		Entry.Invoke = MoveTemp(Invoke);
		Entry.Generation = Reg.NextGeneration++;
		Entry.bResetState = bResetState;
		Entry.bMigrateState = bMigrateState;
	}

	void ClearComponentOverride(FName ComponentId)
	{
		FRuitkHmrRegistry& Reg = HmrRegistry();
		FScopeLock Guard(&Reg.Lock);
		Reg.Overrides.Remove(ComponentId);
	}

	FRuitkComponentOverride FindComponentOverride(FName ComponentId)
	{
		FRuitkHmrRegistry& Reg = HmrRegistry();
		FScopeLock Guard(&Reg.Lock);
		if (const FRuitkComponentOverride* Found = Reg.Overrides.Find(ComponentId))
		{
			return *Found;
		}
		return FRuitkComponentOverride();
	}

	FRuitkElementTypeId InternElementType(FName Tag)
	{
		FRuitkElementTypeRegistry& Reg = ElementTypeRegistry();
		FScopeLock Guard(&Reg.Lock);
		if (const uint16* Found = Reg.TagToId.Find(Tag))
		{
			return FRuitkElementTypeId{*Found};
		}
		checkf(Reg.IdToTag.Num() < MAX_uint16 - 1, TEXT("element type registry overflow"));
		Reg.IdToTag.Add(Tag);
		const uint16 NewId = static_cast<uint16>(Reg.IdToTag.Num()); // ids start at 1; 0 = invalid
		Reg.TagToId.Add(Tag, NewId);
		return FRuitkElementTypeId{NewId};
	}

	FRuitkElementTypeId FindElementType(FName Tag)
	{
		FRuitkElementTypeRegistry& Reg = ElementTypeRegistry();
		FScopeLock Guard(&Reg.Lock);
		if (const uint16* Found = Reg.TagToId.Find(Tag))
		{
			return FRuitkElementTypeId{*Found};
		}
		return FRuitkElementTypeId{};
	}

	FName GetElementTypeName(FRuitkElementTypeId Id)
	{
		FRuitkElementTypeRegistry& Reg = ElementTypeRegistry();
		FScopeLock Guard(&Reg.Lock);
		if (Id.IsValid() && Id.Value <= Reg.IdToTag.Num())
		{
			return Reg.IdToTag[Id.Value - 1];
		}
		return NAME_None;
	}

	int32 NumElementTypes()
	{
		FRuitkElementTypeRegistry& Reg = ElementTypeRegistry();
		FScopeLock Guard(&Reg.Lock);
		return Reg.IdToTag.Num();
	}

	// ── structural factories ──────────────────────────────────────────────────────────

	FRuitkChildren MakeChildren(TArray<FRuitkNode> InChildren)
	{
		if (InChildren.IsEmpty())
		{
			return nullptr;
		}
		return MakeShared<const TArray<FRuitkNode>>(MoveTemp(InChildren));
	}

	FRuitkNode Fragment(TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		FRuitkNode Node;
		Node.Kind = ERuitkNodeKind::Fragment;
		Node.Children = MakeChildren(MoveTemp(Children));
		Node.Key = Key;
		return Node;
	}

	FRuitkNode Portal(FRuitkPortalHandle Target, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		FRuitkNode Node;
		Node.Kind = ERuitkNodeKind::Portal;
		Node.PortalTarget = MoveTemp(Target);
		Node.Children = MakeChildren(MoveTemp(Children));
		Node.Key = Key;
		return Node;
	}

	FRuitkNode ErrorBoundary(FRuitkNode Fallback, TArray<FRuitkNode> Children, FRuitkKey ResetKey,
						   TFunction<void(const FString&)> OnError, FRuitkKey Key)
	{
		FRuitkNode Node;
		Node.Kind = ERuitkNodeKind::ErrorBoundary;
		Node.EbFallback = MakeShared<FRuitkNode>(MoveTemp(Fallback));
		Node.EbOnError = MoveTemp(OnError);
		Node.EbResetKey = ResetKey;
		Node.Children = MakeChildren(MoveTemp(Children));
		Node.Key = Key;
		return Node;
	}
} // namespace Ruitk
