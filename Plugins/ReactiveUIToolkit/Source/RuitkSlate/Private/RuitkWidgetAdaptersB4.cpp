// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Batch 3 wave 3 (WIDGET_COMPLETION_PLAN §3) — the protocol widgets. P5a: SConstraintCanvas's
// anchor slot model (Slot.Offset / Slot.Anchors / Slot.Alignment / Slot.AutoSize / Slot.ZOrder —
// ALL live per-slot setters, mirroring the SCanvas adapter's in-place slot mutation).

#include "RuitkElementAdapter.h"
#include "RuitkEventProxy.h"
#include "RuitkSlateElements.h"
#include "RuitkSlotValue.h"

#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Input/SNumericDropDown.h"
#include "Widgets/Navigation/SBreadcrumbTrail.h"
#include "Misc/EngineVersionComparison.h"
#if !UE_VERSION_OLDER_THAN(5, 7, 0)
#include "SSearchableComboBox.h"
#include "Widgets/Text/STextBlock.h"
#endif
#include "RuitkSlateHost.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Input/SRotatorInputBox.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "Widgets/Layout/SLinkedBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/Layout/SWindowTitleBarArea.h"
#include "Widgets/SNullWidget.h"

namespace
{
	const FName SlotOffsetKey(TEXT("slot.offset"));
	const FName SlotAnchorsKey(TEXT("slot.anchors"));
	const FName SlotAlignmentKey(TEXT("slot.alignment"));
	const FName SlotAutoSizeKey(TEXT("slot.autosize"));
	const FName SlotZOrderKeyB4(TEXT("slot.zorder"));

	/** FMargin from Float (uniform) | Vector2 (h, v) | "l,t,r,b" string. */
	FMargin MarginOf(const FRuitkValue& V)
	{
		switch (V.Kind)
		{
		case FRuitkValue::EKind::Int:
			return FMargin(static_cast<float>(V.IntValue));
		case FRuitkValue::EKind::Float:
			return FMargin(static_cast<float>(V.FloatValue));
		case FRuitkValue::EKind::Vector2:
			return FMargin(static_cast<float>(V.Vector2Value.X), static_cast<float>(V.Vector2Value.Y));
		default:
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
			return FMargin(Parts.Num() == 1 ? FCString::Atof(*Parts[0]) : 0.0f);
		}
		}
	}

	/** FAnchors from Vector2 (uniform min=max) | "minX,minY,maxX,maxY" | "x,y" | uniform num. */
	FAnchors AnchorsOf(const FRuitkValue& V)
	{
		switch (V.Kind)
		{
		case FRuitkValue::EKind::Int:
			return FAnchors(static_cast<float>(V.IntValue));
		case FRuitkValue::EKind::Float:
			return FAnchors(static_cast<float>(V.FloatValue));
		case FRuitkValue::EKind::Vector2:
			return FAnchors(static_cast<float>(V.Vector2Value.X), static_cast<float>(V.Vector2Value.Y));
		default:
		{
			const FString S = V.Kind == FRuitkValue::EKind::Name ? V.NameValue.ToString() : V.StringValue;
			TArray<FString> Parts;
			S.ParseIntoArray(Parts, TEXT(","), true);
			if (Parts.Num() == 4)
			{
				return FAnchors(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]),
								FCString::Atof(*Parts[3]));
			}
			if (Parts.Num() == 2)
			{
				return FAnchors(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]));
			}
			return FAnchors(Parts.Num() == 1 ? FCString::Atof(*Parts[0]) : 0.0f);
		}
		}
	}

	/** Apply the anchor-slot keys to a live SConstraintCanvas slot (absent keys -> defaults). */
	void ApplyConstraintSlot(SConstraintCanvas::FSlot& Slot, const FRuitkStyleDict* SlotProps)
	{
		const FRuitkValue* Offset = SlotProps != nullptr ? SlotProps->Find(SlotOffsetKey) : nullptr;
		const FRuitkValue* Anchors = SlotProps != nullptr ? SlotProps->Find(SlotAnchorsKey) : nullptr;
		const FRuitkValue* Alignment = SlotProps != nullptr ? SlotProps->Find(SlotAlignmentKey) : nullptr;
		const FRuitkValue* AutoSize = SlotProps != nullptr ? SlotProps->Find(SlotAutoSizeKey) : nullptr;
		const FRuitkValue* ZOrder = SlotProps != nullptr ? SlotProps->Find(SlotZOrderKeyB4) : nullptr;
		Slot.SetOffset(Offset != nullptr ? MarginOf(*Offset) : FMargin(0.f, 0.f, 1.f, 1.f));
		Slot.SetAnchors(Anchors != nullptr ? AnchorsOf(*Anchors) : FAnchors(0.f));
		Slot.SetAlignment(Alignment != nullptr ? Ruitk::Slate::SlotValue::AsVector2(*Alignment) : FVector2D(0.5, 0.5));
		Slot.SetAutoSize(AutoSize != nullptr && Ruitk::Slate::SlotValue::AsBool(*AutoSize)); // R11: parses "true"
		Slot.SetZOrder(ZOrder != nullptr ? Ruitk::Slate::SlotValue::AsFloat(*ZOrder) : 0.f);
	}
} // namespace

