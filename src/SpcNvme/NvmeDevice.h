#pragma once

typedef struct _SPCNVME_CONFIG {
    static const ULONG DEFAULT_TX_PAGES = 512; //PRP1 + PRP2 MAX PAGES
    static const ULONG DEFAULT_TX_SIZE = PAGE_SIZE * DEFAULT_TX_PAGES;
    static const UCHAR SUPPORT_NAMESPACES = 1;
    static const UCHAR MAX_TARGETS = 1;
    static const UCHAR MAX_LU = 1;
    static const ULONG MAX_IO_PER_LU = 1024;
    static const UCHAR IOSQES = 6; //sizeof(NVME_COMMAND)==64 == 2^6, so IOSQES== 6.
    static const UCHAR IOCQES = 4; //sizeof(NVME_COMPLETION_ENTRY)==16 == 2^4, so IOCQES== 4.
    static const UCHAR INTCOAL_TIME = 10;   //Interrupt Coalescing time threshold in 100us unit.
    static const UCHAR INTCOAL_THRESHOLD = 8;   //Interrupt Coalescing trigger threshold.


    ULONG MaxTxSize = DEFAULT_TX_SIZE;
    ULONG MaxTxPages = DEFAULT_TX_PAGES;
    UCHAR MaxNamespaces = SUPPORT_NAMESPACES;
    UCHAR MaxTargets = MAX_TARGETS;
    UCHAR MaxLu = MAX_LU;
    ULONG MaxIoPerLU = MAX_IO_PER_LU;
    ULONG MaxTotalIo = MAX_IO_PER_LU * MAX_LU;
    INTERFACE_TYPE InterfaceType = PCIBus;
    
    ULONG BusNumber = NVME_INVALID_ID;
    ULONG SlotNumber = NVME_INVALID_ID;
    ACCESS_RANGE AccessRanges[ACCESS_RANGE_COUNT] = {0};         //AccessRange from miniport HwFindAdapter.
    ULONG AccessRangeCount = 0;
    bool SetAccessRanges(ACCESS_RANGE buffer[], ULONG count)
    {
        if(count > ACCESS_RANGE_COUNT)
            return false;

        AccessRangeCount = count;
        RtlCopyMemory(AccessRanges, buffer, sizeof(ACCESS_RANGE) * count);

        return true;
    }
}SPCNVME_CONFIG, *PSPCNVME_CONFIG;

typedef struct _DOORBELL_PAIR{
    NVME_SUBMISSION_QUEUE_TAIL_DOORBELL SubTail;
    NVME_COMPLETION_QUEUE_HEAD_DOORBELL CplHead;
}DOORBELL_PAIR, *PDOORBELL_PAIR;


class CSpcNvmeDevice {
public:
    static CSpcNvmeDevice *Create(PVOID devext, PSPCNVME_CONFIG cfg);
    static void Delete(CSpcNvmeDevice *ptr);
    static const ULONG STALL_INTERVAL_US = 500;
    //static const ULONG WAIT_STATE_TIMEOUT_US = 20* STALL_INTERVAL_US;
    static const ULONG BUGCHECK_BASE = 0x85157300;
    static const ULONG BUGCHECK_ADAPTER = BUGCHECK_BASE+1;
    static const ULONG BUGCHECK_NOT_IMPLEMENTED = BUGCHECK_BASE+2;

    static const USHORT ADM_QDEPTH = 128;

    static const USHORT SQ_CMD_SIZE = sizeof(NVME_COMMAND);
    static const USHORT SQ_CMD_SIZE_SHIFT = 6; //sizeof(NVME_COMMAND) is 64 bytes == 2^6

public:
    CSpcNvmeDevice();
    CSpcNvmeDevice(PVOID devext, PSPCNVME_CONFIG cfg);
    ~CSpcNvmeDevice();

    void Setup(PVOID devext, PSPCNVME_CONFIG cfg);
    void Teardown();
//    void UpdateIdentifyData(PNVME_IDENTIFY_CONTROLLER_DATA data);
//    void SetMaxIoQueueCount(ULONG max);

    void DoAdminCompletion();       //called when AdminQueue Completion Interrupt invoked
    void DoIoCompletion();          //called when IoQueue Completion Interrupt invoked

    bool EnableController();
    bool DisableController();

    //Controller Admin Commands
    //bool IdentifyController();
    //bool IdentifyNamespace();

    bool CreateIoSubQ();
    bool DeleteIoSubQ();
    bool CreateIoCplQ();
    bool DeleteIoCplQ();

    bool SetInterruptCoalescing();
    bool SetArbitration();
    bool SyncHostTime();
    bool SetPowerManagement();
    bool SetAsyncEvent();

    bool SubmitAdminCmd(PSPCNVME_SRBEXT srbext, PNVME_COMMAND cmd);
    bool SubmitIoCmd(PSPCNVME_SRBEXT srbext, PNVME_COMMAND cmd);

private:
    PNVME_CONTROLLER_REGISTERS          CtrlReg = NULL;
    NVME_IDENTIFY_CONTROLLER_DATA       IdentData = { 0 };
    PDOORBELL_PAIR                      Doorbells = NULL;

    ULONG BusNumber = NVME_INVALID_ID;
    ULONG SlotNumber = NVME_INVALID_ID;
    
    PVOID DevExt = NULL;      //StorPort Device Extension 
    ULONG IoQueueCount = 0;
    ULONG MaxIoQueueCount = 0;      //calculate by device returned config
    
    ULONG CtrlerTimeout = 2000 * STALL_INTERVAL_US;        //should be updated by CAP, unit in micro-seconds
    CNvmeQueuePair  AdminQueue;
    CNvmeQueuePair  *IoQueue[MAX_IO_QUEUE_COUNT] = {NULL};

    SPCNVME_CONFIG  Cfg;
    USHORT  VendorID;
    USHORT  DeviceID;
    bool IsReady = false;
    
    void RefreshByCapability();
    bool MapControllerRegisters(PCI_COMMON_HEADER& header);
    bool GetPciBusData(PCI_COMMON_HEADER& header);

    bool SetupAdminQueuePair();
    bool RegisterAdminQueuePair();
    
    bool CreateIoQueuePairs();
    bool RegisterIoQueuePairs();
    bool RegisterIoQueuePair(ULONG index);

    bool WaitForCtrlerState(ULONG time_us, BOOLEAN csts_rdy);
    bool WaitForCtrlerState(ULONG time_us, BOOLEAN csts_rdy, BOOLEAN cc_en);

    bool SubmitNvmeCommand(PSPCNVME_SRBEXT srbext, PNVME_COMMAND cmd, ULONG qid);
    bool SubmitNvmeCommand(OUT PNVME_COMPLETION_ENTRY completion, PSPCNVME_SRBEXT srbext, PNVME_COMMAND cmd, ULONG qid);


    inline void ReadRegisters(NVME_CONTROLLER_STATUS &csts, NVME_CONTROLLER_CONFIGURATION &cc)
    {
        MemoryBarrier();
        csts.AsUlong = StorPortReadRegisterUlong(DevExt, &CtrlReg->CSTS.AsUlong);
        cc.AsUlong = StorPortReadRegisterUlong(DevExt, &CtrlReg->CC.AsUlong);
    }
    inline void ReadRegisters(NVME_CONTROLLER_STATUS& csts)
    {
        MemoryBarrier();
        csts.AsUlong = StorPortReadRegisterUlong(DevExt, &CtrlReg->CSTS.AsUlong);
    }
};

