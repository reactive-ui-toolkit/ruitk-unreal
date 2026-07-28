// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Style v1 (D-13, load-bearing): the central style-key registry. Hot-path keys map to
// SETTERS (never construct args — a style tweak must never destroy a widget) with explicit
// RESET behavior: style keys DO reset when removed between renders (unlike plain props —
// the family contract). Two layers ship in v1: inline `style` dicts and the `classes`
// merge layer (classes apply in order, inline style wins). The stylesheet layer is TD-002.
//
// Key names are 1:1 with the Unreal setter/property they drive (D-33). Generic keys (any
// SWidget): RenderOpacity, Visibility (Visible|Collapsed|Hidden|HitTestInvisible|
// SelfHitTestInvisible), Enabled, RenderTranslation (Vector2), RenderScale (Float),
// RenderTransformAngle (Float, degrees), RenderTransformPivot (Vector2). Widget-specific
// keys route through IRuitkElementAdapter::ApplyStyleKey (e.g. TextBlock "ColorAndOpacity",
// "Font.Size", "Justification", "AutoWrapText") — same semantics, adapter-owned setters.
// FName matching is case-insensitive; docs use the Unreal casing.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "RuitkTypes.h"
#include "Types/SlateEnums.h"
#include "Widgets/SWidget.h"

class IRuitkElementAdapter;

namespace Ruitk::Slate
{
	/** Register/replace a named style class (the `classes` layer). */
	RUITKSLATE_API void RegisterStyleClass(FName ClassName, FRuitkStyleDict Style);

	RUITKSLATE_API const FRuitkStyleDict* FindStyleClass(FName ClassName);

	/** Build the effective dict: classes in order, then inline style overrides. Returns
	 *  null when nothing contributes. */
	RUITKSLATE_API TSharedPtr<FRuitkStyleDict> BuildEffectiveStyle(const TArray<FName>& Classes,
																	  const TSharedPtr<FRuitkStyleDict>& InlineStyle);

	/** Diff-apply Old -> New on the widget: changed/new keys apply; keys present in Old but
	 *  absent in New RESET to their defaults. Unknown keys warn once per key name. Adapter
	 *  handles widget-specific keys first (may be null). */
	RUITKSLATE_API void ApplyStyleDiff(SWidget& Widget, IRuitkElementAdapter* Adapter, const FRuitkStyleDict* Old,
											const FRuitkStyleDict* New);

	// ── TD-002: the THIRD layer — @theme tokens + @uss stylesheets ─────────────────────────
	//
	// Cascade (lowest -> highest): theme tokens (resolved into values) < class rules < inline
	// style. A style value that is a String beginning with `$` is a TOKEN REFERENCE, resolved
	// against the ACTIVE theme when the effective style is built (missing token -> warn + kept).

	/** Register/replace a named theme (a token name -> FRuitkValue map). */
	RUITKSLATE_API void RegisterTheme(FName ThemeName, FRuitkStyleDict Tokens);

	/** Select the active theme used to resolve `$token` references. NAME_None = no theme. */
	RUITKSLATE_API void SetActiveTheme(FName ThemeName);
	RUITKSLATE_API FName GetActiveTheme();

	/** Resolve a token against the active theme (null if unknown / no active theme). */
	RUITKSLATE_API const FRuitkValue* ResolveThemeToken(FName TokenName);

	/** Parse a `.uss`-style stylesheet source and register its `@theme <name> { ... }` blocks
	 *  and `.<class> { key: value; }` rules. Values: `#rrggbb[aa]` color, numbers (int/float),
	 *  true/false, `$token` refs, "quoted" strings, else a bare Name. Returns the count of
	 *  (themes + classes) registered. Idempotent — re-loading replaces. */
	RUITKSLATE_API int32 LoadStylesheet(const FString& Source);

	/** Parse one style value literal into an FRuitkValue (the stylesheet grammar; also handy for
	 *  the markup codegen's @uss lowering). */
	RUITKSLATE_API FRuitkValue ParseStyleValue(const FString& Literal);
} // namespace Ruitk::Slate

