// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// The first wrapped elements (Phase 2 step 3 — the hand-written pattern setters; the
// remaining widgets production-line onto this shape in the same phase). Props structs +
// element factories only; the adapters live in Private/RuitkCoreAdapters.cpp.
//
// Text renders core FRuitkTextBlockProps (Ruitk::TextBlock) — the GetTextElementType contract — so it
// has no props struct here.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "RuitkCoreElements.h"
#include "RuitkNode.h"
#include "RuitkPropsBase.h"
#include "SRuitkCanvas.h"		// FRuitkDrawFn
#include "Styling/SlateBrush.h" // FSlateBrush (asset brushes, D-17)

/** SVerticalBox (MultiSlot). Layout comes from the children's slot.* props. */
struct RUITKSLATE_API FRuitkVerticalBoxProps final : public FRuitkPropsBase
{
	RUITK_PROPS_BODY(FRuitkVerticalBoxProps, )
};

/** SHorizontalBox (MultiSlot). */
struct RUITKSLATE_API FRuitkHorizontalBoxProps final : public FRuitkPropsBase
{
	RUITK_PROPS_BODY(FRuitkHorizontalBoxProps, )
};

/** SButton (SingleContent) — the event-proxy pattern widget. OnClicked participates in
 *  Equals by IDENTITY (FRuitkCallback ==): a fresh closure means new props, exactly React —
 *  otherwise a bailout would keep firing a stale capture. UseCallback restores memo. */
struct RUITKSLATE_API FRuitkButtonProps final : public FRuitkPropsBase
{
	RUITK_PROP_EVENT(OnClicked, 0)
	RUITK_PROP(bool, bEnabled, 1)
	RUITK_PROP(FMargin, ContentPadding, 2)
	RUITK_PROP(bool, bIsFocusable, 3)
	RUITK_PROPS_BODY(FRuitkButtonProps,
					 RUITK_EQ(OnClicked) RUITK_EQ(bEnabled) RUITK_EQ(ContentPadding) RUITK_EQ(bIsFocusable))
};

/** SOverlay (MultiSlot; also the SRuitkRoot inner panel). slot.zorder orders the slots. */
struct RUITKSLATE_API FRuitkOverlayProps final : public FRuitkPropsBase
{
	RUITK_PROPS_BODY(FRuitkOverlayProps, )
};

/** SCanvas (MultiSlot) — ABSOLUTE placement: each child positions/sizes itself via
 *  `Slot.Position` + `Slot.Size` (FVector2D or "x,y" literals). Paint order = child order
 *  (SCanvas has no per-slot z; keep emission order stable — the Doom-demo container). */
struct RUITKSLATE_API FRuitkCanvasPanelProps final : public FRuitkPropsBase
{
	RUITK_PROPS_BODY(FRuitkCanvasPanelProps, )
};

/** SConstraintCanvas (MultiSlot) - anchor-based absolute panel (P5a): children place via
 *  `Slot.Anchors` ("min" | "x,y" | "minX,minY,maxX,maxY"), `Slot.Offset` (FMargin forms),
 *  `Slot.Alignment` (Vector2), `Slot.AutoSize` (bool), `Slot.ZOrder` (float) - all live. */
struct RUITKSLATE_API FRuitkConstraintCanvasProps final : public FRuitkPropsBase
{
	RUITK_PROPS_BODY(FRuitkConstraintCanvasProps, )
};

/** SSplitter (MultiSlot) - resizable panes (P5b): children carry `Slot.SizeRule`
 *  ("fractionOfParent" default | "sizeToContent"), `Slot.SizeValue` (fraction), `Slot.MinSize`,
 *  `Slot.Resizable` - all live. The user's drag reports back via OnSplitterFinishedResizing
 *  (payload: none - read fractions through a Ref if needed). PhysicalSplitterHandleSize is
 *  construct-only (masked). */
struct RUITKSLATE_API FRuitkSplitterProps final : public FRuitkPropsBase
{
	RUITK_PROP(FName, Orientation, 0)
	RUITK_PROP(float, PhysicalSplitterHandleSize, 1)
	RUITK_PROP_EVENT(OnSplitterFinishedResizing, 2)
	RUITK_PROPS_BODY(FRuitkSplitterProps,
					 RUITK_EQ(Orientation) RUITK_EQ(PhysicalSplitterHandleSize) RUITK_EQ(OnSplitterFinishedResizing))
};

/** SSplitter2x2 (MultiSlot, D-W4) - four resizable quadrants; children route by
 *  `Slot.Role` = "topLeft" (default) | "bottomLeft" | "topRight" | "bottomRight" (live
 *  Set*Content setters). Percentages = 4 fractions in that same order (live,
 *  SetSplitterPercentages; each quadrant's share of its column/row). */
struct RUITKSLATE_API FRuitkSplitter2x2Props final : public FRuitkPropsBase
{
	RUITK_PROP(TArray<FVector2D>, Percentages, 0)
	RUITK_PROPS_BODY(FRuitkSplitter2x2Props, RUITK_EQ(Percentages))
};

/** SMenuAnchor (MultiSlot, P3 - THE popup primitive): the default child is the anchor;
 *  the child with `Slot.Role="menu"` is the popup content. bIsOpen is CONTROLLED
 *  (skip-when-equal vs IsOpen, D-16); OnMenuOpenChanged reports user dismissals (Value =
 *  bool). Placement = menuPlacementBelowAnchor (default) | comboBox | belowRightAnchor |
 *  aboveAnchor | menuRight | center ... (loyal EMenuPlacement names, lowerCamel). */
struct RUITKSLATE_API FRuitkMenuAnchorProps final : public FRuitkPropsBase
{
	RUITK_PROP(bool, bIsOpen, 0)
	RUITK_PROP(FName, Placement, 1)
	RUITK_PROP(bool, bFitInWindow, 2)
	RUITK_PROP_EVENT(OnMenuOpenChanged, 3)
	RUITK_PROPS_BODY(FRuitkMenuAnchorProps,
					 RUITK_EQ(bIsOpen) RUITK_EQ(Placement) RUITK_EQ(bFitInWindow) RUITK_EQ(OnMenuOpenChanged))
};

/** SWindowTitleBarArea (SingleContent): a custom title-bar strip — drag zone + OS window
 *  buttons on the GAME window (wired automatically from the game viewport when present).
 *  RequestToggleFullscreen fires on title-bar double-click. */
struct RUITKSLATE_API FRuitkWindowTitleBarAreaProps final : public FRuitkPropsBase
{
	RUITK_PROP(FName, HAlign, 0)
	RUITK_PROP(FName, VAlign, 1)
	RUITK_PROP(FMargin, Padding, 2)
	RUITK_PROP_EVENT(RequestToggleFullscreen, 3)
	RUITK_PROPS_BODY(FRuitkWindowTitleBarAreaProps,
					 RUITK_EQ(HAlign) RUITK_EQ(VAlign) RUITK_EQ(Padding) RUITK_EQ(RequestToggleFullscreen))
};

/** SNumericDropDown<float> (Leaf): numeric preset dropdown. Values+Labels zip into the
 *  engine's FNamedValue list; everything is construct-only (masked) except the controlled
 *  Value (also masked - attribute-only) - user picks report via OnValueChanged. */
struct RUITKSLATE_API FRuitkNumericDropDownProps final : public FRuitkPropsBase
{
	RUITK_PROP(TArray<float>, Values, 0)
	RUITK_PROP(TArray<FString>, Labels, 1)
	RUITK_PROP(float, Value, 2)
	RUITK_PROP(bool, bShowNamedValue, 3)
	RUITK_PROP_EVENT(OnValueChanged, 4)
	RUITK_PROPS_BODY(FRuitkNumericDropDownProps, RUITK_EQ(Values) RUITK_EQ(Labels) RUITK_EQ(Value)
													 RUITK_EQ(bShowNamedValue) RUITK_EQ(OnValueChanged))
};

