// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuitkHostWidget.h"

#include "RuitkHostProps.h"
#include "RuitkNode.h"
#include "RuitkRoot.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

FRuitkNode URuitkHostWidget::BuildTree() const
{
	// The hosted component wrapped in the host-props provider (TD-028): the designer/BP-set
	// initial props + viewmodel arrive as context (UseHostProp / UseHostViewModel).
	FRuitkHostPropsState State;
	State.Props = InitialProps;
	State.ViewModel = ViewModel.GetObject();
	return Ruitk::Umg::HostPropsProvider(MoveTemp(State), {Ruitk::Named(ComponentName)});
}

TSharedRef<SWidget> URuitkHostWidget::BuildContent()
{
	if (IsDesignTime())
	{
		// The designer must never run live component code — placeholder only.
		return SNew(STextBlock)
			.Text(FText::Format(NSLOCTEXT("Ruitk", "HostDesignTime", "[Reactive UI Toolkit: {0}]"),
								FText::FromName(ComponentName.IsNone() ? FName(TEXT("<unset>")) : ComponentName)));
	}
	{
		FName Resolved;
		TArray<FName> Candidates;
		const Ruitk::EResolveNamed Verdict =
			ComponentName.IsNone() ? Ruitk::EResolveNamed::Miss : Ruitk::ResolveNamed(ComponentName, Resolved, &Candidates);
		if (Verdict == Ruitk::EResolveNamed::Ambiguous)
		{
			// FILE_SCOPED_EXPORTS (FS-05): several files export this short name — name the
			// qualified candidates ON the widget so the designer can paste one; never first-wins.
			FString List;
			for (const FName& C : Candidates)
			{
				List += (List.IsEmpty() ? TEXT("") : TEXT(", ")) + C.ToString();
			}
			return SNew(STextBlock)
				.Text(FText::Format(NSLOCTEXT("Ruitk", "HostAmbiguous",
											  "[Reactive UI Toolkit: '{0}' is ambiguous — use a qualified id: {1}]"),
									FText::FromName(ComponentName), FText::FromString(List)));
		}
		if (Verdict != Ruitk::EResolveNamed::Hit)
		{
			return SNew(STextBlock)
				.Text(FText::Format(
					NSLOCTEXT("Ruitk", "HostUnknown", "[Reactive UI Toolkit: '{0}' is not a registered component]"),
					FText::FromName(ComponentName)));
		}
	}
	Root = FRuitkRoot::Create(BuildTree());
	Root->FlushSync();
	return Root->GetWidget();
}

TSharedRef<SWidget> URuitkHostWidget::RebuildWidget()
{
	// The stable wrapper: UMG caches THIS widget; Remount swaps its content in place.
	Container = SNew(SBox)[BuildContent()];
	return Container.ToSharedRef();
}

void URuitkHostWidget::Remount()
{
	if (Root.IsValid())
	{
		Root->Unmount();
		Root.Reset();
	}
	if (Container.IsValid())
	{
		Container->SetContent(BuildContent()); // in place — the parent slot keeps the wrapper
	}
	InvalidateLayoutAndVolatility();
}

void URuitkHostWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	// Forward property edits into the live tree: re-handing the SAME component under a provider
	// with new state re-provides the context (provider props equality gates it) — consumers
	// re-render in place, hook state preserved. Never runs live code at design time.
	if (!IsDesignTime() && Root.IsValid())
	{
		Root->Update(BuildTree());
		Root->FlushSync();
	}
}

void URuitkHostWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	if (Root.IsValid())
	{
		Root->Unmount(); // cleanups run BEFORE the Slate tree is released (family order)
		Root.Reset();
	}
	Container.Reset();
}

#if WITH_EDITOR
const FText URuitkHostWidget::GetPaletteCategory()
{
	return NSLOCTEXT("Ruitk", "PaletteCategory", "Reactive UI Toolkit");
}
#endif
