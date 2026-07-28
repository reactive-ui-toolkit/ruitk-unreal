// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "SRuitkCanvas.h"

void SRuitkCanvas::Construct(const FArguments&)
{
}

void SRuitkCanvas::SetDrawFn(TSharedPtr<FRuitkDrawFn> InDrawFn)
{
	if (DrawFn != InDrawFn) // identity — the family's callback-identity repaint rule
	{
		DrawFn = MoveTemp(InDrawFn);
		Invalidate(EInvalidateWidgetReason::Paint);
	}
}

void SRuitkCanvas::SetRedrawKey(int64 InKey)
{
	if (RedrawKey != InKey)
	{
		RedrawKey = InKey;
		Invalidate(EInvalidateWidgetReason::Paint);
	}
}

void SRuitkCanvas::SetCanvasDesiredSize(FVector2D InSize)
{
	if (CanvasDesiredSize != InSize)
	{
		CanvasDesiredSize = InSize;
		Invalidate(EInvalidateWidgetReason::Layout);
	}
}

int32 SRuitkCanvas::OnPaint(const FPaintArgs&, const FGeometry& AllottedGeometry, const FSlateRect&,
						  FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle&, bool) const
{
	if (DrawFn.IsValid() && *DrawFn)
	{
		return (*DrawFn)(AllottedGeometry, OutDrawElements, LayerId);
	}
	return LayerId;
}

FVector2D SRuitkCanvas::ComputeDesiredSize(float) const
{
	return CanvasDesiredSize;
}