/** SBreadcrumbTrail<FString> (Leaf): declarative crumbs over the engine's imperative stack -
 *  a Crumbs list change converges via ClearCrumbs+PushCrumb (small lists). OnCrumbClicked
 *  payload = the crumb string. Direct Push/Pop also reachable via P2. */
struct RUITKSLATE_API FRuitkBreadcrumbTrailProps final : public FRuitkPropsBase
{
	RUITK_PROP(TArray<FString>, Crumbs, 0)
	RUITK_PROP(bool, bShowLeadingDelimiter, 1)
	RUITK_PROP_EVENT(OnCrumbClicked, 2)
	RUITK_PROPS_BODY(FRuitkBreadcrumbTrailProps,
					 RUITK_EQ(Crumbs) RUITK_EQ(bShowLeadingDelimiter) RUITK_EQ(OnCrumbClicked))
};

/** SNotificationList (Leaf, P4): a toast mount point. The engine API is imperative-only, so
 *  pushes go through the P2 command path - capture the list with `Ref` and call
 *  `Ruitk::Slate::PushNotification(Handle, Text, Duration)` (or WidgetFromHandle for the full
 *  FNotificationInfo surface). */
struct RUITKSLATE_API FRuitkNotificationListProps final : public FRuitkPropsBase
{
	RUITK_PROPS_BODY(FRuitkNotificationListProps, )
};

/** SSearchableComboBox (Leaf) - sinceUE 5.7 (the widget does not exist in 5.6; mounting on
 *  5.6 warns unknown-adapter). Options are strings; SelectedItem is controlled; picks report
 *  via OnSelectionChanged (text payload). */
struct RUITKSLATE_API FRuitkSearchableComboBoxProps final : public FRuitkPropsBase
{
	RUITK_PROP(TArray<FString>, Options, 0)
	RUITK_PROP(FText, SelectedItem, 1)
	RUITK_PROP_EVENT(OnSelectionChanged, 2)

	virtual bool Equals(const FRuitkPropsBase& OtherBase) const override
	{
		const FRuitkSearchableComboBoxProps& Other = static_cast<const FRuitkSearchableComboBoxProps&>(OtherBase);
		auto TextEq = [](const FText& A, const FText& B) { return A.IdenticalTo(B) || A.ToString() == B.ToString(); };
		return BaseFieldsEqual(Other) && Options == Other.Options && TextEq(SelectedItem, Other.SelectedItem) &&
			   OnSelectionChanged == Other.OnSelectionChanged;
	}
};

/** SLinkedBox (SingleContent): siblings sharing a GroupKey size uniformly (one shared
 *  FLinkedBoxManager per group, adapter-owned). GroupKey is construct-only (masked). */
struct RUITKSLATE_API FRuitkLinkedBoxProps final : public FRuitkPropsBase
{
	RUITK_PROP(FName, GroupKey, 0)
	RUITK_PROPS_BODY(FRuitkLinkedBoxProps, RUITK_EQ(GroupKey))
};

/** SVirtualJoystick (Leaf): the touch joystick overlay. The engine config API is imperative
 *  (FControlInfo structs with brushes) - capture with `Ref` and drive it via
 *  `WidgetFromHandle<SVirtualJoystick>` (P2). Desktop no-op without touch. */
struct RUITKSLATE_API FRuitkVirtualJoystickProps final : public FRuitkPropsBase
{
	RUITK_PROPS_BODY(FRuitkVirtualJoystickProps, )
};

/** SVectorInputBox (= SNumericVectorInputBox<float,3>, Leaf): X/Y/Z numeric row. The whole
 *  surface is attribute/construct-only -> masked; per-axis edits report via OnXChanged etc.
 *  (float payloads). */
struct RUITKSLATE_API FRuitkVectorInputBoxProps final : public FRuitkPropsBase
{
	RUITK_PROP(float, X, 0)
	RUITK_PROP(float, Y, 1)
	RUITK_PROP(float, Z, 2)
	RUITK_PROP(bool, bColorAxisLabels, 3)
	RUITK_PROP_EVENT(OnXChanged, 4)
	RUITK_PROP_EVENT(OnYChanged, 5)
	RUITK_PROP_EVENT(OnZChanged, 6)
	RUITK_PROPS_BODY(FRuitkVectorInputBoxProps, RUITK_EQ(X) RUITK_EQ(Y) RUITK_EQ(Z) RUITK_EQ(bColorAxisLabels)
													RUITK_EQ(OnXChanged) RUITK_EQ(OnYChanged) RUITK_EQ(OnZChanged))
};

/** SRotatorInputBox (= SNumericRotatorInputBox<float>, Leaf): Roll/Pitch/Yaw row - same
 *  masked-controlled contract as VectorInputBox. */
struct RUITKSLATE_API FRuitkRotatorInputBoxProps final : public FRuitkPropsBase
{
	RUITK_PROP(float, Roll, 0)
	RUITK_PROP(float, Pitch, 1)
	RUITK_PROP(float, Yaw, 2)
	RUITK_PROP(bool, bColorAxisLabels, 3)
	RUITK_PROP_EVENT(OnRollChanged, 4)
	RUITK_PROP_EVENT(OnPitchChanged, 5)
	RUITK_PROP_EVENT(OnYawChanged, 6)
	RUITK_PROPS_BODY(FRuitkRotatorInputBoxProps,
					 RUITK_EQ(Roll) RUITK_EQ(Pitch) RUITK_EQ(Yaw) RUITK_EQ(bColorAxisLabels) RUITK_EQ(OnRollChanged)
						 RUITK_EQ(OnPitchChanged) RUITK_EQ(OnYawChanged))
};

/** SBorder (SingleContent). Alignment values: fill|left|center|right / fill|top|center|bottom.
 *  BorderImage takes an FCoreStyle brush NAME (v1 — e.g. "WhiteBrush" for a solid fill
 *  tinted by BorderBackgroundColor; the engine default is a thin frame-type brush). Asset
 *  brushes (textures/materials) are the D-17 work. */
struct RUITKSLATE_API FRuitkBorderProps final : public FRuitkPropsBase
{
	RUITK_PROP(FMargin, Padding, 0)
	RUITK_PROP(FLinearColor, BorderBackgroundColor, 1)
	RUITK_PROP(FName, HAlign, 2)
	RUITK_PROP(FName, VAlign, 3)
	RUITK_PROP(FName, BorderImage, 4)
	RUITK_PROP(TSharedPtr<FSlateBrush>, BorderImageBrush, 5) // asset brush (D-17); wins over BorderImage name
	RUITK_PROPS_BODY(FRuitkBorderProps, RUITK_EQ(Padding) RUITK_EQ(BorderBackgroundColor) RUITK_EQ(HAlign)
											RUITK_EQ(VAlign) RUITK_EQ(BorderImage) RUITK_EQ(BorderImageBrush))
};

/** SBox (SingleContent): size overrides + content alignment. Overrides are settable, not
 *  clearable (family removal semantics: plain props don't reset). */
struct RUITKSLATE_API FRuitkBoxProps final : public FRuitkPropsBase
{
	RUITK_PROP(float, WidthOverride, 0)
	RUITK_PROP(float, HeightOverride, 1)
	RUITK_PROP(float, MinDesiredWidth, 2)
	RUITK_PROP(float, MinDesiredHeight, 3)
	RUITK_PROP(float, MaxDesiredWidth, 4)
	RUITK_PROP(float, MaxDesiredHeight, 5)
	RUITK_PROP(FName, HAlign, 6)
	RUITK_PROP(FName, VAlign, 7)
	RUITK_PROPS_BODY(FRuitkBoxProps, RUITK_EQ(WidthOverride) RUITK_EQ(HeightOverride) RUITK_EQ(MinDesiredWidth)
										 RUITK_EQ(MinDesiredHeight) RUITK_EQ(MaxDesiredWidth) RUITK_EQ(MaxDesiredHeight)
											 RUITK_EQ(HAlign) RUITK_EQ(VAlign))
};

