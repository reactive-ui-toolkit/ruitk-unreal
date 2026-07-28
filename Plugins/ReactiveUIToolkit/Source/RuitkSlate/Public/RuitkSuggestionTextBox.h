// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-012 tail — SSuggestionTextBox, the text field with a live suggestion dropdown. The dropdown's
// PRESENTATION is a menu, but its BEHAVIOUR is a pure function: SSuggestionTextBox::OnShowingSuggestions
// asks "given what the user has typed, which suggestions apply?" — and that is exactly what
// SRuitkSuggestionTextBox computes from its `Suggestions` candidate list (case-insensitive substring
// match). Text is controlled (D-16 caret rule); OnTextChanged/OnTextCommitted carry the text. The
// suggestion filter is exposed (ComputeSuggestions) so the suite verifies the real behaviour headless.

#pragma once

#include "CoreMinimal.h"
#include "RuitkNode.h"
#include "RuitkPropsBase.h"
#include "Widgets/SCompoundWidget.h"

class SSuggestionTextBox;

/** SSuggestionTextBox (Leaf): controlled Text + a `Suggestions` candidate list the widget filters by
 *  the typed substring. OnTextChanged/OnTextCommitted carry the current text. */
struct RUITKSLATE_API FRuitkSuggestionTextBoxProps final : public FRuitkPropsBase
{
	RUITK_PROP(FText, Text, 0)
	RUITK_PROP(FText, HintText, 1)
	RUITK_PROP(TArray<FString>, Suggestions, 2)
	RUITK_PROP_EVENT(OnTextChanged, 3)
	RUITK_PROP_EVENT(OnTextCommitted, 4)

	virtual bool Equals(const FRuitkPropsBase& OtherBase) const override
	{
		const FRuitkSuggestionTextBoxProps& Other = static_cast<const FRuitkSuggestionTextBoxProps&>(OtherBase);
		auto TextEq = [](const FText& A, const FText& B) { return A.IdenticalTo(B) || A.ToString() == B.ToString(); };
		return SetBits == Other.SetBits && BaseFieldsEqual(Other) && TextEq(Text, Other.Text) &&
			   TextEq(HintText, Other.HintText) && Suggestions == Other.Suggestions &&
			   OnTextChanged == Other.OnTextChanged && OnTextCommitted == Other.OnTextCommitted;
	}
};

/** Wraps SSuggestionTextBox with the controlled text + the substring suggestion filter. */
class RUITKSLATE_API SRuitkSuggestionTextBox final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRuitkSuggestionTextBox) {}
	SLATE_ARGUMENT(FText, HintText)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetText(const FText& InText);
	FText GetText() const;
	void SetSuggestionsList(TArray<FString> InSuggestions) { Suggestions = MoveTemp(InSuggestions); }
	void SetOnTextChanged(FRuitkCallback InCb) { OnTextChangedCb = MoveTemp(InCb); }
	void SetOnTextCommitted(FRuitkCallback InCb) { OnTextCommittedCb = MoveTemp(InCb); }

	/** The suggestions that apply to `Input` — case-insensitive substring match; empty input -> none.
	 *  This is exactly what the widget's OnShowingSuggestions handler returns. */
	TArray<FString> ComputeSuggestions(const FString& Input) const;

private:
	void HandleShowingSuggestions(const FString& Input, TArray<FString>& OutSuggestions);
	void HandleTextChanged(const FText& InText);
	void HandleTextCommitted(const FText& InText, ETextCommit::Type CommitType);

	TArray<FString> Suggestions;
	FRuitkCallback OnTextChangedCb;
	FRuitkCallback OnTextCommittedCb;
	TSharedPtr<SSuggestionTextBox> Box;
};

namespace Ruitk::Slate
{
	RUITKSLATE_API FRuitkElementTypeId SuggestionTextBoxType();

	/** A text field with a substring-matched suggestion dropdown. */
	RUITKSLATE_API FRuitkNode SuggestionTextBox(FRuitkSuggestionTextBoxProps Props = FRuitkSuggestionTextBoxProps(),
												FRuitkKey Key = FRuitkKey());

	namespace Detail
	{
		void RegisterSuggestionTextBoxAdapter();
	}
} // namespace Ruitk::Slate
