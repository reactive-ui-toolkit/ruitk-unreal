// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "DemoInteropWidgets.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "RuitkDemoSupport.h"
#include "RuitkHostWidget.h"
#include "RuitkSignalViewModel.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

// ── UDemoHostUserWidget (G1: ours inside theirs) ─────────────────────────────────────────────

TSharedRef<SWidget> UDemoHostUserWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();

		UTextBlock* Caption = WidgetTree->ConstructWidget<UTextBlock>();
		Caption->SetText(NSLOCTEXT("RuitkDemo", "UmgHostCaption",
								   "▲ A hand-built UMG UserWidget. The panel below it is a URuitkHostWidget —"
								   " OUR component, mounted INSIDE this UMG tree by name:"));
		Caption->SetAutoWrapText(true);
		Root->AddChildToVerticalBox(Caption);

		URuitkHostWidget* Host = WidgetTree->ConstructWidget<URuitkHostWidget>();
		Host->ComponentName = FName(TEXT("UmgHostInner"));
		if (UVerticalBoxSlot* HostSlot = Root->AddChildToVerticalBox(Host))
		{
			HostSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
		}

		WidgetTree->RootWidget = Root;
	}
	return Super::RebuildWidget();
}

// ── UDemoVmBoundWidget (G3: their view bound to our viewmodel) ───────────────────────────────

TSharedRef<SWidget> UDemoVmBoundWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		ValueText = WidgetTree->ConstructWidget<UTextBlock>();
		WidgetTree->RootWidget = ValueText;
	}
	return Super::RebuildWidget();
}

void UDemoVmBoundWidget::NativeConstruct()
{
	Super::NativeConstruct();
	URuitkSignalViewModel* Vm = RuitkDemo::GetSharedVm();
	// Their side of the bridge: a standard FieldNotify subscription on OUR viewmodel — the
	// same interface any Epic-MVVM binding uses; no ReactiveUI API in sight.
	VmHandle = Vm->AddFieldValueChangedDelegate(URuitkSignalViewModel::FFieldNotificationClassDescriptor::Int,
												INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(
													this, &UDemoVmBoundWidget::OnVmFieldChanged));
	RefreshText();
}

void UDemoVmBoundWidget::NativeDestruct()
{
	if (VmHandle.IsValid())
	{
		RuitkDemo::GetSharedVm()->RemoveFieldValueChangedDelegate(
			URuitkSignalViewModel::FFieldNotificationClassDescriptor::Int, VmHandle);
		VmHandle.Reset();
	}
	Super::NativeDestruct();
}

void UDemoVmBoundWidget::OnVmFieldChanged(UObject* /*Object*/, UE::FieldNotification::FFieldId /*FieldId*/)
{
	RefreshText();
}

void UDemoVmBoundWidget::RefreshText()
{
	if (ValueText)
	{
		ValueText->SetText(FText::Format(NSLOCTEXT("RuitkDemo", "VmBound", "▲ UMG TextBlock via FieldNotify: {0}"),
										 FText::AsNumber(RuitkDemo::GetSharedVm()->Int)));
	}
}

// ── UDemoActivatableScreen + UDemoStackHostWidget (G2: the real CommonUI stack) ──────────────

UDemoActivatableScreen::UDemoActivatableScreen(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// The hosted component: the same ActivationProbe the stand-in demo uses — here it reads
	// REAL activation published by this screen's NativeOnActivated/Deactivated.
	ComponentName = FName(TEXT("ActivationProbe"));
}

TSharedRef<SWidget> UDemoStackHostWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();

		UTextBlock* Caption = WidgetTree->ConstructWidget<UTextBlock>();
		Caption->SetText(NSLOCTEXT("RuitkDemo", "StackCaption",
								   "▲ A REAL UCommonActivatableWidgetStack. The screen on it is a"
								   " URuitkActivatableScreen hosting the ActivationProbe component:"));
		Caption->SetAutoWrapText(true);
		Root->AddChildToVerticalBox(Caption);

		Stack = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>();
		if (UVerticalBoxSlot* StackSlot = Root->AddChildToVerticalBox(Stack))
		{
			StackSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 6.0f));
		}

		UButton* Toggle = WidgetTree->ConstructWidget<UButton>();
		ToggleLabel = WidgetTree->ConstructWidget<UTextBlock>();
		ToggleLabel->SetText(NSLOCTEXT("RuitkDemo", "StackPop", "Pop the screen (deactivates it)"));
		Toggle->AddChild(ToggleLabel);
		Toggle->OnClicked.AddDynamic(this, &UDemoStackHostWidget::OnToggleClicked);
		Root->AddChildToVerticalBox(Toggle);

		WidgetTree->RootWidget = Root;
	}
	return Super::RebuildWidget();
}

void UDemoStackHostWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (Stack && !ActiveScreen)
	{
		// The promise, literally: OUR screen pushed onto THEIR stack. CommonUI activates it.
		ActiveScreen = Stack->AddWidget<UDemoActivatableScreen>(UDemoActivatableScreen::StaticClass());
	}
}

void UDemoStackHostWidget::OnToggleClicked()
{
	if (!Stack)
	{
		return;
	}
	if (ActiveScreen)
	{
		Stack->RemoveWidget(*ActiveScreen);
		ActiveScreen = nullptr;
		if (ToggleLabel)
		{
			ToggleLabel->SetText(NSLOCTEXT("RuitkDemo", "StackPush", "Push a screen (activates it)"));
		}
	}
	else
	{
		ActiveScreen = Stack->AddWidget<UDemoActivatableScreen>(UDemoActivatableScreen::StaticClass());
		if (ToggleLabel)
		{
			ToggleLabel->SetText(NSLOCTEXT("RuitkDemo", "StackPop", "Pop the screen (deactivates it)"));
		}
	}
}
