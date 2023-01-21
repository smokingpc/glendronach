#pragma once
#define CPP_TAG             (ULONG)'MPPC'       //tag == 'CPPM' in memory
void* operator new (size_t size, POOL_TYPE type, ULONG tag = CPP_TAG);
void* operator new[](size_t size, POOL_TYPE type, ULONG tag = CPP_TAG);

void DebugSrbFunctionCode(ULONG code);
void DebugScsiOpCode(UCHAR opcode);
bool IsSupportedOS(ULONG major, ULONG minor = 0);

class CDebugCallInOut
{
private:
    static const int BufSize = 64;
    char* Name = NULL;
public:
    CDebugCallInOut(char* name);
    CDebugCallInOut(const char* name);
    ~CDebugCallInOut();
};


class CSpinLock{
public:
    CSpinLock(KSPIN_LOCK* lock, bool acquire = true);
    ~CSpinLock();

    void DoAcquire();
    void DoRelease();
protected:
    KSPIN_LOCK* Lock = NULL;
    KIRQL OldIrql = PASSIVE_LEVEL;
    bool IsAcquired = false;
};

//StorSpinLock stands for "StorPort SpinLock".
//This is used in storport miniport driver.
class CStorSpinLock {
public:
    CStorSpinLock(PVOID devext, STOR_SPINLOCK reason, PVOID ctx = NULL);
    ~CStorSpinLock();
    inline void Acquire(STOR_SPINLOCK reason, PVOID ctx = NULL)
    {
        StorPortAcquireSpinLock(this->DevExt, reason, ctx, &this->Handle); 
    }
    inline void Release()
    {
        StorPortReleaseSpinLock(this->DevExt, &this->Handle);
    }

protected:
    PVOID DevExt = NULL;
    STOR_LOCK_HANDLE Handle = {DpcLock, 0};
};

