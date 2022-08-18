#pragma once

typedef enum _QUEUE_TYPE
{
    UNKNOWN = 0,
    ADMIN = 1,
    NORMAL_IO = 2,
    INTERNAL_IO = 3,
}QUEUE_TYPE;

typedef struct _QUEUE_PAIR_CONFIG {
    PVOID DevExt = NULL;
    USHORT QID = 0;
    USHORT Depth = 0;
    ULONG NumaNode = 0;
    QUEUE_TYPE Type = QUEUE_TYPE::UNKNOWN;
    ULONG *SubDbl = NULL;
    ULONG* CplDbl = NULL;
}QUEUE_PAIR_CONFIG, * PQUEUE_PAIR_CONFIG;

typedef struct _QUEUE_CONFIG : _QUEUE_PAIR_CONFIG
{
    PVOID QueueBuffer = NULL;
    size_t QueueBufferSize = 0;
}QUEUE_CONFIG, *PQUEUE_CONFIG;

class CNvmeDoorbell
{
public:

private:
    ULONG *DblRegister = NULL;
    ULONG OldDbl = 0;
};

class CNvmeSubmitQueue
{
    friend class CNvmeQueue;
public:
    CNvmeSubmitQueue();
    CNvmeSubmitQueue(PQUEUE_CONFIG config);
    ~CNvmeSubmitQueue();

    void Setup(PQUEUE_CONFIG config);
    void Teardown();
    operator STOR_PHYSICAL_ADDRESS() const;
    bool SubmitCmd(PNVME_COMMAND new_cmd);
    void UpdateSubQHead(USHORT new_head);
private:
    PVOID DevExt = NULL;
    PVOID RawBuffer = NULL;                 //RawBuffer should be PAGE_ALIGNED
    size_t RawBufferSize = 0;               
    STOR_PHYSICAL_ADDRESS QueuePA = {0};    //Physical Address of RawBuffer. Used to register in NVMe device.
    PNVME_COMMAND Cmds = NULL;              //Cast of RawBuffer
    USHORT Depth = 0;                       //how many items in this->Cmds ?
    USHORT QueueID = NVME_INVALID_QID;
    ULONG NumaNode = 0;
    QUEUE_TYPE Type = QUEUE_TYPE::UNKNOWN;
    USHORT DblTail = INVALID_DBL_TAIL;
    USHORT DblHead = INVALID_DBL_HEAD;
    volatile PNVME_SUBMISSION_QUEUE_TAIL_DOORBELL Doorbell = NULL;
};

class CNvmeCompleteQueue
{
    friend class CNvmeQueue;
public:
    CNvmeCompleteQueue();
    CNvmeCompleteQueue(PQUEUE_CONFIG config);
    ~CNvmeCompleteQueue();
    void Setup(PQUEUE_CONFIG config);
    void Teardown();
    operator STOR_PHYSICAL_ADDRESS() const;
    bool PopCplEntry(NVME_COMPLETION_ENTRY &popdata);

private:
    PVOID DevExt = NULL;
    PVOID RawBuffer = NULL;
    STOR_PHYSICAL_ADDRESS QueuePA = { 0 };
    size_t RawBufferSize = 0;
    PNVME_COMPLETION_ENTRY Entries = NULL;  //Cast of RawBuffer
    USHORT Depth = 0;                       //how many items in this->Entries ?
    USHORT QueueID = NVME_INVALID_QID;
    ULONG NumaNode = 0;
    ULONG DblHead = INIT_CPL_DBL_HEAD;
    QUEUE_TYPE Type = QUEUE_TYPE::UNKNOWN;

    struct {
        USHORT Tag :1;
        USHORT Reserved:15;
    }Phase;

    volatile PNVME_COMPLETION_QUEUE_HEAD_DOORBELL Doorbell = NULL;
};

class CNvmeSrbExtHistory
{
    friend class CNvmeQueue;
public:
    const ULONG BufferTag = (ULONG)'HBRS';

    CNvmeSrbExtHistory();
    CNvmeSrbExtHistory(PVOID devext, USHORT qid, USHORT depth, ULONG numa_node = 0);
    ~CNvmeSrbExtHistory();

    void Setup(PVOID devext, USHORT qid, USHORT depth, ULONG numa_node = 0);
    void Teardown();
    bool Put(int index, PSPCNVME_SRBEXT srbext);
    PSPCNVME_SRBEXT Get(int index);
private:
    PVOID DevExt = NULL;
    PVOID RawBuffer = NULL;
    size_t RawBufferSize = 0;
    PSPCNVME_SRBEXT *History = NULL;     //Cast of RawBuffer
    USHORT Depth = 0;                   //how many items in this->History ?
    USHORT QueueID = NVME_INVALID_QID;
    ULONG NumaNode = 0;
};

class CNvmeQueuePair
{
public:
    const ULONG BufferTag = (ULONG)'QMVN';
    CNvmeQueuePair();
    CNvmeQueuePair(QUEUE_PAIR_CONFIG *config);
    ~CNvmeQueuePair();

    void Setup(QUEUE_PAIR_CONFIG* config);
    void Teardown();
private:
    PVOID DevExt = NULL;
    USHORT QueueID = NVME_INVALID_QID;
    USHORT Depth = 0;       //how many entries in both SubQ and CplQ?
    ULONG* SubQDbl = NULL;  //Doorbell of SubQ
    ULONG* CplQDbl = NULL;  //Doorbell of CplQ
    ULONG NumaNode = 0;
    
    //In CNvmeQueuePair, it allocates SubQ and CplQ in one large continuous block.
    //QueueBuffer is pointer of this large block.
    //Then divide into 2 blocks for SubQ and CplQ.
    PCHAR QueueBuffer = NULL;
    size_t QueueBufferSize = 0;

    //ULONG MsiMsg = 0;       //MSI interrupt message to trigger CplQ
    QUEUE_TYPE Type = QUEUE_TYPE::UNKNOWN;
    bool IsReady = false;
    CNvmeSubmitQueue SubQ;
    CNvmeCompleteQueue CplQ;
    CNvmeSrbExtHistory SrbExts;

};


class CSpcNvmeDevice {
public:
    CSpcNvmeDevice();
    //CSpcNvmeDevice();
    ~CSpcNvmeDevice();

    void Setup();
    void Teardown();

private:
    PPORT_CONFIGURATION_INFORMATION     PciCfg = NULL;
    PNVME_CONTROLLER_REGISTERS          CtrlReg = NULL;
    //NVME_IDENTIFY_CONTROLLER_DATA       DevIdent = {0};
    NVME_CONTROLLER_CAPABILITIES        DevCap = { 0 };

    ULONG   BusNumber = NVME_INVALID_ID;
    ULONG   SlotNumber = NVME_INVALID_ID;
    
    STOR_DPC    AdmQCplDpc;
    NVME_VERSION    NvmeVer = {0};

    CNvmeQueuePair  AdminQueue;
    ULONG IoQueueCount = 0;
    ULONG MaxIoQueueCount = 0;      //calculate by device returned config
    CNvmeQueuePair  *IoQueue = NULL;

    void Init();
};