// ─────────────────────────────────────────────────────────────────────────────────────────
// SConstraintCanvas (P5a) — anchor-based absolute panel; live in-place slot mutation.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkConstraintCanvasAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::MultiSlot; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SConstraintCanvas);
	}

	virtual void ApplyDiff(SWidget&, const FRuitkPropsBase*, const FRuitkPropsBase&) override {}

	virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32,
							 const FRuitkStyleDict* SlotProps) override
	{
		SConstraintCanvas& W = static_cast<SConstraintCanvas&>(Parent);
		{
			SConstraintCanvas::FScopedWidgetSlotArguments Slot = W.AddSlot();
			Slot.AttachWidget(Child);
		} // the scoped args must COMMIT before SetZOrder is legal on the slot
		FChildren* Children = W.GetChildren();
		ApplyConstraintSlot(const_cast<SConstraintCanvas::FSlot&>(
								static_cast<const SConstraintCanvas::FSlot&>(Children->GetSlotAt(Children->Num() - 1))),
							SlotProps);
	}

	virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) override
	{
		static_cast<SConstraintCanvas&>(Parent).RemoveSlot(Child);
	}

	virtual void ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
								 TFunctionRef<const FRuitkStyleDict*(const TSharedRef<SWidget>&)> SlotPropsOf) override
	{
		SConstraintCanvas& W = static_cast<SConstraintCanvas&>(Parent);
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			W.RemoveSlot(Child);
		}
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			{
				SConstraintCanvas::FScopedWidgetSlotArguments Slot = W.AddSlot();
				Slot.AttachWidget(Child);
			}
			FChildren* Children = W.GetChildren();
			ApplyConstraintSlot(const_cast<SConstraintCanvas::FSlot&>(static_cast<const SConstraintCanvas::FSlot&>(
									Children->GetSlotAt(Children->Num() - 1))),
								SlotPropsOf(Child));
		}
	}

	virtual void UpdateChildSlotProps(SWidget& Parent, const TSharedRef<SWidget>& Child,
									  const FRuitkStyleDict* SlotProps) override
	{
		SConstraintCanvas& W = static_cast<SConstraintCanvas&>(Parent);
		FChildren* Children = W.GetChildren();
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			if (&Children->GetChildAt(i).Get() == &Child.Get())
			{
				ApplyConstraintSlot(const_cast<SConstraintCanvas::FSlot&>(
										static_cast<const SConstraintCanvas::FSlot&>(Children->GetSlotAt(i))),
									SlotProps);
				W.Invalidate(EInvalidateWidgetReason::Layout);
				return;
			}
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SSplitter (P5b) — resizable panes; live per-slot fraction/rule/min/resizable setters.
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace
{
	const FName SlotSizeRuleKey(TEXT("slot.sizerule"));
	const FName SlotSizeValueKey(TEXT("slot.sizevalue"));
	const FName SlotMinSizeKey(TEXT("slot.minsize"));
	const FName SlotResizableKey(TEXT("slot.resizable"));

	void ApplySplitterSlot(SSplitter::FSlot& Slot, const FRuitkStyleDict* SlotProps)
	{
		const FRuitkValue* Rule = SlotProps != nullptr ? SlotProps->Find(SlotSizeRuleKey) : nullptr;
		const FRuitkValue* Value = SlotProps != nullptr ? SlotProps->Find(SlotSizeValueKey) : nullptr;
		const FRuitkValue* MinSize = SlotProps != nullptr ? SlotProps->Find(SlotMinSizeKey) : nullptr;
		const FRuitkValue* Resizable = SlotProps != nullptr ? SlotProps->Find(SlotResizableKey) : nullptr;
		const bool bSizeToContent =
			Rule != nullptr && (Rule->Kind == FRuitkValue::EKind::Name ? Rule->NameValue : FName(*Rule->StringValue)) ==
								   FName(TEXT("sizeToContent"));
		Slot.SetSizingRule(bSizeToContent ? SSplitter::SizeToContent : SSplitter::FractionOfParent);
		Slot.SetSizeValue(Value != nullptr ? Ruitk::Slate::SlotValue::AsFloat(*Value) : 1.0f);
		Slot.SetMinSize(MinSize != nullptr ? Ruitk::Slate::SlotValue::AsFloat(*MinSize) : 20.0f);
		Slot.SetResizable(Resizable == nullptr ||
						  Ruitk::Slate::SlotValue::AsBool(*Resizable, true)); // R11: parses "false"
	}
} // namespace

class FRuitkSplitterAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::MultiSlot; }
	virtual bool HasEvents() const override { return true; }

	virtual uint64 GetReconstructMask() const override
	{
		return 1ull << FRuitkSplitterProps::PhysicalSplitterHandleSize_Bit;
	}

	virtual bool ConstructOnlyChanged(const FRuitkPropsBase& Old, const FRuitkPropsBase& New) const override
	{
		const FRuitkSplitterProps& O = static_cast<const FRuitkSplitterProps&>(Old);
		const FRuitkSplitterProps& N = static_cast<const FRuitkSplitterProps&>(New);
		return N.HasPhysicalSplitterHandleSize() &&
			   (!O.HasPhysicalSplitterHandleSize() || !(O.PhysicalSplitterHandleSize == N.PhysicalSplitterHandleSize));
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props,
											 const TSharedPtr<FRuitkEventProxy>& Proxy) override
	{
		const FRuitkSplitterProps& P = static_cast<const FRuitkSplitterProps&>(Props);
		return SNew(SSplitter)
			.Orientation(P.HasOrientation() && P.Orientation == FName(TEXT("vertical")) ? Orient_Vertical
																						: Orient_Horizontal)
			.PhysicalSplitterHandleSize(P.HasPhysicalSplitterHandleSize() ? P.PhysicalSplitterHandleSize : 5.0f)
			.OnSplitterFinishedResizing(
				FSimpleDelegate::CreateSP(Proxy.ToSharedRef(), &FRuitkEventProxy::HandleVoid,
										  static_cast<int32>(FRuitkSplitterProps::OnSplitterFinishedResizing_Bit)));
	}

	virtual void SyncEventHandlers(FRuitkEventProxy& Proxy, const FRuitkPropsBase& New) override
	{
		const FRuitkSplitterProps& N = static_cast<const FRuitkSplitterProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuitkSplitterProps::OnSplitterFinishedResizing_Bit),
						 N.OnSplitterFinishedResizing);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SSplitter& W = static_cast<SSplitter&>(Widget);
		const FRuitkSplitterProps& N = static_cast<const FRuitkSplitterProps&>(New);
		const FRuitkSplitterProps* O = static_cast<const FRuitkSplitterProps*>(Old);
		if (N.HasOrientation() && (O == nullptr || !O->HasOrientation() || !(N.Orientation == O->Orientation)))
		{
			W.SetOrientation(N.Orientation == FName(TEXT("vertical")) ? Orient_Vertical : Orient_Horizontal);
		}
	}

	virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32,
							 const FRuitkStyleDict* SlotProps) override
	{
		SSplitter& W = static_cast<SSplitter&>(Parent);
		{
			SSplitter::FScopedWidgetSlotArguments Slot = W.AddSlot();
			Slot.AttachWidget(Child);
		} // the scoped-args dtor LANDS the slot (owner widget wired) — UE 5.8's attribute-backed
		  // slot setters assert "Slot Attributes has to be registered after the FSlot is
		  // constructed" if touched before this point, so props apply to the LIVE slot below.
		UpdateChildSlotProps(Parent, Child, SlotProps);
	}

	virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) override
	{
		SSplitter& W = static_cast<SSplitter&>(Parent);
		FChildren* Children = W.GetChildren();
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			if (&Children->GetChildAt(i).Get() == &Child.Get())
			{
				W.RemoveAt(i);
				return;
			}
		}
	}

	virtual void ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
								 TFunctionRef<const FRuitkStyleDict*(const TSharedRef<SWidget>&)> SlotPropsOf) override
	{
		SSplitter& W = static_cast<SSplitter&>(Parent);
		while (W.GetChildren()->Num() > 0)
		{
			W.RemoveAt(0);
		}
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			SSplitter::FScopedWidgetSlotArguments Slot = W.AddSlot();
			Slot.AttachWidget(Child);
		} // each scoped-args dtor lands its slot before the next iteration
		// Apply slot props to the LANDED slots (5.8 slot-attribute contract — see InsertChild);
		// after the re-add loop, child order matches Ordered index-for-index.
		FChildren* Children = W.GetChildren();
		for (int32 i = 0; i < Children->Num() && i < Ordered.Num(); ++i)
		{
			ApplySplitterSlot(
				const_cast<SSplitter::FSlot&>(static_cast<const SSplitter::FSlot&>(Children->GetSlotAt(i))),
				SlotPropsOf(Ordered[i]));
		}
	}

	virtual void UpdateChildSlotProps(SWidget& Parent, const TSharedRef<SWidget>& Child,
									  const FRuitkStyleDict* SlotProps) override
	{
		SSplitter& W = static_cast<SSplitter&>(Parent);
		FChildren* Children = W.GetChildren();
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			if (&Children->GetChildAt(i).Get() == &Child.Get())
			{
				ApplySplitterSlot(
					const_cast<SSplitter::FSlot&>(static_cast<const SSplitter::FSlot&>(Children->GetSlotAt(i))),
					SlotProps);
				W.Invalidate(EInvalidateWidgetReason::Layout);
				return;
			}
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SSplitter2x2 (D-W4) — four resizable quadrants; children route by slot.role
// ("topLeft" default / "bottomLeft" / "topRight" / "bottomRight" — live Set*Content).
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace
{
	const FName SlotRoleKey2x2(TEXT("slot.role"));

	/** 0=TL 1=BL 2=TR 3=BR (SSplitter2x2's own percentages order). */
	int32 QuadrantOf(const FRuitkStyleDict* SlotProps)
	{
		if (SlotProps != nullptr)
		{
			if (const FRuitkValue* V = SlotProps->Find(SlotRoleKey2x2))
			{
				const FName Role = V->Kind == FRuitkValue::EKind::Name ? V->NameValue : FName(*V->StringValue);
				if (Role == FName(TEXT("bottomLeft")))
				{
					return 1;
				}
				if (Role == FName(TEXT("topRight")))
				{
					return 2;
				}
				if (Role == FName(TEXT("bottomRight")))
				{
					return 3;
				}
			}
		}
		return 0;
	}

	void SetQuadrant(SSplitter2x2& W, int32 Quadrant, const TSharedRef<SWidget>& Child)
	{
		switch (Quadrant)
		{
		case 1:
			W.SetBottomLeftContent(Child);
			break;
		case 2:
			W.SetTopRightContent(Child);
			break;
		case 3:
			W.SetBottomRightContent(Child);
			break;
		default:
			W.SetTopLeftContent(Child);
			break;
		}
	}
} // namespace

class FRuitkSplitter2x2Adapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::MultiSlot; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SSplitter2x2);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SSplitter2x2& W = static_cast<SSplitter2x2&>(Widget);
		const FRuitkSplitter2x2Props& N = static_cast<const FRuitkSplitter2x2Props&>(New);
		const FRuitkSplitter2x2Props* O = static_cast<const FRuitkSplitter2x2Props*>(Old);
		if (N.HasPercentages() && N.Percentages.Num() == 4 &&
			(O == nullptr || !O->HasPercentages() || !(O->Percentages == N.Percentages)))
		{
			TArray<FVector2D> Percentages = N.Percentages;
			W.SetSplitterPercentages(Percentages);
		}
	}

	virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32,
							 const FRuitkStyleDict* SlotProps) override
	{
		SetQuadrant(static_cast<SSplitter2x2&>(Parent), QuadrantOf(SlotProps), Child);
	}

	virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>& Child) override
	{
		// Find which quadrant holds the child and reset just that one.
		SSplitter2x2& W = static_cast<SSplitter2x2&>(Parent);
		if (&W.GetTopLeftContent().Get() == &Child.Get())
		{
			W.SetTopLeftContent(SNullWidget::NullWidget);
		}
		else if (&W.GetBottomLeftContent().Get() == &Child.Get())
		{
			W.SetBottomLeftContent(SNullWidget::NullWidget);
		}
		else if (&W.GetTopRightContent().Get() == &Child.Get())
		{
			W.SetTopRightContent(SNullWidget::NullWidget);
		}
		else if (&W.GetBottomRightContent().Get() == &Child.Get())
		{
			W.SetBottomRightContent(SNullWidget::NullWidget);
		}
	}

	virtual void ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
								 TFunctionRef<const FRuitkStyleDict*(const TSharedRef<SWidget>&)> SlotPropsOf) override
	{
		SSplitter2x2& W = static_cast<SSplitter2x2&>(Parent);
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			SetQuadrant(W, QuadrantOf(SlotPropsOf(Child)), Child);
		}
	}

	virtual void UpdateChildSlotProps(SWidget& Parent, const TSharedRef<SWidget>& Child,
									  const FRuitkStyleDict* SlotProps) override
	{
		InsertChild(Parent, Child, 0, SlotProps);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SMenuAnchor (P3) — THE popup primitive: anchor child + slot.role="menu" popup content;
// controlled bIsOpen (D-16 skip vs IsOpen()); user dismissals report via OnMenuOpenChanged.
// ─────────────────────────────────────────────────────────────────────────────────────────

namespace
{
	const FName SlotRoleKeyB4(TEXT("slot.role"));

	bool IsMenuRole(const FRuitkStyleDict* SlotProps)
	{
		if (SlotProps != nullptr)
		{
			if (const FRuitkValue* V = SlotProps->Find(SlotRoleKeyB4))
			{
				return (V->Kind == FRuitkValue::EKind::Name ? V->NameValue : FName(*V->StringValue)) ==
					   FName(TEXT("menu"));
			}
		}
		return false;
	}

	EMenuPlacement PlacementOf(FName V)
	{
		return V == FName(TEXT("comboBox"))				 ? MenuPlacement_ComboBox
			   : V == FName(TEXT("belowRightAnchor"))	 ? MenuPlacement_BelowRightAnchor
			   : V == FName(TEXT("aboveAnchor"))		 ? MenuPlacement_AboveAnchor
			   : V == FName(TEXT("centeredAboveAnchor")) ? MenuPlacement_CenteredAboveAnchor
			   : V == FName(TEXT("menuRight"))			 ? MenuPlacement_MenuRight
			   : V == FName(TEXT("menuLeft"))			 ? MenuPlacement_MenuLeft
			   : V == FName(TEXT("center"))				 ? MenuPlacement_Center
			   : V == FName(TEXT("centeredBelowAnchor")) ? MenuPlacement_CenteredBelowAnchor
														 : MenuPlacement_BelowAnchor;
	}
} // namespace

class FRuitkMenuAnchorAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::MultiSlot; }
	virtual bool HasEvents() const override { return true; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props,
											 const TSharedPtr<FRuitkEventProxy>& Proxy) override
	{
		const FRuitkMenuAnchorProps& P = static_cast<const FRuitkMenuAnchorProps&>(Props);
		return SNew(SMenuAnchor)
			.Placement(P.HasPlacement() ? PlacementOf(P.Placement) : MenuPlacement_BelowAnchor)
			.FitInWindow(!P.HasbFitInWindow() || P.bFitInWindow)
			.OnMenuOpenChanged(
				FOnIsOpenChanged::CreateSP(Proxy.ToSharedRef(), &FRuitkEventProxy::HandleBool,
										   static_cast<int32>(FRuitkMenuAnchorProps::OnMenuOpenChanged_Bit)));
	}

	virtual void SyncEventHandlers(FRuitkEventProxy& Proxy, const FRuitkPropsBase& New) override
	{
		const FRuitkMenuAnchorProps& N = static_cast<const FRuitkMenuAnchorProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuitkMenuAnchorProps::OnMenuOpenChanged_Bit), N.OnMenuOpenChanged);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SMenuAnchor& W = static_cast<SMenuAnchor&>(Widget);
		const FRuitkMenuAnchorProps& N = static_cast<const FRuitkMenuAnchorProps&>(New);
		const FRuitkMenuAnchorProps* O = static_cast<const FRuitkMenuAnchorProps*>(Old);
		if (N.HasPlacement() && (O == nullptr || !O->HasPlacement() || !(N.Placement == O->Placement)))
		{
			W.SetMenuPlacement(PlacementOf(N.Placement));
		}
		if (N.HasbFitInWindow() && (O == nullptr || !O->HasbFitInWindow() || O->bFitInWindow != N.bFitInWindow))
		{
			W.SetFitInWindow(N.bFitInWindow);
		}
		// Controlled open state (D-16): skip when the widget already agrees.
		if (N.HasbIsOpen() && W.IsOpen() != N.bIsOpen)
		{
			W.SetIsOpen(N.bIsOpen, /*bFocusMenu*/ true);
		}
	}

	virtual void InsertChild(SWidget& Parent, const TSharedRef<SWidget>& Child, int32,
							 const FRuitkStyleDict* SlotProps) override
	{
		SMenuAnchor& W = static_cast<SMenuAnchor&>(Parent);
		if (IsMenuRole(SlotProps))
		{
			W.SetMenuContent(Child);
		}
		else
		{
			W.SetContent(Child);
		}
	}

	virtual void RemoveChild(SWidget& Parent, const TSharedRef<SWidget>&) override
	{
		// Two logical holders; clearing either resets to null content (cheap — reassigned on
		// the next InsertChild).
		SMenuAnchor& W = static_cast<SMenuAnchor&>(Parent);
		W.SetMenuContent(SNullWidget::NullWidget);
	}

	virtual void ReorderChildren(SWidget& Parent, const TArray<TSharedRef<SWidget>>& Ordered,
								 TFunctionRef<const FRuitkStyleDict*(const TSharedRef<SWidget>&)> SlotPropsOf) override
	{
		SMenuAnchor& W = static_cast<SMenuAnchor&>(Parent);
		for (const TSharedRef<SWidget>& Child : Ordered)
		{
			if (IsMenuRole(SlotPropsOf(Child)))
			{
				W.SetMenuContent(Child);
			}
			else
			{
				W.SetContent(Child);
			}
		}
	}

	virtual void UpdateChildSlotProps(SWidget& Parent, const TSharedRef<SWidget>& Child,
									  const FRuitkStyleDict* SlotProps) override
	{
		InsertChild(Parent, Child, 0, SlotProps);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SWindowTitleBarArea — custom title-bar strip; auto-wired to the game window when present.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkWindowTitleBarAreaAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::SingleContent; }
	virtual bool HasEvents() const override { return true; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props,
											 const TSharedPtr<FRuitkEventProxy>& Proxy) override
	{
		const FRuitkWindowTitleBarAreaProps& P = static_cast<const FRuitkWindowTitleBarAreaProps&>(Props);
		TSharedRef<SWindowTitleBarArea> W =
			SNew(SWindowTitleBarArea)
				.RequestToggleFullscreen(FSimpleDelegate::CreateSP(
					Proxy.ToSharedRef(), &FRuitkEventProxy::HandleVoid,
					static_cast<int32>(FRuitkWindowTitleBarAreaProps::RequestToggleFullscreen_Bit)));
		if (GEngine != nullptr && GEngine->GameViewport != nullptr)
		{
			W->SetGameWindow(GEngine->GameViewport->GetWindow());
		}
		return W;
	}

	virtual void SyncEventHandlers(FRuitkEventProxy& Proxy, const FRuitkPropsBase& New) override
	{
		const FRuitkWindowTitleBarAreaProps& N = static_cast<const FRuitkWindowTitleBarAreaProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuitkWindowTitleBarAreaProps::RequestToggleFullscreen_Bit),
						 N.RequestToggleFullscreen);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SWindowTitleBarArea& W = static_cast<SWindowTitleBarArea&>(Widget);
		const FRuitkWindowTitleBarAreaProps& N = static_cast<const FRuitkWindowTitleBarAreaProps&>(New);
		const FRuitkWindowTitleBarAreaProps* O = static_cast<const FRuitkWindowTitleBarAreaProps*>(Old);
		if (N.HasHAlign() && (O == nullptr || !O->HasHAlign() || !(N.HAlign == O->HAlign)))
		{
			W.SetHAlign(N.HAlign == FName(TEXT("left"))		? HAlign_Left
						: N.HAlign == FName(TEXT("center")) ? HAlign_Center
						: N.HAlign == FName(TEXT("right"))	? HAlign_Right
															: HAlign_Fill);
		}
		if (N.HasVAlign() && (O == nullptr || !O->HasVAlign() || !(N.VAlign == O->VAlign)))
		{
			W.SetVAlign(N.VAlign == FName(TEXT("top"))		? VAlign_Top
						: N.VAlign == FName(TEXT("center")) ? VAlign_Center
						: N.VAlign == FName(TEXT("bottom")) ? VAlign_Bottom
															: VAlign_Fill);
		}
		if (N.HasPadding() && (O == nullptr || !O->HasPadding() || !(N.Padding == O->Padding)))
		{
			W.SetPadding(N.Padding);
		}
	}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		static_cast<SWindowTitleBarArea&>(Parent).SetContent(Child.IsValid() ? Child.ToSharedRef()
																			 : SNullWidget::NullWidget);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SNumericDropDown<float> — preset dropdown; fully attribute/construct-only → masked.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkNumericDropDownAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	virtual uint64 GetReconstructMask() const override
	{
		return (1ull << FRuitkNumericDropDownProps::Values_Bit) | (1ull << FRuitkNumericDropDownProps::Labels_Bit) |
			   (1ull << FRuitkNumericDropDownProps::Value_Bit) |
			   (1ull << FRuitkNumericDropDownProps::bShowNamedValue_Bit);
	}

	virtual bool ConstructOnlyChanged(const FRuitkPropsBase& Old, const FRuitkPropsBase& New) const override
	{
		const FRuitkNumericDropDownProps& O = static_cast<const FRuitkNumericDropDownProps&>(Old);
		const FRuitkNumericDropDownProps& N = static_cast<const FRuitkNumericDropDownProps&>(New);
		auto Changed = [](bool bNewHas, bool bOldHas, const auto& OldV, const auto& NewV)
		{ return bNewHas && (!bOldHas || !(OldV == NewV)); };
		return Changed(N.HasValues(), O.HasValues(), O.Values, N.Values) ||
			   Changed(N.HasLabels(), O.HasLabels(), O.Labels, N.Labels) ||
			   Changed(N.HasValue(), O.HasValue(), O.Value, N.Value) ||
			   Changed(N.HasbShowNamedValue(), O.HasbShowNamedValue(), O.bShowNamedValue, N.bShowNamedValue);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props,
											 const TSharedPtr<FRuitkEventProxy>& Proxy) override
	{
		const FRuitkNumericDropDownProps& P = static_cast<const FRuitkNumericDropDownProps&>(Props);
		TArray<SNumericDropDown<float>::FNamedValue> Named;
		for (int32 i = 0; i < P.Values.Num(); ++i)
		{
			const FText Label =
				P.Labels.IsValidIndex(i) ? FText::FromString(P.Labels[i]) : FText::AsNumber(P.Values[i]);
			Named.Add(SNumericDropDown<float>::FNamedValue(P.Values[i], Label, Label));
		}
		return SNew(SNumericDropDown<float>)
			.DropDownValues(Named)
			.Value(P.HasValue() ? P.Value : 0.0f)
			.bShowNamedValue(P.HasbShowNamedValue() && P.bShowNamedValue)
			.OnValueChanged(SNumericDropDown<float>::FOnValueChanged::CreateSP(
				Proxy.ToSharedRef(), &FRuitkEventProxy::HandleFloat,
				static_cast<int32>(FRuitkNumericDropDownProps::OnValueChanged_Bit)));
	}

	virtual void SyncEventHandlers(FRuitkEventProxy& Proxy, const FRuitkPropsBase& New) override
	{
		const FRuitkNumericDropDownProps& N = static_cast<const FRuitkNumericDropDownProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuitkNumericDropDownProps::OnValueChanged_Bit), N.OnValueChanged);
	}

	virtual void ApplyDiff(SWidget&, const FRuitkPropsBase*, const FRuitkPropsBase&) override {} // all masked
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SBreadcrumbTrail<FString> — declarative Crumbs list converged onto the imperative stack.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkBreadcrumbTrailAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	virtual uint64 GetReconstructMask() const override
	{
		return 1ull << FRuitkBreadcrumbTrailProps::bShowLeadingDelimiter_Bit;
	}

	virtual bool ConstructOnlyChanged(const FRuitkPropsBase& Old, const FRuitkPropsBase& New) const override
	{
		const FRuitkBreadcrumbTrailProps& O = static_cast<const FRuitkBreadcrumbTrailProps&>(Old);
		const FRuitkBreadcrumbTrailProps& N = static_cast<const FRuitkBreadcrumbTrailProps&>(New);
		return N.HasbShowLeadingDelimiter() &&
			   (!O.HasbShowLeadingDelimiter() || O.bShowLeadingDelimiter != N.bShowLeadingDelimiter);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props,
											 const TSharedPtr<FRuitkEventProxy>& Proxy) override
	{
		const FRuitkBreadcrumbTrailProps& P = static_cast<const FRuitkBreadcrumbTrailProps&>(Props);
		TWeakPtr<FRuitkEventProxy> WeakProxy = Proxy;
		TSharedRef<SBreadcrumbTrail<FString>> W =
			SNew(SBreadcrumbTrail<FString>)
				.ShowLeadingDelimiter(P.HasbShowLeadingDelimiter() && P.bShowLeadingDelimiter)
				.PersistentBreadcrumbs(true)
				.OnCrumbClicked(SBreadcrumbTrail<FString>::FOnCrumbClicked::CreateLambda(
					[WeakProxy](const FString& Crumb)
					{
						if (TSharedPtr<FRuitkEventProxy> Pinned = WeakProxy.Pin())
						{
							Pinned->HandleText(FText::FromString(Crumb),
											   static_cast<int32>(FRuitkBreadcrumbTrailProps::OnCrumbClicked_Bit));
						}
					}));
		SyncCrumbs(*W, P);
		return W;
	}

	virtual void SyncEventHandlers(FRuitkEventProxy& Proxy, const FRuitkPropsBase& New) override
	{
		const FRuitkBreadcrumbTrailProps& N = static_cast<const FRuitkBreadcrumbTrailProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuitkBreadcrumbTrailProps::OnCrumbClicked_Bit), N.OnCrumbClicked);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		const FRuitkBreadcrumbTrailProps& N = static_cast<const FRuitkBreadcrumbTrailProps&>(New);
		const FRuitkBreadcrumbTrailProps* O = static_cast<const FRuitkBreadcrumbTrailProps*>(Old);
		if (N.HasCrumbs() && (O == nullptr || !O->HasCrumbs() || !(N.Crumbs == O->Crumbs)))
		{
			SyncCrumbs(static_cast<SBreadcrumbTrail<FString>&>(Widget), N);
		}
	}

