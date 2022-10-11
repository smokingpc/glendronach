#include "pch.h"

#pragma region ======== class CNvmeQueuePair ========
CNvmeQueuePair::CNvmeQueuePair()
{
}
CNvmeQueuePair::CNvmeQueuePair(QUEUE_PAIR_CONFIG* config)
    : CNvmeQueuePair()
{
    Setup(config);
}
CNvmeQueuePair::~CNvmeQueuePair()
{
    Teardown();
}
bool CNvmeQueuePair::Setup(QUEUE_PAIR_CONFIG* config)
{
    this->DevExt = config->DevExt;
    this->QueueID = config->QID;
    this->Depth = config->Depth;
    this->NumaNode = config->NumaNode;
    this->Type = config->Type;
    this->Buffer = config->PreAllocBuffer;
    bool ok = this->SetupQueueBuffer();

    if(ok)
    {
        ok = ok && Doorbell.Setup(this, this->DevExt, config->SubDbl, config->CplDbl, this->Depth);
        ok = ok && SubQ.Setup(this, this->Depth, this->SubQBuffer, this->SubQBufferSize);
        ok = ok && CplQ.Setup(this, this->Depth, this->CplQBuffer, this->CplQBufferSize);
        ok = ok && History.Setup(this, this->DevExt, this->Depth, this->NumaNode);
    }
    this->IsReady = ok;

    return IsReady;
}
void CNvmeQueuePair::Teardown()
{
    this->IsReady = false;
    SubQ.Teardown();
    CplQ.Teardown();
    History.Teardown();
    Doorbell.Teardown();
}
bool CNvmeQueuePair::SubmitCmd(PNVME_COMMAND cmd, PSPCNVME_SRBEXT srbext, bool self)
{
    CStorSpinLock(this->DevExt, StartIoLock);
    if (!this->IsReady)
        return false;
    //submit command are called from StartIo so I use StorportSpinLock(StartIoLock)
    USHORT sub_tail = Doorbell.GetNextSubTail();
    if(INVALID_DBL_TAIL == sub_tail)
        return false;
    NVME_CMD_TYPE type = (this->Type == QUEUE_TYPE::ADM_QUEUE)? NVME_CMD_TYPE::ADM_CMD : NVME_CMD_TYPE::IO_CMD;
    if(self)
        type = (NVME_CMD_TYPE)(type | NVME_CMD_TYPE::SELF_ISSUED);
    if(false == History.Push(sub_tail, type, srbext))
        return false;

    SubQ.Submit(sub_tail, cmd);
    Doorbell.DoorbellSubTail(sub_tail);
    return true;
}
bool CNvmeQueuePair::CompleteCmd(PNVME_COMPLETION_ENTRY result, PSPCNVME_SRBEXT *ret_srbext)
{
    //Cmd Completion is always called from DPC routine, which triggered by Interrupt or Timer.
    CStorSpinLock(this->DevExt, DpcLock);
    USHORT cpl_head = Doorbell.GetCurrentCplHead();
    bool ok = CplQ.PopCplEntry(&cpl_head, result);
    if(ok)
    {
        Doorbell.DoorbellCplHead(cpl_head);
        USHORT sub_head = result->DW2.SQHD;
        USHORT sub_old_tail = result->DW3.CID;
        Doorbell.UpdateSubQHead(sub_head);

        CMD_INFO info = { USE_STATE::FREE, NVME_CMD_TYPE::UNKNOWN_CMD};
        History.Pop(sub_old_tail, &info);
        *ret_srbext = (PSPCNVME_SRBEXT)info.Context;

        return true;
    }

    return false;
}
void CNvmeQueuePair::GetQueueAddrVA(PVOID* subq, PVOID* cplq)
{  
    if(subq != NULL)
        *subq = SubQBuffer;

    if (cplq != NULL)
        *cplq = CplQBuffer;
}
void CNvmeQueuePair::GetQueueAddrPA(PHYSICAL_ADDRESS* subq, PHYSICAL_ADDRESS* cplq)
{
    if (subq != NULL)
        subq->QuadPart = SubQBufferPA.QuadPart;

    if (cplq != NULL)
        cplq->QuadPart = CplQBufferPA.QuadPart;
}
bool CNvmeQueuePair::AllocQueueBuffer()
{
    PHYSICAL_ADDRESS low = { 0 };
    PHYSICAL_ADDRESS high = { .QuadPart = (LONGLONG)-1 };
    PHYSICAL_ADDRESS align = { 0 };

    //Queue buffer doesn't need cache. Cache could cause read coherence issue and bother cpu cache performance.
    ULONG spstatus = StorPortAllocateDmaMemory(
        this->DevExt, this->BufferSize,
        low, high, align,
        MmWriteCombined, this->NumaNode,
        &this->Buffer, &this->BufferPA);
    if (STOR_STATUS_SUCCESS == spstatus)
        return this->BindQueueBuffer();
    else if (STOR_STATUS_NOT_IMPLEMENTED == spstatus)
        KeBugCheckEx(MANUALLY_INITIATED_CRASH1, 0, 0, 0, 0);

    //todo: log 
    return false;
}
bool CNvmeQueuePair::BindQueueBuffer()
{
    if(0 == this->BufferPA.QuadPart)
    {
        ULONG mapped_size = 0;
        this->BufferPA = StorPortGetPhysicalAddress(this->DevExt, NULL, this->Buffer, &mapped_size);
        if(mapped_size != this->BufferSize)
            return false;       //todo: log
    }

    RtlZeroMemory(this->Buffer, this->BufferSize);
    this->SubQBuffer = this->Buffer;
    this->SubQBufferPA.QuadPart = this->BufferPA.QuadPart;

    this->CplQBuffer = (PVOID)(((PCHAR)this->SubQBuffer) + this->SubQBufferSize);
    this->CplQBufferPA.QuadPart = this->SubQBufferPA.QuadPart + this->SubQBufferSize;
    return true;
}
bool CNvmeQueuePair::SetupQueueBuffer()
{
    //SubQ and CplQ should be placed on same continuous memory block.
    //1.Calculate total block size. 
    //2.Split total size to SubQ size and CplQ size. NOTE: both of them should be PAGE_ALIGNED
    this->SubQBufferSize = this->Depth * sizeof(NVME_COMMAND);
    this->SubQBufferSize = ROUND_TO_PAGES(this->SubQBufferSize);

    this->CplQBufferSize = this->Depth * sizeof(NVME_COMPLETION_ENTRY);
    this->CplQBufferSize = ROUND_TO_PAGES(this->CplQBufferSize);

    this->BufferSize = this->SubQBufferSize + this->CplQBufferSize;
    if(NULL == this->Buffer)
        return this->AllocQueueBuffer();
    else
        return this->BindQueueBuffer();
}
#pragma endregion

