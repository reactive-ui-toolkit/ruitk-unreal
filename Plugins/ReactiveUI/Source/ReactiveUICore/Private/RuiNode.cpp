// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiNode.h"
#include "RuiElementRegistry.h"

// Per-file static category (the LogRuiCoreHooks pattern) — NEVER a second LogRuiCore: the
// module cpp defines that one, and unity builds merge TUs (a duplicate static is C2011).
DEFINE_LOG_CATEGORY_STATIC(LogRuiCoreRegistry, Log, All);

// ─────────────────────────────────────────────────────────────────────────────────────────
// Component registry (D-05)
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace
{
	struct FRuiComponentRegistry
	{
		// Fn pointer → registered FName. Pointers may be RE-REGISTERED after Live Coding
		// relocates code (RUI_COMPONENT's static initializer runs again in the patched
		// module); the NAME is the identity, so re-pointing is exactly the desired behavior.
		TMap<void*, FName> PtrToId;
		FCriticalSection Lock;

		static FRuiComponentRegistry& Get()
		{
			static FRuiComponentRegistry Instance;
			return Instance;
		}
	};

	struct FRuiElementTypeRegistry
	{
		TMap<FName, uint16> TagToId;
		TArray<FName> IdToTag; // index = id - 1
		FCriticalSection Lock;

		static FRuiElementTypeRegistry& Get()
		{
			static FRuiElementTypeRegistry Instance;
			return Instance;
		}
	};

	struct FRuiNamedFactoryRegistry
	{
		TMap<FName, TFunction<FRuiNode()>> Factories;
		FCriticalSection Lock;

		static FRuiNamedFactoryRegistry& Get()
		{
			static FRuiNamedFactoryRegistry Instance;
			return Instance;
		}
	};
} // namespace

namespace RUI
{
	FName RegisterComponentId(void* FnPtr, FName Id)
	{
		FRuiComponentRegistry& Reg = FRuiComponentRegistry::Get();
		FScopeLock Guard(&Reg.Lock);
		Reg.PtrToId.Add(FnPtr, Id);
		return Id;
	}

	FName FindComponentId(void* FnPtr)
	{
		FRuiComponentRegistry& Reg = FRuiComponentRegistry::Get();
		FScopeLock Guard(&Reg.Lock);
		if (const FName* Found = Reg.PtrToId.Find(FnPtr))
		{
			return *Found;
		}
		return NAME_None;
	}

	bool RegisterNamedFactory(FName Name, TFunction<FRuiNode()> Factory)
	{
		FRuiNamedFactoryRegistry& Reg = FRuiNamedFactoryRegistry::Get();
		FScopeLock Guard(&Reg.Lock);
		Reg.Factories.Add(Name, MoveTemp(Factory)); // replace-on-re-register (Live Coding/HMR)
		return true;
	}

	EResolveNamed ResolveNamed(FName NameOrFqn, FName& OutKey, TArray<FName>* OutCandidates)
	{
		FRuiNamedFactoryRegistry& Reg = FRuiNamedFactoryRegistry::Get();
		FScopeLock Guard(&Reg.Lock);
		// Exact key first — a fully-qualified id always addresses one registration.
		if (Reg.Factories.Contains(NameOrFqn))
		{
			OutKey = NameOrFqn;
			return EResolveNamed::Hit;
		}
		// FILE_SCOPED_EXPORTS (FS-05): generated registrations key by the FILE-QUALIFIED name
		// (`RuiUetkx_<path>::<Name>`), but the designer edges (URuiHostWidget.ComponentName,
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
		for (const TPair<FName, TFunction<FRuiNode()>>& Pair : Reg.Factories)
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
		FRuiNamedFactoryRegistry& Reg = FRuiNamedFactoryRegistry::Get();
		FScopeLock Guard(&Reg.Lock);
		Reg.Factories.GenerateKeyArray(Out);
		Out.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
	}