private:
	static void SyncCrumbs(SBreadcrumbTrail<FString>& W, const FRuitkBreadcrumbTrailProps& P)
	{
		W.ClearCrumbs();
		for (const FString& Crumb : P.Crumbs)
		{
			W.PushCrumb(FText::FromString(Crumb), Crumb);
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SNotificationList (P4) — toast mount point; pushes ride the P2 command path.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkNotificationListAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SNotificationList);
	}

	virtual void ApplyDiff(SWidget&, const FRuitkPropsBase*, const FRuitkPropsBase&) override {}
};

namespace Ruitk::Slate
{
	void PushNotification(const FRuitkHostHandle& Handle, const FText& Text, float ExpireDuration)
	{
		if (TSharedPtr<SNotificationList> List = WidgetFromHandle<SNotificationList>(Handle))
		{
			if (List->GetType() == FName(TEXT("SNotificationList"))) // guard the cast (P2 rule)
			{
				FNotificationInfo Info(Text);
				Info.ExpireDuration = ExpireDuration;
				List->AddNotification(Info);
			}
		}
	}
} // namespace Ruitk::Slate

// ─────────────────────────────────────────────────────────────────────────────────────────
// SSearchableComboBox — sinceUE 5.7 (absent from 5.6): the adapter compiles out on older
// engines; the tag/props/factory stay (mounting on 5.6 warns unknown-adapter).
// ─────────────────────────────────────────────────────────────────────────────────────────

