// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-012 tail — SSuggestionTextBox wrapper. OnShowingSuggestions delegates to ComputeSuggestions
// (case-insensitive substring match over the candidate list); Text is controlled (D-16 caret rule).

#include "RuitkSuggestionTextBox.h"

#include "RuitkElementAdapter.h"
#include "Widgets/Input/SSuggestionTextBox.h"

void SRuitkSuggestionTextBox::Construct(const FArguments& InArgs)
{
	// clang-format off
	ChildSlot
	[
		SAssignNew(Box, SSuggestionTextBox)
		.HintText(InArgs._HintText)
		.OnShowingSuggestions(this, &SRuitkSuggestionTextBox::HandleShowingSuggestions)
		.OnTextChanged(this, &SRuitkSuggestionTextBox::HandleTextChanged)
		.OnTextCommitted(this, &SRuitkSuggestionTextBox::HandleTextCommitted)
	];
	// clang-format on
}

void SRuitkSuggestionTextBox::SetText(const FText& InText)
{
	// D-16 caret rule: compare against the widget's LIVE text so typing survives the round-trip.
	if (Box.IsValid() && !Box->GetText().EqualTo(InText))
	{
		Box->SetText(InText);
	}
}

FText SRuitkSuggestionTextBox::GetText() const
{
	return Box.IsValid() ? Box->GetText() : FText::GetEmpty();
}

TArray<FString> SRuitkSuggestionTextBox::ComputeSuggestions(const FString& Input) const
{
	TArray<FString> Out;
	if (Input.IsEmpty())
	{
		return Out; // nothing typed -> no suggestions
	}
	for (const FString& Candidate : Suggestions)
	{
		if (Candidate.Contains(Input, ESearchCase::IgnoreCase))
		{
			Out.Add(Candidate);
		}
	}
	return Out;
}

void SRuitkSuggestionTextBox::HandleShowingSuggestions(const FString& Input, TArray<FString>& OutSuggestions)
{
	OutSuggestions = ComputeSuggestions(Input);
}

void SRuitkSuggestionTextBox::HandleTextChanged(const FText& InText)
{
	if (OnTextChangedCb.IsBound())
	{
		OnTextChangedCb.Execute(FRuitkValue(InText));
	}
}

void SRuitkSuggestionTextBox::HandleTextCommitted(const FText& InText, ETextCommit::Type)
{
	if (OnTextCommittedCb.IsBound())
	{
		OnTextCommittedCb.Execute(FRuitkValue(InText));
	}
}

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Adapter (Leaf)
// ─────────────────────────────────────────────────────────────────────────────────────────────

class FRuitkSuggestionTextBoxAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::Leaf; }
	virtual bool IsPoolable() const override { return false; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase& Props, const TSharedPtr<FRuitkEventProxy>&) override
	{
		const FRuitkSuggestionTextBoxProps& P = static_cast<const FRuitkSuggestionTextBoxProps&>(Props);
		return SNew(SRuitkSuggestionTextBox).HintText(P.HasHintText() ? P.HintText : FText::GetEmpty());
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SRuitkSuggestionTextBox& W = static_cast<SRuitkSuggestionTextBox&>(Widget);
		const FRuitkSuggestionTextBoxProps& N = static_cast<const FRuitkSuggestionTextBoxProps&>(New);
		const FRuitkSuggestionTextBoxProps* O = static_cast<const FRuitkSuggestionTextBoxProps*>(Old);
		if (N.HasSuggestions() && (O == nullptr || !O->HasSuggestions() || !(N.Suggestions == O->Suggestions)))
		{
			W.SetSuggestionsList(N.Suggestions);
		}
		if (N.HasText())
		{
			W.SetText(N.Text); // controlled (skip-when-equal against live text inside)
		}
		if (N.HasOnTextChanged())
		{
			W.SetOnTextChanged(N.OnTextChanged);
		}
		if (N.HasOnTextCommitted())
		{
			W.SetOnTextCommitted(N.OnTextCommitted);
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Type, factory, registration
// ─────────────────────────────────────────────────────────────────────────────────────────────

namespace Ruitk::Slate
{
	FRuitkElementTypeId SuggestionTextBoxType()
	{
		return Ruitk::InternElementType(FName(TEXT("SuggestionTextBox")));
	}

	FRuitkNode SuggestionTextBox(FRuitkSuggestionTextBoxProps Props, FRuitkKey Key)
	{
		FRuitkNode Node;
		Node.Kind = ERuitkNodeKind::Host;
		Node.ElementType = SuggestionTextBoxType();
		Node.Props = MakeShared<FRuitkSuggestionTextBoxProps>(MoveTemp(Props));
		Node.Key = Key;
		return Node;
	}

	namespace Detail
	{
		void RegisterSuggestionTextBoxAdapter()
		{
			RegisterAdapter(SuggestionTextBoxType(), MakeUnique<FRuitkSuggestionTextBoxAdapter>());
		}
	} // namespace Detail
} // namespace Ruitk::Slate
