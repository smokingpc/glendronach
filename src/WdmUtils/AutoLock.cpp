#include "pch.h"

CSpinLock::CSpinLock(KSPIN_LOCK* lock, bool acquire)
{
    //No checking and no protect. Let it crash if wrong argument assigned.
    //If protect it here, driver could have timing issue because SpinLock not work.
    //It is more difficult to debug.
    this->Lock = lock;
    this->IsAcquired = false;
    if(acquire)
        DoAcquire();
}
CSpinLock::~CSpinLock()
{
    DoRelease();
    this->Lock = NULL;
    this->IsAcquired = false;
}
void CSpinLock::DoAcquire()
{
    if(!IsAcquired)
    {
        KeAcquireSpinLock(this->Lock, &this->OldIrql);
        IsAcquired = true;
    }
}
void CSpinLock::DoRelease()
{
    if(IsAcquired)
    {
        KeReleaseSpinLock(this->Lock, this->OldIrql);
        IsAcquired = false;
    }
}

CQueuedSpinLock::CQueuedSpinLock(KSPIN_LOCK* lock, bool acquire)
{
    //No checking and no protect. Let it crash if wrong argument assigned.
    //If protect it here, driver could have timing issue because SpinLock not work.
    //It is more difficult to debug.
    this->Lock = lock;
    this->IsAcquired = false;
    RtlZeroMemory(&this->QueueHandle, sizeof(KLOCK_QUEUE_HANDLE));
    if (acquire)
        DoAcquire();
}
CQueuedSpinLock::~CQueuedSpinLock()
{
    DoRelease();
    this->Lock = NULL;
}
void CQueuedSpinLock::DoAcquire()
{
    if (!IsAcquired)
    {
        OldIrql = KeGetCurrentIrql();
        if(OldIrql < DISPATCH_LEVEL)
            KeAcquireInStackQueuedSpinLock(this->Lock, &this->QueueHandle);
        else
            KeAcquireInStackQueuedSpinLockAtDpcLevel(this->Lock, &this->QueueHandle);

        IsAcquired = true;
    }
}
void CQueuedSpinLock::DoRelease()
{
    if (IsAcquired)
    {
        if (OldIrql < DISPATCH_LEVEL)
            KeReleaseInStackQueuedSpinLock(&this->QueueHandle);
        else
            KeReleaseInStackQueuedSpinLockFromDpcLevel(&this->QueueHandle);

        KeReleaseSpinLock(this->Lock, this->OldIrql);
        IsAcquired = false;
    }
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
