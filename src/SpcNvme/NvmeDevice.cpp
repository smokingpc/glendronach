#include "pch.h"

#pragma region ======== CSpcNvmeDevice Factory Methods ======== 
CNvmeDevice *CNvmeDevice::Create(PVOID devext, PSPCNVME_CONFIG cfg)
{
    CNvmeDevice *ret = new CNvmeDevice(devext, cfg);

    return ret;
}
void CNvmeDevice::Delete(CNvmeDevice* ptr)
{
    ptr->Teardown();
    delete ptr;
}
#pragma endregion

#pragma region ======== class CSpcNvmeDevice ======== 

inline void CNvmeDevice::ReadNvmeRegister(NVME_CONTROLLER_CONFIGURATION& cc, bool barrier)
{
    if(barrier)
        MemoryBarrier();
    cc.AsUlong = StorPortReadRegisterUlong(DevExt, &CtrlReg->CC.AsUlong);
}
inline void CNvmeDevice::ReadNvmeRegister(NVME_CONTROLLER_STATUS& csts, bool barrier)
{
    if (barrier)
        MemoryBarrier();
    csts.AsUlong = StorPortReadRegisterUlong(DevExt, &CtrlReg->CSTS.AsUlong);
}
inline void CNvmeDevice::ReadNvmeRegister(NVME_VERSION& ver, bool barrier)
{
    if (barrier)
        MemoryBarrier();
    ver.AsUlong = StorPortReadRegisterUlong(DevExt, &CtrlReg->VS.AsUlong);
}
inline void CNvmeDevice::ReadNvmeRegister(NVME_CONTROLLER_CAPABILITIES& cap, bool barrier)
{
    if (barrier)
        MemoryBarrier();
    cap.AsUlonglong = StorPortReadRegisterUlong64(DevExt, &CtrlReg->CAP.AsUlonglong);
}
inline void CNvmeDevice::WriteNvmeRegister(NVME_CONTROLLER_CONFIGURATION& cc, bool barrier)
{
    if (barrier)
        MemoryBarrier();
    StorPortWriteRegisterUlong(DevExt, &CtrlReg->CC.AsUlong, cc.AsUlong);
}
inline void CNvmeDevice::WriteNvmeRegister(NVME_CONTROLLER_STATUS& csts, bool barrier)
{
    if (barrier)
        MemoryBarrier();
    StorPortWriteRegisterUlong(DevExt, &CtrlReg->CSTS.AsUlong, csts.AsUlong);
}
inline void CNvmeDevice::WriteNvmeRegister(NVME_ADMIN_QUEUE_ATTRIBUTES& aqa,
    NVME_ADMIN_SUBMISSION_QUEUE_BASE_ADDRESS& asq,
    NVME_ADMIN_COMPLETION_QUEUE_BASE_ADDRESS& acq,
    bool barrier)
{
    if (barrier)
        MemoryBarrier();

    StorPortWriteRegisterUlong(DevExt, &CtrlReg->AQA.AsUlong, aqa.AsUlong);
    StorPortWriteRegisterUlong64(DevExt, &CtrlReg->ASQ.AsUlonglong, asq.AsUlonglong);
    StorPortWriteRegisterUlong64(DevExt, &CtrlReg->ACQ.AsUlonglong, acq.AsUlonglong);
}

inline BOOLEAN CNvmeDevice::IsControllerEnabled(bool barrier)
{
    NVME_CONTROLLER_CONFIGURATION cc = {0};
    ReadNvmeRegister(cc, barrier);
    return (TRUE == cc.EN)?TRUE:FALSE;
}
inline BOOLEAN CNvmeDevice::IsControllerReady(bool barrier)
{
    NVME_CONTROLLER_STATUS csts = { 0 };
    ReadNvmeRegister(csts, barrier);
    return (TRUE == csts.RDY)?TRUE:FALSE;
}

CNvmeDevice::CNvmeDevice(){}
CNvmeDevice::CNvmeDevice(PVOID devext, PSPCNVME_CONFIG cfg) 
    : CNvmeDevice()
{
    Setup(devext, cfg);
}
CNvmeDevice::~CNvmeDevice()
{
    Teardown();
}

void CNvmeDevice::Setup(PVOID devext, PSPCNVME_CONFIG cfg)
{
    bool map_ok = false;

    if(IsReady)
        return;

    DevExt = devext;
    RtlCopyMemory(&Cfg, cfg, sizeof(SPCNVME_CONFIG));
    PCI_COMMON_HEADER header = { 0 };
    if (GetPciBusData(header))
    {
        map_ok = MapControllerRegisters(header);
    }

    if(map_ok)
        ReadCtrlCapAndInfo();

    IsReady = map_ok;
}
void CNvmeDevice::Teardown()
{
    if(!IsReady)
        return ;

    DisableController();
    IsReady = false;
}

