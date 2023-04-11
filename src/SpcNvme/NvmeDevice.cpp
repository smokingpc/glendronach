#include "pch.h"

BOOLEAN CNvmeDevice::NvmeMsixISR(IN PVOID devext, IN ULONG msgid)
{
    CNvmeDevice* nvme = (CNvmeDevice*)devext;
    if(nvme->IsWorking())
    {
        switch(msgid)
        {
        case 0:
            StorPortIssueDpc(devext, &nvme->AdmQueue->QueueCplDpc, nvme->AdmQueue, NULL);
            break;
        default:
            for(ULONG i=0; i<nvme->RegisteredIoQ; i++)
            {
                if (NULL != nvme->IoQueue[i])
                    StorPortIssueDpc(devext, &nvme->IoQueue[i]->QueueCplDpc, nvme->IoQueue[i], NULL);
            }

            break;
        }
    }
    return TRUE;
}


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
inline void CNvmeDevice::UpdateMaxTxSize()
{
    this->MaxTxSize = (ULONG)((1 << this->CtrlIdent.MDTS) * this->MinPageSize);
    this->MaxTxPages = (ULONG)(this->MaxTxSize / PAGE_SIZE);
}
#if 0
ULONG CNvmeDevice::MinPageSize()
{
    return (ULONG)(1 << (12 + CtrlCap.MPSMIN));
}
ULONG CNvmeDevice::MaxPageSize()
{
    return (ULONG)(1 << (12 + CtrlCap.MPSMAX));
}
ULONG CNvmeDevice::MaxTxSize()
{
    return (ULONG)((1 << this->CtrlIdent.MDTS) * MinPageSize());
}
ULONG CNvmeDevice::MaxTxPages()
{
    return (ULONG)(MaxTxSize() / PAGE_SIZE);
}
ULONG CNvmeDevice::NsCount()
{
    return NamespaceCount;
}
#endif
bool CNvmeDevice::IsWorking() { return (State == NVME_STATE::RUNNING); }
bool CNvmeDevice::IsSetup() { return (State == NVME_STATE::SETUP); }
bool CNvmeDevice::IsTeardown() { return (State == NVME_STATE::TEARDOWN); }
bool CNvmeDevice::IsStop() { return (State == NVME_STATE::STOP); }
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
NTSTATUS CNvmeDevice::InitIdentifyCtrl()
{
    return IdentifyController(NULL, &this->CtrlIdent, true);
}
NTSTATUS CNvmeDevice::InitIdentifyNS()
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    CAutoPtr<ULONG, NonPagedPool, DEV_POOL_TAG> idlist(new ULONG[NVME_CONST::MAX_NS_COUNT]);
    ULONG ret_count = 0;
    status = IdentifyActiveNamespaceIdList(NULL, idlist, ret_count, true);
    if(!NT_SUCCESS(status))
        return status;

    //query ns one by one. NS ID is 1 based index
    ULONG *nsid_list = idlist;
    this->NamespaceCount = min(ret_count, NVME_CONST::SUPPORT_NAMESPACES);
    for (ULONG i = 0; i < NamespaceCount; i++)
    {
        if(0 == nsid_list[i])
            break;

        status = IdentifyNamespace(NULL, nsid_list[i], &this->NsData[i], true);
        if (!NT_SUCCESS(status))
            return status;
    }
    return STATUS_SUCCESS;
}
NTSTATUS CNvmeDevice::InitIdentifyFirstNS()
{
    NTSTATUS status = IdentifyNamespace(NULL, 1, &this->NsData[0], true);
    if(NT_SUCCESS(status))
        NamespaceCount = 1;
    return status;
}
NTSTATUS CNvmeDevice::InitCreateIoQueues()
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    status = SetNumberOfIoQueue(NVME_CONST::IO_QUEUE_COUNT);
    if(!NT_SUCCESS(status))
        return status;

    status = CreateIoQ();
    return status;
}
NTSTATUS CNvmeDevice::IdentifyController(PSPCNVME_SRBEXT srbext, PNVME_IDENTIFY_CONTROLLER_DATA ident, bool poll)
{
    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    CAutoPtr<SPCNVME_SRBEXT, NonPagedPool, DEV_POOL_TAG> srbext_ptr;
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
    if(!NT_SUCCESS(status) || !poll)
        goto END;

    do
    {
        ULONG done_count = 0;
        StorPortStallExecution(StallDelay);
        status = AdmQueue->CompleteCmd(4, done_count);
        if (done_count > 0)
            break;
    }while(SRB_STATUS_PENDING == my_srbext->SrbStatus);

    if (SRB_STATUS_SUCCESS == my_srbext->SrbStatus)
    {
        UpdateMaxTxSize();
        status = STATUS_SUCCESS;
    }
    else
        status = STATUS_UNSUCCESSFUL;
END:

    return status;
}
NTSTATUS CNvmeDevice::IdentifyNamespace(PSPCNVME_SRBEXT srbext, ULONG nsid, PNVME_IDENTIFY_NAMESPACE_DATA data, bool poll)
{
    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    CAutoPtr<SPCNVME_SRBEXT, NonPagedPool, DEV_POOL_TAG> srbext_ptr;
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
    if (!NT_SUCCESS(status) || !poll)
        goto END;

    do
    {
        ULONG done_count = 0;
        StorPortStallExecution(StallDelay);
        status = AdmQueue->CompleteCmd(4, done_count);
        if (done_count > 0)
            break;
    } while (SRB_STATUS_PENDING == my_srbext->SrbStatus);

    if (SRB_STATUS_SUCCESS == my_srbext->SrbStatus)
        status = STATUS_SUCCESS;
    else
        status = STATUS_UNSUCCESSFUL;
END:
    return status;
}
NTSTATUS CNvmeDevice::IdentifyActiveNamespaceIdList(PSPCNVME_SRBEXT srbext, PVOID nsid_list, ULONG& ret_count, bool poll)
{
//list_count is "how many elemens(not bytes) in nsid_list can store."
//nsid_list is buffer to retrieve nsid returned by this command.
//ret_count is "how many actual nsid(elements, not bytes) returned by this command".
    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    if (NULL == nsid_list)
        return STATUS_INVALID_PARAMETER;

    CAutoPtr<SPCNVME_SRBEXT, NonPagedPool, DEV_POOL_TAG> srbext_ptr;
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
    if (!NT_SUCCESS(status) || !poll)
        return status;

    do
    {
        ULONG done_count = 0;
        StorPortStallExecution(StallDelay);
        status = AdmQueue->CompleteCmd(1, done_count);
        if (done_count > 0)
            break;
    } while (SRB_STATUS_PENDING == my_srbext->SrbStatus);

    if (my_srbext->SrbStatus != SRB_STATUS_SUCCESS)
        return STATUS_UNSUCCESSFUL;

    ret_count = 0;
    ULONG *idlist = (ULONG*)nsid_list;
    for(ULONG i=0; i < NVME_CONST::MAX_NS_COUNT; i++)
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

    CAutoPtr<SPCNVME_SRBEXT, NonPagedPool, DEV_POOL_TAG> my_srbext(new SPCNVME_SRBEXT());
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    my_srbext->Init(this, NULL);

    BuildCmd_SetIoQueueCount(my_srbext, count);
    status = SubmitAdmCmd(my_srbext, &my_srbext->NvmeCmd);

    //if(poll == true) means this request comes from StartIo
    if (!NT_SUCCESS(status))
        goto END;

    do
    {
        ULONG done_count = 0;
        StorPortStallExecution(StallDelay);
        status = AdmQueue->CompleteCmd(4, done_count);
        if (done_count > 0)
            break;
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

    CAutoPtr<SPCNVME_SRBEXT, NonPagedPool, DEV_POOL_TAG> my_srbext(new SPCNVME_SRBEXT());
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    my_srbext->Init(this, NULL);

    BuildCmd_InterruptCoalescing(my_srbext, CoalescingThreshold, CoalescingTime);
    status = SubmitAdmCmd(my_srbext, &my_srbext->NvmeCmd);

    if (!NT_SUCCESS(status))
        return status;

    do
    {
        ULONG done_count = 0;
        StorPortStallExecution(StallDelay);
        status = AdmQueue->CompleteCmd(4, done_count);
        if (done_count > 0)
            break;
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

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "SetAsyncEvent() still not implemented yet!!\n");
    return STATUS_SUCCESS;
}
NTSTATUS CNvmeDevice::SetArbitration()
{
    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "SetArbitration() still not implemented yet!!\n");
    return STATUS_SUCCESS;
}
NTSTATUS CNvmeDevice::SetSyncHostTime()
{
    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "SetSyncHostTime() still not implemented yet!!\n");
    return STATUS_SUCCESS;
}
NTSTATUS CNvmeDevice::SetPowerManagement()
{
    if (!IsWorking())
        return STATUS_INVALID_DEVICE_STATE;

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "SetPowerManagement() still not implemented yet!!\n");
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
    return AdmQueue->SubmitCmd(srbext, cmd);
}
NTSTATUS CNvmeDevice::SubmitIoCmd(PSPCNVME_SRBEXT srbext, PNVME_COMMAND cmd)
{
    if (!IsWorking() || NULL == IoQueue)
        return STATUS_DEVICE_NOT_READY;

    ULONG cpu_idx = KeGetCurrentProcessorNumberEx(NULL);
    ULONG qid = cpu_idx % RegisteredIoQ;

    return IoQueue[qid]->SubmitCmd(srbext, cmd);
}

