// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuitkStyle.h"

#include "RuitkElementAdapter.h"
#include "RuitkSlateLog.h"
#include "RuitkSlotValue.h"

namespace
{
	FCriticalSection GClassLock;
	TMap<FName, FRuitkStyleDict> GStyleClasses;

	// TD-002: theme registry + active theme (guarded by GClassLock alongside the classes).
	TMap<FName, FRuitkStyleDict> GThemes;
	FName GActiveTheme = NAME_None;

	/** A style value that references a theme token: a String starting with '$'. */
	bool IsTokenRef(const FRuitkValue& V)
	{
		return V.Kind == FRuitkValue::EKind::String && V.StringValue.StartsWith(TEXT("$"));
	}

	float AsFloat(const FRuitkValue& V)
	{
		// R11: route through the SLOT-1 hardened reader — the toolchain emits literal style
		// values as Strings (`RenderOpacity="0.5"`), and the union-field read silently gave 0.
		return Ruitk::Slate::SlotValue::AsFloat(V);
	}

	EVisibility VisibilityOf(const FRuitkValue& V)
	{
		const FName Name = V.Kind == FRuitkValue::EKind::Name ? V.NameValue : FName(*V.StringValue);
		if (Name == FName(TEXT("collapsed")))
		{
			return EVisibility::Collapsed;
		}
		if (Name == FName(TEXT("hidden")))
		{
			return EVisibility::Hidden;
		}
		if (Name == FName(TEXT("hitTestInvisible")))
		{
			return EVisibility::HitTestInvisible;
		}
		if (Name == FName(TEXT("selfHitTestInvisible")))
		{
			return EVisibility::SelfHitTestInvisible;
		}
		return EVisibility::Visible;
	}

	void ApplyRenderTransform(SWidget& W, const FRuitkStyleDict* Dict)
	{
		// RenderTranslation/RenderScale/RenderTransformAngle compose into ONE transform;
		// absent -> identity (the UMG UWidget setter names, D-33).
		FVector2D Translation = FVector2D::ZeroVector;
		float Scale = 1.0f;
		float AngleDeg = 0.0f;
		bool bAny = false;
		if (Dict != nullptr)
		{
			if (const FRuitkValue* T = Dict->Find(FName(TEXT("RenderTranslation"))))
			{
				Translation = Ruitk::Slate::SlotValue::AsVector2(*T); // parses the `"x,y"` literal form (R11)
				bAny = true;
			}
			if (const FRuitkValue* S = Dict->Find(FName(TEXT("RenderScale"))))
			{
				Scale = AsFloat(*S);
				bAny = true;
			}
			if (const FRuitkValue* A = Dict->Find(FName(TEXT("RenderTransformAngle"))))
			{
				AngleDeg = AsFloat(*A);
				bAny = true;
			}
		}
		if (bAny)
		{
			W.SetRenderTransform(::TransformCast<FSlateRenderTransform>(
				::Concatenate(FScale2D(Scale), FQuat2D(FMath::DegreesToRadians(AngleDeg)), FVector2D(Translation))));
		}
		else
		{
			W.SetRenderTransform(TOptional<FSlateRenderTransform>()); // reset -> identity
		}
	}

	bool IsTransformKey(FName Key)
	{
		return Key == FName(TEXT("RenderTranslation")) || Key == FName(TEXT("RenderScale")) ||
			   Key == FName(TEXT("RenderTransformAngle"));
	}

