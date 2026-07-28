// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuitkWorldSubsystem.h"

#include "Engine/World.h"
#include "RuitkRoot.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuitkSubsystem, Log, All);

int32 URuitkWorldSubsystem::MountNamed(FName ComponentName, int32 ZOrder)
{
	FName Resolved;
	TArray<FName> Candidates;
	switch (Ruitk::ResolveNamed(ComponentName, Resolved, &Candidates))
	{
	case Ruitk::EResolveNamed::Miss:
		UE_LOG(LogRuitkSubsystem, Error, TEXT("MountNamed: '%s' is not a registered component"),
			   *ComponentName.ToString());
		return INDEX_NONE;
	case Ruitk::EResolveNamed::Ambiguous:
	{
		// FILE_SCOPED_EXPORTS (FS-05): several files export this short name — list the
		// qualified ids so the caller can pick one; never a silent first-wins.
		FString List;
		for (const FName& C : Candidates)
		{
			List += (List.IsEmpty() ? TEXT("") : TEXT(", ")) + C.ToString();
		}
		UE_LOG(LogRuitkSubsystem, Error, TEXT("MountNamed: '%s' is ambiguous — use a qualified id: %s"),
			   *ComponentName.ToString(), *List);
		return INDEX_NONE;
	}
	default:
		break;
	}
	return MountNode(Ruitk::Named(Resolved), ZOrder);
}

int32 URuitkWorldSubsystem::MountNode(FRuitkNode Node, int32 ZOrder)
{
	// In a real game world the root parents to the viewport; worlds without one (headless
	// tests, dedicated servers) get a detached root — the teardown contract is identical.
	UWorld* World = GetWorld();
	TSharedPtr<FRuitkRoot> Root;
	if (World && World->GetGameViewport())
	{
		Root = FRuitkRoot::CreateInViewport(MoveTemp(Node), ZOrder);
	}
	else
	{
		Root = FRuitkRoot::Create(MoveTemp(Node));
	}
	Root->FlushSync();
	const int32 Handle = NextHandle++;
	Roots.Add(Handle, Root);
	return Handle;
}

void URuitkWorldSubsystem::UnmountHandle(int32 Handle)
{
	TSharedPtr<FRuitkRoot> Root;
	if (Roots.RemoveAndCopyValue(Handle, Root) && Root.IsValid())
	{
		Root->Unmount();
	}
}

void URuitkWorldSubsystem::UnmountAll()
{
	for (TPair<int32, TSharedPtr<FRuitkRoot>>& Pair : Roots)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->Unmount();
		}
	}
	Roots.Empty();
}

void URuitkWorldSubsystem::Deinitialize()
{
	// PIE end / level travel / world death: every mounted root unmounts NOW — cleanups and
	// refs run before the world's UObjects are GC'd (the family teardown contract).
	if (Roots.Num() > 0)
	{
		UE_LOG(LogRuitkSubsystem, Display, TEXT("world teardown: unmounting %d ReactiveUI root(s)"), Roots.Num());
	}
	UnmountAll();
	Super::Deinitialize();
}
