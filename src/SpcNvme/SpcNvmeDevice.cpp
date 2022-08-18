#include "pch.h"

#pragma region ======== class CSpcNvmeDevice ======== 

CSpcNvmeDevice::CSpcNvmeDevice(){}
//CSpcNvmeDevice::CSpcNvmeDevice(){}
CSpcNvmeDevice::~CSpcNvmeDevice(){}

void CSpcNvmeDevice::Setup(){}
void CSpcNvmeDevice::Teardown(){}

void CSpcNvmeDevice::Init()
{
}
#pragma endregion


#pragma region ======== class CNvmeQueuePair ========
CNvmeQueuePair::CNvmeQueuePair()
{}
CNvmeQueuePair::CNvmeQueuePair(QUEUE_PAIR_CONFIG* config) 
    : CNvmeQueuePair()
{
    Setup(config);
}
CNvmeQueuePair::~CNvmeQueuePair()
{
    Teardown();
}
void CNvmeQueuePair::Setup(QUEUE_PAIR_CONFIG* config)
{
    UNREFERENCED_PARAMETER(config);
    //PVOID DevExt = NULL;
    //USHORT QueueID = NVME_INVALID_QID;
    //USHORT Depth = 0;
    //ULONG* SubQDbl = NULL;  //Doorbell of SubQ
    //ULONG* CplQDbl = NULL;  //Doorbell of CplQ
    //ULONG NumaNode = 0;
    //USHORT Depth = 0;       //how many entries in both SubQ and CplQ?

    ////In CNvmeQueuePair, it allocates SubQ and CplQ in one large continuous block.
    ////QueueBuffer is pointer of this large block.
    ////Then divide into 2 blocks for SubQ and CplQ.
    //PCHAR QueueBuffer = NULL;
    //size_t QueueBufferSize = 0;

    ////ULONG MsiMsg = 0;       //MSI interrupt message to trigger CplQ
    //QUEUE_TYPE Type = QUEUE_TYPE::UNKNOWN;
    //bool IsReady = false;
    //CNvmeSubmitQueue SubQ;
    //CNvmeCompleteQueue CplQ;
    //CNvmeSrbExtHistory SrbExts;
}
void CNvmeQueuePair::Teardown()
{
    
    this->IsReady = false;
}
#pragma endregion


