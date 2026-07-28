// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// The demo gallery (family pattern: one game, many example screens): a menu shell plus the
// screens ported from the Unity sibling's Samples. Mounted in PIE by ARuitkDemoGameMode;
// every entry is also mounted headlessly by Ruitk.Demos (the demos_test.gd analogue).

#pragma once

#include "CoreMinimal.h"
#include "RuitkNode.h"

namespace RuitkDemo
{
	struct FRuitkDemoEntry
	{
		FString Name;
		FRuitkNode (*Make)();
	};

	/** Every gallery screen (stable order; the shell and the Demos suite both iterate it). */
	RUITKDEMO_API const TArray<FRuitkDemoEntry>& GetGalleryEntries();

	/** The compiled component names behind the entries (the Demos suite asserts every one
	 *  registered — Ruitk::Named falls back to an empty Fragment, which must never pass). */
	RUITKDEMO_API const TArray<FName>& GetCompiledScreenNames();

	/** The gallery root vnode (menu + selected screen). */
	RUITKDEMO_API FRuitkNode GalleryRoot();
} // namespace RuitkDemo