	/** Apply one generic key (Value null = RESET to default). Returns false when unknown. */
	bool ApplyGenericKey(SWidget& W, FName Key, const FRuitkValue* Value)
	{
		if (Key == FName(TEXT("RenderOpacity")))
		{
			W.SetRenderOpacity(Value != nullptr ? AsFloat(*Value) : 1.0f);
			return true;
		}
		if (Key == FName(TEXT("visibility")))
		{
			W.SetVisibility(Value != nullptr ? VisibilityOf(*Value) : EVisibility::Visible);
			return true;
		}
		if (Key == FName(TEXT("enabled")))
		{
			W.SetEnabled(Value == nullptr || Ruitk::Slate::SlotValue::AsBool(*Value, true));
			return true;
		}
		if (Key == FName(TEXT("RenderTransformPivot")))
		{
			W.SetRenderTransformPivot(Value != nullptr ? Ruitk::Slate::SlotValue::AsVector2(*Value)
													   : FVector2D::ZeroVector);
			return true;
		}
		if (Key == FName(TEXT("ToolTipText")))
		{
			// Universal tooltip (P1, WIDGET_COMPLETION_PLAN): the family contract is a PROP on
			// every element, never a wrapper widget — SWidget::SetToolTipText is the loyal
			// setter. Reset = empty text (Slate shows no tooltip for empty).
			const FText Text = Value == nullptr							 ? FText::GetEmpty()
							   : Value->Kind == FRuitkValue::EKind::Text ? Value->TextValue
							   : Value->Kind == FRuitkValue::EKind::Name ? FText::FromName(Value->NameValue)
																		 : FText::FromString(Value->StringValue);
			W.SetToolTipText(Text);
			return true;
		}
		if (Key == FName(TEXT("Clipping")))
		{
			// D-33 loyal: EWidgetClipping member names, lowerCamel. Slate does NOT clip children
			// to bounds by default — absolute-placed content (Canvas framebuffers) needs this.
			EWidgetClipping Clipping = EWidgetClipping::Inherit; // reset = the SWidget default
			if (Value != nullptr)
			{
				const FName Name =
					Value->Kind == FRuitkValue::EKind::Name ? Value->NameValue : FName(*Value->StringValue);
				Clipping = Name == FName(TEXT("clipToBounds")) ? EWidgetClipping::ClipToBounds
						   : Name == FName(TEXT("clipToBoundsWithoutIntersecting"))
							   ? EWidgetClipping::ClipToBoundsWithoutIntersecting
						   : Name == FName(TEXT("clipToBoundsAlways")) ? EWidgetClipping::ClipToBoundsAlways
						   : Name == FName(TEXT("onDemand"))		   ? EWidgetClipping::OnDemand
																	   : EWidgetClipping::Inherit;
			}
			W.SetClipping(Clipping);
			return true;
		}
		return false; // transform keys handled together; everything else is adapter/unknown
	}

	void WarnUnknownKey(FName Key)
	{
		static FCriticalSection WarnLock;
		static TSet<FName> Warned;
		FScopeLock Lock(&WarnLock);
		if (!Warned.Contains(Key))
		{
			Warned.Add(Key);
			UE_LOG(LogRuitkSlate, Warning, TEXT("[Ruitk] unknown style key '%s' (ignored)"), *Key.ToString());
		}
	}
} // namespace

namespace Ruitk::Slate
{
	void RegisterStyleClass(FName ClassName, FRuitkStyleDict Style)
	{
		FScopeLock Lock(&GClassLock);
		GStyleClasses.Add(ClassName, MoveTemp(Style));
	}

	const FRuitkStyleDict* FindStyleClass(FName ClassName)
	{
		FScopeLock Lock(&GClassLock);
		return GStyleClasses.Find(ClassName);
	}

	// TD-002 third layer: resolve `$token` references in `Out` against the active theme (lowest
	// priority — the token supplies the VALUE a class/inline key referenced).
	void ResolveThemeTokens(FRuitkStyleDict& Out)
	{
		FScopeLock Lock(&GClassLock);
		if (GActiveTheme == NAME_None)
		{
			return;
		}
		const FRuitkStyleDict* Theme = GThemes.Find(GActiveTheme);
		if (Theme == nullptr)
		{
			return;
		}
		for (TPair<FName, FRuitkValue>& Pair : Out)
		{
			// Resolve a `$token` chain transitively — a token whose value is itself `$other` (bughunt
			// STYLE-3, which previously left the second-hop `$other` string reaching the adapter). Bounded
			// hop count breaks a cycle (`$a -> $b -> $a`) rather than looping forever.
			constexpr int32 MaxHops = 8;
			int32 Hops = 0;
			while (IsTokenRef(Pair.Value) && Hops++ < MaxHops)
			{
				const FName Token(*Pair.Value.StringValue.RightChop(1));
				const FRuitkValue* Resolved = Theme->Find(Token);
				if (Resolved == nullptr)
				{
					WarnUnknownKey(FName(*(TEXT("token:") + Token.ToString())));
					break;
				}
				Pair.Value = *Resolved;
			}
			if (IsTokenRef(Pair.Value)) // still a ref after the hop budget → a cycle/too-deep chain
			{
				WarnUnknownKey(FName(*(TEXT("token-cycle:") + Pair.Value.StringValue)));
			}
		}
	}