#pragma region ======== class CNvmeSubmitQueue ========
CNvmeSubmitQueue::CNvmeSubmitQueue()
{}
CNvmeSubmitQueue::CNvmeSubmitQueue(PQUEUE_CONFIG config)
    :CNvmeSubmitQueue()
{
    Setup(config);
}
CNvmeSubmitQueue::~CNvmeSubmitQueue()
{
    Teardown();
}
void CNvmeSubmitQueue::Setup(PQUEUE_CONFIG config)
{
    ULONG mapped_size = 0;
    this->DevExt = config->DevExt;
    this->QueueID = config->QID;
    this->Depth = config->Depth;
    this->NumaNode = config->NumaNode;
    this->Doorbell = (PNVME_SUBMISSION_QUEUE_TAIL_DOORBELL)config->SubDbl;
    this->RawBuffer = config->QueueBuffer;
    this->RawBufferSize = config->QueueBufferSize;
    RtlZeroMemory(this->RawBuffer, this->RawBufferSize);
    this->QueuePA = StorPortGetPhysicalAddress(this->DevExt, NULL, this->RawBuffer, &mapped_size);
    this->Cmds = (PNVME_COMMAND)this->RawBuffer;
    this->Type = config->Type;
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
bool CNvmeSubmitQueue::SubmitCmd(PNVME_COMMAND new_cmd)
{
    USHORT new_tail = 0;
    NVME_COMMAND* cmd = NULL;
    //SubmitCmd() could be called from StartIo with multi-call.
    //So need a lock to protect command entries.
    CStorSpinLock lock(this->DevExt, StartIoLock, NULL);

    new_tail = (this->DblTail + 1) % this->Depth;
    if (new_tail == this->DblHead)
        return false;
    cmd = &this->Cmds[this->DblTail];

    StorPortCopyMemory((PVOID)cmd, new_cmd, sizeof(NVME_COMMAND));
    this->DblTail = new_tail;

    /* Now issue the command via Doorbell register */
    StorPortWriteRegisterUlong(this->DevExt, &this->Doorbell->AsUlong, new_tail);
    return true;
}
void CNvmeSubmitQueue::UpdateSubQHead(USHORT new_head)
{
    CStorSpinLock lock(this->DevExt, DpcLock, NULL);
    if(this->DblHead != new_head)
        this->DblHead = new_head;
}
#pragma endregion


#pragma region ======== class CNvmeCompleteQueue ========
CNvmeCompleteQueue::CNvmeCompleteQueue()
{}
CNvmeCompleteQueue::CNvmeCompleteQueue(PQUEUE_CONFIG config)
    :CNvmeCompleteQueue()
{
    Setup(config);
}
CNvmeCompleteQueue::~CNvmeCompleteQueue()
{
    Teardown();
}
void CNvmeCompleteQueue::Setup(PQUEUE_CONFIG config)
{
    ULONG mapped_size = 0;
    this->DevExt = config->DevExt;
    this->QueueID = config->QID;
    this->Depth = config->Depth;
    this->NumaNode = config->NumaNode;
    this->Phase.Tag = 0;
    this->Doorbell = (PNVME_COMPLETION_QUEUE_HEAD_DOORBELL)config->CplDbl;
    this->RawBuffer = config->QueueBuffer;
    this->RawBufferSize = config->QueueBufferSize;
    RtlZeroMemory(this->RawBuffer, this->RawBufferSize);
    this->QueuePA = StorPortGetPhysicalAddress(this->DevExt, NULL, this->RawBuffer, &mapped_size);
    this->Entries = (PNVME_COMPLETION_ENTRY)this->RawBuffer;
    this->Type = config->Type;
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
bool CNvmeCompleteQueue::PopCplEntry(NVME_COMPLETION_ENTRY& popdata)
{
//Completion Queue will be traversed when interrupt occurred.
//I am too lazy to add lock here.... :p
    PNVME_COMPLETION_ENTRY cpl = &this->Entries[this->DblHead];

    //Each round(visit all entries in queue) of traversing entire CplQ, 
    //the phase tag will be changed by NVMe device.
    //NVMe device will set cpl->DW3.Status.P to 1 in 1st rount , set to 0 in 2nd round, and so on.
    //Host driver should check phase to make sure "is this entry a new completed entry?".
    //Refer to NVMe spec 1.3 , section 4.6 , Figure 28: Completion Queue Entry: DW 3
    if(this->Phase.Tag == cpl->DW3.Status.P)
        return false;
    
    StorPortCopyMemory(&popdata, cpl, sizeof(NVME_COMPLETION_ENTRY));
    this->DblHead = (this->DblHead + 1) % this->Depth;
    if(0 == this->DblHead)
        this->Phase.Tag = !this->Phase.Tag;
    
    return true;
}
#pragma endregion


#pragma region ======== class CNvmeSrbExtHistory ========
CNvmeSrbExtHistory::CNvmeSrbExtHistory()
{}
CNvmeSrbExtHistory::CNvmeSrbExtHistory(PVOID devext, USHORT qid, USHORT depth, ULONG numa_node)
        : CNvmeSrbExtHistory()
{   Setup(devext, qid, depth, numa_node);    }
CNvmeSrbExtHistory::~CNvmeSrbExtHistory()
{
    Teardown();
}
void CNvmeSrbExtHistory::Setup(PVOID devext, USHORT qid, USHORT depth, ULONG numa_node)
{
    this->QueueID = qid;
    this->NumaNode = numa_node;
    this->Depth = depth;
    this->DevExt = devext;
    this->RawBufferSize = depth * sizeof(PSPCNVME_SRBEXT);

    PHYSICAL_ADDRESS low = {0};
    PHYSICAL_ADDRESS high = {.QuadPart = -1};
    PHYSICAL_ADDRESS align = {0};
    ULONG spstatus = StorPortAllocatePool(devext, (ULONG)this->RawBufferSize, this->BufferTag, &this->RawBuffer);
    if(STOR_STATUS_SUCCESS == spstatus)
    {
        RtlZeroMemory(this->RawBuffer, this->RawBufferSize);
        this->History = (PSPCNVME_SRBEXT*)this->RawBuffer;
    }
}
void CNvmeSrbExtHistory::Teardown()
{
    //todo: complete all remained SRBEXT

    if(NULL != this->RawBuffer)
        StorPortFreePool(this->DevExt, this->RawBuffer);

    this->RawBuffer = NULL;
    this->History = NULL;
}
bool CNvmeSrbExtHistory::Put(int index, PSPCNVME_SRBEXT srbext)
{
    PVOID old = InterlockedCompareExchangePointer(
                    (volatile PVOID*)(History+index), srbext, NULL);
    if(NULL != old)
        return false;
    return true;
}
PSPCNVME_SRBEXT CNvmeSrbExtHistory::Get(int index)
{
    return (this->History[index]);
}
#pragma endregion