/** SImage (Leaf): tint + desired size + an optional asset brush (D-17). The field is `Image` —
 *  the loyal Unreal name (SImage::SetImage), also the markup attr: `<Image Image={ Brush } />`.
 *  Build the brush ONCE with Ruitk::Umg::MakeAssetBrush (it GC-roots the texture/material) and
 *  pass it by identity — RUITK_EQ(Image) compares the shared pointer, so wrap it in
 *  UseMemo/UseRef to avoid re-applying. (Renamed from `Brush` 2026-07-15, D-33 compliance.) */
struct RUITKSLATE_API FRuitkImageProps final : public FRuitkPropsBase
{
	RUITK_PROP(FLinearColor, ColorAndOpacity, 0)
	RUITK_PROP(FVector2D, DesiredSizeOverride, 1)
	RUITK_PROP(TSharedPtr<FSlateBrush>, Image, 2)
	RUITK_PROPS_BODY(FRuitkImageProps, RUITK_EQ(ColorAndOpacity) RUITK_EQ(DesiredSizeOverride) RUITK_EQ(Image))
};

/** SScrollBox (MultiSlot). Orientation is runtime-settable (header-sweep verified). */
struct RUITKSLATE_API FRuitkScrollBoxProps final : public FRuitkPropsBase
{
	RUITK_PROP(FName, Orientation, 0)			// "vertical" (default) | "horizontal"
	RUITK_PROP(bool, bAllowOverscroll, 1)		// TD-012 sweep: live SetAllowOverscroll
	RUITK_PROP(bool, bAnimateWheelScrolling, 2) // live SetAnimateWheelScrolling
	RUITK_PROP(float, WheelScrollMultiplier, 3) // live SetWheelScrollMultiplier
	RUITK_PROPS_BODY(FRuitkScrollBoxProps, RUITK_EQ(Orientation) RUITK_EQ(bAllowOverscroll)
											   RUITK_EQ(bAnimateWheelScrolling) RUITK_EQ(WheelScrollMultiplier))
};

/** SSpacer (Leaf). */
struct RUITKSLATE_API FRuitkSpacerProps final : public FRuitkPropsBase
{
	RUITK_PROP(FVector2D, Size, 0)
	RUITK_PROPS_BODY(FRuitkSpacerProps, RUITK_EQ(Size))
};

/** SEditableTextBox (Leaf) — THE controlled input (D-16): Text is applied skip-when-equal
 *  against the WIDGET's current text so the caret survives the typing round-trip. */
struct RUITKSLATE_API FRuitkEditableTextBoxProps final : public FRuitkPropsBase
{
	RUITK_PROP(FText, Text, 0)
	RUITK_PROP(FText, HintText, 1)
	RUITK_PROP(bool, bIsReadOnly, 2)
	RUITK_PROP_EVENT(OnTextChanged, 3)
	RUITK_PROP_EVENT(OnTextCommitted, 4)

	virtual bool Equals(const FRuitkPropsBase& OtherBase) const override
	{
		const FRuitkEditableTextBoxProps& Other = static_cast<const FRuitkEditableTextBoxProps&>(OtherBase);
		auto TextEq = [](const FText& A, const FText& B) { return A.IdenticalTo(B) || A.ToString() == B.ToString(); };
		return SetBits == Other.SetBits && BaseFieldsEqual(Other) && TextEq(Text, Other.Text) &&
			   TextEq(HintText, Other.HintText) && bIsReadOnly == Other.bIsReadOnly &&
			   OnTextChanged == Other.OnTextChanged && OnTextCommitted == Other.OnTextCommitted;
	}
};

/** SCheckBox (SingleContent — the label is the child). */
struct RUITKSLATE_API FRuitkCheckBoxProps final : public FRuitkPropsBase
{
	RUITK_PROP(bool, bIsChecked, 0)
	RUITK_PROP_EVENT(OnCheckStateChanged, 1)
	RUITK_PROPS_BODY(FRuitkCheckBoxProps, RUITK_EQ(bIsChecked) RUITK_EQ(OnCheckStateChanged))
};

/** SSlider (Leaf). Value applies skip-when-equal (self-notifying family, D-16). */
struct RUITKSLATE_API FRuitkSliderProps final : public FRuitkPropsBase
{
	RUITK_PROP(float, Value, 0)
	RUITK_PROP(float, MinValue, 1)
	RUITK_PROP(float, MaxValue, 2)
	RUITK_PROP_EVENT(OnValueChanged, 3)
	RUITK_PROP(float, StepSize, 4)
	RUITK_PROP(FName, Orientation, 5)			   // TD-012 sweep: live SetOrientation
	RUITK_PROP(bool, bLocked, 6)				   // live SetLocked
	RUITK_PROP(bool, bIndentHandle, 7)			   // live SetIndentHandle
	RUITK_PROP(FLinearColor, SliderBarColor, 8)	   // live SetSliderBarColor
	RUITK_PROP(FLinearColor, SliderHandleColor, 9) // live SetSliderHandleColor
	RUITK_PROPS_BODY(FRuitkSliderProps,
					 RUITK_EQ(Value) RUITK_EQ(MinValue) RUITK_EQ(MaxValue) RUITK_EQ(OnValueChanged) RUITK_EQ(StepSize)
						 RUITK_EQ(Orientation) RUITK_EQ(bLocked) RUITK_EQ(bIndentHandle) RUITK_EQ(SliderBarColor)
							 RUITK_EQ(SliderHandleColor))
};

/** SProgressBar (Leaf). */
struct RUITKSLATE_API FRuitkProgressBarProps final : public FRuitkPropsBase
{
	RUITK_PROP(float, Percent, 0)
	RUITK_PROP(FName, BarFillType, 1)
	RUITK_PROPS_BODY(FRuitkProgressBarProps, RUITK_EQ(Percent) RUITK_EQ(BarFillType))
};

/** SRuitkCanvas (Leaf) — draw_fn by IDENTITY (wrap in a shared fn once; see MakeDrawFn). */
struct RUITKSLATE_API FRuitkCanvasProps final : public FRuitkPropsBase
{
	RUITK_PROP(TSharedPtr<FRuitkDrawFn>, DrawFn, 0)
	RUITK_PROP(int64, RedrawKey, 1)
	RUITK_PROP(FVector2D, CanvasSize, 2)
	RUITK_PROPS_BODY(FRuitkCanvasProps, RUITK_EQ(DrawFn) RUITK_EQ(RedrawKey) RUITK_EQ(CanvasSize))
};

// ── Batch 2 (Phase 7 step 8) — the everyday game set (WIDGET_INVENTORY.md) ─────────────────

/** SWidgetSwitcher (MultiSlot): shows exactly one child by index. WidgetIndex is a runtime
 *  setter (SetActiveWidgetIndex) — the classic tab/page panel. */
struct RUITKSLATE_API FRuitkWidgetSwitcherProps final : public FRuitkPropsBase
{
	RUITK_PROP(int32, WidgetIndex, 0)
	RUITK_PROPS_BODY(FRuitkWidgetSwitcherProps, RUITK_EQ(WidgetIndex))
};

