#include "pch.h"

#pragma region ======== CSpcNvmeDevice Factory Methods ======== 
CSpcNvmeDevice *CSpcNvmeDevice::Create(PVOID devext, PSPCNVME_CONFIG cfg)
{
    CSpcNvmeDevice *ret = new CSpcNvmeDevice(devext, cfg);

    return ret;
}
void CSpcNvmeDevice::Delete(CSpcNvmeDevice* ptr)
{
    ptr->Teardown();
    delete ptr;
}
#pragma endregion

#pragma region ======== class CSpcNvmeDevice ======== 

CSpcNvmeDevice::CSpcNvmeDevice(){}
CSpcNvmeDevice::CSpcNvmeDevice(PVOID devext, PSPCNVME_CONFIG cfg) 
    : CSpcNvmeDevice()
{
    Setup(devext, cfg);
}
CSpcNvmeDevice::~CSpcNvmeDevice()
{
    Teardown();
}

void CSpcNvmeDevice::Setup(PVOID devext, PSPCNVME_CONFIG cfg)
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
    {
        RefreshByCapability();
        IsReady = map_ok && SetupAdminQueuePair();
    }
}
void CSpcNvmeDevice::Teardown()
{
    if(!IsReady)
        return ;

    DisableController();
    AdminQueue.Teardown();
    IsReady = false;
}

void CSpcNvmeDevice::UpdateIdentifyData(PNVME_IDENTIFY_CONTROLLER_DATA data)
{
    RtlCopyMemory(&IdentData, data, sizeof(NVME_IDENTIFY_CONTROLLER_DATA));
}

void CSpcNvmeDevice::SetMaxIoQueueCount(ULONG max)
{
    MaxIoQueueCount = max;
}

bool CSpcNvmeDevice::EnableController()
{
    //if set CC.EN = 1 WHEN CSTS.RDY == 1 and CC.EN == 0, it is undefined behavior.
    //we should wait controller state changing until (CC.EN == 0 and CSTS.RDY == 0).
    bool ok = WaitForCtrlerState(CtrlerTimeout, 0, 0);
    if (!ok)
        KeBugCheckEx(BUGCHECK_ADAPTER, (ULONG_PTR)this, 0, 0, 0);

    NVME_CONTROLLER_CONFIGURATION cc = {0};
    cc.CSS = NVME_CSS_NVM_COMMAND_SET;
    cc.AMS = NVME_AMS_ROUND_ROBIN;
    cc.SHN = NVME_CSTS_SHST_NO_SHUTDOWN;
    cc.IOSQES = SPCNVME_CONFIG::IOSQES;
    cc.IOCQES = SPCNVME_CONFIG::IOCQES;
    //cc.MPS = ??;
    cc.EN = 0;
    StorPortWriteRegisterUlong(DevExt, &CtrlReg->CC.AsUlong, cc.AsUlong);
    ok = WaitForCtrlerState(CtrlerTimeout, 0, 0);
    if (!ok)
        return false;

    NVME_ADMIN_QUEUE_ATTRIBUTES aqa = {0};
    NVME_ADMIN_SUBMISSION_QUEUE_BASE_ADDRESS asq = {0};
    NVME_ADMIN_COMPLETION_QUEUE_BASE_ADDRESS acq = {0};

    //tell controller: how many entries in my Admin Sub and Cpl Queue?
    //these attributes should be write to controller ONLY WHEN CC.EN == 0.
    aqa.ASQS = ADM_QDEPTH;
    aqa.ACQS = ADM_QDEPTH;
    StorPortWriteRegisterUlong(DevExt, &CtrlReg->AQA.AsUlong, aqa.AsUlong);

    PHYSICAL_ADDRESS subq_pa = {0};
    PHYSICAL_ADDRESS cplq_pa = { 0 };
    AdminQueue.GetQueueAddrPA(&subq_pa, &cplq_pa);

    asq.AsUlonglong = subq_pa.QuadPart;
    StorPortWriteRegisterUlong64(DevExt, &CtrlReg->ASQ.AsUlonglong, asq.AsUlonglong);

    acq.AsUlonglong = cplq_pa.QuadPart;
    StorPortWriteRegisterUlong64(DevExt, &CtrlReg->ACQ.AsUlonglong, acq.AsUlonglong);

    cc.EN = 1;
    StorPortWriteRegisterUlong(DevExt, &CtrlReg->CC.AsUlong, cc.AsUlong);

    return WaitForCtrlerState(CtrlerTimeout, 1);
}

