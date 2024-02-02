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


typedef struct _QUEUE_PAIR_CONFIG {
    PVOID DevExt = NULL;
    PVOID NvmeDev = NULL;
    USHORT QID = 0;         //QueueID is zero-based. ID==0 is assigned to AdminQueue constantly
    USHORT Depth = 0;
    USHORT HistoryDepth = 0;
    ULONG NumaNode = MM_ANY_NODE_OK;
    QUEUE_TYPE Type = QUEUE_TYPE::IO_QUEUE;
    PNVME_SUBMISSION_QUEUE_TAIL_DOORBELL SubDbl = NULL;
    PNVME_COMPLETION_QUEUE_HEAD_DOORBELL CplDbl = NULL;
    PVOID PreAllocBuffer = NULL;            //SubQ and CplQ should be continuous memory together
    size_t PreAllocBufSize = 0; 
    _QUEUE_PAIR_CONFIG()
    {
        RtlZeroMemory(this, sizeof(_QUEUE_PAIR_CONFIG));
    }
}QUEUE_PAIR_CONFIG, * PQUEUE_PAIR_CONFIG;

class CNvmeQueue
{
public:
    const static MEMORY_CACHING_TYPE CacheType = MEMORY_CACHING_TYPE::MmNonCached;
    static VOID QueueCplDpcRoutine(
        _In_ PSTOR_DPC dpc,
        _In_ PVOID devext,
        _In_opt_ PVOID sysarg1,
        _In_opt_ PVOID sysarg2
    );

    CNvmeQueue();
    ~CNvmeQueue();

    NTSTATUS Setup(QUEUE_PAIR_CONFIG* config);
    void Teardown();

    inline bool IsInitOK(){return this->IsReady;}
    
    NTSTATUS SubmitCmd(SPCNVME_SRBEXT* srbext, PNVME_COMMAND src_cmd);
    void CompleteCmd(ULONG max_count = 0);
    void GiveupAllCmd();
    void GetQueueAddr(PVOID* subva, PHYSICAL_ADDRESS* subpa, PVOID* cplva, PHYSICAL_ADDRESS* cplpa);
    void GetQueueAddr(PVOID *subq, PVOID* cplq);
    void GetQueueAddr(PHYSICAL_ADDRESS* subq, PHYSICAL_ADDRESS* cplq);
    void GetSubQAddr(PHYSICAL_ADDRESS* subq);
    void GetCplQAddr(PHYSICAL_ADDRESS* cplq);

    STOR_DPC QueueCplDpc;
    PVOID DevExt = NULL;
    PVOID NvmeDev = NULL;
    USHORT QueueID = NVME_INVALID_QID;  //1-based ID, 0 is reserved for AdminQ
    USHORT Depth = 0;       //how many entries in both SubQ and CplQ?
    ULONG NumaNode = MM_ANY_NODE_OK;
    bool IsReady = false;

    QUEUE_TYPE Type = QUEUE_TYPE::IO_QUEUE;
    ULONG SubTail = INIT_DBL_VALUE;
    ULONG SubHead = INIT_DBL_VALUE;
    ULONG CplHead = INIT_DBL_VALUE;
    USHORT PhaseTag = CPL_INIT_PHASETAG;
    PNVME_SUBMISSION_QUEUE_TAIL_DOORBELL SubDbl = NULL;
    PNVME_COMPLETION_QUEUE_HEAD_DOORBELL CplDbl = NULL;

    KSPIN_LOCK SubLock;

    volatile LONG InflightCmds = 0;
    bool UseExtBuffer = false; //Is this Queue use "external allocated buffer" ?
    //In CNvmeQueuePair, it allocates SubQ and CplQ in one large continuous block.
    //QueueBuffer is pointer of this large block.
    //Then divide into 2 blocks for SubQ and CplQ.
    PVOID Buffer = NULL;
    PHYSICAL_ADDRESS BufferPA = {0};
    size_t BufferSize = 0;      //total size of entire queue buffer, BufferSize >= (SubQ_Size + CplQ_Size)

    PNVME_COMMAND SubQ_VA = NULL;       //Virtual address of SubQ Buffer.
    PHYSICAL_ADDRESS SubQ_PA = { 0 }; 
    size_t SubQ_Size = 0;       //total length of SubQ Buffer.

    PNVME_COMPLETION_ENTRY CplQ_VA = NULL;       //Virtual address of CplQ Buffer.
    PHYSICAL_ADDRESS CplQ_PA = { 0 }; 
    size_t CplQ_Size = 0;       //total length of CplQ Buffer.

    volatile USHORT InternalCid = 0;
    PSPCNVME_SRBEXT *OriginalSrbExt = NULL;    //record the caller's SRBEXT, complete them when request done.
    PSPCNVME_SRBEXT SpecialSrbExt = NULL;     //special cmd's srbext which should reserve cid. e.g. AsyncEvent....

    ULONG ReadSubTail();
    void WriteSubTail(ULONG value);
    ULONG ReadCplHead();
    void WriteCplHead(ULONG value);
    bool InitQueueBuffer();    //init contents of this queue
    bool AllocQueueBuffer();    //allocate memory of this queue
    bool AllocSrbExtBuffer();
    void DeallocSrbExtBuffer();
    void DeallocQueueBuffer();

    inline USHORT GetNextCid()
    {
        ULONG new_cid = (ULONG)InterlockedIncrement((volatile LONG*)&InternalCid);
        return 1 + (new_cid % Depth); //let CID be 1-based index so add 1.
    }
    inline USHORT CidToSrbExtIdx(USHORT cid)
    {
        return cid - 1;
    }
    inline void PushSrbExt(PSPCNVME_SRBEXT srbext, USHORT cid)
    {
        USHORT idx = CidToSrbExtIdx(cid);
        if(idx >= Depth)
        {
            SpecialSrbExt = srbext;
        }
        else
        {
            PSPCNVME_SRBEXT old_srbext = (PSPCNVME_SRBEXT)InterlockedCompareExchangePointer(
                (volatile PVOID*)OriginalSrbExt + idx, srbext, NULL);

            if (NULL != old_srbext)
                old_srbext->CompleteSrb(SRB_STATUS_ABORTED);
        }
    }
    inline PSPCNVME_SRBEXT PopSrbExt(USHORT cid)
    {
        USHORT idx = CidToSrbExtIdx(cid);
        PSPCNVME_SRBEXT srbext = NULL;
        if (idx >= Depth)
        {
            srbext = SpecialSrbExt;
            SpecialSrbExt = NULL;
        }
        else
        {
            srbext = (PSPCNVME_SRBEXT)InterlockedExchangePointer(
                (volatile PVOID*)OriginalSrbExt + idx, NULL);

            ASSERT(srbext != NULL);
        }
        return srbext;
    }
    inline bool IsSafeForSubmit()
    {
        return ((Depth - SAFE_SUBMIT_THRESHOLD) > (USHORT)InflightCmds) ? true : false;
    }
};