/** SScaleBox (SingleContent): scales its content. Stretch = none|fill|scaleToFit|scaleToFitX|
 *  scaleToFitY|scaleToFill|scaleBySafeZone; StretchDirection = both|downOnly|upOnly.
 *  HAlign/VAlign place the scaled content inside the box (default center|center). */
struct RUITKSLATE_API FRuitkScaleBoxProps final : public FRuitkPropsBase
{
	RUITK_PROP(FName, Stretch, 0)
	RUITK_PROP(FName, StretchDirection, 1)
	RUITK_PROP(FName, HAlign, 2)
	RUITK_PROP(FName, VAlign, 3)
	RUITK_PROPS_BODY(FRuitkScaleBoxProps,
					 RUITK_EQ(Stretch) RUITK_EQ(StretchDirection) RUITK_EQ(HAlign) RUITK_EQ(VAlign))
};

/** SThrobber (Leaf): a busy indicator. Animate = all|vertical|horizontal|opacity|
 *  verticalAndOpacity|none. */
struct RUITKSLATE_API FRuitkThrobberProps final : public FRuitkPropsBase
{
	RUITK_PROP(int32, NumPieces, 0)
	RUITK_PROP(FName, Animate, 1)
	RUITK_PROPS_BODY(FRuitkThrobberProps, RUITK_EQ(NumPieces) RUITK_EQ(Animate))
};

/** SWrapBox (MultiSlot): flows children onto new lines. Orientation = horizontal (default) |
 *  vertical. WrapSize is the wrap threshold (ignored while bUseAllottedSize). */
struct RUITKSLATE_API FRuitkWrapBoxProps final : public FRuitkPropsBase
{
	RUITK_PROP(FName, Orientation, 0)
	RUITK_PROP(float, WrapSize, 1)
	RUITK_PROP(FVector2D, InnerSlotPadding, 2)
	RUITK_PROP(bool, bUseAllottedSize, 3)
	RUITK_PROPS_BODY(FRuitkWrapBoxProps,
					 RUITK_EQ(Orientation) RUITK_EQ(WrapSize) RUITK_EQ(InnerSlotPadding) RUITK_EQ(bUseAllottedSize))
};

/** SMultiLineEditableTextBox (Leaf) — multi-line controlled input; same D-16 caret rule as
 *  SEditableTextBox (Text applied skip-when-equal against the WIDGET's live text). */
struct RUITKSLATE_API FRuitkMultiLineEditableTextBoxProps final : public FRuitkPropsBase
{
	RUITK_PROP(FText, Text, 0)
	RUITK_PROP(FText, HintText, 1)
	RUITK_PROP(bool, bIsReadOnly, 2)
	RUITK_PROP_EVENT(OnTextChanged, 3)
	RUITK_PROP_EVENT(OnTextCommitted, 4)

	virtual bool Equals(const FRuitkPropsBase& OtherBase) const override
	{
		const FRuitkMultiLineEditableTextBoxProps& Other =
			static_cast<const FRuitkMultiLineEditableTextBoxProps&>(OtherBase);
		auto TextEq = [](const FText& A, const FText& B) { return A.IdenticalTo(B) || A.ToString() == B.ToString(); };
		return SetBits == Other.SetBits && BaseFieldsEqual(Other) && TextEq(Text, Other.Text) &&
			   TextEq(HintText, Other.HintText) && bIsReadOnly == Other.bIsReadOnly &&
			   OnTextChanged == Other.OnTextChanged && OnTextCommitted == Other.OnTextCommitted;
	}
};

/** SSearchBox (Leaf) — an SEditableTextBox specialization with a search affordance. The search
 *  text flows through OnTextChanged/OnTextCommitted (SSearchBox::OnSearch is up/down navigation,
 *  not a text callback). Text is the same controlled-input caret rule (D-16). */
struct RUITKSLATE_API FRuitkSearchBoxProps final : public FRuitkPropsBase
{
	RUITK_PROP(FText, Text, 0)
	RUITK_PROP(FText, HintText, 1)
	RUITK_PROP_EVENT(OnTextChanged, 2)
	RUITK_PROP_EVENT(OnTextCommitted, 3)

	virtual bool Equals(const FRuitkPropsBase& OtherBase) const override
	{
		const FRuitkSearchBoxProps& Other = static_cast<const FRuitkSearchBoxProps&>(OtherBase);
		auto TextEq = [](const FText& A, const FText& B) { return A.IdenticalTo(B) || A.ToString() == B.ToString(); };
		return SetBits == Other.SetBits && BaseFieldsEqual(Other) && TextEq(Text, Other.Text) &&
			   TextEq(HintText, Other.HintText) && OnTextChanged == Other.OnTextChanged &&
			   OnTextCommitted == Other.OnTextCommitted;
	}
};

/** SSafeZone (SingleContent): pads content into the device title/action safe area. */
struct RUITKSLATE_API FRuitkSafeZoneProps final : public FRuitkPropsBase
{
	RUITK_PROP(bool, bIsTitleSafe, 0)
	RUITK_PROP(bool, bPadLeft, 1)
	RUITK_PROP(bool, bPadRight, 2)
	RUITK_PROP(bool, bPadTop, 3)
	RUITK_PROP(bool, bPadBottom, 4)
	RUITK_PROPS_BODY(FRuitkSafeZoneProps, RUITK_EQ(bIsTitleSafe) RUITK_EQ(bPadLeft) RUITK_EQ(bPadRight)
											  RUITK_EQ(bPadTop) RUITK_EQ(bPadBottom))
};

/** SDPIScaler (SingleContent): scales its content by a DPI factor. */
struct RUITKSLATE_API FRuitkDPIScalerProps final : public FRuitkPropsBase
{
	RUITK_PROP(float, DPIScale, 0)
	RUITK_PROPS_BODY(FRuitkDPIScalerProps, RUITK_EQ(DPIScale))
};

/** SSeparator (Leaf): a styled line. Orientation (horizontal|vertical) + Thickness are
 *  CONSTRUCT-ONLY (Slate bakes them at build) — a change replaces the widget (TD-011 reconstruct
 *  mask, the first shipped widget to exercise it). ColorAndOpacity is a live setter. */
struct RUITKSLATE_API FRuitkSeparatorProps final : public FRuitkPropsBase
{
	RUITK_PROP(FName, Orientation, 0)			 // construct-only
	RUITK_PROP(float, Thickness, 1)				 // construct-only
	RUITK_PROP(FLinearColor, ColorAndOpacity, 2) // runtime
	RUITK_PROPS_BODY(FRuitkSeparatorProps, RUITK_EQ(Orientation) RUITK_EQ(Thickness) RUITK_EQ(ColorAndOpacity))
};

/** SSpinBox<float> (Leaf): numeric drag/type input. Value applies skip-when-equal (D-16
 *  self-notifying). Delta is the drag step (0 = continuous). */
struct RUITKSLATE_API FRuitkSpinBoxProps final : public FRuitkPropsBase
{
	RUITK_PROP(float, Value, 0)
	RUITK_PROP(float, MinValue, 1)
	RUITK_PROP(float, MaxValue, 2)
	RUITK_PROP(float, Delta, 3)
	RUITK_PROP_EVENT(OnValueChanged, 4)
	RUITK_PROPS_BODY(FRuitkSpinBoxProps,
					 RUITK_EQ(Value) RUITK_EQ(MinValue) RUITK_EQ(MaxValue) RUITK_EQ(Delta) RUITK_EQ(OnValueChanged))
};

/** SUniformWrapPanel (MultiSlot): a wrap panel that gives every child the same cell size. */
struct RUITKSLATE_API FRuitkUniformWrapPanelProps final : public FRuitkPropsBase
{
	RUITK_PROP(float, SlotPadding, 0)
	RUITK_PROP(FName, HAlign, 1)
	RUITK_PROPS_BODY(FRuitkUniformWrapPanelProps, RUITK_EQ(SlotPadding) RUITK_EQ(HAlign))
};