bool CSpcNvmeDevice::DisableController()
{
    NVME_CONTROLLER_STATUS csts = {0};
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
        if(!ok)
            KeBugCheckEx(BUGCHECK_ADAPTER, (ULONG_PTR) this, (ULONG_PTR) &cc, (ULONG_PTR) &csts, 0);
    }
    else if (1 == csts.RDY && 0 == cc.EN)   //abnormal state...
        KeBugCheckEx(BUGCHECK_ADAPTER, (ULONG_PTR)this, (ULONG_PTR)&cc, (ULONG_PTR)&csts, 0);
#endif

    //if set CC.EN = 0 WHEN CSTS.RDY == 0 and CC.EN == 1, it is undefined behavior.
    //we should wait controller state changing until (CC.EN == 1 and CSTS.RDY == 1).
    bool ok = WaitForCtrlerState(CtrlerTimeout, 1, 1);
    if(!ok)
        KeBugCheckEx(BUGCHECK_ADAPTER, (ULONG_PTR)this, 0, 0, 0);

    cc.AsUlong = StorPortReadRegisterUlong(DevExt, &CtrlReg->CC.AsUlong);
    cc.EN = FALSE;
    cc.SHN = NVME_CSTS_SHST_NO_SHUTDOWN;
    StorPortWriteRegisterUlong(DevExt, &CtrlReg->CC.AsUlong, cc.AsUlong);

    return WaitForCtrlerState(CtrlerTimeout, 0);
}

void CSpcNvmeDevice::RefreshByCapability()
{
    //CtrlReg->CAP.TO is timeout value in 500 ms.
    //We should convert it to micro-seconds for StorPortStallExecution using.
    ULONG cap_timeout = CtrlReg->CAP.TO;
    CtrlerTimeout = cap_timeout * (500 * 1000);
}
bool CSpcNvmeDevice::MapControllerRegisters(PCI_COMMON_HEADER &header)
{
    if(NULL == DevExt)
        return false;

    BOOLEAN in_iospace = FALSE;

    STOR_PHYSICAL_ADDRESS bar0 = { 0 };
    CtrlReg = NULL;
    Doorbells = NULL;
    //todo: use correct structure to parse and convert this physical address
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
                return true;
            }
        }
    }

    return false;
}
bool CSpcNvmeDevice::GetPciBusData(PCI_COMMON_HEADER& header)
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
bool CSpcNvmeDevice::SetupAdminQueuePair()
{
    QUEUE_PAIR_CONFIG qpcfg = { 0 };
    qpcfg.Type = QUEUE_TYPE::ADM_QUEUE;
    qpcfg.Depth = ADM_QDEPTH;
    qpcfg.DevExt = DevExt;
    qpcfg.QID = 0;
    qpcfg.SubDbl = &Doorbells[qpcfg.QID].SubTail.AsUlong;
    qpcfg.CplDbl = &Doorbells[qpcfg.QID].CplHead.AsUlong;

    return AdminQueue.Setup(&qpcfg);
}
bool CSpcNvmeDevice::WaitForCtrlerState(ULONG time_us, BOOLEAN csts_rdy)
{
    ULONG elapsed = 0;
    NVME_CONTROLLER_STATUS csts = { 0 };
    csts.AsUlong = StorPortReadRegisterUlong(DevExt, &CtrlReg->CSTS.AsUlong);

    while(csts.RDY != csts_rdy)
    {
        StorPortStallExecution(STALL_INTERVAL_US);
        elapsed += STALL_INTERVAL_US;
        if(elapsed > time_us)
            return false;

        csts.AsUlong = StorPortReadRegisterUlong(DevExt, &CtrlReg->CSTS.AsUlong);
    }

    return true;
}
bool CSpcNvmeDevice::WaitForCtrlerState(ULONG time_us, BOOLEAN csts_rdy, BOOLEAN cc_en)
{
    ULONG elapsed = 0;
    NVME_CONTROLLER_STATUS csts = { 0 };
    NVME_CONTROLLER_CONFIGURATION cc = { 0 };

    csts.AsUlong = StorPortReadRegisterUlong(DevExt, &CtrlReg->CSTS.AsUlong);
    cc.AsUlong = StorPortReadRegisterUlong(DevExt, &CtrlReg->CC.AsUlong);

    while (csts.RDY != csts_rdy || cc.EN != cc_en)
    {
        StorPortStallExecution(STALL_INTERVAL_US);
        elapsed += STALL_INTERVAL_US;
        if (elapsed > time_us)
            return false;

        csts.AsUlong = StorPortReadRegisterUlong(DevExt, &CtrlReg->CSTS.AsUlong);
        cc.AsUlong = StorPortReadRegisterUlong(DevExt, &CtrlReg->CC.AsUlong);
    }

    return true;
}

#pragma endregion