	/** True iff a theme is active AND `Dict` holds at least one `$token` ref needing resolution. */
	bool NeedsTokenResolution(const FRuitkStyleDict& Dict)
	{
		FScopeLock Lock(&GClassLock);
		if (GActiveTheme == NAME_None)
		{
			return false;
		}
		for (const TPair<FName, FRuitkValue>& Pair : Dict)
		{
			if (IsTokenRef(Pair.Value))
			{
				return true;
			}
		}
		return false;
	}

	TSharedPtr<FRuitkStyleDict> BuildEffectiveStyle(const TArray<FName>& Classes,
													const TSharedPtr<FRuitkStyleDict>& InlineStyle)
	{
		if (Classes.IsEmpty())
		{
			// Fast path: no classes. Share the inline dict untouched UNLESS it carries `$token` refs that
			// an active theme must resolve (bughunt B4 — the early return skipped resolution, so an
			// inline-only `$token` reached the adapter as a raw String). Copy+resolve only then.
			if (!InlineStyle.IsValid() || !NeedsTokenResolution(*InlineStyle))
			{
				return InlineStyle;
			}
			TSharedPtr<FRuitkStyleDict> Out = MakeShared<FRuitkStyleDict>(*InlineStyle);
			ResolveThemeTokens(*Out);
			return Out->IsEmpty() ? TSharedPtr<FRuitkStyleDict>() : Out;
		}
		TSharedPtr<FRuitkStyleDict> Out = MakeShared<FRuitkStyleDict>();
		{
			FScopeLock Lock(&GClassLock);
			for (const FName& ClassName : Classes)
			{
				if (const FRuitkStyleDict* ClassDict = GStyleClasses.Find(ClassName))
				{
					for (const TPair<FName, FRuitkValue>& Pair : *ClassDict)
					{
						Out->Add(Pair.Key, Pair.Value);
					}
				}
				else
				{
					WarnUnknownKey(FName(*(TEXT("class:") + ClassName.ToString())));
				}
			}
		}
		if (InlineStyle.IsValid())
		{
			for (const TPair<FName, FRuitkValue>& Pair : *InlineStyle)
			{
				Out->Add(Pair.Key, Pair.Value); // inline wins
			}
		}
		ResolveThemeTokens(*Out);
		return Out->IsEmpty() ? TSharedPtr<FRuitkStyleDict>() : Out;
	}

	void ApplyStyleDiff(SWidget& Widget, IRuitkElementAdapter* Adapter, const FRuitkStyleDict* Old,
						const FRuitkStyleDict* New)
	{
		bool bTransformTouched = false;

		// Pass 1: keys in New — apply when new or changed.
		if (New != nullptr)
		{
			for (const TPair<FName, FRuitkValue>& Pair : *New)
			{
				const FRuitkValue* OldValue = Old != nullptr ? Old->Find(Pair.Key) : nullptr;
				if (OldValue != nullptr && *OldValue == Pair.Value)
				{
					continue;
				}
				if (IsTransformKey(Pair.Key))
				{
					bTransformTouched = true;
					continue;
				}
				if (Adapter != nullptr && Adapter->ApplyStyleKey(Widget, Pair.Key, &Pair.Value))
				{
					continue;
				}
				if (!ApplyGenericKey(Widget, Pair.Key, &Pair.Value))
				{
					WarnUnknownKey(Pair.Key);
				}
			}
		}

		// Pass 2: keys REMOVED (in Old, not in New) — reset to defaults (the family rule).
		if (Old != nullptr)
		{
			for (const TPair<FName, FRuitkValue>& Pair : *Old)
			{
				if (New != nullptr && New->Contains(Pair.Key))
				{
					continue;
				}
				if (IsTransformKey(Pair.Key))
				{
					bTransformTouched = true;
					continue;
				}
				if (Adapter != nullptr && Adapter->ApplyStyleKey(Widget, Pair.Key, nullptr))
				{
					continue;
				}
				ApplyGenericKey(Widget, Pair.Key, nullptr); // unknown keys already warned on apply
			}
		}

		if (bTransformTouched)
		{
			ApplyRenderTransform(Widget, New); // recompose from the NEW dict (absent -> identity)
		}
	}

