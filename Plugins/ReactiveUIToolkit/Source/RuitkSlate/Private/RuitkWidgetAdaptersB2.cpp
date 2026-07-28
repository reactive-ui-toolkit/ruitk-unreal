// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Batch 2 (Phase 7 step 8) — the everyday game widget set (WIDGET_INVENTORY.md), on the same
// production-line shape RuitkWidgetAdapters.cpp established (props struct → adapter rows → factory →
// codegen tag + interp builder → contract/test). Setters are header-verified RUNTIME setters
// wherever Slate exposes them; SSeparator is the exception — its Orientation/Thickness bake at
// construction, so it declares a reconstruct mask (the first shipped widget to exercise TD-011).

#include "RuitkElementAdapter.h"
#include "RuitkEventProxy.h"
#include "RuitkSlateElements.h"
#include "RuitkSlateLog.h"
#include "RuitkSlotValue.h"

#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SSafeZone.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SUniformWrapPanel.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/SRichTextBlock.h"

namespace
{
	// ── shared parsers (batch-2-local; the box-panel set lives in RuitkCoreAdapters.cpp) ─────

	EHorizontalAlignment HAlignB2(FName Value)
	{
		if (Value == FName(TEXT("left")))
		{
			return HAlign_Left;
		}
		if (Value == FName(TEXT("center")))
		{
			return HAlign_Center;
		}
		if (Value == FName(TEXT("right")))
		{
			return HAlign_Right;
		}
		return HAlign_Fill;
	}

	EVerticalAlignment VAlignB2(FName Value)
	{
		if (Value == FName(TEXT("top")))
		{
			return VAlign_Top;
		}
		if (Value == FName(TEXT("center")))
		{
			return VAlign_Center;
		}
		if (Value == FName(TEXT("bottom")))
		{
			return VAlign_Bottom;
		}
		return VAlign_Fill;
	}

	EStretch::Type StretchOf(FName V)
	{
		if (V == FName(TEXT("fill")))
		{
			return EStretch::Fill;
		}
		if (V == FName(TEXT("scaleToFit")))
		{
			return EStretch::ScaleToFit;
		}
		if (V == FName(TEXT("scaleToFitX")))
		{
			return EStretch::ScaleToFitX;
		}
		if (V == FName(TEXT("scaleToFitY")))
		{
			return EStretch::ScaleToFitY;
		}
		if (V == FName(TEXT("scaleToFill")))
		{
			return EStretch::ScaleToFill;
		}
		if (V == FName(TEXT("scaleBySafeZone")))
		{
			return EStretch::ScaleBySafeZone;
		}
		return EStretch::None;
	}

	EStretchDirection::Type StretchDirOf(FName V)
	{
		if (V == FName(TEXT("downOnly")))
		{
			return EStretchDirection::DownOnly;
		}
		if (V == FName(TEXT("upOnly")))
		{
			return EStretchDirection::UpOnly;
		}
		return EStretchDirection::Both;
	}

	SThrobber::EAnimation ThrobberAnimOf(FName V)
	{
		if (V == FName(TEXT("none")))
		{
			return SThrobber::None;
		}
		if (V == FName(TEXT("vertical")))
		{
			return SThrobber::Vertical;
		}
		if (V == FName(TEXT("horizontal")))
		{
			return SThrobber::Horizontal;
		}
		if (V == FName(TEXT("opacity")))
		{
			return SThrobber::Opacity;
		}
		if (V == FName(TEXT("verticalAndOpacity")))
		{
			return SThrobber::VerticalAndOpacity;
		}
		return SThrobber::All;
	}

	/** Apply the common slot.* layout keys to a scoped slot-args object (padding/halign/valign). */
	template <typename TSlotArgs> void ConfigureCommonSlot(TSlotArgs& Slot, const FRuitkStyleDict* SlotProps)
	{
		if (SlotProps == nullptr)
		{
			return;
		}
		if (const FRuitkValue* Padding = SlotProps->Find(FName(TEXT("slot.padding"))))
		{
			// Full parser (String "l,t,r,b"/"h,v"/"u", Vector2, uniform) so these panels honor
			// slot.padding identically to the box panels (bughunt SLOT-2).
			Slot.Padding(Ruitk::Slate::SlotValue::AsMargin(*Padding));
		}
		if (const FRuitkValue* H = SlotProps->Find(FName(TEXT("slot.halign"))))
		{
			Slot.HAlign(HAlignB2(H->Kind == FRuitkValue::EKind::Name ? H->NameValue : FName(*H->StringValue)));
		}
		if (const FRuitkValue* V = SlotProps->Find(FName(TEXT("slot.valign"))))
		{
			Slot.VAlign(VAlignB2(V->Kind == FRuitkValue::EKind::Name ? V->NameValue : FName(*V->StringValue)));
		}
	}

