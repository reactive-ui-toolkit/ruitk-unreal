// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// Compile-coverage TU: includes EVERY public header of the module so a header that only
// compiles by include-order luck (or not at all) fails HERE, in this module, on every
// build — not in the first downstream consumer. Also the honest IWYU check for our own
// public surface. Add each new public header to this list in the same commit.

#include "RuitkTypes.h"
#include "RuitkPropsBase.h"
#include "RuitkElementRegistry.h"
#include "RuitkNode.h"
#include "RuitkHostConfig.h"
#include "RuitkHooksInternal.h"
#include "RuitkComponentState.h"
#include "RuitkFiber.h"
#include "RuitkContextHandle.h"
#include "RuitkCoreMisc.h"
#include "RuitkContext.h"
#include "RuitkReconciler.h"
#include "RuitkSignal.h"
#include "RuitkCoreElements.h"

// Instantiate the templates a header-only consumer would (template errors surface at
// instantiation, not parse).
namespace RuitkCoreChecks
{
	static void CompileCheck()
	{
		TRuitkSetter<int32> IntSetter;
		TRuitkSetter<FString> StrSetter;
		(void)(IntSetter == IntSetter);
		(void)(StrSetter == StrSetter);

		FRuitkDeps D = Ruitk::Deps(1, 2.0f, TEXT("x"), FName(TEXT("n")));
		(void)Ruitk::DepsChanged(D, D);

		TRuitkStateCell<int32> State(0);
		TRuitkRefCell<float> Ref(0.0f);
		TRuitkMemoCell<FString> Memo;
		TRuitkDeferredCell<int32> Deferred;
		(void)State;
		(void)Ref;
		(void)Memo;
		(void)Deferred;

		FRuitkFiberSlab Slab;
		FRuitkFiber* F = Slab.Acquire();
		Slab.Release(F);
	}

	// Never called; exists so the linker keeps the instantiations honest without running.
	void* Sink = (void*)&CompileCheck;
} // namespace RuitkCoreChecks
