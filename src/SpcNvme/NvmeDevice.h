#pragma once

typedef struct _SPCNVME_CONFIG {
    ULONG MaxTxSize = NVME_CONST::TX_SIZE;
    ULONG MaxTxPages = NVME_CONST::TX_PAGES;
    UCHAR MaxNamespaces = NVME_CONST::SUPPORT_NAMESPACES;
    UCHAR MaxTargets = NVME_CONST::MAX_TARGETS;
    UCHAR MaxLu = NVME_CONST::MAX_LU;
    ULONG MaxIoPerLU = NVME_CONST::MAX_IO_PER_LU;
    ULONG MaxTotalIo = NVME_CONST::MAX_IO_PER_LU * NVME_CONST::MAX_LU;
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


class CNvmeDevice {
public:
    static const ULONG BUGCHECK_BASE = 0x85157300;
    static const ULONG BUGCHECK_ADAPTER = BUGCHECK_BASE+1;
    static const ULONG BUGCHECK_NOT_IMPLEMENTED = BUGCHECK_BASE+2;

    static CNvmeDevice* Create(PVOID devext, PSPCNVME_CONFIG cfg);
    static void Delete(CNvmeDevice* ptr);

public:
    CNvmeDevice();
    CNvmeDevice(PVOID devext, PSPCNVME_CONFIG cfg);
    ~CNvmeDevice();

    void Setup(PVOID devext, PSPCNVME_CONFIG cfg);
    void Teardown();

    bool EnableController();
    bool DisableController();

    inline bool GetAdmDoorbell(PNVME_SUBMISSION_QUEUE_TAIL_DOORBELL *subdbl, PNVME_COMPLETION_QUEUE_HEAD_DOORBELL *cpldbl)
    { return GetDoorbell(0, subdbl, cpldbl); }
    //qid == 0 is always AdminQ. IoQueue QID is 1 based.
    bool GetDoorbell(ULONG qid, PNVME_SUBMISSION_QUEUE_TAIL_DOORBELL* subdbl, PNVME_COMPLETION_QUEUE_HEAD_DOORBELL* cpldbl);

    //void UpdateIdentifyData(PNVME_IDENTIFY_CONTROLLER_DATA data);
    bool RegisterAdminQueuePair(CNvmeQueuePair* qp);
    bool UnregisterAdminQueuePair();
    bool RegisterIoQueuePair(CNvmeQueuePair* qp);
    bool UnregisterIoQueuePair(CNvmeQueuePair* qp);

private:
    PNVME_CONTROLLER_REGISTERS          CtrlReg = NULL;
    NVME_VERSION NvmeVer = {0};
    NVME_CONTROLLER_CAPABILITIES NvmeCap = {0};
    //NVME_IDENTIFY_CONTROLLER_DATA       IdentData = { 0 };
    PDOORBELL_PAIR                      Doorbells = NULL;

    ULONG BusNumber = NVME_INVALID_ID;
    ULONG SlotNumber = NVME_INVALID_ID;
    
    PVOID DevExt = NULL;      //StorPort Device Extension 
    ULONG MaxDblCount = 0;

    ULONG CtrlerTimeout = 2000 * NVME_CONST::STALL_INTERVAL_US;        //should be updated by CAP, unit in micro-seconds
    ULONG StallDelay = NVME_CONST::SLEEP_TIME_US;
    CNvmeQueuePair  AdminQueue;
    CNvmeQueuePair  *IoQueue[MAX_IO_QUEUE_COUNT] = {NULL};

    SPCNVME_CONFIG  Cfg;
    USHORT  VendorID;
    USHORT  DeviceID;
    bool IsReady = false;
    
    void ReadCtrlCapAndInfo();      //load capability and informations AFTER register address mapped.
    bool MapControllerRegisters(PCI_COMMON_HEADER& header);
    bool GetPciBusData(PCI_COMMON_HEADER& header);

    bool WaitForCtrlerState(ULONG time_us, BOOLEAN csts_rdy);
    bool WaitForCtrlerState(ULONG time_us, BOOLEAN csts_rdy, BOOLEAN cc_en);

    void ReadNvmeRegister(NVME_CONTROLLER_CONFIGURATION& cc, bool barrier = true);
    void ReadNvmeRegister(NVME_CONTROLLER_STATUS& csts, bool barrier = true);
    void ReadNvmeRegister(NVME_VERSION &ver, bool barrier = true);
    void ReadNvmeRegister(NVME_CONTROLLER_CAPABILITIES &cap, bool barrier = true);

    void WriteNvmeRegister(NVME_CONTROLLER_CONFIGURATION& cc, bool barrier = true);
    void WriteNvmeRegister(NVME_CONTROLLER_STATUS& csts, bool barrier = true);
    void WriteNvmeRegister(NVME_ADMIN_QUEUE_ATTRIBUTES &aqa, 
                        NVME_ADMIN_SUBMISSION_QUEUE_BASE_ADDRESS &asq, 
                        NVME_ADMIN_COMPLETION_QUEUE_BASE_ADDRESS &acq, 
                        bool barrier = true);

    BOOLEAN IsControllerEnabled(bool barrier = true);
    BOOLEAN IsControllerReady(bool barrier = true);

    //void TakeSnooze(ULONG interval = NVME_CONST::SLEEP_TIME_US);
};

