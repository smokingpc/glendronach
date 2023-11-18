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
    HistoryDepth = config->HistoryDepth;
    //Todo: StorPortNotification(SetTargetProcessorDpc)
    StorPortInitializeDpc(DevExt, &QueueCplDpc, QueueCplDpcRoutine);
    KeInitializeSpinLock(&SubLock);

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

    status = History.Setup(this, this->DevExt, this->HistoryDepth, this->NumaNode);
    if (!NT_SUCCESS(status))
        goto ERROR;

    IsReady = true;
    return STATUS_SUCCESS;
ERROR:
    Teardown();
    return status;
}
void CNvmeQueue::Teardown()
{
    this->IsReady = false;
    DeallocQueueBuffer();
    History.Teardown();
}
NTSTATUS CNvmeQueue::SubmitCmd(SPCNVME_SRBEXT* srbext, PNVME_COMMAND src_cmd)
{
    CSpinLock lock(&SubLock);
    NTSTATUS status = STATUS_SUCCESS;
    ULONG cid = 0;
    if (!this->IsReady)
        return STATUS_DEVICE_NOT_READY;

    //throttle of submittion. If SubTail exceed CplHead, 
    //NVMe device will have fatal error and stopped.
    if(!IsSafeForSubmit())
        return STATUS_DEVICE_BUSY;

    ASSERT(NULL != srbext);
    
    cid = (ULONG)(src_cmd->CDW0.CID & 0xFFFF);
    if (this->Type == QUEUE_TYPE::ADM_QUEUE)
    {
        ASSERT(IsAdmCid(cid));
        cid = cid ^ ADM_CMD_CID_FLAG;
    }
    status = History.Push(cid, srbext);

    if (STATUS_ALREADY_COMMITTED == status)
    {
        KdBreakPoint();
        //old cmd is timed out, release it...
        PSPCNVME_SRBEXT old_srbext = NULL;
        History.Pop(cid, old_srbext);
        old_srbext->CompleteSrb(SRB_STATUS_TIMEOUT);

        //after release old cmd, push current cmd again...
        History.Push(cid, srbext);
    }
    else if (!NT_SUCCESS(status))
    {
        KdBreakPoint();
        return status;
    }

    //For Debugging
    srbext->SubTail = SubTail;
    srbext->SubmittedCmd = (SubQ_VA + SubTail);
    srbext->SubmittedQ = this;
    srbext->SubmitCid = srbext->SubmittedCmd->CDW0.CID;

    RtlCopyMemory((SubQ_VA+SubTail), src_cmd, sizeof(NVME_COMMAND));
    InterlockedIncrement(&InflightCmds);
    SubTail = (SubTail + 1) % Depth;
    WriteDbl(this->DevExt, this->SubDbl, this->SubTail);

    return STATUS_SUCCESS;
}
void CNvmeQueue::ResetAllCmd()
{
    CSpinLock lock(&SubLock);
    History.Reset();
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
    ULONG update_count = 0;

    if(0 == max_count)
        max_count = this->Depth;

    while(NewCplArrived(cpl, this->PhaseTag))
    {
        PSPCNVME_SRBEXT srbext = NULL;
        USHORT cid = cpl->DW3.CID;
        SubHead = cpl->DW2.SQHD;

        if(QUEUE_TYPE::ADM_QUEUE == this->Type)// && ADM_CMD_CID_FLAG == (cid & ADM_CMD_CID_FLAG))
        {
            ASSERT(ADM_CMD_CID_FLAG == (cid & ADM_CMD_CID_FLAG));
            cid = (cid & (~ADM_CMD_CID_FLAG));
        }

        History.Pop(cid, srbext);
        if(NULL != srbext)
        {
            update_count++;
            RtlCopyMemory(&srbext->NvmeCpl, cpl, sizeof(NVME_COMPLETION_ENTRY));
            if (srbext->CompletionCB)
                srbext->CompletionCB(srbext);

            srbext->CompleteSrb(srbext->NvmeCpl.DW3.Status);
            srbext->CleanUp();
            if (srbext->DeleteInComplete)
                delete srbext;
        }
        else
            KdBreakPoint();
        InterlockedDecrement(&InflightCmds);
        UpdateCplHeadAndPhase(CplHead, PhaseTag, Depth);
        cpl = &CplQ_VA[CplHead];
        if(max_count <= update_count)
            break;
    }

    if(update_count != 0)
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
bool CNvmeQueue::IsSafeForSubmit()
{
    return ((Depth - NVME_CONST::SAFE_SUBMIT_THRESHOLD) > (USHORT)InflightCmds)? true : false;
}
ULONG CNvmeQueue::ReadSubTail()
{
    if (IsValidQid(QueueID) && NULL != SubDbl)
        return ReadDbl(DevExt, SubDbl);
    KdBreakPoint();
    return NVME_CONST::INVALID_DBL_VALUE;
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
    return NVME_CONST::INVALID_DBL_VALUE;
}
void CNvmeQueue::WriteCplHead(ULONG value)
{
    if (IsValidQid(QueueID) && NULL != CplDbl)
        return WriteDbl(DevExt, CplDbl, value);
    KdBreakPoint();
}

bool CNvmeQueue::AllocQueueBuffer()
{
    PHYSICAL_ADDRESS low = { 0 }; //I want to allocate it above 4G...
    PHYSICAL_ADDRESS high = { 0 };
    PHYSICAL_ADDRESS align = { 0 };
    
    low.HighPart = 0X000000001;
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
#pragma endregion

#pragma region ======== class CCmdHistory ========
CCmdHistory::CCmdHistory()
{
    //KeInitializeSpinLock(&CmdLock);
}
CCmdHistory::CCmdHistory(class CNvmeQueue* parent, PVOID devext, USHORT depth, ULONG numa_node)
        : CCmdHistory()
{   Setup(parent, devext, depth, numa_node);    }
CCmdHistory::~CCmdHistory()
{
    Teardown();
}
NTSTATUS CCmdHistory::Setup(class CNvmeQueue* parent, PVOID devext, USHORT depth, ULONG numa_node)
{
    this->Parent = parent;
    this->NumaNode = numa_node;
    this->Depth = depth;
    this->DevExt = devext;
    this->BufferSize = depth * sizeof(PSPCNVME_SRBEXT);
    this->QueueID = parent->QueueID;

    ULONG status = StorPortAllocatePool(devext, (ULONG)this->BufferSize, this->BufferTag, &this->Buffer);
    if(STOR_STATUS_SUCCESS != status)
        return STATUS_MEMORY_NOT_ALLOCATED;

    RtlZeroMemory(this->Buffer, this->BufferSize);
    this->History = (PSPCNVME_SRBEXT *)this->Buffer;
    return STATUS_SUCCESS;
}
void CCmdHistory::Teardown()
{
    //todo: complete all remained SRBEXT

    if(NULL != this->Buffer)
        StorPortFreePool(this->DevExt, this->Buffer);

    this->Buffer = NULL;
}
void CCmdHistory::Reset()
{
    if(NULL == this->History)
        return;

    for(ULONG i=0; i<Depth; i++)
    {
        if(History[i] != NULL)
        {
            History[i]->CompleteSrb(SRB_STATUS_BUS_RESET);
            History[i] = NULL;
        }
    }
}
NTSTATUS CCmdHistory::Push(ULONG index, PSPCNVME_SRBEXT srbext)
{
    if(index >= Depth)
        return STATUS_UNSUCCESSFUL;

    PVOID old_ptr = InterlockedCompareExchangePointer(
                        (volatile PVOID*)&History[index], srbext, NULL);

    if(NULL != old_ptr)
        return STATUS_ALREADY_COMMITTED;

    return STATUS_SUCCESS;
}
NTSTATUS CCmdHistory::Pop(ULONG index, PSPCNVME_SRBEXT& srbext)
{
    if (index >= Depth)
    {
        KdBreakPoint();
        return STATUS_UNSUCCESSFUL;
    }

    srbext = (PSPCNVME_SRBEXT)InterlockedExchangePointer((volatile PVOID*)&History[index], NULL);
    if(NULL == srbext)
    {
        KdBreakPoint();
        return STATUS_INVALID_DEVICE_STATE;
    }
    return STATUS_SUCCESS;
}
#pragma endregion
