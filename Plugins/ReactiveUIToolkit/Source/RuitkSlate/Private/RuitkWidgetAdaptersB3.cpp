// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Batch 3 wave 1 (WIDGET_COMPLETION_PLAN §3) — the mechanical leaves, on the B2 production
// pattern. The wave's defining trait: FOUR of these widgets expose NO runtime setters
// (SColorBlock / the two gradients / SHyperlink bake every arg at Construct), so they are the
// first shipped widgets whose ENTIRE prop surface rides the TD-011 reconstruct mask — a prop
// change replaces the widget in place (ReplaceWidget; cheap leaves, no state to lose).
// SBackgroundBlur / SInvalidationPanel are ordinary setter-based single-content wraps;
// SEnableBox / SScissorRectBox are content-only.

#include "RuitkElementAdapter.h"
#include "RuitkEventProxy.h"
#include "RuitkSlateElements.h"

#include "Styling/StyleDefaults.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Colors/SColorSpectrum.h"
#include "Widgets/Colors/SColorWheel.h"
#include "Widgets/Colors/SComplexGradient.h"
#include "Widgets/Colors/SSimpleGradient.h"
#include "Widgets/Images/SLayeredImage.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Input/SInputKeySelector.h"
#include "Widgets/Input/SVirtualKeyboardEntry.h"
#include "Widgets/Input/SVolumeControl.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBox.h" // SEnableBox is an SBox — content goes through SBox::SetContent
#include "Widgets/Layout/SEnableBox.h"
#include "Widgets/Layout/SRadialBox.h"
#include "Widgets/Layout/SScissorRectBox.h"
#include "Widgets/SInvalidationPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/ColorGrading/SColorGradingWheel.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/STextScroller.h"

namespace
{
	// One compare-and-set row (the B2 convention).
#define RUITK_ROW(Prop, ApplyExpr)                                                                                     \
	if (N.Has##Prop() && (O == nullptr || !O->Has##Prop() || !(N.Prop == O->Prop)))                                    \
	{                                                                                                                  \
		ApplyExpr;                                                                                                     \
	}

	// One construct-only change gate (the Separator convention, bughunt SEP-REBUILD-1: gate on
	// the Has-bits so REMOVING a prop never forces a spurious rebuild).
#define RUITK_CTOR_CHANGED(Prop) (N.Has##Prop() && (!O.Has##Prop() || !(O.Prop == N.Prop)))

	EOrientation OrientOf(FName V)
	{
		return V == FName(TEXT("horizontal")) ? Orient_Horizontal : Orient_Vertical;
	}
} // namespace

// ─────────────────────────────────────────────────────────────────────────────────────────
// SColorBlock — fully construct-only (no engine setters); every prop is masked.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkColorBlockAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }

	virtual uint64 GetReconstructMask() const override
	{
		return (1ull << FRuitkColorBlockProps::Color_Bit) | (1ull << FRuitkColorBlockProps::Size_Bit) |
			   (1ull << FRuitkColorBlockProps::bUseSRGB_Bit) |
			   (1ull << FRuitkColorBlockProps::bShowBackgroundForAlpha_Bit) |
			   (1ull << FRuitkColorBlockProps::bColorIsHSV_Bit) | (1ull << FRuitkColorBlockProps::AlphaDisplayMode_Bit);
	}

	virtual bool ConstructOnlyChanged(const FRuitkPropsBase& Old, const FRuitkPropsBase& New) const override
	{
		const FRuitkColorBlockProps& O = static_cast<const FRuitkColorBlockProps&>(Old);
		const FRuitkColorBlockProps& N = static_cast<const FRuitkColorBlockProps&>(New);
		return RUITK_CTOR_CHANGED(Color) || RUITK_CTOR_CHANGED(Size) || RUITK_CTOR_CHANGED(bUseSRGB) ||
			   RUITK_CTOR_CHANGED(bShowBackgroundForAlpha) || RUITK_CTOR_CHANGED(bColorIsHSV) ||
			   RUITK_CTOR_CHANGED(AlphaDisplayMode);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props, const TSharedPtr<FRuitkEventProxy>&) override
	{
		const FRuitkColorBlockProps& P = static_cast<const FRuitkColorBlockProps&>(Props);
		const EColorBlockAlphaDisplayMode Alpha =
			P.AlphaDisplayMode == FName(TEXT("separate")) ? EColorBlockAlphaDisplayMode::Separate
			: P.AlphaDisplayMode == FName(TEXT("ignore")) ? EColorBlockAlphaDisplayMode::Ignore
														  : EColorBlockAlphaDisplayMode::Combined;
		return SNew(SColorBlock)
			.Color(P.HasColor() ? P.Color : FLinearColor::White)
			.Size(P.HasSize() ? P.Size : FVector2D(16.0, 16.0))
			.UseSRGB(P.HasbUseSRGB() ? P.bUseSRGB : true)
			.ShowBackgroundForAlpha(P.HasbShowBackgroundForAlpha() && P.bShowBackgroundForAlpha)
			.ColorIsHSV(P.HasbColorIsHSV() && P.bColorIsHSV)
			.AlphaDisplayMode(Alpha);
	}

	virtual void ApplyDiff(SWidget&, const FRuitkPropsBase*, const FRuitkPropsBase&) override {} // all masked
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SSimpleGradient / SComplexGradient — construct-only paint leaves; fully masked.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkSimpleGradientAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }

	virtual uint64 GetReconstructMask() const override
	{
		return (1ull << FRuitkSimpleGradientProps::StartColor_Bit) | (1ull << FRuitkSimpleGradientProps::EndColor_Bit) |
			   (1ull << FRuitkSimpleGradientProps::Orientation_Bit) |
			   (1ull << FRuitkSimpleGradientProps::bHasAlphaBackground_Bit);
	}

	virtual bool ConstructOnlyChanged(const FRuitkPropsBase& Old, const FRuitkPropsBase& New) const override
	{
		const FRuitkSimpleGradientProps& O = static_cast<const FRuitkSimpleGradientProps&>(Old);
		const FRuitkSimpleGradientProps& N = static_cast<const FRuitkSimpleGradientProps&>(New);
		return RUITK_CTOR_CHANGED(StartColor) || RUITK_CTOR_CHANGED(EndColor) || RUITK_CTOR_CHANGED(Orientation) ||
			   RUITK_CTOR_CHANGED(bHasAlphaBackground);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props, const TSharedPtr<FRuitkEventProxy>&) override
	{
		const FRuitkSimpleGradientProps& P = static_cast<const FRuitkSimpleGradientProps&>(Props);
		return SNew(SSimpleGradient)
			.StartColor(P.HasStartColor() ? P.StartColor : FLinearColor::Black)
			.EndColor(P.HasEndColor() ? P.EndColor : FLinearColor::White)
			.Orientation(P.HasOrientation() ? OrientOf(P.Orientation) : Orient_Vertical)
			.HasAlphaBackground(P.HasbHasAlphaBackground() && P.bHasAlphaBackground);
	}

	virtual void ApplyDiff(SWidget&, const FRuitkPropsBase*, const FRuitkPropsBase&) override {}
};

class FRuitkComplexGradientAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }

	virtual uint64 GetReconstructMask() const override
	{
		return (1ull << FRuitkComplexGradientProps::GradientColors_Bit) |
			   (1ull << FRuitkComplexGradientProps::Orientation_Bit) |
			   (1ull << FRuitkComplexGradientProps::bHasAlphaBackground_Bit) |
			   (1ull << FRuitkComplexGradientProps::DesiredSizeOverride_Bit);
	}

	virtual bool ConstructOnlyChanged(const FRuitkPropsBase& Old, const FRuitkPropsBase& New) const override
	{
		const FRuitkComplexGradientProps& O = static_cast<const FRuitkComplexGradientProps&>(Old);
		const FRuitkComplexGradientProps& N = static_cast<const FRuitkComplexGradientProps&>(New);
		return RUITK_CTOR_CHANGED(GradientColors) || RUITK_CTOR_CHANGED(Orientation) ||
			   RUITK_CTOR_CHANGED(bHasAlphaBackground) || RUITK_CTOR_CHANGED(DesiredSizeOverride);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props, const TSharedPtr<FRuitkEventProxy>&) override
	{
		const FRuitkComplexGradientProps& P = static_cast<const FRuitkComplexGradientProps&>(Props);
		return SNew(SComplexGradient)
			.GradientColors(P.GradientColors)
			.Orientation(P.HasOrientation() ? OrientOf(P.Orientation) : Orient_Vertical)
			.HasAlphaBackground(P.HasbHasAlphaBackground() && P.bHasAlphaBackground)
			.DesiredSizeOverride(P.HasDesiredSizeOverride() ? TOptional<FVector2D>(P.DesiredSizeOverride)
															: TOptional<FVector2D>());
	}

	virtual void ApplyDiff(SWidget&, const FRuitkPropsBase*, const FRuitkPropsBase&) override {}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SHyperlink — construct-only text/padding (masked) + OnNavigate through the event proxy.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkHyperlinkAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	virtual uint64 GetReconstructMask() const override
	{
		return (1ull << FRuitkHyperlinkProps::Text_Bit) | (1ull << FRuitkHyperlinkProps::Padding_Bit);
	}

	virtual bool ConstructOnlyChanged(const FRuitkPropsBase& Old, const FRuitkPropsBase& New) const override
	{
		const FRuitkHyperlinkProps& O = static_cast<const FRuitkHyperlinkProps&>(Old);
		const FRuitkHyperlinkProps& N = static_cast<const FRuitkHyperlinkProps&>(New);
		const bool bText = N.HasText() && (!O.HasText() || !O.Text.IdenticalTo(N.Text));
		return bText || RUITK_CTOR_CHANGED(Padding);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props,
											 const TSharedPtr<FRuitkEventProxy>& Proxy) override
	{
		const FRuitkHyperlinkProps& P = static_cast<const FRuitkHyperlinkProps&>(Props);
		return SNew(SHyperlink)
			.Text(P.Text)
			.Padding(P.HasPadding() ? P.Padding : FMargin(0.0f))
			.OnNavigate(FSimpleDelegate::CreateSP(Proxy.ToSharedRef(), &FRuitkEventProxy::HandleVoid,
												  static_cast<int32>(FRuitkHyperlinkProps::OnNavigate_Bit)));
	}

	virtual void SyncEventHandlers(FRuitkEventProxy& Proxy, const FRuitkPropsBase& New) override
	{
		const FRuitkHyperlinkProps& N = static_cast<const FRuitkHyperlinkProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuitkHyperlinkProps::OnNavigate_Bit), N.OnNavigate);
	}

	virtual void ApplyDiff(SWidget&, const FRuitkPropsBase*, const FRuitkPropsBase&) override {}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SEnableBox / SScissorRectBox — content-only containers.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkEnableBoxAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::SingleContent; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SEnableBox);
	}

	virtual void ApplyDiff(SWidget&, const FRuitkPropsBase*, const FRuitkPropsBase&) override {}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		// SEnableBox has no SetContent — content is its (single) ChildSlot via SBox.
		static_cast<SBox&>(Parent).SetContent(Child.IsValid() ? Child.ToSharedRef() : SNullWidget::NullWidget);
	}
};

class FRuitkScissorRectBoxAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::SingleContent; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SScissorRectBox);
	}

	virtual void ApplyDiff(SWidget&, const FRuitkPropsBase*, const FRuitkPropsBase&) override {}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		static_cast<SScissorRectBox&>(Parent).SetContent(Child.IsValid() ? Child.ToSharedRef()
																		 : SNullWidget::NullWidget);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SBackgroundBlur — setter-based single content.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkBackgroundBlurAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::SingleContent; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SBackgroundBlur);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SBackgroundBlur& W = static_cast<SBackgroundBlur&>(Widget);
		const FRuitkBackgroundBlurProps& N = static_cast<const FRuitkBackgroundBlurProps&>(New);
		const FRuitkBackgroundBlurProps* O = static_cast<const FRuitkBackgroundBlurProps*>(Old);
		RUITK_ROW(BlurStrength, W.SetBlurStrength(N.BlurStrength))
		RUITK_ROW(BlurRadius, W.SetBlurRadius(TOptional<int32>(N.BlurRadius)))
		RUITK_ROW(bApplyAlphaToBlur, W.SetApplyAlphaToBlur(N.bApplyAlphaToBlur))
		RUITK_ROW(Padding, W.SetPadding(N.Padding))
	}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		static_cast<SBackgroundBlur&>(Parent).SetContent(Child.IsValid() ? Child.ToSharedRef()
																		 : SNullWidget::NullWidget);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SInvalidationPanel — setter-based single content (opt-in paint cache).
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkInvalidationPanelAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::SingleContent; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SInvalidationPanel);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SInvalidationPanel& W = static_cast<SInvalidationPanel&>(Widget);
		const FRuitkInvalidationPanelProps& N = static_cast<const FRuitkInvalidationPanelProps&>(New);
		const FRuitkInvalidationPanelProps* O = static_cast<const FRuitkInvalidationPanelProps*>(Old);
		RUITK_ROW(bCanCache, W.SetCanCache(N.bCanCache))
	}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		static_cast<SInvalidationPanel&>(Parent).SetContent(Child.IsValid() ? Child.ToSharedRef()
																			: SNullWidget::NullWidget);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SVolumeControl (wave 2) — Volume/Muted are attribute-only (no setters): controlled via the