bool CNvmeDevice::IsInValidIoRange(ULONG nsid, ULONG64 offset, ULONG len)
{
    //namespace id should be 1 based.
    //** according NVME_COMMAND::CDW0, NSID==0 if not used in command.
    //   So I guess the NSID is 1 based index....
    if (0 == nsid || 0 == NamespaceCount)
        return false;

    ULONG64 max_block_id = NsData[nsid - 1].NSZE - 1;
    if((offset + len - 1) > max_block_id)
        return false;
    return true;
}
NTSTATUS CNvmeDevice::RegisterIoQ(PSPCNVME_SRBEXT srbext)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    CAutoPtr<SPCNVME_SRBEXT, NonPagedPool, DEV_POOL_TAG> temp(new SPCNVME_SRBEXT());

    if (!IsWorking())
    { 
        status = STATUS_INVALID_DEVICE_STATE;
        goto END;
    }

    for (ULONG i = 0; i < AllocatedIoQ; i++)
    {
        temp->Init(this, NULL);
//register IoQueue should register CplQ first, then SubQ.
//They are "Pair" .
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

END:
    if (RegisteredIoQ == DesiredIoQ)
    {
        status = STATUS_SUCCESS;
        if (NULL != srbext)
            srbext->CompleteSrbWithStatus(SRB_STATUS_SUCCESS);
    }
    else
    {
        if (NULL != srbext)
            srbext->CompleteSrbWithStatus(SRB_STATUS_ERROR);
    }
    return status;
}
NTSTATUS CNvmeDevice::UnregisterIoQ(PSPCNVME_SRBEXT srbext)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    CAutoPtr<SPCNVME_SRBEXT, NonPagedPool, DEV_POOL_TAG> temp(new SPCNVME_SRBEXT());
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
            srbext->SetStatus(SRB_STATUS_SUCCESS);
        else
            srbext->SetStatus(SRB_STATUS_ERROR);
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
    //CtrlReg->CAP.TO is timeout value in 500 ms.
    //It indicates the timeout worst case of Enabling/Disabling Controller.
    //e.g. CAP.TO==3 means worst case timout to Enable/Disable controller is 1500 milli-second.
    //We should convert it to micro-seconds for StorPortStallExecution() using.
    
    ReadNvmeRegister(NvmeVer);
    ReadNvmeRegister(CtrlCap);
    DeviceTimeout = ((UCHAR)CtrlCap.TO) * (500 * 1000);
    MinPageSize = (ULONG)(1 << (12 + CtrlCap.MPSMIN));
    MaxPageSize = (ULONG)(1 << (12 + CtrlCap.MPSMAX));
    MaxTxSize = (ULONG)((1 << this->CtrlIdent.MDTS) * MinPageSize);
    if(0 == MaxTxSize)
        MaxTxSize = NVME_CONST::DEFAULT_MAX_TXSIZE;
    MaxTxPages = (ULONG)(MaxTxSize / PAGE_SIZE);

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
    AllocatedIoQ = 0;
    DesiredIoQ = NVME_CONST::IO_QUEUE_COUNT;
    DeviceTimeout = 2000 * NVME_CONST::STALL_TIME_US;        //should be updated by CAP, unit in micro-seconds
    StallDelay = NVME_CONST::STALL_TIME_US;
    AccessRangeCount = 0;
    NamespaceCount = 0;
    CoalescingThreshold = DEFAULT_INT_COALESCE_COUNT;
    CoalescingTime = DEFAULT_INT_COALESCE_TIME;

    ReadCacheEnabled = WriteCacheEnabled = false;

    RtlZeroMemory(AccessRanges, sizeof(ACCESS_RANGE)* ACCESS_RANGE_COUNT);

    MaxNamespaces = NVME_CONST::SUPPORT_NAMESPACES;
    AdmDepth = NVME_CONST::ADMIN_QUEUE_DEPTH;
    IoDepth = NVME_CONST::IO_QUEUE_DEPTH;
    AdmQueue = NULL;
    RtlZeroMemory(IoQueue, sizeof(CNvmeQueue*) * MAX_IO_QUEUE_COUNT);

    RtlZeroMemory(&PciCfg, sizeof(PCI_COMMON_CONFIG));
    RtlZeroMemory(&NvmeVer, sizeof(NVME_VERSION));
    RtlZeroMemory(&CtrlCap, sizeof(NVME_CONTROLLER_CAPABILITIES));
    RtlZeroMemory(&CtrlIdent, sizeof(NVME_IDENTIFY_CONTROLLER_DATA));

    for(ULONG i=0; i< NVME_CONST::SUPPORT_NAMESPACES; i++)
    {
        memset(NsData + i, 0xFF, sizeof(NVME_IDENTIFY_NAMESPACE_DATA));
    }
}
void CNvmeDevice::LoadRegistry()
{
    ULONG size = sizeof(ULONG);
    BOOLEAN ok = FALSE;

    UCHAR* buffer = StorPortAllocateRegistryBuffer(this, &size);
    if (buffer == NULL)
        return;

    RtlZeroMemory(buffer, size);
    ok = StorPortRegistryRead(this, REGNAME_COALESCE_COUNT, TRUE, 
                        MINIPORT_REG_DWORD, (PUCHAR) buffer, &size);
    if(ok)
        CoalescingThreshold = (UCHAR) *((ULONG*)buffer);

    RtlZeroMemory(buffer, size);
    ok = StorPortRegistryRead(this, REGNAME_COALESCE_TIME, TRUE,
        MINIPORT_REG_DWORD, (PUCHAR)buffer, &size);
    if (ok)
        CoalescingTime = (UCHAR) *((ULONG*)buffer);

    RtlZeroMemory(buffer, size);
    ok = StorPortRegistryRead(this, REGNAME_ADMQ_DEPTH, TRUE,
        MINIPORT_REG_DWORD, (PUCHAR)buffer, &size);
    if (ok)
        AdmDepth = (USHORT) *((ULONG*)buffer);

    RtlZeroMemory(buffer, size);
    ok = StorPortRegistryRead(this, REGNAME_IOQ_DEPTH, TRUE,
        MINIPORT_REG_DWORD, (PUCHAR)buffer, &size);
    if (ok)
        IoDepth = (USHORT) *((ULONG*)buffer);

    RtlZeroMemory(buffer, size);
    ok = StorPortRegistryRead(this, REGNAME_IOQ_COUNT, TRUE,
        MINIPORT_REG_DWORD, (PUCHAR)buffer, &size);
    if (ok)
        DesiredIoQ = *((ULONG*)buffer);

    StorPortFreeRegistryBuffer(this, buffer);
}
//void CNvmeDevice::DoQueueCompletion(CNvmeQueue* queue)
//{
//    ULONG done = 0;
//    NTSTATUS status = STATUS_UNSUCCESSFUL;
//    status = queue->CompleteCmd(queue->GetQueueDepth(), done);
//    //todo: error checking and log
//}
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
        //Dbl[0] is for AdminQ
        this->GetQueueDbl(i+1, cfg.SubDbl, cfg.CplDbl);
        cfg.QID = i + 1;
        status = queue->Setup(&cfg);
        if(!NT_SUCCESS(status))
            return status;
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

#pragma endregion

