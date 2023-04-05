#pragma once

typedef struct _DOORBELL_PAIR{
    NVME_SUBMISSION_QUEUE_TAIL_DOORBELL SubTail;
    NVME_COMPLETION_QUEUE_HEAD_DOORBELL CplHead;
}DOORBELL_PAIR, *PDOORBELL_PAIR;


//Using this class represents the DeviceExtension.
//Because memory allocation in kernel is still C style,
//the constructor and destructor are useless. They won't be called.
//So using Setup() and Teardown() to replace them.
class CNvmeDevice {
public:
    static const ULONG BUGCHECK_BASE = 0x23939889;          //pizzahut....  XD
    static const ULONG BUGCHECK_ADAPTER = BUGCHECK_BASE + 1;            //adapter has some problem. e.g. CSTS.CFS==1
    static const ULONG BUGCHECK_INVALID_STATE = BUGCHECK_BASE + 2;      //if action in invalid controller state, fire this bugcheck
    static const ULONG BUGCHECK_NOT_IMPLEMENTED = BUGCHECK_BASE + 10;
    static const ULONG DEV_POOL_TAG = (ULONG) 'veDN';

public:
    NTSTATUS Setup(PPORT_CONFIGURATION_INFORMATION pci);
    void Teardown();
    void DoQueueCplByDPC(ULONG msix_msgid);

    NTSTATUS EnableController();
    NTSTATUS DisableController();
    NTSTATUS ShutdownController();  //set CC.SHN and wait CSTS.SHST==2

    NTSTATUS InitController();      //for FindAdapter
    NTSTATUS RestartController();   //for AdapterControl's ScsiRestartAdaptor

    NTSTATUS RegisterIoQ(PSPCNVME_SRBEXT srbext);//, bool poll = false);
    NTSTATUS UnregisterIoQ(PSPCNVME_SRBEXT srbext);//, bool poll = false);

    NTSTATUS InitIdentifyCtrl();
    NTSTATUS InitIdentifyNS();
    NTSTATUS InitIdentifyFirstNS();
    NTSTATUS InitCreateIoQueues();

    NTSTATUS IdentifyController(PSPCNVME_SRBEXT srbext, PNVME_IDENTIFY_CONTROLLER_DATA ident, bool poll = false);
    NTSTATUS IdentifyNamespace(PSPCNVME_SRBEXT srbext, ULONG nsid, PNVME_IDENTIFY_NAMESPACE_DATA data, bool poll = false);
    //nsid_list : variable to store query result. It's size should be PAGE_SIZE.(NVMe max support 1024 NameSpace)
    NTSTATUS IdentifyActiveNamespaceIdList(PSPCNVME_SRBEXT srbext, PVOID nsid_list, ULONG &ret_count, bool poll = false);

    NTSTATUS SetNumberOfIoQueue(USHORT count);  //tell NVMe device: I want xx i/o queues. then device reply: I can allow you use xxxx queues.
    NTSTATUS SetInterruptCoalescing();
    NTSTATUS SetAsyncEvent();
    NTSTATUS SetArbitration();
    NTSTATUS SetSyncHostTime();
    NTSTATUS SetPowerManagement();
    NTSTATUS GetLbaFormat(ULONG nsid, NVME_LBA_FORMAT &format);
    NTSTATUS GetNamespaceBlockSize(ULONG nsid, ULONG& size);    //get LBA block size in Bytes
    NTSTATUS GetNamespaceTotalBlocks(ULONG nsid, ULONG64& blocks);    //get LBA total block count of specified namespace.
    NTSTATUS SubmitAdmCmd(PSPCNVME_SRBEXT srbext, PNVME_COMMAND cmd);
    NTSTATUS SubmitIoCmd(PSPCNVME_SRBEXT srbext, PNVME_COMMAND cmd);
    bool IsInValidIoRange(ULONG nsid, ULONG64 offset, ULONG len);

    bool IsWorking();
    bool IsSetup();
    bool IsTeardown();
    bool IsStop();

    ULONG MinPageSize;
    ULONG MaxPageSize;
    ULONG MaxTxSize;
    ULONG MaxTxPages;
    NVME_STATE State;
    ULONG RegisteredIoQ = 0;
    ULONG AllocatedIoQ = 0;
    ULONG DesiredIoQ = 0;

    ULONG DeviceTimeout;        //should be updated by CAP, unit in micro-seconds
    ULONG StallDelay;

