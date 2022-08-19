#include "pch.h"

CSpinLock::CSpinLock(KSPIN_LOCK* lock)
{
    //No checking and no protect. Let it crash if wrong argument assigned.
    //If protect it here, driver could have timing issue because SpinLock not work.
    //It is more difficult to debug.
    this->Lock = lock;
    KeAcquireSpinLock(this->Lock, &this->OldIrql);
}

CSpinLock::~CSpinLock()
{
    KeReleaseSpinLock(this->Lock, this->OldIrql);
    this->Lock = NULL;
}

CStorSpinLock::CStorSpinLock(PVOID devext, STOR_SPINLOCK reason, PVOID ctx)
{
    this->DevExt = devext;
    Acquire(reason, ctx);
}
CStorSpinLock::~CStorSpinLock()
{
    Release();
}
