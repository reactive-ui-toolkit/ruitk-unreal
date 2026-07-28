// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Typed per-element props: the D-04 decision made code. Every element/component gets a
// concrete FRuitkPropsBase subclass whose fields are declared via RUITK_PROP — which generates
// the field, its set-bit, a fluent builder setter (the D-02 public codegen contract), and
// the memberwise Equals/Diff rows. NO prop bags: diffing is flat memberwise compare-and-set,
// no FName hashing, no boxing (rejected alternatives recorded in the plan).
//
// Set-bit semantics (the family's removed-prop contract):
//   bit set   = this render SPECIFIED the field.
//   bit clear = unspecified. A field set last render but clear now is IGNORED for plain
//               fields (removed plain props don't reset — family law); event/ref/style
//               fields ARE cleaned up on removal (the host/adapter handles those).

#pragma once

#include "CoreMinimal.h"
#include "RuitkTypes.h"

/** Polymorphic base for all typed props. Props are immutable once built and shared by
 *  pointer between the vnode and the committed fiber — pointer identity IS the memo
 *  fast-path (Godot's is_same(), React's ===). */
struct RUITKCORE_API FRuitkPropsBase
{
	virtual ~FRuitkPropsBase() = default;

	/** Which fields this render specified (RUITK_PROP bit indices). */
	uint64 SetBits = 0;

	bool Has(uint32 Bit) const { return (SetBits & (1ull << Bit)) != 0; }

	/**
	 * Memberwise equality for bailout, EXCLUDING event fields (FRuitkCallback compares by
	 * inner identity — a fresh inline lambda is unequal, exactly like React; events also
	 * never force widget re-apply since they're proxy-swapped). Generated overrides call
	 * the RUITK_PROP comparison rows. A type mismatch is never equal.
	 */
	virtual bool Equals(const FRuitkPropsBase& Other) const { return SetBits == Other.SetBits; }

	// --- Reserved cross-element props (the family's reserved set, D-03). These live on the
	//     base so the reconciler/host can reach them without downcasting. ---

	/** Inline style layer (v1 styling, D-13). Null = no styles. Applied/reset by the host. */
	TSharedPtr<FRuitkStyleDict> Style;

	/** `classes` merge layer (v1 styling). */
	TArray<FName> Classes;

	/** Widget ref: receives the host handle on attach, cleared on detach (React lifecycle,
	 *  D-08.4 — a deliberate divergence from the Godot port's call-every-commit refs). */
	TFunction<void(const FRuitkHostHandle&)> Ref;

	/** GO-09 reuse_by_slot opt-in on container elements (see FRuitkReconciler fast paths). */
	bool bReuseBySlot = false;

	/** Slot-layout props (parent-owned, applied to the child's SLOT — the slot.* namespace).
	 *  Opaque to the core; the Slate host defines the keys. */
	TSharedPtr<FRuitkStyleDict> SlotProps;

	/** Optional custom memo comparer (V.memo parity). When set, bailout consults it instead
	 *  of Equals() when pointer identity fails. */
	TFunction<bool(const FRuitkPropsBase& Old, const FRuitkPropsBase& New)> MemoEquals;

protected:
	/** Shared piece of generated Equals(): the reserved/base fields that participate.
	 *  Style/Classes compare by VALUE (style dicts are rebuilt each render and equal dicts
	 *  must bail); Ref/MemoEquals are function fields → excluded (identity-less). */
	bool BaseFieldsEqual(const FRuitkPropsBase& Other) const
	{
		if (SetBits != Other.SetBits || bReuseBySlot != Other.bReuseBySlot || Classes != Other.Classes)
		{
			return false;
		}
		if (Style.IsValid() != Other.Style.IsValid())
		{
			return false;
		}
		if (Style.IsValid() && !Style->OrderIndependentCompareEqual(*Other.Style))
		{
			return false;
		}
		if (SlotProps.IsValid() != Other.SlotProps.IsValid())
		{
			return false;
		}
		if (SlotProps.IsValid() && !SlotProps->OrderIndependentCompareEqual(*Other.SlotProps))
		{
			return false;
		}
		return true;
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// RUITK_PROP — one line per field in a props struct:
//
//   struct FRuitkButtonProps : public FRuitkPropsBase
//   {
//       RUITK_PROP(FText, Label, 0)
//       RUITK_PROP(bool, bEnabled, 1)
//       RUITK_PROP_EVENT(OnClicked, 2)
//       RUITK_PROPS_BODY(FRuitkButtonProps, RUITK_EQ(Label) RUITK_EQ(bEnabled))
//   };
//
// Generates: the field + Bit constant + Set<Name>() fluent-ish mutator (used by the node
// builders, D-02) + Has<Name>(). RUITK_PROPS_BODY generates Equals() from the RUITK_EQ rows
// (events deliberately absent from the rows — see FRuitkPropsBase::Equals doc).
// ─────────────────────────────────────────────────────────────────────────────────────────

#define RUITK_PROP(Type, Name, Bit)                                                                                    \
	Type Name{};                                                                                                       \
	static constexpr uint32 Name##_Bit = (Bit);                                                                        \
	void Set##Name(Type InValue)                                                                                       \
	{                                                                                                                  \
		Name = MoveTemp(InValue);                                                                                      \
		SetBits |= (1ull << Name##_Bit);                                                                               \
	}                                                                                                                  \
	bool Has##Name() const                                                                                             \
	{                                                                                                                  \
		return Has(Name##_Bit);                                                                                        \
	}

#define RUITK_PROP_EVENT(Name, Bit)                                                                                    \
	FRuitkCallback Name;                                                                                               \
	static constexpr uint32 Name##_Bit = (Bit);                                                                        \
	void Set##Name(FRuitkCallback InValue)                                                                             \
	{                                                                                                                  \
		Name = MoveTemp(InValue);                                                                                      \
		SetBits |= (1ull << Name##_Bit);                                                                               \
	}                                                                                                                  \
	bool Has##Name() const                                                                                             \
	{                                                                                                                  \
		return Has(Name##_Bit);                                                                                        \
	}

/** One comparison row inside RUITK_PROPS_BODY. */
#define RUITK_EQ(Name)                                                                                                 \
	if (!(Name == Typed->Name))                                                                                        \
	{                                                                                                                  \
		return false;                                                                                                  \
	}

#define RUITK_PROPS_BODY(StructType, EqRows)                                                                           \
	virtual bool Equals(const FRuitkPropsBase& Other) const override                                                   \
	{                                                                                                                  \
		const StructType* Typed = static_cast<const StructType*>(&Other);                                              \
		if (!BaseFieldsEqual(Other))                                                                                   \
		{                                                                                                              \
			return false;                                                                                              \
		}                                                                                                              \
		EqRows return true;                                                                                            \
	}

/** Empty props for components/elements with none. */
struct FRuitkEmptyProps final : public FRuitkPropsBase
{
	virtual bool Equals(const FRuitkPropsBase& Other) const override { return BaseFieldsEqual(Other); }
};
