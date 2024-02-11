#include "pch.h"
static __inline size_t CalcQueueBufferSize(USHORT depth)
{
    size_t page_count = BYTES_TO_PAGES(depth * sizeof(NVME_COMMAND)) +
                    BYTES_TO_PAGES(depth * sizeof(NVME_COMPLETION_ENTRY));
    return page_count * PAGE_SIZE;
}
static __inline ULONG ReadDbl(PVOID devext, PNVME_SUBMISSION_QUEUE_TAIL_DOORBELL dbl)
{
//In release build, Read/Write Register use READ_REGISTER_ULONG / WRITE_REGISTER_ULONG
//Macros. They all have FastFence() already so no need to call MemoryBarrier in release build.
#if !defined(DBG)
    MemoryBarrier();
    UNREFERENCED_PARAMETER(devext);
#endif
    return StorPortReadRegisterUlong(devext, &dbl->AsUlong);
}
static __inline ULONG ReadDbl(PVOID devext, PNVME_COMPLETION_QUEUE_HEAD_DOORBELL dbl)
{
//In release build, Read/Write Register use READ_REGISTER_ULONG / WRITE_REGISTER_ULONG
//Macros. They all have FastFence() already so no need to call MemoryBarrier in release build.
#if !defined(DBG)
    MemoryBarrier();
    UNREFERENCED_PARAMETER(devext);
#endif
    return StorPortReadRegisterUlong(devext, &dbl->AsUlong);
}
static __inline void WriteDbl(PVOID devext, PNVME_SUBMISSION_QUEUE_TAIL_DOORBELL dbl, ULONG value)
{
//In release build, Read/Write Register use READ_REGISTER_ULONG / WRITE_REGISTER_ULONG
//Macros. They all have FastFence() already so no need to call MemoryBarrier in release build.
#if !defined(DBG)
    UNREFERENCED_PARAMETER(devext);
#endif
    //Doorbell should write 32bit ULONG.
    //Some controller won't accept the doorbell value if you write only 16bit USHORT value.
    MemoryBarrier();
    StorPortWriteRegisterUlong(devext, &dbl->AsUlong, value);
}
static __inline void WriteDbl(PVOID devext, PNVME_COMPLETION_QUEUE_HEAD_DOORBELL dbl, ULONG value)
{
//In release build, Read/Write Register use READ_REGISTER_ULONG / WRITE_REGISTER_ULONG
//Macros. They all have FastFence() already so no need to call MemoryBarrier in release build.
#if !defined(DBG)
    MemoryBarrier();
    UNREFERENCED_PARAMETER(devext);
#endif
    //Doorbell should write 32bit ULONG.
    //Some controller won't accept the doorbell value if you write only 16bit USHORT value.
    StorPortWriteRegisterUlong(devext, &dbl->AsUlong, value);
}
static __inline bool IsValidQid(ULONG qid)
{
    return (qid != NVME_INVALID_QID);
}
static __inline bool NewCplArrived(PNVME_COMPLETION_ENTRY entry, USHORT current_tag)
{
    if (entry->DW3.Status.P == current_tag)
        return true;
    return false;
}
static __inline void UpdateCplHead(ULONG &cpl_head, USHORT depth)
{
    cpl_head = (cpl_head + 1) % depth;
}
static __inline void UpdateCplHeadAndPhase(ULONG& cpl_head, USHORT& phase, USHORT depth)
{
    UpdateCplHead(cpl_head, depth);
    if (0 == cpl_head)
        phase = !phase;
    //quick calculation, write a boolean table will know why.
    //    phase = !(cpl_head ^ phase);      
}
static __inline void SetCidForAsyncEvent(CNvmeQueue *queue, PNVME_COMMAND cmd)
{
    //AsyncEvent use special CID. Don't let it overlap to normal CID range. 
    if (cmd->CDW0.OPC == NVME_ADMIN_COMMAND_ASYNC_EVENT_REQUEST)
    {
        cmd->CDW0.CID = queue->Depth+1;
    }
}

