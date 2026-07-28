// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuitkDemoScreens.h"

#include "RuitkContext.h"
#include "RuitkDemoShared.h"
#include "RuitkStyle.h"

using namespace RuitkDemo;

namespace RuitkDemo
{
	// Every screen is a COMPILED .uetkx component (Screens/*.uetkx -> committed .uetkx.inl,
	// built through RuitkDemo.Uetkx.gen.cpp). The generated code self-registers named factories;
	// entries resolve through Ruitk::Named — the wrappers themselves are TU-local by design.
	const TArray<FRuitkDemoEntry>& GetGalleryEntries()
	{
		static const TArray<FRuitkDemoEntry> Entries = {
			{TEXT("Hello World"), +[]() { return Ruitk::Named(FName(TEXT("HelloWorld"))); }},
			{TEXT("Counter"), +[]() { return Ruitk::Named(FName(TEXT("SimpleCounter"))); }},
			{TEXT("Click Counter"), +[]() { return Ruitk::Named(FName(TEXT("ClickCounter"))); }},
			{TEXT("Text Field"), +[]() { return Ruitk::Named(FName(TEXT("SimpleTextField"))); }},
			{TEXT("Use Effect"), +[]() { return Ruitk::Named(FName(TEXT("SimpleUseEffect"))); }},
			{TEXT("Signals"), +[]() { return Ruitk::Named(FName(TEXT("SignalCounter"))); }},
			{TEXT("Context"), +[]() { return Ruitk::Named(FName(TEXT("ContextDemo"))); }},
			{TEXT("Keyed Diff"), +[]() { return Ruitk::Named(FName(TEXT("KeyedDiff"))); }},
			{TEXT("Styled Panels"), +[]() { return Ruitk::Named(FName(TEXT("StyledPanels"))); }},
			{TEXT("Tic Tac Toe"), +[]() { return Ruitk::Named(FName(TEXT("TicTacToe"))); }},
			{TEXT("Custom Draw"), +[]() { return Ruitk::Named(FName(TEXT("CustomDraw"))); }},
			{TEXT("Stress Test"), +[]() { return Ruitk::Named(FName(TEXT("StressTest"))); }},
			{TEXT("Router"), +[]() { return Ruitk::Named(FName(TEXT("RouterDemo"))); }},
			{TEXT("Doom"), +[]() { return Ruitk::Named(FName(TEXT("DoomGame"))); }},
			{TEXT("Acceptance Lab"), +[]() { return Ruitk::Named(FName(TEXT("AcceptanceLab"))); }},
			// Epic-interop pillars (compiled .uetkx like everything else).
			{TEXT("MVVM (data feed)"), +[]() { return Ruitk::Named(FName(TEXT("MvvmDemo"))); }},
			{TEXT("CommonUI (activation)"), +[]() { return Ruitk::Named(FName(TEXT("CommonUiDemo"))); }},
			{TEXT("UMG Host & Reverse MVVM"), +[]() { return Ruitk::Named(FName(TEXT("UmgHostDemo"))); }},
			{TEXT("Interop — all 4 pillars"), +[]() { return Ruitk::Named(FName(TEXT("InteropShowcase"))); }},
		};
		return Entries;
	}

	const TArray<FName>& GetCompiledScreenNames()
	{
		static const TArray<FName> Names = {
			FName(TEXT("HelloWorld")),		FName(TEXT("SimpleCounter")),	FName(TEXT("ClickCounter")),
			FName(TEXT("SimpleTextField")), FName(TEXT("SimpleUseEffect")), FName(TEXT("SignalCounter")),
			FName(TEXT("ContextDemo")),		FName(TEXT("KeyedDiff")),		FName(TEXT("StyledPanels")),
			FName(TEXT("TicTacToe")),		FName(TEXT("CustomDraw")),		FName(TEXT("StressTest")),
			FName(TEXT("RouterDemo")),		FName(TEXT("AcceptanceLab")),	FName(TEXT("MvvmDemo")),
			FName(TEXT("CommonUiDemo")),	FName(TEXT("UmgHostDemo")),		FName(TEXT("InteropShowcase")),
			FName(TEXT("DoomGame")),
		};
		return Names;
	}
} // namespace RuitkDemo

// ── the shell: menu column + selected screen (switching remounts — cleanups exercised) ────

