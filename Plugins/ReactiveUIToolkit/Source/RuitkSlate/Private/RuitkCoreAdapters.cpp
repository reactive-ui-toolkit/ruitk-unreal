// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// The hand-written pattern adapters (Phase 2 step 3, first batch): STextBlock (the text
// contract), SVerticalBox/SHorizontalBox (MultiSlot panels + slot.* props), SButton
// (SingleContent + the event-proxy pattern), SOverlay (MultiSlot; the SRuitkRoot panel).
// Every later widget copies these shapes via templates/widget_wrapper.template.cpp.
//
// slot.* keys (v1; parsed here — the host is key-agnostic):
//   slot.padding  Float (uniform) | Vector2 (h, v) | String "l,t,r,b"
//   slot.halign   Name/String: fill|left|center|right
//   slot.valign   Name/String: fill|top|center|bottom
//   slot.fill     Float (box panels: FillHeight/FillWidth; 0/absent = AutoHeight/AutoWidth)
//   slot.zorder   Int (overlay only; defaults keep declaration order)
//   slot.position Vector2 | String "x,y" (canvas only; absolute placement)
//   slot.size     Vector2 | String "w,h" (canvas only; absolute size)

#include "RuitkElementAdapter.h"
#include "RuitkEventProxy.h"
#include "RuitkSlateElements.h"
#include "RuitkSlotValue.h"

#include "RuitkSlateLog.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

// ─────────────────────────────────────────────────────────────────────────────────────────
// slot.* parsing
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace
{
	FMargin ParsePadding(const FRuitkValue& V)
	{
		switch (V.Kind)
		{
		case FRuitkValue::EKind::Int:
			return FMargin(static_cast<float>(V.IntValue));
		case FRuitkValue::EKind::Float:
			return FMargin(static_cast<float>(V.FloatValue));
		case FRuitkValue::EKind::Vector2:
			return FMargin(static_cast<float>(V.Vector2Value.X), static_cast<float>(V.Vector2Value.Y));
		case FRuitkValue::EKind::String:
		case FRuitkValue::EKind::Name:
		{
			const FString S = V.Kind == FRuitkValue::EKind::Name ? V.NameValue.ToString() : V.StringValue;
			TArray<FString> Parts;
			S.ParseIntoArray(Parts, TEXT(","), true);
			if (Parts.Num() == 4)
			{
				return FMargin(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]),
							   FCString::Atof(*Parts[3]));
			}
			if (Parts.Num() == 2)
			{
				return FMargin(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]));
			}
			if (Parts.Num() == 1)
			{
				return FMargin(FCString::Atof(*Parts[0]));
			}
			break;
		}
		default:
			break;
		}
		UE_LOG(LogRuitkSlate, Warning, TEXT("[ReactiveUI] slot.padding: unsupported value shape — using 0"));
		return FMargin(0.0f);
	}

	FString ValueAsLowerString(const FRuitkValue& V)
	{
		if (V.Kind == FRuitkValue::EKind::Name)
		{
			return V.NameValue.ToString().ToLower();
		}
		if (V.Kind == FRuitkValue::EKind::String)
		{
			return V.StringValue.ToLower();
		}
		return FString();
	}

	EHorizontalAlignment ParseHAlign(const FRuitkValue& V)
	{
		const FString S = ValueAsLowerString(V);
		if (S == TEXT("left"))
		{
			return HAlign_Left;
		}
		if (S == TEXT("center"))
		{
			return HAlign_Center;
		}
		if (S == TEXT("right"))
		{
			return HAlign_Right;
		}
		return HAlign_Fill;
	}

	EVerticalAlignment ParseVAlign(const FRuitkValue& V)
	{
		const FString S = ValueAsLowerString(V);
		if (S == TEXT("top"))
		{
			return VAlign_Top;
		}
		if (S == TEXT("center"))
		{
			return VAlign_Center;
		}
		if (S == TEXT("bottom"))
		{
			return VAlign_Bottom;
		}
		return VAlign_Fill;
	}

	const FName SlotPaddingKey(TEXT("slot.padding"));
	const FName SlotHAlignKey(TEXT("slot.halign"));
	const FName SlotVAlignKey(TEXT("slot.valign"));
	const FName SlotFillKey(TEXT("slot.fill"));
	const FName SlotZOrderKey(TEXT("slot.zorder"));
	const FName SlotPositionKey(TEXT("slot.position"));
	const FName SlotSizeKey(TEXT("slot.size"));

	float SlotFillOf(const FRuitkStyleDict* SlotProps)
	{
		if (SlotProps != nullptr)
		{
			if (const FRuitkValue* Fill = SlotProps->Find(SlotFillKey))
			{
				return Ruitk::Slate::SlotValue::AsFloat(*Fill); // String/Name literal forms too (SLOT-1)
			}
		}
		return 0.0f; // absent -> Auto size
	}
} // namespace

