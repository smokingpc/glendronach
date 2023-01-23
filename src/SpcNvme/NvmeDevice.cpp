#include "pch.h"

#pragma region ======== class CSpcNvmeDevice ======== 

inline void CNvmeDevice::ReadNvmeRegister(NVME_CONTROLLER_CONFIGURATION& cc, bool barrier)
{
    if(barrier)
        MemoryBarrier();
    cc.AsUlong = StorPortReadRegisterUlong(this, &CtrlReg->CC.AsUlong);
}
inline void CNvmeDevice::ReadNvmeRegister(NVME_CONTROLLER_STATUS& csts, bool barrier)
{
    if (barrier)
        MemoryBarrier();
    csts.AsUlong = StorPortReadRegisterUlong(this, &CtrlReg->CSTS.AsUlong);
}
inline void CNvmeDevice::ReadNvmeRegister(NVME_VERSION& ver, bool barrier)
{
    if (barrier)
        MemoryBarrier();
    ver.AsUlong = StorPortReadRegisterUlong(this, &CtrlReg->VS.AsUlong);
}
inline void CNvmeDevice::ReadNvmeRegister(NVME_CONTROLLER_CAPABILITIES& cap, bool barrier)
{
    if (barrier)
        MemoryBarrier();
    cap.AsUlonglong = StorPortReadRegisterUlong64(this, &CtrlReg->CAP.AsUlonglong);
}
inline void CNvmeDevice::WriteNvmeRegister(NVME_CONTROLLER_CONFIGURATION& cc, bool barrier)
{
    if (barrier)
        MemoryBarrier();
    StorPortWriteRegisterUlong(this, &CtrlReg->CC.AsUlong, cc.AsUlong);
}
inline void CNvmeDevice::WriteNvmeRegister(NVME_CONTROLLER_STATUS& csts, bool barrier)
{
    if (barrier)
        MemoryBarrier();
    StorPortWriteRegisterUlong(this, &CtrlReg->CSTS.AsUlong, csts.AsUlong);
}
inline void CNvmeDevice::WriteNvmeRegister(NVME_ADMIN_QUEUE_ATTRIBUTES& aqa,
    NVME_ADMIN_SUBMISSION_QUEUE_BASE_ADDRESS& asq,
    NVME_ADMIN_COMPLETION_QUEUE_BASE_ADDRESS& acq,
    bool barrier)
{
    if (barrier)
        MemoryBarrier();

    StorPortWriteRegisterUlong(this, &CtrlReg->AQA.AsUlong, aqa.AsUlong);
    StorPortWriteRegisterUlong64(this, &CtrlReg->ASQ.AsUlonglong, asq.AsUlonglong);
    StorPortWriteRegisterUlong64(this, &CtrlReg->ACQ.AsUlonglong, acq.AsUlonglong);
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
    return (TRUE == csts.RDY && FALSE == csts.CFS) ? TRUE : FALSE;
}
NTSTATUS CNvmeDevice::Setup(PPORT_CONFIGURATION_INFORMATION pci)
{
    InitVars();
    LoadRegistry();
    GetPciBusData();

    PortCfg = pci;
    AccessRangeCount = min(ACCESS_RANGE_COUNT, PortCfg->NumberOfAccessRanges);
    RtlCopyMemory(AccessRanges, PortCfg->AccessRanges, 
            sizeof(ACCESS_RANGE) * AccessRangeCount);
    if(!MapCtrlRegisters())
        return STATUS_NOT_MAPPED_DATA;

    ReadCtrlCap();
    IsReady = true;
    return STATUS_SUCCESS;
}
void CNvmeDevice::Teardown()
{
    if(!IsReady)
        return;
    IsReady = false;
}

NTSTATUS CNvmeDevice::EnableController()
{
    //if set CC.EN = 1 WHEN CSTS.RDY == 1 and CC.EN == 0, it is undefined behavior.
    //we should wait controller state changing until (CC.EN == 0 and CSTS.RDY == 0).
    bool ok = WaitForCtrlerState(DeviceTimeout, FALSE, FALSE);
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

    ok = WaitForCtrlerState(DeviceTimeout, TRUE);

    if(!ok)
        return STATUS_INTERNAL_ERROR;
    return STATUS_SUCCESS;
}
NTSTATUS CNvmeDevice::DisableController()
{
    NVME_CONTROLLER_STATUS csts = { 0 };
    NVME_CONTROLLER_CONFIGURATION cc = { 0 };

    //if set CC.EN = 0 WHEN CSTS.RDY == 0 and CC.EN == 1, it is undefined behavior.
    //we should wait controller state changing until (CC.EN == 1 and CSTS.RDY == 1).
    bool ok = WaitForCtrlerState(DeviceTimeout, 1, 1);
    if (!ok)
        KeBugCheckEx(BUGCHECK_ADAPTER, (ULONG_PTR)this, 0, 0, 0);

    ReadNvmeRegister(cc);
    cc.EN = FALSE;
    cc.SHN = NVME_CSTS_SHST_NO_SHUTDOWN;
    WriteNvmeRegister(cc);

    ok = WaitForCtrlerState(DeviceTimeout, 0);
    if (!ok)
        return STATUS_INTERNAL_ERROR;
    return STATUS_SUCCESS;
}

