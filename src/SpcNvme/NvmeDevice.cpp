#include "pch.h"

#pragma region ======== CSpcNvmeDevice inline routines ======== 
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
inline void CNvmeDevice::ReadNvmeRegister(NVME_ADMIN_QUEUE_ATTRIBUTES& aqa,
    NVME_ADMIN_SUBMISSION_QUEUE_BASE_ADDRESS& asq,
    NVME_ADMIN_COMPLETION_QUEUE_BASE_ADDRESS& acq,
    bool barrier)
{
    if (barrier)
        MemoryBarrier();

    aqa.AsUlong = StorPortReadRegisterUlong(this, &CtrlReg->AQA.AsUlong);
    asq.AsUlonglong = StorPortReadRegisterUlong64(this, &CtrlReg->ASQ.AsUlonglong);
    acq.AsUlonglong = StorPortReadRegisterUlong64(this, &CtrlReg->ACQ.AsUlonglong);
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
inline void CNvmeDevice::GetAdmQueueDbl(PNVME_SUBMISSION_QUEUE_TAIL_DOORBELL& sub, PNVME_COMPLETION_QUEUE_HEAD_DOORBELL& cpl)
{
    GetQueueDbl(0, sub, cpl);
}
inline void CNvmeDevice::GetQueueDbl(ULONG qid, PNVME_SUBMISSION_QUEUE_TAIL_DOORBELL& sub, PNVME_COMPLETION_QUEUE_HEAD_DOORBELL& cpl)
{
    if (NULL == Doorbells)
    {
        sub = NULL;
        cpl = NULL;
        return;
    }

    sub = (PNVME_SUBMISSION_QUEUE_TAIL_DOORBELL)&Doorbells[qid*2];
    cpl = (PNVME_COMPLETION_QUEUE_HEAD_DOORBELL)&Doorbells[qid*2+1];
}
inline bool CNvmeDevice::IsWorking() { return (State == NVME_STATE::RUNNING); }
inline bool CNvmeDevice::IsSetup() { return (State == NVME_STATE::SETUP); }
inline bool CNvmeDevice::IsTeardown() { return (State == NVME_STATE::TEARDOWN); }
inline bool CNvmeDevice::IsStop() { return (State == NVME_STATE::STOP); }

#pragma endregion

#pragma region ======== CSpcNvmeDevice ======== 
NTSTATUS CNvmeDevice::Setup(PPORT_CONFIGURATION_INFORMATION pci)
{
    NTSTATUS status = STATUS_SUCCESS;

    State = NVME_STATE::SETUP;
    InitVars();
    LoadRegistry();
    GetPciBusData(pci->AdapterInterfaceType, pci->SystemIoBusNumber, pci->SlotNumber);
    PortCfg = pci;

    //todo: handle NUMA nodes for each queue
    //KeQueryLogicalProcessorRelationship(&ProcNum, RelationNumaNode, &ProcInfo, &ProcInfoSize);
    AccessRangeCount = min(ACCESS_RANGE_COUNT, PortCfg->NumberOfAccessRanges);
    RtlCopyMemory(AccessRanges, PortCfg->AccessRanges, 
            sizeof(ACCESS_RANGE) * AccessRangeCount);
    if(!MapCtrlRegisters())
        return STATUS_NOT_MAPPED_DATA;

    //if(!GetMsixTable())
    //    return STATUS_INTERNAL_ERROR;

    ReadCtrlCap();

    status = CreateAdmQ();
    if (!NT_SUCCESS(status))
        return status;

    status = CreateIoQ();
    if (!NT_SUCCESS(status))
        return status;

    State = NVME_STATE::RUNNING;
    return STATUS_SUCCESS;
}

#if 0
bool CNvmeDevice::GetMsixTable()
{
}
#endif 

void CNvmeDevice::Teardown()
{
    if (!IsWorking())
        return;

    State = NVME_STATE::TEARDOWN;
    DeleteAdmQ();
    DeleteIoQ();
    
    State = NVME_STATE::STOP;
    //if(shutdown)
    //    ShutdownController();
}

void CNvmeDevice::DoQueueCplByDPC(ULONG msix_msgid)
{
    if (!IsWorking())
        return;

    if(0 == msix_msgid)
        DoQueueCompletion(AdmQueue);
    else
    {
        for(CNvmeQueue* &queue : IoQueue)
        {
            DoQueueCompletion(queue);
        }
    }
}

NTSTATUS CNvmeDevice::EnableController()
{
    //if (!IsWorking())
    //    return STATUS_INVALID_DEVICE_STATE;

    if (IsControllerReady())
        return STATUS_SUCCESS;

    //if set CC.EN = 1 WHEN CSTS.RDY == 1 and CC.EN == 0, it is undefined behavior.
    //we should wait controller state changing until (CC.EN == 0 and CSTS.RDY == 0).
    bool ok = WaitForCtrlerState(DeviceTimeout, FALSE, FALSE);
    if (!ok)
        return STATUS_INVALID_DEVICE_STATE;
        //KeBugCheckEx(BUGCHECK_ADAPTER, (ULONG_PTR)this, 0, 0, 0);

    //before Enable, update these basic information to controller.
    //these fields only can be modified when CC.EN == 0. (plz refer to nvme 1.3 spec)
    NVME_CONTROLLER_CONFIGURATION cc = { 0 };
    cc.CSS = NVME_CSS_NVM_COMMAND_SET;
    cc.AMS = NVME_AMS_ROUND_ROBIN;
    cc.SHN = NVME_CC_SHN_NO_NOTIFICATION;
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
    //if (!IsWorking())
    //    return STATUS_INVALID_DEVICE_STATE;

    if(!IsControllerReady())
        return STATUS_SUCCESS;

    //if set CC.EN = 1 WHEN CSTS.RDY == 1 and CC.EN == 0, it is undefined behavior.
    //we should wait controller state changing until (CC.EN == 0 and CSTS.RDY == 0).
    bool ok = WaitForCtrlerState(DeviceTimeout, TRUE, TRUE);
    if (!ok)
        return STATUS_INVALID_DEVICE_STATE;
    //        KeBugCheckEx(BUGCHECK_ADAPTER, (ULONG_PTR)this, 0, 0, 0);

    //before Enable, update these basic information to controller.
    //these fields only can be modified when CC.EN == 0. (plz refer to nvme 1.3 spec)
    NVME_CONTROLLER_CONFIGURATION cc = { 0 };
    ReadNvmeRegister(cc);
    cc.EN = 0;
    WriteNvmeRegister(cc);

    //take a break let controller have enough time to retrieve CC values.
    StorPortStallExecution(StallDelay);
    ok = WaitForCtrlerState(DeviceTimeout, FALSE);

    if (!ok)
        return STATUS_INTERNAL_ERROR;
    return STATUS_SUCCESS;
}

//refet to NVME 1.3 , chapter 3.1
//This function set CC.SHN and wait CSTS.SHST==2.
//If called this function then you want to do anything(e.g. submit cmd), you should do
//DisableController()->EnableController. 
//Note : (In NVMe 1.3 spec) If CC.SHN shutdown progress issued then didn't do restart controler, 
//       all following behavior (e.g. submit new cmd) are UNDEFINED BEHAVIOR.
NTSTATUS CNvmeDevice::ShutdownController()
{
    if (!IsStop() && !IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    State = NVME_STATE::SHUTDOWN;
    NVME_CONTROLLER_CONFIGURATION cc = { 0 };
    //NTSTATUS status = STATUS_SUCCESS;
    //if set CC.EN = 0 WHEN CSTS.RDY == 0 and CC.EN == 1, it is undefined behavior.
    //we should wait controller state changing until (CC.EN == 1 and CSTS.RDY == 1).
    bool ok = WaitForCtrlerState(DeviceTimeout, TRUE, TRUE);
    if (!ok)
        goto ERROR_BSOD;

    ReadNvmeRegister(cc);
    cc.SHN = NVME_CC_SHN_NORMAL_SHUTDOWN;
    WriteNvmeRegister(cc);

    ok = WaitForCtrlerShst(DeviceTimeout);
    if (!ok)
        goto ERROR_BSOD;

    return DisableController();

ERROR_BSOD:
    NVME_CONTROLLER_STATUS csts = { 0 };
    ReadNvmeRegister(cc);
    ReadNvmeRegister(csts);
    KeBugCheckEx(BUGCHECK_ADAPTER, (ULONG_PTR)this, (ULONG_PTR)cc.AsUlong, (ULONG_PTR)csts.AsUlong, 0);
}
NTSTATUS CNvmeDevice::InitController()
{
    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    NTSTATUS status = STATUS_SUCCESS;
    status = DisableController();
    if(!NT_SUCCESS(status))
        return status;
    status = RegisterAdmQ();
    if (!NT_SUCCESS(status))
        return status;
    status = EnableController();

    return status;
}
NTSTATUS CNvmeDevice::RestartController()
{
    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;
    return STATUS_NOT_IMPLEMENTED;
}
NTSTATUS CNvmeDevice::IdentifyController(PSPCNVME_SRBEXT srbext)
{
    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    CAutoPtr<SPCNVME_SRBEXT, NonPagedPool, DEV_POOL_TAG> srbext_ptr;
    PSPCNVME_SRBEXT my_srbext = srbext;
    NTSTATUS status = STATUS_SUCCESS;
    if(NULL == my_srbext)
    {
        my_srbext = new SPCNVME_SRBEXT();
        my_srbext->Init(this, NULL);
        srbext_ptr.Reset(my_srbext);
    }
    BuildCmd_IdentCtrler(&my_srbext->NvmeCmd, &this->CtrlIdent);
    status = AdmQueue->SubmitCmd(my_srbext);

    //if(srbext != NULL) means this request comes from StartIo
    if(srbext != NULL || !NT_SUCCESS(status))
        return status;

    //wait loop for FindAdapter called IdentifyController
    do
    {
        StorPortStallExecution(StallDelay);
    }while(SRB_STATUS_PENDING != my_srbext->SrbStatus);

    if(my_srbext->SrbStatus != SRB_STATUS_SUCCESS)
        return STATUS_REQUEST_NOT_ACCEPTED;

    return STATUS_SUCCESS;
}
NTSTATUS CNvmeDevice::IdentifyNamespace(PSPCNVME_SRBEXT srbext)
{
    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    UNREFERENCED_PARAMETER(srbext);
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "IdentifyNamespace() still not implemented yet!!\n");
    return STATUS_SUCCESS;
}
NTSTATUS CNvmeDevice::SetInterruptCoalescing(PSPCNVME_SRBEXT srbext)
{
    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    UNREFERENCED_PARAMETER(srbext);
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "SetInterruptCoalescing() still not implemented yet!!\n");
    return STATUS_SUCCESS;
}
NTSTATUS CNvmeDevice::SetAsyncEvent(PSPCNVME_SRBEXT srbext)
{
    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    UNREFERENCED_PARAMETER(srbext);
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "SetAsyncEvent() still not implemented yet!!\n");
    return STATUS_SUCCESS;
}
NTSTATUS CNvmeDevice::SetArbitration(PSPCNVME_SRBEXT srbext)
{
    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    UNREFERENCED_PARAMETER(srbext);
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "SetArbitration() still not implemented yet!!\n");
    return STATUS_SUCCESS;
}
NTSTATUS CNvmeDevice::SetSyncHostTime(PSPCNVME_SRBEXT srbext)
{
    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    UNREFERENCED_PARAMETER(srbext);
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "SetSyncHostTime() still not implemented yet!!\n");
    return STATUS_SUCCESS;
}
NTSTATUS CNvmeDevice::SetPowerManagement(PSPCNVME_SRBEXT srbext)
{
    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    UNREFERENCED_PARAMETER(srbext);
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "SetPowerManagement() still not implemented yet!!\n");
    return STATUS_SUCCESS;
}

