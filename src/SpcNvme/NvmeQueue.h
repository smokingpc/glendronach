#pragma once

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

typedef struct _CMD_INFO
{
    //volatile USE_STATE InUsed = USE_STATE::FREE;
    bool InUsed = false;
    CMD_CTX_TYPE CtxType = CMD_CTX_TYPE::SRB;
    PVOID Context = NULL;           //SRBEXT of original request
    USHORT CID = NVME_INVALID_CID;  //CmdID in SubQ which submit this request.
    USHORT Tag = NVME_INVALID_CID;  //SCSI queue tag from miniport
}CMD_INFO, *PCMD_INFO;

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
    NTSTATUS Push(ULONG index, CMD_CTX_TYPE type, NVME_COMMAND* cmd, PVOID context, bool do_lock = false);
    NTSTATUS Pop(ULONG index, PCMD_INFO result, bool do_lock = false);

private:
    PVOID DevExt = NULL;
    PVOID Buffer = NULL;
    size_t BufferSize = 0;
    PCMD_INFO History = NULL;     //Cast of RawBuffer
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
    const static MEMORY_CACHING_TYPE CacheType = MEMORY_CACHING_TYPE::MmCached;
    CNvmeQueue();
    CNvmeQueue(QUEUE_PAIR_CONFIG* config);
    ~CNvmeQueue();

    NTSTATUS Setup(QUEUE_PAIR_CONFIG* config);
    void Teardown();
    
    NTSTATUS SubmitCmd(PNVME_COMMAND src_cmd, ULONG tag, SPCNVME_SRBEXT *srbext);
    NTSTATUS SubmitCmd(PNVME_COMMAND src_cmd, ULONG tag,  KEVENT *wait_event);
    //NTSTATUS SubmitCmd(PNVME_COMMAND src_cmd, ULONG tag, PVOID context, CMD_CTX_TYPE type);

    NTSTATUS CompleteCmd(PNVME_COMPLETION_ENTRY result, PVOID &context, CMD_CTX_TYPE &type);

    void GetQueueAddr(PVOID* subva, PHYSICAL_ADDRESS* subpa, PVOID* cplva, PHYSICAL_ADDRESS* cplpa);
    void GetQueueAddr(PVOID *subq, PVOID* cplq);
    void GetQueueAddr(PHYSICAL_ADDRESS* subq, PHYSICAL_ADDRESS* cplq);

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

    USHORT CID = NVME_INVALID_CID;
    QUEUE_TYPE Type = QUEUE_TYPE::IO_QUEUE;

    CCmdHistory History;

    ULONG SubTail = INVALID_DBL_VALUE;
    ULONG SubHead = INVALID_DBL_VALUE;
    ULONG CplHead = INIT_CPL_DBL_HEAD;
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
    void CmdCompleteDispatcher();
};