void CNvmeDevice::ReadCtrlCap()
{
    //CtrlReg->CAP.TO is timeout value in 500 ms.
    //It indicates the timeout worst case of Enabling/Disabling Controller.
    //e.g. CAP.TO==3 means worst case timout to Enable/Disable controller is 1500 milli-second.
    //We should convert it to micro-seconds for StorPortStallExecution() using.
    
    ReadNvmeRegister(NvmeVer);
    ReadNvmeRegister(CtrlCap);
    DeviceTimeout = ((UCHAR)CtrlCap.TO) * (500 * 1000);

}
bool CNvmeDevice::MapCtrlRegisters()
{
    BOOLEAN in_iospace = FALSE;
    STOR_PHYSICAL_ADDRESS bar0 = { 0 };
    PACCESS_RANGE range = AccessRanges;
    INTERFACE_TYPE type = PortCfg->AdapterInterfaceType;
    //I got this mapping method by cracking stornvme.sys.
    bar0.LowPart = (PciCfg.u.type0.BaseAddresses[0] & 0xFFFFC000);
    bar0.HighPart = PciCfg.u.type0.BaseAddresses[1];

    for (ULONG i = 0; i < AccessRangeCount; i++)
    {
        if(true == IsAddrEqual(range->RangeStart, bar0))
        {
            in_iospace = !range->RangeInMemory;
            PUCHAR addr = (PUCHAR)StorPortGetDeviceBase(
                                    this, type, 
                                    PortCfg->SystemIoBusNumber, bar0, 
                                    range->RangeLength, in_iospace);
            if (NULL != addr)
            {
                CtrlReg = (PNVME_CONTROLLER_REGISTERS)addr;
                Bar0Size = range->RangeLength;
                Doorbells = CtrlReg->Doorbells;
                if(Bar0Size > PAGE_SIZE*2)
                    PcieMsixTable = addr + (PAGE_SIZE * 2);
                return true;
            }
        }
    }

    return false;
}
bool CNvmeDevice::GetPciBusData()
{
    ULONG size = sizeof(PciCfg);
    ULONG status = StorPortGetBusData(this, PortCfg->AdapterInterfaceType,
        PortCfg->SystemIoBusNumber, PortCfg->SlotNumber, &PciCfg, size);

    //refer to MSDN StorPortGetBusData() to check why 2==status is error.
    if (2 == status || status != size)
        return false;

    VendorID = PciCfg.VendorID;
    DeviceID = PciCfg.DeviceID;
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

void CNvmeDevice::InitVars()
{
    CtrlReg = NULL;
    PortCfg = NULL;
    Doorbells = NULL;

    VendorID = 0;
    DeviceID = 0;
    IsReady = FALSE;
    State = NVME_STATE::STOP;
    CpuCount = 0;
    TotalNumaNodes = 0;
    RegisteredIoQ = 0;
    DesiredIoQ = 0;
    DeviceTimeout = 2000 * NVME_CONST::STALL_TIME_US;        //should be updated by CAP, unit in micro-seconds
    StallDelay = NVME_CONST::SLEEP_TIME_US;

    AccessRangeCount = 0;
    RtlZeroMemory(AccessRanges, sizeof(ACCESS_RANGE)* ACCESS_RANGE_COUNT);

    MaxTxSize = NVME_CONST::TX_SIZE;
    MaxTxPages = NVME_CONST::TX_PAGES;
    MaxNamespaces = NVME_CONST::SUPPORT_NAMESPACES;
    MaxTargets = NVME_CONST::MAX_TARGETS;
    MaxLu = NVME_CONST::MAX_LU;
    MaxIoPerLU = NVME_CONST::MAX_IO_PER_LU;
    MaxTotalIo = NVME_CONST::MAX_IO_PER_LU * NVME_CONST::MAX_LU;

    AdmQ = NULL;
    RtlZeroMemory(IoQueue, sizeof(CNvmeQueue)* MAX_IO_QUEUE_COUNT);

    RtlZeroMemory(&PciCfg, sizeof(PCI_COMMON_CONFIG));
    RtlZeroMemory(&NvmeVer, sizeof(NVME_VERSION));
    RtlZeroMemory(&CtrlCap, sizeof(NVME_CONTROLLER_CAPABILITIES));
    RtlZeroMemory(&CtrlIdent, sizeof(NVME_IDENTIFY_CONTROLLER_DATA));
    RtlZeroMemory(&NsData, sizeof(NVME_IDENTIFY_NAMESPACE_DATA)* NVME_CONST::SUPPORT_NAMESPACES);
}
void CNvmeDevice::LoadRegistry()
{
    //not implemented
}

#pragma endregion