	/** Apply the common slot.* keys to a LIVE FSlot in place (padding/halign/valign), resetting absent
	 *  keys to the slot defaults so a live update mirrors a fresh insert — WITHOUT detach/re-add, which
	 *  on an append-only panel (SWrapBox) would jump the child to the end (bughunt WRAP-1). */
	template <typename TSlot> void ConfigureCommonSlotLive(TSlot& Slot, const FRuitkStyleDict* SlotProps)
	{
		const FRuitkValue* Padding = SlotProps ? SlotProps->Find(FName(TEXT("slot.padding"))) : nullptr;
		Slot.SetPadding(Padding ? Ruitk::Slate::SlotValue::AsMargin(*Padding) : FMargin(0.0f));
		const FRuitkValue* H = SlotProps ? SlotProps->Find(FName(TEXT("slot.halign"))) : nullptr;
		Slot.SetHorizontalAlignment(
			H ? HAlignB2(H->Kind == FRuitkValue::EKind::Name ? H->NameValue : FName(*H->StringValue)) : HAlign_Fill);
		const FRuitkValue* V = SlotProps ? SlotProps->Find(FName(TEXT("slot.valign"))) : nullptr;
		Slot.SetVerticalAlignment(
			V ? VAlignB2(V->Kind == FRuitkValue::EKind::Name ? V->NameValue : FName(*V->StringValue)) : VAlign_Fill);
	}

	bool ChildPresent(FChildren* Children, const TSharedRef<SWidget>& Child)
	{
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			if (&Children->GetChildAt(i).Get() == &Child.Get())
			{
				return true;
			}
		}
		return false;
	}

	/** Read an integer slot.* key (grid column/row); absent -> Def. Parses the String/Name literal forms
	 *  the toolchain emits for a literal `Slot.Column="1"`, not just the expression Int form (SLOT-1). */
	int32 SlotIntOf(const FRuitkStyleDict* SlotProps, const TCHAR* Key, int32 Def)
	{
		if (SlotProps != nullptr)
		{
			if (const FRuitkValue* V = SlotProps->Find(FName(Key)))
			{
				return Ruitk::Slate::SlotValue::AsInt(*V, Def);
			}
		}
		return Def;
	}
} // namespace

