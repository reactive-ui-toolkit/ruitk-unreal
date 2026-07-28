// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuitkActivatableScreen.h"

#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Engine/LocalPlayer.h"
#include "RuitkNode.h"
#include "RuitkRoot.h"
#include "Widgets/Text/STextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuiActivatable, Log, All);

namespace
{
	ERuitkInputMethod MapInputType(ECommonInputType InType)
	{
		switch (InType)
		{
		case ECommonInputType::Gamepad:
			return ERuitkInputMethod::Gamepad;
		case ECommonInputType::Touch:
			return ERuitkInputMethod::Touch;
		default:
			return ERuitkInputMethod::MouseAndKeyboard;
		}
	}
} // namespace

URuitkActivatableScreen::URuitkActivatableScreen(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// TD-029: the screen must be focusable so CommonUI's SetFocus on GetDesiredFocusTarget()
	// (which returns THIS widget when the tree designated a target) actually lands here —
	// NativeOnFocusReceived then forwards to the designated Slate widget.
	SetIsFocusable(true);
	FocusRegistry = MakeShared<FRuitkFocusTargetRegistry>();
}

FRuitkNode URuitkActivatableScreen::BuildTree() const
{
	TArray<FRuitkNode> Children;
	FName Resolved;
	TArray<FName> Candidates;
	if (!ComponentName.IsNone())
	{
		switch (Ruitk::ResolveNamed(ComponentName, Resolved, &Candidates))
		{
		case Ruitk::EResolveNamed::Hit:
			Children.Add(Ruitk::Named(Resolved));
			break;
		case Ruitk::EResolveNamed::Ambiguous:
		{
			// FILE_SCOPED_EXPORTS (FS-05): never a silent first-wins — name the qualified ids.
			FString List;
			for (const FName& C : Candidates)
			{
				List += (List.IsEmpty() ? TEXT("") : TEXT(", ")) + C.ToString();
			}
			UE_LOG(LogRuiActivatable, Error, TEXT("ActivatableScreen: '%s' is ambiguous — use a qualified id: %s"),
				   *ComponentName.ToString(), *List);
			break;
		}
		default:
			break;
		}
	}
	// Activation state outside, focus registry inside — components read both from context.
	return Ruitk::CommonUI::ActivationProvider(State,
											 {Ruitk::CommonUI::FocusTargetProvider(FocusRegistry, MoveTemp(Children))});
}

TSharedRef<SWidget> URuitkActivatableScreen::RebuildWidget()
{
	if (IsDesignTime())
	{
		return SNew(STextBlock)
			.Text(FText::Format(NSLOCTEXT("ReactiveUI", "ActivatableDesignTime", "[ReactiveUI screen: {0}]"),
								FText::FromName(ComponentName.IsNone() ? FName(TEXT("<unset>")) : ComponentName)));
	}
	RefreshInputMethod();
	Root = FRuitkRoot::Create(BuildTree());
	Root->FlushSync();
	return Root->GetWidget();
}

void URuitkActivatableScreen::NativeOnActivated()
{
	Super::NativeOnActivated();
	State.bActive = true;
	RefreshInputMethod();
	Rerender();
}

void URuitkActivatableScreen::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();
	State.bActive = false;
	Rerender();
}

void URuitkActivatableScreen::Rerender()
{
	if (Root.IsValid())
	{
		Root->Update(BuildTree());
		Root->FlushSync();
	}
}

void URuitkActivatableScreen::RefreshInputMethod()
{
	// Best-effort: read the current input device when a local player is present (a live game); in a
	// player-less context (automation) the state simply stays mouse-and-keyboard.
	if (const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(LocalPlayer))
		{
			State.InputMethod = MapInputType(Input->GetCurrentInputType());
			// Subscribe so a live mid-session device switch (mouse<->gamepad<->touch) re-renders the tree
			// — sampling only at activation left UseInputMethod consumers stale (bughunt B12). Re-point if
			// the owning player's subsystem CHANGED (split-screen reassignment) so we never keep tracking a
			// stale/dead player's subsystem (bughunt CMU-1).
			if (BoundInputSubsystem.Get() != Input)
			{
				UnbindInputMethod();
				InputMethodHandle = Input->OnInputMethodChangedNative.AddUObject(
					this, &URuitkActivatableScreen::HandleInputMethodChanged);
				BoundInputSubsystem = Input;
			}
		}
	}
}

void URuitkActivatableScreen::UnbindInputMethod()
{
	if (InputMethodHandle.IsValid())
	{
		if (UCommonInputSubsystem* Old = BoundInputSubsystem.Get())
		{
			Old->OnInputMethodChangedNative.Remove(InputMethodHandle);
		}
		InputMethodHandle.Reset();
	}
	BoundInputSubsystem.Reset();
}

UWidget* URuitkActivatableScreen::NativeGetDesiredFocusTarget() const
{
	// TD-029: with a tree-designated target, hand CommonUI this widget — the focus it sets is
	// forwarded by NativeOnFocusReceived below. Without one, defer to the base class (a BP
	// override of GetDesiredFocusTarget still wins there).
	if (HasDesiredFocusTarget())
	{
		return const_cast<URuitkActivatableScreen*>(this);
	}
	return Super::NativeGetDesiredFocusTarget();
}

FReply URuitkActivatableScreen::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
	if (HasDesiredFocusTarget())
	{
		// Forward to the designated Slate widget (focus moves off this screen widget; the
		// child's own focus events fire — no re-entry back here).
		FocusRegistry->FocusDesired();
		return FReply::Handled();
	}
	return Super::NativeOnFocusReceived(InGeometry, InFocusEvent);
}

void URuitkActivatableScreen::HandleInputMethodChanged(ECommonInputType NewInputType)
{
	const ERuitkInputMethod Mapped = MapInputType(NewInputType);
	if (Mapped != State.InputMethod)
	{
		State.InputMethod = Mapped;
		Rerender();
	}
}

void URuitkActivatableScreen::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	UnbindInputMethod(); // remove from the subsystem we actually bound to (CMU-1), not the current player's
	if (Root.IsValid())
	{
		Root->Unmount(); // cleanups run before the Slate tree is released (family teardown order)
		Root.Reset();
	}
}

#if WITH_EDITOR
const FText URuitkActivatableScreen::GetPaletteCategory()
{
	return NSLOCTEXT("ReactiveUI", "PaletteCategory", "ReactiveUI");
}
#endif
