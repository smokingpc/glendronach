#include "pch.h"

BOOLEAN CNvmeDevice::NvmeMsixISR(IN PVOID hbaext, IN ULONG msgid)
{
    CNvmeDevice* nvme = (CNvmeDevice*)hbaext;
    CNvmeQueue *queue = (msgid == 0)? nvme->AdmQueue : nvme->IoQueue[msgid-1];
    if (NULL == queue || !nvme->IsWorking())
        goto END;

    BOOLEAN ok = FALSE;
    ok = StorPortIssueDpc(hbaext, &queue->QueueCplDpc, NULL, NULL);
END:
    return TRUE;
}
void CNvmeDevice::RestartAdapterDpc(
    IN PSTOR_DPC  Dpc,
    IN PVOID  NvmeDev,
    IN PVOID  Arg1,
    IN PVOID  Arg2)
{
    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(Arg1);
    UNREFERENCED_PARAMETER(Arg2);
    CNvmeDevice *nvme = (CNvmeDevice*)NvmeDev;
    ULONG stor_status = STOR_STATUS_SUCCESS;
    if(!nvme->IsWorking())
        return;

    //todo: log error
    //STOR_STATUS_BUSY : already queued this workitem.
    //STOR_STATUS_INVALID_DEVICE_STATE : device is removing.
    //STOR_STATUS_INVALID_IRQL: IRQL > DISPATCH_LEVEL
    StorPortInitializeWorker(nvme, &nvme->RestartWorker);
    stor_status = StorPortQueueWorkItem(NvmeDev, CNvmeDevice::RestartAdapterWorker, nvme->RestartWorker, NULL);
    ASSERT(stor_status == STOR_STATUS_SUCCESS);
}
void CNvmeDevice::RestartAdapterWorker(
    _In_ PVOID NvmeDev,
    _In_ PVOID Context,
    _In_ PVOID Worker)
{
    UNREFERENCED_PARAMETER(Context);
    UNREFERENCED_PARAMETER(Worker);

    CNvmeDevice* nvme = (CNvmeDevice*)NvmeDev;
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    //ULONG stor_status = STOR_STATUS_SUCCESS;
    if (!nvme->IsWorking())
        return;

    //In InitController, DisableController and EnableController could call 
    //StorPortStallExecution(). It could cause system lag if called in DIRQL.
    //So I move InitController() here...
    status = nvme->InitNvmeStage1();
    ASSERT(NT_SUCCESS(status));

    status = nvme->InitNvmeStage2();
    ASSERT(NT_SUCCESS(status));
    //resume adapter AFTER restart controller done.
    StorPortResume(NvmeDev);
    StorPortFreeWorker(nvme, &nvme->RestartWorker);
    nvme->RestartWorker = NULL;
}
VOID CNvmeDevice::HandleAsyncEvent(
    _In_ PSPCNVME_SRBEXT srbext)
{
    PNVME_COMPLETION_DW0_ASYNC_EVENT_REQUEST event =
        (PNVME_COMPLETION_DW0_ASYNC_EVENT_REQUEST)&srbext->NvmeCpl.DW0;
    ((CNvmeDevice*)srbext->NvmeDev)->SaveAsyncEvent(event);

    //If AsyncEvent can notify back, AdmQ should be still working.
    //Just ask controller : What happened?
    ((CNvmeDevice*)srbext->NvmeDev)->GetLogPageForAsyncEvent(event->LogPage);
}
VOID CNvmeDevice::HandleErrorInfoLogPage(
    _In_ PSPCNVME_SRBEXT srbext)
{
    PNVME_ERROR_INFO_LOG errlog = (PNVME_ERROR_INFO_LOG)srbext->ExtBuf;
    CNvmeDevice* devext = (CNvmeDevice*)srbext->DevExt;

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "****[ErrorInfo LogPage]:\n");
    
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "    RootCause Cid(%04X), ErrorCount(%llX), SubQID(%04X) CmdStatus(%04X), ParamErrLoc(Byte%d, Bit%d)\n", 
            errlog->CMDID, errlog->ErrorCount, errlog->SQID, errlog->Status.AsUshort, errlog->ParameterErrorLocation.Byte, errlog->ParameterErrorLocation.Bit);
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "    LBA(%llX), NameSpace(%08X), CmdSpecificInfo(%llX), VendorSpecificInfo(%X)\n",
            errlog->Lba, errlog->NameSpace, errlog->CommandSpecificInfo, errlog->VendorInfoAvailable);

    //SaveAsyncEventLogPage() will copy srbext->ExtBuf pointer and save it.
    //Should replace srbext->ExtBuf by NULL to prevent completion function free it.
    devext->SaveAsyncEventLogPage(srbext->ExtBuf);
    srbext->ExtBuf = NULL;

    if(srbext->NvmeCpl.DW3.Status.M)
        devext->GetLogPageForAsyncEvent(1);
    else
        devext->RequestAsyncEvent();
}
VOID CNvmeDevice::HandleSmartInfoLogPage(
    _In_ PSPCNVME_SRBEXT srbext)
{
    PNVME_HEALTH_INFO_LOG smartlog = (PNVME_HEALTH_INFO_LOG)srbext->ExtBuf;
    CNvmeDevice* devext = (CNvmeDevice*)srbext->DevExt;

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "****[SmartInfo LogPage]:\n");
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "    AvailableSpaceLow(%d), TemperatureThreshold(%d), ReliabilityDegraded(%d), ReadOnly(%d), VolatileMemoryBackupDeviceFailed(%d)\n",
        smartlog->CriticalWarning.AvailableSpaceLow, smartlog->CriticalWarning.TemperatureThreshold, 
        smartlog->CriticalWarning.ReliabilityDegraded, smartlog->CriticalWarning.ReadOnly, 
        smartlog->CriticalWarning.VolatileMemoryBackupDeviceFailed);

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "    Temperature[0]=(%d), Temperature[1]=(%d)\n", 
        smartlog->Temperature[0]+ KELVIN_ZERO_TO_CELSIUS, smartlog->Temperature[1]+ KELVIN_ZERO_TO_CELSIUS);

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "    AvailableSpare(%d), PercentageUsed(%d)\n", 
        smartlog->AvailableSpare, smartlog->PercentageUsed);

    //SaveAsyncEventLogPage() will copy srbext->ExtBuf pointer and save it.
    //Should replace srbext->ExtBuf by NULL to prevent completion function free it.
    devext->SaveAsyncEventLogPage(srbext->ExtBuf);
    srbext->ExtBuf = NULL;

    if (srbext->NvmeCpl.DW3.Status.M)
        devext->GetLogPageForAsyncEvent(1);
    else
        devext->RequestAsyncEvent();
}
VOID CNvmeDevice::HandleFwSlotInfoLogPage(
    _In_ PSPCNVME_SRBEXT srbext)
{
    PNVME_FIRMWARE_SLOT_INFO_LOG slotinfo = (PNVME_FIRMWARE_SLOT_INFO_LOG)srbext->ExtBuf;
    CNvmeDevice* devext = (CNvmeDevice*)srbext->DevExt;
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "****[SmartInfo LogPage]:\n");
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "    ActiveSlot(%d), PendingActivateSlot(%d)\n",
        slotinfo->AFI.ActiveSlot, slotinfo->AFI.PendingActivateSlot);
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "    ");
    for(int i=0; i<7; i++)
    {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "    FRS[%d]=%llX,", i, slotinfo->FRS[i]);
    }
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "\n");

    //SaveAsyncEventLogPage() will copy srbext->ExtBuf pointer and save it.
    //Should replace srbext->ExtBuf by NULL to prevent completion function free it.
    devext->SaveAsyncEventLogPage(srbext->ExtBuf);
    srbext->ExtBuf = NULL;

    if (srbext->NvmeCpl.DW3.Status.M)
        devext->GetLogPageForAsyncEvent(1);
    else
        devext->RequestAsyncEvent();
}