bool CNvmeDevice::EnableController()
{
    //if set CC.EN = 1 WHEN CSTS.RDY == 1 and CC.EN == 0, it is undefined behavior.
    //we should wait controller state changing until (CC.EN == 0 and CSTS.RDY == 0).
    bool ok = WaitForCtrlerState(CtrlerTimeout, FALSE, FALSE);
    if (!ok)
        KeBugCheckEx(BUGCHECK_ADAPTER, (ULONG_PTR)this, 0, 0, 0);

    //before Enable, update these basic information to controller.
    //these fields only can be modified when CC.EN == 0. (plz refer to nvme 1.3 spec)
    NVME_CONTROLLER_CONFIGURATION cc = { 0 };
    cc.CSS = NVME_CSS_NVM_COMMAND_SET;
    cc.AMS = NVME_AMS_ROUND_ROBIN;
    cc.SHN = NVME_CSTS_SHST_NO_SHUTDOWN;
    cc.IOSQES = NVME_CONST::IOSQES;
    cc.IOCQES = NVME_CONST::IOCQES;
    cc.EN = 0;
    WriteNvmeRegister(cc);

    //take a break let controller have enough time to retrieve CC values.
    StorPortStallExecution(StallDelay);

    cc.EN = 1;
    WriteNvmeRegister(cc);

    return WaitForCtrlerState(CtrlerTimeout, 1);
}
bool CNvmeDevice::DisableController()
{
    NVME_CONTROLLER_STATUS csts = { 0 };
    NVME_CONTROLLER_CONFIGURATION cc = { 0 };

#if 0
    csts.AsUlong = StorPortReadRegisterUlong(DevExt, &CtrlReg->CSTS.AsUlong);
    cc.AsUlong = StorPortReadRegisterUlong(DevExt, &CtrlReg->CC.AsUlong);

    if (csts.RDY == 0 && cc.EN == 0)
        return;

    //if set CC.EN = 0 WHEN CSTS.RDY == 0 and CC.EN == 1, it is undefined behavior.
    if (0 == csts.RDY && 1 == cc.EN)
    {
        //stall and wait our desired state become the correct value we want.
        bool ok = WaitForCtrlerState(CtrlerTimeout, 0, 0);
        //cc and csts not meet our desired state, the controller could have problem.
        if (!ok)
            KeBugCheckEx(BUGCHECK_ADAPTER, (ULONG_PTR)this, (ULONG_PTR)&cc, (ULONG_PTR)&csts, 0);
    }
    else if (1 == csts.RDY && 0 == cc.EN)   //abnormal state...
        KeBugCheckEx(BUGCHECK_ADAPTER, (ULONG_PTR)this, (ULONG_PTR)&cc, (ULONG_PTR)&csts, 0);
#endif

    //if set CC.EN = 0 WHEN CSTS.RDY == 0 and CC.EN == 1, it is undefined behavior.
    //we should wait controller state changing until (CC.EN == 1 and CSTS.RDY == 1).
    bool ok = WaitForCtrlerState(CtrlerTimeout, 1, 1);
    if (!ok)
        KeBugCheckEx(BUGCHECK_ADAPTER, (ULONG_PTR)this, 0, 0, 0);

    ReadNvmeRegister(cc);
    cc.EN = FALSE;
    cc.SHN = NVME_CSTS_SHST_NO_SHUTDOWN;
    WriteNvmeRegister(cc);

    return WaitForCtrlerState(CtrlerTimeout, 0);
}

bool CNvmeDevice::GetDoorbell(ULONG qid, PNVME_SUBMISSION_QUEUE_TAIL_DOORBELL* subdbl, 
                                        PNVME_COMPLETION_QUEUE_HEAD_DOORBELL* cpldbl)
{
    if(qid >= MaxDblCount)
        return false;

    *subdbl = &Doorbells[qid].SubTail;
    *cpldbl = &Doorbells[qid].CplHead;
    return true;
}

