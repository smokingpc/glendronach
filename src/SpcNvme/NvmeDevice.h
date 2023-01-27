#pragma once

#pragma push(1)
typedef struct _MSIX_VECTOR
{
    PHYSICAL_ADDRESS MsgAddress;
    ULONG MsgData;
    struct {
        ULONG Mask : 1;
        ULONG Reserved : 31;
    };
}MsixVector, *PMsixVector;
#pragma pop()

//typedef struct _SPCNVME_CONFIG {
//    ULONG MaxTxSize = NVME_CONST::TX_SIZE;
//    ULONG MaxTxPages = NVME_CONST::TX_PAGES;
//    UCHAR MaxNamespaces = NVME_CONST::SUPPORT_NAMESPACES;
//    UCHAR MaxTargets = NVME_CONST::MAX_TARGETS;
//    UCHAR MaxLu = NVME_CONST::MAX_LU;
//    ULONG MaxIoPerLU = NVME_CONST::MAX_IO_PER_LU;
//    ULONG MaxTotalIo = NVME_CONST::MAX_IO_PER_LU * NVME_CONST::MAX_LU;
//    INTERFACE_TYPE InterfaceType = PCIBus;
//    
//    ULONG BusNumber = NVME_INVALID_ID;
//    ULONG SlotNumber = NVME_INVALID_ID;
//    ACCESS_RANGE AccessRanges[ACCESS_RANGE_COUNT] = {0};         //AccessRange from miniport HwFindAdapter.
//    ULONG AccessRangeCount = 0;
//
//    _SPCNVME_CONFIG(){}
//    _SPCNVME_CONFIG(ULONG bus, ULONG slot, ACCESS_RANGE buffer[], ULONG range_count)
//    {
//        BusNumber = bus;
//        SlotNumber = slot;
//        SetAccessRanges(buffer, range_count);
//    }
//
//    bool SetAccessRanges(ACCESS_RANGE buffer[], ULONG count)
//    {
//        if(count > ACCESS_RANGE_COUNT)
//            return false;
//
//        AccessRangeCount = count;
//        RtlCopyMemory(AccessRanges, buffer, sizeof(ACCESS_RANGE) * count);
//
//        return true;
//    }
//}SPCNVME_CONFIG, *PSPCNVME_CONFIG;

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
    //static CNvmeDevice* Create(PVOID devext, PSPCNVME_CONFIG cfg);
    //static void Delete(CNvmeDevice* ptr);

public:
    NTSTATUS Setup(PPORT_CONFIGURATION_INFORMATION pci);
    void Teardown();

    NTSTATUS EnableController();
    NTSTATUS DisableController();

    NTSTATUS InitController(bool wait = true);      //for FindAdapter
    NTSTATUS RestartController();   //for AdapterControl's ScsiRestartAdaptor

    NTSTATUS IdentifyController(PSPCNVME_SRBEXT srbext);
    NTSTATUS IdentifyNamespace(bool wait = true);
    NTSTATUS SetInterruptCoalescing(bool wait = true);
    NTSTATUS SetAsyncEvent(bool wait = true);
    NTSTATUS SetArbitration(bool wait = true);
    NTSTATUS SetSyncHostTime(bool wait = true);
    NTSTATUS SetPowerManagement(bool wait = true);

    NTSTATUS RegisterAdmQ();
    NTSTATUS UnregisterAdmQ();
    NTSTATUS CreateIoQueue();   //create one more IO queue and register it
    NTSTATUS RegisterIoQ();
    NTSTATUS UnregisterIoQ();
    NTSTATUS DeleteIoQueue();   //unregister and delete one IO queue.
private:
    PNVME_CONTROLLER_REGISTERS          CtrlReg;
    PPORT_CONFIGURATION_INFORMATION     PortCfg;
    ULONG *Doorbells;
    MsixVector *MsixVectors;
    CNvmeQueue* AdmQueue;
    CNvmeQueue* IoQueue[MAX_IO_QUEUE_COUNT];

    USHORT  VendorID;
    USHORT  DeviceID;
    BOOLEAN IsReady = FALSE;
    NVME_STATE State;
    ULONG CpuCount;
    ULONG TotalNumaNodes;
    ULONG RegisteredIoQ = 0;
    ULONG DesiredIoQ = 0;
    ULONG DeviceTimeout = 2000 * NVME_CONST::STALL_TIME_US;        //should be updated by CAP, unit in micro-seconds
    ULONG StallDelay = NVME_CONST::SLEEP_TIME_US;

    ACCESS_RANGE AccessRanges[ACCESS_RANGE_COUNT];         //AccessRange from miniport HwFindAdapter.
    ULONG AccessRangeCount;
    ULONG Bar0Size;
    ULONG MaxTxSize;
    ULONG MaxTxPages;
    UCHAR MaxNamespaces;
    UCHAR MaxTargets;
    UCHAR MaxLu;
    ULONG MaxIoPerLU;
    ULONG MaxTotalIo;

    ULONG AdmDepth;
    ULONG IoDepth;

    //Following are huge data.
    //for more convenient windbg debugging, I put them on tail of class data.
    PCI_COMMON_CONFIG                   PciCfg;
    NVME_VERSION                        NvmeVer;
    NVME_CONTROLLER_CAPABILITIES        CtrlCap;
    NVME_IDENTIFY_CONTROLLER_DATA       CtrlIdent;
    NVME_IDENTIFY_NAMESPACE_DATA NsData[NVME_CONST::SUPPORT_NAMESPACES];

    void InitVars();
    void LoadRegistry();

    NTSTATUS CreateAdmQ();
    NTSTATUS RegisterAdmQ();
    NTSTATUS UnregisterAdmQ();
    NTSTATUS DeleteAdmQ();

    NTSTATUS AddIoQ();      //create one more IO queue
    NTSTATUS RegisterIoQ();
    NTSTATUS UnregisterIoQ();
    NTSTATUS RemoveIoQ();   //delete last one IO queue. If it's still registered this function return fail.

    void ReadCtrlCap();      //load capability and informations AFTER register address mapped.
    bool MapCtrlRegisters();
    bool GetPciBusData();

    bool WaitForCtrlerState(ULONG time_us, BOOLEAN csts_rdy);
    bool WaitForCtrlerState(ULONG time_us, BOOLEAN csts_rdy, BOOLEAN cc_en);

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

    BOOLEAN IsControllerEnabled(bool barrier = true);
    BOOLEAN IsControllerReady(bool barrier = true);

    //void TakeSnooze(ULONG interval = NVME_CONST::SLEEP_TIME_US);
};