// Convenience for the repetitive row shape (mirrors RuitkWidgetAdapters.cpp).
#define RUITK_ROW(Prop, ApplyExpr)                                                                                     \
	if (N.Has##Prop() && (O == nullptr || !O->Has##Prop() || !(N.Prop == O->Prop)))                                    \
	{                                                                                                                  \
		ApplyExpr;                                                                                                     \
	}

// ─────────────────────────────────────────────────────────────────────────────────────────
// SWidgetSwitcher (MultiSlot; shows one child by index)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkWidgetSwitcherAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::MultiSlot; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SWidgetSwitcher);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SWidgetSwitcher& W = static_cast<SWidgetSwitcher&>(Widget);
		const FRuitkWidgetSwitcherProps& N = static_cast<const FRuitkWidgetSwitcherProps&>(New);
		const FRuitkWidgetSwitcherProps* O = static_cast<const FRuitkWidgetSwitcherProps*>(Old);
		RUITK_ROW(WidgetIndex, W.SetActiveWidgetIndex(N.WidgetIndex))
	}

	virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32 Index,
							 const FRuitkStyleDict* SlotProps) override
	{
		SWidgetSwitcher& W = static_cast<SWidgetSwitcher&>(Parent);
		SWidgetSwitcher::FScopedWidgetSlotArguments Slot = W.AddSlot(Index);
		Slot.AttachWidget(Child);
		ConfigureCommonSlot(Slot, SlotProps);
	}

	virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) override
	{
		static_cast<SWidgetSwitcher&>(Parent).RemoveSlot(Child);
	}

	virtual void ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
								 TFunctionRef<const FRuitkStyleDict*(const TSharedRef<SWidget>&)> SlotPropsOf) override
	{
		SWidgetSwitcher& W = static_cast<SWidgetSwitcher&>(Parent);
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			W.RemoveSlot(Child);
		}
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			SWidgetSwitcher::FScopedWidgetSlotArguments Slot = W.AddSlot();
			Slot.AttachWidget(Child);
			ConfigureCommonSlot(Slot, SlotPropsOf(Child));
		}
	}

	virtual void UpdateChildSlotProps(SWidget& Parent, const TSharedRef<SWidget>& Child,
									  const FRuitkStyleDict* SlotProps) override
	{
		// SWidgetSwitcher::GetChildren is protected — RemoveSlot returns the removed index
		// (INDEX_NONE if absent), so re-add at the SAME index to preserve the switcher position.
		SWidgetSwitcher& W = static_cast<SWidgetSwitcher&>(Parent);
		const int32 At = W.RemoveSlot(Child);
		if (At != INDEX_NONE)
		{
			SWidgetSwitcher::FScopedWidgetSlotArguments Slot = W.AddSlot(At);
			Slot.AttachWidget(Child);
			ConfigureCommonSlot(Slot, SlotProps);
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SScaleBox (SingleContent)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkScaleBoxAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::SingleContent; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SScaleBox);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SScaleBox& W = static_cast<SScaleBox&>(Widget);
		const FRuitkScaleBoxProps& N = static_cast<const FRuitkScaleBoxProps&>(New);
		const FRuitkScaleBoxProps* O = static_cast<const FRuitkScaleBoxProps*>(Old);
		RUITK_ROW(Stretch, W.SetStretch(StretchOf(N.Stretch)))
		RUITK_ROW(StretchDirection, W.SetStretchDirection(StretchDirOf(N.StretchDirection)))
		// Scaled-content placement inside the box (SScaleBox live setters; default center|center).
		RUITK_ROW(HAlign, W.SetHAlign(N.HAlign == FName(TEXT("left"))	 ? HAlign_Left
									  : N.HAlign == FName(TEXT("right")) ? HAlign_Right
									  : N.HAlign == FName(TEXT("fill"))	 ? HAlign_Fill
																		 : HAlign_Center))
		RUITK_ROW(VAlign, W.SetVAlign(N.VAlign == FName(TEXT("top"))	  ? VAlign_Top
									  : N.VAlign == FName(TEXT("bottom")) ? VAlign_Bottom
									  : N.VAlign == FName(TEXT("fill"))	  ? VAlign_Fill
																		  : VAlign_Center))
	}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		static_cast<SScaleBox&>(Parent).SetContent(Child.IsValid() ? Child.ToSharedRef() : SNullWidget::NullWidget);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SThrobber (Leaf)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkThrobberAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SThrobber);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SThrobber& W = static_cast<SThrobber&>(Widget);
		const FRuitkThrobberProps& N = static_cast<const FRuitkThrobberProps&>(New);
		const FRuitkThrobberProps* O = static_cast<const FRuitkThrobberProps*>(Old);
		RUITK_ROW(NumPieces, W.SetNumPieces(N.NumPieces))
		RUITK_ROW(Animate, W.SetAnimate(ThrobberAnimOf(N.Animate)))
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SWrapBox (MultiSlot)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkWrapBoxAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::MultiSlot; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SWrapBox);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SWrapBox& W = static_cast<SWrapBox&>(Widget);
		const FRuitkWrapBoxProps& N = static_cast<const FRuitkWrapBoxProps&>(New);
		const FRuitkWrapBoxProps* O = static_cast<const FRuitkWrapBoxProps*>(Old);
		RUITK_ROW(Orientation,
				  W.SetOrientation(N.Orientation == FName(TEXT("vertical")) ? Orient_Vertical : Orient_Horizontal))
		RUITK_ROW(bUseAllottedSize, W.SetUseAllottedSize(N.bUseAllottedSize))
		RUITK_ROW(WrapSize, W.SetWrapSize(N.WrapSize))
		RUITK_ROW(InnerSlotPadding, W.SetInnerSlotPadding(N.InnerSlotPadding))
	}

	virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32,
							 const FRuitkStyleDict* SlotProps) override
	{
		SWrapBox& W = static_cast<SWrapBox&>(Parent);
		SWrapBox::FScopedWidgetSlotArguments Slot = W.AddSlot();
		Slot.AttachWidget(Child);
		ConfigureCommonSlot(Slot, SlotProps);
	}

	virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) override
	{
		static_cast<SWrapBox&>(Parent).RemoveSlot(Child);
	}

	virtual void ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
								 TFunctionRef<const FRuitkStyleDict*(const TSharedRef<SWidget>&)> SlotPropsOf) override
	{
		SWrapBox& W = static_cast<SWrapBox&>(Parent);
		W.ClearChildren();
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			SWrapBox::FScopedWidgetSlotArguments Slot = W.AddSlot();
			Slot.AttachWidget(Child);
			ConfigureCommonSlot(Slot, SlotPropsOf(Child));
		}
	}

	virtual void UpdateChildSlotProps(SWidget& Parent, const TSharedRef<SWidget>& Child,
									  const FRuitkStyleDict* SlotProps) override
	{
		// Mutate the LIVE FSlot in place (like the box panels' TD-010(a) path) instead of remove+append,
		// which on SWrapBox (append-only, no InsertSlot) jumped a non-last child to the end (bughunt WRAP-1).
		SWrapBox& W = static_cast<SWrapBox&>(Parent);
		FChildren* Children = W.GetChildren();
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			if (&Children->GetChildAt(i).Get() == &Child.Get())
			{
				SWrapBox::FSlot& Slot =
					const_cast<SWrapBox::FSlot&>(static_cast<const SWrapBox::FSlot&>(Children->GetSlotAt(i)));
				ConfigureCommonSlotLive(Slot, SlotProps);
				W.Invalidate(EInvalidateWidgetReason::Layout);
				return;
			}
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SMultiLineEditableTextBox — multi-line controlled input (D-16 caret rule)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkMultiLineEditableTextAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>& Proxy) override
	{
		return SNew(SMultiLineEditableTextBox)
			.OnTextChanged(
				FOnTextChanged::CreateSP(Proxy.ToSharedRef(), &FRuitkEventProxy::HandleText,
										 static_cast<int32>(FRuitkMultiLineEditableTextBoxProps::OnTextChanged_Bit)))
			.OnTextCommitted(FOnTextCommitted::CreateSP(
				Proxy.ToSharedRef(), &FRuitkEventProxy::HandleTextCommit,
				static_cast<int32>(FRuitkMultiLineEditableTextBoxProps::OnTextCommitted_Bit)));
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SMultiLineEditableTextBox& W = static_cast<SMultiLineEditableTextBox&>(Widget);
		const FRuitkMultiLineEditableTextBoxProps& N = static_cast<const FRuitkMultiLineEditableTextBoxProps&>(New);
		const FRuitkMultiLineEditableTextBoxProps* O = static_cast<const FRuitkMultiLineEditableTextBoxProps*>(Old);
		// The D-16 caret rule: compare against the WIDGET's live text (survives the typing round-trip).
		if (N.HasText() && !W.GetText().EqualTo(N.Text))
		{
			W.SetText(N.Text);
		}
		if (N.HasHintText() && (O == nullptr || !O->HasHintText() || !N.HintText.EqualTo(O->HintText)))
		{
			W.SetHintText(N.HintText);
		}
		RUITK_ROW(bIsReadOnly, W.SetIsReadOnly(N.bIsReadOnly))
	}

	virtual void SyncEventHandlers(FRuitkEventProxy& Proxy, const FRuitkPropsBase& New) override
	{
		const FRuitkMultiLineEditableTextBoxProps& N = static_cast<const FRuitkMultiLineEditableTextBoxProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuitkMultiLineEditableTextBoxProps::OnTextChanged_Bit), N.OnTextChanged);
		Proxy.SetHandler(static_cast<int32>(FRuitkMultiLineEditableTextBoxProps::OnTextCommitted_Bit),
						 N.OnTextCommitted);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SSearchBox — SEditableTextBox specialization (controlled text; D-16 caret rule)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkSearchBoxAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>& Proxy) override
	{
		return SNew(SSearchBox)
			.OnTextChanged(FOnTextChanged::CreateSP(Proxy.ToSharedRef(), &FRuitkEventProxy::HandleText,
													static_cast<int32>(FRuitkSearchBoxProps::OnTextChanged_Bit)))
			.OnTextCommitted(FOnTextCommitted::CreateSP(Proxy.ToSharedRef(), &FRuitkEventProxy::HandleTextCommit,
														static_cast<int32>(FRuitkSearchBoxProps::OnTextCommitted_Bit)));
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SSearchBox& W = static_cast<SSearchBox&>(Widget);
		const FRuitkSearchBoxProps& N = static_cast<const FRuitkSearchBoxProps&>(New);
		const FRuitkSearchBoxProps* O = static_cast<const FRuitkSearchBoxProps*>(Old);
		if (N.HasText() && !W.GetText().EqualTo(N.Text))
		{
			W.SetText(N.Text);
		}
		if (N.HasHintText() && (O == nullptr || !O->HasHintText() || !N.HintText.EqualTo(O->HintText)))
		{
			W.SetHintText(N.HintText);
		}
	}

	virtual void SyncEventHandlers(FRuitkEventProxy& Proxy, const FRuitkPropsBase& New) override
	{
		const FRuitkSearchBoxProps& N = static_cast<const FRuitkSearchBoxProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuitkSearchBoxProps::OnTextChanged_Bit), N.OnTextChanged);
		Proxy.SetHandler(static_cast<int32>(FRuitkSearchBoxProps::OnTextCommitted_Bit), N.OnTextCommitted);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SSafeZone (SingleContent)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkSafeZoneAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::SingleContent; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SSafeZone);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SSafeZone& W = static_cast<SSafeZone&>(Widget);
		const FRuitkSafeZoneProps& N = static_cast<const FRuitkSafeZoneProps&>(New);
		const FRuitkSafeZoneProps* O = static_cast<const FRuitkSafeZoneProps*>(Old);
		RUITK_ROW(bIsTitleSafe, W.SetTitleSafe(N.bIsTitleSafe))
		// The four pad-side bools apply together (single Slate setter).
		if ((N.HasbPadLeft() || N.HasbPadRight() || N.HasbPadTop() || N.HasbPadBottom()) &&
			(O == nullptr || !(O->bPadLeft == N.bPadLeft) || !(O->bPadRight == N.bPadRight) ||
			 !(O->bPadTop == N.bPadTop) || !(O->bPadBottom == N.bPadBottom)))
		{
			W.SetSidesToPad(N.HasbPadLeft() ? N.bPadLeft : true, N.HasbPadRight() ? N.bPadRight : true,
							N.HasbPadTop() ? N.bPadTop : true, N.HasbPadBottom() ? N.bPadBottom : true);
		}
	}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		static_cast<SSafeZone&>(Parent).SetContent(Child.IsValid() ? Child.ToSharedRef() : SNullWidget::NullWidget);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SDPIScaler (SingleContent)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkDPIScalerAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::SingleContent; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SDPIScaler);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SDPIScaler& W = static_cast<SDPIScaler&>(Widget);
		const FRuitkDPIScalerProps& N = static_cast<const FRuitkDPIScalerProps&>(New);
		const FRuitkDPIScalerProps* O = static_cast<const FRuitkDPIScalerProps*>(Old);
		RUITK_ROW(DPIScale, W.SetDPIScale(N.DPIScale))
	}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		static_cast<SDPIScaler&>(Parent).SetContent(Child.IsValid() ? Child.ToSharedRef() : SNullWidget::NullWidget);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SSeparator (Leaf) — Orientation + Thickness are CONSTRUCT-ONLY (the first shipped widget to
// exercise the TD-011 reconstruct mask); ColorAndOpacity is a live setter (inherited SBorder).
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkSeparatorAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }

	// Orientation (bit 0) + Thickness (bit 1) bake at construction — a change replaces the widget.
	virtual uint64 GetReconstructMask() const override
	{
		return (1ull << FRuitkSeparatorProps::Orientation_Bit) | (1ull << FRuitkSeparatorProps::Thickness_Bit);
	}

	virtual bool ConstructOnlyChanged(const FRuitkPropsBase& Old, const FRuitkPropsBase& New) const override
	{
		const FRuitkSeparatorProps& O = static_cast<const FRuitkSeparatorProps&>(Old);
		const FRuitkSeparatorProps& N = static_cast<const FRuitkSeparatorProps&>(New);
		// Gate on the Has-bits (bughunt SEP-REBUILD-1): merely REMOVING a construct-only prop in the new
		// render is not a change (removed plain props don't reset) — comparing raw values would see
		// old!=default and force a spurious rebuild that resets the widget to its literal default.
		const bool bOrient = N.HasOrientation() && (!O.HasOrientation() || !(O.Orientation == N.Orientation));
		const bool bThickness = N.HasThickness() && (!O.HasThickness() || !(O.Thickness == N.Thickness));
		return bOrient || bThickness;
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props, const TSharedPtr<FRuitkEventProxy>&) override
	{
		const FRuitkSeparatorProps& P = static_cast<const FRuitkSeparatorProps&>(Props);
		return SNew(SSeparator)
			.Orientation(P.HasOrientation() && P.Orientation == FName(TEXT("vertical")) ? Orient_Vertical
																						: Orient_Horizontal)
			.Thickness(P.HasThickness() ? P.Thickness : 3.0f);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SSeparator& W = static_cast<SSeparator&>(Widget);
		const FRuitkSeparatorProps& N = static_cast<const FRuitkSeparatorProps&>(New);
		const FRuitkSeparatorProps* O = static_cast<const FRuitkSeparatorProps*>(Old);
		RUITK_ROW(ColorAndOpacity, W.SetColorAndOpacity(N.ColorAndOpacity))
	}

	// Markup routes ColorAndOpacity through the STYLE dict (D-13 widget-specific key, like
	// TextBlock's) — the typed row above only serves the C++ authoring API. null = reset to
	// the SCompoundWidget default (opaque white).
	virtual bool ApplyStyleKey(SWidget& Widget, FName Key, const FRuitkValue* Value) override
	{
		if (Key == FName(TEXT("ColorAndOpacity")))
		{
			static_cast<SSeparator&>(Widget).SetColorAndOpacity(Value != nullptr ? Value->ColorValue
																				 : FLinearColor::White);
			return true;
		}
		return false;
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SSpinBox<float> (Leaf) — numeric input (self-notifying skip; D-16)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkSpinBoxAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>& Proxy) override
	{
		return SNew(SSpinBox<float>)
			.OnValueChanged(
				SSpinBox<float>::FOnValueChanged::CreateSP(Proxy.ToSharedRef(), &FRuitkEventProxy::HandleFloat,
														   static_cast<int32>(FRuitkSpinBoxProps::OnValueChanged_Bit)));
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SSpinBox<float>& W = static_cast<SSpinBox<float>&>(Widget);
		const FRuitkSpinBoxProps& N = static_cast<const FRuitkSpinBoxProps&>(New);
		const FRuitkSpinBoxProps* O = static_cast<const FRuitkSpinBoxProps*>(Old);
		RUITK_ROW(MinValue, W.SetMinValue(N.MinValue))
		RUITK_ROW(MaxValue, W.SetMaxValue(N.MaxValue))
		RUITK_ROW(Delta, W.SetDelta(N.Delta))
		// Self-notifying skip: the drag/commit round-trip lands on an equal value (D-16).
		if (N.HasValue() && !FMath::IsNearlyEqual(W.GetValue(), N.Value))
		{
			W.SetValue(N.Value);
		}
	}

	virtual void SyncEventHandlers(FRuitkEventProxy& Proxy, const FRuitkPropsBase& New) override
	{
		const FRuitkSpinBoxProps& N = static_cast<const FRuitkSpinBoxProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuitkSpinBoxProps::OnValueChanged_Bit), N.OnValueChanged);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SUniformWrapPanel (MultiSlot)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkUniformWrapPanelAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::MultiSlot; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SUniformWrapPanel);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SUniformWrapPanel& W = static_cast<SUniformWrapPanel&>(Widget);
		const FRuitkUniformWrapPanelProps& N = static_cast<const FRuitkUniformWrapPanelProps&>(New);
		const FRuitkUniformWrapPanelProps* O = static_cast<const FRuitkUniformWrapPanelProps*>(Old);
		RUITK_ROW(SlotPadding, W.SetSlotPadding(FMargin(N.SlotPadding)))
		RUITK_ROW(HAlign, W.SetHorizontalAlignment(HAlignB2(N.HAlign)))
	}

	virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32, const FRuitkStyleDict*) override
	{
		SUniformWrapPanel& W = static_cast<SUniformWrapPanel&>(Parent);
		SUniformWrapPanel::FScopedWidgetSlotArguments Slot = W.AddSlot();
		Slot.AttachWidget(Child);
	}

	virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) override
	{
		FChildren* Children = static_cast<SUniformWrapPanel&>(Parent).GetChildren();
		// SUniformWrapPanel has no RemoveSlot(widget) — rebuild without the removed child.
		TArray<TSharedRef<SWidget>> Kept;
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			TSharedRef<SWidget> C = Children->GetChildAt(i);
			if (&C.Get() != &Child.Get())
			{
				Kept.Add(C);
			}
		}
		Rebuild(static_cast<SUniformWrapPanel&>(Parent), Kept);
	}

	virtual void ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
								 TFunctionRef<const FRuitkStyleDict*(const TSharedRef<SWidget>&)>) override
	{
		Rebuild(static_cast<SUniformWrapPanel&>(Parent), Ordered);
	}