namespace Ruitk
{
	// TD-013: fluent, compile-time-safe authoring for style + slot.* dicts. One method per
	// registered v1 key — FName keys and FRuitkValue kinds match the .uetkx markup EXACTLY, so a
	// C++-authored dict behaves identically to markup (single vocabulary). `Set` is the forward-
	// compat escape hatch for keys not yet surfaced as a typed method. Header-only: it just
	// populates the FRuitkStyleDict (TMap) the props take; no runtime cost beyond the map.

	/** `Ruitk::Style().RenderOpacity(0.5f).ColorAndOpacity(FLinearColor::Red)` -> the style dict. */
	struct FRuitkStyleBuilder
	{
		TSharedRef<FRuitkStyleDict> Dict = MakeShared<FRuitkStyleDict>();

		FRuitkStyleBuilder& RenderOpacity(float V) { return Set(FName(TEXT("RenderOpacity")), FRuitkValue(V)); }
		FRuitkStyleBuilder& Visibility(FName V) { return Set(FName(TEXT("Visibility")), FRuitkValue(V)); }
		FRuitkStyleBuilder& Enabled(bool V) { return Set(FName(TEXT("Enabled")), FRuitkValue(V)); }
		FRuitkStyleBuilder& RenderTranslation(const FVector2D& V)
		{
			return Set(FName(TEXT("RenderTranslation")), FRuitkValue(V));
		}
		FRuitkStyleBuilder& RenderScale(float V) { return Set(FName(TEXT("RenderScale")), FRuitkValue(V)); }
		FRuitkStyleBuilder& RenderTransformAngle(float Degrees)
		{
			return Set(FName(TEXT("RenderTransformAngle")), FRuitkValue(Degrees));
		}
		FRuitkStyleBuilder& RenderTransformPivot(const FVector2D& V)
		{
			return Set(FName(TEXT("RenderTransformPivot")), FRuitkValue(V));
		}
		FRuitkStyleBuilder& ColorAndOpacity(const FLinearColor& V)
		{
			return Set(FName(TEXT("ColorAndOpacity")), FRuitkValue(V));
		}
		FRuitkStyleBuilder& FontSize(float V) { return Set(FName(TEXT("Font.Size")), FRuitkValue(V)); }
		FRuitkStyleBuilder& Justification(FName V) { return Set(FName(TEXT("Justification")), FRuitkValue(V)); }
		FRuitkStyleBuilder& AutoWrapText(bool V) { return Set(FName(TEXT("AutoWrapText")), FRuitkValue(V)); }
		FRuitkStyleBuilder& FillColorAndOpacity(const FLinearColor& V)
		{
			return Set(FName(TEXT("FillColorAndOpacity")), FRuitkValue(V));
		}
		FRuitkStyleBuilder& Clipping(FName V) { return Set(FName(TEXT("Clipping")), FRuitkValue(V)); }
		FRuitkStyleBuilder& ToolTipText(const FText& V) { return Set(FName(TEXT("ToolTipText")), FRuitkValue(V)); }
		FRuitkStyleBuilder& LineHeightPercentage(float V)
		{
			return Set(FName(TEXT("LineHeightPercentage")), FRuitkValue(V));
		}
		FRuitkStyleBuilder& OverflowPolicy(FName V) { return Set(FName(TEXT("OverflowPolicy")), FRuitkValue(V)); }

		FRuitkStyleBuilder& Set(FName Key, FRuitkValue V)
		{
			Dict->Add(Key, MoveTemp(V));
			return *this;
		}
		operator TSharedPtr<FRuitkStyleDict>() const { return Dict; }
		TSharedPtr<FRuitkStyleDict> Build() const { return Dict; }
	};

	inline FRuitkStyleBuilder Style()
	{
		return FRuitkStyleBuilder();
	}

	/** `Ruitk::Slot().Padding(8).HAlign(HAlign_Center).Fill(1.f)` -> the slot.* dict. */
	struct FRuitkSlotBuilder
	{
		TSharedRef<FRuitkStyleDict> Dict = MakeShared<FRuitkStyleDict>();

