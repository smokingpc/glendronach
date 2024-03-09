#pragma once
// ================================================================
// SpcNvme : OpenSource NVMe Driver for Windows 8+
// Author : Roy Wang(SmokingPC).
// Licensed by MIT License.
// 
// Copyright (C) 2022, Roy Wang (SmokingPC)
// https://github.com/smokingpc/
// 
// NVMe Spec: https://nvmexpress.org/specifications/
// Contact Me : smokingpc@gmail.com
// ================================================================
// Permission is hereby granted, free of charge, to any person obtaining a 
// copy of this softwareand associated documentation files(the "Software"), 
// to deal in the Software without restriction, including without limitation 
// the rights to use, copy, modify, merge, publish, distribute, sublicense, 
// and /or sell copies of the Software, and to permit persons to whom the 
// Software is furnished to do so, subject to the following conditions :
//
// The above copyright noticeand this permission notice shall be included in 
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE 
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS 
// IN THE SOFTWARE.
// ================================================================
// [Additional Statement]
// This Driver is implemented by NVMe Spec 1.3 and Windows Storport Miniport Driver.
// You can copy, modify, redistribute the source code. 
// 
// There is only one requirement to use this source code:
// PLEASE DO NOT remove or modify the "original author" of this codes.
// Keep "original author" declaration unmodified.
// 
// Enjoy it.
// ================================================================


#define CPP_TAG             (ULONG)'MPPC'       //tag == 'CPPM' in memory
#if 0
void* operator new (size_t size, POOL_TYPE type, ULONG tag = CPP_TAG);
void* operator new[](size_t size, POOL_TYPE type, ULONG tag = CPP_TAG);
#endif

void* MemAlloc(POOL_TYPE type, size_t size, ULONG tag = CPP_TAG);
void MemDelete(void* ptr, ULONG tag = CPP_TAG);

template<typename T>
T* MemAllocEx(POOL_TYPE type, ULONG tag = CPP_TAG)
{
    return (T*) MemAlloc(type, sizeof(T), tag);
}


void DebugSrbFunctionCode(ULONG code);
void DebugScsiOpCode(UCHAR opcode);
void DebugAdapterControlCode(SCSI_ADAPTER_CONTROL_TYPE code);
void DebugUnitControlCode(SCSI_UNIT_CONTROL_TYPE code);
bool IsSupportedOS(ULONG major, ULONG minor = 0);

class CDebugCallInOut
{
private:
    static const int BufSize = 64;

    //don't call ExAllocatePool() at DIRQL, or DriverVerifier will BugCheck(0xc4)
    //using a array, don't allocate dynamically.
    char Name[BufSize];
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
class CQueuedSpinLock {
public:
    CQueuedSpinLock(KSPIN_LOCK* lock, bool acquire = true);
    ~CQueuedSpinLock();

    void DoAcquire();
    void DoRelease();
protected:
    KSPIN_LOCK* Lock = NULL;
    KIRQL OldIrql = PASSIVE_LEVEL;
    KLOCK_QUEUE_HANDLE QueueHandle;
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