static void PushSrbExt(CNvmeQueue* queue, PSPCNVME_SRBEXT srbext, USHORT cid)
{
    USHORT idx = queue->CidToSrbExtIdx(cid);
    if (idx >= queue->Depth)
    {
        queue->SpecialSrbExt = srbext;
    }
    else
    {
        PSPCNVME_SRBEXT old_srbext = (PSPCNVME_SRBEXT)InterlockedCompareExchangePointer(
            (volatile PVOID*)&queue->OriginalSrbExt[idx], srbext, NULL);

        if (NULL != old_srbext)
            old_srbext->CompleteSrb(SRB_STATUS_ABORTED);
    }
}
static PSPCNVME_SRBEXT PopSrbExt(CNvmeQueue* queue, USHORT cid)
{
    USHORT idx = queue->CidToSrbExtIdx(cid);
    PSPCNVME_SRBEXT srbext = NULL;
    if (idx >= queue->Depth)
    {
        srbext = queue->SpecialSrbExt;
        queue->SpecialSrbExt = NULL;
    }
    else
    {
        srbext = (PSPCNVME_SRBEXT)InterlockedExchangePointer(
            (volatile PVOID*)&queue->OriginalSrbExt[idx], NULL);

        ASSERT(srbext != NULL);
    }
    return srbext;
}


