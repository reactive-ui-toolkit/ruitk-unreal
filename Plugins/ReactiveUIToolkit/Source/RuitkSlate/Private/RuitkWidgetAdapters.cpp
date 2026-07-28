// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Phase 2 step 3, batch 2 — the remaining core widgets on the pattern RuitkCoreAdapters.cpp
// established (production-line shape: props struct → adapter rows → factory → contract
// coverage in Ruitk.Widgets.*). Traps honored per widget: SLATE_EVENT args bind at
// construction (the proxy arrives in CreateWidget), SScrollBox orientation is construct-only
// (reconstruct mask), SEditableTextBox applies text skip-when-equal against the WIDGET
// (D-16 caret rule), SRuitkCanvas swaps its draw fn by identity (D-12).

#include "RuitkElementAdapter.h"
#include "RuitkEventProxy.h"
#include "RuitkSlateElements.h"
#include "RuitkSlateLog.h"
#include "RuitkSlotValue.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleDefaults.h" // FStyleDefaults::GetNoBrush (pointer-backed brush reset, D-17)
#include "SRuitkCanvas.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SNullWidget.h"

namespace
{
	EHorizontalAlignment HAlignOf(FName Value)
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

	EVerticalAlignment VAlignOf(FName Value)
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

	/** One compare-and-set row: apply when set this render AND (fresh widget OR changed). */
	template <typename TProps, typename TValue, typename TApply>
	void Row(const TProps* O, bool bNewHas, bool bOldHas, const TValue& NewValue, const TValue& OldValue, TApply Apply)
	{
		if (bNewHas && (O == nullptr || !bOldHas || !(NewValue == OldValue)))
		{
			Apply(NewValue);
		}
	}
} // namespace

