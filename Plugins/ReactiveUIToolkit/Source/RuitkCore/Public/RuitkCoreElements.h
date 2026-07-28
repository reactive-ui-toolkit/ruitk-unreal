// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// The two element/component pieces the CORE itself must know:
//   Text     — the auto-wrap target for raw string children (family: String -> V.text);
//              hosts register their adapter under the same interned tag ("Text").
//   Suspense — the family's declarative boundary polyfill (NO throw-to-suspend — D-10's
//              no-exceptions reality): a plain function component over the hooks, driven by
//              an IsReady poll (re-armed per frame via the host's RequestFrame).

#pragma once

#include "CoreMinimal.h"
#include "RuitkNode.h"
#include "RuitkContext.h"

/** Text element props ("Text" tag; hosts map to STextBlock / mock text). */
struct FRuitkTextBlockProps final : public FRuitkPropsBase
{
	RUITK_PROP(FText, Text, 0)

	// Hand-written (FText has no operator== — identity first, then display-string compare).
	virtual bool Equals(const FRuitkPropsBase& Other) const override
	{
		const FRuitkTextBlockProps* Typed = static_cast<const FRuitkTextBlockProps*>(&Other);
		if (!BaseFieldsEqual(Other))
		{
			return false;
		}
		return Text.IdenticalTo(Typed->Text) || Text.ToString() == Typed->Text.ToString();
	}
};

namespace Ruitk
{
	/** The interned "Text" element id (stable across the process). */
	RUITKCORE_API FRuitkElementTypeId TextBlockElementType();

	/** Text node factory (also what raw-string children auto-wrap into). */
	RUITKCORE_API FRuitkNode TextBlock(FText InText, FRuitkKey Key = FRuitkKey());
	RUITKCORE_API FRuitkNode TextBlock(const FString& InText, FRuitkKey Key = FRuitkKey());

	// ─────────────────────────────────────────────────────────────────────────────────────
	// Fmt — clean FText interpolation for .uetkx bindings. `Ruitk::Fmt(TEXT("Count: {}"), Count)`
	// fills each `{}` with the next argument (type-generic, ordered), so a binding reads as
	//   Text={ Fmt(TEXT("Count: {}"), Count) }
	// instead of the FText::FromString(FString::Printf(TEXT("Count: %d"), Count)) nesting — and
	// there is no %-specifier to get wrong. Literal braces are written `{{` / `}}`.
	// ─────────────────────────────────────────────────────────────────────────────────────
	namespace Detail
	{
		inline FString FmtArg(const FString& S)
		{
			return S;
		}
		inline FString FmtArg(const TCHAR* S)
		{
			return FString(S);
		}
		inline FString FmtArg(const FText& T)
		{
			return T.ToString();
		}
		inline FString FmtArg(FName N)
		{
			return N.ToString();
		}
		inline FString FmtArg(int32 V)
		{
			return FString::FromInt(V);
		}
		inline FString FmtArg(int64 V)
		{
			return FString::Printf(TEXT("%lld"), V);
		}
		inline FString FmtArg(float V)
		{
			return FString::SanitizeFloat(V);
		}
		inline FString FmtArg(double V)
		{
			return FString::SanitizeFloat(V);
		}
		inline FString FmtArg(bool V)
		{
			return V ? FString(TEXT("true")) : FString(TEXT("false"));
		}
	} // namespace Detail

	template <typename... TArgs> FText Fmt(const TCHAR* Template, TArgs&&... Args)
	{
		// Parts[0] is a sentinel so a no-arg Fmt (or a template with no `{}`) still forms a valid array.
		const FString Parts[] = {FString(), Detail::FmtArg(Args)...};
		FString Out;
		int32 Next = 1;
		for (const TCHAR* P = Template; *P != TEXT('\0'); ++P)
		{
			if (P[0] == TEXT('{') && P[1] == TEXT('{')) // escaped '{'
			{
				Out.AppendChar(TEXT('{'));
				++P;
			}
			else if (P[0] == TEXT('}') && P[1] == TEXT('}')) // escaped '}'
			{
				Out.AppendChar(TEXT('}'));
				++P;
			}
			else if (P[0] == TEXT('{') && P[1] == TEXT('}')) // placeholder
			{
				if (Next < UE_ARRAY_COUNT(Parts))
				{
					Out += Parts[Next++];
				}
				++P;
			}
			else
			{
				Out.AppendChar(*P);
			}
		}
		return FText::FromString(MoveTemp(Out));
	}

	/** Suspense props. */
	struct FRuitkSuspenseProps final : public FRuitkPropsBase
	{
		/** Shown while not ready (empty node = render nothing). */
		TSharedPtr<FRuitkNode> Fallback;
		/** Polled once immediately, then per frame until true. */
		TFunction<bool()> IsReady;

		virtual bool Equals(const FRuitkPropsBase& Other) const override
		{
			// Function fields are identity-less; Fallback is a node — Suspense re-renders
			// when its parent does (never bails on props), which matches the family's
			// function-component polyfill behavior.
			return false;
		}
	};

	/** The Suspense component function (registered; use via Ruitk::Suspense below). */
	RUITKCORE_API FRuitkNodeArray SuspenseComponent(FRuitkContext& Ctx, const FRuitkSuspenseProps& Props,
													   const TArray<FRuitkNode>& Children);

	/** Declarative boundary: fallback until IsReady() flips true, then the children. */
	RUITKCORE_API FRuitkNode Suspense(TFunction<bool()> IsReady, FRuitkNode Fallback, TArray<FRuitkNode> Children,
										 FRuitkKey Key = FRuitkKey());
} // namespace Ruitk
