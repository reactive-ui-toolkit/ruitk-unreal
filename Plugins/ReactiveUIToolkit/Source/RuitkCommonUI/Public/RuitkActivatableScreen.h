// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-021 — "our UI as a CommonUI screen". URuitkActivatableScreen is a UCommonActivatableWidget that
// hosts a Reactive UI Toolkit component and publishes its CommonUI activation state (active? input method?)
// into the tree via ActivationProvider, so the component reacts with UseActivation/UseInputMethod.
// Push it onto a UCommonActivatableWidgetStack like any other screen; it re-renders on activation and
// input-method changes, and unmounts (running cleanups) when its Slate resources release.

#pragma once

#include "CommonActivatableWidget.h"
#include "CoreMinimal.h"
#include "RuitkActivation.h"
#include "UObject/WeakObjectPtr.h"
#include "RuitkActivatableScreen.generated.h"

class FRuitkRoot;
class UCommonInputSubsystem;
enum class ECommonInputType : uint8;

UCLASS(meta = (DisplayName = "Reactive UI Toolkit Activatable Screen"))
class RUITKCOMMONUI_API URuitkActivatableScreen : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/** The registered component to host (a compiled .uetkx component or a Ruitk::RegisterNamedFactory). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactive UI Toolkit")
	FName ComponentName;

	/** Is this screen currently active on its stack? (mirrors the state pushed into the tree). */
	UFUNCTION(BlueprintCallable, Category = "Reactive UI Toolkit")
	bool IsScreenActive() const { return State.bActive; }

	/** The activation state currently published to the hosted tree. */
	const FRuitkActivationState& GetActivationState() const { return State; }

	/** Has the hosted tree designated a desired focus target (Ruitk::CommonUI::UseDesiredFocus)? */
	bool HasDesiredFocusTarget() const { return FocusRegistry.IsValid() && FocusRegistry->HasTarget(); }

	URuitkActivatableScreen(const FObjectInitializer& ObjectInitializer);

	// UVisual declares this public — keep it so (hosts and tests drive teardown directly).
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	/** TD-029 — CommonUI's focus-restoration contract. The contract wants a UWidget but our tree
	 *  is pure Slate, so when the hosted tree designated a target (UseDesiredFocus) the screen
	 *  returns ITSELF; the focus then arrives at NativeOnFocusReceived, which forwards it to the
	 *  designated widget. No designation → the base behavior. */
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

private:
	FRuitkNode BuildTree() const;
	void Rerender();
	void RefreshInputMethod();
	/** Bound to UCommonInputSubsystem::OnInputMethodChangedNative so a live device switch re-renders. */
	void HandleInputMethodChanged(ECommonInputType NewInputType);
	/** Remove the input-method subscription from the subsystem we actually bound to (not the current
	 *  owning player's — they may differ, bughunt CMU-1). */
	void UnbindInputMethod();

	TSharedPtr<FRuitkRoot> Root;
	FRuitkActivationState State;
	/** TD-029 — owned by the screen, provided into the tree via FocusTargetProvider. */
	TSharedPtr<FRuitkFocusTargetRegistry> FocusRegistry;
	FDelegateHandle InputMethodHandle;
	/** The subsystem InputMethodHandle is registered on — tracked so an owning-player change re-points
	 *  the subscription and teardown removes it from the RIGHT subsystem (bughunt CMU-1). */
	TWeakObjectPtr<UCommonInputSubsystem> BoundInputSubsystem;
};
