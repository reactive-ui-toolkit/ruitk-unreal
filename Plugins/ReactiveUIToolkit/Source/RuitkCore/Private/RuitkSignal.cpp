// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuitkSignal.h"

namespace
{
	TMap<FName, TSharedPtr<FRuitkSignalBase>>& SignalRegistry()
	{
		static TMap<FName, TSharedPtr<FRuitkSignalBase>> Registry;
		return Registry;
	}
} // namespace

namespace Ruitk
{
	TSharedPtr<FRuitkSignalBase>* FindOrAddSignalSlot(FName Key)
	{
		return &SignalRegistry().FindOrAdd(Key);
	}

	TSharedPtr<FRuitkSignalBase> TryGetSignal(FName Key)
	{
		if (const TSharedPtr<FRuitkSignalBase>* Found = SignalRegistry().Find(Key))
		{
			return *Found;
		}
		return nullptr;
	}

	bool HasSignal(FName Key)
	{
		return SignalRegistry().Contains(Key);
	}

	void ClearSignals()
	{
		SignalRegistry().Empty();
	}
} // namespace Ruitk