		FRuitkSlotBuilder& Padding(const FMargin& M)
		{
			return Set(FName(TEXT("slot.padding")),
					   FRuitkValue(FString::Printf(TEXT("%g,%g,%g,%g"), M.Left, M.Top, M.Right, M.Bottom)));
		}
		FRuitkSlotBuilder& Padding(float Uniform) { return Set(FName(TEXT("slot.padding")), FRuitkValue(Uniform)); }
		FRuitkSlotBuilder& HAlign(EHorizontalAlignment H)
		{
			return Set(FName(TEXT("slot.halign")), FRuitkValue(HAlignName(H)));
		}
		FRuitkSlotBuilder& VAlign(EVerticalAlignment V)
		{
			return Set(FName(TEXT("slot.valign")), FRuitkValue(VAlignName(V)));
		}
		FRuitkSlotBuilder& Fill(float Coefficient) { return Set(FName(TEXT("slot.fill")), FRuitkValue(Coefficient)); }
		FRuitkSlotBuilder& ZOrder(int32 Z) { return Set(FName(TEXT("Slot.ZOrder")), FRuitkValue(static_cast<int64>(Z))); }
		FRuitkSlotBuilder& Position(const FVector2D& V) { return Set(FName(TEXT("Slot.Position")), FRuitkValue(V)); }
		FRuitkSlotBuilder& Size(const FVector2D& V) { return Set(FName(TEXT("Slot.Size")), FRuitkValue(V)); }
		FRuitkSlotBuilder& Offset(const FMargin& M)
		{
			return Set(FName(TEXT("Slot.Offset")),
					   FRuitkValue(FString::Printf(TEXT("%g,%g,%g,%g"), M.Left, M.Top, M.Right, M.Bottom)));
		}
		FRuitkSlotBuilder& Anchors(const FVector2D& MinMax)
		{
			return Set(FName(TEXT("Slot.Anchors")), FRuitkValue(MinMax));
		}
		FRuitkSlotBuilder& Alignment(const FVector2D& V) { return Set(FName(TEXT("Slot.Alignment")), FRuitkValue(V)); }
		FRuitkSlotBuilder& AutoSize(bool V) { return Set(FName(TEXT("Slot.AutoSize")), FRuitkValue(V)); }
		// R12: GridPanel/UniformGridPanel placement — these were consumed by the adapters all
		// along but missing from the exported canon (the LSP flagged them as unknown).
		FRuitkSlotBuilder& Column(int32 V) { return Set(FName(TEXT("Slot.Column")), FRuitkValue(static_cast<int64>(V))); }
		FRuitkSlotBuilder& Row(int32 V) { return Set(FName(TEXT("Slot.Row")), FRuitkValue(static_cast<int64>(V))); }
		FRuitkSlotBuilder& Role(FName V) { return Set(FName(TEXT("Slot.Role")), FRuitkValue(V)); }
		FRuitkSlotBuilder& SizeRule(FName V) { return Set(FName(TEXT("Slot.SizeRule")), FRuitkValue(V)); }
		FRuitkSlotBuilder& SizeValue(float V) { return Set(FName(TEXT("Slot.SizeValue")), FRuitkValue(V)); }
		FRuitkSlotBuilder& MinSize(float V) { return Set(FName(TEXT("Slot.MinSize")), FRuitkValue(V)); }
		FRuitkSlotBuilder& Resizable(bool V) { return Set(FName(TEXT("Slot.Resizable")), FRuitkValue(V)); }

		FRuitkSlotBuilder& Set(FName Key, FRuitkValue V)
		{
			Dict->Add(Key, MoveTemp(V));
			return *this;
		}
		operator TSharedPtr<FRuitkStyleDict>() const { return Dict; }
		TSharedPtr<FRuitkStyleDict> Build() const { return Dict; }

	private:
		static FName HAlignName(EHorizontalAlignment H)
		{
			switch (H)
			{
			case HAlign_Left:
				return FName(TEXT("left"));
			case HAlign_Center:
				return FName(TEXT("center"));
			case HAlign_Right:
				return FName(TEXT("right"));
			default:
				return FName(TEXT("fill"));
			}
		}
		static FName VAlignName(EVerticalAlignment V)
		{
			switch (V)
			{
			case VAlign_Top:
				return FName(TEXT("top"));
			case VAlign_Center:
				return FName(TEXT("center"));
			case VAlign_Bottom:
				return FName(TEXT("bottom"));
			default:
				return FName(TEXT("fill"));
			}
		}
	};

	inline FRuitkSlotBuilder Slot()
	{
		return FRuitkSlotBuilder();
	}
} // namespace Ruitk
