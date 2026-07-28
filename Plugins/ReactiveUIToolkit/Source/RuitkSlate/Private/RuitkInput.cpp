// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuitkInput.h"

#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "RuitkContext.h"
#include "RuitkHooksInternal.h"
#include "RuitkSlateHost.h"

bool Ruitk::Slate::FRuitkShortcut::Matches(const FKeyEvent& Event) const
{
	if (Event.GetKey() != Key)
	{
		return false;
	}
	const FModifierKeysState M = Event.GetModifierKeys();
	return M.IsControlDown() == bCtrl && M.IsShiftDown() == bShift && M.IsAltDown() == bAlt &&
		   M.IsCommandDown() == bCmd;
}

int32 Ruitk::Slate::FRuitkShortcut::DepKey() const
{
	const int32 Mods = (bCtrl ? 1 : 0) | (bShift ? 2 : 0) | (bAlt ? 4 : 0) | (bCmd ? 8 : 0);
	return static_cast<int32>(HashCombine(GetTypeHash(Key), ::GetTypeHash(Mods)));
}

namespace
{
	/** Fires the box's CURRENT callback (latest closure) when the chord matches; consumes the key. */
	class FRuitkShortcutProcessor : public IInputProcessor
	{
	public:
		FRuitkShortcutProcessor(const Ruitk::Slate::FRuitkShortcut& InChord,
								TSharedRef<TRuitkRef<TFunction<void()>>> InBox)
			: Chord(InChord), Box(MoveTemp(InBox))
		{
		}

		virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}

		virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
		{
			// Fire once per physical press: ignore OS auto-repeat while the key is held (bughunt IW-4).
			// Still consume the matched chord's repeats so they don't leak to other handlers.
			if (Chord.Matches(Event) && Box->Current)
			{
				if (!Event.IsRepeat())
				{
					Box->Current();
				}
				return true;
			}
			return false;
		}

		virtual const TCHAR* GetDebugName() const override { return TEXT("RuitkShortcut"); }

	private:
		Ruitk::Slate::FRuitkShortcut Chord;
		TSharedRef<TRuitkRef<TFunction<void()>>> Box;
	};
} // namespace

void Ruitk::Slate::UseShortcut(FRuitkContext& Ctx, const FRuitkShortcut& Chord, TFunction<void()> OnTrigger)
{
	// A stable box holding the LATEST callback, refreshed each render — the pre-processor always
	// fires the current closure, so the effect re-registers only when the CHORD itself changes.
	TSharedRef<TRuitkRef<TFunction<void()>>> Box = Ctx.UseRef<TFunction<void()>>(TFunction<void()>());
	Box->Current = MoveTemp(OnTrigger);

	Ctx.UseEffect(
		[Chord, Box]() -> FRuitkEffectCleanup
		{
			if (!FSlateApplication::IsInitialized())
			{
				return FRuitkEffectCleanup();
			}
			TSharedRef<FRuitkShortcutProcessor> Proc = MakeShared<FRuitkShortcutProcessor>(Chord, Box);
			FSlateApplication::Get().RegisterInputPreProcessor(Proc);
			return [Proc]()
			{
				if (FSlateApplication::IsInitialized())
				{
					FSlateApplication::Get().UnregisterInputPreProcessor(Proc);
				}
			};
		},
		Ruitk::Deps(Chord.DepKey()));
}

// ─────────────────────────────────────────────────────────────────────────────────────────
// TD-022 — focus extensions
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace
{
	/** The SWidget behind a host handle (null-safe). */
	TSharedPtr<SWidget> WidgetOf(const FRuitkHostHandle& Handle)
	{
		if (FRuitkSlateNode* Node = FRuitkSlateHost::Resolve(Handle))
		{
			return Node->Widget;
		}
		return nullptr;
	}
} // namespace

void Ruitk::Slate::FocusWidget(const FRuitkHostHandle& Handle)
{
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}
	if (TSharedPtr<SWidget> Widget = WidgetOf(Handle))
	{
		FSlateApplication::Get().SetUserFocus(FSlateApplication::CursorUserIndex, Widget, EFocusCause::SetDirectly);
	}
}

void Ruitk::Slate::ClearFocus()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().ClearUserFocus(FSlateApplication::CursorUserIndex, EFocusCause::SetDirectly);
	}
}

Ruitk::Slate::FRuitkFocusHandle Ruitk::Slate::UseFocus(FRuitkContext& Ctx)
{
	// A stable weak box the ref keeps in sync (attach -> widget, detach -> null); Focus/IsFocused
	// read it. Ref/Focus/IsFocused capture the same box, so they stay valid for the component's life.
	TSharedRef<TRuitkRef<TWeakPtr<SWidget>>> Box = Ctx.UseRef<TWeakPtr<SWidget>>();

	FRuitkFocusHandle Handle;
	Handle.Ref = [Box](const FRuitkHostHandle& H) { Box->Current = WidgetOf(H); };
	Handle.Focus = [Box]()
	{
		if (!FSlateApplication::IsInitialized())
		{
			return;
		}
		if (TSharedPtr<SWidget> Widget = Box->Current.Pin())
		{
			FSlateApplication::Get().SetUserFocus(FSlateApplication::CursorUserIndex, Widget, EFocusCause::SetDirectly);
		}
	};
	Handle.IsFocused = [Box]() -> bool
	{
		if (!FSlateApplication::IsInitialized())
		{
			return false;
		}
		const TSharedPtr<SWidget> Widget = Box->Current.Pin();
		return Widget.IsValid() &&
			   FSlateApplication::Get().GetUserFocusedWidget(FSlateApplication::CursorUserIndex) == Widget;
	};
	return Handle;
}