private:
	static void Rebuild(SUniformWrapPanel& W, const TArray<TSharedRef<SWidget>>& Children)
	{
		W.ClearChildren();
		for (const TSharedRef<SWidget>& Child : Children)
		{
			SUniformWrapPanel::FScopedWidgetSlotArguments Slot = W.AddSlot();
			Slot.AttachWidget(Child);
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SRichTextBlock (Leaf)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkRichTextBlockAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SRichTextBlock);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SRichTextBlock& W = static_cast<SRichTextBlock&>(Widget);
		const FRuitkRichTextBlockProps& N = static_cast<const FRuitkRichTextBlockProps&>(New);
		const FRuitkRichTextBlockProps* O = static_cast<const FRuitkRichTextBlockProps*>(Old);
		if (N.HasText() && (O == nullptr || !O->HasText() || !N.Text.EqualTo(O->Text)))
		{
			W.SetText(N.Text);
		}
		RUITK_ROW(bAutoWrapText, W.SetAutoWrapText(N.bAutoWrapText))
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SGridPanel / SUniformGridPanel (MultiSlot; slot.column + slot.row place each child)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkGridPanelAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::MultiSlot; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SGridPanel);
	}

	virtual void ApplyDiff(SWidget&, const FRuitkPropsBase*, const FRuitkPropsBase&) override {}

	virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32,
							 const FRuitkStyleDict* SlotProps) override
	{
		SGridPanel& W = static_cast<SGridPanel&>(Parent);
		SGridPanel::FScopedWidgetSlotArguments Slot =
			W.AddSlot(SlotIntOf(SlotProps, TEXT("slot.column"), 0), SlotIntOf(SlotProps, TEXT("slot.row"), 0));
		Slot.AttachWidget(Child);
		ConfigureCommonSlot(Slot, SlotProps);
	}

	virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) override
	{
		Rebuild(static_cast<SGridPanel&>(Parent), Child);
	}

	virtual void UpdateChildSlotProps(SWidget& Parent, const TSharedRef<SWidget>& Child,
									  const FRuitkStyleDict* SlotProps) override
	{
		// A grid places by cell, not by child order, so re-place the child at its (possibly new)
		// column/row on any slot-prop change (bughunt GRID-1: previously the base no-op left the
		// child stuck in its original cell). SGridPanel::FSlot has no runtime column/row setter.
		SGridPanel& W = static_cast<SGridPanel&>(Parent);
		if (!ChildPresent(W.GetChildren(), Child))
		{
			return;
		}
		W.RemoveSlot(Child);
		SGridPanel::FScopedWidgetSlotArguments Slot =
			W.AddSlot(SlotIntOf(SlotProps, TEXT("slot.column"), 0), SlotIntOf(SlotProps, TEXT("slot.row"), 0));
		Slot.AttachWidget(Child);
		ConfigureCommonSlot(Slot, SlotProps);
	}

	virtual void ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
								 TFunctionRef<const FRuitkStyleDict*(const TSharedRef<SWidget>&)> SlotPropsOf) override
	{
		SGridPanel& W = static_cast<SGridPanel&>(Parent);
		W.ClearChildren();
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			const FRuitkStyleDict* SlotProps = SlotPropsOf(Child);
			SGridPanel::FScopedWidgetSlotArguments Slot =
				W.AddSlot(SlotIntOf(SlotProps, TEXT("slot.column"), 0), SlotIntOf(SlotProps, TEXT("slot.row"), 0));
			Slot.AttachWidget(Child);
			ConfigureCommonSlot(Slot, SlotProps);
		}
	}