#if !UE_VERSION_OLDER_THAN(5, 7, 0)
/** Owns the TSharedPtr<FString> options storage SSearchableComboBox borrows a pointer to. */
class SRuitkSearchableComboBox final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRuitkSearchableComboBox) {}
	SLATE_ARGUMENT(TArray<FString>, Options)
	SLATE_ARGUMENT(FString, InitiallySelected)
	SLATE_EVENT(SSearchableComboBox::FOnSelectionChanged, OnSelectionChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		for (const FString& Option : InArgs._Options)
		{
			Options.Add(MakeShared<FString>(Option));
		}
		// clang-format off
		ChildSlot
		[
			SAssignNew(Inner, SSearchableComboBox)
			.OptionsSource(&Options)
			.OnSelectionChanged(InArgs._OnSelectionChanged)
			.OnGenerateWidget(SSearchableComboBox::FOnGenerateWidget::CreateLambda(
				[](TSharedPtr<FString> Item)
				{ return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString())); }))
			.Content()
			[
				SNew(STextBlock)
					.Text_Lambda([this]()
					{
						const TSharedPtr<FString> Sel =
							Inner.IsValid() ? StaticCastSharedPtr<FString>(Inner->GetSelectedItem()) : nullptr;
						return FText::FromString(Sel.IsValid() ? *Sel : FString());
					})
			]
		];
		// clang-format on
		SetSelected(InArgs._InitiallySelected);
	}

	void SetOptions(const TArray<FString>& InOptions)
	{
		Options.Reset();
		for (const FString& Option : InOptions)
		{
			Options.Add(MakeShared<FString>(Option));
		}
		Inner->RefreshOptions();
	}

	void SetSelected(const FString& Value)
	{
		for (const TSharedPtr<FString>& Option : Options)
		{
			if (*Option == Value)
			{
				Inner->SetSelectedItem(Option);
				return;
			}
		}
		Inner->ClearSelection();
	}

	FString GetSelected() const
	{
		const TSharedPtr<FString> Sel = StaticCastSharedPtr<FString>(Inner->GetSelectedItem());
		return Sel.IsValid() ? *Sel : FString();
	}