// ─────────────────────────────────────────────────────────────────────────────────────────
// Text (STextBlock) — renders core FRuitkTextBlockProps (the GetTextElementType contract)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkTextAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(STextBlock);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		STextBlock& W = static_cast<STextBlock&>(Widget);
		const FRuitkTextBlockProps& N = static_cast<const FRuitkTextBlockProps&>(New);
		const FRuitkTextBlockProps* O = static_cast<const FRuitkTextBlockProps*>(Old);
		if (N.HasText())
		{
			const bool bSame = O != nullptr && O->HasText() &&
							   (N.Text.IdenticalTo(O->Text) || N.Text.ToString() == O->Text.ToString());
			if (!bSame)
			{
				W.SetText(N.Text);
			}
		}
	}

	// Widget-specific style keys (D-13): color + fontSize map to setters; null = reset.
	virtual bool ApplyStyleKey(SWidget& Widget, FName Key, const FRuitkValue* Value) override
	{
		STextBlock& W = static_cast<STextBlock&>(Widget);
		if (Key == FName(TEXT("ColorAndOpacity")))
		{
			W.SetColorAndOpacity(Value != nullptr ? FSlateColor(Value->ColorValue) : FSlateColor::UseForeground());
			return true;
		}
		if (Key == FName(TEXT("Font.Size")))
		{
			// R11: SlotValue reader — the literal form (`Font.Size="12"`) is a String and the
			// union-field read silently gave size 0 (SLOT-1's class, style side).
			const int32 Size = Value != nullptr ? Ruitk::Slate::SlotValue::AsInt(*Value, 9) : 9;
			W.SetFont(FCoreStyle::GetDefaultFontStyle("Regular", Size));
			return true;
		}
		if (Key == FName(TEXT("Justification")))
		{
			ETextJustify::Type Justify = ETextJustify::Left;
			if (Value != nullptr)
			{
				const FName Name =
					Value->Kind == FRuitkValue::EKind::Name ? Value->NameValue : FName(*Value->StringValue);
				Justify = Name == FName(TEXT("center"))	 ? ETextJustify::Center
						  : Name == FName(TEXT("right")) ? ETextJustify::Right
														 : ETextJustify::Left;
			}
			W.SetJustification(Justify);
			return true;
		}
		if (Key == FName(TEXT("AutoWrapText")))
		{
			W.SetAutoWrapText(Value != nullptr && Ruitk::Slate::SlotValue::AsBool(*Value)); // R11: parses "true"
			return true;
		}
		// TD-012 sweep (WIDGET_COMPLETION_PLAN wave 2): the remaining text-shaping setters.
		if (Key == FName(TEXT("LineHeightPercentage")))
		{
			W.SetLineHeightPercentage(Value != nullptr ? Ruitk::Slate::SlotValue::AsFloat(*Value, 1.0f) : 1.0f);
			return true;
		}
		if (Key == FName(TEXT("OverflowPolicy")))
		{
			TOptional<ETextOverflowPolicy> Policy; // reset = unset (inherit the style)
			if (Value != nullptr)
			{
				const FName Name =
					Value->Kind == FRuitkValue::EKind::Name ? Value->NameValue : FName(*Value->StringValue);
				Policy = Name == FName(TEXT("ellipsis")) ? ETextOverflowPolicy::Ellipsis : ETextOverflowPolicy::Clip;
			}
			W.SetOverflowPolicy(Policy);
			return true;
		}
		return false;
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// Box panels (SVerticalBox / SHorizontalBox) — MultiSlot
// ─────────────────────────────────────────────────────────────────────────────────────────

/** Shared mechanics for the two box panels (their slot APIs are twins). CreateWidget lives
 *  on the concrete subclasses: SNew stringizes its type token for the widget's debug type
 *  name, so `SNew(TBox)` would label every panel "TBox" in the Widget Reflector. */
template <typename TBox> class TRuitkBoxPanelAdapter : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::MultiSlot; }

	virtual void ApplyDiff(SWidget&, const FRuitkPropsBase*, const FRuitkPropsBase&) override
	{
		// Panels have no own props in v1 — layout is all slot.* on the children.
	}

	virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32 Index,
							 const FRuitkStyleDict* SlotProps) override
	{
		TBox& Box = static_cast<TBox&>(Parent);
		typename TBox::FScopedWidgetSlotArguments Slot = Box.InsertSlot(Index);
		Slot.AttachWidget(Child);
		ConfigureSlot(Slot, SlotProps);
	}

	virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) override
	{
		static_cast<TBox&>(Parent).RemoveSlot(Child);
	}

	virtual void ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
								 TFunctionRef<const FRuitkStyleDict*(const TSharedRef<SWidget>&)> SlotPropsOf) override
	{
		// Minimal-move enforce-order (the family's move_child walk): only out-of-place
		// widgets detach + reinsert. The Bench.SlateReorder spike pits this against a full
		// rebuild — decision recorded in plans/TECH_DEBT.md.
		TBox& Box = static_cast<TBox&>(Parent);
		FChildren* Children = Box.GetChildren();
		for (int32 i = 0; i < Ordered.Num(); ++i)
		{
			if (i < Children->Num() && &Children->GetChildAt(i).Get() == &Ordered[i].Get())
			{
				continue;
			}
			Box.RemoveSlot(Ordered[i]);
			typename TBox::FScopedWidgetSlotArguments Slot = Box.InsertSlot(i);
			Slot.AttachWidget(Ordered[i]);
			ConfigureSlot(Slot, SlotPropsOf(Ordered[i]));
		}
	}

	virtual void UpdateChildSlotProps(SWidget& Parent, const TSharedRef<SWidget>& Child,
									  const FRuitkStyleDict* SlotProps) override
	{
		// TD-010(a): mutate the LIVE FSlot in place — no detach/reinsert churn on hot slot-prop
		// animation paths (e.g. an animated Slot.Padding). GetSlotAt returns a const ref; the slot
		// is engine-mutable, so we cast to the concrete box FSlot and drive its runtime setters.
		TBox& Box = static_cast<TBox&>(Parent);
		FChildren* Children = Box.GetChildren();
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			if (&Children->GetChildAt(i).Get() == &Child.Get())
			{
				typename TBox::FSlot& Slot =
					const_cast<typename TBox::FSlot&>(static_cast<const typename TBox::FSlot&>(Children->GetSlotAt(i)));
				ConfigureSlotLive(Slot, SlotProps);
				Box.Invalidate(EInvalidateWidgetReason::Layout);
				return;
			}
		}
	}