private:
	static void Rebuild(SGridPanel& W, const TSharedRef<SWidget>& Remove)
	{
		// SGridPanel::RemoveSlot exists but takes a widget; a bare remove drops cell placement,
		// so rebuild keeping every OTHER child at its current column/row is out of reach without
		// stored cells. v1: just remove the slot (the reconciler re-inserts survivors on reorder).
		W.RemoveSlot(Remove);
	}
};

class FRuitkUniformGridPanelAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::MultiSlot; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SUniformGridPanel);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SUniformGridPanel& W = static_cast<SUniformGridPanel&>(Widget);
		const FRuitkUniformGridPanelProps& N = static_cast<const FRuitkUniformGridPanelProps&>(New);
		const FRuitkUniformGridPanelProps* O = static_cast<const FRuitkUniformGridPanelProps*>(Old);
		RUITK_ROW(SlotPadding, W.SetSlotPadding(FMargin(N.SlotPadding)))
		RUITK_ROW(MinDesiredSlotWidth, W.SetMinDesiredSlotWidth(N.MinDesiredSlotWidth))
		RUITK_ROW(MinDesiredSlotHeight, W.SetMinDesiredSlotHeight(N.MinDesiredSlotHeight))
	}

	virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32,
							 const FRuitkStyleDict* SlotProps) override
	{
		SUniformGridPanel& W = static_cast<SUniformGridPanel&>(Parent);
		SUniformGridPanel::FScopedWidgetSlotArguments Slot =
			W.AddSlot(SlotIntOf(SlotProps, TEXT("slot.column"), 0), SlotIntOf(SlotProps, TEXT("slot.row"), 0));
		Slot.AttachWidget(Child);
	}

	virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) override
	{
		static_cast<SUniformGridPanel&>(Parent).RemoveSlot(Child);
	}

	virtual void UpdateChildSlotProps(SWidget& Parent, const TSharedRef<SWidget>& Child,
									  const FRuitkStyleDict* SlotProps) override
	{
		// Re-place at the (possibly new) column/row on a slot-prop change (bughunt GRID-1).
		SUniformGridPanel& W = static_cast<SUniformGridPanel&>(Parent);
		if (!ChildPresent(W.GetChildren(), Child))
		{
			return;
		}
		W.RemoveSlot(Child);
		SUniformGridPanel::FScopedWidgetSlotArguments Slot =
			W.AddSlot(SlotIntOf(SlotProps, TEXT("slot.column"), 0), SlotIntOf(SlotProps, TEXT("slot.row"), 0));
		Slot.AttachWidget(Child);
	}

	virtual void ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
								 TFunctionRef<const FRuitkStyleDict*(const TSharedRef<SWidget>&)> SlotPropsOf) override
	{
		SUniformGridPanel& W = static_cast<SUniformGridPanel&>(Parent);
		W.ClearChildren();
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			const FRuitkStyleDict* SlotProps = SlotPropsOf(Child);
			SUniformGridPanel::FScopedWidgetSlotArguments Slot =
				W.AddSlot(SlotIntOf(SlotProps, TEXT("slot.column"), 0), SlotIntOf(SlotProps, TEXT("slot.row"), 0));
			Slot.AttachWidget(Child);
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// Types, factories, registration
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace Ruitk::Slate
{
	namespace
	{
		template <typename TProps>
		FRuitkNode MakeHostNodeB2(FRuitkElementTypeId Type, TProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
		{
			FRuitkNode Node;
			Node.Kind = ERuitkNodeKind::Host;
			Node.ElementType = Type;
			Node.Props = MakeShared<TProps>(MoveTemp(Props));
			Node.Children = Ruitk::MakeChildren(MoveTemp(Children));
			Node.Key = Key;
			return Node;
		}

		FRuitkElementTypeId WidgetSwitcherType()
		{
			return Ruitk::InternElementType(FName(TEXT("WidgetSwitcher")));
		}
		FRuitkElementTypeId ScaleBoxType()
		{
			return Ruitk::InternElementType(FName(TEXT("ScaleBox")));
		}
		FRuitkElementTypeId ThrobberType()
		{
			return Ruitk::InternElementType(FName(TEXT("Throbber")));
		}
		FRuitkElementTypeId WrapBoxType()
		{
			return Ruitk::InternElementType(FName(TEXT("WrapBox")));
		}
		FRuitkElementTypeId MultiLineEditableTextBoxType()
		{
			return Ruitk::InternElementType(FName(TEXT("MultiLineEditableTextBox")));
		}
		FRuitkElementTypeId SearchBoxType()
		{
			return Ruitk::InternElementType(FName(TEXT("SearchBox")));
		}
		FRuitkElementTypeId SafeZoneType()
		{
			return Ruitk::InternElementType(FName(TEXT("SafeZone")));
		}
		FRuitkElementTypeId DPIScalerType()
		{
			return Ruitk::InternElementType(FName(TEXT("DPIScaler")));
		}
		FRuitkElementTypeId SeparatorType()
		{
			return Ruitk::InternElementType(FName(TEXT("Separator")));
		}
		FRuitkElementTypeId SpinBoxType()
		{
			return Ruitk::InternElementType(FName(TEXT("SpinBox")));
		}
		FRuitkElementTypeId UniformWrapPanelType()
		{
			return Ruitk::InternElementType(FName(TEXT("UniformWrapPanel")));
		}
		FRuitkElementTypeId RichTextBlockType()
		{
			return Ruitk::InternElementType(FName(TEXT("RichTextBlock")));
		}
		FRuitkElementTypeId GridPanelType()
		{
			return Ruitk::InternElementType(FName(TEXT("GridPanel")));
		}
		FRuitkElementTypeId UniformGridPanelType()
		{
			return Ruitk::InternElementType(FName(TEXT("UniformGridPanel")));
		}
	} // namespace

	FRuitkNode WidgetSwitcher(FRuitkWidgetSwitcherProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		return MakeHostNodeB2(WidgetSwitcherType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuitkNode ScaleBox(FRuitkScaleBoxProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		return MakeHostNodeB2(ScaleBoxType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuitkNode Throbber(FRuitkThrobberProps Props, FRuitkKey Key)
	{
		return MakeHostNodeB2(ThrobberType(), MoveTemp(Props), TArray<FRuitkNode>(), Key);
	}
	FRuitkNode WrapBox(FRuitkWrapBoxProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		return MakeHostNodeB2(WrapBoxType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuitkNode MultiLineEditableTextBox(FRuitkMultiLineEditableTextBoxProps Props, FRuitkKey Key)
	{
		return MakeHostNodeB2(MultiLineEditableTextBoxType(), MoveTemp(Props), TArray<FRuitkNode>(), Key);
	}
	FRuitkNode SearchBox(FRuitkSearchBoxProps Props, FRuitkKey Key)
	{
		return MakeHostNodeB2(SearchBoxType(), MoveTemp(Props), TArray<FRuitkNode>(), Key);
	}
	FRuitkNode SafeZone(FRuitkSafeZoneProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		return MakeHostNodeB2(SafeZoneType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuitkNode DPIScaler(FRuitkDPIScalerProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		return MakeHostNodeB2(DPIScalerType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuitkNode Separator(FRuitkSeparatorProps Props, FRuitkKey Key)
	{
		return MakeHostNodeB2(SeparatorType(), MoveTemp(Props), TArray<FRuitkNode>(), Key);
	}
	FRuitkNode SpinBox(FRuitkSpinBoxProps Props, FRuitkKey Key)
	{
		return MakeHostNodeB2(SpinBoxType(), MoveTemp(Props), TArray<FRuitkNode>(), Key);
	}
	FRuitkNode UniformWrapPanel(FRuitkUniformWrapPanelProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		return MakeHostNodeB2(UniformWrapPanelType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuitkNode RichTextBlock(FRuitkRichTextBlockProps Props, FRuitkKey Key)
	{
		return MakeHostNodeB2(RichTextBlockType(), MoveTemp(Props), TArray<FRuitkNode>(), Key);
	}
	FRuitkNode GridPanel(FRuitkGridPanelProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		return MakeHostNodeB2(GridPanelType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuitkNode UniformGridPanel(FRuitkUniformGridPanelProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		return MakeHostNodeB2(UniformGridPanelType(), MoveTemp(Props), MoveTemp(Children), Key);
	}

	namespace Detail
	{
		void RegisterBatch2WidgetAdapters()
		{
			RegisterAdapter(WidgetSwitcherType(), MakeUnique<FRuitkWidgetSwitcherAdapter>());
			RegisterAdapter(ScaleBoxType(), MakeUnique<FRuitkScaleBoxAdapter>());
			RegisterAdapter(ThrobberType(), MakeUnique<FRuitkThrobberAdapter>());
			RegisterAdapter(WrapBoxType(), MakeUnique<FRuitkWrapBoxAdapter>());
			RegisterAdapter(MultiLineEditableTextBoxType(), MakeUnique<FRuitkMultiLineEditableTextAdapter>());
			RegisterAdapter(SearchBoxType(), MakeUnique<FRuitkSearchBoxAdapter>());
			RegisterAdapter(SafeZoneType(), MakeUnique<FRuitkSafeZoneAdapter>());
			RegisterAdapter(DPIScalerType(), MakeUnique<FRuitkDPIScalerAdapter>());
			RegisterAdapter(SeparatorType(), MakeUnique<FRuitkSeparatorAdapter>());
			RegisterAdapter(SpinBoxType(), MakeUnique<FRuitkSpinBoxAdapter>());
			RegisterAdapter(UniformWrapPanelType(), MakeUnique<FRuitkUniformWrapPanelAdapter>());
			RegisterAdapter(RichTextBlockType(), MakeUnique<FRuitkRichTextBlockAdapter>());
			RegisterAdapter(GridPanelType(), MakeUnique<FRuitkGridPanelAdapter>());
			RegisterAdapter(UniformGridPanelType(), MakeUnique<FRuitkUniformGridPanelAdapter>());
		}
	} // namespace Detail
} // namespace Ruitk::Slate

#undef RUITK_ROW