private:
	TArray<TSharedPtr<FString>> Options;
	TSharedPtr<SSearchableComboBox> Inner;
};

class FRuitkSearchableComboBoxAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props,
											 const TSharedPtr<FRuitkEventProxy>& Proxy) override
	{
		const FRuitkSearchableComboBoxProps& P = static_cast<const FRuitkSearchableComboBoxProps&>(Props);
		TWeakPtr<FRuitkEventProxy> WeakProxy = Proxy;
		return SNew(SRuitkSearchableComboBox)
			.Options(P.Options)
			.InitiallySelected(P.SelectedItem.ToString())
			.OnSelectionChanged(SSearchableComboBox::FOnSelectionChanged::CreateLambda(
				[WeakProxy](TSharedPtr<FString> Item, ESelectInfo::Type)
				{
					if (TSharedPtr<FRuitkEventProxy> Pinned = WeakProxy.Pin())
					{
						Pinned->HandleText(FText::FromString(Item.IsValid() ? *Item : FString()),
										   static_cast<int32>(FRuitkSearchableComboBoxProps::OnSelectionChanged_Bit));
					}
				}));
	}

	virtual void SyncEventHandlers(FRuitkEventProxy& Proxy, const FRuitkPropsBase& New) override
	{
		const FRuitkSearchableComboBoxProps& N = static_cast<const FRuitkSearchableComboBoxProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuitkSearchableComboBoxProps::OnSelectionChanged_Bit),
						 N.OnSelectionChanged);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SRuitkSearchableComboBox& W = static_cast<SRuitkSearchableComboBox&>(Widget);
		const FRuitkSearchableComboBoxProps& N = static_cast<const FRuitkSearchableComboBoxProps&>(New);
		const FRuitkSearchableComboBoxProps* O = static_cast<const FRuitkSearchableComboBoxProps*>(Old);
		if (N.HasOptions() && (O == nullptr || !O->HasOptions() || !(N.Options == O->Options)))
		{
			W.SetOptions(N.Options);
		}
		// Controlled selection (D-16): skip when the widget already agrees.
		if (N.HasSelectedItem() && W.GetSelected() != N.SelectedItem.ToString())
		{
			W.SetSelected(N.SelectedItem.ToString());
		}
	}
};
#endif // !UE_VERSION_OLDER_THAN(5, 7, 0)