// Convenience for the repetitive row shape below.
#define RUITK_ROW(Prop, ApplyExpr)                                                                                     \
	if (N.Has##Prop() && (O == nullptr || !O->Has##Prop() || !(N.Prop == O->Prop)))                                    \
	{                                                                                                                  \
		ApplyExpr;                                                                                                     \
	}

// ─────────────────────────────────────────────────────────────────────────────────────────
// SBorder
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkBorderAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::SingleContent; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SBorder);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SBorder& W = static_cast<SBorder&>(Widget);
		const FRuitkBorderProps& N = static_cast<const FRuitkBorderProps&>(New);
		const FRuitkBorderProps* O = static_cast<const FRuitkBorderProps*>(Old);
		RUITK_ROW(Padding, W.SetPadding(N.Padding))
		RUITK_ROW(BorderBackgroundColor, W.SetBorderBackgroundColor(FSlateColor(N.BorderBackgroundColor)))
		RUITK_ROW(HAlign, W.SetHAlign(HAlignOf(N.HAlign)))
		RUITK_ROW(VAlign, W.SetVAlign(VAlignOf(N.VAlign)))
		// Border image (D-17). The asset brush (BorderImageBrush) is POINTER-backed and wins over the
		// FCoreStyle name, so it must (a) reset on removal — else SBorder keeps a raw FSlateBrush* into
		// freed props (use-after-free on Paint, bughunt B11) — and (b) be re-applied LAST on ANY change
		// to either input, else a name-only change clobbers a still-set asset (bughunt B10). The name is
		// a plain prop (family: doesn't reset on removal) handled by RUITK_ROW when no asset is present.
		const bool bFresh = (O == nullptr);
		const bool bAssetChanged = N.HasBorderImageBrush() &&
								   (bFresh || !O->HasBorderImageBrush() || N.BorderImageBrush != O->BorderImageBrush);
		const bool bNameChanged =
			N.HasBorderImage() && (bFresh || !O->HasBorderImage() || !(N.BorderImage == O->BorderImage));
		if (O != nullptr && O->HasBorderImageBrush() && !N.HasBorderImageBrush())
		{
			W.SetBorderImage(N.HasBorderImage() ? FCoreStyle::Get().GetBrush(N.BorderImage)
												: FStyleDefaults::GetNoBrush());
		}
		else if (N.HasBorderImageBrush() && (bAssetChanged || bNameChanged))
		{
			W.SetBorderImage(N.BorderImageBrush.Get()); // asset wins, applied last
		}
		else
		{
			RUITK_ROW(BorderImage, W.SetBorderImage(FCoreStyle::Get().GetBrush(N.BorderImage)))
		}
	}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		static_cast<SBorder&>(Parent).SetContent(Child.IsValid() ? Child.ToSharedRef() : SNullWidget::NullWidget);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SBox
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkBoxAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::SingleContent; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SBox);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SBox& W = static_cast<SBox&>(Widget);
		const FRuitkBoxProps& N = static_cast<const FRuitkBoxProps&>(New);
		const FRuitkBoxProps* O = static_cast<const FRuitkBoxProps*>(Old);
		RUITK_ROW(WidthOverride, W.SetWidthOverride(FOptionalSize(N.WidthOverride)))
		RUITK_ROW(HeightOverride, W.SetHeightOverride(FOptionalSize(N.HeightOverride)))
		RUITK_ROW(MinDesiredWidth, W.SetMinDesiredWidth(FOptionalSize(N.MinDesiredWidth)))
		RUITK_ROW(MinDesiredHeight, W.SetMinDesiredHeight(FOptionalSize(N.MinDesiredHeight)))
		RUITK_ROW(MaxDesiredWidth, W.SetMaxDesiredWidth(FOptionalSize(N.MaxDesiredWidth)))
		RUITK_ROW(MaxDesiredHeight, W.SetMaxDesiredHeight(FOptionalSize(N.MaxDesiredHeight)))
		RUITK_ROW(HAlign, W.SetHAlign(HAlignOf(N.HAlign)))
		RUITK_ROW(VAlign, W.SetVAlign(VAlignOf(N.VAlign)))
	}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		static_cast<SBox&>(Parent).SetContent(Child.IsValid() ? Child.ToSharedRef() : SNullWidget::NullWidget);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SImage (v1: tint + desired size — brush content is the D-17 asset/GC work)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkImageAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SImage);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SImage& W = static_cast<SImage&>(Widget);
		const FRuitkImageProps& N = static_cast<const FRuitkImageProps&>(New);
		const FRuitkImageProps* O = static_cast<const FRuitkImageProps*>(Old);
		RUITK_ROW(ColorAndOpacity, W.SetColorAndOpacity(FSlateColor(N.ColorAndOpacity)))
		RUITK_ROW(DesiredSizeOverride, W.SetDesiredSizeOverride(N.DesiredSizeOverride))
		// Asset brush (D-17): POINTER-backed, so it must RESET on removal — the committed props own the
		// sole TSharedPtr, and once they are released SImage's raw FSlateBrush* dangles (use-after-free on
		// the next Paint, bughunt B11). Removed asset -> reset to the no-brush default.
		if (O != nullptr && O->HasImage() && !N.HasImage())
		{
			W.SetImage(FStyleDefaults::GetNoBrush());
		}
		else
		{
			RUITK_ROW(Image, W.SetImage(N.Image.Get()))
		}
	}

	// Markup routes ColorAndOpacity through the STYLE dict (D-13 widget-specific key, like
	// TextBlock's) — the typed row above only serves the C++ authoring API. null = reset to
	// the SImage default (opaque white).
	virtual bool ApplyStyleKey(SWidget& Widget, FName Key, const FRuitkValue* Value) override
	{
		if (Key == FName(TEXT("ColorAndOpacity")))
		{
			static_cast<SImage&>(Widget).SetColorAndOpacity(Value != nullptr ? FSlateColor(Value->ColorValue)
																			 : FSlateColor(FLinearColor::White));
			return true;
		}
		return false;
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SScrollBox (Orientation = construct-only; reorder rebuilds — scroll lists pair with
// reuse_by_slot, which never reorders)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkScrollBoxAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::MultiSlot; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props, const TSharedPtr<FRuitkEventProxy>&) override
	{
		const FRuitkScrollBoxProps& P = static_cast<const FRuitkScrollBoxProps&>(Props);
		const EOrientation Orientation =
			(P.HasOrientation() && P.Orientation == FName(TEXT("horizontal"))) ? Orient_Horizontal : Orient_Vertical;
		return SNew(SScrollBox).Orientation(Orientation);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		// Header-sweep correction: SetOrientation IS a runtime setter — no reconstruct mask.
		SScrollBox& W = static_cast<SScrollBox&>(Widget);
		const FRuitkScrollBoxProps& N = static_cast<const FRuitkScrollBoxProps&>(New);
		const FRuitkScrollBoxProps* O = static_cast<const FRuitkScrollBoxProps*>(Old);
		RUITK_ROW(Orientation,
				  W.SetOrientation(N.Orientation == FName(TEXT("horizontal")) ? Orient_Horizontal : Orient_Vertical))
		// TD-012 sweep (WIDGET_COMPLETION_PLAN wave 2). ScrollToEnd-class imperatives ride P2
		// (WidgetFromHandle<SScrollBox>).
		RUITK_ROW(bAllowOverscroll,
				  W.SetAllowOverscroll(N.bAllowOverscroll ? EAllowOverscroll::Yes : EAllowOverscroll::No))
		RUITK_ROW(bAnimateWheelScrolling, W.SetAnimateWheelScrolling(N.bAnimateWheelScrolling))
		RUITK_ROW(WheelScrollMultiplier, W.SetWheelScrollMultiplier(N.WheelScrollMultiplier))
	}

	virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32,
							 const FRuitkStyleDict* SlotProps) override
	{
		SScrollBox& Box = static_cast<SScrollBox&>(Parent);
		SScrollBox::FScopedWidgetSlotArguments Slot = Box.AddSlot();
		Slot.AttachWidget(Child);
		ConfigureSlot(Slot, SlotProps);
	}

	virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) override
	{
		static_cast<SScrollBox&>(Parent).RemoveSlot(Child);
	}

	virtual void ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
								 TFunctionRef<const FRuitkStyleDict*(const TSharedRef<SWidget>&)> SlotPropsOf) override
	{
		SScrollBox& Box = static_cast<SScrollBox&>(Parent);
		Box.ClearChildren();
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			SScrollBox::FScopedWidgetSlotArguments Slot = Box.AddSlot();
			Slot.AttachWidget(Child);
			ConfigureSlot(Slot, SlotPropsOf(Child));
		}
	}

	virtual void UpdateChildSlotProps(SWidget& Parent, const TSharedRef<SWidget>& Child,
									  const FRuitkStyleDict* SlotProps) override
	{
		// Mutate the LIVE FSlot in place rather than remove+append, which jumped a non-last child to
		// the end (bughunt WRAP-1: SScrollBox appends — GetChildAt order is the visual order).
		SScrollBox& Box = static_cast<SScrollBox&>(Parent);
		FChildren* Children = Box.GetChildren();
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			if (&Children->GetChildAt(i).Get() == &Child.Get())
			{
				SScrollBox::FSlot& Slot =
					const_cast<SScrollBox::FSlot&>(static_cast<const SScrollBox::FSlot&>(Children->GetSlotAt(i)));
				const FRuitkValue* Padding = SlotProps ? SlotProps->Find(FName(TEXT("slot.padding"))) : nullptr;
				Slot.SetPadding(Padding ? Ruitk::Slate::SlotValue::AsMargin(*Padding) : FMargin(0.0f));
				Box.Invalidate(EInvalidateWidgetReason::Layout);
				return;
			}
		}
	}