#pragma region ======== CSpcNvmeDevice inline routines ======== 
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
inline void CNvmeDevice::ReadNvmeRegister(NVME_ADMIN_QUEUE_ATTRIBUTES& aqa,
    NVME_ADMIN_SUBMISSION_QUEUE_BASE_ADDRESS& asq,
    NVME_ADMIN_COMPLETION_QUEUE_BASE_ADDRESS& acq,
    bool barrier)
{
    if (barrier)
        MemoryBarrier();

    aqa.AsUlong = StorPortReadRegisterUlong(DevExt, &CtrlReg->AQA.AsUlong);
    asq.AsUlonglong = StorPortReadRegisterUlong64(DevExt, &CtrlReg->ASQ.AsUlonglong);
    acq.AsUlonglong = StorPortReadRegisterUlong64(DevExt, &CtrlReg->ACQ.AsUlonglong);
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
bool CNvmeDevice::IsWorking() { return (State == NVME_STATE::RUNNING); }
bool CNvmeDevice::IsSetup() { return (State == NVME_STATE::SETUP); }
bool CNvmeDevice::IsTeardown() { return (State == NVME_STATE::TEARDOWN); }
bool CNvmeDevice::IsStop() { return (State == NVME_STATE::STOP); }
#pragma endregion

#pragma region ======== CSpcNvmeDevice ======== 
NTSTATUS CNvmeDevice::Setup(PPORT_CONFIGURATION_INFORMATION pci)
{
    NTSTATUS status = STATUS_SUCCESS;
    if(NVME_STATE::STOP != State && !this->RebalancingPnp)
        return STATUS_INVALID_DEVICE_STATE;

    State = NVME_STATE::SETUP;
    InitVars();
    DevExt = this;
    InitDpcs();
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

    ReadCtrlCap();

    status = CreateAdmQ();
    if (!NT_SUCCESS(status))
        return status;

    State = NVME_STATE::RUNNING;
    return STATUS_SUCCESS;
}
NTSTATUS CNvmeDevice::Setup(PVOID devext, PVOID pcidata, PVOID ctrlreg)
{
    NTSTATUS status = STATUS_SUCCESS;
    if (NVME_STATE::STOP != State && !this->RebalancingPnp)
        return STATUS_INVALID_DEVICE_STATE;
    if(!this->RebalancingPnp)
        RtlZeroMemory(this, sizeof(CNvmeDevice));

    State = NVME_STATE::SETUP;
    InitVars();
    DevExt = devext;
    InitDpcs();
    RtlCopyMemory(&PciCfg, pcidata, sizeof(PCI_COMMON_CONFIG));
    VendorID = PciCfg.VendorID;
    DeviceID = PciCfg.DeviceID;

    CtrlReg = (PNVME_CONTROLLER_REGISTERS)ctrlreg;
    Bar0Size = 4 * PAGE_SIZE;
    Doorbells = CtrlReg->Doorbells;

    ReadCtrlCap();

    status = CreateAdmQ();
    if (!NT_SUCCESS(status))
        return status;

    State = NVME_STATE::RUNNING;
    return STATUS_SUCCESS;
}
void CNvmeDevice::Teardown()
{
    if (!IsWorking())
        return;

    State = NVME_STATE::TEARDOWN;
    DeleteIoQ();
    DeleteAdmQ();
    State = NVME_STATE::STOP;
    if(NULL != this->CtrlReg)
    {
        StorPortFreeDeviceBase(DevExt, this->CtrlReg);
        CtrlReg = NULL;
    }

    if(NULL != MsgGroupAffinity)
    {
        delete[] MsgGroupAffinity;
        MsgGroupAffinity = NULL;
    }
}
NTSTATUS CNvmeDevice::EnableController()
{
    if (IsControllerReady())
        return STATUS_SUCCESS;

    //if set CC.EN = 1 WHEN CSTS.RDY == 1 and CC.EN == 0, it is undefined behavior.
    //we should wait controller state changing until (CC.EN == 0 and CSTS.RDY == 0).
    bool ok = WaitForCtrlerState(DeviceTimeout, FALSE, FALSE);
    if (!ok)
        return STATUS_INVALID_DEVICE_STATE;

    //before Enable, update these basic information to controller.
    //these fields only can be modified when CC.EN == 0. (plz refer to nvme 1.3 spec)
    NVME_CONTROLLER_CONFIGURATION cc = { 0 };
    cc.CSS = NVME_CSS_NVM_COMMAND_SET;
    cc.AMS = NVME_AMS_ROUND_ROBIN;
    cc.SHN = NVME_CC_SHN_NO_NOTIFICATION;
    cc.IOSQES = SUBQ_ENTRY_SIZE;
    cc.IOCQES = CPLQ_ENTRY_SIZE;
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

    //VMware NVMe 1.0 not guarantee CSTS.SHST will response?
    ok = WaitForCtrlerShst(DeviceTimeout);
    //if (!ok)
    //    goto ERROR_BSOD;

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
    
    //Disable NVMe Contoller will unregister all I/O queues automatically.
    RegisteredIoQ = 0;

    if(!NT_SUCCESS(status))
        return status;
    status = RegisterAdmQ();
    if (!NT_SUCCESS(status))
        return status;
    status = EnableController();
    return status;
}
NTSTATUS CNvmeDevice::InitNvmeStage1()
{
    NTSTATUS status = STATUS_SUCCESS;

    //Todo: supports multiple controller of NVMe v2.0  
    status = IdentifyController(NULL, &this->CtrlIdent);
    if (!NT_SUCCESS(status))
        return status;

    if (1 == this->NvmeVer.MJR && 0 == this->NvmeVer.MNR)
        status = IdentifyFirstNamespace();
    else
        status = IdentifyAllNamespaces();

    if (!NT_SUCCESS(status))
        return status;

    return status;
}
NTSTATUS CNvmeDevice::InitNvmeStage2()
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    status = SetInterruptCoalescing();
    ASSERT(NT_SUCCESS(status));

    status = SetArbitration();
    ASSERT(NT_SUCCESS(status));

    status = SetPowerManagement();
    ASSERT(NT_SUCCESS(status));

    status = SetHostBuffer();
    ASSERT(NT_SUCCESS(status));

    //optional feature
    status = SetSyncHostTime();

    //status = SetAsyncEvent();
    //if(NT_SUCCESS(status))
    //    RequestAsyncEvent();

    return STATUS_SUCCESS;
}
NTSTATUS CNvmeDevice::RestartController()
{
//***Running at DIRQL, called by HwAdapterControl
    BOOLEAN ok = FALSE;
    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    //stop handling any request BEFORE restart HBA done.
    StorPortPause(DevExt, MAXULONG);

    //[workaround]
    //StorPortQueueWorkItem() only can be called at IRQL <= DISPATCH_LEVEL.
    //And 
    //CNvmeDevice::RegisterIoQueue() should be called at IRQL < DISPATCH_LEVEL.
    //So I have to call DPC to do StorPortQueueWorkItem().
    ok = StorPortIssueDpc(DevExt, &this->RestartDpc, NULL, NULL);
    ASSERT(ok);
    return STATUS_SUCCESS;
}
NTSTATUS CNvmeDevice::IdentifyAllNamespaces()
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    CAutoPtr<ULONG, NonPagedPool, TAG_DEV_POOL> idlist(new ULONG[MAX_NS_COUNT]);
    ULONG ret_count = 0;
    status = IdentifyActiveNamespaceIdList(NULL, idlist, ret_count);
    if(!NT_SUCCESS(status))
        return status;

    //query ns one by one. NS ID is 1 based index
    ULONG *nsid_list = idlist;
    this->NamespaceCount = min(ret_count, SUPPORT_NAMESPACES);
    for (ULONG i = 0; i < NamespaceCount; i++)
    {
        if(0 == nsid_list[i])
            break;

        status = IdentifyNamespace(NULL, nsid_list[i], &this->NsData[i]);
        if (!NT_SUCCESS(status))
            return status;
    }
    return STATUS_SUCCESS;
}
NTSTATUS CNvmeDevice::IdentifyFirstNamespace()
{
    NTSTATUS status = IdentifyNamespace(NULL, 1, &this->NsData[0]);
    if(NT_SUCCESS(status))
        NamespaceCount = 1;
    return status;
}
NTSTATUS CNvmeDevice::CreateIoQueues(bool force)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    if(force)
        DeleteIoQ();

    if(0 == this->DesiredIoQ)
    {
        status = SetNumberOfIoQueue((USHORT)this->DesiredIoQ);
        if(!NT_SUCCESS(status))
            return status;
    }
    status = CreateIoQ();
    return status;
}
NTSTATUS CNvmeDevice::IdentifyController(PSPCNVME_SRBEXT srbext, PNVME_IDENTIFY_CONTROLLER_DATA ident, bool poll)
{
    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    CAutoPtr<SPCNVME_SRBEXT, NonPagedPool, TAG_SRBEXT> srbext_ptr;
    PSPCNVME_SRBEXT my_srbext = srbext;
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    
    if(NULL == srbext)
    {
        srbext_ptr.Reset(new SPCNVME_SRBEXT());
        srbext_ptr->Init(this, NULL);
        my_srbext = srbext_ptr.Get();
    }
    
    BuildCmd_IdentCtrler(my_srbext, ident);
    status = SubmitAdmCmd(my_srbext, &my_srbext->NvmeCmd);
    
    //if(poll == true) means this request comes from StartIo
    if(!NT_SUCCESS(status))
        goto END;

    do
    {
        StorPortStallExecution(StallDelay);
        if(poll)
        {
            AdmQueue->CompleteCmd();
        }
    }while(SRB_STATUS_PENDING == my_srbext->SrbStatus);

    if (SRB_STATUS_SUCCESS == my_srbext->SrbStatus)
    {
        UpdateParamsByCtrlIdent();
        status = STATUS_SUCCESS;
    }
    else
        status = STATUS_UNSUCCESSFUL;
END:
    return status;
}
NTSTATUS CNvmeDevice::IdentifyNamespace(PSPCNVME_SRBEXT srbext, ULONG nsid, PNVME_IDENTIFY_NAMESPACE_DATA data)
{
    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    CAutoPtr<SPCNVME_SRBEXT, NonPagedPool, TAG_SRBEXT> srbext_ptr;
    PSPCNVME_SRBEXT my_srbext = srbext;
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    if (NULL == my_srbext)
    {
        srbext_ptr.Reset(new SPCNVME_SRBEXT());
        srbext_ptr->Init(this, NULL);
        my_srbext = srbext_ptr.Get();
    }

    //Query ID List of all active Namespace
    BuildCmd_IdentSpecifiedNS(my_srbext, data, nsid);
    status = SubmitAdmCmd(my_srbext, &my_srbext->NvmeCmd);
    //if(poll == true) means this request comes from StartIo
    if (!NT_SUCCESS(status))
        goto END;

    do
    {
        StorPortStallExecution(StallDelay);
    } while (SRB_STATUS_PENDING == my_srbext->SrbStatus);

    if (SRB_STATUS_SUCCESS == my_srbext->SrbStatus)
        status = STATUS_SUCCESS;
    else
        status = STATUS_UNSUCCESSFUL;
END:
    return status;
}
NTSTATUS CNvmeDevice::IdentifyActiveNamespaceIdList(PSPCNVME_SRBEXT srbext, PVOID nsid_list, ULONG& ret_count)
{
//list_count is "how many elemens(not bytes) in nsid_list can store."
//nsid_list is buffer to retrieve nsid returned by this command.
//ret_count is "how many actual nsid(elements, not bytes) returned by this command".
    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    if (NULL == nsid_list)
        return STATUS_INVALID_PARAMETER;

    CAutoPtr<SPCNVME_SRBEXT, NonPagedPool, TAG_SRBEXT> srbext_ptr;
    PSPCNVME_SRBEXT my_srbext = srbext;
    NTSTATUS status = STATUS_SUCCESS;
    if (NULL == my_srbext)
    {
        srbext_ptr.Reset(new SPCNVME_SRBEXT());
        srbext_ptr->Init(this, NULL);
        my_srbext = srbext_ptr.Get();
    }

    //Query ID List of all active Namespace
    BuildCmd_IdentActiveNsidList(my_srbext, nsid_list, PAGE_SIZE);
    status = SubmitAdmCmd(my_srbext, &my_srbext->NvmeCmd);
    if (!NT_SUCCESS(status))
        return status;

    do
    {
        StorPortStallExecution(StallDelay);
    } while (SRB_STATUS_PENDING == my_srbext->SrbStatus);

    if (my_srbext->SrbStatus != SRB_STATUS_SUCCESS)
        return STATUS_UNSUCCESSFUL;

    ret_count = 0;
    ULONG *idlist = (ULONG*)nsid_list;
    for(ULONG i=0; i < MAX_NS_COUNT; i++)
    {
        if(0 == idlist[i])
            break;
        ret_count ++;
    }

    return STATUS_SUCCESS;
}
NTSTATUS CNvmeDevice::SetNumberOfIoQueue(USHORT count)
{
    //this is SET_FEATURE of NVMe Adm Command.
    //The flow is :
    //  1.host tell device "how many i/o queues I want to use".
    //  2.device reply to host "how many queues I can permit you to use".
    //CNvmeDevice::DesiredIoQ should be filled by step2's answer.
    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    CAutoPtr<SPCNVME_SRBEXT, NonPagedPool, TAG_SRBEXT> my_srbext(new SPCNVME_SRBEXT());
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    my_srbext->Init(this, NULL);

    BuildCmd_SetIoQueueCount(my_srbext, count);
    status = SubmitAdmCmd(my_srbext, &my_srbext->NvmeCmd);

    //if(poll == true) means this request comes from StartIo
    if (!NT_SUCCESS(status))
        goto END;

    do
    {
        StorPortStallExecution(StallDelay);
    } while (SRB_STATUS_PENDING == my_srbext->SrbStatus);

    if (SRB_STATUS_SUCCESS == my_srbext->SrbStatus)
    {
        status = STATUS_SUCCESS;
        DesiredIoQ = (MAXUSHORT & (my_srbext->NvmeCpl.DW0 + 1));
    }
    else
        status = STATUS_UNSUCCESSFUL;
END:

    return status;
}
NTSTATUS CNvmeDevice::SetInterruptCoalescing()
{
    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    if (0 == CoalescingTime && 0 == CoalescingThreshold)
        return STATUS_SUCCESS;

    CAutoPtr<SPCNVME_SRBEXT, NonPagedPool, TAG_SRBEXT> my_srbext(new SPCNVME_SRBEXT());
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    my_srbext->Init(this, NULL);

    BuildCmd_InterruptCoalescing(my_srbext, CoalescingThreshold, CoalescingTime);
    status = SubmitAdmCmd(my_srbext, &my_srbext->NvmeCmd);

    if (!NT_SUCCESS(status))
        return status;

    do
    {
        StorPortStallExecution(StallDelay);
    } while (SRB_STATUS_PENDING == my_srbext->SrbStatus);

    if (SRB_STATUS_SUCCESS == my_srbext->SrbStatus)
        status = STATUS_SUCCESS;
    else
        status = STATUS_UNSUCCESSFUL;

    return status;
}
NTSTATUS CNvmeDevice::SetAsyncEvent()
{
    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    if (0 == this->CtrlIdent.AERL)
        return STATUS_NOT_SUPPORTED;

    //Only support SMART health / critical AsyncEvent now.
    //No checking CtrlIdent.OAES fields.
    CAutoPtr<SPCNVME_SRBEXT, NonPagedPool, TAG_SRBEXT> srbext_ptr;
    PSPCNVME_SRBEXT my_srbext = NULL;
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    my_srbext = new SPCNVME_SRBEXT();
    srbext_ptr.Reset(my_srbext);
    srbext_ptr->Init(this, NULL);

    BuildCmd_SetAsyncEvent(my_srbext);
    status = SubmitAdmCmd(my_srbext, &my_srbext->NvmeCmd);

    do
    {
        StorPortStallExecution(StallDelay);
    } while (SRB_STATUS_PENDING == my_srbext->SrbStatus);

    if (SRB_STATUS_SUCCESS == my_srbext->SrbStatus)
        status = STATUS_SUCCESS;
    else
        status = STATUS_UNSUCCESSFUL;

    return status;
}
NTSTATUS CNvmeDevice::RequestAsyncEvent()
{
    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    PSPCNVME_SRBEXT srbext = new(NonPagedPool, TAG_SRBEXT) SPCNVME_SRBEXT;
    srbext->Init(this, NULL);
    srbext->DeleteInComplete = TRUE;
    srbext->CompletionCB = HandleAsyncEvent;
    BuildCmd_RequestAsyncEvent(srbext);
    //AsyncEvent use special CID. Don't let it overlap to normal CID range. 
    srbext->NvmeCmd.CDW0.CID = AdmQueue->Depth + 1;
    status = SubmitAdmCmd(srbext, &srbext->NvmeCmd);
    return status;
}
NTSTATUS CNvmeDevice::GetLogPageForAsyncEvent(UCHAR logid)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    PSPCNVME_SRBEXT srbext = new(NonPagedPool, TAG_SRBEXT) SPCNVME_SRBEXT();
    srbext->Init(this, NULL);
    srbext->DeleteInComplete = TRUE;
    srbext->ExtBuf = new (NonPagedPool, TAG_GENBUF) UCHAR[PAGE_SIZE];
    RtlZeroMemory(srbext->ExtBuf, PAGE_SIZE);

    switch(logid)
    {
    case NVME_LOG_PAGE_ERROR_INFO:
        srbext->CompletionCB = HandleErrorInfoLogPage;
        break;
    case NVME_LOG_PAGE_HEALTH_INFO:
        srbext->CompletionCB = HandleSmartInfoLogPage;
        break;
    case NVME_LOG_PAGE_FIRMWARE_SLOT_INFO:
        srbext->CompletionCB = HandleFwSlotInfoLogPage;
        break;
    }

    if(this->NvmeVer.AsUlong >= 0x10300)
        BuildCmd_GetLogPageV13(srbext, logid, srbext->ExtBuf, PAGE_SIZE);
    else
        BuildCmd_GetLogPage(srbext, logid, srbext->ExtBuf, PAGE_SIZE);

    status = SubmitAdmCmd(srbext, &srbext->NvmeCmd);
    return status;
}
NTSTATUS CNvmeDevice::SetArbitration()
{
    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    CAutoPtr<SPCNVME_SRBEXT, NonPagedPool, TAG_SRBEXT> srbext_ptr;
    PSPCNVME_SRBEXT my_srbext = NULL;
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    
    my_srbext = new SPCNVME_SRBEXT();
    srbext_ptr.Reset(my_srbext);
    srbext_ptr->Init(this, NULL);

    BuildCmd_SetArbitration(my_srbext);
    status = SubmitAdmCmd(my_srbext, &my_srbext->NvmeCmd);

    do
    {
        StorPortStallExecution(StallDelay);
    } while (SRB_STATUS_PENDING == my_srbext->SrbStatus);

    if (SRB_STATUS_SUCCESS == my_srbext->SrbStatus)
        status = STATUS_SUCCESS;
    else
        status = STATUS_UNSUCCESSFUL;

    return status;
}
NTSTATUS CNvmeDevice::SetSyncHostTime(PSPCNVME_SRBEXT srbext)
{
    CAutoPtr<SPCNVME_SRBEXT, NonPagedPool, TAG_SRBEXT> srbext_ptr;
    PSPCNVME_SRBEXT my_srbext = srbext;
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    if (!CtrlIdent.ONCS.Timestamp)
    {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "SetSyncHostTime() is not supported!!\n");
        return STATUS_NOT_SUPPORTED;
    }

    if (NULL == srbext)
    {
        srbext_ptr.Reset(new SPCNVME_SRBEXT());
        srbext_ptr->Init(this, NULL);
        my_srbext = srbext_ptr.Get();
    }

    //KeQuerySystemTime() get system tick(100 ns) count since 1601/1/1 00:00:00
    LARGE_INTEGER systime = { 0 };
    LARGE_INTEGER elapsed = { 0 };
    KeQuerySystemTime(&systime);
    RtlTimeToSecondsSince1970(&systime, &elapsed.LowPart);
    elapsed.QuadPart = elapsed.LowPart * 1000;

    BuildCmd_SyncHostTime(my_srbext, elapsed);
    status = SubmitAdmCmd(my_srbext, &my_srbext->NvmeCmd);

    do
    {
        StorPortStallExecution(StallDelay);
    } while (SRB_STATUS_PENDING == my_srbext->SrbStatus);

    if (SRB_STATUS_SUCCESS == my_srbext->SrbStatus)
        status = STATUS_SUCCESS;
    else
        status = STATUS_UNSUCCESSFUL;

    return status;
}
NTSTATUS CNvmeDevice::SetPowerManagement()
{
    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "SetPowerManagement() still not implemented yet!!\n");
    return STATUS_SUCCESS;
}
NTSTATUS CNvmeDevice::SetHostBuffer()
{
    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "SetHostBuffer() still not implemented yet!!\n");
    return STATUS_SUCCESS;
}
NTSTATUS CNvmeDevice::GetLbaFormat(ULONG nsid, NVME_LBA_FORMAT& format)
{
    //namespace id should be 1 based.
    //** according NVME_COMMAND::CDW0, NSID==0 if not used in command.
    //   So I guess the NSID is 1 based index....
    if (0 == nsid)
        return STATUS_INVALID_PARAMETER;

    if (0 == NamespaceCount)
        return STATUS_DEVICE_NOT_READY;

    UCHAR lba_index = NsData[nsid-1].FLBAS.LbaFormatIndex;
    RtlCopyMemory(&format, &NsData[nsid - 1].LBAF[lba_index], sizeof(NVME_LBA_FORMAT));
    return STATUS_SUCCESS;
}
NTSTATUS CNvmeDevice::GetNamespaceBlockSize(ULONG nsid, ULONG& size)
{
    //namespace id should be 1 based.
    //** according NVME_COMMAND::CDW0, NSID==0 if not used in command.
    //   So I guess the NSID is 1 based index....
    if (0 == nsid)
        return STATUS_INVALID_PARAMETER;

    if (0 == NamespaceCount)
        return STATUS_DEVICE_NOT_READY;

    UCHAR lba_index = NsData[nsid - 1].FLBAS.LbaFormatIndex;
    size = (1 << NsData[nsid - 1].LBAF[lba_index].LBADS);
    return STATUS_SUCCESS;
}
NTSTATUS CNvmeDevice::GetNamespaceTotalBlocks(ULONG nsid, ULONG64& blocks)
{
    //namespace id should be 1 based.
    //** according NVME_COMMAND::CDW0, NSID==0 if not used in command.
    //   So I guess the NSID is 1 based index....
    if (0 == nsid)
        return STATUS_INVALID_PARAMETER;

    if (0 == NamespaceCount)
        return STATUS_DEVICE_NOT_READY;

    blocks = NsData[nsid - 1].NSZE;
    return STATUS_SUCCESS;
}
NTSTATUS CNvmeDevice::SubmitAdmCmd(PSPCNVME_SRBEXT srbext, PNVME_COMMAND cmd)
{
    if(!IsWorking() || NULL == AdmQueue)
        return STATUS_DEVICE_NOT_READY;

    if(MAXUSHORT == srbext->NvmeCmd.CDW0.CID)
        srbext->NvmeCmd.CDW0.CID = AdmQueue->GetNextCid();
    return AdmQueue->SubmitCmd(srbext, cmd);
}
NTSTATUS CNvmeDevice::SubmitIoCmd(PSPCNVME_SRBEXT srbext, PNVME_COMMAND cmd)
{
    if (!IsWorking() || NULL == IoQueue || 0 == RegisteredIoQ)
        return STATUS_DEVICE_NOT_READY;

    ULONG cpu_idx = KeGetCurrentProcessorNumberEx(NULL);
    //todo: determine idx by NUMA rules to improve performance
    ULONG idx = (cpu_idx % RegisteredIoQ);

    if (MAXUSHORT == srbext->NvmeCmd.CDW0.CID)
        srbext->NvmeCmd.CDW0.CID = IoQueue[idx]->GetNextCid();

    return IoQueue[idx]->SubmitCmd(srbext, cmd);
}
void CNvmeDevice::ReleaseOutstandingSrbs()
{
    if (!IsWorking() || NULL == IoQueue || NULL == AdmQueue)
        return;

    State = NVME_STATE::RESETBUS;
    AdmQueue->GiveupAllCmd();
    for(ULONG i=0; i<RegisteredIoQ;i++)
        IoQueue[i]->GiveupAllCmd();

    State = NVME_STATE::RUNNING;
}
NTSTATUS CNvmeDevice::SetPerfOpts()
{
    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    //initialize perf options
    PERF_CONFIGURATION_DATA set_perf = { 0 };
    PERF_CONFIGURATION_DATA supported = { 0 };
    ULONG stor_status = STOR_STATUS_SUCCESS;
    //Just using STOR_PERF_VERSION_5, STOR_PERF_VERSION_6 is for Win2019 and above...
    supported.Version = STOR_PERF_VERSION_5;
    supported.Size = sizeof(PERF_CONFIGURATION_DATA);
    stor_status = StorPortInitializePerfOpts(DevExt, TRUE, &supported);
    if (STOR_STATUS_SUCCESS != stor_status)
        return FALSE;

    set_perf.Version = STOR_PERF_VERSION_5;
    set_perf.Size = sizeof(PERF_CONFIGURATION_DATA);

    //Allow multiple I/O incoming concurrently. 
    if(0 != (supported.Flags & STOR_PERF_CONCURRENT_CHANNELS))
    {
        set_perf.Flags |= STOR_PERF_CONCURRENT_CHANNELS;
        set_perf.ConcurrentChannels = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
    }
    //I don't use SGL... but Win10 don't support this flag
    if (0 != (supported.Flags & STOR_PERF_NO_SGL))
        set_perf.Flags |= STOR_PERF_NO_SGL;

    //spread DPC to all cpu. don't make single cpu too busy.
    if (0 != (supported.Flags & STOR_PERF_DPC_REDIRECTION))
        set_perf.Flags |= STOR_PERF_DPC_REDIRECTION;

    //IF not set this flag, storport will attempt to fire completion DPC on
    //original cpu which accept this I/O request.
    if (0 != (supported.Flags & STOR_PERF_DPC_REDIRECTION_CURRENT_CPU))
        set_perf.Flags |= STOR_PERF_DPC_REDIRECTION_CURRENT_CPU;

    if (0 != (supported.Flags & STOR_PERF_OPTIMIZE_FOR_COMPLETION_DURING_STARTIO))
        set_perf.Flags |= STOR_PERF_OPTIMIZE_FOR_COMPLETION_DURING_STARTIO;

    stor_status = StorPortInitializePerfOpts(DevExt, FALSE, &set_perf);
    if(STOR_STATUS_SUCCESS != stor_status)
        return STATUS_UNSUCCESSFUL;

    return STATUS_SUCCESS;
}
void CNvmeDevice::SaveAsyncEvent(PNVME_COMPLETION_DW0_ASYNC_EVENT_REQUEST event)
{
    //note: if extend AsyncEvent to multiple event, here should be refactor to 
    //      make saving AsyncEventLog atomic.
    CurrentAsyncEvent = (CurrentAsyncEvent+1) % MAX_ASYNC_EVENT_LOG;
    RtlCopyMemory(&AsyncEventLog[CurrentAsyncEvent], event,
        sizeof(NVME_COMPLETION_DW0_ASYNC_EVENT_REQUEST));
}
void CNvmeDevice::SaveAsyncEventLogPage(PVOID page)
{
    //note: if extend AsyncEvent to multiple event, here should be refactor to 
    //      make saving AsyncEventLog atomic.
    CurrentLogPage = (CurrentLogPage + 1) % MAX_ASYNC_EVENT_LOGPAGES;
    PVOID temp = AsyncEventLogPage[CurrentLogPage];
    AsyncEventLogPage[CurrentLogPage] = page;
    if(NULL != temp)
        delete[] temp;
}
bool CNvmeDevice::IsFitValidIoRange(ULONG nsid, ULONG64 offset, ULONG len)
{
    //namespace id should be 1 based.
    //** according NVME_COMMAND::CDW0, NSID==0 if not used in command.
    //   So I guess the NSID is 1 based index....
    if(!IsNsExist(nsid))
        return false;

    ULONG64 total_blocks = 0;
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    status = GetNamespaceTotalBlocks(nsid, total_blocks);
    if(!NT_SUCCESS(status))
        return false;
    ULONG64 max_block_id = total_blocks - 1;

    if((offset + len - 1) > max_block_id)
        return false;
    return true;
}
bool CNvmeDevice::IsNsExist(ULONG nsid)
{
    if (0 == nsid || nsid > NamespaceCount)
        return false;

    return (NsData[nsid-1].NSZE > 0);
}
bool CNvmeDevice::IsLunExist(UCHAR lun)
{
    return IsNsExist(LunToNsId(lun));
}
NTSTATUS CNvmeDevice::RegisterIoQueues(PSPCNVME_SRBEXT srbext)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    CAutoPtr<SPCNVME_SRBEXT, NonPagedPool, TAG_SRBEXT> temp(new SPCNVME_SRBEXT());

    if (!IsWorking())
    { 
        status = STATUS_INVALID_DEVICE_STATE;
        goto END;
    }

    for (ULONG i = 0; i < AllocatedIoQ; i++)
    {
        temp->Init(this, NULL);
//register IoQueue should register CplQ first, then SubQ.
//They are "QueuePair" .
        BuildCmd_RegIoCplQ(temp, IoQueue[i]);
        status = AdmQueue->SubmitCmd(temp, &temp->NvmeCmd);
        if(!NT_SUCCESS(status))
        {
            status = STATUS_REQUEST_ABORTED;
            goto END;
        }

        do
        {
            //when Register I/O Queues, Interrupt are already connected.
            //We just wait for ISR complete commands...
            StorPortStallExecution(StallDelay);
        } while (SRB_STATUS_PENDING == temp->SrbStatus);

        if (temp->SrbStatus != SRB_STATUS_SUCCESS)
            goto END;

        temp->Init(this, NULL);
        BuildCmd_RegIoSubQ(temp, IoQueue[i]);
        status = AdmQueue->SubmitCmd(temp, &temp->NvmeCmd);
        if (!NT_SUCCESS(status))
        {
            status = STATUS_REQUEST_ABORTED;
            goto END;
        }

        do
        {
            //when Register I/O Queues, Interrupt are already connected.
            //We just wait for ISR complete commands...
            StorPortStallExecution(StallDelay);
        } while (SRB_STATUS_PENDING == temp->SrbStatus);

        if (temp->SrbStatus != SRB_STATUS_SUCCESS)
            goto END;

        RegisteredIoQ++;
    }
    status = STATUS_SUCCESS;