bool CNvmeDevice::RegisterAdminQueuePair(CNvmeQueue* qp)
{
    if(IsControllerEnabled())
    {
        if(false == WaitForCtrlerState(CtrlerTimeout, FALSE, FALSE))
            return false;
    }

    NVME_ADMIN_QUEUE_ATTRIBUTES aqa = { 0 };
    NVME_ADMIN_SUBMISSION_QUEUE_BASE_ADDRESS asq = { 0 };
    NVME_ADMIN_COMPLETION_QUEUE_BASE_ADDRESS acq = { 0 };

    //tell controller: how many entries in my Admin Sub and Cpl Queue?
    //these attributes should be write to controller ONLY WHEN CC.EN == 0.
    aqa.ASQS = NVME_CONST::ADMIN_QUEUE_DEPTH;
    aqa.ACQS = NVME_CONST::ADMIN_QUEUE_DEPTH;

    PHYSICAL_ADDRESS subq_pa = { 0 };
    PHYSICAL_ADDRESS cplq_pa = { 0 };
    qp->GetQueueAddr(&subq_pa, &cplq_pa);
    asq.AsUlonglong = subq_pa.QuadPart;
    acq.AsUlonglong = cplq_pa.QuadPart;

    WriteNvmeRegister(aqa, asq, acq);
    return true;
}
bool CNvmeDevice::UnregisterAdminQueuePair()
{
    if (IsControllerEnabled())
    {
        if (false == WaitForCtrlerState(CtrlerTimeout, FALSE, FALSE))
            return false;
    }

    NVME_ADMIN_QUEUE_ATTRIBUTES aqa = { 0 };
    NVME_ADMIN_SUBMISSION_QUEUE_BASE_ADDRESS asq = { 0 };
    NVME_ADMIN_COMPLETION_QUEUE_BASE_ADDRESS acq = { 0 };

    WriteNvmeRegister(aqa, asq, acq);
    return true;
}
bool CNvmeDevice::RegisterIoQueuePair(CNvmeQueue* qp)
{
    UNREFERENCED_PARAMETER(qp);
    return false;
}
bool CNvmeDevice::UnregisterIoQueuePair(CNvmeQueue* qp)
{
    UNREFERENCED_PARAMETER(qp);
    return false;
}

void CNvmeDevice::ReadCtrlCapAndInfo()
{
    //CtrlReg->CAP.TO is timeout value in 500 ms.
    //It indicates the timeout worst case of Enabling/Disabling Controller.
    //e.g. CAP.TO==3 means worst case timout to Enable/Disable controller is 1500 milli-second.
    //We should convert it to micro-seconds for StorPortStallExecution() using.
    
    ReadNvmeRegister(NvmeVer);
    ReadNvmeRegister(NvmeCap);
    CtrlerTimeout = ((UCHAR)NvmeCap.TO) * (500 * 1000);

}
bool CNvmeDevice::MapControllerRegisters(PCI_COMMON_HEADER &header)
{
    if(NULL == DevExt)
        return false;

    BOOLEAN in_iospace = FALSE;

    STOR_PHYSICAL_ADDRESS bar0 = { 0 };
    CtrlReg = NULL;
    Doorbells = NULL;
    
    //todo: use correct structure to parse and convert this physical address

    //I got this mapping method by cracking stornvme.sys.
    bar0.LowPart = (header.u.type0.BaseAddresses[0] & 0xFFFFC000);
    bar0.HighPart = header.u.type0.BaseAddresses[1];

    PACCESS_RANGE range = Cfg.AccessRanges;
    for (ULONG i = 0; i < Cfg.AccessRangeCount; i++)
    {
        if(true == IsAddrEqual(range->RangeStart, bar0))
        {
            in_iospace = !range->RangeInMemory;
            PUCHAR addr = (PUCHAR)StorPortGetDeviceBase(
                                    DevExt, PCIBus, 
                                    Cfg.BusNumber, bar0, 
                                    range->RangeLength, in_iospace);
            if (NULL != addr)
            {
                CtrlReg = (PNVME_CONTROLLER_REGISTERS) addr;
                Doorbells = (PDOORBELL_PAIR)(addr + 0x1000);
                //todo: support more queues.
                MaxDblCount = PAGE_SIZE / sizeof(DOORBELL_PAIR);
                return true;
            }
        }
    }

    return false;
}
bool CNvmeDevice::GetPciBusData(PCI_COMMON_HEADER& header)
{
    ULONG size = sizeof(PCI_COMMON_HEADER);
    ULONG status = StorPortGetBusData(DevExt, Cfg.InterfaceType,
        Cfg.BusNumber, Cfg.SlotNumber, &header, size);

    //refer to MSDN StorPortGetBusData() to check why 2==status is error.
    if (2 == status || status != size)
        return false;

    VendorID = header.VendorID;
    DeviceID = header.DeviceID;
    return true;
}

bool CNvmeDevice::WaitForCtrlerState(ULONG time_us, BOOLEAN csts_rdy)
{
    ULONG elapsed = 0;
    BOOLEAN is_ready = IsControllerReady();

    while(is_ready != csts_rdy)
    {
        StorPortStallExecution(StallDelay);
        elapsed += StallDelay;
        if(elapsed > time_us)
            return false;

        is_ready = IsControllerReady();
    }

    return true;
}
bool CNvmeDevice::WaitForCtrlerState(ULONG time_us, BOOLEAN csts_rdy, BOOLEAN cc_en)
{
    ULONG elapsed = 0;
    BOOLEAN is_enable = IsControllerEnabled();
    BOOLEAN is_ready = IsControllerReady();

    while (is_ready != csts_rdy || is_enable != cc_en)
    {
        StorPortStallExecution(StallDelay);
        elapsed += StallDelay;
        if (elapsed > time_us)
            return false;

        is_enable = IsControllerEnabled();
        is_ready = IsControllerReady();
    }

    return true;
}

#pragma endregion

