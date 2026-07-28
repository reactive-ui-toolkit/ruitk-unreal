// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// URuitkWorldSubsystem — the per-world mount surface with the TEARDOWN CONTRACT: every root
// mounted through it is unmounted (cleanups, refs, host widgets) when the world dies — PIE
// end, level travel, world destruction — BEFORE the world's UObjects are GC'd. "See it on
// screen in one step": GetWorld()->GetSubsystem<URuitkWorldSubsystem>()->MountNamed("Menu").

#pragma once

#include "CoreMinimal.h"
#include "RuitkNode.h"
#include "Subsystems/WorldSubsystem.h"
#include "RuitkWorldSubsystem.generated.h"

class FRuitkRoot;

UCLASS()
class RUITKUMG_API URuitkWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Mount a registered component over the game viewport. Returns a handle id (for
	 *  UnmountHandle); INDEX_NONE when the name is unknown. */
	UFUNCTION(BlueprintCallable, Category = "Reactive UI Toolkit")
	int32 MountNamed(FName ComponentName, int32 ZOrder = 10);

	/** Mount an arbitrary node (C++ callers). */
	int32 MountNode(FRuitkNode Node, int32 ZOrder = 10);

	UFUNCTION(BlueprintCallable, Category = "Reactive UI Toolkit")
	void UnmountHandle(int32 Handle);

	UFUNCTION(BlueprintCallable, Category = "Reactive UI Toolkit")
	void UnmountAll();

	int32 NumLiveRoots() const { return Roots.Num(); }

	// UWorldSubsystem
	virtual void Deinitialize() override; // the teardown contract

private:
	TMap<int32, TSharedPtr<FRuitkRoot>> Roots;
	int32 NextHandle = 1;
};