private:
	static void ConfigureSlot(SScrollBox::FScopedWidgetSlotArguments& Slot, const FRuitkStyleDict* SlotProps)
	{
		if (SlotProps == nullptr)
		{
			return;
		}
		if (const FRuitkValue* Padding = SlotProps->Find(FName(TEXT("slot.padding"))))
		{
			// String/Vector2/uniform forms (bughunt SLOT-2 — was Int/Float-only → String → 0).
			Slot.Padding(Ruitk::Slate::SlotValue::AsMargin(*Padding));
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SSpacer
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkSpacerAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SSpacer);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SSpacer& W = static_cast<SSpacer&>(Widget);
		const FRuitkSpacerProps& N = static_cast<const FRuitkSpacerProps&>(New);
		const FRuitkSpacerProps* O = static_cast<const FRuitkSpacerProps*>(Old);
		RUITK_ROW(Size, W.SetSize(N.Size))
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SEditableTextBox — controlled input (D-16)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkEditableTextAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }

	virtual bool HasEvents() const override { return true; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>& Proxy) override
	{
		return SNew(SEditableTextBox)
			.OnTextChanged(FOnTextChanged::CreateSP(Proxy.ToSharedRef(), &FRuitkEventProxy::HandleText,
													static_cast<int32>(FRuitkEditableTextBoxProps::OnTextChanged_Bit)))
			.OnTextCommitted(
				FOnTextCommitted::CreateSP(Proxy.ToSharedRef(), &FRuitkEventProxy::HandleTextCommit,
										   static_cast<int32>(FRuitkEditableTextBoxProps::OnTextCommitted_Bit)));
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SEditableTextBox& W = static_cast<SEditableTextBox&>(Widget);
		const FRuitkEditableTextBoxProps& N = static_cast<const FRuitkEditableTextBoxProps&>(New);
		const FRuitkEditableTextBoxProps* O = static_cast<const FRuitkEditableTextBoxProps*>(Old);
		// THE caret rule: compare against the WIDGET's live text, not old props — after the
		// typing round-trip (OnTextChanged -> setState -> re-render) the values match and the
		// skip preserves caret/selection. A programmatic state change still applies.
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
		const FRuitkEditableTextBoxProps& N = static_cast<const FRuitkEditableTextBoxProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuitkEditableTextBoxProps::OnTextChanged_Bit), N.OnTextChanged);
		Proxy.SetHandler(static_cast<int32>(FRuitkEditableTextBoxProps::OnTextCommitted_Bit), N.OnTextCommitted);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SCheckBox
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkCheckBoxAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::SingleContent; }

	virtual bool HasEvents() const override { return true; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>& Proxy) override
	{
		return SNew(SCheckBox).OnCheckStateChanged(
			FOnCheckStateChanged::CreateSP(Proxy.ToSharedRef(), &FRuitkEventProxy::HandleChecked,
										   static_cast<int32>(FRuitkCheckBoxProps::OnCheckStateChanged_Bit)));
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SCheckBox& W = static_cast<SCheckBox&>(Widget);
		const FRuitkCheckBoxProps& N = static_cast<const FRuitkCheckBoxProps&>(New);
		// Self-notifying family: skip when the widget already agrees (D-16).
		if (N.HasbIsChecked())
		{
			const ECheckBoxState Want = N.bIsChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			if (W.GetCheckedState() != Want)
			{
				W.SetIsChecked(Want);
			}
		}
	}

	virtual void SyncEventHandlers(FRuitkEventProxy& Proxy, const FRuitkPropsBase& New) override
	{
		const FRuitkCheckBoxProps& N = static_cast<const FRuitkCheckBoxProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuitkCheckBoxProps::OnCheckStateChanged_Bit), N.OnCheckStateChanged);
	}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		static_cast<SCheckBox&>(Parent).SetContent(Child.IsValid() ? Child.ToSharedRef() : SNullWidget::NullWidget);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SSlider
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkSliderAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }

	virtual bool HasEvents() const override { return true; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>& Proxy) override
	{
		return SNew(SSlider).OnValueChanged(
			FOnFloatValueChanged::CreateSP(Proxy.ToSharedRef(), &FRuitkEventProxy::HandleFloat,
										   static_cast<int32>(FRuitkSliderProps::OnValueChanged_Bit)));
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SSlider& W = static_cast<SSlider&>(Widget);
		const FRuitkSliderProps& N = static_cast<const FRuitkSliderProps&>(New);
		const FRuitkSliderProps* O = static_cast<const FRuitkSliderProps*>(Old);
		if ((N.HasMinValue() || N.HasMaxValue()) &&
			(O == nullptr || !(O->MinValue == N.MinValue) || !(O->MaxValue == N.MaxValue)))
		{
			W.SetMinAndMaxValues(N.HasMinValue() ? N.MinValue : 0.0f, N.HasMaxValue() ? N.MaxValue : 1.0f);
		}
		RUITK_ROW(StepSize, W.SetStepSize(N.StepSize))
		// TD-012 sweep (WIDGET_COMPLETION_PLAN wave 2): the remaining live setters.
		RUITK_ROW(Orientation,
				  W.SetOrientation(N.Orientation == FName(TEXT("horizontal")) ? Orient_Horizontal : Orient_Vertical))
		RUITK_ROW(bLocked, W.SetLocked(N.bLocked))
		RUITK_ROW(bIndentHandle, W.SetIndentHandle(N.bIndentHandle))
		RUITK_ROW(SliderBarColor, W.SetSliderBarColor(FSlateColor(N.SliderBarColor)))
		RUITK_ROW(SliderHandleColor, W.SetSliderHandleColor(FSlateColor(N.SliderHandleColor)))
		// Self-notifying skip (D-16): the drag round-trip lands on an equal value.
		if (N.HasValue() && !FMath::IsNearlyEqual(W.GetValue(), N.Value))
		{
			W.SetValue(N.Value);
		}
	}

	virtual void SyncEventHandlers(FRuitkEventProxy& Proxy, const FRuitkPropsBase& New) override
	{
		const FRuitkSliderProps& N = static_cast<const FRuitkSliderProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuitkSliderProps::OnValueChanged_Bit), N.OnValueChanged);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SProgressBar
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkProgressBarAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SProgressBar);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SProgressBar& W = static_cast<SProgressBar&>(Widget);
		const FRuitkProgressBarProps& N = static_cast<const FRuitkProgressBarProps&>(New);
		const FRuitkProgressBarProps* O = static_cast<const FRuitkProgressBarProps*>(Old);
		RUITK_ROW(Percent, W.SetPercent(N.Percent))
		// TD-012 rider (WIDGET_COMPLETION_PLAN wave 1): loyal lowerCamel enum names.
		RUITK_ROW(BarFillType,
				  W.SetBarFillType(N.BarFillType == FName(TEXT("rightToLeft")) ? EProgressBarFillType::RightToLeft
								   : N.BarFillType == FName(TEXT("fillFromCenter"))
									   ? EProgressBarFillType::FillFromCenter
								   : N.BarFillType == FName(TEXT("fillFromCenterHorizontal"))
									   ? EProgressBarFillType::FillFromCenterHorizontal
								   : N.BarFillType == FName(TEXT("fillFromCenterVertical"))
									   ? EProgressBarFillType::FillFromCenterVertical
								   : N.BarFillType == FName(TEXT("topToBottom")) ? EProgressBarFillType::TopToBottom
								   : N.BarFillType == FName(TEXT("bottomToTop")) ? EProgressBarFillType::BottomToTop
																				 : EProgressBarFillType::LeftToRight))
	}

	virtual bool ApplyStyleKey(SWidget& Widget, FName Key, const FRuitkValue* Value) override
	{
		if (Key == FName(TEXT("FillColorAndOpacity")))
		{
			static_cast<SProgressBar&>(Widget).SetFillColorAndOpacity(
				Value != nullptr ? FSlateColor(Value->ColorValue) : FSlateColor(FLinearColor::White));
			return true;
		}
		return false;
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SRuitkCanvas
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkCanvasAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SRuitkCanvas);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SRuitkCanvas& W = static_cast<SRuitkCanvas&>(Widget);
		const FRuitkCanvasProps& N = static_cast<const FRuitkCanvasProps&>(New);
		const FRuitkCanvasProps* O = static_cast<const FRuitkCanvasProps*>(Old);
		if (N.HasDrawFn())
		{
			W.SetDrawFn(N.DrawFn); // identity-guarded inside the widget
		}
		RUITK_ROW(RedrawKey, W.SetRedrawKey(N.RedrawKey))
		RUITK_ROW(CanvasSize, W.SetCanvasDesiredSize(N.CanvasSize))
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
		FRuitkNode MakeHostNode2(FRuitkElementTypeId Type, TProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
		{
			FRuitkNode Node;
			Node.Kind = ERuitkNodeKind::Host;
			Node.ElementType = Type;
			Node.Props = MakeShared<TProps>(MoveTemp(Props));
			Node.Children = Ruitk::MakeChildren(MoveTemp(Children));
			Node.Key = Key;
			return Node;
		}

		FRuitkElementTypeId BorderType()
		{
			return Ruitk::InternElementType(FName(TEXT("Border")));
		}
		FRuitkElementTypeId BoxType()
		{
			return Ruitk::InternElementType(FName(TEXT("Box")));
		}
		FRuitkElementTypeId ImageType()
		{
			return Ruitk::InternElementType(FName(TEXT("Image")));
		}
		FRuitkElementTypeId ScrollBoxType()
		{
			return Ruitk::InternElementType(FName(TEXT("ScrollBox")));
		}
		FRuitkElementTypeId SpacerType()
		{
			return Ruitk::InternElementType(FName(TEXT("Spacer")));
		}
		FRuitkElementTypeId EditableTextBoxType()
		{
			return Ruitk::InternElementType(FName(TEXT("EditableTextBox")));
		}
		FRuitkElementTypeId CheckBoxType()
		{
			return Ruitk::InternElementType(FName(TEXT("CheckBox")));
		}
		FRuitkElementTypeId SliderType()
		{
			return Ruitk::InternElementType(FName(TEXT("Slider")));
		}
		FRuitkElementTypeId ProgressBarType()
		{
			return Ruitk::InternElementType(FName(TEXT("ProgressBar")));
		}
		FRuitkElementTypeId RuitkCanvasType()
		{
			return Ruitk::InternElementType(FName(TEXT("RuitkCanvas")));
		}
	} // namespace

	FRuitkNode Border(FRuitkBorderProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		return MakeHostNode2(BorderType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuitkNode Box(FRuitkBoxProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		return MakeHostNode2(BoxType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuitkNode Image(FRuitkImageProps Props, FRuitkKey Key)
	{
		return MakeHostNode2(ImageType(), MoveTemp(Props), TArray<FRuitkNode>(), Key);
	}
	FRuitkNode ScrollBox(FRuitkScrollBoxProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		return MakeHostNode2(ScrollBoxType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuitkNode Spacer(FRuitkSpacerProps Props, FRuitkKey Key)
	{
		return MakeHostNode2(SpacerType(), MoveTemp(Props), TArray<FRuitkNode>(), Key);
	}
	FRuitkNode EditableTextBox(FRuitkEditableTextBoxProps Props, FRuitkKey Key)
	{
		return MakeHostNode2(EditableTextBoxType(), MoveTemp(Props), TArray<FRuitkNode>(), Key);
	}
	FRuitkNode CheckBox(FRuitkCheckBoxProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		return MakeHostNode2(CheckBoxType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuitkNode Slider(FRuitkSliderProps Props, FRuitkKey Key)
	{
		return MakeHostNode2(SliderType(), MoveTemp(Props), TArray<FRuitkNode>(), Key);
	}
	FRuitkNode ProgressBar(FRuitkProgressBarProps Props, FRuitkKey Key)
	{
		return MakeHostNode2(ProgressBarType(), MoveTemp(Props), TArray<FRuitkNode>(), Key);
	}
	FRuitkNode RuitkCanvas(FRuitkCanvasProps Props, FRuitkKey Key)
	{
		return MakeHostNode2(RuitkCanvasType(), MoveTemp(Props), TArray<FRuitkNode>(), Key);
	}

	TSharedPtr<FRuitkDrawFn> MakeDrawFn(FRuitkDrawFn Fn)
	{
		return MakeShared<FRuitkDrawFn>(MoveTemp(Fn));
	}

	namespace Detail
	{
		void RegisterBatch2Adapters()
		{
			RegisterAdapter(BorderType(), MakeUnique<FRuitkBorderAdapter>());
			RegisterAdapter(BoxType(), MakeUnique<FRuitkBoxAdapter>());
			RegisterAdapter(ImageType(), MakeUnique<FRuitkImageAdapter>());
			RegisterAdapter(ScrollBoxType(), MakeUnique<FRuitkScrollBoxAdapter>());
			RegisterAdapter(SpacerType(), MakeUnique<FRuitkSpacerAdapter>());
			RegisterAdapter(EditableTextBoxType(), MakeUnique<FRuitkEditableTextAdapter>());
			RegisterAdapter(CheckBoxType(), MakeUnique<FRuitkCheckBoxAdapter>());
			RegisterAdapter(SliderType(), MakeUnique<FRuitkSliderAdapter>());
			RegisterAdapter(ProgressBarType(), MakeUnique<FRuitkProgressBarAdapter>());
			RegisterAdapter(RuitkCanvasType(), MakeUnique<FRuitkCanvasAdapter>());
		}
	} // namespace Detail
} // namespace Ruitk::Slate

#undef RUITK_ROW
