// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-004 (input APIs, drag-and-drop half) — typed DnD over Slate's FDragDropOperation. Slate DnD is
// driven by widget event OVERRIDES (OnDragDetected / OnDrop / OnDragEnter…), not by props or ref
// capture, so the API is a pair of wrapper ELEMENTS rather than a hook: wrap the draggable content
// in `Ruitk::Slate::DragSource` (it carries an FRuitkValue payload + a type tag) and the drop zone in
// `Ruitk::Slate::DropTarget` (it filters by accepted type tags and fires OnDrop with the payload).
//
// C++-FIRST (like ListView / MakeDrawFn): the drop handler is a closure and the payload is an
// FRuitkValue, neither cleanly markup-expressible (nor is an array-of-accept-types attribute), so
// there is no `.uetkx` tag. The events are plain FRuitkCallbacks routed straight to the overrides —
// no event proxy (Slate DnD is not a delegate surface). The concrete widgets + operation are
// exported so tests/tools can drive a synthetic drop (the overrides are directly callable) without
// a live Slate drag loop.

#pragma once

#include "CoreMinimal.h"
#include "Input/DragAndDrop.h"
#include "Input/Events.h"
#include "RuitkNode.h"
#include "RuitkPropsBase.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SNullWidget.h"

// ── the operation ─────────────────────────────────────────────────────────────────────────────

/** The drag operation carrying our payload + type tag across a drag. */
class RUITKSLATE_API FRuitkDragDropOp final : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FRuitkDragDropOp, FDragDropOperation)

	FName DragType;
	FRuitkValue Payload;
	TFunction<void(bool)> OnEnded; // fired once when the drag concludes (arg = was the drop handled)

	virtual void OnDrop(bool bDropWasHandled, const FPointerEvent& MouseEvent) override
	{
		if (OnEnded)
		{
			OnEnded(bDropWasHandled);
			OnEnded = nullptr; // fire exactly once
		}
		FDragDropOperation::OnDrop(bDropWasHandled, MouseEvent);
	}

	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override; // simple type-label (defined in .cpp)

	static TSharedRef<FRuitkDragDropOp> New(FName InType, FRuitkValue InPayload, TFunction<void(bool)> InOnEnded = nullptr)
	{
		TSharedRef<FRuitkDragDropOp> Op = MakeShared<FRuitkDragDropOp>();
		Op->DragType = InType;
		Op->Payload = MoveTemp(InPayload);
		Op->OnEnded = MoveTemp(InOnEnded);
		Op->Construct();
		return Op;
	}
};

// ── the draggable source ──────────────────────────────────────────────────────────────────────

class RUITKSLATE_API SRuitkDragSource final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRuitkDragSource) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&) { ChildSlot[SNullWidget::NullWidget]; }

	void SetContent(const TSharedPtr<SWidget>& Content)
	{
		ChildSlot[Content.IsValid() ? Content.ToSharedRef() : SNullWidget::NullWidget];
	}

	void SetDragType(FName InType) { DragType = InType; }
	void SetPayload(FRuitkValue InPayload) { Payload = MoveTemp(InPayload); }
	void SetOnDragStart(FRuitkCallback InCb) { OnDragStart = MoveTemp(InCb); }
	void SetOnDragEnd(FRuitkCallback InCb) { OnDragEnd = MoveTemp(InCb); }

	virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent& Event) override
	{
		if (Event.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
		}
		return FReply::Unhandled();
	}

	virtual FReply OnDragDetected(const FGeometry&, const FPointerEvent&) override
	{
		const FRuitkCallback EndCb = OnDragEnd;
		TSharedRef<FRuitkDragDropOp> Op = FRuitkDragDropOp::New(DragType, Payload,
															[EndCb](bool bHandled)
															{
																if (EndCb.IsBound())
																{
																	EndCb.Execute(FRuitkValue(bHandled));
																}
															});
		if (OnDragStart.IsBound())
		{
			OnDragStart.Execute(Payload);
		}
		return FReply::Handled().BeginDragDrop(Op);
	}

private:
	FName DragType;
	FRuitkValue Payload;
	FRuitkCallback OnDragStart;
	FRuitkCallback OnDragEnd;
};

// ── the drop zone ─────────────────────────────────────────────────────────────────────────────