	FRuiNode Named(FName Name)
	{
		FName Key;
		TArray<FName> Candidates;
		const EResolveNamed Verdict = ResolveNamed(Name, Key, &Candidates);
		if (Verdict == EResolveNamed::Ambiguous)
		{
			// Never a silent first-wins: name every candidate ONCE per ambiguous name so the
			// designer can paste the qualified id (the on-widget error text does the same).
			// Lock-guarded like every registry structure (mounts can come from async loads).
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
					   TEXT("RUI::Named('%s') is AMBIGUOUS — several files export this name; use a qualified id: %s"),
					   *Name.ToString(), *List);
			}
			return Fragment({});
		}
		TFunction<FRuiNode()> Factory;
		if (Verdict == EResolveNamed::Hit)
		{
			FRuiNamedFactoryRegistry& Reg = FRuiNamedFactoryRegistry::Get();
			FScopeLock Guard(&Reg.Lock);
			if (const TFunction<FRuiNode()>* Found = Reg.Factories.Find(Key))
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
		struct FRuiHmrRegistry
		{
			TMap<FName, uint32> HookSignatures;
			TMap<FName, FRuiComponentOverride> Overrides;
			uint32 NextGeneration = 1;
			FCriticalSection Lock;

			static FRuiHmrRegistry& Get()
			{
				static FRuiHmrRegistry Instance;
				return Instance;
			}
		};
	} // namespace

	void RegisterHookSignature(FName ComponentId, uint32 Signature)
	{
		FRuiHmrRegistry& Reg = FRuiHmrRegistry::Get();
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
		FRuiHmrRegistry& Reg = FRuiHmrRegistry::Get();
		FScopeLock Guard(&Reg.Lock);
		if (const uint32* Found = Reg.HookSignatures.Find(ComponentId))
		{
			return *Found;
		}
		return 0;
	}

	void SetComponentOverride(FName ComponentId, TSharedPtr<FRuiComponentInvoke> Invoke, bool bResetState,
							  bool bMigrateState)
	{
		FRuiHmrRegistry& Reg = FRuiHmrRegistry::Get();
		FScopeLock Guard(&Reg.Lock);
		FRuiComponentOverride& Entry = Reg.Overrides.FindOrAdd(ComponentId);
		Entry.Invoke = MoveTemp(Invoke);
		Entry.Generation = Reg.NextGeneration++;
		Entry.bResetState = bResetState;
		Entry.bMigrateState = bMigrateState;
	}

	void ClearComponentOverride(FName ComponentId)
	{
		FRuiHmrRegistry& Reg = FRuiHmrRegistry::Get();
		FScopeLock Guard(&Reg.Lock);
		Reg.Overrides.Remove(ComponentId);
	}

	FRuiComponentOverride FindComponentOverride(FName ComponentId)
	{
		FRuiHmrRegistry& Reg = FRuiHmrRegistry::Get();
		FScopeLock Guard(&Reg.Lock);
		if (const FRuiComponentOverride* Found = Reg.Overrides.Find(ComponentId))
		{
			return *Found;
		}
		return FRuiComponentOverride();
	}

	FRuiElementTypeId InternElementType(FName Tag)
	{
		FRuiElementTypeRegistry& Reg = FRuiElementTypeRegistry::Get();
		FScopeLock Guard(&Reg.Lock);
		if (const uint16* Found = Reg.TagToId.Find(Tag))
		{
			return FRuiElementTypeId{*Found};
		}
		checkf(Reg.IdToTag.Num() < MAX_uint16 - 1, TEXT("element type registry overflow"));
		Reg.IdToTag.Add(Tag);
		const uint16 NewId = static_cast<uint16>(Reg.IdToTag.Num()); // ids start at 1; 0 = invalid
		Reg.TagToId.Add(Tag, NewId);
		return FRuiElementTypeId{NewId};
	}

	FRuiElementTypeId FindElementType(FName Tag)
	{
		FRuiElementTypeRegistry& Reg = FRuiElementTypeRegistry::Get();
		FScopeLock Guard(&Reg.Lock);
		if (const uint16* Found = Reg.TagToId.Find(Tag))
		{
			return FRuiElementTypeId{*Found};
		}
		return FRuiElementTypeId{};
	}

	FName GetElementTypeName(FRuiElementTypeId Id)
	{
		FRuiElementTypeRegistry& Reg = FRuiElementTypeRegistry::Get();
		FScopeLock Guard(&Reg.Lock);
		if (Id.IsValid() && Id.Value <= Reg.IdToTag.Num())
		{
			return Reg.IdToTag[Id.Value - 1];
		}
		return NAME_None;
	}

	int32 NumElementTypes()
	{
		FRuiElementTypeRegistry& Reg = FRuiElementTypeRegistry::Get();
		FScopeLock Guard(&Reg.Lock);
		return Reg.IdToTag.Num();
	}

	// ── structural factories ──────────────────────────────────────────────────────────

	FRuiChildren MakeChildren(TArray<FRuiNode> InChildren)
	{
		if (InChildren.IsEmpty())
		{
			return nullptr;
		}
		return MakeShared<const TArray<FRuiNode>>(MoveTemp(InChildren));
	}

	FRuiNode Fragment(TArray<FRuiNode> Children, FRuiKey Key)
	{
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Fragment;
		Node.Children = MakeChildren(MoveTemp(Children));
		Node.Key = Key;
		return Node;
	}

	FRuiNode Portal(FRuiPortalHandle Target, TArray<FRuiNode> Children, FRuiKey Key)
	{
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Portal;
		Node.PortalTarget = MoveTemp(Target);
		Node.Children = MakeChildren(MoveTemp(Children));
		Node.Key = Key;
		return Node;
	}

	FRuiNode ErrorBoundary(FRuiNode Fallback, TArray<FRuiNode> Children, FRuiKey ResetKey,
						   TFunction<void(const FString&)> OnError, FRuiKey Key)
	{
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::ErrorBoundary;
		Node.EbFallback = MakeShared<FRuiNode>(MoveTemp(Fallback));
		Node.EbOnError = MoveTemp(OnError);
		Node.EbResetKey = ResetKey;
		Node.Children = MakeChildren(MoveTemp(Children));
		Node.Key = Key;
		return Node;
	}
} // namespace RUI