/** SRichTextBlock (Leaf): text with inline markup (default decorator set). AutoWrapText wraps. */
struct RUITKSLATE_API FRuitkRichTextBlockProps final : public FRuitkPropsBase
{
	RUITK_PROP(FText, Text, 0)
	RUITK_PROP(bool, bAutoWrapText, 1)

	virtual bool Equals(const FRuitkPropsBase& OtherBase) const override
	{
		const FRuitkRichTextBlockProps& Other = static_cast<const FRuitkRichTextBlockProps&>(OtherBase);
		auto TextEq = [](const FText& A, const FText& B) { return A.IdenticalTo(B) || A.ToString() == B.ToString(); };
		return SetBits == Other.SetBits && BaseFieldsEqual(Other) && TextEq(Text, Other.Text) &&
			   bAutoWrapText == Other.bAutoWrapText;
	}
};

/** SGridPanel (MultiSlot): places children by slot.column / slot.row (both default 0). */
struct RUITKSLATE_API FRuitkGridPanelProps final : public FRuitkPropsBase
{
	RUITK_PROPS_BODY(FRuitkGridPanelProps, )
};

/** SUniformGridPanel (MultiSlot): uniform cells placed by slot.column / slot.row. */
struct RUITKSLATE_API FRuitkUniformGridPanelProps final : public FRuitkPropsBase
{
	RUITK_PROP(float, SlotPadding, 0)
	RUITK_PROP(float, MinDesiredSlotWidth, 1)
	RUITK_PROP(float, MinDesiredSlotHeight, 2)
	RUITK_PROPS_BODY(FRuitkUniformGridPanelProps,
					 RUITK_EQ(SlotPadding) RUITK_EQ(MinDesiredSlotWidth) RUITK_EQ(MinDesiredSlotHeight))
};

// ── Batch 3 wave 1 (WIDGET_COMPLETION_PLAN) ────────────────────────────────────────────────

/** SColorBlock (Leaf): a color swatch. ALL props are construct-only (no engine setters) —
 *  every bit is on the reconstruct mask; the leaf is cheap to rebuild. AlphaDisplayMode =
 *  combined|separate|ignore. */
struct RUITKSLATE_API FRuitkColorBlockProps final : public FRuitkPropsBase
{
	RUITK_PROP(FLinearColor, Color, 0)
	RUITK_PROP(FVector2D, Size, 1)
	RUITK_PROP(bool, bUseSRGB, 2)
	RUITK_PROP(bool, bShowBackgroundForAlpha, 3)
	RUITK_PROP(bool, bColorIsHSV, 4)
	RUITK_PROP(FName, AlphaDisplayMode, 5)
	RUITK_PROPS_BODY(FRuitkColorBlockProps,
					 RUITK_EQ(Color) RUITK_EQ(Size) RUITK_EQ(bUseSRGB) RUITK_EQ(bShowBackgroundForAlpha)
						 RUITK_EQ(bColorIsHSV) RUITK_EQ(AlphaDisplayMode))
};

/** SSimpleGradient (Leaf-ish paint widget): two-stop gradient. Construct-only (no setters) —
 *  fully masked. Orientation = vertical (default) | horizontal. */
struct RUITKSLATE_API FRuitkSimpleGradientProps final : public FRuitkPropsBase
{
	RUITK_PROP(FLinearColor, StartColor, 0)
	RUITK_PROP(FLinearColor, EndColor, 1)
	RUITK_PROP(FName, Orientation, 2)
	RUITK_PROP(bool, bHasAlphaBackground, 3)
	RUITK_PROPS_BODY(FRuitkSimpleGradientProps,
					 RUITK_EQ(StartColor) RUITK_EQ(EndColor) RUITK_EQ(Orientation) RUITK_EQ(bHasAlphaBackground))
};

/** SComplexGradient (Leaf-ish paint widget): N-stop gradient. Construct-only — fully masked. */
struct RUITKSLATE_API FRuitkComplexGradientProps final : public FRuitkPropsBase
{
	RUITK_PROP(TArray<FLinearColor>, GradientColors, 0)
	RUITK_PROP(FName, Orientation, 1)
	RUITK_PROP(bool, bHasAlphaBackground, 2)
	RUITK_PROP(FVector2D, DesiredSizeOverride, 3)
	RUITK_PROPS_BODY(FRuitkComplexGradientProps, RUITK_EQ(GradientColors) RUITK_EQ(Orientation)
													 RUITK_EQ(bHasAlphaBackground) RUITK_EQ(DesiredSizeOverride))
};

/** SHyperlink (Leaf): a link. Text/Padding are construct-only (the inner text block bakes at
 *  Construct) — masked; OnNavigate binds at construction via the event proxy. */
struct RUITKSLATE_API FRuitkHyperlinkProps final : public FRuitkPropsBase
{
	RUITK_PROP(FText, Text, 0)
	RUITK_PROP(FMargin, Padding, 1)
	RUITK_PROP(FRuitkCallback, OnNavigate, 2)
	// FText has no operator== — identity first, then display-string compare (the TextBlock rule).
	virtual bool Equals(const FRuitkPropsBase& Other) const override
	{
		const FRuitkHyperlinkProps* Typed = static_cast<const FRuitkHyperlinkProps*>(&Other);
		if (!BaseFieldsEqual(Other))
		{
			return false;
		}
		if (!(Padding == Typed->Padding))
		{
			return false;
		}
		return Text.IdenticalTo(Typed->Text) || Text.ToString() == Typed->Text.ToString();
	}
};

/** SEnableBox (SingleContent): renders its child as if every ancestor were enabled. */
struct RUITKSLATE_API FRuitkEnableBoxProps final : public FRuitkPropsBase
{
	RUITK_PROPS_BODY(FRuitkEnableBoxProps, )
};

/** SScissorRectBox (SingleContent): hardware scissor-clips its child (render transforms
 *  included — unlike Clipping="clipToBounds"'s rect). */
struct RUITKSLATE_API FRuitkScissorRectBoxProps final : public FRuitkPropsBase
{
	RUITK_PROPS_BODY(FRuitkScissorRectBoxProps, )
};

/** SBackgroundBlur (SingleContent): post-process blur behind the content. Live setters. */
struct RUITKSLATE_API FRuitkBackgroundBlurProps final : public FRuitkPropsBase
{
	RUITK_PROP(float, BlurStrength, 0)
	RUITK_PROP(int32, BlurRadius, 1)
	RUITK_PROP(bool, bApplyAlphaToBlur, 2)
	RUITK_PROP(FMargin, Padding, 3)
	RUITK_PROPS_BODY(FRuitkBackgroundBlurProps,
					 RUITK_EQ(BlurStrength) RUITK_EQ(BlurRadius) RUITK_EQ(bApplyAlphaToBlur) RUITK_EQ(Padding))
};

/** SInvalidationPanel (SingleContent): opt-in retained-paint cache around static subtrees. */
struct RUITKSLATE_API FRuitkInvalidationPanelProps final : public FRuitkPropsBase
{
	RUITK_PROP(bool, bCanCache, 0)
	RUITK_PROPS_BODY(FRuitkInvalidationPanelProps, RUITK_EQ(bCanCache))
};

// ── Batch 3 wave 2 (WIDGET_COMPLETION_PLAN) ────────────────────────────────────────────────

/** SVolumeControl (Leaf): slider + mute composite. Volume/Muted are engine ATTRIBUTES with no
 *  setters — controlled via the reconstruct mask (D-16 semantics ride the rebuild); the two
 *  events report user edits back. */
