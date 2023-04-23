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
    USHORT QID = 0;         //QueueID is zero-based. ID==0 is assigned to AdminQueue constantly
    USHORT Depth = 0;
    ULONG NumaNode = MM_ANY_NODE_OK;
    QUEUE_TYPE Type = QUEUE_TYPE::IO_QUEUE;
    PNVME_SUBMISSION_QUEUE_TAIL_DOORBELL SubDbl = NULL;
    PNVME_COMPLETION_QUEUE_HEAD_DOORBELL CplDbl = NULL;
    PVOID PreAllocBuffer = NULL;            //SubQ and CplQ should be continuous memory together
    size_t PreAllocBufSize = 0; 
}QUEUE_PAIR_CONFIG, * PQUEUE_PAIR_CONFIG;

//typedef struct _CMD_INFO
//{
//    //volatile USE_STATE InUsed = USE_STATE::FREE;
//    bool InUsed = false;
//    PSPCNVME_SRBEXT SrbExt;
//    //CMD_CTX_TYPE CtxType = CMD_CTX_TYPE::SRBEXT;
//    //PVOID Context = NULL;             //SRBEXT of original request
//    ULONG Index = ;    //CmdID in SubQ which submit this request.
//    //USHORT Tag = INVALID_QI;      //SCSI queue tag from miniport
//}CMD_INFO, *PCMD_INFO;

class CCmdHistory
{
    friend class CNvmeQueue;
public:
    const ULONG BufferTag = (ULONG)'HBRS';

    CCmdHistory();
    CCmdHistory(class CNvmeQueue*parent, PVOID devext, USHORT depth, ULONG numa_node = 0);
    ~CCmdHistory();

    NTSTATUS Setup(class CNvmeQueue*parent, PVOID devext, USHORT depth, ULONG numa_node = 0);
    void Teardown();
    NTSTATUS Push(ULONG index, PSPCNVME_SRBEXT srbext);
    NTSTATUS Pop(ULONG index, PSPCNVME_SRBEXT &srbext);

private:
    PVOID DevExt = NULL;
    PVOID Buffer = NULL;
    size_t BufferSize = 0;
    PSPCNVME_SRBEXT *History = NULL; //Cast of RawBuffer
    ULONG Depth = 0;                   //how many items in this->History ?
    USHORT QueueID = NVME_INVALID_QID;
    ULONG NumaNode = MM_ANY_NODE_OK;

    KSPIN_LOCK CmdLock;
    class CNvmeQueue* Parent = NULL;
};

class CNvmeQueue
{
public:
    const static ULONG BufferTag = (ULONG)'QMVN';
    const static MEMORY_CACHING_TYPE CacheType = MEMORY_CACHING_TYPE::MmNonCached;
    static VOID QueueCplDpcRoutine(
        _In_ PSTOR_DPC dpc,
        _In_ PVOID devext,
        _In_opt_ PVOID sysarg1,
        _In_opt_ PVOID sysarg2
    );
    
    CNvmeQueue();
    CNvmeQueue(QUEUE_PAIR_CONFIG* config);
    ~CNvmeQueue();

    NTSTATUS Setup(QUEUE_PAIR_CONFIG* config);
    void Teardown();

    inline bool IsInitOK(){return this->IsReady;}
    
    NTSTATUS SubmitCmd(SPCNVME_SRBEXT* srbext);
    NTSTATUS SubmitCmd(SPCNVME_SRBEXT* srbext, PNVME_COMMAND src_cmd);
    NTSTATUS CompleteCmd(ULONG max_count, ULONG & done_count);

    void GetQueueAddr(PVOID* subva, PHYSICAL_ADDRESS* subpa, PVOID* cplva, PHYSICAL_ADDRESS* cplpa);
    void GetQueueAddr(PVOID *subq, PVOID* cplq);
    void GetQueueAddr(PHYSICAL_ADDRESS* subq, PHYSICAL_ADDRESS* cplq);
    void GetSubQAddr(PHYSICAL_ADDRESS* subq);
    void GetCplQAddr(PHYSICAL_ADDRESS* cplq);
    USHORT GetQueueID();
    USHORT GetQueueDepth();
    QUEUE_TYPE GetQueueType();

    STOR_DPC QueueCplDpc;
private:
    PVOID DevExt = NULL;
    USHORT QueueID = NVME_INVALID_QID;
    USHORT Depth = 0;       //how many entries in both SubQ and CplQ?
    ULONG NumaNode = MM_ANY_NODE_OK;
    bool IsReady = false;
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

//    USHORT CID = NVME_INVALID_CID;
    QUEUE_TYPE Type = QUEUE_TYPE::IO_QUEUE;

    CCmdHistory History;

    ULONG SubTail = NVME_CONST::INIT_DBL_VALUE;
    ULONG SubHead = NVME_CONST::INIT_DBL_VALUE;
    ULONG CplHead = NVME_CONST::INIT_DBL_VALUE;
    USHORT PhaseTag = NVME_CONST::CPL_INIT_PHASETAG;
    PNVME_SUBMISSION_QUEUE_TAIL_DOORBELL SubDbl = NULL;
    PNVME_COMPLETION_QUEUE_HEAD_DOORBELL CplDbl = NULL;

    KSPIN_LOCK SubLock;
    KSPIN_LOCK CplLock;

    ULONG ReadSubTail();
    void WriteSubTail(ULONG value);
    ULONG ReadCplHead();
    void WriteCplHead(ULONG value);

    bool InitQueueBuffer();    //init contents of this queue
    bool AllocQueueBuffer();    //allocate memory of this queue
    void DeallocQueueBuffer();
};
