// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// The element type registry: markup tags / Ruitk:: element factories → interned
// FRuitkElementTypeId → (host-side) adapter. The CORE owns the interning so vnodes and
// fibers compare element types by a 16-bit id (no FName hashing on the hot path — D-04's
// spirit applied to types); hosts register their adapters against the same ids (Phase 2).

#pragma once

#include "CoreMinimal.h"
#include "RuitkTypes.h"

namespace Ruitk
{
	/** Intern (or fetch) the id for an element tag. Thread-safe at module-init time only —
	 *  registration happens from StartupModule/static-init on the game thread; lookups are
	 *  lock-free reads afterward (the family's registries share this contract). */
	RUITKCORE_API FRuitkElementTypeId InternElementType(FName Tag);

	/** Lookup without interning (invalid id if unknown). */
	RUITKCORE_API FRuitkElementTypeId FindElementType(FName Tag);

	/** Reverse lookup for diagnostics ("Button", "Text", ...). NAME_None for invalid. */
	RUITKCORE_API FName GetElementTypeName(FRuitkElementTypeId Id);

	/** Number of interned types (docs-drift/test introspection). */
	RUITKCORE_API int32 NumElementTypes();
} // namespace Ruitk