struct RUITKSLATE_API FRuitkVolumeControlProps final : public FRuitkPropsBase
{
	RUITK_PROP(float, Volume, 0)
	RUITK_PROP(bool, bMuted, 1)
	RUITK_PROP_EVENT(OnVolumeChanged, 2)
	RUITK_PROP_EVENT(OnMuteChanged, 3)
	RUITK_PROPS_BODY(FRuitkVolumeControlProps,
					 RUITK_EQ(Volume) RUITK_EQ(bMuted) RUITK_EQ(OnVolumeChanged) RUITK_EQ(OnMuteChanged))
};

/** STextScroller (SingleContent): marquee auto-scroll around single-line text. Options are
 *  construct-only (masked); Start/Suspend/Reset ride P2 (`WidgetFromHandle<STextScroller>`). */
struct RUITKSLATE_API FRuitkTextScrollerProps final : public FRuitkPropsBase
{
	RUITK_PROP(float, Speed, 0)
	RUITK_PROP(float, StartDelay, 1)
	RUITK_PROP(float, EndDelay, 2)
	RUITK_PROP(FName, ScrollOrientation, 3)
	RUITK_PROPS_BODY(FRuitkTextScrollerProps,
					 RUITK_EQ(Speed) RUITK_EQ(StartDelay) RUITK_EQ(EndDelay) RUITK_EQ(ScrollOrientation))
};

/** SRadialBox (MultiSlot, bare slots — children place around the arc in declaration order).
 *  PreferredWidth is construct-only (masked); the angle/distribution params are live. */
struct RUITKSLATE_API FRuitkRadialBoxProps final : public FRuitkPropsBase
{
	RUITK_PROP(float, PreferredWidth, 0)
	RUITK_PROP(bool, bUseAllottedWidth, 1)
	RUITK_PROP(float, StartingAngle, 2)
	RUITK_PROP(bool, bDistributeItemsEvenly, 3)
	RUITK_PROP(float, AngleBetweenItems, 4)
	RUITK_PROP(float, SectorCentralAngle, 5)
	RUITK_PROPS_BODY(FRuitkRadialBoxProps,
					 RUITK_EQ(PreferredWidth) RUITK_EQ(bUseAllottedWidth) RUITK_EQ(StartingAngle)
						 RUITK_EQ(bDistributeItemsEvenly) RUITK_EQ(AngleBetweenItems) RUITK_EQ(SectorCentralAngle))
};

/** SColorWheel (Leaf): HSV wheel. SelectedColor is HSV-space, attribute-only (no setter) —
 *  controlled via the reconstruct mask; drag edits report through OnValueChanged. */
struct RUITKSLATE_API FRuitkColorWheelProps final : public FRuitkPropsBase
{
	RUITK_PROP(FLinearColor, SelectedColor, 0)
	RUITK_PROP_EVENT(OnValueChanged, 1)
	RUITK_PROP_EVENT(OnMouseCaptureBegin, 2)
	RUITK_PROP_EVENT(OnMouseCaptureEnd, 3)
	RUITK_PROPS_BODY(FRuitkColorWheelProps, RUITK_EQ(SelectedColor) RUITK_EQ(OnValueChanged)
												RUITK_EQ(OnMouseCaptureBegin) RUITK_EQ(OnMouseCaptureEnd))
};

/** SColorSpectrum (Leaf): saturation/value box — same controlled contract as ColorWheel. */
struct RUITKSLATE_API FRuitkColorSpectrumProps final : public FRuitkPropsBase
{
	RUITK_PROP(FLinearColor, SelectedColor, 0)
	RUITK_PROP_EVENT(OnValueChanged, 1)
	RUITK_PROP_EVENT(OnMouseCaptureBegin, 2)
	RUITK_PROP_EVENT(OnMouseCaptureEnd, 3)
	RUITK_PROPS_BODY(FRuitkColorSpectrumProps, RUITK_EQ(SelectedColor) RUITK_EQ(OnValueChanged)
												   RUITK_EQ(OnMouseCaptureBegin) RUITK_EQ(OnMouseCaptureEnd))
};

/** SLayeredImage (Leaf): SImage + N overlay layers, all live (RemoveAllLayers + AddLayer on a
 *  layer-list change; brushes by identity like Image). */
struct RUITKSLATE_API FRuitkLayeredImageProps final : public FRuitkPropsBase
{
	RUITK_PROP(FLinearColor, ColorAndOpacity, 0)
	RUITK_PROP(FVector2D, DesiredSizeOverride, 1)
	RUITK_PROP(TSharedPtr<FSlateBrush>, Image, 2)
	RUITK_PROP(TArray<TSharedPtr<FSlateBrush>>, Layers, 3)
	RUITK_PROPS_BODY(FRuitkLayeredImageProps,
					 RUITK_EQ(ColorAndOpacity) RUITK_EQ(DesiredSizeOverride) RUITK_EQ(Image) RUITK_EQ(Layers))
};

/** SInputKeySelector (Leaf composite): key-binding capture. SelectedKey is a live setter (key
 *  NAME; modifiers are the TD-016 multi-field payload trigger — key-only in v1). The
 *  capture-behavior args are construct-only (masked). */
struct RUITKSLATE_API FRuitkInputKeySelectorProps final : public FRuitkPropsBase
{
	RUITK_PROP(FName, SelectedKey, 0)
	RUITK_PROP(FText, KeySelectionText, 1)
	RUITK_PROP(FText, NoKeySpecifiedText, 2)
	RUITK_PROP(bool, bAllowModifierKeys, 3)
	RUITK_PROP(bool, bAllowGamepadKeys, 4)
	RUITK_PROP(bool, bEscapeCancelsSelection, 5)
	RUITK_PROP_EVENT(OnKeySelected, 6)
	RUITK_PROP_EVENT(OnIsSelectingKeyChanged, 7)

	// FText fields have no operator== — hand-written Equals (the EditableTextBox rule).
	virtual bool Equals(const FRuitkPropsBase& OtherBase) const override
	{
		const FRuitkInputKeySelectorProps& Other = static_cast<const FRuitkInputKeySelectorProps&>(OtherBase);
		auto TextEq = [](const FText& A, const FText& B) { return A.IdenticalTo(B) || A.ToString() == B.ToString(); };
		return BaseFieldsEqual(Other) && SelectedKey == Other.SelectedKey &&
			   TextEq(KeySelectionText, Other.KeySelectionText) &&
			   TextEq(NoKeySpecifiedText, Other.NoKeySpecifiedText) && bAllowModifierKeys == Other.bAllowModifierKeys &&
			   bAllowGamepadKeys == Other.bAllowGamepadKeys &&
			   bEscapeCancelsSelection == Other.bEscapeCancelsSelection && OnKeySelected == Other.OnKeySelected &&
			   OnIsSelectingKeyChanged == Other.OnIsSelectingKeyChanged;
	}
};

/** SEditableText (Leaf): the RAW single-line text edit (no box chrome) — full live setters;
 *  Text follows the D-16 controlled rule (skip-when-equal against the widget). */
struct RUITKSLATE_API FRuitkEditableTextProps final : public FRuitkPropsBase
{
	RUITK_PROP(FText, Text, 0)
	RUITK_PROP(FText, HintText, 1)
	RUITK_PROP(bool, bIsReadOnly, 2)
	RUITK_PROP(bool, bIsPassword, 3)
	RUITK_PROP(float, MinDesiredWidth, 4)
	RUITK_PROP_EVENT(OnTextChanged, 5)
	RUITK_PROP_EVENT(OnTextCommitted, 6)

