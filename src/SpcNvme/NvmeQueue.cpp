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
inline USHORT CNvmeQueue::GetNextCid()
{
    ULONG new_cid = (ULONG)InterlockedIncrement((volatile LONG*)&InternalCid);
    return 1 + (new_cid % Depth); //let CID be 1-based index so add 1.
}
inline USHORT CNvmeQueue::CidToSrbExtIdx(USHORT cid)
{
    return cid - 1;
}
inline bool CNvmeQueue::IsSafeForSubmit()
{
    return ((Depth - SAFE_SUBMIT_THRESHOLD) > (USHORT)InflightCmds) ? true : false;
}
#pragma region ======== class CNvmeQueue ========
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
CNvmeQueue::CNvmeQueue()
{
}
CNvmeQueue::CNvmeQueue(QUEUE_PAIR_CONFIG* config)
    : CNvmeQueue()
{
    Setup(config);
}
CNvmeQueue::~CNvmeQueue()
{
    Teardown();
}
NTSTATUS CNvmeQueue::Setup(QUEUE_PAIR_CONFIG* config)
{
    bool ok = false;
    NTSTATUS status = STATUS_SUCCESS;

    DevExt = config->DevExt;
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

    if(nullptr == Buffer)
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
    IsReady = false;
    DeallocSrbExtBuffer();
    DeallocQueueBuffer();
}
NTSTATUS CNvmeQueue::SubmitCmd(PSPC_SRBEXT srbext, PNVME_COMMAND src_cmd)
{
    if (!IsReady)
        return STATUS_DEVICE_NOT_READY;
    //throttle of submittion. If SubTail exceed CplHead, 
    //NVMe device will have fatal error and stopped.
    if(!IsSafeForSubmit())
        return STATUS_DEVICE_BUSY;
    {
        CQueuedSpinLock lock(&SubLock);

        ASSERT(nullptr != srbext);
        if (MAXUSHORT == src_cmd->CDW0.CID)
            src_cmd->CDW0.CID = GetNextCid();

        PushSrbExt(srbext, src_cmd->CDW0.CID);

        //For Debugging
        srbext->SubTail = SubTail;
        srbext->SubmitCmdPtr = (SubQ_VA + SubTail);
        srbext->SubmittedQ = this;

        RtlCopyMemory((SubQ_VA+SubTail), src_cmd, sizeof(NVME_COMMAND));
        InterlockedIncrement(&InflightCmds);
        SubTail = (SubTail + 1) % Depth;
        WriteDbl(DevExt, SubDbl, SubTail);
    }

    return STATUS_SUCCESS;
}
void CNvmeQueue::GiveupAllCmd()
{
    CQueuedSpinLock lock(&SubLock);

    for (ULONG i = 0; i < Depth; i++)
    {   
        PSPC_SRBEXT srbext = (PSPC_SRBEXT)InterlockedExchangePointer(
                (volatile PVOID*)&OriginalSrbExt[i], nullptr);

        if (nullptr != srbext)
            srbext->CompleteSrb(SRB_STATUS_BUS_RESET);
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
        max_count = Depth;

    while(NewCplArrived(cpl, PhaseTag) && max_count > done_count)
    {
        USHORT cid = cpl->DW3.CID;
        PSPC_SRBEXT srbext = PopSrbExt(cid);
        SubHead = cpl->DW2.SQHD;

        if(nullptr != srbext)
        {
            done_count++;
            RtlCopyMemory(&srbext->NvmeCpl, cpl, sizeof(NVME_COMPLETION_ENTRY));
            srbext->CompleteSrb(srbext->NvmeCpl.DW3.Status);
            srbext->CleanUp();
            //if (srbext->DeleteInComplete)
            //    delete srbext;
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
    if(subq != nullptr)
        *subq = SubQ_VA;

    if (cplq != nullptr)
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
    if (IsValidQid(QueueID) && nullptr != SubDbl)
        return ReadDbl(DevExt, SubDbl);
    KdBreakPoint();
    return INVALID_DBL_VALUE;
}
void CNvmeQueue::WriteSubTail(ULONG value)
{
    if (IsValidQid(QueueID) && nullptr != SubDbl)
        return WriteDbl(DevExt, SubDbl, value);
    KdBreakPoint();
}
ULONG CNvmeQueue::ReadCplHead()
{
    if (IsValidQid(QueueID) && nullptr != CplDbl)
        return ReadDbl(DevExt, CplDbl);
    KdBreakPoint();
    return INVALID_DBL_VALUE;
}
void CNvmeQueue::WriteCplHead(ULONG value)
{
    if (IsValidQid(QueueID) && nullptr != CplDbl)
        return WriteDbl(DevExt, CplDbl, value);
    KdBreakPoint();
}
void CNvmeQueue::PushSrbExt(PSPC_SRBEXT srbext, USHORT cid)
{
//for easier debugging, define this variable at first line.
    USHORT idx = CidToSrbExtIdx(cid);
    if(NVME_ASYNC_REQ_CID == cid)
        SpecialSrbExt = srbext;
    else
    {
        PSPC_SRBEXT old_srbext = (PSPC_SRBEXT)InterlockedCompareExchangePointer(
            (volatile PVOID*)OriginalSrbExt + idx, srbext, nullptr);

        if (nullptr != old_srbext)
            old_srbext->CompleteSrb(SRB_STATUS_ABORTED);
    }
}
PSPC_SRBEXT CNvmeQueue::PopSrbExt(USHORT cid)
{
    //for easier debugging, define these variables at first line.
    USHORT idx = CidToSrbExtIdx(cid);
    PSPC_SRBEXT srbext = nullptr;
    if (NVME_ASYNC_REQ_CID == cid)
    {
        srbext = SpecialSrbExt;
        SpecialSrbExt = nullptr;
    }
    else
    {
        srbext = (PSPC_SRBEXT)InterlockedExchangePointer(
            (volatile PVOID*)OriginalSrbExt + idx, nullptr);

        ASSERT(srbext != nullptr);
    }
    return srbext;
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
        DevExt, BufferSize,
        low, high, align,
        CNvmeQueue::CacheType, NumaNode,
        &Buffer);

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
    SubQ_Size = Depth * sizeof(NVME_COMMAND);
    CplQ_Size = Depth * sizeof(NVME_COMPLETION_ENTRY);

    PUCHAR cursor = (PUCHAR) Buffer;
    SubQ_VA = (PNVME_COMMAND)ROUND_TO_PAGES(cursor);
    cursor += SubQ_Size;
    CplQ_VA = (PNVME_COMPLETION_ENTRY)ROUND_TO_PAGES(cursor);

    //this->BufferSize = this->SubQ_Size + this->CplQ_Size;
    //Because Align to Page could cause extra waste space in memory.
    //So should check if CplQ exceeds total buffer length...
    if((cursor + CplQ_Size) > ((PUCHAR)Buffer + BufferSize))
        goto ERROR; //todo: log

    RtlZeroMemory(Buffer, BufferSize);
    SubQ_PA = MmGetPhysicalAddress(SubQ_VA);
    CplQ_PA = MmGetPhysicalAddress(CplQ_VA);
    return true;

ERROR:
    SubQ_Size = 0;
    SubQ_VA = nullptr;
    CplQ_Size = 0;
    CplQ_VA = nullptr;
    return false;
}
void CNvmeQueue::DeallocQueueBuffer()
{
    if(UseExtBuffer)
        return;

    if(nullptr != Buffer)
    { 
        StorPortFreeContiguousMemorySpecifyCache(
                DevExt, Buffer, BufferSize, CNvmeQueue::CacheType);
    }

    Buffer = nullptr;
    BufferSize = 0;
}
bool CNvmeQueue::AllocSrbExtBuffer()
{
    OriginalSrbExt = (PSPC_SRBEXT*) 
            new(NonPagedPool, TAG_SRB_HISTORY) PSPC_SRBEXT[Depth];
    if(nullptr == OriginalSrbExt)
        return false;

    RtlZeroMemory(OriginalSrbExt, sizeof(PSPC_SRBEXT)*Depth);
    return true;
}
void CNvmeQueue::DeallocSrbExtBuffer()
{
    if (nullptr != OriginalSrbExt)
    {
        delete[] OriginalSrbExt;
        OriginalSrbExt = nullptr;
    }
}
#pragma endregion
