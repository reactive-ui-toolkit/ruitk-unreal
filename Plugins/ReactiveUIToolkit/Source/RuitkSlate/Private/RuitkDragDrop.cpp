// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-004 drag-and-drop half — adapters/factories over the wrapper widgets + operation declared in
// RuitkDragDrop.h. The events are plain FRuitkCallbacks invoked from the widget overrides (Slate DnD is
// an override surface, not a delegate one — so no event proxy).

#include "RuitkDragDrop.h"

#include "RuitkElementAdapter.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

TSharedPtr<SWidget> FRuitkDragDropOp::GetDefaultDecorator() const
{
	return SNew(SBorder)[SNew(STextBlock).Text(FText::FromName(DragType))];
}

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Adapters
// ─────────────────────────────────────────────────────────────────────────────────────────────

class FRuitkDragSourceAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::SingleContent; }
	virtual bool IsPoolable() const override { return false; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SRuitkDragSource);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SRuitkDragSource& W = static_cast<SRuitkDragSource&>(Widget);
		const FRuitkDragSourceProps& N = static_cast<const FRuitkDragSourceProps&>(New);
		const FRuitkDragSourceProps* O = static_cast<const FRuitkDragSourceProps*>(Old);
		if (N.HasDragType() && (O == nullptr || !O->HasDragType() || !(N.DragType == O->DragType)))
		{
			W.SetDragType(N.DragType);
		}
		if (N.HasPayload() && (O == nullptr || !O->HasPayload() || !(N.Payload == O->Payload)))
		{
			W.SetPayload(N.Payload);
		}
		if (N.HasOnDragStart())
		{
			W.SetOnDragStart(N.OnDragStart);
		}
		if (N.HasOnDragEnd())
		{
			W.SetOnDragEnd(N.OnDragEnd);
		}
	}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		static_cast<SRuitkDragSource&>(Parent).SetContent(Child);
	}
};

class FRuitkDropTargetAdapter final : public IRuitkElementAdapter
{
public:
	virtual ERuitkChildKind GetChildKind() const override { return ERuitkChildKind::SingleContent; }
	virtual bool IsPoolable() const override { return false; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuitkPropsBase&, const TSharedPtr<FRuitkEventProxy>&) override
	{
		return SNew(SRuitkDropTarget);
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuitkPropsBase* Old, const FRuitkPropsBase& New) override
	{
		SRuitkDropTarget& W = static_cast<SRuitkDropTarget&>(Widget);
		const FRuitkDropTargetProps& N = static_cast<const FRuitkDropTargetProps&>(New);
		const FRuitkDropTargetProps* O = static_cast<const FRuitkDropTargetProps*>(Old);
		if (N.HasAcceptTypes() && (O == nullptr || !O->HasAcceptTypes() || !(N.AcceptTypes == O->AcceptTypes)))
		{
			W.SetAcceptTypes(N.AcceptTypes);
		}
		if (N.HasOnDrop())
		{
			W.SetOnDrop(N.OnDrop);
		}
		if (N.HasOnDragEnter())
		{
			W.SetOnDragEnter(N.OnDragEnter);
		}
		if (N.HasOnDragLeave())
		{
			W.SetOnDragLeave(N.OnDragLeave);
		}
	}

	virtual void SetContent(SWidget& Parent, const TSharedPtr<SWidget>& Child) override
	{
		static_cast<SRuitkDropTarget&>(Parent).SetContent(Child);
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Types, factories, registration
// ─────────────────────────────────────────────────────────────────────────────────────────────

namespace Ruitk::Slate
{
	FRuitkElementTypeId DragSourceType()
	{
		return Ruitk::InternElementType(FName(TEXT("DragSource")));
	}
	FRuitkElementTypeId DropTargetType()
	{
		return Ruitk::InternElementType(FName(TEXT("DropTarget")));
	}

	namespace
	{
		template <typename TProps>
		FRuitkNode MakeDnDNode(FRuitkElementTypeId Type, TProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
		{
			FRuitkNode Node;
			Node.Kind = ERuitkNodeKind::Host;
			Node.ElementType = Type;
			Node.Props = MakeShared<TProps>(MoveTemp(Props));
			Node.Children = Ruitk::MakeChildren(MoveTemp(Children));
			Node.Key = Key;
			return Node;
		}
	} // namespace

	FRuitkNode DragSource(FRuitkDragSourceProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		return MakeDnDNode(DragSourceType(), MoveTemp(Props), MoveTemp(Children), Key);
	}

	FRuitkNode DropTarget(FRuitkDropTargetProps Props, TArray<FRuitkNode> Children, FRuitkKey Key)
	{
		return MakeDnDNode(DropTargetType(), MoveTemp(Props), MoveTemp(Children), Key);
	}

	namespace Detail
	{
		void RegisterDragDropAdapters()
		{
			RegisterAdapter(DragSourceType(), MakeUnique<FRuitkDragSourceAdapter>());
			RegisterAdapter(DropTargetType(), MakeUnique<FRuitkDropTargetAdapter>());
		}
	} // namespace Detail
} // namespace Ruitk::Slate