NTSTATUS CNvmeDevice::RegisterIoQ()
{
    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    CAutoPtr<SPCNVME_SRBEXT, NonPagedPool, DEV_POOL_TAG> srbext_ptr;
    PSPCNVME_SRBEXT srbext = new SPCNVME_SRBEXT();
    NTSTATUS status = STATUS_SUCCESS;
    if (NULL == srbext)
        return STATUS_MEMORY_NOT_ALLOCATED;

    srbext->Init(this, NULL);
    srbext_ptr.Reset(srbext);

    for (ULONG i = 0; i < DesiredIoQ; i++)
    {
        if(NULL == IoQueue[i])
            continue;
//register IoQueue should register CplQ first, then SubQ.
//They are "Pair" .
        BuildCmd_RegIoCplQ(&srbext->NvmeCmd, IoQueue[i]);
        status = AdmQueue->SubmitCmd(srbext);

        BuildCmd_RegIoSubQ(&srbext->NvmeCmd, IoQueue[i]);
        status = AdmQueue->SubmitCmd(srbext);
    }

    //wait loop for
    do
    {
        StorPortStallExecution(StallDelay);
    } while (SRB_STATUS_PENDING != srbext->SrbStatus);

    if (srbext->SrbStatus != SRB_STATUS_SUCCESS)
        return STATUS_REQUEST_NOT_ACCEPTED;

    return STATUS_SUCCESS;
}
NTSTATUS CNvmeDevice::UnregisterIoQ()
{
    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    CAutoPtr<SPCNVME_SRBEXT, NonPagedPool, DEV_POOL_TAG> srbext_ptr;
    PSPCNVME_SRBEXT srbext = new SPCNVME_SRBEXT();
    NTSTATUS status = STATUS_SUCCESS;
    if (NULL == srbext)
        return STATUS_MEMORY_NOT_ALLOCATED;

    srbext->Init(this, NULL);
    srbext_ptr.Reset(srbext);

    //todo: submit cmd
    //BuildCmd_IdentCtrler(&srbext->NvmeCmd, &this->CtrlIdent);

    for (ULONG i = 0; i < DesiredIoQ; i++)
    {
        if (NULL == IoQueue[i])
            continue;
        BuildCmd_UnRegIoSubQ(&srbext->NvmeCmd, IoQueue[i]);
        status = AdmQueue->SubmitCmd(srbext);
        BuildCmd_UnRegIoCplQ(&srbext->NvmeCmd, IoQueue[i]);
        status = AdmQueue->SubmitCmd(srbext);

    }

    //wait loop for FindAdapter called IdentifyController
    do
    {
        StorPortStallExecution(StallDelay);
    } while (SRB_STATUS_PENDING != srbext->SrbStatus);

    if (srbext->SrbStatus != SRB_STATUS_SUCCESS)
        return STATUS_REQUEST_NOT_ACCEPTED;

    return STATUS_SUCCESS;
}