// reconstruct mask; the two user-edit events flow through the proxy.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkVolumeControlAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	virtual uint64 GetReconstructMask() const override
	{
		return (1ull << FRuitkVolumeControlProps::Volume_Bit) | (1ull << FRuitkVolumeControlProps::bMuted_Bit);
	}

	virtual bool ConstructOnlyChanged(const FRuitkPropsBase& Old, const FRuitkPropsBase& New) const override
	{
		const FRuitkVolumeControlProps& O = static_cast<const FRuitkVolumeControlProps&>(Old);
		const FRuitkVolumeControlProps& N = static_cast<const FRuitkVolumeControlProps&>(New);
		return RUITK_CTOR_CHANGED(Volume) || RUITK_CTOR_CHANGED(bMuted);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props,
											 const TSharedPtr<FRuitkEventProxy>& Proxy) override
	{
		const FRuitkVolumeControlProps& P = static_cast<const FRuitkVolumeControlProps&>(Props);
		return SNew(SVolumeControl)
			.Volume(P.HasVolume() ? P.Volume : 1.0f)
			.Muted(P.HasbMuted() && P.bMuted)
			.OnVolumeChanged(
				FOnFloatValueChanged::CreateSP(Proxy.ToSharedRef(), &FRuitkEventProxy::HandleFloat,
											   static_cast<int32>(FRuitkVolumeControlProps::OnVolumeChanged_Bit)))
			.OnMuteChanged(
				SVolumeControl::FOnMuted::CreateSP(Proxy.ToSharedRef(), &FRuitkEventProxy::HandleBool,
												   static_cast<int32>(FRuitkVolumeControlProps::OnMuteChanged_Bit)));
	}

	virtual void SyncEventHandlers(FRuitkEventProxy& Proxy, const FRuitkPropsBase& New) override
	{
		const FRuitkVolumeControlProps& N = static_cast<const FRuitkVolumeControlProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuitkVolumeControlProps::OnVolumeChanged_Bit), N.OnVolumeChanged);
		Proxy.SetHandler(static_cast<int32>(FRuitkVolumeControlProps::OnMuteChanged_Bit), N.OnMuteChanged);
	}

	virtual void ApplyDiff(SWidget&, const FRuitkPropsBase*, const FRuitkPropsBase&) override {}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// STextScroller (wave 2) — construct-only options (masked); Start/Suspend/Reset via P2.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkTextScrollerAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::SingleContent; }

	virtual uint64 GetReconstructMask() const override
	{
		return (1ull << FRuitkTextScrollerProps::Speed_Bit) | (1ull << FRuitkTextScrollerProps::StartDelay_Bit) |
			   (1ull << FRuitkTextScrollerProps::EndDelay_Bit) |
			   (1ull << FRuitkTextScrollerProps::ScrollOrientation_Bit);
	}

	virtual bool ConstructOnlyChanged(const FRuitkPropsBase& Old, const FRuitkPropsBase& New) const override
	{
		const FRuitkTextScrollerProps& O = static_cast<const FRuitkTextScrollerProps&>(Old);
		const FRuitkTextScrollerProps& N = static_cast<const FRuitkTextScrollerProps&>(New);
		return RUITK_CTOR_CHANGED(Speed) || RUITK_CTOR_CHANGED(StartDelay) || RUITK_CTOR_CHANGED(EndDelay) ||
			   RUITK_CTOR_CHANGED(ScrollOrientation);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props, const TSharedPtr<FRuitkEventProxy>&) override
	{
		const FRuitkTextScrollerProps& P = static_cast<const FRuitkTextScrollerProps&>(Props);
		FTextScrollerOptions Options;
		if (P.HasSpeed())
		{
			Options.Speed = P.Speed;
		}
		if (P.HasStartDelay())
		{
			Options.StartDelay = P.StartDelay;
		}
		if (P.HasEndDelay())
		{
			Options.EndDelay = P.EndDelay;
		}
		return SNew(STextScroller)
			.ScrollOptions(Options)
			.ScrollOrientation(P.HasScrollOrientation() ? OrientOf(P.ScrollOrientation) : Orient_Horizontal);
	}

	virtual void ApplyDiff(SWidget&, const FRuitkPropsBase*, const FRuitkPropsBase&) override {}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		// STextScroller exposes no content setter (SLATE_DEFAULT_SLOT only) — a data-free peek
		// subclass re-exports the protected ChildSlot (layout-identical, so the cast is safe).
		struct FScrollerPeek : STextScroller
		{
			using SCompoundWidget::ChildSlot;
		};
		static_cast<FScrollerPeek&>(static_cast<STextScroller&>(Parent))
			.ChildSlot.AttachWidget(Child.IsValid() ? Child.ToSharedRef() : SNullWidget::NullWidget);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SRadialBox (wave 2) — MultiSlot with BARE slots (no per-child args; arc order = child
// order). PreferredWidth is construct-only; the angle params have live setters — the inline
// ones don't invalidate, so we invalidate layout explicitly (the Canvas precedent).
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkRadialBoxAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::MultiSlot; }

	virtual uint64 GetReconstructMask() const override { return 1ull << FRuitkRadialBoxProps::PreferredWidth_Bit; }

	virtual bool ConstructOnlyChanged(const FRuitkPropsBase& Old, const FRuitkPropsBase& New) const override
	{
		const FRuitkRadialBoxProps& O = static_cast<const FRuitkRadialBoxProps&>(Old);
		const FRuitkRadialBoxProps& N = static_cast<const FRuitkRadialBoxProps&>(New);
		return RUITK_CTOR_CHANGED(PreferredWidth);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props, const TSharedPtr<FRuitkEventProxy>&) override
	{
		const FRuitkRadialBoxProps& P = static_cast<const FRuitkRadialBoxProps&>(Props);
		return SNew(SRadialBox)
			.PreferredWidth(P.HasPreferredWidth() ? P.PreferredWidth : 100.f)
			.UseAllottedWidth(P.HasbUseAllottedWidth() && P.bUseAllottedWidth)
			.StartingAngle(P.HasStartingAngle() ? P.StartingAngle : 0.f)
			.bDistributeItemsEvenly(!P.HasbDistributeItemsEvenly() || P.bDistributeItemsEvenly)
			.AngleBetweenItems(P.HasAngleBetweenItems() ? P.AngleBetweenItems : 45.f)
			.SectorCentralAngle(P.HasSectorCentralAngle() ? P.SectorCentralAngle : 360.f);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SRadialBox& W = static_cast<SRadialBox&>(Widget);
		const FRuitkRadialBoxProps& N = static_cast<const FRuitkRadialBoxProps&>(New);
		const FRuitkRadialBoxProps* O = static_cast<const FRuitkRadialBoxProps*>(Old);
		bool bTouched = false;
		RUITK_ROW(bUseAllottedWidth, W.SetUseAllottedWidth(N.bUseAllottedWidth))
		RUITK_ROW(StartingAngle, (W.SetStartingAngle(N.StartingAngle), bTouched = true))
		RUITK_ROW(bDistributeItemsEvenly, (W.SetDistributeItemsEvenly(N.bDistributeItemsEvenly), bTouched = true))
		RUITK_ROW(AngleBetweenItems, (W.SetAngleBetweenItems(N.AngleBetweenItems), bTouched = true))
		RUITK_ROW(SectorCentralAngle, (W.SetSectorCentralAngle(N.SectorCentralAngle), bTouched = true))
		if (bTouched)
		{
			W.Invalidate(EInvalidateWidgetReason::Layout); // the inline setters skip invalidation
		}
	}

	virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32, const FRuitkStyleDict*) override
	{
		SRadialBox::FScopedWidgetSlotArguments Slot = static_cast<SRadialBox&>(Parent).AddSlot();
		Slot.AttachWidget(Child);
	}

	virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) override
	{
		static_cast<SRadialBox&>(Parent).RemoveSlot(Child);
	}

	virtual void ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
								 TFunctionRef<const FRuitkStyleDict*(const TSharedRef<SWidget>&)>) override
	{
		SRadialBox& W = static_cast<SRadialBox&>(Parent);
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			W.RemoveSlot(Child);
		}
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			SRadialBox::FScopedWidgetSlotArguments Slot = W.AddSlot();
			Slot.AttachWidget(Child);
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SColorWheel / SColorSpectrum (wave 2) — SelectedColor is attribute-only: controlled via the
// reconstruct mask; drags report through OnValueChanged (+ capture begin/end).
// ─────────────────────────────────────────────────────────────────────────────────────────

