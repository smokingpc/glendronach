#pragma once

typedef enum _QUEUE_TYPE
{
    IO_QUEUE = 0,
    //INTIO_QUEUE = 1,
    ADM_QUEUE = 99,
}QUEUE_TYPE;

_Enum_is_bitflag_
typedef enum _NVME_CMD_TYPE
{
    UNKNOWN_CMD = 0,
    ADM_CMD = 1,
    IO_CMD = 2,
    SELF_ISSUED = 0x80000000,           //this command issued by SpcNvme.sys myself.
}NVME_CMD_TYPE;

typedef enum _USE_STATE
{
    FREE = 0,
    USED = 1,
}USE_STATE;

typedef struct _QUEUE_PAIR_CONFIG {
    PVOID DevExt = NULL;
    USHORT QID = 0;         //QueueID is zero-based. ID==0 is assigned to AdminQueue constantly
    USHORT Depth = 0;
    ULONG NumaNode = 0;
    QUEUE_TYPE Type = QUEUE_TYPE::ADM_QUEUE;
    PNVME_SUBMISSION_QUEUE_TAIL_DOORBELL SubDbl = NULL;
    PNVME_COMPLETION_QUEUE_HEAD_DOORBELL CplDbl = NULL;
    PVOID PreAllocBuffer = NULL;            //SubQ and CplQ should be continuous memory together
}QUEUE_PAIR_CONFIG, * PQUEUE_PAIR_CONFIG;

typedef struct _CMD_INFO
{
    volatile USE_STATE InUsed = USE_STATE::FREE;
    NVME_CMD_TYPE Type = NVME_CMD_TYPE::UNKNOWN_CMD;
    PVOID Context = NULL;           //SRBEXT of original request
    USHORT CID = NVME_INVALID_CID;   //CmdID in SubQ which submit this request.
}CMD_INFO, *PCMD_INFO;

class CNvmeDevice;
class CNvmeQueuePair;

class CNvmeDoorbell
{
    friend class CNvmeSubmitQueue;
    friend class CNvmeCompleteQueue;

public:
    CNvmeDoorbell();
    CNvmeDoorbell(class CNvmeQueuePair*parent, PVOID devext, ULONG *subdbl, ULONG *cpldbl, USHORT depth);
    ~CNvmeDoorbell();

    bool Setup(class CNvmeQueuePair*parent, PVOID devext, ULONG* subdbl, ULONG *cpldbl, USHORT depth);
    void Teardown();
    void UpdateSubQHead(USHORT new_head);
    USHORT GetNextSubTail();
    void DoorbellSubTail(USHORT new_value);
    USHORT GetCurrentCplHead();
    void DoorbellCplHead(USHORT new_value);
private:
    class CNvmeQueuePair* Parent = NULL;
    PVOID DevExt = NULL;
    USHORT Depth = 0;
    USHORT SubTail = INVALID_DBL_TAIL;
    USHORT SubHead = INVALID_DBL_HEAD;
    USHORT CplHead = INIT_CPL_DBL_HEAD;
    volatile PNVME_SUBMISSION_QUEUE_TAIL_DOORBELL SubDbl = NULL;
    volatile PNVME_COMPLETION_QUEUE_HEAD_DOORBELL CplDbl = NULL;
};

class CNvmeSubmitQueue
{
    friend class CNvmeQueue;
public:
    CNvmeSubmitQueue();
    CNvmeSubmitQueue(class CNvmeQueuePair*parent, USHORT depth, PVOID buffer, size_t size);
    ~CNvmeSubmitQueue();

    bool Setup(class CNvmeQueuePair*parent, USHORT depth, PVOID buffer, size_t size);
    void Teardown();
    operator STOR_PHYSICAL_ADDRESS() const;
    bool Submit(USHORT sub_tail, PNVME_COMMAND new_cmd);
private:
    PVOID DevExt = NULL;
    PVOID RawBuffer = NULL;                 //RawBuffer should be PAGE_ALIGNED
    size_t RawBufferSize = 0;
    STOR_PHYSICAL_ADDRESS QueuePA = { 0 };    //Physical Address of RawBuffer. Used to register in NVMe device.
    PNVME_COMMAND Cmds = NULL;              //Cast of RawBuffer
    USHORT Depth = 0;                       //how many items in this->Cmds ?
    USHORT QueueID = NVME_INVALID_QID;

    class CNvmeQueuePair* Parent = NULL;
};

