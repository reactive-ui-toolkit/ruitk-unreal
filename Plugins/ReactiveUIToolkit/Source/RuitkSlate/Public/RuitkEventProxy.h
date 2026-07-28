// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// FRuitkEventProxy — the bind-once-swap-inner event seam (D-11). Slate delegates are bound
// ONCE at widget construction to this per-node proxy (CreateSP with the event's slot index
// as payload); every later render just swaps the slot's inner FRuitkCallback. No rebinding,
// and TFunction's missing operator== never matters.
//
// Handler policy (decided Phase 2 step 1, MASTER_PLAN): user callbacks are void-returning;
// FReply-shaped delegates get FReply::Handled() from the proxy. Where pass-through matters
// (mouse/key), a per-slot FReply-returning override can be installed instead — the void
// path stays the default the 15 core widgets ship on.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "RuitkTypes.h"
#include "Styling/SlateTypes.h" // ECheckBoxState
#include "Types/SlateEnums.h"	// ETextCommit

class RUITKSLATE_API FRuitkEventProxy : public TSharedFromThis<FRuitkEventProxy>
{
public:
	/** Swap the slot's inner callback (unbound FRuitkCallback clears it). Slot indices are the
	 *  props struct's event bit indices — the adapter owns the mapping. */
	void SetHandler(int32 Slot, FRuitkCallback Handler)
	{
		EnsureSlot(Slot);
		Slots[Slot].Handler = MoveTemp(Handler);
	}

	/** Install an FReply-returning override for pass-through events (rare; see policy). */
	void SetReplyHandler(int32 Slot, TFunction<FReply(const FRuitkValue&)> Handler)
	{
		EnsureSlot(Slot);
		Slots[Slot].ReplyHandler = MoveTemp(Handler);
	}

	/** Drop every inner callback (release: user closures must not outlive the node). */
	void ClearAll() { Slots.Reset(); }

	// ── bind targets (delegates bind these once, payload = slot index) ────────────────────

	FReply HandleReply(int32 Slot) { return Fire(Slot, FRuitkValue()); }
	void HandleVoid(int32 Slot) { Fire(Slot, FRuitkValue()); }
	void HandleText(const FText& Value, int32 Slot) { Fire(Slot, FRuitkValue(Value)); }
	void HandleTextCommit(const FText& Value, ETextCommit::Type, int32 Slot) { Fire(Slot, FRuitkValue(Value)); }
	void HandleChecked(ECheckBoxState Value, int32 Slot) { Fire(Slot, FRuitkValue(Value == ECheckBoxState::Checked)); }
	void HandleFloat(float Value, int32 Slot) { Fire(Slot, FRuitkValue(Value)); }
	void HandleBool(bool Value, int32 Slot) { Fire(Slot, FRuitkValue(Value)); }
	void HandleColor(FLinearColor Value, int32 Slot) { Fire(Slot, FRuitkValue(Value)); }
	void HandleColorRef(const FLinearColor& Value, int32 Slot) { Fire(Slot, FRuitkValue(Value)); }
	void HandleName(FName Value, int32 Slot) { Fire(Slot, FRuitkValue(Value)); }

private:
	struct FSlot
	{
		FRuitkCallback Handler;
		TFunction<FReply(const FRuitkValue&)> ReplyHandler;
	};

	void EnsureSlot(int32 Slot)
	{
		checkf(Slot >= 0 && Slot < 64, TEXT("Ruitk: event slot index out of range"));
		if (Slots.Num() <= Slot)
		{
			Slots.SetNum(Slot + 1);
		}
	}

	FReply Fire(int32 Slot, const FRuitkValue& Value)
	{
		if (Slots.IsValidIndex(Slot))
		{
			if (Slots[Slot].ReplyHandler)
			{
				return Slots[Slot].ReplyHandler(Value);
			}
			Slots[Slot].Handler.Execute(Value);
		}
		return FReply::Handled();
	}

	TArray<FSlot, TInlineAllocator<4>> Slots;
};