class RUITKSLATE_API SRuitkDropTarget final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRuitkDropTarget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&) { ChildSlot[SNullWidget::NullWidget]; }

	void SetContent(const TSharedPtr<SWidget>& Content)
	{
		ChildSlot[Content.IsValid() ? Content.ToSharedRef() : SNullWidget::NullWidget];
	}

	void SetAcceptTypes(TArray<FName> InTypes) { AcceptTypes = MoveTemp(InTypes); }
	void SetOnDrop(FRuitkCallback InCb) { OnDropCb = MoveTemp(InCb); }
	void SetOnDragEnter(FRuitkCallback InCb) { OnEnterCb = MoveTemp(InCb); }
	void SetOnDragLeave(FRuitkCallback InCb) { OnLeaveCb = MoveTemp(InCb); }

	bool IsOver() const { return bIsOver; }

	virtual void OnDragEnter(const FGeometry&, const FDragDropEvent& Event) override
	{
		const TSharedPtr<FRuitkDragDropOp> Op = Event.GetOperationAs<FRuitkDragDropOp>();
		if (Accepts(Op))
		{
			bIsOver = true;
			if (OnEnterCb.IsBound())
			{
				OnEnterCb.Execute(Op->Payload);
			}
		}
	}

	virtual void OnDragLeave(const FDragDropEvent&) override
	{
		if (bIsOver)
		{
			bIsOver = false;
			if (OnLeaveCb.IsBound())
			{
				OnLeaveCb.Execute(FRuitkValue());
			}
		}
	}

	virtual FReply OnDragOver(const FGeometry&, const FDragDropEvent& Event) override
	{
		return Accepts(Event.GetOperationAs<FRuitkDragDropOp>()) ? FReply::Handled() : FReply::Unhandled();
	}

	virtual FReply OnDrop(const FGeometry&, const FDragDropEvent& Event) override
	{
		const TSharedPtr<FRuitkDragDropOp> Op = Event.GetOperationAs<FRuitkDragDropOp>();
		if (Accepts(Op))
		{
			if (OnDropCb.IsBound())
			{
				OnDropCb.Execute(Op->Payload);
			}
			// Slate delivers no OnDragLeave to the widget it was dropped on (still under the cursor), so
			// fire the leave callback here — else any hover styling lit on OnDragEnter stays on forever
			// after the drop (bughunt B7). Mirror the OnDragLeave guard: only when we were hovered.
			if (bIsOver)
			{
				bIsOver = false;
				if (OnLeaveCb.IsBound())
				{
					OnLeaveCb.Execute(FRuitkValue());
				}
			}
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

private:
	bool Accepts(const TSharedPtr<FRuitkDragDropOp>& Op) const
	{
		return Op.IsValid() && (AcceptTypes.Num() == 0 || AcceptTypes.Contains(Op->DragType));
	}

	TArray<FName> AcceptTypes;
	FRuitkCallback OnDropCb;
	FRuitkCallback OnEnterCb;
	FRuitkCallback OnLeaveCb;
	bool bIsOver = false;
};

// ── typed props + factories ─────────────────────────────────────────────────────────────────────

/** A draggable source (SingleContent): its child becomes grabbable; a left-drag begins an operation
 *  carrying `Payload` tagged `DragType`. OnDragStart fires with the payload when a drag begins;
 *  OnDragEnd fires with a bool (was the drop handled?) when it ends. */
struct RUITKSLATE_API FRuitkDragSourceProps final : public FRuitkPropsBase
{
	RUITK_PROP(FName, DragType, 0)
	RUITK_PROP(FRuitkValue, Payload, 1)
	RUITK_PROP_EVENT(OnDragStart, 2)
	RUITK_PROP_EVENT(OnDragEnd, 3)
	RUITK_PROPS_BODY(FRuitkDragSourceProps, RUITK_EQ(DragType) RUITK_EQ(Payload) RUITK_EQ(OnDragStart) RUITK_EQ(OnDragEnd))
};

/** A drop zone (SingleContent): accepts operations whose DragType is in `AcceptTypes` (empty = accept
 *  ANY FRuitkDragDropOp). OnDrop fires with the dropped payload; OnDragEnter/OnDragLeave fire with the
 *  hovering payload (for hover styling) as an accepted op enters/leaves. */
struct RUITKSLATE_API FRuitkDropTargetProps final : public FRuitkPropsBase
{
	RUITK_PROP(TArray<FName>, AcceptTypes, 0)
	RUITK_PROP_EVENT(OnDrop, 1)
	RUITK_PROP_EVENT(OnDragEnter, 2)
	RUITK_PROP_EVENT(OnDragLeave, 3)
	RUITK_PROPS_BODY(FRuitkDropTargetProps, RUITK_EQ(AcceptTypes) RUITK_EQ(OnDrop) RUITK_EQ(OnDragEnter) RUITK_EQ(OnDragLeave))
};

namespace Ruitk::Slate
{
	RUITKSLATE_API FRuitkElementTypeId DragSourceType();
	RUITKSLATE_API FRuitkElementTypeId DropTargetType();

	/** Wrap draggable content. The child is grabbed on a left-drag; the operation carries `Payload`. */
	RUITKSLATE_API FRuitkNode DragSource(FRuitkDragSourceProps Props = FRuitkDragSourceProps(),
											TArray<FRuitkNode> Children = TArray<FRuitkNode>(), FRuitkKey Key = FRuitkKey());

	/** Wrap a drop zone. Accepted operations dropped here fire OnDrop with their payload. */
	RUITKSLATE_API FRuitkNode DropTarget(FRuitkDropTargetProps Props = FRuitkDropTargetProps(),
											TArray<FRuitkNode> Children = TArray<FRuitkNode>(), FRuitkKey Key = FRuitkKey());

	/** Register the DragSource/DropTarget adapters (called from RegisterBuiltinAdapters; idempotent). */
	namespace Detail
	{
		void RegisterDragDropAdapters();
	}
} // namespace Ruitk::Slate