#pragma region ======== class CNvmeQueuePair ========
CNvmeDoorbell::CNvmeDoorbell()
{}
CNvmeDoorbell::CNvmeDoorbell(class CNvmeQueuePair* parent, PVOID devext, ULONG* subdbl, ULONG *cpldbl, USHORT depth)
    :CNvmeDoorbell()
{   Setup(parent, devext, subdbl, cpldbl, depth);  }
CNvmeDoorbell::~CNvmeDoorbell()
{   Teardown(); }
bool CNvmeDoorbell::Setup(class CNvmeQueuePair* parent, PVOID devext, ULONG* subdbl, ULONG *cpldbl, USHORT depth)
{
    this->Parent = parent;
    this->DevExt = devext;
    this->SubDbl = (PNVME_SUBMISSION_QUEUE_TAIL_DOORBELL) subdbl;
    this->CplDbl = (PNVME_COMPLETION_QUEUE_HEAD_DOORBELL) cpldbl;
    this->Depth = depth;
    return true;
}
void CNvmeDoorbell::Teardown()
{
}
void CNvmeDoorbell::UpdateSubQHead(USHORT new_head)
{
    if(new_head != this->SubHead)
        this->SubHead = new_head;
}
USHORT CNvmeDoorbell::GetNextSubTail()
{
    USHORT new_tail = ((this->SubTail + 1) % this->Depth);
    if(new_tail == this->SubHead)
        return INVALID_DBL_TAIL;
    return new_tail;
}