// ─────────────────────────────────────────────────────────────────────────────────────────
// SLinkedBox — uniform-size sibling groups via a shared, adapter-owned FLinkedBoxManager.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkLinkedBoxAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::SingleContent; }

	virtual uint64 GetReconstructMask() const override { return 1ull << FRuitkLinkedBoxProps::GroupKey_Bit; }

	virtual bool ConstructOnlyChanged(const FRuitkPropsBase& Old, const FRuitkPropsBase& New) const override
	{
		const FRuitkLinkedBoxProps& O = static_cast<const FRuitkLinkedBoxProps&>(Old);
		const FRuitkLinkedBoxProps& N = static_cast<const FRuitkLinkedBoxProps&>(New);
		return N.HasGroupKey() && (!O.HasGroupKey() || !(O.GroupKey == N.GroupKey));
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props, const TSharedPtr<FRuitkEventProxy>&) override
	{
		const FRuitkLinkedBoxProps& P = static_cast<const FRuitkLinkedBoxProps&>(Props);
		const FName Group = P.HasGroupKey() ? P.GroupKey : NAME_None;
		TSharedPtr<FLinkedBoxManager> Manager = Managers.FindRef(Group).Pin();
		if (!Manager.IsValid())
		{
			Manager = MakeShared<FLinkedBoxManager>();
			Managers.Add(Group, Manager);
			// Opportunistic prune (groups are few; keeps dead weaks from accumulating).
			for (auto It = Managers.CreateIterator(); It; ++It)
			{
				if (!It->Value.IsValid())
				{
					It.RemoveCurrent();
				}
			}
		}
		TSharedRef<SLinkedBox> W = SNew(SLinkedBox, Manager.ToSharedRef());
		LiveManagers.Add(&W.Get(), Manager); // keep the manager alive as long as any member widget
		return W;
	}

	virtual void ApplyDiff(SWidget&, const FRuitkPropsBase*, const FRuitkPropsBase&) override {}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		static_cast<SBox&>(Parent).SetContent(Child.IsValid() ? Child.ToSharedRef() : SNullWidget::NullWidget);
	}

private:
	TMap<FName, TWeakPtr<FLinkedBoxManager>> Managers;
	TMap<const SWidget*, TSharedPtr<FLinkedBoxManager>> LiveManagers; // strong refs per live widget
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SVirtualJoystick — touch overlay mount; configured imperatively via P2 (FControlInfo).
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkVirtualJoystickAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SVirtualJoystick);
	}

	virtual void ApplyDiff(SWidget&, const FRuitkPropsBase*, const FRuitkPropsBase&) override {}
};

// ─────────────────────────────────────────────────────────────────────────────────────────
// SVectorInputBox / SRotatorInputBox (wave 4) — templated float rows; fully masked.
// ─────────────────────────────────────────────────────────────────────────────────────────

class FRuitkVectorInputBoxAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	virtual uint64 GetReconstructMask() const override
	{
		return (1ull << FRuitkVectorInputBoxProps::X_Bit) | (1ull << FRuitkVectorInputBoxProps::Y_Bit) |
			   (1ull << FRuitkVectorInputBoxProps::Z_Bit) | (1ull << FRuitkVectorInputBoxProps::bColorAxisLabels_Bit);
	}

	virtual bool ConstructOnlyChanged(const FRuitkPropsBase& Old, const FRuitkPropsBase& New) const override
	{
		const FRuitkVectorInputBoxProps& O = static_cast<const FRuitkVectorInputBoxProps&>(Old);
		const FRuitkVectorInputBoxProps& N = static_cast<const FRuitkVectorInputBoxProps&>(New);
		auto Changed = [](bool bNewHas, bool bOldHas, const auto& OldV, const auto& NewV)
		{ return bNewHas && (!bOldHas || !(OldV == NewV)); };
		return Changed(N.HasX(), O.HasX(), O.X, N.X) || Changed(N.HasY(), O.HasY(), O.Y, N.Y) ||
			   Changed(N.HasZ(), O.HasZ(), O.Z, N.Z) ||
			   Changed(N.HasbColorAxisLabels(), O.HasbColorAxisLabels(), O.bColorAxisLabels, N.bColorAxisLabels);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props,
											 const TSharedPtr<FRuitkEventProxy>& Proxy) override
	{
		const FRuitkVectorInputBoxProps& P = static_cast<const FRuitkVectorInputBoxProps&>(Props);
		return SNew(SVectorInputBox)
			.X(P.HasX() ? TOptional<float>(P.X) : TOptional<float>())
			.Y(P.HasY() ? TOptional<float>(P.Y) : TOptional<float>())
			.Z(P.HasZ() ? TOptional<float>(P.Z) : TOptional<float>())
			.bColorAxisLabels(P.HasbColorAxisLabels() && P.bColorAxisLabels)
			.OnXChanged(SVectorInputBox::FOnNumericValueChanged::CreateSP(
				Proxy.ToSharedRef(), &FRuitkEventProxy::HandleFloat,
				static_cast<int32>(FRuitkVectorInputBoxProps::OnXChanged_Bit)))
			.OnYChanged(SVectorInputBox::FOnNumericValueChanged::CreateSP(
				Proxy.ToSharedRef(), &FRuitkEventProxy::HandleFloat,
				static_cast<int32>(FRuitkVectorInputBoxProps::OnYChanged_Bit)))
			.OnZChanged(SVectorInputBox::FOnNumericValueChanged::CreateSP(
				Proxy.ToSharedRef(), &FRuitkEventProxy::HandleFloat,
				static_cast<int32>(FRuitkVectorInputBoxProps::OnZChanged_Bit)));
	}

	virtual void SyncEventHandlers(FRuitkEventProxy& Proxy, const FRuitkPropsBase& New) override
	{
		const FRuitkVectorInputBoxProps& N = static_cast<const FRuitkVectorInputBoxProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuitkVectorInputBoxProps::OnXChanged_Bit), N.OnXChanged);
		Proxy.SetHandler(static_cast<int32>(FRuitkVectorInputBoxProps::OnYChanged_Bit), N.OnYChanged);
		Proxy.SetHandler(static_cast<int32>(FRuitkVectorInputBoxProps::OnZChanged_Bit), N.OnZChanged);
	}

	virtual void ApplyDiff(SWidget&, const FRuitkPropsBase*, const FRuitkPropsBase&) override {} // all masked
};

class FRuitkRotatorInputBoxAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }
	virtual bool HasEvents() const override { return true; }

	virtual uint64 GetReconstructMask() const override
	{
		return (1ull << FRuitkRotatorInputBoxProps::Roll_Bit) | (1ull << FRuitkRotatorInputBoxProps::Pitch_Bit) |
			   (1ull << FRuitkRotatorInputBoxProps::Yaw_Bit) |
			   (1ull << FRuitkRotatorInputBoxProps::bColorAxisLabels_Bit);
	}

	virtual bool ConstructOnlyChanged(const FRuitkPropsBase& Old, const FRuitkPropsBase& New) const override
	{
		const FRuitkRotatorInputBoxProps& O = static_cast<const FRuitkRotatorInputBoxProps&>(Old);
		const FRuitkRotatorInputBoxProps& N = static_cast<const FRuitkRotatorInputBoxProps&>(New);
		auto Changed = [](bool bNewHas, bool bOldHas, const auto& OldV, const auto& NewV)
		{ return bNewHas && (!bOldHas || !(OldV == NewV)); };
		return Changed(N.HasRoll(), O.HasRoll(), O.Roll, N.Roll) ||
			   Changed(N.HasPitch(), O.HasPitch(), O.Pitch, N.Pitch) || Changed(N.HasYaw(), O.HasYaw(), O.Yaw, N.Yaw) ||
			   Changed(N.HasbColorAxisLabels(), O.HasbColorAxisLabels(), O.bColorAxisLabels, N.bColorAxisLabels);
	}

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props,
											 const TSharedPtr<FRuitkEventProxy>& Proxy) override
	{
		const FRuitkRotatorInputBoxProps& P = static_cast<const FRuitkRotatorInputBoxProps&>(Props);
		return SNew(SRotatorInputBox)
			.Roll(P.HasRoll() ? TOptional<float>(P.Roll) : TOptional<float>())
			.Pitch(P.HasPitch() ? TOptional<float>(P.Pitch) : TOptional<float>())
			.Yaw(P.HasYaw() ? TOptional<float>(P.Yaw) : TOptional<float>())
			.bColorAxisLabels(P.HasbColorAxisLabels() && P.bColorAxisLabels)
			.OnRollChanged(SRotatorInputBox::FOnNumericValueChanged::CreateSP(
				Proxy.ToSharedRef(), &FRuitkEventProxy::HandleFloat,
				static_cast<int32>(FRuitkRotatorInputBoxProps::OnRollChanged_Bit)))
			.OnPitchChanged(SRotatorInputBox::FOnNumericValueChanged::CreateSP(
				Proxy.ToSharedRef(), &FRuitkEventProxy::HandleFloat,
				static_cast<int32>(FRuitkRotatorInputBoxProps::OnPitchChanged_Bit)))
			.OnYawChanged(SRotatorInputBox::FOnNumericValueChanged::CreateSP(
				Proxy.ToSharedRef(), &FRuitkEventProxy::HandleFloat,
				static_cast<int32>(FRuitkRotatorInputBoxProps::OnYawChanged_Bit)));
	}

	virtual void SyncEventHandlers(FRuitkEventProxy& Proxy, const FRuitkPropsBase& New) override
	{
		const FRuitkRotatorInputBoxProps& N = static_cast<const FRuitkRotatorInputBoxProps&>(New);
		Proxy.SetHandler(static_cast<int32>(FRuitkRotatorInputBoxProps::OnRollChanged_Bit), N.OnRollChanged);
		Proxy.SetHandler(static_cast<int32>(FRuitkRotatorInputBoxProps::OnPitchChanged_Bit), N.OnPitchChanged);
		Proxy.SetHandler(static_cast<int32>(FRuitkRotatorInputBoxProps::OnYawChanged_Bit), N.OnYawChanged);
	}

	virtual void ApplyDiff(SWidget&, const FRuitkPropsBase*, const FRuitkPropsBase&) override {} // all masked
};

namespace Ruitk::Slate
{
	namespace
	{
		FRuitkElementTypeId ConstraintCanvasType()
		{
			return Ruitk::InternElementType(FName(TEXT("ConstraintCanvas")));
		}
		FRuitkElementTypeId SplitterType()
		{
			return Ruitk::InternElementType(FName(TEXT("Splitter")));
		}
		FRuitkElementTypeId Splitter2x2Type()
		{
			return Ruitk::InternElementType(FName(TEXT("Splitter2x2")));
		}
		FRuitkElementTypeId MenuAnchorType()
		{
			return Ruitk::InternElementType(FName(TEXT("MenuAnchor")));
		}
		FRuitkElementTypeId WindowTitleBarAreaType()
		{
			return Ruitk::InternElementType(FName(TEXT("WindowTitleBarArea")));
		}
		FRuitkElementTypeId NumericDropDownType()
		{
			return Ruitk::InternElementType(FName(TEXT("NumericDropDown")));
		}
		FRuitkElementTypeId BreadcrumbTrailType()
		{
			return Ruitk::InternElementType(FName(TEXT("BreadcrumbTrail")));
		}
		FRuitkElementTypeId NotificationListType()
		{
			return Ruitk::InternElementType(FName(TEXT("NotificationList")));
		}
		FRuitkElementTypeId SearchableComboBoxType()
		{
			return Ruitk::InternElementType(FName(TEXT("SearchableComboBox")));
		}
		FRuitkElementTypeId LinkedBoxType()
		{
			return Ruitk::InternElementType(FName(TEXT("LinkedBox")));
		}
		FRuitkElementTypeId VirtualJoystickType()
		{
			return Ruitk::InternElementType(FName(TEXT("VirtualJoystick")));
		}
		FRuitkElementTypeId VectorInputBoxType()
		{
			return Ruitk::InternElementType(FName(TEXT("VectorInputBox")));
		}
		FRuitkElementTypeId RotatorInputBoxType()
		{
			return Ruitk::InternElementType(FName(TEXT("RotatorInputBox")));
		}
	} // namespace

	FRuitkNode ConstraintCanvas(FRuitkConstraintCanvasProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		FRuitkNode Node;
		Node.Kind = ERuitkNodeKind::Host;
		Node.ElementType = ConstraintCanvasType();
		Node.Props = MakeShared<FRuitkConstraintCanvasProps>(MoveTemp(Props));
		Node.Children = Ruitk::MakeChildren(MoveTemp(Children));
		Node.Key = Key;
		return Node;
	}