template <typename TWidget, typename TProps> class TRuitkColorPickerAdapter : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	virtual uint64 GetReconstructMask() const override { return 1ull << TProps::SelectedColor_Bit; }

	virtual bool ConstructOnlyChanged(const FRuitkPropsBase& Old, const FRuitkPropsBase& New) const override
	{
		const TProps& O = static_cast<const TProps&>(Old);
		const TProps& N = static_cast<const TProps&>(New);
		return N.HasSelectedColor() && (!O.HasSelectedColor() || !(O.SelectedColor == N.SelectedColor));
	}

	virtual void SyncEventHandlers(FRuitkEventProxy& Proxy, const FRuitkPropsBase& New) override
	{
		const TProps& N = static_cast<const TProps&>(New);
		Proxy.SetHandler(static_cast<int32>(TProps::OnValueChanged_Bit), N.OnValueChanged);
		Proxy.SetHandler(static_cast<int32>(TProps::OnMouseCaptureBegin_Bit), N.OnMouseCaptureBegin);
		Proxy.SetHandler(static_cast<int32>(TProps::OnMouseCaptureEnd_Bit), N.OnMouseCaptureEnd);
	}

	virtual void ApplyDiff(SWidget&, const FRuitkPropsBase*, const FRuitkPropsBase&) override {}

protected:
	template <typename TArgs> void FillCommon(TArgs& Args, const TProps& P, const TSharedPtr<FRuitkEventProxy>& Proxy)
	{
		Args.SelectedColor(P.HasSelectedColor() ? P.SelectedColor : FLinearColor::White)
			.OnValueChanged(FOnLinearColorValueChanged::CreateSP(Proxy.ToSharedRef(), &FRuitkEventProxy::HandleColor,
																 static_cast<int32>(TProps::OnValueChanged_Bit)))
			.OnMouseCaptureBegin(FSimpleDelegate::CreateSP(Proxy.ToSharedRef(), &FRuitkEventProxy::HandleVoid,
														   static_cast<int32>(TProps::OnMouseCaptureBegin_Bit)))
			.OnMouseCaptureEnd(FSimpleDelegate::CreateSP(Proxy.ToSharedRef(), &FRuitkEventProxy::HandleVoid,
														 static_cast<int32>(TProps::OnMouseCaptureEnd_Bit)));
	}
};

class FRuitkColorWheelAdapter final : public TRuitkColorPickerAdapter<SColorWheel, FRuitkColorWheelProps>
{
public:
	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props,
											 const TSharedPtr<FRuitkEventProxy>& Proxy) override
	{
		const FRuitkColorWheelProps& P = static_cast<const FRuitkColorWheelProps&>(Props);
		SColorWheel::FArguments Args;
		FillCommon(Args, P, Proxy);
		TSharedRef<SColorWheel> W = SNew(SColorWheel);
		W->Construct(Args);
		return W;
	}
};