NTSTATUS CNvmeDevice::CreateAdmQ()
{
    if(NULL != AdmQueue)
        return STATUS_ALREADY_INITIALIZED;

    QUEUE_PAIR_CONFIG cfg = {0};
    cfg.Depth = (USHORT)AdmDepth;
    cfg.QID = 0;
    cfg.DevExt = this;
    cfg.NumaNode = MM_ANY_NODE_OK;
    cfg.Type = QUEUE_TYPE::ADM_QUEUE;
    GetAdmQueueDbl(cfg.SubDbl , cfg.CplDbl);
    AdmQueue = new CNvmeQueue(&cfg);
    if(!AdmQueue->IsInitOK())
        return STATUS_MEMORY_NOT_ALLOCATED;
    return STATUS_SUCCESS;
}
NTSTATUS CNvmeDevice::RegisterAdmQ()
{
//AQA register should be only modified when csts.RDY==0(cc.EN == 0)
    if(IsControllerReady() || NULL == AdmQueue)
    {
        NVME_CONTROLLER_STATUS csts = { 0 };
        ReadNvmeRegister(csts, true);
        KeBugCheckEx(BUGCHECK_INVALID_STATE, (ULONG_PTR) this, (ULONG_PTR) csts.AsUlong, 0, 0);
    }

    PHYSICAL_ADDRESS subq = { 0 };
    PHYSICAL_ADDRESS cplq = { 0 };
    NVME_ADMIN_QUEUE_ATTRIBUTES aqa = { 0 };
    NVME_ADMIN_SUBMISSION_QUEUE_BASE_ADDRESS asq = { 0 };
    NVME_ADMIN_COMPLETION_QUEUE_BASE_ADDRESS acq = { 0 };
    AdmQueue->GetQueueAddr(&subq, &cplq);

    if(0 == subq.QuadPart || NULL == cplq.QuadPart)
        return STATUS_MEMORY_NOT_ALLOCATED;
    aqa.ASQS = AdmDepth - 1;    //ASQS and ACQS are zero based index. here we should fill "MAX index" not total count;
    aqa.ACQS = AdmDepth - 1;
    asq.ASQB = subq.QuadPart;
    acq.ACQB = cplq.QuadPart;
    WriteNvmeRegister(aqa, asq, acq);
    return STATUS_SUCCESS;
}
NTSTATUS CNvmeDevice::UnregisterAdmQ()
{
    //AQA register should be only modified when (csts.RDY==0 && cc.EN == 0)
    if (IsControllerReady())
    {
        NVME_CONTROLLER_STATUS csts = { 0 };
        ReadNvmeRegister(csts, true);
        KeBugCheckEx(BUGCHECK_INVALID_STATE, (ULONG_PTR)this, (ULONG_PTR)csts.AsUlong, 0, 0);
    }

    NVME_ADMIN_QUEUE_ATTRIBUTES aqa = { 0 };
    NVME_ADMIN_SUBMISSION_QUEUE_BASE_ADDRESS asq = { 0 };
    NVME_ADMIN_COMPLETION_QUEUE_BASE_ADDRESS acq = { 0 };
    WriteNvmeRegister(aqa, asq, acq);
    return STATUS_SUCCESS;
}
NTSTATUS CNvmeDevice::DeleteAdmQ()
{
    if(NULL == AdmQueue)
        return STATUS_MEMORY_NOT_ALLOCATED;

    AdmQueue->Teardown();
    delete AdmQueue;
    AdmQueue = NULL;
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
    INTERFACE_TYPE type = PortCfg->AdapterInterfaceType;
    //I got this mapping method by cracking stornvme.sys.
    bar0.LowPart = (PciCfg.u.type0.BaseAddresses[0] & 0xFFFFC000);
    bar0.HighPart = PciCfg.u.type0.BaseAddresses[1];

    for (ULONG i = 0; i < AccessRangeCount; i++)
    {
        PACCESS_RANGE range = &AccessRanges[i];
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
                //if(Bar0Size > PAGE_SIZE*2)
                //    MsixVectors = (MsixVector*)(addr + (PAGE_SIZE * 2));
                return true;
            }
        }
    }

    return false;
}
bool CNvmeDevice::GetPciBusData(INTERFACE_TYPE type, ULONG bus, ULONG slot)
{
    ULONG size = sizeof(PciCfg);
    ULONG status = StorPortGetBusData(this, type, bus, slot, &PciCfg, size);

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
bool CNvmeDevice::WaitForCtrlerShst(ULONG time_us)
{
    ULONG elapsed = 0;
    //BOOLEAN is_ready = IsControllerReady();
    BOOLEAN shn_done = FALSE;

    while (!shn_done)
    {
        NVME_CONTROLLER_STATUS csts = { 0 };
        ReadNvmeRegister(csts);
        if(csts.SHST == NVME_CSTS_SHST_SHUTDOWN_COMPLETED)
            shn_done = TRUE;
        else
        {
            StorPortStallExecution(StallDelay);
            elapsed += StallDelay;
            if (elapsed > time_us)
                return false;
        }
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
    State = NVME_STATE::STOP;
    CpuCount = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
    TotalNumaNodes = 0;     //todo: query total numa nodes.
    RegisteredIoQ = 0;
    CreatedIoQ = 0;
    DesiredIoQ = NVME_CONST::IO_QUEUE_COUNT;
    DeviceTimeout = 2000 * NVME_CONST::STALL_TIME_US;        //should be updated by CAP, unit in micro-seconds
    StallDelay = NVME_CONST::STALL_TIME_US;
    AccessRangeCount = 0;
    RtlZeroMemory(AccessRanges, sizeof(ACCESS_RANGE)* ACCESS_RANGE_COUNT);

    MaxTxSize = NVME_CONST::TX_SIZE;
    MaxTxPages = NVME_CONST::TX_PAGES;
    MaxNamespaces = NVME_CONST::SUPPORT_NAMESPACES;
    AdmDepth = NVME_CONST::ADMIN_QUEUE_DEPTH;
    IoDepth = NVME_CONST::IO_QUEUE_DEPTH;
    AdmQueue = NULL;
    RtlZeroMemory(IoQueue, sizeof(CNvmeQueue*) * MAX_IO_QUEUE_COUNT);

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
void CNvmeDevice::DoQueueCompletion(CNvmeQueue* queue)
{
    UNREFERENCED_PARAMETER(queue);
    //not implemented
}
NTSTATUS CNvmeDevice::CreateIoQ()
{
    //TODO: if IoQ already allocated?
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    QUEUE_PAIR_CONFIG cfg = { 0 };
    cfg.DevExt = this;
    cfg.Depth = IoDepth;
    cfg.NumaNode = 0;
    cfg.Type = QUEUE_TYPE::IO_QUEUE;

    for(USHORT i=0; i<DesiredIoQ; i++)
    {
        if(NULL != IoQueue[i])
            continue;
        CNvmeQueue* queue = new CNvmeQueue();
        this->GetQueueDbl(i, cfg.SubDbl, cfg.CplDbl);
        cfg.QID = i + 1;
        status = queue->Setup(&cfg);
        if(!NT_SUCCESS(status))
            return status;
        IoQueue[i] = queue;
        CreatedIoQ++;
    }

    return STATUS_SUCCESS;
}
NTSTATUS CNvmeDevice::DeleteIoQ()
{
    for(CNvmeQueue* &queue : IoQueue)
    {
        if(NULL == queue)
            continue;

        queue->Teardown();
        delete queue;
        CreatedIoQ--;
    }

    RtlZeroMemory(IoQueue, sizeof(CNvmeQueue*) * MAX_IO_QUEUE_COUNT);
    return STATUS_SUCCESS;
}

#pragma endregion