private:
	static void ConfigureSlot(typename TBox::FScopedWidgetSlotArguments& Slot, const FRuitkStyleDict* SlotProps)
	{
		// Slate box slots DEFAULT TO FILL (why engine code writes .AutoHeight() everywhere).
		// The family default is content-hugging AUTO — fill only when slot.fill asks. Without
		// this, every row squeezes proportionally: clipped titles, crushed inputs, invisible
		// button labels (the owner's first-playtest report).
		const float Fill = SlotFillOf(SlotProps);
		if (Fill > 0.0f)
		{
			SetFill(Slot, Fill);
		}
		else
		{
			SetAuto(Slot);
		}
		if (SlotProps == nullptr)
		{
			return;
		}
		if (const FRuitkValue* Padding = SlotProps->Find(SlotPaddingKey))
		{
			Slot.Padding(ParsePadding(*Padding));
		}
		if (const FRuitkValue* H = SlotProps->Find(SlotHAlignKey))
		{
			Slot.HAlign(ParseHAlign(*H));
		}
		if (const FRuitkValue* V = SlotProps->Find(SlotVAlignKey))
		{
			Slot.VAlign(ParseVAlign(*V));
		}
	}

	static void SetFill(SVerticalBox::FScopedWidgetSlotArguments& Slot, float Fill) { Slot.FillHeight(Fill); }
	static void SetFill(SHorizontalBox::FScopedWidgetSlotArguments& Slot, float Fill) { Slot.FillWidth(Fill); }
	static void SetAuto(SVerticalBox::FScopedWidgetSlotArguments& Slot) { Slot.AutoHeight(); }
	static void SetAuto(SHorizontalBox::FScopedWidgetSlotArguments& Slot) { Slot.AutoWidth(); }

	// Live-FSlot counterparts (TD-010(a) in-place update).
	static void SetFillLive(SVerticalBox::FSlot& Slot, float Fill) { Slot.SetFillHeight(Fill); }
	static void SetFillLive(SHorizontalBox::FSlot& Slot, float Fill) { Slot.SetFillWidth(Fill); }
	static void SetAutoLive(SVerticalBox::FSlot& Slot) { Slot.SetAutoHeight(); }
	static void SetAutoLive(SHorizontalBox::FSlot& Slot) { Slot.SetAutoWidth(); }

	/** Apply the full slot config to a LIVE FSlot. Absent keys RESET to the box slot defaults
	 *  (padding 0, HAlign/VAlign Fill) so an update mirrors the fresh-reinsert result exactly. */
	static void ConfigureSlotLive(typename TBox::FSlot& Slot, const FRuitkStyleDict* SlotProps)
	{
		const float Fill = SlotFillOf(SlotProps);
		if (Fill > 0.0f)
		{
			SetFillLive(Slot, Fill);
		}
		else
		{
			SetAutoLive(Slot);
		}
		const FRuitkValue* Padding = SlotProps ? SlotProps->Find(SlotPaddingKey) : nullptr;
		Slot.SetPadding(Padding ? ParsePadding(*Padding) : FMargin(0.0f));
		const FRuitkValue* H = SlotProps ? SlotProps->Find(SlotHAlignKey) : nullptr;
		Slot.SetHorizontalAlignment(H ? ParseHAlign(*H) : HAlign_Fill);
		const FRuitkValue* V = SlotProps ? SlotProps->Find(SlotVAlignKey) : nullptr;
		Slot.SetVerticalAlignment(V ? ParseVAlign(*V) : VAlign_Fill);
	}
};