END:
    if (RegisteredIoQ == DesiredIoQ)
    {
        status = STATUS_SUCCESS;
        if (NULL != srbext)
            srbext->CompleteSrb(SRB_STATUS_SUCCESS);
    }
    else
    {
        if (NULL != srbext)
            srbext->CompleteSrb(SRB_STATUS_ERROR);
    }
    return status;
}
NTSTATUS CNvmeDevice::UnregisterIoQueues(PSPCNVME_SRBEXT srbext)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    CAutoPtr<SPCNVME_SRBEXT, NonPagedPool, TAG_SRBEXT> temp(new SPCNVME_SRBEXT());
    if (!IsWorking())
    {
        status = STATUS_INVALID_DEVICE_STATE;
        goto END;
    }
    if (temp.IsNull())
    {
        status = STATUS_MEMORY_NOT_ALLOCATED;
        goto END;
    }

    for (ULONG i = 0; i < DesiredIoQ; i++)
    {
        temp->Init(this, NULL);
        //register IoQueue should register CplQ first, then SubQ.
        //They are "Pair" .
        //when UNREGISTER IoQueues, the sequence should be reversed :
        // unregister SubQ first, then CplQ.
        BuildCmd_UnRegIoSubQ(temp, IoQueue[i]);
        status = AdmQueue->SubmitCmd(temp, &temp->NvmeCmd);
        if (!NT_SUCCESS(status))
        {
            status = STATUS_REQUEST_ABORTED;
            goto END;
        }

        do
        {
            //when Register I/O Queues, Interrupt are already connected.
            //We just wait for ISR complete commands...
            StorPortStallExecution(StallDelay);
        } while (SRB_STATUS_PENDING == temp->SrbStatus);

        if (temp->SrbStatus != SRB_STATUS_SUCCESS)
            goto END;

        temp->Init(this, NULL);
        BuildCmd_UnRegIoCplQ(temp, IoQueue[i]);
        status = AdmQueue->SubmitCmd(temp, &temp->NvmeCmd);
        if (!NT_SUCCESS(status))
        {
            status = STATUS_REQUEST_ABORTED;
            goto END;
        }

        do
        {
            //when Register I/O Queues, Interrupt are already connected.
            //We just wait for ISR complete commands...
            StorPortStallExecution(StallDelay);
        } while (SRB_STATUS_PENDING == temp->SrbStatus);

        if (temp->SrbStatus != SRB_STATUS_SUCCESS)
            goto END;

        RegisteredIoQ++;
    }