	// ── TD-002: @theme tokens + @uss stylesheet loading ────────────────────────────────────

	void RegisterTheme(FName ThemeName, FRuitkStyleDict Tokens)
	{
		// Store token keys BARE: a reference strips its leading `$` before lookup (ResolveThemeTokens),
		// so a `$token:` declaration form must normalize to `token` or it can never resolve (bughunt
		// STYLE-1). Both `accent:` and `$accent:` are accepted and stored as `accent`.
		FRuitkStyleDict Normalized;
		Normalized.Reserve(Tokens.Num());
		for (const TPair<FName, FRuitkValue>& Token : Tokens)
		{
			const FString K = Token.Key.ToString();
			Normalized.Add(K.StartsWith(TEXT("$")) ? FName(*K.RightChop(1)) : Token.Key, Token.Value);
		}
		FScopeLock Lock(&GClassLock);
		GThemes.Add(ThemeName, MoveTemp(Normalized));
	}

	void SetActiveTheme(FName ThemeName)
	{
		FScopeLock Lock(&GClassLock);
		GActiveTheme = ThemeName;
	}

	FName GetActiveTheme()
	{
		FScopeLock Lock(&GClassLock);
		return GActiveTheme;
	}

	const FRuitkValue* ResolveThemeToken(FName TokenName)
	{
		FScopeLock Lock(&GClassLock);
		if (GActiveTheme == NAME_None)
		{
			return nullptr;
		}
		const FRuitkStyleDict* Theme = GThemes.Find(GActiveTheme);
		return Theme ? Theme->Find(TokenName) : nullptr;
	}

	FRuitkValue ParseStyleValue(const FString& Literal)
	{
		FString S = Literal;
		S.TrimStartAndEndInline();
		if (S.IsEmpty())
		{
			return FRuitkValue();
		}
		// Token reference: keep as a '$'-prefixed String (resolved at effective-style build time).
		if (S.StartsWith(TEXT("$")))
		{
			return FRuitkValue(S);
		}
		// Hex color: #rrggbb or #rrggbbaa.
		if (S.StartsWith(TEXT("#")))
		{
			return FRuitkValue(FLinearColor(FColor::FromHex(S)));
		}
		// Quoted string.
		if (S.Len() >= 2 && S.StartsWith(TEXT("\"")) && S.EndsWith(TEXT("\"")))
		{
			return FRuitkValue(S.Mid(1, S.Len() - 2));
		}
		if (S == TEXT("true"))
		{
			return FRuitkValue(true);
		}
		if (S == TEXT("false"))
		{
			return FRuitkValue(false);
		}
		// Vector2: "x,y" (two numbers). Trim each component so `"x, y"` (a space after the comma) still
		// parses as a Vector2 rather than falling through to a Name (bughunt STYLE-2).
		{
			TArray<FString> Parts;
			S.ParseIntoArray(Parts, TEXT(","), true);
			if (Parts.Num() == 2)
			{
				Parts[0].TrimStartAndEndInline();
				Parts[1].TrimStartAndEndInline();
				if (Parts[0].IsNumeric() && Parts[1].IsNumeric())
				{
					return FRuitkValue(FVector2D(FCString::Atod(*Parts[0]), FCString::Atod(*Parts[1])));
				}
			}
		}
		// Numbers: integer vs float.
		if (S.IsNumeric())
		{
			if (S.Contains(TEXT(".")))
			{
				return FRuitkValue(static_cast<double>(FCString::Atod(*S)));
			}
			return FRuitkValue(static_cast<int64>(FCString::Atoi64(*S)));
		}
		// Bare identifier -> Name (enum-ish values: visible, center, ...).
		return FRuitkValue(FName(*S));
	}