	virtual bool Equals(const FRuitkPropsBase& OtherBase) const override
	{
		const FRuitkEditableTextProps& Other = static_cast<const FRuitkEditableTextProps&>(OtherBase);
		auto TextEq = [](const FText& A, const FText& B) { return A.IdenticalTo(B) || A.ToString() == B.ToString(); };
		return BaseFieldsEqual(Other) && TextEq(Text, Other.Text) && TextEq(HintText, Other.HintText) &&
			   bIsReadOnly == Other.bIsReadOnly && bIsPassword == Other.bIsPassword &&
			   MinDesiredWidth == Other.MinDesiredWidth && OnTextChanged == Other.OnTextChanged &&
			   OnTextCommitted == Other.OnTextCommitted;
	}
};

/** SInlineEditableTextBlock (Leaf): text that turns into an editor on slow-click/F2.
 *  bMultiLine is construct-only (masked); Enter/ExitEditingMode ride P2. */
struct RUITKSLATE_API FRuitkInlineEditableTextBlockProps final : public FRuitkPropsBase
{
	RUITK_PROP(FText, Text, 0)
	RUITK_PROP(FText, HintText, 1)
	RUITK_PROP(bool, bIsReadOnly, 2)
	RUITK_PROP(float, WrapTextAt, 3)
	RUITK_PROP(bool, bMultiLine, 4)
	RUITK_PROP_EVENT(OnTextCommitted, 5)

	virtual bool Equals(const FRuitkPropsBase& OtherBase) const override
	{
		const FRuitkInlineEditableTextBlockProps& Other =
			static_cast<const FRuitkInlineEditableTextBlockProps&>(OtherBase);
		auto TextEq = [](const FText& A, const FText& B) { return A.IdenticalTo(B) || A.ToString() == B.ToString(); };
		return BaseFieldsEqual(Other) && TextEq(Text, Other.Text) && TextEq(HintText, Other.HintText) &&
			   bIsReadOnly == Other.bIsReadOnly && WrapTextAt == Other.WrapTextAt && bMultiLine == Other.bMultiLine &&
			   OnTextCommitted == Other.OnTextCommitted;
	}
};

/** SVirtualKeyboardEntry (Leaf): the mobile OS-keyboard text field. Text is live (D-16);
 *  HintText/bIsReadOnly/KeyboardType are construct-only (masked). Ticks. */
struct RUITKSLATE_API FRuitkVirtualKeyboardEntryProps final : public FRuitkPropsBase
{
	RUITK_PROP(FText, Text, 0)
	RUITK_PROP(FText, HintText, 1)
	RUITK_PROP(bool, bIsReadOnly, 2)
	RUITK_PROP(FName, KeyboardType, 3)
	RUITK_PROP_EVENT(OnTextChanged, 4)
	RUITK_PROP_EVENT(OnTextCommitted, 5)

	virtual bool Equals(const FRuitkPropsBase& OtherBase) const override
	{
		const FRuitkVirtualKeyboardEntryProps& Other = static_cast<const FRuitkVirtualKeyboardEntryProps&>(OtherBase);
		auto TextEq = [](const FText& A, const FText& B) { return A.IdenticalTo(B) || A.ToString() == B.ToString(); };
		return BaseFieldsEqual(Other) && TextEq(Text, Other.Text) && TextEq(HintText, Other.HintText) &&
			   bIsReadOnly == Other.bIsReadOnly && KeyboardType == Other.KeyboardType &&
			   OnTextChanged == Other.OnTextChanged && OnTextCommitted == Other.OnTextCommitted;
	}
};

/** SColorGradingWheel (Leaf; the AdvancedWidgets module — live one; the Slate-module twin is
 *  deprecated 5.5): all attrs have live attribute setters. SelectedColor is HSV-space. */
struct RUITKSLATE_API FRuitkColorGradingWheelProps final : public FRuitkPropsBase
{
	RUITK_PROP(FLinearColor, SelectedColor, 0)
	RUITK_PROP(int32, DesiredWheelSize, 1)
	RUITK_PROP(float, ExponentDisplacement, 2)
	RUITK_PROP_EVENT(OnValueChanged, 3)
	RUITK_PROP_EVENT(OnMouseCaptureBegin, 4)
	RUITK_PROP_EVENT(OnMouseCaptureEnd, 5)
	RUITK_PROPS_BODY(FRuitkColorGradingWheelProps,
					 RUITK_EQ(SelectedColor) RUITK_EQ(DesiredWheelSize) RUITK_EQ(ExponentDisplacement)
						 RUITK_EQ(OnValueChanged) RUITK_EQ(OnMouseCaptureBegin) RUITK_EQ(OnMouseCaptureEnd))
};

namespace Ruitk::Slate
{
	RUITKSLATE_API FRuitkElementTypeId VerticalBoxType();
	RUITKSLATE_API FRuitkElementTypeId HorizontalBoxType();
	RUITKSLATE_API FRuitkElementTypeId ButtonType();
	RUITKSLATE_API FRuitkElementTypeId OverlayType();
	RUITKSLATE_API FRuitkElementTypeId CanvasType();

