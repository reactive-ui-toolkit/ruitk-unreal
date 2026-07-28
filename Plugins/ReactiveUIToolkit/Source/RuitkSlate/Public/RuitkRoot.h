// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// SRuitkRoot + FRuitkRoot — the mount surfaces (family: ReactiveRoot.create; hold the returned
// root for the UI's lifetime, Unmount() runs every cleanup). SRuitkRoot is a plain compound
// widget whose inner overlay hosts the reconciler's top-level children; FRuitkRoot owns the
// host + reconciler + widget and wires the three mount surfaces: detached (tests/tools),
// game viewport, and an SWindow. (The editor-tab surface ships with Phase 8's Inspector.)

#pragma once

#include "CoreMinimal.h"
#include "RuitkNode.h"
#include "RuitkReconciler.h"
#include "RuitkSlateHost.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"

class SWindow;

class RUITKSLATE_API SRuitkRoot : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRuitkRoot) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	TSharedRef<SOverlay> GetRootPanel() const { return RootPanel.ToSharedRef(); }

private:
	TSharedPtr<SOverlay> RootPanel;
};

/**
 * A mounted ReactiveUI root. Create*() renders synchronously (no empty first frame).
 * Keep the returned shared ref alive for the UI's lifetime; Unmount() (or destruction)
 * tears down: effect cleanups, refs nulled, widgets detached, fibers freed.
 */
class RUITKSLATE_API FRuitkRoot
{
public:
	/** Detached: the SRuitkRoot widget exists but is not parented anywhere (tests, tools,
	 *  callers that place GetWidget() themselves). */
	static TSharedRef<FRuitkRoot> Create(FRuitkNode RootNode);

	/** Game viewport overlay (AddViewportWidgetContent). Requires a live GameViewport —
	 *  logs an error and returns a detached root otherwise. */
	static TSharedRef<FRuitkRoot> CreateInViewport(FRuitkNode RootNode, int32 ZOrder = 10);

	/** Fill an existing SWindow's content slot. */
	static TSharedRef<FRuitkRoot> CreateInWindow(const TSharedRef<SWindow>& Window, FRuitkNode RootNode);

	~FRuitkRoot();
	FRuitkRoot(const FRuitkRoot&) = delete;
	FRuitkRoot& operator=(const FRuitkRoot&) = delete;

	/** Replace the root vnode (top-level re-render; synchronous). */
	void Update(FRuitkNode RootNode);

	/** Run any pending coalesced work NOW (tests, HMR, teardown fences). */
	void FlushSync();

	/** Full teardown. Idempotent; also detaches from the viewport/window surface. */
	void Unmount();

	bool IsMounted() const { return Reconciler.IsValid() && Reconciler->IsMounted(); }

	TSharedRef<SRuitkRoot> GetWidget() const { return Widget.ToSharedRef(); }
	FRuitkReconciler& GetReconciler() { return *Reconciler; }
	FRuitkSlateHost& GetHost() { return *Host; }

private:
	FRuitkRoot() = default;
	static TSharedRef<FRuitkRoot> CreateDetachedInternal(FRuitkNode RootNode);

	// Host must outlive the reconciler (the reconciler holds IRuitkHostConfig&); members
	// destroy in reverse declaration order, so keep this order.
	TUniquePtr<FRuitkSlateHost> Host;
	TUniquePtr<FRuitkReconciler> Reconciler;
	TSharedPtr<SRuitkRoot> Widget;

	/** Which surface we attached to (for Unmount detach). */
	TWeakPtr<SWindow> MountedWindow;
	bool bMountedInViewport = false;
	int32 ViewportZOrder = 0;
};