    ACCESS_RANGE AccessRanges[ACCESS_RANGE_COUNT];         //AccessRange from miniport HwFindAdapter.
    ULONG AccessRangeCount;
    ULONG Bar0Size;
    UCHAR MaxNamespaces;
    USHORT IoDepth;
    USHORT AdmDepth;
    ULONG TotalNumaNodes;
    ULONG NamespaceCount;       //how many namespace active in current device?
    
    UCHAR CoalescingThreshold;  //how many interrupt should be coalesced into one interrupt?
    UCHAR CoalescingTime;       //how long(time) should interrupts be coalesced?

    USHORT  VendorID;
    USHORT  DeviceID;
    ULONG   CpuCount;

    STOR_DPC QueueCplDpc;
    NVME_VERSION                        NvmeVer;
    NVME_CONTROLLER_CAPABILITIES        CtrlCap;
    NVME_IDENTIFY_CONTROLLER_DATA       CtrlIdent;
    NVME_IDENTIFY_NAMESPACE_DATA        NsData[NVME_CONST::SUPPORT_NAMESPACES];

private:
    PNVME_CONTROLLER_REGISTERS          CtrlReg;
    PPORT_CONFIGURATION_INFORMATION     PortCfg;
    ULONG *Doorbells;
    PMSIX_TABLE_ENTRY MsixTable;
    CNvmeQueue* AdmQueue;
    CNvmeQueue* IoQueue[MAX_IO_QUEUE_COUNT];

    //Following are huge data.
    //for more convenient windbg debugging, I put them on tail of class data.
    PCI_COMMON_CONFIG                   PciCfg;

    void InitVars();
    void LoadRegistry();

    NTSTATUS CreateAdmQ();
    NTSTATUS RegisterAdmQ();
    NTSTATUS UnregisterAdmQ();
    NTSTATUS DeleteAdmQ();

    NTSTATUS CreateIoQ();   //create all IO queue
    NTSTATUS DeleteIoQ();   //delete all IO queue

    void ReadCtrlCap();      //load capability and informations AFTER register address mapped.
    bool MapCtrlRegisters();
    //bool GetMsixTable();
    bool GetPciBusData(INTERFACE_TYPE type, ULONG bus, ULONG slot);

    bool WaitForCtrlerState(ULONG time_us, BOOLEAN csts_rdy);
    bool WaitForCtrlerState(ULONG time_us, BOOLEAN csts_rdy, BOOLEAN cc_en);
    bool WaitForCtrlerShst(ULONG time_us);

    void ReadNvmeRegister(NVME_CONTROLLER_CONFIGURATION& cc, bool barrier = true);
    void ReadNvmeRegister(NVME_CONTROLLER_STATUS& csts, bool barrier = true);
    void ReadNvmeRegister(NVME_VERSION &ver, bool barrier = true);
    void ReadNvmeRegister(NVME_CONTROLLER_CAPABILITIES &cap, bool barrier = true);
    void ReadNvmeRegister(NVME_ADMIN_QUEUE_ATTRIBUTES& aqa,
                        NVME_ADMIN_SUBMISSION_QUEUE_BASE_ADDRESS& asq,
                        NVME_ADMIN_COMPLETION_QUEUE_BASE_ADDRESS& acq,
                        bool barrier = true);

    void WriteNvmeRegister(NVME_CONTROLLER_CONFIGURATION& cc, bool barrier = true);
    void WriteNvmeRegister(NVME_CONTROLLER_STATUS& csts, bool barrier = true);
    void WriteNvmeRegister(NVME_ADMIN_QUEUE_ATTRIBUTES &aqa, 
                        NVME_ADMIN_SUBMISSION_QUEUE_BASE_ADDRESS &asq, 
                        NVME_ADMIN_COMPLETION_QUEUE_BASE_ADDRESS &acq, 
                        bool barrier = true);
    
    void GetAdmQueueDbl(PNVME_SUBMISSION_QUEUE_TAIL_DOORBELL &sub, PNVME_COMPLETION_QUEUE_HEAD_DOORBELL &cpl);
    void GetQueueDbl(ULONG qid, PNVME_SUBMISSION_QUEUE_TAIL_DOORBELL& sub, PNVME_COMPLETION_QUEUE_HEAD_DOORBELL& cpl);

    BOOLEAN IsControllerEnabled(bool barrier = true);
    BOOLEAN IsControllerReady(bool barrier = true);
    void UpdateMaxTxSize();
    void DoQueueCompletion(CNvmeQueue* queue);
};