void CNvmeDoorbell::DoorbellSubTail(USHORT new_value)
{
    if(this->SubTail == new_value)
        return;
    this->SubTail = new_value;
    StorPortWriteRegisterUlong(this->DevExt, &this->SubDbl->AsUlong, new_value);
}
USHORT CNvmeDoorbell::GetCurrentCplHead()
{
    return this->CplHead;
}
void CNvmeDoorbell::DoorbellCplHead(USHORT new_value)
{
    if(this->CplHead == new_value)
        return;
    this->CplHead = new_value;
    StorPortWriteRegisterUlong(this->DevExt, &this->CplDbl->AsUlong, new_value);
}
#pragma endregion

#pragma region ======== class CNvmeSubmitQueue ========
CNvmeSubmitQueue::CNvmeSubmitQueue()
{}
CNvmeSubmitQueue::CNvmeSubmitQueue(class CNvmeQueuePair* parent, USHORT depth, PVOID buffer, size_t size)
    :CNvmeSubmitQueue()
{
    Setup(parent, depth, buffer, size);
}
CNvmeSubmitQueue::~CNvmeSubmitQueue()
{
    Teardown();
}
bool CNvmeSubmitQueue::Setup(class CNvmeQueuePair* parent, USHORT depth, PVOID buffer, size_t size)
{
    ULONG mapped_size = 0;
    this->Parent = parent;
    this->Depth = depth;
    this->RawBuffer = buffer;
    this->RawBufferSize = size;
    RtlZeroMemory(this->RawBuffer, this->RawBufferSize);
    this->QueuePA = StorPortGetPhysicalAddress(this->DevExt, NULL, this->RawBuffer, &mapped_size);
    this->Cmds = (PNVME_COMMAND)this->RawBuffer;

    return true;
}
void CNvmeSubmitQueue::Teardown()
{
    if (NULL != this->RawBuffer)
        StorPortFreeContiguousMemorySpecifyCache(this->DevExt, this->RawBuffer, this->RawBufferSize, MmCached);

    this->RawBuffer = NULL;
    this->Cmds = NULL;
    this->QueuePA.QuadPart = 0;
}
CNvmeSubmitQueue::operator STOR_PHYSICAL_ADDRESS() const
{
    return this->QueuePA;
}
bool CNvmeSubmitQueue::Submit(USHORT sub_tail, PNVME_COMMAND new_cmd)
{
    NVME_COMMAND* cmd = NULL;
    cmd = &this->Cmds[sub_tail];
    StorPortCopyMemory((PVOID)cmd, new_cmd, sizeof(NVME_COMMAND));
    return true;
}
#pragma endregion


