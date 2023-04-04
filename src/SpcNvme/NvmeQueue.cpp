#include "pch.h"
static inline size_t CalcQueueBufferSize(USHORT depth)
{
    size_t page_count = BYTES_TO_PAGES(depth * sizeof(NVME_COMMAND)) +
                    BYTES_TO_PAGES(depth * sizeof(NVME_COMPLETION_ENTRY));
    return page_count * PAGE_SIZE;
}

static inline ULONG ReadDbl(PVOID devext, PNVME_SUBMISSION_QUEUE_TAIL_DOORBELL dbl)
{
    MemoryBarrier();
    return StorPortReadRegisterUlong(devext, &dbl->AsUlong);
}
static inline ULONG ReadDbl(PVOID devext, PNVME_COMPLETION_QUEUE_HEAD_DOORBELL dbl)
{
    MemoryBarrier();
    return StorPortReadRegisterUlong(devext, &dbl->AsUlong);
}
static inline void WriteDbl(PVOID devext, PNVME_SUBMISSION_QUEUE_TAIL_DOORBELL dbl, ULONG value)
{
    //Doorbell should write 32bit ULONG.
    //Some controller won't accept the doorbell value if you write only 16bit USHORT value.
    MemoryBarrier();
    StorPortWriteRegisterUlong(devext, &dbl->AsUlong, value);
}
static inline void WriteDbl(PVOID devext, PNVME_COMPLETION_QUEUE_HEAD_DOORBELL dbl, ULONG value)
{
    //Doorbell should write 32bit ULONG.
    //Some controller won't accept the doorbell value if you write only 16bit USHORT value.
    MemoryBarrier();
    StorPortWriteRegisterUlong(devext, &dbl->AsUlong, value);
}
static inline bool IsValidQid(ULONG qid)
{
    return (qid != NVME_INVALID_QID);
}
static inline bool NewCplArrived(PNVME_COMPLETION_ENTRY entry, USHORT current_tag)
{
    if (entry->DW3.Status.P == current_tag)
        return true;
    return false;
}
static inline void UpdateCplHeadAndPhase(ULONG &cpl_head, USHORT &phase, USHORT depth)
{
    cpl_head = (cpl_head + 1) % depth;
    if (0 == cpl_head)
        phase = !phase;
    //quick calculation, write a boolean table will know why.
    //    phase = !(cpl_head ^ phase);      
}
#pragma region ======== class CNvmeQueue ========
CNvmeQueue::CNvmeQueue()
{
    KeInitializeSpinLock(&SubLock);
    KeInitializeSpinLock(&CplLock);
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

    status = History.Setup(this, this->DevExt, this->Depth, this->NumaNode);
    if (!NT_SUCCESS(status))
    {
        //status = STATUS_INTERNAL_ERROR;
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
    DeallocQueueBuffer();
    History.Teardown();
}
USHORT CNvmeQueue::GetQueueID(){return QueueID;}
USHORT CNvmeQueue::GetQueueDepth(){return this->Depth;}
QUEUE_TYPE CNvmeQueue::GetQueueType(){return this->Type;}

NTSTATUS CNvmeQueue::SubmitCmd(SPCNVME_SRBEXT *srbext)
{
    return SubmitCmd(srbext, &srbext->NvmeCmd);
}
NTSTATUS CNvmeQueue::SubmitCmd(SPCNVME_SRBEXT* srbext, PNVME_COMMAND src_cmd)
{
    CSpinLock lock(&SubLock);
    NTSTATUS status = STATUS_SUCCESS;
    ULONG history_idx = 0;
    if (!this->IsReady)
        return STATUS_DEVICE_NOT_READY;

    history_idx = src_cmd->CDW0.CID % Depth;
    status = History.Push(history_idx, srbext);
    if (!NT_SUCCESS(status))
    {
        //old cmd is timed out, release it...
        PSPCNVME_SRBEXT old_srbext = NULL;
        History.Pop(history_idx, old_srbext);
        SrbSetSrbStatus(old_srbext->Srb, SRB_STATUS_TIMEOUT);
        StorPortNotification(RequestComplete, old_srbext->DevExt, old_srbext->Srb);

        //after release old cmd, push current cmd again...
        History.Push(history_idx, srbext);
    }

    RtlCopyMemory((SubQ_VA+SubTail), src_cmd, sizeof(NVME_COMMAND));
    SubTail = (SubTail + 1) % Depth;
    WriteDbl(this->DevExt, this->SubDbl, this->SubTail);

    return STATUS_SUCCESS;
}
NTSTATUS CNvmeQueue::CompleteCmd(ULONG max_count, ULONG& done_count)
{
    //max_count : max cmd completion done in this round in CompleteCmd().
    //cpl_count : how mant cmd completion done in this round?

    NTSTATUS status = STATUS_SUCCESS;
    done_count = 0;
    while(max_count > 0)
    {
        PSPCNVME_SRBEXT srbext = NULL;
        {
            CSpinLock lock(&CplLock);
            PNVME_COMPLETION_ENTRY entry = &CplQ_VA[CplHead];
            ULONG history_idx = 0;
            USHORT cid = 0;
            if(!NewCplArrived(entry, PhaseTag))
                break;

            cid = entry->DW3.CID;
            history_idx = cid % Depth;
            srbext = NULL;
            status = History.Pop(history_idx, srbext);

            //if pop failed, previously saved SrbExt could be released by CID collision cmd.
            //skip it and process next completion.
            if(NT_SUCCESS(status) && NULL != srbext)
            {
                RtlCopyMemory(&srbext->NvmeCpl, entry, sizeof(NVME_COMPLETION_ENTRY));
            }
            else
                DbgBreakPoint();
            SubHead = entry->DW2.SQHD;
            UpdateCplHeadAndPhase(CplHead, PhaseTag, Depth);
            done_count++;
        }

        if(NULL != srbext)
        {
            if (srbext->CompletionCB)
                srbext->CompletionCB(srbext);
            else
            {
                UCHAR srb_status = ToSrbStatus(srbext->NvmeCpl.DW3.Status);
                srbext->CompleteSrbWithStatus(srb_status);
                srbext->ResetExtBuf(NULL);
            }
        }

        if(max_count > 0 && done_count >= max_count)
            break;
    }

    if(done_count > 0)
        WriteDbl(this->DevExt, this->CplDbl, this->CplHead);

    return STATUS_SUCCESS;
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
    PHYSICAL_ADDRESS low = { .HighPart = 0X000000001 }; //I want to allocate it above 4G...
    PHYSICAL_ADDRESS high = { .QuadPart = (LONGLONG)-1 };
    PHYSICAL_ADDRESS align = { 0 };

    //I am too lazy to check if NVMe device request continuous page or not, so.... 
    //Allocate SubQ and CplQ together into a continuous physical memory block.
    ULONG status = StorPortAllocateContiguousMemorySpecifyCacheNode(
        this->DevExt, this->BufferSize,
        low, high, align,
        this->CacheType, this->NumaNode,
        &this->Buffer);

    //todo: log 
    if(STOR_STATUS_SUCCESS != status)
    {
        KdBreakPoint();
        return false;
    }
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
                DevExt, this->Buffer, this->BufferSize, this->CacheType);
    }

    this->Buffer = NULL;
    this->BufferSize = 0;
}
#pragma endregion

#pragma region ======== class CCmdHistory ========
CCmdHistory::CCmdHistory()
{
    KeInitializeSpinLock(&CmdLock);
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
NTSTATUS CCmdHistory::Push(ULONG index, PSPCNVME_SRBEXT srbext)
{
    if(index >= Depth)
        return STATUS_UNSUCCESSFUL;

    PVOID old_ptr = InterlockedCompareExchangePointer(
                        (volatile PVOID*)&History[index], srbext, NULL);

    if(NULL != old_ptr)
        return STATUS_DEVICE_BUSY;

    return STATUS_SUCCESS;
}
NTSTATUS CCmdHistory::Pop(ULONG index, PSPCNVME_SRBEXT& srbext)
{
    if (index >= Depth)
        return STATUS_UNSUCCESSFUL;

    srbext = (PSPCNVME_SRBEXT)InterlockedExchangePointer((volatile PVOID*)&History[index], NULL);
    if(NULL == srbext)
        return STATUS_INVALID_DEVICE_STATE;
    return STATUS_SUCCESS;
}
#pragma endregion