class FRuitkVerticalBoxAdapter final : public TRuitkBoxPanelAdapter<SVerticalBox>
{
public:
	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SVerticalBox);
	}
};

class FRuitkHorizontalBoxAdapter final : public TRuitkBoxPanelAdapter<SHorizontalBox>
{
public:
	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SHorizontalBox);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// Button (SButton) — SingleContent + the event-proxy pattern
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkButtonAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::SingleContent; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props,
											 const TSharedPtr<FRuitkEventProxy>& Proxy) override
	{
		const FRuitkButtonProps& P = static_cast<const FRuitkButtonProps&>(Props);
		TSharedRef<SButton> W = SNew(SButton).IsFocusable(P.HasbIsFocusable() ? P.bIsFocusable : true);
		W->SetOnClicked(FOnClicked::CreateSP(Proxy.ToSharedRef(), &FRuitkEventProxy::HandleReply,
											 static_cast<int32>(FRuitkButtonProps::OnClicked_Bit)));
		return W;
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SButton& W = static_cast<SButton&>(Widget);
		const FRuitkButtonProps& N = static_cast<const FRuitkButtonProps&>(New);
		const FRuitkButtonProps* O = static_cast<const FRuitkButtonProps*>(Old);
		if (N.HasbEnabled() && (O == nullptr || !O->HasbEnabled() || O->bEnabled != N.bEnabled))
		{
			W.SetEnabled(N.bEnabled);
		}
		if (N.HasContentPadding() &&
			(O == nullptr || !O->HasContentPadding() || !(N.ContentPadding == O->ContentPadding)))
		{
			W.SetContentPadding(N.ContentPadding);
		}
	}

	// bIsFocusable is CONSTRUCT-ONLY (SButton::SetIsFocusable is protected in 5.6) — it rides
	// the TD-011 reconstruct mask like Separator's Orientation (TD-012 rider, wave 1).
	virtual uint64 GetReconstructMask() const override { return 1ull << FRuitkButtonProps::bIsFocusable_Bit; }

	virtual bool ConstructOnlyChanged(const FRuitkPropsBase& Old, const FRuitkPropsBase& New) const override
	{
		const FRuitkButtonProps& O = static_cast<const FRuitkButtonProps&>(Old);
		const FRuitkButtonProps& N = static_cast<const FRuitkButtonProps&>(New);
		return N.HasbIsFocusable() && (!O.HasbIsFocusable() || O.bIsFocusable != N.bIsFocusable);
	}

	virtual bool HasEvents() const override { return true; }

	virtual void SyncEventHandlers(FRuitkEventProxy& Proxy, const FRuitkPropsBase& New) override
	{
		const FRuitkButtonProps& N = static_cast<const FRuitkButtonProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuitkButtonProps::OnClicked_Bit), N.OnClicked);
	}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		static_cast<SButton&>(Parent).SetContent(Child.IsValid() ? Child.ToSharedRef() : SNullWidget::NullWidget);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// Overlay (SOverlay) — MultiSlot (also the SRuitkRoot inner panel)
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkOverlayAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::MultiSlot; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SOverlay);
	}

	virtual void ApplyDiff(SWidget&, const FRuitkPropsBase*, const FRuitkPropsBase&) override {}

	virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32 Index,
							 const FRuitkStyleDict* SlotProps) override
	{
		SOverlay& Overlay = static_cast<SOverlay&>(Parent);
		(void)Index; // overlay stacking order == slot order; reorder enforces it
		SOverlay::FScopedWidgetSlotArguments Slot = Overlay.AddSlot(ZOrderOf(SlotProps));
		Slot.AttachWidget(Child);
		ConfigureSlot(Slot, SlotProps);
	}

	virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) override
	{
		static_cast<SOverlay&>(Parent).RemoveSlot(Child);
	}

	virtual void ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
								 TFunctionRef<const FRuitkStyleDict*(const TSharedRef<SWidget>&)> SlotPropsOf) override
	{
		// AddSlot's parameter is a Z-ORDER, not an index — a minimal-move walk can't target
		// positions. Overlays stack a handful of layers, so rebuild: declaration order wins,
		// explicit slot.zorder re-applies on the way back in.
		SOverlay& Overlay = static_cast<SOverlay&>(Parent);
		Overlay.ClearChildren();
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			const FRuitkStyleDict* SlotProps = SlotPropsOf(Child);
			SOverlay::FScopedWidgetSlotArguments Slot = Overlay.AddSlot(ZOrderOf(SlotProps));
			Slot.AttachWidget(Child);
			ConfigureSlot(Slot, SlotProps);
		}
	}

	virtual void UpdateChildSlotProps(SWidget& Parent, const TSharedRef<SWidget>& Child,
									  const FRuitkStyleDict* SlotProps) override
	{
		SOverlay& Overlay = static_cast<SOverlay&>(Parent);
		if (Overlay.RemoveSlot(Child))
		{
			SOverlay::FScopedWidgetSlotArguments Slot = Overlay.AddSlot(ZOrderOf(SlotProps));
			Slot.AttachWidget(Child);
			ConfigureSlot(Slot, SlotProps);
		}
	}