#pragma region ======== class CNvmeCompleteQueue ========
CNvmeCompleteQueue::CNvmeCompleteQueue()
{}
CNvmeCompleteQueue::CNvmeCompleteQueue(class CNvmeQueuePair* parent, USHORT depth, PVOID buffer, size_t size)
    :CNvmeCompleteQueue()
{
    Setup(parent, depth, buffer, size);
}
CNvmeCompleteQueue::~CNvmeCompleteQueue()
{
    Teardown();
}
bool CNvmeCompleteQueue::Setup(class CNvmeQueuePair* parent, USHORT depth, PVOID buffer, size_t size)
{
    ULONG mapped_size = 0;
    this->Parent = parent;
    this->Depth = depth;
    this->Phase.Tag = 0;
    this->RawBuffer = buffer;
    this->RawBufferSize = size;
    RtlZeroMemory(this->RawBuffer, this->RawBufferSize);
    this->QueuePA = StorPortGetPhysicalAddress(this->DevExt, NULL, this->RawBuffer, &mapped_size);
    this->Entries = (PNVME_COMPLETION_ENTRY)this->RawBuffer;
    return true;
}
void CNvmeCompleteQueue::Teardown()
{
    if (NULL != this->RawBuffer)
        StorPortFreeContiguousMemorySpecifyCache(this->DevExt, this->RawBuffer, this->RawBufferSize, MmCached);

    this->RawBuffer = NULL;
    this->Entries = NULL;
    this->QueuePA.QuadPart = 0;
}
CNvmeCompleteQueue::operator STOR_PHYSICAL_ADDRESS() const
{
    return this->QueuePA;
}
bool CNvmeCompleteQueue::PopCplEntry(USHORT *cpl_head, PNVME_COMPLETION_ENTRY entry)
{
    PNVME_COMPLETION_ENTRY cpl = &this->Entries[*cpl_head];

    //Each round(visit all entries in queue) of traversing entire CplQ, 
    //the phase tag will be changed by NVMe device.
    //NVMe device will set cpl->DW3.Status.P to 1 in 1st rount , set to 0 in 2nd round, and so on.
    //Host driver should check phase to make sure "is this entry a new completed entry?".
    //Refer to NVMe spec 1.3 , section 4.6 , Figure 28: Completion Queue Entry: DW 3
    if(this->Phase.Tag == cpl->DW3.Status.P)
        return false;
    
    StorPortCopyMemory(entry, cpl, sizeof(NVME_COMPLETION_ENTRY));
    *cpl_head = (*cpl_head + 1) % this->Depth;
    if(0 == *cpl_head)
        this->Phase.Tag = !this->Phase.Tag;
    
    return true;
}
#pragma endregion


#pragma region ======== class CNvmeSrbExtHistory ========
CCmdHistory::CCmdHistory()
{}
CCmdHistory::CCmdHistory(class CNvmeQueuePair* parent, PVOID devext, USHORT depth, ULONG numa_node)
        : CCmdHistory()
{   Setup(parent, devext, depth, numa_node);    }
CCmdHistory::~CCmdHistory()
{
    Teardown();
}
bool CCmdHistory::Setup(class CNvmeQueuePair* parent, PVOID devext, USHORT depth, ULONG numa_node)
{
    this->Parent = parent;
    this->NumaNode = numa_node;
    this->Depth = depth;
    this->DevExt = devext;
    this->RawBufferSize = depth * sizeof(CMD_INFO);

    PHYSICAL_ADDRESS low = {0};
    PHYSICAL_ADDRESS high = { .QuadPart = (LONGLONG)-1 };
    PHYSICAL_ADDRESS align = {0};
    ULONG spstatus = StorPortAllocatePool(devext, (ULONG)this->RawBufferSize, this->BufferTag, &this->RawBuffer);

    if(STOR_STATUS_SUCCESS == spstatus)
    {
        RtlZeroMemory(this->RawBuffer, this->RawBufferSize);
        this->History = (PCMD_INFO)this->RawBuffer;
        return true;
    }

    return false;
}
void CCmdHistory::Teardown()
{
    //todo: complete all remained SRBEXT

    if(NULL != this->RawBuffer)
        StorPortFreePool(this->DevExt, this->RawBuffer);

    this->RawBuffer = NULL;
    this->History = NULL;
}
bool CCmdHistory::Push(USHORT index, NVME_CMD_TYPE type, PSPCNVME_SRBEXT srbext)
{
    USE_STATE old_state = (USE_STATE)InterlockedCompareExchange(
                                (volatile long*)&History[index].InUsed, 
                                USE_STATE::USED, USE_STATE::FREE);
    if(USE_STATE::FREE != old_state)
        return false;

    PCMD_INFO entry = &History[index];
    entry->CID = index;
    entry->Context = srbext;
    entry->Type = type;
    return true;
}
bool CCmdHistory::Pop(USHORT index, PCMD_INFO result)
{
    if(History[index].InUsed == USE_STATE::FREE)
        return false;

    RtlCopyMemory(result, &History[index], sizeof(CMD_INFO));
    History[index].InUsed = USE_STATE::FREE;
    return true;
}
#pragma endregion