END:
    if (RegisteredIoQ == DesiredIoQ)
        status = STATUS_SUCCESS;

    if (NULL != srbext)
    {
        if (NT_SUCCESS(status))
            srbext->CompleteSrb(SRB_STATUS_SUCCESS);
        else
            srbext->CompleteSrb(SRB_STATUS_ERROR);
    }
    return status;
}
NTSTATUS CNvmeDevice::CreateAdmQ()
{
    if(NULL != AdmQueue)
        return STATUS_ALREADY_INITIALIZED;

    QUEUE_PAIR_CONFIG cfg = {0};
    cfg.Depth = (USHORT)AdmDepth;
    cfg.QID = 0;
    cfg.DevExt = DevExt;
    cfg.NvmeDev = this;
    cfg.NumaNode = MM_ANY_NODE_OK;
    cfg.Type = QUEUE_TYPE::ADM_QUEUE;

    //AdmQ histroy depth should reserve one more element for AsyncEvent.
    cfg.HistoryDepth = 1 + MAX_IO_PER_LU;
    GetAdmQueueDbl(cfg.SubDbl , cfg.CplDbl);
    AdmQueue = new(NonPagedPool, TAG_NVME_QUEUE) CNvmeQueue(&cfg);
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
    asq.AsUlonglong = (ULONGLONG)subq.QuadPart;
    acq.AsUlonglong = (ULONGLONG)cplq.QuadPart;
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
    //CtrlReg->CAP.TO is timeout value in "500 ms" unit.
    //It indicates the timeout worst case of Enabling/Disabling Controller.
    //e.g. CAP.TO==3 means worst case timout to Enable/Disable controller is 1500 milli-second.
    //We should convert it to micro-seconds for StorPortStallExecution() using.
    
    ReadNvmeRegister(NvmeVer);
    ReadNvmeRegister(CtrlCap);
    DeviceTimeout = ((UCHAR)CtrlCap.TO) * (500 * 1000);
    MinPageSize = CalcMinPageSize(CtrlCap.MPSMIN);
    MaxPageSize = CalcMinPageSize(CtrlCap.MPSMAX);
    MaxTxSize = CalcMaxTxSize(CtrlIdent.MDTS, CtrlCap.MPSMIN);
    if(0 == MaxTxSize)
        MaxTxSize = DEFAULT_MAX_TXSIZE;
    MaxTxPages = (ULONG)(MaxTxSize / PAGE_SIZE);

    AdmDepth = min((USHORT)CtrlCap.MQES + 1, AdmDepth);
    IoDepth = min((USHORT)CtrlCap.MQES + 1, IoDepth);
}
bool CNvmeDevice::MapCtrlRegisters()
{
    BOOLEAN in_iospace = FALSE;
    STOR_PHYSICAL_ADDRESS bar0 = { 0 };
    INTERFACE_TYPE type = PortCfg->AdapterInterfaceType;
    //I got this mapping method by cracking stornvme.sys.
    bar0.LowPart = (PciCfg.u.type0.BaseAddresses[0] & BAR0_LOWPART_MASK);
    bar0.HighPart = PciCfg.u.type0.BaseAddresses[1];

    for (ULONG i = 0; i < AccessRangeCount; i++)
    {
        PACCESS_RANGE range = &AccessRanges[i];
        if(true == IsAddrEqual(range->RangeStart, bar0))
        {
            in_iospace = !range->RangeInMemory;
            PUCHAR addr = (PUCHAR)StorPortGetDeviceBase(
                                    DevExt, type,
                                    PortCfg->SystemIoBusNumber, bar0, 
                                    range->RangeLength, in_iospace);
            if (NULL != addr)
            {
                CtrlReg = (PNVME_CONTROLLER_REGISTERS)addr;
                Bar0Size = range->RangeLength;
                Doorbells = CtrlReg->Doorbells;
                return true;
            }
        }
    }

    return false;
}
bool CNvmeDevice::GetPciBusData(INTERFACE_TYPE type, ULONG bus, ULONG slot)
{
    ULONG size = sizeof(PciCfg);
    ULONG status = StorPortGetBusData(DevExt, type, bus, slot, &PciCfg, size);

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
//DeviceExtension is created by storport and ZEROED.
//It WILL NOT call constructor of class or struct.
//Should re-init variables again.
    ReadCacheEnabled = false;
    WriteCacheEnabled = false;
    RebalancingPnp = FALSE;
    MinPageSize = PAGE_SIZE;
    MaxPageSize = PAGE_SIZE;
    MaxTxSize = 0;
    MaxTxPages = 0;
    State = NVME_STATE::STOP;
    RegisteredIoQ = 0;
    AllocatedIoQ = 0;
    DesiredIoQ = MAX_IO_QUEUE_COUNT;
    DeviceTimeout = 2000 * STALL_TIME_US;//should be updated by CAP, unit in micro-seconds
    StallDelay = STALL_TIME_US;
    RtlZeroMemory(AccessRanges, sizeof(AccessRanges));
    AccessRangeCount = 0;
    Bar0Size = 0;
    MaxNamespaces = SUPPORT_NAMESPACES;
    IoDepth = IO_QUEUE_DEPTH;
    AdmDepth = ADMIN_QUEUE_DEPTH;
    NamespaceCount = 0;       //how many namespace active in current device?
    CoalescingThreshold = DEFAULT_INT_COALESCE_COUNT;  //how many interrupt should be coalesced into one interrupt?
    CoalescingTime = DEFAULT_INT_COALESCE_TIME;       //how long(100us unit) should interrupts be coalesced?
    VendorID = 0;
    DeviceID = 0;

    RtlZeroMemory(&NvmeVer, sizeof(NVME_VERSION));
    RtlZeroMemory(&CtrlCap, sizeof(NVME_CONTROLLER_CAPABILITIES));
    RtlZeroMemory(&CtrlIdent, sizeof(NVME_IDENTIFY_CONTROLLER_DATA));
    RtlZeroMemory(NsData, sizeof(NsData));

    //One interrupt could be handled by multiple CPU, especially in system with lots of CPU.
    //e.g. AMD EPYC 9654.
    //So MsgGroupAffinity should be allocated by CpuCount.
    CpuCount = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
    MsgGroupAffinity = (PGROUP_AFFINITY) 
                new(NonPagedPool, TAG_GROUP_AFFINITY) GROUP_AFFINITY[CpuCount];
    RtlZeroMemory(&MsgGroupAffinity, sizeof(GROUP_AFFINITY) * CpuCount);

    CtrlReg = NULL;
    PortCfg = NULL;
    Doorbells = NULL;
    AdmQueue = NULL;
    RtlZeroMemory(IoQueue, sizeof(IoQueue));
    UncachedExt = NULL;

    RtlZeroMemory(AsyncEventLog, sizeof(AsyncEventLog));
    CurrentAsyncEvent = MAXULONG;
    RtlZeroMemory(AsyncEventLogPage, sizeof(AsyncEventLogPage));
    CurrentLogPage = MAXULONG;

    RtlZeroMemory(&PciCfg, sizeof(PCI_COMMON_CONFIG));
}
void CNvmeDevice::InitDpcs()
{
    //RestartWorker and RestartDpc are used for HwAdapterControl::ScsiRestartAdapter event.
    RestartWorker = NULL;
    StorPortInitializeDpc(DevExt, &this->RestartDpc, 
                            CNvmeDevice::RestartAdapterDpc);
}
void CNvmeDevice::LoadRegistry()
{
    ULONG size = sizeof(ULONG);
    ULONG ret_size = 0;
    BOOLEAN ok = FALSE;
    UCHAR* buffer = StorPortAllocateRegistryBuffer(DevExt, &size);

    if (buffer == NULL)
        return;

    RtlZeroMemory(buffer, size);
    //ret_size should assign buffer length when calling in,
    //then it will return "how many bytes read from registry".
    ret_size = size;    
    ok = StorPortRegistryRead(DevExt, REGNAME_COALESCE_COUNT, TRUE,
        REG_DWORD, (PUCHAR) buffer, &ret_size);
    if(ok)
        CoalescingThreshold = (UCHAR) *((ULONG*)buffer);

    RtlZeroMemory(buffer, size);
    ret_size = size;
    ok = StorPortRegistryRead(DevExt, REGNAME_COALESCE_TIME, TRUE,
        REG_DWORD, (PUCHAR)buffer, &ret_size);
    if (ok)
        CoalescingTime = (UCHAR) *((ULONG*)buffer);
    
    RtlZeroMemory(buffer, size);
    ret_size = size;
    ok = StorPortRegistryRead(DevExt, REGNAME_ADMQ_DEPTH, TRUE,
        REG_DWORD, (PUCHAR)buffer, &ret_size);
    if (ok)
        AdmDepth = (USHORT) *((ULONG*)buffer);

    RtlZeroMemory(buffer, size);
    ret_size = size;
    ok = StorPortRegistryRead(DevExt, REGNAME_IOQ_DEPTH, TRUE,
        REG_DWORD, (PUCHAR)buffer, &ret_size);
    if (ok)
        IoDepth = (USHORT) *((ULONG*)buffer);

    RtlZeroMemory(buffer, size);
    ret_size = size;
    ok = StorPortRegistryRead(DevExt, REGNAME_IOQ_COUNT, TRUE,
        REG_DWORD, (PUCHAR)buffer, &ret_size);
    if (ok)
        DesiredIoQ = *((ULONG*)buffer);

    StorPortFreeRegistryBuffer(DevExt, buffer);
}
NTSTATUS CNvmeDevice::CreateIoQ()
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    QUEUE_PAIR_CONFIG cfg = { 0 };
    cfg.NvmeDev = this;
    cfg.Depth = IoDepth;
    cfg.NumaNode = 0;
    cfg.Type = QUEUE_TYPE::IO_QUEUE;
    cfg.HistoryDepth = MAX_IO_PER_LU + 1;    //HistoryDepth should equal to MaxScsiTag (ScsiTag Depth).

    for(USHORT i=0; i<DesiredIoQ; i++)
    {
        if(NULL != IoQueue[i])
            continue;
        //Dbl[0] is for AdminQ
        cfg.QID = i + 1;
        this->GetQueueDbl(cfg.QID, cfg.SubDbl, cfg.CplDbl);
        CNvmeQueue* queue = new(NonPagedPool, TAG_NVME_QUEUE) CNvmeQueue;
        status = queue->Setup(&cfg);
        if(!NT_SUCCESS(status))
        {
            delete queue;
            return status;
        }
        IoQueue[i] = queue;
        AllocatedIoQ++;
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
        AllocatedIoQ--;
    }

    RtlZeroMemory(IoQueue, sizeof(CNvmeQueue*) * MAX_IO_QUEUE_COUNT);
    return STATUS_SUCCESS;
}
void CNvmeDevice::UpdateParamsByCtrlIdent()
{
    this->MaxTxSize = (ULONG)((1 << this->CtrlIdent.MDTS) * this->MinPageSize);
    this->MaxTxPages = (ULONG)(this->MaxTxSize / PAGE_SIZE);
}

#pragma endregion