	RUITKSLATE_API FRuitkNode VerticalBox(FRuitkVerticalBoxProps Props = FRuitkVerticalBoxProps(),
										  TArray<FRuitkNode> Children = TArray<FRuitkNode>(),
										  FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode HorizontalBox(FRuitkHorizontalBoxProps Props = FRuitkHorizontalBoxProps(),
											TArray<FRuitkNode> Children = TArray<FRuitkNode>(),
											FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode Button(FRuitkButtonProps Props = FRuitkButtonProps(),
									 TArray<FRuitkNode> Children = TArray<FRuitkNode>(), FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode Overlay(FRuitkOverlayProps Props = FRuitkOverlayProps(),
									  TArray<FRuitkNode> Children = TArray<FRuitkNode>(), FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode Canvas(FRuitkCanvasPanelProps Props = FRuitkCanvasPanelProps(),
									 TArray<FRuitkNode> Children = TArray<FRuitkNode>(), FRuitkKey Key = FRuitkKey());

	RUITKSLATE_API FRuitkNode Border(FRuitkBorderProps Props = FRuitkBorderProps(),
									 TArray<FRuitkNode> Children = TArray<FRuitkNode>(), FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode Box(FRuitkBoxProps Props = FRuitkBoxProps(),
								  TArray<FRuitkNode> Children = TArray<FRuitkNode>(), FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode Image(FRuitkImageProps Props = FRuitkImageProps(), FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode ScrollBox(FRuitkScrollBoxProps Props = FRuitkScrollBoxProps(),
										TArray<FRuitkNode> Children = TArray<FRuitkNode>(),
										FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode Spacer(FRuitkSpacerProps Props = FRuitkSpacerProps(), FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode EditableTextBox(FRuitkEditableTextBoxProps Props = FRuitkEditableTextBoxProps(),
											  FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode CheckBox(FRuitkCheckBoxProps Props = FRuitkCheckBoxProps(),
									   TArray<FRuitkNode> Children = TArray<FRuitkNode>(), FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode Slider(FRuitkSliderProps Props = FRuitkSliderProps(), FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode ProgressBar(FRuitkProgressBarProps Props = FRuitkProgressBarProps(),
										  FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode RuitkCanvas(FRuitkCanvasProps Props = FRuitkCanvasProps(), FRuitkKey Key = FRuitkKey());

	// ── Batch 2 (Phase 7 step 8) factories ────────────────────────────────────────────────
	RUITKSLATE_API FRuitkNode WidgetSwitcher(FRuitkWidgetSwitcherProps Props = FRuitkWidgetSwitcherProps(),
											 TArray<FRuitkNode> Children = TArray<FRuitkNode>(),
											 FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode ScaleBox(FRuitkScaleBoxProps Props = FRuitkScaleBoxProps(),
									   TArray<FRuitkNode> Children = TArray<FRuitkNode>(), FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode Throbber(FRuitkThrobberProps Props = FRuitkThrobberProps(), FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode WrapBox(FRuitkWrapBoxProps Props = FRuitkWrapBoxProps(),
									  TArray<FRuitkNode> Children = TArray<FRuitkNode>(), FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode MultiLineEditableTextBox(
		FRuitkMultiLineEditableTextBoxProps Props = FRuitkMultiLineEditableTextBoxProps(), FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode SearchBox(FRuitkSearchBoxProps Props = FRuitkSearchBoxProps(),
										FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode SafeZone(FRuitkSafeZoneProps Props = FRuitkSafeZoneProps(),
									   TArray<FRuitkNode> Children = TArray<FRuitkNode>(), FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode DPIScaler(FRuitkDPIScalerProps Props = FRuitkDPIScalerProps(),
										TArray<FRuitkNode> Children = TArray<FRuitkNode>(),
										FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode Separator(FRuitkSeparatorProps Props = FRuitkSeparatorProps(),
										FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode SpinBox(FRuitkSpinBoxProps Props = FRuitkSpinBoxProps(), FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode UniformWrapPanel(FRuitkUniformWrapPanelProps Props = FRuitkUniformWrapPanelProps(),
											   TArray<FRuitkNode> Children = TArray<FRuitkNode>(),
											   FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode RichTextBlock(FRuitkRichTextBlockProps Props = FRuitkRichTextBlockProps(),
											FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode GridPanel(FRuitkGridPanelProps Props = FRuitkGridPanelProps(),
										TArray<FRuitkNode> Children = TArray<FRuitkNode>(),
										FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode ColorBlock(FRuitkColorBlockProps Props = FRuitkColorBlockProps(),
										 FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode SimpleGradient(FRuitkSimpleGradientProps Props = FRuitkSimpleGradientProps(),
											 FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode ComplexGradient(FRuitkComplexGradientProps Props = FRuitkComplexGradientProps(),
											  FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode Hyperlink(FRuitkHyperlinkProps Props = FRuitkHyperlinkProps(),
										FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode EnableBox(FRuitkEnableBoxProps Props = FRuitkEnableBoxProps(),
										TArray<FRuitkNode> Children = TArray<FRuitkNode>(),
										FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode ScissorRectBox(FRuitkScissorRectBoxProps Props = FRuitkScissorRectBoxProps(),
											 TArray<FRuitkNode> Children = TArray<FRuitkNode>(),
											 FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode BackgroundBlur(FRuitkBackgroundBlurProps Props = FRuitkBackgroundBlurProps(),
											 TArray<FRuitkNode> Children = TArray<FRuitkNode>(),
											 FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode InvalidationPanel(FRuitkInvalidationPanelProps Props = FRuitkInvalidationPanelProps(),
												TArray<FRuitkNode> Children = TArray<FRuitkNode>(),
												FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode VolumeControl(FRuitkVolumeControlProps Props = FRuitkVolumeControlProps(),
											FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode TextScroller(FRuitkTextScrollerProps Props = FRuitkTextScrollerProps(),
										   TArray<FRuitkNode> Children = TArray<FRuitkNode>(),
										   FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode RadialBox(FRuitkRadialBoxProps Props = FRuitkRadialBoxProps(),
										TArray<FRuitkNode> Children = TArray<FRuitkNode>(),
										FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode ConstraintCanvas(FRuitkConstraintCanvasProps Props = FRuitkConstraintCanvasProps(),
											   TArray<FRuitkNode> Children = TArray<FRuitkNode>(),
											   FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode Splitter(FRuitkSplitterProps Props = FRuitkSplitterProps(),
									   TArray<FRuitkNode> Children = TArray<FRuitkNode>(), FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode Splitter2x2(FRuitkSplitter2x2Props Props = FRuitkSplitter2x2Props(),
										  TArray<FRuitkNode> Children = TArray<FRuitkNode>(),
										  FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode MenuAnchor(FRuitkMenuAnchorProps Props = FRuitkMenuAnchorProps(),
										 TArray<FRuitkNode> Children = TArray<FRuitkNode>(),
										 FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode WindowTitleBarArea(FRuitkWindowTitleBarAreaProps Props = FRuitkWindowTitleBarAreaProps(),
												 TArray<FRuitkNode> Children = TArray<FRuitkNode>(),
												 FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode NumericDropDown(FRuitkNumericDropDownProps Props = FRuitkNumericDropDownProps(),
											  FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode BreadcrumbTrail(FRuitkBreadcrumbTrailProps Props = FRuitkBreadcrumbTrailProps(),
											  FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode NotificationList(FRuitkNotificationListProps Props = FRuitkNotificationListProps(),
											   FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode SearchableComboBox(FRuitkSearchableComboBoxProps Props = FRuitkSearchableComboBoxProps(),
												 FRuitkKey Key = FRuitkKey()); // sinceUE 5.7
	RUITKSLATE_API FRuitkNode LinkedBox(FRuitkLinkedBoxProps Props = FRuitkLinkedBoxProps(),
										TArray<FRuitkNode> Children = TArray<FRuitkNode>(),
										FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode VirtualJoystick(FRuitkVirtualJoystickProps Props = FRuitkVirtualJoystickProps(),
											  FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode VectorInputBox(FRuitkVectorInputBoxProps Props = FRuitkVectorInputBoxProps(),
											 FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode RotatorInputBox(FRuitkRotatorInputBoxProps Props = FRuitkRotatorInputBoxProps(),
											  FRuitkKey Key = FRuitkKey());

	/** P4 command: push a toast onto a Ref-captured <NotificationList> (no-op on a dead/wrong
	 *  handle). The full FNotificationInfo surface stays reachable via WidgetFromHandle. */
	RUITKSLATE_API void PushNotification(const FRuitkHostHandle& Handle, const FText& Text,
										 float ExpireDuration = 4.0f);
	RUITKSLATE_API FRuitkNode ColorWheel(FRuitkColorWheelProps Props = FRuitkColorWheelProps(),
										 FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode ColorSpectrum(FRuitkColorSpectrumProps Props = FRuitkColorSpectrumProps(),
											FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode LayeredImage(FRuitkLayeredImageProps Props = FRuitkLayeredImageProps(),
										   FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode InputKeySelector(FRuitkInputKeySelectorProps Props = FRuitkInputKeySelectorProps(),
											   FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode EditableText(FRuitkEditableTextProps Props = FRuitkEditableTextProps(),
										   FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode InlineEditableTextBlock(
		FRuitkInlineEditableTextBlockProps Props = FRuitkInlineEditableTextBlockProps(), FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode VirtualKeyboardEntry(
		FRuitkVirtualKeyboardEntryProps Props = FRuitkVirtualKeyboardEntryProps(), FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode ColorGradingWheel(FRuitkColorGradingWheelProps Props = FRuitkColorGradingWheelProps(),
												FRuitkKey Key = FRuitkKey());
	RUITKSLATE_API FRuitkNode UniformGridPanel(FRuitkUniformGridPanelProps Props = FRuitkUniformGridPanelProps(),
											   TArray<FRuitkNode> Children = TArray<FRuitkNode>(),
											   FRuitkKey Key = FRuitkKey());

	/** Wrap a paint lambda ONCE (UseMemo/UseRef it) — the canvas repaints on identity change. */
	RUITKSLATE_API TSharedPtr<FRuitkDrawFn> MakeDrawFn(FRuitkDrawFn Fn);

	/** Register the built-in adapters (module startup; idempotent — replaces on re-run). */
	RUITKSLATE_API void RegisterBuiltinAdapters();
} // namespace Ruitk::Slate