	FRuitkNode Splitter(FRuitkSplitterProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		FRuitkNode Node;
		Node.Kind = ERuitkNodeKind::Host;
		Node.ElementType = SplitterType();
		Node.Props = MakeShared<FRuitkSplitterProps>(MoveTemp(Props));
		Node.Children = Ruitk::MakeChildren(MoveTemp(Children));
		Node.Key = Key;
		return Node;
	}

	FRuitkNode Splitter2x2(FRuitkSplitter2x2Props Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		FRuitkNode Node;
		Node.Kind = ERuitkNodeKind::Host;
		Node.ElementType = Splitter2x2Type();
		Node.Props = MakeShared<FRuitkSplitter2x2Props>(MoveTemp(Props));
		Node.Children = Ruitk::MakeChildren(MoveTemp(Children));
		Node.Key = Key;
		return Node;
	}

	FRuitkNode MenuAnchor(FRuitkMenuAnchorProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		FRuitkNode Node;
		Node.Kind = ERuitkNodeKind::Host;
		Node.ElementType = MenuAnchorType();
		Node.Props = MakeShared<FRuitkMenuAnchorProps>(MoveTemp(Props));
		Node.Children = Ruitk::MakeChildren(MoveTemp(Children));
		Node.Key = Key;
		return Node;
	}

	FRuitkNode WindowTitleBarArea(FRuitkWindowTitleBarAreaProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		FRuitkNode Node;
		Node.Kind = ERuitkNodeKind::Host;
		Node.ElementType = WindowTitleBarAreaType();
		Node.Props = MakeShared<FRuitkWindowTitleBarAreaProps>(MoveTemp(Props));
		Node.Children = Ruitk::MakeChildren(MoveTemp(Children));
		Node.Key = Key;
		return Node;
	}

	FRuitkNode NumericDropDown(FRuitkNumericDropDownProps Props, FRuitkKey Key)
	{
		FRuitkNode Node;
		Node.Kind = ERuitkNodeKind::Host;
		Node.ElementType = NumericDropDownType();
		Node.Props = MakeShared<FRuitkNumericDropDownProps>(MoveTemp(Props));
		Node.Children = Ruitk::MakeChildren(TArray<FRuitkNode>());
		Node.Key = Key;
		return Node;
	}

	FRuitkNode BreadcrumbTrail(FRuitkBreadcrumbTrailProps Props, FRuitkKey Key)
	{
		FRuitkNode Node;
		Node.Kind = ERuitkNodeKind::Host;
		Node.ElementType = BreadcrumbTrailType();
		Node.Props = MakeShared<FRuitkBreadcrumbTrailProps>(MoveTemp(Props));
		Node.Children = Ruitk::MakeChildren(TArray<FRuitkNode>());
		Node.Key = Key;
		return Node;
	}

	FRuitkNode NotificationList(FRuitkNotificationListProps Props, FRuitkKey Key)
	{
		FRuitkNode Node;
		Node.Kind = ERuitkNodeKind::Host;
		Node.ElementType = NotificationListType();
		Node.Props = MakeShared<FRuitkNotificationListProps>(MoveTemp(Props));
		Node.Children = Ruitk::MakeChildren(TArray<FRuitkNode>());
		Node.Key = Key;
		return Node;
	}

	FRuitkNode SearchableComboBox(FRuitkSearchableComboBoxProps Props, FRuitkKey Key)
	{
		FRuitkNode Node;
		Node.Kind = ERuitkNodeKind::Host;
		Node.ElementType = SearchableComboBoxType();
		Node.Props = MakeShared<FRuitkSearchableComboBoxProps>(MoveTemp(Props));
		Node.Children = Ruitk::MakeChildren(TArray<FRuitkNode>());
		Node.Key = Key;
		return Node;
	}

	FRuitkNode LinkedBox(FRuitkLinkedBoxProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		FRuitkNode Node;
		Node.Kind = ERuitkNodeKind::Host;
		Node.ElementType = LinkedBoxType();
		Node.Props = MakeShared<FRuitkLinkedBoxProps>(MoveTemp(Props));
		Node.Children = Ruitk::MakeChildren(MoveTemp(Children));
		Node.Key = Key;
		return Node;
	}

	FRuitkNode VirtualJoystick(FRuitkVirtualJoystickProps Props, FRuitkKey Key)
	{
		FRuitkNode Node;
		Node.Kind = ERuitkNodeKind::Host;
		Node.ElementType = VirtualJoystickType();
		Node.Props = MakeShared<FRuitkVirtualJoystickProps>(MoveTemp(Props));
		Node.Children = Ruitk::MakeChildren(TArray<FRuitkNode>());
		Node.Key = Key;
		return Node;
	}

	FRuitkNode VectorInputBox(FRuitkVectorInputBoxProps Props, FRuitkKey Key)
	{
		FRuitkNode Node;
		Node.Kind = ERuitkNodeKind::Host;
		Node.ElementType = VectorInputBoxType();
		Node.Props = MakeShared<FRuitkVectorInputBoxProps>(MoveTemp(Props));
		Node.Children = Ruitk::MakeChildren(TArray<FRuitkNode>());
		Node.Key = Key;
		return Node;
	}

	FRuitkNode RotatorInputBox(FRuitkRotatorInputBoxProps Props, FRuitkKey Key)
	{
		FRuitkNode Node;
		Node.Kind = ERuitkNodeKind::Host;
		Node.ElementType = RotatorInputBoxType();
		Node.Props = MakeShared<FRuitkRotatorInputBoxProps>(MoveTemp(Props));
		Node.Children = Ruitk::MakeChildren(TArray<FRuitkNode>());
		Node.Key = Key;
		return Node;
	}

	namespace Detail
	{
		void RegisterBatch3Wave3Adapters()
		{
			RegisterAdapter(ConstraintCanvasType(), MakeUnique<FRuitkConstraintCanvasAdapter>());
			RegisterAdapter(SplitterType(), MakeUnique<FRuitkSplitterAdapter>());
			RegisterAdapter(Splitter2x2Type(), MakeUnique<FRuitkSplitter2x2Adapter>());
			RegisterAdapter(MenuAnchorType(), MakeUnique<FRuitkMenuAnchorAdapter>());
			RegisterAdapter(WindowTitleBarAreaType(), MakeUnique<FRuitkWindowTitleBarAreaAdapter>());
			RegisterAdapter(NumericDropDownType(), MakeUnique<FRuitkNumericDropDownAdapter>());
			RegisterAdapter(BreadcrumbTrailType(), MakeUnique<FRuitkBreadcrumbTrailAdapter>());
			RegisterAdapter(NotificationListType(), MakeUnique<FRuitkNotificationListAdapter>());
#if !UE_VERSION_OLDER_THAN(5, 7, 0)
			RegisterAdapter(SearchableComboBoxType(), MakeUnique<FRuitkSearchableComboBoxAdapter>());
#endif
			RegisterAdapter(LinkedBoxType(), MakeUnique<FRuitkLinkedBoxAdapter>());
			RegisterAdapter(VirtualJoystickType(), MakeUnique<FRuitkVirtualJoystickAdapter>());
			RegisterAdapter(VectorInputBoxType(), MakeUnique<FRuitkVectorInputBoxAdapter>());
			RegisterAdapter(RotatorInputBoxType(), MakeUnique<FRuitkRotatorInputBoxAdapter>());
		}
	} // namespace Detail
} // namespace Ruitk::Slate