class FRuitkColorSpectrumAdapter final : public TRuitkColorPickerAdapter<SColorSpectrum, FRuitkColorSpectrumProps>
{
public:
	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props,
											 const TSharedPtr<FRuitkEventProxy>& Proxy) override
	{
		const FRuitkColorSpectrumProps& P = static_cast<const FRuitkColorSpectrumProps&>(Props);
		SColorSpectrum::FArguments Args;
		FillCommon(Args, P, Proxy);
		TSharedRef<SColorSpectrum> W = SNew(SColorSpectrum);
		W->Construct(Args);
		return W;
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SLayeredImage (wave 2) — SImage + live overlay layers (brush identity, B11 reset rule).
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkLayeredImageAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SLayeredImage);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SLayeredImage& W = static_cast<SLayeredImage&>(Widget);
		const FRuitkLayeredImageProps& N = static_cast<const FRuitkLayeredImageProps&>(New);
		const FRuitkLayeredImageProps* O = static_cast<const FRuitkLayeredImageProps*>(Old);
		RUITK_ROW(ColorAndOpacity, W.SetColorAndOpacity(FSlateColor(N.ColorAndOpacity)))
		RUITK_ROW(DesiredSizeOverride, W.SetDesiredSizeOverride(N.DesiredSizeOverride))
		// Base brush: pointer-backed — reset on removal (B11), else the widget dangles.
		if (O != nullptr && O->HasImage() && !N.HasImage())
		{
			W.SetImage(FStyleDefaults::GetNoBrush());
		}
		else
		{
			RUITK_ROW(Image, W.SetImage(N.Image.Get()))
		}
		// Layers: identity-diffed as a list; any change rebuilds the layer stack (cheap).
		const bool bLayersRemoved = O != nullptr && O->HasLayers() && !N.HasLayers();
		const bool bLayersChanged = N.HasLayers() && (O == nullptr || !O->HasLayers() || !(N.Layers == O->Layers));
		if (bLayersRemoved || bLayersChanged)
		{
			W.RemoveAllLayers();
			if (N.HasLayers())
			{
				for (const TSharedPtr<FSlateBrush>& Layer : N.Layers)
				{
					W.AddLayer(Layer.Get());
				}
			}
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SInputKeySelector (wave 2) — live SelectedKey; capture-behavior args construct-only.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkInputKeySelectorAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	virtual uint64 GetReconstructMask() const override
	{
		return (1ull << FRuitkInputKeySelectorProps::KeySelectionText_Bit) |
			   (1ull << FRuitkInputKeySelectorProps::NoKeySpecifiedText_Bit) |
			   (1ull << FRuitkInputKeySelectorProps::bAllowModifierKeys_Bit) |
			   (1ull << FRuitkInputKeySelectorProps::bAllowGamepadKeys_Bit) |
			   (1ull << FRuitkInputKeySelectorProps::bEscapeCancelsSelection_Bit);
	}

	virtual bool ConstructOnlyChanged(const FRuitkPropsBase& Old, const FRuitkPropsBase& New) const override
	{
		const FRuitkInputKeySelectorProps& O = static_cast<const FRuitkInputKeySelectorProps&>(Old);
		const FRuitkInputKeySelectorProps& N = static_cast<const FRuitkInputKeySelectorProps&>(New);
		auto TextChanged = [](bool bNewHas, bool bOldHas, const FText& A, const FText& B)
		{ return bNewHas && (!bOldHas || !(B.IdenticalTo(A) || B.ToString() == A.ToString())); };
		return TextChanged(N.HasKeySelectionText(), O.HasKeySelectionText(), O.KeySelectionText, N.KeySelectionText) ||
			   TextChanged(N.HasNoKeySpecifiedText(), O.HasNoKeySpecifiedText(), O.NoKeySpecifiedText,
						   N.NoKeySpecifiedText) ||
			   RUITK_CTOR_CHANGED(bAllowModifierKeys) || RUITK_CTOR_CHANGED(bAllowGamepadKeys) ||
			   RUITK_CTOR_CHANGED(bEscapeCancelsSelection);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props,
											 const TSharedPtr<FRuitkEventProxy>& Proxy) override
	{
		const FRuitkInputKeySelectorProps& P = static_cast<const FRuitkInputKeySelectorProps&>(Props);
		TWeakPtr<FRuitkEventProxy> WeakProxy = Proxy;
		return SNew(SInputKeySelector)
			.SelectedKey(P.HasSelectedKey() ? FInputChord(FKey(P.SelectedKey)) : FInputChord())
			.KeySelectionText(P.KeySelectionText)
			.NoKeySpecifiedText(P.NoKeySpecifiedText)
			.AllowModifierKeys(!P.HasbAllowModifierKeys() || P.bAllowModifierKeys)
			.AllowGamepadKeys(P.HasbAllowGamepadKeys() && P.bAllowGamepadKeys)
			.EscapeCancelsSelection(!P.HasbEscapeCancelsSelection() || P.bEscapeCancelsSelection)
			.OnKeySelected(SInputKeySelector::FOnKeySelected::CreateLambda(
				[WeakProxy](const FInputChord& Chord)
				{
					if (TSharedPtr<FRuitkEventProxy> Pinned = WeakProxy.Pin())
					{
						// Key-only payload (TD-016: modifiers are the multi-field trigger).
						Pinned->HandleName(Chord.Key.GetFName(),
										   static_cast<int32>(FRuitkInputKeySelectorProps::OnKeySelected_Bit));
					}
				}))
			.OnIsSelectingKeyChanged(SInputKeySelector::FOnIsSelectingKeyChanged::CreateSP(
				Proxy.ToSharedRef(), &FRuitkEventProxy::HandleVoid,
				static_cast<int32>(FRuitkInputKeySelectorProps::OnIsSelectingKeyChanged_Bit)));
	}

	virtual void SyncEventHandlers(FRuitkEventProxy& Proxy, const FRuitkPropsBase& New) override
	{
		const FRuitkInputKeySelectorProps& N = static_cast<const FRuitkInputKeySelectorProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuitkInputKeySelectorProps::OnKeySelected_Bit), N.OnKeySelected);
		Proxy.SetHandler(static_cast<int32>(FRuitkInputKeySelectorProps::OnIsSelectingKeyChanged_Bit),
						 N.OnIsSelectingKeyChanged);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SInputKeySelector& W = static_cast<SInputKeySelector&>(Widget);
		const FRuitkInputKeySelectorProps& N = static_cast<const FRuitkInputKeySelectorProps&>(New);
		const FRuitkInputKeySelectorProps* O = static_cast<const FRuitkInputKeySelectorProps*>(Old);
		RUITK_ROW(SelectedKey, W.SetSelectedKey(FInputChord(FKey(N.SelectedKey))))
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SEditableText (wave 2) — the raw single-line edit; full live setters; D-16 caret rule.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkRawEditableTextAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }
	// Stateful (caret/selection): IsPoolable() already excludes event-bearing leaves.

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>& Proxy) override
	{
		return SNew(SEditableText)
			.OnTextChanged(FOnTextChanged::CreateSP(Proxy.ToSharedRef(), &FRuitkEventProxy::HandleText,
													static_cast<int32>(FRuitkEditableTextProps::OnTextChanged_Bit)))
			.OnTextCommitted(
				FOnTextCommitted::CreateSP(Proxy.ToSharedRef(), &FRuitkEventProxy::HandleTextCommit,
										   static_cast<int32>(FRuitkEditableTextProps::OnTextCommitted_Bit)));
	}

	virtual void SyncEventHandlers(FRuitkEventProxy& Proxy, const FRuitkPropsBase& New) override
	{
		const FRuitkEditableTextProps& N = static_cast<const FRuitkEditableTextProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuitkEditableTextProps::OnTextChanged_Bit), N.OnTextChanged);
		Proxy.SetHandler(static_cast<int32>(FRuitkEditableTextProps::OnTextCommitted_Bit), N.OnTextCommitted);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SEditableText& W = static_cast<SEditableText&>(Widget);
		const FRuitkEditableTextProps& N = static_cast<const FRuitkEditableTextProps&>(New);
		const FRuitkEditableTextProps* O = static_cast<const FRuitkEditableTextProps*>(Old);
		// D-16: skip-when-equal against the WIDGET so the caret survives the typing round-trip.
		if (N.HasText() && W.GetText().ToString() != N.Text.ToString())
		{
			W.SetText(N.Text);
		}
		if (N.HasHintText() &&
			(O == nullptr || !O->HasHintText() ||
			 !(N.HintText.IdenticalTo(O->HintText) || N.HintText.ToString() == O->HintText.ToString())))
		{
			W.SetHintText(N.HintText);
		}
		RUITK_ROW(bIsReadOnly, W.SetIsReadOnly(N.bIsReadOnly))
		RUITK_ROW(bIsPassword, W.SetIsPassword(N.bIsPassword))
		RUITK_ROW(MinDesiredWidth, W.SetMinDesiredWidth(N.MinDesiredWidth))
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SInlineEditableTextBlock (wave 2) — click-to-edit label; bMultiLine construct-only.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkInlineEditableTextBlockAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	virtual uint64 GetReconstructMask() const override
	{
		return 1ull << FRuitkInlineEditableTextBlockProps::bMultiLine_Bit;
	}

	virtual bool ConstructOnlyChanged(const FRuitkPropsBase& Old, const FRuitkPropsBase& New) const override
	{
		const FRuitkInlineEditableTextBlockProps& O = static_cast<const FRuitkInlineEditableTextBlockProps&>(Old);
		const FRuitkInlineEditableTextBlockProps& N = static_cast<const FRuitkInlineEditableTextBlockProps&>(New);
		return RUITK_CTOR_CHANGED(bMultiLine);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props,
											 const TSharedPtr<FRuitkEventProxy>& Proxy) override
	{
		const FRuitkInlineEditableTextBlockProps& P = static_cast<const FRuitkInlineEditableTextBlockProps&>(Props);
		return SNew(SInlineEditableTextBlock)
			.MultiLine(P.HasbMultiLine() && P.bMultiLine)
			.OnTextCommitted(FOnTextCommitted::CreateSP(
				Proxy.ToSharedRef(), &FRuitkEventProxy::HandleTextCommit,
				static_cast<int32>(FRuitkInlineEditableTextBlockProps::OnTextCommitted_Bit)));
	}

	virtual void SyncEventHandlers(FRuitkEventProxy& Proxy, const FRuitkPropsBase& New) override
	{
		const FRuitkInlineEditableTextBlockProps& N = static_cast<const FRuitkInlineEditableTextBlockProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuitkInlineEditableTextBlockProps::OnTextCommitted_Bit),
						 N.OnTextCommitted);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SInlineEditableTextBlock& W = static_cast<SInlineEditableTextBlock&>(Widget);
		const FRuitkInlineEditableTextBlockProps& N = static_cast<const FRuitkInlineEditableTextBlockProps&>(New);
		const FRuitkInlineEditableTextBlockProps* O = static_cast<const FRuitkInlineEditableTextBlockProps*>(Old);
		// No widget-side text getter — diff against the previous PROPS (commit-to-commit).
		if (N.HasText() && (O == nullptr || !O->HasText() ||
							!(N.Text.IdenticalTo(O->Text) || N.Text.ToString() == O->Text.ToString())))
		{
			W.SetText(N.Text);
		}
		if (N.HasHintText() &&
			(O == nullptr || !O->HasHintText() ||
			 !(N.HintText.IdenticalTo(O->HintText) || N.HintText.ToString() == O->HintText.ToString())))
		{
			W.SetHintText(N.HintText);
		}
		RUITK_ROW(bIsReadOnly, W.SetReadOnly(N.bIsReadOnly))
		RUITK_ROW(WrapTextAt, W.SetWrapTextAt(N.WrapTextAt))
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SVirtualKeyboardEntry (wave 2) — mobile OS-keyboard field; Text live, the rest masked.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkVirtualKeyboardEntryAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	virtual uint64 GetReconstructMask() const override
	{
		return (1ull << FRuitkVirtualKeyboardEntryProps::HintText_Bit) |
			   (1ull << FRuitkVirtualKeyboardEntryProps::bIsReadOnly_Bit) |
			   (1ull << FRuitkVirtualKeyboardEntryProps::KeyboardType_Bit);
	}

	virtual bool ConstructOnlyChanged(const FRuitkPropsBase& Old, const FRuitkPropsBase& New) const override
	{
		const FRuitkVirtualKeyboardEntryProps& O = static_cast<const FRuitkVirtualKeyboardEntryProps&>(Old);
		const FRuitkVirtualKeyboardEntryProps& N = static_cast<const FRuitkVirtualKeyboardEntryProps&>(New);
		const bool bHint = N.HasHintText() && (!O.HasHintText() || !(N.HintText.IdenticalTo(O.HintText) ||
																	 N.HintText.ToString() == O.HintText.ToString()));
		return bHint || RUITK_CTOR_CHANGED(bIsReadOnly) || RUITK_CTOR_CHANGED(KeyboardType);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props,
											 const TSharedPtr<FRuitkEventProxy>& Proxy) override
	{
		const FRuitkVirtualKeyboardEntryProps& P = static_cast<const FRuitkVirtualKeyboardEntryProps&>(Props);
		const EKeyboardType Keyboard = P.KeyboardType == FName(TEXT("number"))	   ? Keyboard_Number
									   : P.KeyboardType == FName(TEXT("web"))	   ? Keyboard_Web
									   : P.KeyboardType == FName(TEXT("email"))	   ? Keyboard_Email
									   : P.KeyboardType == FName(TEXT("password")) ? Keyboard_Password
																				   : Keyboard_Default;
		return SNew(SVirtualKeyboardEntry)
			.Text(P.Text)
			.HintText(P.HintText)
			.IsReadOnly(P.HasbIsReadOnly() && P.bIsReadOnly)
			.KeyboardType(Keyboard)
			.OnTextChanged(
				FOnTextChanged::CreateSP(Proxy.ToSharedRef(), &FRuitkEventProxy::HandleText,
										 static_cast<int32>(FRuitkVirtualKeyboardEntryProps::OnTextChanged_Bit)))
			.OnTextCommitted(
				FOnTextCommitted::CreateSP(Proxy.ToSharedRef(), &FRuitkEventProxy::HandleTextCommit,
										   static_cast<int32>(FRuitkVirtualKeyboardEntryProps::OnTextCommitted_Bit)));
	}

	virtual void SyncEventHandlers(FRuitkEventProxy& Proxy, const FRuitkPropsBase& New) override
	{
		const FRuitkVirtualKeyboardEntryProps& N = static_cast<const FRuitkVirtualKeyboardEntryProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuitkVirtualKeyboardEntryProps::OnTextChanged_Bit), N.OnTextChanged);
		Proxy.SetHandler(static_cast<int32>(FRuitkVirtualKeyboardEntryProps::OnTextCommitted_Bit), N.OnTextCommitted);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SVirtualKeyboardEntry& W = static_cast<SVirtualKeyboardEntry&>(Widget);
		const FRuitkVirtualKeyboardEntryProps& N = static_cast<const FRuitkVirtualKeyboardEntryProps&>(New);
		if (N.HasText() && W.GetText().ToString() != N.Text.ToString()) // D-16
		{
			W.SetText(N.Text);
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SColorGradingWheel (wave 2; AdvancedWidgets module) — live attribute setters throughout.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkColorGradingWheelAdapter final : public IRuitkElementAdapter
{
	using SWheel = UE::ColorGrading::SColorGradingWheel;

public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	// The attribute setters are PROTECTED in the engine class - construct-only from outside.
	virtual uint64 GetReconstructMask() const override
	{
		return (1ull << FRuitkColorGradingWheelProps::SelectedColor_Bit) |
			   (1ull << FRuitkColorGradingWheelProps::DesiredWheelSize_Bit) |
			   (1ull << FRuitkColorGradingWheelProps::ExponentDisplacement_Bit);
	}

	virtual bool ConstructOnlyChanged(const FRuitkPropsBase& Old, const FRuitkPropsBase& New) const override
	{
		const FRuitkColorGradingWheelProps& O = static_cast<const FRuitkColorGradingWheelProps&>(Old);
		const FRuitkColorGradingWheelProps& N = static_cast<const FRuitkColorGradingWheelProps&>(New);
		return RUITK_CTOR_CHANGED(SelectedColor) || RUITK_CTOR_CHANGED(DesiredWheelSize) ||
			   RUITK_CTOR_CHANGED(ExponentDisplacement);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props,
											 const TSharedPtr<FRuitkEventProxy>& Proxy) override
	{
		const FRuitkColorGradingWheelProps& P = static_cast<const FRuitkColorGradingWheelProps&>(Props);
		SWheel::FArguments Args;
		Args.SelectedColor(P.HasSelectedColor() ? P.SelectedColor : FLinearColor::White)
			.OnValueChanged(SWheel::FOnColorGradingWheelValueChanged::CreateSP(
				Proxy.ToSharedRef(), &FRuitkEventProxy::HandleColorRef,
				static_cast<int32>(FRuitkColorGradingWheelProps::OnValueChanged_Bit)))
			.OnMouseCaptureBegin(SWheel::FOnColorGradingWheelMouseCapture::CreateSP(
				Proxy.ToSharedRef(), &FRuitkEventProxy::HandleColorRef,
				static_cast<int32>(FRuitkColorGradingWheelProps::OnMouseCaptureBegin_Bit)))
			.OnMouseCaptureEnd(SWheel::FOnColorGradingWheelMouseCapture::CreateSP(
				Proxy.ToSharedRef(), &FRuitkEventProxy::HandleColorRef,
				static_cast<int32>(FRuitkColorGradingWheelProps::OnMouseCaptureEnd_Bit)));
		if (P.HasDesiredWheelSize())
		{
			Args.DesiredWheelSize(P.DesiredWheelSize);
		}
		if (P.HasExponentDisplacement())
		{
			Args.ExponentDisplacement(P.ExponentDisplacement);
		}
		TSharedRef<SWheel> W = SNew(SWheel);
		W->Construct(Args);
		return W;
	}

	virtual void SyncEventHandlers(FRuitkEventProxy& Proxy, const FRuitkPropsBase& New) override
	{
		const FRuitkColorGradingWheelProps& N = static_cast<const FRuitkColorGradingWheelProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuitkColorGradingWheelProps::OnValueChanged_Bit), N.OnValueChanged);
		Proxy.SetHandler(static_cast<int32>(FRuitkColorGradingWheelProps::OnMouseCaptureBegin_Bit),
						 N.OnMouseCaptureBegin);
		Proxy.SetHandler(static_cast<int32>(FRuitkColorGradingWheelProps::OnMouseCaptureEnd_Bit), N.OnMouseCaptureEnd);
	}

	virtual void ApplyDiff(SWidget&, const FRuitkPropsBase*, const FRuitkPropsBase&) override {} // all masked
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// Type ids + factories + registration
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace Ruitk::Slate
{
	namespace
	{
		template <typename TProps>
		FRuitkNode MakeHostNodeB3(FRuitkElementTypeId Type, TProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
		{
			FRuitkNode Node;
			Node.Kind = ERuitkNodeKind::Host;
			Node.ElementType = Type;
			Node.Props = MakeShared<TProps>(MoveTemp(Props));
			Node.Children = Ruitk::MakeChildren(MoveTemp(Children));
			Node.Key = Key;
			return Node;
		}

		FRuitkElementTypeId ColorBlockType()
		{
			return Ruitk::InternElementType(FName(TEXT("ColorBlock")));
		}
		FRuitkElementTypeId SimpleGradientType()
		{
			return Ruitk::InternElementType(FName(TEXT("SimpleGradient")));
		}
		FRuitkElementTypeId ComplexGradientType()
		{
			return Ruitk::InternElementType(FName(TEXT("ComplexGradient")));
		}
		FRuitkElementTypeId HyperlinkType()
		{
			return Ruitk::InternElementType(FName(TEXT("Hyperlink")));
		}
		FRuitkElementTypeId EnableBoxType()
		{
			return Ruitk::InternElementType(FName(TEXT("EnableBox")));
		}
		FRuitkElementTypeId ScissorRectBoxType()
		{
			return Ruitk::InternElementType(FName(TEXT("ScissorRectBox")));
		}
		FRuitkElementTypeId BackgroundBlurType()
		{
			return Ruitk::InternElementType(FName(TEXT("BackgroundBlur")));
		}
		FRuitkElementTypeId InvalidationPanelType()
		{
			return Ruitk::InternElementType(FName(TEXT("InvalidationPanel")));
		}
		FRuitkElementTypeId VolumeControlType()
		{
			return Ruitk::InternElementType(FName(TEXT("VolumeControl")));
		}
		FRuitkElementTypeId TextScrollerType()
		{
			return Ruitk::InternElementType(FName(TEXT("TextScroller")));
		}
		FRuitkElementTypeId RadialBoxType()
		{
			return Ruitk::InternElementType(FName(TEXT("RadialBox")));
		}
		FRuitkElementTypeId ColorWheelType()
		{
			return Ruitk::InternElementType(FName(TEXT("ColorWheel")));
		}
		FRuitkElementTypeId ColorSpectrumType()
		{
			return Ruitk::InternElementType(FName(TEXT("ColorSpectrum")));
		}
		FRuitkElementTypeId LayeredImageType()
		{
			return Ruitk::InternElementType(FName(TEXT("LayeredImage")));
		}
		FRuitkElementTypeId InputKeySelectorType()
		{
			return Ruitk::InternElementType(FName(TEXT("InputKeySelector")));
		}
		FRuitkElementTypeId EditableTextType()
		{
			return Ruitk::InternElementType(FName(TEXT("EditableText")));
		}
		FRuitkElementTypeId InlineEditableTextBlockType()
		{
			return Ruitk::InternElementType(FName(TEXT("InlineEditableTextBlock")));
		}
		FRuitkElementTypeId VirtualKeyboardEntryType()
		{
			return Ruitk::InternElementType(FName(TEXT("VirtualKeyboardEntry")));
		}
		FRuitkElementTypeId ColorGradingWheelType()
		{
			return Ruitk::InternElementType(FName(TEXT("ColorGradingWheel")));
		}
	} // namespace

	FRuitkNode ColorBlock(FRuitkColorBlockProps Props, FRuitkKey Key)
	{
		return MakeHostNodeB3(ColorBlockType(), MoveTemp(Props), TArray<FRuitkNode>(), Key);
	}
	FRuitkNode SimpleGradient(FRuitkSimpleGradientProps Props, FRuitkKey Key)
	{
		return MakeHostNodeB3(SimpleGradientType(), MoveTemp(Props), TArray<FRuitkNode>(), Key);
	}
	FRuitkNode ComplexGradient(FRuitkComplexGradientProps Props, FRuitkKey Key)
	{
		return MakeHostNodeB3(ComplexGradientType(), MoveTemp(Props), TArray<FRuitkNode>(), Key);
	}
	FRuitkNode Hyperlink(FRuitkHyperlinkProps Props, FRuitkKey Key)
	{
		return MakeHostNodeB3(HyperlinkType(), MoveTemp(Props), TArray<FRuitkNode>(), Key);
	}
	FRuitkNode EnableBox(FRuitkEnableBoxProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		return MakeHostNodeB3(EnableBoxType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuitkNode ScissorRectBox(FRuitkScissorRectBoxProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		return MakeHostNodeB3(ScissorRectBoxType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuitkNode BackgroundBlur(FRuitkBackgroundBlurProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		return MakeHostNodeB3(BackgroundBlurType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuitkNode InvalidationPanel(FRuitkInvalidationPanelProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		return MakeHostNodeB3(InvalidationPanelType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuitkNode VolumeControl(FRuitkVolumeControlProps Props, FRuitkKey Key)
	{
		return MakeHostNodeB3(VolumeControlType(), MoveTemp(Props), TArray<FRuitkNode>(), Key);
	}
	FRuitkNode TextScroller(FRuitkTextScrollerProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		return MakeHostNodeB3(TextScrollerType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuitkNode RadialBox(FRuitkRadialBoxProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		return MakeHostNodeB3(RadialBoxType(), MoveTemp(Props), MoveTemp(Children), Key);
	}
	FRuitkNode ColorWheel(FRuitkColorWheelProps Props, FRuitkKey Key)
	{
		return MakeHostNodeB3(ColorWheelType(), MoveTemp(Props), TArray<FRuitkNode>(), Key);
	}
	FRuitkNode ColorSpectrum(FRuitkColorSpectrumProps Props, FRuitkKey Key)
	{
		return MakeHostNodeB3(ColorSpectrumType(), MoveTemp(Props), TArray<FRuitkNode>(), Key);
	}
	FRuitkNode LayeredImage(FRuitkLayeredImageProps Props, FRuitkKey Key)
	{
		return MakeHostNodeB3(LayeredImageType(), MoveTemp(Props), TArray<FRuitkNode>(), Key);
	}
	FRuitkNode InputKeySelector(FRuitkInputKeySelectorProps Props, FRuitkKey Key)
	{
		return MakeHostNodeB3(InputKeySelectorType(), MoveTemp(Props), TArray<FRuitkNode>(), Key);
	}
	FRuitkNode EditableText(FRuitkEditableTextProps Props, FRuitkKey Key)
	{
		return MakeHostNodeB3(EditableTextType(), MoveTemp(Props), TArray<FRuitkNode>(), Key);
	}
	FRuitkNode InlineEditableTextBlock(FRuitkInlineEditableTextBlockProps Props, FRuitkKey Key)
	{
		return MakeHostNodeB3(InlineEditableTextBlockType(), MoveTemp(Props), TArray<FRuitkNode>(), Key);
	}
	FRuitkNode VirtualKeyboardEntry(FRuitkVirtualKeyboardEntryProps Props, FRuitkKey Key)
	{
		return MakeHostNodeB3(VirtualKeyboardEntryType(), MoveTemp(Props), TArray<FRuitkNode>(), Key);
	}
	FRuitkNode ColorGradingWheel(FRuitkColorGradingWheelProps Props, FRuitkKey Key)
	{
		return MakeHostNodeB3(ColorGradingWheelType(), MoveTemp(Props), TArray<FRuitkNode>(), Key);
	}

	namespace Detail
	{
		void RegisterBatch3WidgetAdapters()
		{
			RegisterAdapter(ColorBlockType(), MakeUnique<FRuitkColorBlockAdapter>());
			RegisterAdapter(SimpleGradientType(), MakeUnique<FRuitkSimpleGradientAdapter>());
			RegisterAdapter(ComplexGradientType(), MakeUnique<FRuitkComplexGradientAdapter>());
			RegisterAdapter(HyperlinkType(), MakeUnique<FRuitkHyperlinkAdapter>());
			RegisterAdapter(EnableBoxType(), MakeUnique<FRuitkEnableBoxAdapter>());
			RegisterAdapter(ScissorRectBoxType(), MakeUnique<FRuitkScissorRectBoxAdapter>());
			RegisterAdapter(BackgroundBlurType(), MakeUnique<FRuitkBackgroundBlurAdapter>());
			RegisterAdapter(InvalidationPanelType(), MakeUnique<FRuitkInvalidationPanelAdapter>());
			RegisterAdapter(VolumeControlType(), MakeUnique<FRuitkVolumeControlAdapter>());
			RegisterAdapter(TextScrollerType(), MakeUnique<FRuitkTextScrollerAdapter>());
			RegisterAdapter(RadialBoxType(), MakeUnique<FRuitkRadialBoxAdapter>());
			RegisterAdapter(ColorWheelType(), MakeUnique<FRuitkColorWheelAdapter>());
			RegisterAdapter(ColorSpectrumType(), MakeUnique<FRuitkColorSpectrumAdapter>());
			RegisterAdapter(LayeredImageType(), MakeUnique<FRuitkLayeredImageAdapter>());
			RegisterAdapter(InputKeySelectorType(), MakeUnique<FRuitkInputKeySelectorAdapter>());
			RegisterAdapter(EditableTextType(), MakeUnique<FRuitkRawEditableTextAdapter>());
			RegisterAdapter(InlineEditableTextBlockType(), MakeUnique<FRuitkInlineEditableTextBlockAdapter>());
			RegisterAdapter(VirtualKeyboardEntryType(), MakeUnique<FRuitkVirtualKeyboardEntryAdapter>());
			RegisterAdapter(ColorGradingWheelType(), MakeUnique<FRuitkColorGradingWheelAdapter>());
		}
	} // namespace Detail
} // namespace Ruitk::Slate

#undef RUITK_ROW
#undef RUITK_CTOR_CHANGED