private:
	static int32 ZOrderOf(const FRuitkStyleDict* SlotProps)
	{
		if (SlotProps != nullptr)
		{
			if (const FRuitkValue* Z = SlotProps->Find(SlotZOrderKey))
			{
				return Ruitk::Slate::SlotValue::AsInt(*Z, INDEX_NONE); // String/Name literal forms too (SLOT-1)
			}
		}
		return INDEX_NONE;
	}

	static void ConfigureSlot(SOverlay::FScopedWidgetSlotArguments& Slot, const FRuitkStyleDict* SlotProps)
	{
		if (SlotProps == nullptr)
		{
			return;
		}
		if (const FRuitkValue* Padding = SlotProps->Find(SlotPaddingKey))
		{
			Slot.Padding(ParsePadding(*Padding));
		}
		if (const FRuitkValue* H = SlotProps->Find(SlotHAlignKey))
		{
			Slot.HAlign(ParseHAlign(*H));
		}
		if (const FRuitkValue* V = SlotProps->Find(SlotVAlignKey))
		{
			Slot.VAlign(ParseVAlign(*V));
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// Canvas (SCanvas) — MultiSlot, ABSOLUTE placement: slot.position + slot.size per child.
// Paint order = child order (no per-slot z; the reconciler's declaration order is the
// painter's order — keep emission stable, the Doom-demo container contract).
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkCanvasPanelAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::MultiSlot; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SCanvas);
	}

	virtual void ApplyDiff(SWidget&, const FRuitkPropsBase*, const FRuitkPropsBase&) override {}

	virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32 Index,
							 const FRuitkStyleDict* SlotProps) override
	{
		// SCanvas has no indexed insertion; the reconciler appends in declaration order and
		// ReorderChildren enforces any later divergence.
		(void)Index;
		SCanvas& Canvas = static_cast<SCanvas&>(Parent);
		SCanvas::FScopedWidgetSlotArguments Slot = Canvas.AddSlot();
		Slot.AttachWidget(Child);
		ConfigureSlot(Slot, SlotProps);
	}

	virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) override
	{
		static_cast<SCanvas&>(Parent).RemoveSlot(Child);
	}

	virtual void ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
								 TFunctionRef<const FRuitkStyleDict*(const TSharedRef<SWidget>&)> SlotPropsOf) override
	{
		// Order IS the painter's order here; a keyed reorder rebuilds (rare on this panel —
		// the demo contract is stable emission order).
		SCanvas& Canvas = static_cast<SCanvas&>(Parent);
		Canvas.ClearChildren();
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			SCanvas::FScopedWidgetSlotArguments Slot = Canvas.AddSlot();
			Slot.AttachWidget(Child);
			ConfigureSlot(Slot, SlotPropsOf(Child));
		}
	}

	virtual void UpdateChildSlotProps(SWidget& Parent, const TSharedRef<SWidget>& Child,
									  const FRuitkStyleDict* SlotProps) override
	{
		// The hot path: SCanvas slots mutate Position/Size IN PLACE — no slot churn (this is
		// what makes per-frame quad movement cheap; contrast the Overlay remove/re-add).
		// Same live-FSlot pattern as the box panels (TD-010a): GetSlotAt returns a const ref,
		// the slot is engine-mutable, so cast to the concrete FSlot and drive its setters.
		SCanvas& Canvas = static_cast<SCanvas&>(Parent);
		FChildren* Children = Canvas.GetChildren();
		for (int32 i = 0; Children != nullptr && i < Children->Num(); ++i)
		{
			if (&Children->GetChildAt(i).Get() == &Child.Get())
			{
				SCanvas::FSlot& Slot =
					const_cast<SCanvas::FSlot&>(static_cast<const SCanvas::FSlot&>(Children->GetSlotAt(i)));
				ApplySlot(Slot, SlotProps);
				Canvas.Invalidate(EInvalidateWidgetReason::Layout);
				return;
			}
		}
	}