VOID CNvmeQueue::QueueCplDpcRoutine(
    _In_ PSTOR_DPC dpc,
    _In_ PVOID devext,
    _In_opt_ PVOID sysarg1,
    _In_opt_ PVOID sysarg2
)
{
    UNREFERENCED_PARAMETER(devext);
    UNREFERENCED_PARAMETER(sysarg1);
    UNREFERENCED_PARAMETER(sysarg2);
    CNvmeQueue* queue = CONTAINING_RECORD(dpc, CNvmeQueue, QueueCplDpc);
    queue->CompleteCmd();
}
NTSTATUS CNvmeQueue::Setup(QUEUE_PAIR_CONFIG* config)
{
    NTSTATUS status = STATUS_SUCCESS;
    bool ok = false;
    InitVars();
    DevExt = config->DevExt;
    NvmeDev = config->NvmeDev;
    QueueID = config->QID;
    Depth = config->Depth;
    NumaNode = config->NumaNode;
    Type = config->Type;
    Buffer = config->PreAllocBuffer;
    BufferSize = config->PreAllocBufSize;
    SubDbl = config->SubDbl;
    CplDbl = config->CplDbl;

    //Todo: StorPortNotification(SetTargetProcessorDpc)
    StorPortInitializeDpc(DevExt, &QueueCplDpc, QueueCplDpcRoutine);
    KeInitializeSpinLock(&SubLock);

    ok = AllocSrbExtBuffer();
    if (!ok)
    {
        status = STATUS_MEMORY_NOT_ALLOCATED;
        goto ERROR;
    }

    if(NULL == Buffer)
    {
        BufferSize = CalcQueueBufferSize(Depth);
        ok = AllocQueueBuffer();
        if(!ok)
        {
            status = STATUS_MEMORY_NOT_ALLOCATED;
            goto ERROR;
        }
    }
    else
    {
        UseExtBuffer = true;
    }

    ok = InitQueueBuffer();
    if(!ok)
    {
        status = STATUS_INTERNAL_ERROR;
        goto ERROR;
    }

    IsReady = true;
    return STATUS_SUCCESS;
ERROR:
    Teardown();
    return status;
}
void CNvmeQueue::Teardown()
{
    this->IsReady = false;
    DeallocSrbExtBuffer();
    DeallocQueueBuffer();
}
NTSTATUS CNvmeQueue::SubmitCmd(SPCNVME_SRBEXT* srbext, PNVME_COMMAND src_cmd)
{
    if (!this->IsReady)
        return STATUS_DEVICE_NOT_READY;

    //throttle of submittion. If SubTail exceed CplHead, 
    //NVMe device will have fatal error and stopped.
    if(!IsSafeForSubmit())
        return STATUS_DEVICE_BUSY;

    {
        CQueuedSpinLock lock(&SubLock);

        ASSERT(NULL != srbext);
        PushSrbExt(this, srbext, src_cmd->CDW0.CID);

        //For Debugging
        srbext->SubTail = SubTail;
        srbext->SubmitCmdPtr = (SubQ_VA + SubTail);
        srbext->SubmittedQ = this;

        RtlCopyMemory((SubQ_VA+SubTail), src_cmd, sizeof(NVME_COMMAND));
        InterlockedIncrement(&InflightCmds);
        SubTail = (SubTail + 1) % Depth;
        WriteDbl(this->DevExt, this->SubDbl, this->SubTail);
    }

    return STATUS_SUCCESS;
}
void CNvmeQueue::GiveupAllCmd()
{
    CQueuedSpinLock lock(&SubLock);

    for (ULONG i = 0; i < Depth; i++)
    {
        if (OriginalSrbExt[i] != NULL)
        {
            OriginalSrbExt[i]->CompleteSrb(SRB_STATUS_BUS_RESET);
            OriginalSrbExt[i] = NULL;
        }
    }

    InflightCmds = 0;
    while(CplHead != SubTail)
    {
        UpdateCplHeadAndPhase(CplHead, PhaseTag, Depth);
    }
}
void CNvmeQueue::CompleteCmd(ULONG max_count)
{
    //DPC is global scope mutex?
    PNVME_COMPLETION_ENTRY cpl = &CplQ_VA[CplHead];
    ULONG done_count = 0;

    if(0 == max_count)
        max_count = this->Depth;

    while(max_count > done_count)
    {
        if(!NewCplArrived(cpl, this->PhaseTag))
            break;
        USHORT cid = cpl->DW3.CID;
        PSPCNVME_SRBEXT srbext = PopSrbExt(this, cid);
        SubHead = cpl->DW2.SQHD;

        if(NULL != srbext)
        {
            done_count++;
            RtlCopyMemory(&srbext->NvmeCpl, cpl, sizeof(NVME_COMPLETION_ENTRY));
            if (srbext->CompletionCB)
                srbext->CompletionCB(srbext);

            srbext->CompleteSrb(srbext->NvmeCpl.DW3.Status);
            srbext->CleanUp();
            if (srbext->DeleteInComplete)
                MemDelete(srbext, TAG_SRBEXT);
        }
        else
            KdBreakPoint();
        InterlockedDecrement(&InflightCmds);
        UpdateCplHeadAndPhase(CplHead, PhaseTag, Depth);
        cpl = &CplQ_VA[CplHead];
    }

    if(done_count != 0)
        WriteDbl(DevExt, CplDbl, CplHead);
}
void CNvmeQueue::GetQueueAddr(PVOID* subq, PVOID* cplq)
{  
    if(subq != NULL)
        *subq = SubQ_VA;

    if (cplq != NULL)
        *cplq = CplQ_VA;
}
void CNvmeQueue::GetQueueAddr(PVOID* subva, PHYSICAL_ADDRESS* subpa, PVOID* cplva, PHYSICAL_ADDRESS* cplpa)
{
    GetQueueAddr(subva, cplva);
    GetQueueAddr(subpa, cplpa);
}
void CNvmeQueue::GetQueueAddr(PHYSICAL_ADDRESS* subq, PHYSICAL_ADDRESS* cplq)
{
    GetSubQAddr(subq);
    GetCplQAddr(cplq);
}
void CNvmeQueue::GetSubQAddr(PHYSICAL_ADDRESS* subq)
{
    subq->QuadPart = SubQ_PA.QuadPart;
}
void CNvmeQueue::GetCplQAddr(PHYSICAL_ADDRESS* cplq)
{
    cplq->QuadPart = CplQ_PA.QuadPart;
}
ULONG CNvmeQueue::ReadSubTail()
{
    if (IsValidQid(QueueID) && NULL != SubDbl)
        return ReadDbl(DevExt, SubDbl);
    KdBreakPoint();
    return INVALID_DBL_VALUE;
}
void CNvmeQueue::WriteSubTail(ULONG value)
{
    if (IsValidQid(QueueID) && NULL != SubDbl)
        return WriteDbl(DevExt, SubDbl, value);
    KdBreakPoint();
}
ULONG CNvmeQueue::ReadCplHead()
{
    if (IsValidQid(QueueID) && NULL != CplDbl)
        return ReadDbl(DevExt, CplDbl);
    KdBreakPoint();
    return INVALID_DBL_VALUE;
}
void CNvmeQueue::WriteCplHead(ULONG value)
{
    if (IsValidQid(QueueID) && NULL != CplDbl)
        return WriteDbl(DevExt, CplDbl, value);
    KdBreakPoint();
}

