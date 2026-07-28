// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Shared building blocks for the demo screens (C++ builder sugar until Phase 3 markup).

#pragma once

#include "CoreMinimal.h"
#include "RuitkCoreElements.h"
#include "RuitkNode.h"
#include "RuitkSlateElements.h"

namespace RuitkDemo
{
	/** Section card: dark translucent backdrop + inner padding + a gap above (Slot.*). */
	inline FRuitkNode Padded(FRuitkNode Inner, float Padding = 8.0f)
	{
		FRuitkBorderProps P;
		P.SetPadding(FMargin(Padding + 4.0f));
		P.SetBorderImage(FName(TEXT("WhiteBrush"))); // solid fill (the engine default brush is a thin frame)
		P.SetBorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.85f));
		FRuitkNode Node = Ruitk::Slate::Border(MoveTemp(P), {MoveTemp(Inner)});
		TSharedRef<FRuitkBorderProps> Props =
			MakeShared<FRuitkBorderProps>(static_cast<const FRuitkBorderProps&>(*Node.Props));
		Props->SlotProps = MakeShared<FRuitkStyleDict>();
		Props->SlotProps->Add(FName(TEXT("Slot.Padding")), FRuitkValue(TEXT("0,10,0,0")));
		Node.Props = Props;
		return Node;
	}

	/** Breathing room between rows inside a section. */
	inline FRuitkNode Gap(float Height = 6.0f)
	{
		FRuitkSpacerProps P;
		P.SetSize(FVector2D(1.0f, Height));
		return Ruitk::Slate::Spacer(MoveTemp(P));
	}

	inline FRuitkNode StyledText(const FString& S, float FontSize, FLinearColor Color = FLinearColor::White)
	{
		FRuitkNode Node = Ruitk::TextBlock(S);
		TSharedRef<FRuitkTextBlockProps> Props =
			MakeShared<FRuitkTextBlockProps>(static_cast<const FRuitkTextBlockProps&>(*Node.Props));
		Props->Style = MakeShared<FRuitkStyleDict>();
		Props->Style->Add(FName(TEXT("Font.Size")), FRuitkValue(FontSize));
		Props->Style->Add(FName(TEXT("ColorAndOpacity")), FRuitkValue(Color));
		Node.Props = Props;
		return Node;
	}

	inline FRuitkNode LabeledButton(const FString& Label, TFunction<void()> OnClick)
	{
		FRuitkButtonProps P;
		P.SetOnClicked(FRuitkCallback::Create(MoveTemp(OnClick)));
		P.SetContentPadding(FMargin(12.0f, 4.0f));
		FRuitkNode Node = Ruitk::Slate::Button(MoveTemp(P), {Ruitk::TextBlock(Label)});
		TSharedRef<FRuitkButtonProps> Props =
			MakeShared<FRuitkButtonProps>(static_cast<const FRuitkButtonProps&>(*Node.Props));
		Props->SlotProps = MakeShared<FRuitkStyleDict>();
		Props->SlotProps->Add(FName(TEXT("Slot.Padding")), FRuitkValue(TEXT("0,0,6,0")));
		Node.Props = Props;
		return Node;
	}

	/** Attach Slot.* props to any node (per-node copy; demo authoring sugar). */
	inline FRuitkNode WithSlot(FRuitkNode Node, const FName Key, FRuitkValue Value)
	{
		if (!Node.Props.IsValid())
		{
			// A props-less node (e.g. the empty Fragment Ruitk::Named returns on a miss) cannot
			// carry slot props — pass it through instead of asserting (TB-21: this assert was
			// the visible face of a split registry; the node renders nothing either way).
			return Node;
		}
		TSharedRef<FRuitkPropsBase> Props = ConstCastSharedRef<FRuitkPropsBase>(Node.Props.ToSharedRef());
		if (!Props->SlotProps.IsValid())
		{
			Props->SlotProps = MakeShared<FRuitkStyleDict>();
		}
		Props->SlotProps->Add(Key, MoveTemp(Value));
		return Node;
	}

} // namespace RuitkDemo
