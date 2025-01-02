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
// Please keep my name in "author" field.
// 
// Enjoy it.
// ================================================================

//QUEUE_PAIR stands for "Submission and Completion Queue in one pair".
//Each SubmissionQueue match to a unique CompletionQueue.
//So I combine them into a "Queue Pair". 
//Each NVMe device has multiple "Queue Pair" to handle I/O and Admin Cmds.
typedef struct _QUEUE_PAIR_CONFIG {
    PVOID DevExt = nullptr;
    USHORT QID = 0;         //QueueID is zero-based. ID==0 is assigned to AdminQueue constantly
    USHORT Depth = 0;       //How many Submission Entry(equal to Completion Entry) in each queue?
    ULONG NumaNode = MM_ANY_NODE_OK;
    QUEUE_TYPE Type = QUEUE_TYPE::IO_QUEUE;
    PNVME_SUBMISSION_QUEUE_TAIL_DOORBELL SubDbl = nullptr;
    PNVME_COMPLETION_QUEUE_HEAD_DOORBELL CplDbl = nullptr;
    PVOID PreAllocBuffer = nullptr;            //SubQ and CplQ should be continuous memory together
    size_t PreAllocBufSize = 0; 
}QUEUE_PAIR_CONFIG, * PQUEUE_PAIR_CONFIG;

class CNvmeQueue
{
public:
#pragma region ======== Static Functions ========
    const static MEMORY_CACHING_TYPE CacheType = MEMORY_CACHING_TYPE::MmNonCached;
    static VOID QueueCplDpcRoutine(
        _In_ PSTOR_DPC dpc,
        _In_ PVOID devext,
        _In_opt_ PVOID sysarg1,
        _In_opt_ PVOID sysarg2
    );
#pragma endregion

#pragma region ======== Data Members ========
    STOR_DPC QueueCplDpc;
    PVOID DevExt = nullptr;
    USHORT QueueID = NVME_INVALID_QID;  //1-based ID, 0 is reserved for AdminQ
    USHORT Depth = 0;       //how many entries in both SubQ and CplQ?
    ULONG NumaNode = MM_ANY_NODE_OK;
    bool IsReady = false;

    QUEUE_TYPE Type = QUEUE_TYPE::IO_QUEUE;
    ULONG SubTail = INIT_DBL_VALUE;
    ULONG SubHead = INIT_DBL_VALUE;
    ULONG CplHead = INIT_DBL_VALUE;
    USHORT PhaseTag = CPL_INIT_PHASETAG;
    PNVME_SUBMISSION_QUEUE_TAIL_DOORBELL SubDbl = nullptr;
    PNVME_COMPLETION_QUEUE_HEAD_DOORBELL CplDbl = nullptr;

    KSPIN_LOCK SubLock;

    volatile LONG InflightCmds = 0;
    bool UseExtBuffer = false; //Is this Queue use "external allocated buffer" ?
    //In CNvmeQueuePair, it allocates SubQ and CplQ in one large continuous block.
    //QueueBuffer is pointer of this large block.
    //Then divide into 2 blocks for SubQ and CplQ.
    PVOID Buffer = nullptr;
    PHYSICAL_ADDRESS BufferPA = { 0 };
    size_t BufferSize = 0;      //total size of entire queue buffer, BufferSize >= (SubQ_Size + CplQ_Size)

    PNVME_COMMAND SubQ_VA = nullptr;       //Virtual address of SubQ Buffer.
    PHYSICAL_ADDRESS SubQ_PA = { 0 };
    size_t SubQ_Size = 0;       //total length of SubQ Buffer.

    PNVME_COMPLETION_ENTRY CplQ_VA = nullptr;       //Virtual address of CplQ Buffer.
    PHYSICAL_ADDRESS CplQ_PA = { 0 };
    size_t CplQ_Size = 0;       //total length of CplQ Buffer.

    volatile USHORT InternalCid = 0;
    PSPC_SRBEXT* OriginalSrbExt = nullptr;    //record the caller's SRBEXT, complete them when request done.
    PSPC_SRBEXT SpecialSrbExt = nullptr;     //special cmd's srbext which should reserve cid. e.g. AsyncEvent....
#pragma endregion

#pragma region ======== Ctor, Dtor, Setup and Teardown ========
    CNvmeQueue();
    CNvmeQueue(QUEUE_PAIR_CONFIG* config);
    ~CNvmeQueue();

    NTSTATUS Setup(QUEUE_PAIR_CONFIG* config);
    void Teardown();
#pragma endregion

#pragma region ======== Methods ========
    NTSTATUS SubmitCmd(PSPC_SRBEXT srbext, PNVME_COMMAND src_cmd);
    void CompleteCmd(ULONG max_count = 0);
    void GiveupAllCmd();
    void GetQueueAddr(PVOID* subva, PHYSICAL_ADDRESS* subpa, PVOID* cplva, PHYSICAL_ADDRESS* cplpa);
    void GetQueueAddr(PVOID *subq, PVOID* cplq);
    void GetQueueAddr(PHYSICAL_ADDRESS* subq, PHYSICAL_ADDRESS* cplq);
    void GetSubQAddr(PHYSICAL_ADDRESS* subq);
    void GetCplQAddr(PHYSICAL_ADDRESS* cplq);

    ULONG ReadSubTail();
    void WriteSubTail(ULONG value);
    ULONG ReadCplHead();
    void WriteCplHead(ULONG value);
    bool InitQueueBuffer();    //init contents of this queue
    bool AllocQueueBuffer();    //allocate memory of this queue
    bool AllocSrbExtBuffer();   //SrbExtBuffer stores original srbext of submitted cmd.
    void DeallocSrbExtBuffer();
    void DeallocQueueBuffer();

    USHORT GetNextCid();
    USHORT CidToSrbExtIdx(USHORT cid);
    bool IsSafeForSubmit();
    void PushSrbExt(PSPC_SRBEXT srbext, USHORT cid);
    PSPC_SRBEXT PopSrbExt(USHORT cid);
#pragma endregion


    inline bool IsInitOK(){return IsReady;}
};