bool CNvmeQueue::AllocQueueBuffer()
{
    PHYSICAL_ADDRESS low = { 0 };
    PHYSICAL_ADDRESS high = { 0 };
    PHYSICAL_ADDRESS align = { 0 };
    low.QuadPart = ABOVE_4G_ADDR; //I want to allocate it above 4G...
    high.QuadPart = (LONGLONG)-1;

    //I am too lazy to check if NVMe device request continuous page or not, so.... 
    //Allocate SubQ and CplQ together into a continuous physical memory block.
    ULONG status = StorPortAllocateContiguousMemorySpecifyCacheNode(
        this->DevExt, this->BufferSize,
        low, high, align,
        CNvmeQueue::CacheType, this->NumaNode,
        &this->Buffer);

    //todo: log 
    if(STOR_STATUS_SUCCESS != status)
    {
        KdBreakPoint();
        return false;
    }

    BufferPA = MmGetPhysicalAddress(Buffer);
    return true;
}
bool CNvmeQueue::InitQueueBuffer()
{
    //SubQ and CplQ should be placed on same continuous memory block.
    //1.Calculate total block size. 
    //2.Split total size to SubQ size and CplQ size. 
    //NOTE: both of them should be PAGE_ALIGNED
    this->SubQ_Size = this->Depth * sizeof(NVME_COMMAND);
    this->CplQ_Size = this->Depth * sizeof(NVME_COMPLETION_ENTRY);

    PUCHAR cursor = (PUCHAR) this->Buffer;
    this->SubQ_VA = (PNVME_COMMAND)ROUND_TO_PAGES(cursor);
    cursor += this->SubQ_Size;
    this->CplQ_VA = (PNVME_COMPLETION_ENTRY)ROUND_TO_PAGES(cursor);

    //this->BufferSize = this->SubQ_Size + this->CplQ_Size;
    //Because Align to Page could cause extra waste space in memory.
    //So should check if CplQ exceeds total buffer length...
    if((cursor + this->CplQ_Size) > ((PUCHAR)this->Buffer + this->BufferSize))
        goto ERROR; //todo: log

    RtlZeroMemory(this->Buffer, this->BufferSize);
    this->SubQ_PA = MmGetPhysicalAddress(this->SubQ_VA);
    this->CplQ_PA = MmGetPhysicalAddress(this->CplQ_VA);
    return true;

ERROR:
    this->SubQ_Size = 0;
    this->SubQ_VA = NULL;
    this->CplQ_Size = 0;
    this->CplQ_VA = NULL;
    return false;
}
void CNvmeQueue::DeallocQueueBuffer()
{
    if(UseExtBuffer)
        return;

    if(NULL != this->Buffer)
    { 
        StorPortFreeContiguousMemorySpecifyCache(
            DevExt, this->Buffer, this->BufferSize, CNvmeQueue::CacheType);
    }

    this->Buffer = NULL;
    this->BufferSize = 0;
}
bool CNvmeQueue::AllocSrbExtBuffer()
{
    ULONG size = sizeof(PSPCNVME_SRBEXT) * Depth;
    OriginalSrbExt = (PSPCNVME_SRBEXT*) MemAlloc(NonPagedPool, size, TAG_SRB_HISTORY);
    if(NULL == OriginalSrbExt)
        return false;

    RtlZeroMemory(OriginalSrbExt, size);
    return true;
}
void CNvmeQueue::DeallocSrbExtBuffer()
{
    if (NULL != OriginalSrbExt)
    {
        MemDelete(OriginalSrbExt, TAG_SRB_HISTORY);
        OriginalSrbExt = NULL;
    }
}

void CNvmeQueue::InitVars()
{
    Guard = 0x23939889;
    IsReady = false;
    DevExt = NULL;
    NvmeDev = NULL;
    QueueID = NVME_INVALID_QID;
    Depth = IO_QUEUE_DEPTH;
    NumaNode = MM_ANY_NODE_OK;
    Type = QUEUE_TYPE::IO_QUEUE;
    Buffer = NULL;
    BufferSize = 0;
    SubDbl = NULL;
    CplDbl = NULL;
    SubTail = INIT_DBL_VALUE;
    SubHead = INIT_DBL_VALUE;
    CplHead = INIT_DBL_VALUE;
    PhaseTag = CPL_INIT_PHASETAG;
    InflightCmds = 0;
    UseExtBuffer = false;
    Buffer = NULL;
    BufferPA.QuadPart = 0;
    BufferSize = 0;
    SubQ_VA = NULL;       //Virtual address of SubQ Buffer.
    SubQ_PA.QuadPart = 0;
    SubQ_Size = 0;       //total length of SubQ Buffer.
    CplQ_VA = NULL;       //Virtual address of CplQ Buffer.
    CplQ_PA.QuadPart = 0;
    CplQ_Size = 0;       //total length of CplQ Buffer.
    InternalCid = 0;
    OriginalSrbExt = NULL;    //record the caller's SRBEXT, complete them when request done.
    SpecialSrbExt = NULL;     //special cmd's srbext which should reserve cid. e.g. AsyncEvent....
}
