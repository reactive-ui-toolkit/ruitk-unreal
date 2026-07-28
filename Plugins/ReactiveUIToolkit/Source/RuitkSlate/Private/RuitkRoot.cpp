// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuitkRoot.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "RuitkSlateElements.h"
#include "RuitkSlateLog.h"
#include "Widgets/SWindow.h"

void SRuitkRoot::Construct(const FArguments&)
{
	// clang-format off
	ChildSlot
	[
		SAssignNew(RootPanel, SOverlay)
	];
	// clang-format on
}

TSharedRef<FRuitkRoot> FRuitkRoot::CreateDetachedInternal(FRuitkNode RootNode)
{
	TSharedRef<FRuitkRoot> Root = MakeShareable(new FRuitkRoot());
	Root->Host = MakeUnique<FRuitkSlateHost>();
	Root->Widget = SNew(SRuitkRoot);
	const FRuitkHostHandle RootHandle =
		Root->Host->WrapExternalPanel(Root->Widget->GetRootPanel(), Ruitk::Slate::OverlayType());
	Root->Reconciler = MakeUnique<FRuitkReconciler>(*Root->Host, RootHandle);
	Root->Reconciler->Render(MoveTemp(RootNode));
	return Root;
}

TSharedRef<FRuitkRoot> FRuitkRoot::Create(FRuitkNode RootNode)
{
	return CreateDetachedInternal(MoveTemp(RootNode));
}

TSharedRef<FRuitkRoot> FRuitkRoot::CreateInViewport(FRuitkNode RootNode, int32 ZOrder)
{
	TSharedRef<FRuitkRoot> Root = CreateDetachedInternal(MoveTemp(RootNode));
	UGameViewportClient* Viewport = GEngine != nullptr ? GEngine->GameViewport : nullptr;
	if (Viewport == nullptr)
	{
		UE_LOG(LogRuitkSlate, Error,
			   TEXT("[ReactiveUI] CreateInViewport: no game viewport (PIE not running?) — root left detached"));
		return Root;
	}
	Viewport->AddViewportWidgetContent(Root->GetWidget(), ZOrder);
	Root->bMountedInViewport = true;
	Root->ViewportZOrder = ZOrder;
	return Root;
}

TSharedRef<FRuitkRoot> FRuitkRoot::CreateInWindow(const TSharedRef<SWindow>& Window, FRuitkNode RootNode)
{
	TSharedRef<FRuitkRoot> Root = CreateDetachedInternal(MoveTemp(RootNode));
	Window->SetContent(Root->GetWidget());
	Root->MountedWindow = Window;
	return Root;
}

FRuitkRoot::~FRuitkRoot()
{
	Unmount();
}

void FRuitkRoot::Update(FRuitkNode RootNode)
{
	if (Reconciler.IsValid())
	{
		Reconciler->Render(MoveTemp(RootNode));
	}
}

void FRuitkRoot::FlushSync()
{
	if (Reconciler.IsValid())
	{
		Reconciler->FlushSync();
	}
}

void FRuitkRoot::Unmount()
{
	if (Reconciler.IsValid() && Reconciler->IsMounted())
	{
		Reconciler->Unmount();
	}
	if (bMountedInViewport)
	{
		bMountedInViewport = false;
		if (GEngine != nullptr && GEngine->GameViewport != nullptr && Widget.IsValid())
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(Widget.ToSharedRef());
		}
	}
	if (TSharedPtr<SWindow> Window = MountedWindow.Pin())
	{
		MountedWindow.Reset();
		Window->SetContent(SNullWidget::NullWidget);
	}
}
