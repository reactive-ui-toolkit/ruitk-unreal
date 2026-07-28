// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuitkDemoGameMode.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "RuitkDemoScreens.h"
#include "RuitkDemoSupport.h"
#include "RuitkRoot.h"

void ARuitkDemoGameMode::BeginPlay()
{
	Super::BeginPlay();
	RuitkDemo::SetDemoWorld(GetWorld()); // the interop screens' UMG embeds read this, not GWorld
	Root = FRuitkRoot::CreateInViewport(RuitkDemo::GalleryRoot(), /*ZOrder*/ 10);

	// UI-friendly input: keep the cursor visible and never lock it to the viewport (PIE's
	// default game input mode captures the mouse on click — the Shift+F1 annoyance).
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		PC->bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
	}
}

void ARuitkDemoGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Root.IsValid())
	{
		Root->Unmount(); // before the world dies: cleanups + widget detach (D-17 order)
		Root.Reset();
	}
	RuitkDemo::SetDemoWorld(nullptr);
	Super::EndPlay(EndPlayReason);
}