private:
	static void ConfigureSlot(SCanvas::FScopedWidgetSlotArguments& Slot, const FRuitkStyleDict* SlotProps)
	{
		if (SlotProps == nullptr)
		{
			return;
		}
		if (const FRuitkValue* P = SlotProps->Find(SlotPositionKey))
		{
			Slot.Position(Ruitk::Slate::SlotValue::AsVector2(*P));
		}
		if (const FRuitkValue* S = SlotProps->Find(SlotSizeKey))
		{
			Slot.Size(Ruitk::Slate::SlotValue::AsVector2(*S));
		}
		if (const FRuitkValue* H = SlotProps->Find(SlotHAlignKey))
		{
			Slot.HAlign(ParseHAlign(*H));
		}
		if (const FRuitkValue* V = SlotProps->Find(SlotVAlignKey))
		{
			Slot.VAlign(ParseVAlign(*V));
		}
	}

	static void ApplySlot(SCanvas::FSlot& Slot, const FRuitkStyleDict* SlotProps)
	{
		// Removal RESETS to the slot defaults (Position 0,0 / Size 1,1 / Left / Top) — the same
		// family semantic the box panels honor: an update result mirrors a fresh reinsert.
		const FRuitkValue* P = SlotProps != nullptr ? SlotProps->Find(SlotPositionKey) : nullptr;
		const FRuitkValue* S = SlotProps != nullptr ? SlotProps->Find(SlotSizeKey) : nullptr;
		const FRuitkValue* H = SlotProps != nullptr ? SlotProps->Find(SlotHAlignKey) : nullptr;
		const FRuitkValue* V = SlotProps != nullptr ? SlotProps->Find(SlotVAlignKey) : nullptr;
		Slot.SetPosition(P != nullptr ? Ruitk::Slate::SlotValue::AsVector2(*P) : FVector2D::ZeroVector);
		Slot.SetSize(S != nullptr ? Ruitk::Slate::SlotValue::AsVector2(*S) : FVector2D(1.0, 1.0));
		Slot.SetHorizontalAlignment(H != nullptr ? ParseHAlign(*H) : HAlign_Left);
		Slot.SetVerticalAlignment(V != nullptr ? ParseVAlign(*V) : VAlign_Top);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// Element factories + registration
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace Ruitk::Slate
{
	FRuitkElementTypeId VerticalBoxType()
	{
		static FRuitkElementTypeId Id = Ruitk::InternElementType(FName(TEXT("VerticalBox")));
		return Id;
	}
	FRuitkElementTypeId HorizontalBoxType()
	{
		static FRuitkElementTypeId Id = Ruitk::InternElementType(FName(TEXT("HorizontalBox")));
		return Id;
	}
	FRuitkElementTypeId ButtonType()
	{
		static FRuitkElementTypeId Id = Ruitk::InternElementType(FName(TEXT("Button")));
		return Id;
	}
	FRuitkElementTypeId OverlayType()
	{
		static FRuitkElementTypeId Id = Ruitk::InternElementType(FName(TEXT("Overlay")));
		return Id;
	}
	FRuitkElementTypeId CanvasType()
	{
		static FRuitkElementTypeId Id = Ruitk::InternElementType(FName(TEXT("Canvas")));
		return Id;
	}

	namespace
	{
		template <typename TProps>
		FRuitkNode MakeHostNode(FRuitkElementTypeId Type, TProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
		{
			FRuitkNode Node;
			Node.Kind = ERuitkNodeKind::Host;
			Node.ElementType = Type;
			Node.Props = MakeShared<TProps>(MoveTemp(Props));
			Node.Children = Ruitk::MakeChildren(MoveTemp(Children));
			Node.Key = Key;
			return Node;
		}
	} // namespace

	FRuitkNode VerticalBox(FRuitkVerticalBoxProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		return MakeHostNode(VerticalBoxType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuitkNode HorizontalBox(FRuitkHorizontalBoxProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		return MakeHostNode(HorizontalBoxType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuitkNode Button(FRuitkButtonProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		return MakeHostNode(ButtonType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuitkNode Overlay(FRuitkOverlayProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		return MakeHostNode(OverlayType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuitkNode Canvas(FRuitkCanvasPanelProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		return MakeHostNode(CanvasType(), MoveTemp(Props), MoveTemp(Children), Key);
	}

	namespace Detail
	{
		void RegisterBatch2Adapters();			 // RuitkWidgetAdapters.cpp
		void RegisterBatch2WidgetAdapters();	 // RuitkWidgetAdaptersB2.cpp (Phase 7 batch-2 set)
		void RegisterItemViewAdapters();		 // RuitkListView.cpp (TD-022 virtualized ListView/TileView)
		void RegisterDragDropAdapters();		 // RuitkDragDrop.cpp (TD-004 DragSource/DropTarget)
		void RegisterExpandableAreaAdapter();	 // RuitkExpandableArea.cpp (TD-012 tail; two role slots)
		void RegisterSegmentedControlAdapter();	 // RuitkSegmentedControl.cpp (TD-012 tail; tab bar)
		void RegisterNumericEntryBoxAdapter();	 // RuitkNumericEntryBox.cpp (TD-012 tail; numeric field)
		void RegisterComboBoxAdapter();			 // RuitkComboBox.cpp (TD-012 tail; dropdown selector)
		void RegisterSuggestionTextBoxAdapter(); // RuitkSuggestionTextBox.cpp (TD-012 tail; autocomplete)
		void RegisterBatch3WidgetAdapters();	 // RuitkWidgetAdaptersB3.cpp (WIDGET_COMPLETION_PLAN wave 1)
		void RegisterExpandableButtonAdapter();	 // RuitkExpandableButton.cpp (wave 2; three role slots)
		void RegisterBatch3Wave3Adapters();		 // RuitkWidgetAdaptersB4.cpp (wave 3 protocol widgets)
		void RegisterTreeViewAdapter();			 // RuitkTreeView.cpp (wave 4; TD-022 closure + P5c columns)
	} // namespace Detail

	void RegisterBuiltinAdapters()
	{
		RegisterAdapter(Ruitk::TextBlockElementType(), MakeUnique<FRuitkTextAdapter>());
		RegisterAdapter(VerticalBoxType(), MakeUnique<FRuitkVerticalBoxAdapter>());
		RegisterAdapter(HorizontalBoxType(), MakeUnique<FRuitkHorizontalBoxAdapter>());
		RegisterAdapter(ButtonType(), MakeUnique<FRuitkButtonAdapter>());
		RegisterAdapter(OverlayType(), MakeUnique<FRuitkOverlayAdapter>());
		RegisterAdapter(CanvasType(), MakeUnique<FRuitkCanvasPanelAdapter>());
		Detail::RegisterBatch2Adapters();
		Detail::RegisterBatch2WidgetAdapters();
		Detail::RegisterItemViewAdapters();
		Detail::RegisterDragDropAdapters();
		Detail::RegisterExpandableAreaAdapter();
		Detail::RegisterSegmentedControlAdapter();
		Detail::RegisterNumericEntryBoxAdapter();
		Detail::RegisterComboBoxAdapter();
		Detail::RegisterSuggestionTextBoxAdapter();
		Detail::RegisterBatch3WidgetAdapters();
		Detail::RegisterExpandableButtonAdapter();
		Detail::RegisterBatch3Wave3Adapters();
		Detail::RegisterTreeViewAdapter();
	}
} // namespace Ruitk::Slate