static FRuitkNodeArray GalleryShellComp(FRuitkContext& Ctx, const FRuitkEmptyProps&, const TArray<FRuitkNode>&)
{
	auto [Selected, SetSelected] = Ctx.UseState<int32>(0);
	TFunction<void(int32)> Select = SetSelected;
	const int32 SelectedNow = Selected;

	Ctx.UseEffect(
		[]()
		{
			FRuitkStyleDict Dim;
			Dim.Add(FName(TEXT("RenderOpacity")), FRuitkValue(0.35f));
			Ruitk::Slate::RegisterStyleClass(FName(TEXT("rui-demo-dim")), MoveTemp(Dim));
		},
		Ruitk::Deps());

	const TArray<FRuitkDemoEntry>& Entries = GetGalleryEntries();

	TArray<FRuitkNode> MenuRows;
	MenuRows.Add(StyledText(TEXT("Examples"), 14.0f, FLinearColor(0.7f, 0.7f, 0.8f)));
	MenuRows.Add(Gap(4.0f));
	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		FRuitkButtonProps P;
		P.SetOnClicked(FRuitkCallback::Create([Select, i]() { Select(i); }));
		P.SetContentPadding(FMargin(10.0f, 3.0f));
		FRuitkNode Row = Ruitk::Slate::Button(
			MoveTemp(P), {Ruitk::TextBlock((i == SelectedNow ? TEXT("> ") : TEXT("  ")) + Entries[i].Name)});
		Row.Key = FRuitkKey(i);
		FRuitkNode Spaced = WithSlot(MoveTemp(Row), FName(TEXT("Slot.Padding")), FRuitkValue(TEXT("0,0,0,3")));
		MenuRows.Add(WithSlot(MoveTemp(Spaced), FName(TEXT("Slot.HAlign")), FRuitkValue(FName(TEXT("fill")))));
	}

	FRuitkBorderProps MenuCard;
	MenuCard.SetPadding(FMargin(8.0f));
	MenuCard.SetBorderImage(FName(TEXT("WhiteBrush")));
	MenuCard.SetBorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.85f));

	FRuitkBoxProps MenuWidth;
	MenuWidth.SetWidthOverride(180.0f);
	MenuWidth.SetVAlign(FName(TEXT("top")));

	// Key the content by index: switching demos fully unmounts the old screen (cleanups run).
	// The content slot FILLS the remaining viewport (Slot.Fill both axes) — card-style demo
	// screens are top-left content and look the same, but screens that scale to their area
	// (Doom's ScaleBox letterbox) get the real estate they need to reach the window bottom.
	FRuitkNode Content = Entries[SelectedNow].Make();
	Content.Key = FRuitkKey(1000 + SelectedNow);
	FRuitkNode ContentSlot = WithSlot(MoveTemp(Content), FName(TEXT("Slot.Padding")), FRuitkValue(TEXT("10,0,0,0")));
	ContentSlot = WithSlot(MoveTemp(ContentSlot), FName(TEXT("Slot.Fill")), FRuitkValue(1.0f));

	FRuitkNode BodyRow = Ruitk::Slate::HorizontalBox(
		FRuitkHorizontalBoxProps(),
		{Ruitk::Slate::Box(
			 MoveTemp(MenuWidth),
			 {Ruitk::Slate::Border(MoveTemp(MenuCard),
								   {Ruitk::Slate::VerticalBox(FRuitkVerticalBoxProps(), MoveTemp(MenuRows))})}),
		 MoveTemp(ContentSlot)});
	BodyRow = WithSlot(MoveTemp(BodyRow), FName(TEXT("Slot.Fill")), FRuitkValue(1.0f));

	return {Ruitk::Slate::Box(
		FRuitkBoxProps(),
		{Ruitk::Slate::VerticalBox(FRuitkVerticalBoxProps(),
								   {StyledText(TEXT("Reactive UI Toolkit for Unreal — example gallery"), 20.0f),
									Gap(4.0f), MoveTemp(BodyRow)})})};
}
RUITK_COMPONENT(GalleryShellComp)

namespace RuitkDemo
{
	FRuitkNode GalleryRoot()
	{
		return Ruitk::FC(&GalleryShellComp);
	}
} // namespace RuitkDemo