	/** Strip `/​* ... *​/` and `//` comments, QUOTE-AWARE: a `//` or `/​*` inside a `"..."` value is a
	 *  literal, not a comment (bughunt B5 — the old global Find-based strip truncated any value
	 *  containing `//`, e.g. `"img://cdn/x.png"`, silently eating the rest of the block). Newlines are
	 *  preserved so line structure survives. */
	FString StripComments(const FString& In)
	{
		FString Out;
		Out.Reserve(In.Len());
		const int32 N = In.Len();
		bool bInString = false;
		for (int32 i = 0; i < N;)
		{
			const TCHAR C = In[i];
			if (bInString)
			{
				if (C == TEXT('\\') && i + 1 < N) // keep an escaped char pair verbatim
				{
					Out.AppendChar(C);
					Out.AppendChar(In[i + 1]);
					i += 2;
					continue;
				}
				Out.AppendChar(C);
				if (C == TEXT('"'))
				{
					bInString = false;
				}
				++i;
				continue;
			}
			if (C == TEXT('"'))
			{
				bInString = true;
				Out.AppendChar(C);
				++i;
				continue;
			}
			if (C == TEXT('/') && i + 1 < N && In[i + 1] == TEXT('*'))
			{
				const int32 End = In.Find(TEXT("*/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, i + 2);
				i = (End == INDEX_NONE) ? N : End + 2;
				continue;
			}
			if (C == TEXT('/') && i + 1 < N && In[i + 1] == TEXT('/'))
			{
				const int32 NL = In.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, i + 2);
				i = (NL == INDEX_NONE) ? N : NL; // stop before the newline (kept next iteration)
				continue;
			}
			Out.AppendChar(C);
			++i;
		}
		return Out;
	}

	int32 LoadStylesheet(const FString& Source)
	{
		int32 Registered = 0;
		// Strip comments (quote-aware), then walk `header { body }` blocks.
		FString Src = StripComments(Source);

		int32 Cursor = 0;
		while (Cursor < Src.Len())
		{
			const int32 BraceOpen = Src.Find(TEXT("{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Cursor);
			if (BraceOpen == INDEX_NONE)
			{
				break;
			}
			const int32 BraceClose = Src.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, BraceOpen);
			if (BraceClose == INDEX_NONE)
			{
				break;
			}
			FString Header = Src.Mid(Cursor, BraceOpen - Cursor);
			Header.TrimStartAndEndInline();
			const FString Body = Src.Mid(BraceOpen + 1, BraceClose - BraceOpen - 1);
			Cursor = BraceClose + 1;

			// Parse the body into a dict (comments already stripped globally above).
			FRuitkStyleDict Dict;
			TArray<FString> Decls;
			Body.ParseIntoArray(Decls, TEXT(";"), true);
			for (const FString& Decl : Decls)
			{
				FString Key, Value;
				if (Decl.Split(TEXT(":"), &Key, &Value))
				{
					Key.TrimStartAndEndInline();
					if (!Key.IsEmpty())
					{
						Dict.Add(FName(*Key), ParseStyleValue(Value));
					}
				}
			}

			if (Header.StartsWith(TEXT("@theme")))
			{
				FString Name = Header.RightChop(6);
				Name.TrimStartAndEndInline();
				RegisterTheme(FName(*Name), MoveTemp(Dict));
				++Registered;
			}
			else if (Header.StartsWith(TEXT(".")))
			{
				RegisterStyleClass(FName(*Header.RightChop(1).TrimStartAndEnd()), MoveTemp(Dict));
				++Registered;
			}
			else if (!Header.IsEmpty())
			{
				// bare selector -> treat as a class name (loyal to inline `classes` names)
				RegisterStyleClass(FName(*Header), MoveTemp(Dict));
				++Registered;
			}
		}
		return Registered;
	}
} // namespace Ruitk::Slate