class CNvmeCompleteQueue
{
    friend class CNvmeQueue;
public:
    CNvmeCompleteQueue();
    CNvmeCompleteQueue(class CNvmeQueuePair*parent, USHORT depth, PVOID buffer, size_t size);
    ~CNvmeCompleteQueue();
    bool Setup(class CNvmeQueuePair*parent, USHORT depth, PVOID buffer, size_t size);
    void Teardown();
    operator STOR_PHYSICAL_ADDRESS() const;
    bool PopCplEntry(USHORT *cpl_head, PNVME_COMPLETION_ENTRY entry);

private:
    PVOID DevExt = NULL;
    PVOID RawBuffer = NULL;
    STOR_PHYSICAL_ADDRESS QueuePA = { 0 };
    size_t RawBufferSize = 0;
    PNVME_COMPLETION_ENTRY Entries = NULL;  //Cast of RawBuffer
    USHORT Depth = 0;                       //how many items in this->Entries ?

    struct {
        USHORT Tag : 1;
        USHORT Reserved : 15;
    }Phase;

    class CNvmeQueuePair* Parent = NULL;
};

class CCmdHistory
{
    friend class CNvmeQueue;
public:
    const ULONG BufferTag = (ULONG)'HBRS';

    CCmdHistory();
    CCmdHistory(class CNvmeQueuePair*parent, PVOID devext, USHORT depth, ULONG numa_node = 0);
    ~CCmdHistory();

    bool Setup(class CNvmeQueuePair*parent, PVOID devext, USHORT depth, ULONG numa_node = 0);
    void Teardown();
    bool Push(USHORT index, NVME_CMD_TYPE type, PSPCNVME_SRBEXT srbext);
    bool Pop(USHORT index, PCMD_INFO result);
private:
    PVOID DevExt = NULL;
    PVOID RawBuffer = NULL;
    size_t RawBufferSize = 0;
    PCMD_INFO History = NULL;     //Cast of RawBuffer
    USHORT Depth = 0;                   //how many items in this->History ?
    USHORT QueueID = NVME_INVALID_QID;
    ULONG NumaNode = 0;

    class CNvmeQueuePair* Parent = NULL;
};

class CNvmeQueuePair
{
public:
    const static ULONG BufferTag = (ULONG)'QMVN';

    CNvmeQueuePair();
    CNvmeQueuePair(QUEUE_PAIR_CONFIG* config);
    ~CNvmeQueuePair();

    bool Setup(QUEUE_PAIR_CONFIG* config);
    void Teardown();

    bool SubmitCmd(PNVME_COMMAND cmd, PSPCNVME_SRBEXT srbext, bool self = false);
    bool CompleteCmd(PNVME_COMPLETION_ENTRY result, PSPCNVME_SRBEXT* ret_srbext);

    void GetQueueAddrVA(PVOID *subq, PVOID* cplq);
    void GetQueueAddrPA(PHYSICAL_ADDRESS* subq, PHYSICAL_ADDRESS* cplq);

private:
    PVOID DevExt = NULL;
    USHORT QueueID = NVME_INVALID_QID;
    USHORT Depth = 0;       //how many entries in both SubQ and CplQ?
    ULONG* SubQDbl = NULL;  //Doorbell of SubQ
    ULONG* CplQDbl = NULL;  //Doorbell of CplQ
    ULONG NumaNode = 0;
    bool IsReady = false;
    //In CNvmeQueuePair, it allocates SubQ and CplQ in one large continuous block.
    //QueueBuffer is pointer of this large block.
    //Then divide into 2 blocks for SubQ and CplQ.
    PVOID Buffer = NULL;
    PHYSICAL_ADDRESS BufferPA = {0};
    size_t BufferSize = 0;
    PVOID SubQBuffer = NULL;
    PHYSICAL_ADDRESS SubQBufferPA = { 0 }; 
    size_t SubQBufferSize = 0;
    PVOID CplQBuffer = NULL;
    PHYSICAL_ADDRESS CplQBufferPA = { 0 }; 
    size_t CplQBufferSize = 0;

    QUEUE_TYPE Type = QUEUE_TYPE::IO_QUEUE;
    CNvmeSubmitQueue SubQ;
    CNvmeCompleteQueue CplQ;
    CCmdHistory History;
    CNvmeDoorbell Doorbell;

    bool SetupQueueBuffer();
    bool AllocQueueBuffer();
    bool BindQueueBuffer();
};
