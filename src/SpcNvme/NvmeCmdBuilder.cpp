#include "pch.h"
UCHAR BuiildCmd_ReadWrite(PSPCNVME_SRBEXT srbext, ULONG64 offset, ULONG blocks, bool is_write)
{
    PNVME_COMMAND cmd = &srbext->NvmeCmd;
    RtlZeroMemory(cmd, sizeof(NVME_COMMAND));
    ULONG nsid = srbext->Lun() + 1;

    if (!srbext->DevExt->IsInValidIoRange(nsid, offset, blocks))
        return SRB_STATUS_ERROR;

    cmd->CDW0.OPC = (is_write)? NVME_NVM_COMMAND_WRITE : NVME_NVM_COMMAND_READ;
    cmd->NSID = nsid;
    BuildPrp(srbext, cmd, srbext->DataBuf(), srbext->DataBufLen());
    cmd->u.READWRITE.LBALOW = (ULONG)(offset & 0xFFFFFFFFULL);
    cmd->u.READWRITE.LBAHIGH = (ULONG)(offset >> 32);
    cmd->u.READWRITE.CDW12.NLB = blocks - 1;
    cmd->CDW0.CID = srbext->ScsiQTag();

    return SRB_STATUS_SUCCESS;
}

//to build NVME_COMMAND for IdentifyController command
void BuildCmd_IdentCtrler(PSPCNVME_SRBEXT srbext, PNVME_IDENTIFY_CONTROLLER_DATA data)
{
    PNVME_COMMAND cmd = &srbext->NvmeCmd;
    RtlZeroMemory(cmd, sizeof(NVME_COMMAND));
    cmd->CDW0.OPC = NVME_ADMIN_COMMAND_IDENTIFY;
    cmd->NSID = NVME_CONST::UNSPECIFIC_NSID;
    cmd->u.IDENTIFY.CDW10.CNS = NVME_IDENTIFY_CNS_CONTROLLER;
    cmd->u.IDENTIFY.CDW10.CNTID = 0;

    BuildPrp(srbext, cmd, (PVOID) data, sizeof(NVME_IDENTIFY_CONTROLLER_DATA));
}
void BuildCmd_IdentActiveNsidList(PSPCNVME_SRBEXT srbext, PVOID nsid_list, size_t list_size)
{
//nsid_list is a ULONG array buffer to retrieve all nsid which is active in this NVMe.
//list_size is size IN BYTES of nsid_list.
    PNVME_COMMAND cmd = &srbext->NvmeCmd;
    RtlZeroMemory(cmd, sizeof(NVME_COMMAND));
    cmd->CDW0.OPC = NVME_ADMIN_COMMAND_IDENTIFY;
    cmd->NSID = NVME_CONST::UNSPECIFIC_NSID;
    cmd->u.IDENTIFY.CDW10.CNS = NVME_IDENTIFY_CNS_ACTIVE_NAMESPACES;

    BuildPrp(srbext, cmd, nsid_list, list_size);
}

void BuildCmd_IdentSpecifiedNS(PSPCNVME_SRBEXT srbext, PNVME_IDENTIFY_NAMESPACE_DATA data, ULONG nsid)
{
    PNVME_COMMAND cmd = &srbext->NvmeCmd;
    RtlZeroMemory(cmd, sizeof(NVME_COMMAND));
    cmd->CDW0.OPC = NVME_ADMIN_COMMAND_IDENTIFY;
    cmd->NSID = nsid;
    cmd->u.IDENTIFY.CDW10.CNS = NVME_IDENTIFY_CNS_SPECIFIC_NAMESPACE;

    BuildPrp(srbext, cmd, (PVOID)data, sizeof(NVME_IDENTIFY_NAMESPACE_DATA));
}
void BuildCmd_IdentAllNSList(PSPCNVME_SRBEXT srbext, PVOID ns_buf, size_t buf_size)
{
    //ns_buf is a array to retrieve all NameSpace list.
    //buf_size is SIZE IN BYTES of ns_buf.
    PNVME_COMMAND cmd = &srbext->NvmeCmd;
    RtlZeroMemory(cmd, sizeof(NVME_COMMAND));
    cmd->CDW0.OPC = NVME_ADMIN_COMMAND_IDENTIFY;
    cmd->NSID = NVME_CONST::UNSPECIFIC_NSID;
    cmd->u.IDENTIFY.CDW10.CNS = NVME_IDENTIFY_CNS_ALLOCATED_NAMESPACE_LIST;

    BuildPrp(srbext, cmd, (PVOID)ns_buf, buf_size);
}
void BuildCmd_SetIoQueueCount(PSPCNVME_SRBEXT srbext, USHORT count)
{
    PNVME_COMMAND cmd = &srbext->NvmeCmd;
    RtlZeroMemory(cmd, sizeof(NVME_COMMAND));
    cmd->CDW0.OPC = NVME_ADMIN_COMMAND_SET_FEATURES;
    cmd->u.SETFEATURES.CDW10.FID = NVME_FEATURE_NUMBER_OF_QUEUES;

    //NSQ and NCQ should be 0 based.
    cmd->u.SETFEATURES.CDW11.NumberOfQueues.NSQ =
        cmd->u.SETFEATURES.CDW11.NumberOfQueues.NCQ = count - 1;
}
void BuildCmd_RegIoSubQ(PSPCNVME_SRBEXT srbext, CNvmeQueue *queue)
{
    PNVME_COMMAND cmd = &srbext->NvmeCmd;
    PHYSICAL_ADDRESS paddr = {0};
    RtlZeroMemory(cmd, sizeof(NVME_COMMAND));
    queue->GetSubQAddr(&paddr);

    cmd->CDW0.OPC = NVME_ADMIN_COMMAND_CREATE_IO_SQ;
    cmd->PRP1 = (ULONG64)paddr.QuadPart;
    cmd->NSID = NVME_CONST::UNSPECIFIC_NSID;
    cmd->u.CREATEIOSQ.CDW10.QID = queue->GetQueueID();
    cmd->u.CREATEIOSQ.CDW10.QSIZE = queue->GetQueueDepth();
    cmd->u.CREATEIOSQ.CDW11.CQID = queue->GetQueueID();
    cmd->u.CREATEIOSQ.CDW11.PC = 1;

    if(queue->GetQueueType() == QUEUE_TYPE::ADM_QUEUE)
        cmd->u.CREATEIOSQ.CDW11.QPRIO = NVME_NVM_QUEUE_PRIORITY_URGENT;
    else
        cmd->u.CREATEIOSQ.CDW11.QPRIO = NVME_NVM_QUEUE_PRIORITY_HIGH;
}
void BuildCmd_RegIoCplQ(PSPCNVME_SRBEXT srbext, CNvmeQueue* queue)
{
    PNVME_COMMAND cmd = &srbext->NvmeCmd;
    PHYSICAL_ADDRESS paddr = { 0 };
    RtlZeroMemory(cmd, sizeof(NVME_COMMAND));
    queue->GetCplQAddr(&paddr);

    cmd->CDW0.OPC = NVME_ADMIN_COMMAND_CREATE_IO_CQ;
    cmd->PRP1 = (ULONG64)paddr.QuadPart;
    cmd->NSID = NVME_CONST::UNSPECIFIC_NSID;
    cmd->u.CREATEIOCQ.CDW10.QID = queue->GetQueueID();
    cmd->u.CREATEIOCQ.CDW10.QSIZE = queue->GetQueueDepth();
    cmd->u.CREATEIOCQ.CDW11.IEN = TRUE;
    cmd->u.CREATEIOCQ.CDW11.IV = (queue->GetQueueType() == QUEUE_TYPE::ADM_QUEUE ? 0 : 1 );
    cmd->u.CREATEIOCQ.CDW11.PC = TRUE;
}
void BuildCmd_UnRegIoSubQ(PSPCNVME_SRBEXT srbext, CNvmeQueue* queue)
{
    PNVME_COMMAND cmd = &srbext->NvmeCmd;
    RtlZeroMemory(cmd, sizeof(NVME_COMMAND));
    cmd->CDW0.OPC = NVME_ADMIN_COMMAND_DELETE_IO_SQ;
    cmd->NSID = NVME_CONST::UNSPECIFIC_NSID;
    cmd->u.CREATEIOSQ.CDW10.QID = queue->GetQueueID();
}
void BuildCmd_UnRegIoCplQ(PSPCNVME_SRBEXT srbext, CNvmeQueue* queue)
{
    PNVME_COMMAND cmd = &srbext->NvmeCmd;
    RtlZeroMemory(cmd, sizeof(NVME_COMMAND));
    cmd->CDW0.OPC = NVME_ADMIN_COMMAND_DELETE_IO_CQ;
    cmd->NSID = NVME_CONST::UNSPECIFIC_NSID;
    cmd->u.CREATEIOSQ.CDW10.QID = queue->GetQueueID();
}
void BuildCmd_InterruptCoalescing(PSPCNVME_SRBEXT srbext, UCHAR threshold, UCHAR interval)
{
//threshold : how many interrupt collected then fire interrupt once?
//interval : how much time to waiting collect coalesced interrupt?
//=> if (merged interrupt >= threshold || waiting merge time >= interval), then device fire INTERRUPT once
    PNVME_COMMAND cmd = &srbext->NvmeCmd;
    RtlZeroMemory(cmd, sizeof(NVME_COMMAND));
    cmd->CDW0.OPC = NVME_ADMIN_COMMAND_SET_FEATURES;
    cmd->NSID = NVME_CONST::UNSPECIFIC_NSID;
    cmd->u.SETFEATURES.CDW10.FID = NVME_FEATURE_INTERRUPT_COALESCING;
    cmd->u.SETFEATURES.CDW11.InterruptCoalescing.THR = threshold;
    cmd->u.SETFEATURES.CDW11.InterruptCoalescing.TIME = interval;
}
void BuildCmd_SetArbitration(PSPCNVME_SRBEXT srbext)
{
    PNVME_COMMAND cmd = &srbext->NvmeCmd;
    RtlZeroMemory(cmd, sizeof(NVME_COMMAND));
    cmd->CDW0.OPC = NVME_ADMIN_COMMAND_SET_FEATURES;
    cmd->NSID = NVME_CONST::UNSPECIFIC_NSID;
    cmd->u.SETFEATURES.CDW10.FID = NVME_FEATURE_ARBITRATION;
    cmd->u.SETFEATURES.CDW11.Arbitration.AB = NVME_CONST::AB_BURST;
    cmd->u.SETFEATURES.CDW11.Arbitration.HPW = NVME_CONST::AB_HPW;
    cmd->u.SETFEATURES.CDW11.Arbitration.MPW = NVME_CONST::AB_MPW;
    cmd->u.SETFEATURES.CDW11.Arbitration.LPW = NVME_CONST::AB_LPW;
}
void BuildCmd_SyncHostTime(PSPCNVME_SRBEXT srbext, LARGE_INTEGER *timestamp)
{
    UNREFERENCED_PARAMETER(timestamp);
    //KeQuerySystemTime() get system tick(100 ns) count since 1601/1/1 00:00:00
    LARGE_INTEGER systime = { 0 };
    PNVME_COMMAND cmd = &srbext->NvmeCmd;
    LARGE_INTEGER elapsed = { 0 };

    KeQuerySystemTime(&systime);
    RtlTimeToSecondsSince1970(&systime, &elapsed.LowPart);
    elapsed.QuadPart = elapsed.LowPart * 1000;

    RtlZeroMemory(cmd, sizeof(NVME_COMMAND));
    cmd->CDW0.OPC = NVME_ADMIN_COMMAND_SET_FEATURES;
    cmd->NSID = NVME_CONST::UNSPECIFIC_NSID;
    cmd->u.SETFEATURES.CDW10.FID = NVME_FEATURE_TIMESTAMP;

    //PVOID timestamp = NULL;
    //ULONG size = PAGE_SIZE;
    //ULONG status = StorPortAllocatePool(devext, size, TAG_GENBUF, &timestamp);
    //if (STOR_STATUS_SUCCESS != status)
    //    return FALSE;
    //RtlZeroMemory(timestamp, size);
    //RtlCopyMemory(timestamp, &elapsed, sizeof(LARGE_INTEGER));

    //cmd.PRP1 = StorPortGetPhysicalAddress(devext, NULL, timestamp, &size).QuadPart;

    ////implement wait
    //return STATUS_INTERNAL_ERROR;
    ////submit command
    //bool ok = devext->AdminQueue->SubmitCmd(&cmd, NULL, CMD_CTX_TYPE::WAIT_EVENT);
}

#if 0
NTSTATUS SetFeature_InterruptCoalescing(PSPCNVME_DEVEXT devext, bool wait)
{
    UNREFERENCED_PARAMETER(devext);
    UNREFERENCED_PARAMETER(wait);

    return STATUS_NOT_IMPLEMENTED;
//    KIRQL irql;
//    StorPortGetCurrentIrql((PVOID)devext, &irql);
//    ASSERT(irql == PASSIVE_LEVEL);
//
//    NVME_COMMAND cmd = {0};
//    cmd.CDW0.OPC = NVME_ADMIN_COMMAND_SET_FEATURES;
//    cmd.NSID = NVME_CONST::UNSPECIFIC_NSID;
//    cmd.u.SETFEATURES.CDW10.FID = NVME_FEATURE_INTERRUPT_COALESCING;
//    cmd.u.SETFEATURES.CDW11.InterruptCoalescing.THR = NVME_CONST::INTCOAL_THRESHOLD;
//    cmd.u.SETFEATURES.CDW11.InterruptCoalescing.TIME = NVME_CONST::INTCOAL_TIME;
//
//    //submit command
//    bool ok = devext->AdminQueue->SubmitCmd(&cmd, NULL, true);
//    if(!ok)
//        return STATUS_INTERNAL_ERROR;
//
//    if(!wait)
//        return STATUS_SUCCESS;
//
////todo: refactor NVME_STATUS replying
//    NVME_COMMAND_STATUS nvme_status = {0};
//    NTSTATUS status = WaitAndPollCompletion(nvme_status, devext, devext->AdminQueue, &cmd);
//    return status;
}
NTSTATUS SetFeature_Arbitration(PSPCNVME_DEVEXT devext, bool wait)
{
    UNREFERENCED_PARAMETER(devext);
    UNREFERENCED_PARAMETER(wait);
    return STATUS_NOT_IMPLEMENTED;
    //KIRQL irql;
    //StorPortGetCurrentIrql((PVOID)devext, &irql);
    //ASSERT(irql == PASSIVE_LEVEL);

    //NVME_COMMAND cmd = { 0 };
    //cmd.CDW0.OPC = NVME_ADMIN_COMMAND_SET_FEATURES;
    //cmd.NSID = NVME_CONST::UNSPECIFIC_NSID;
    //cmd.u.SETFEATURES.CDW10.FID = NVME_FEATURE_ARBITRATION;
    //cmd.u.SETFEATURES.CDW11.Arbitration.AB = NVME_CONST::AB_BURST;
    //cmd.u.SETFEATURES.CDW11.Arbitration.HPW = NVME_CONST::AB_HPW;
    //cmd.u.SETFEATURES.CDW11.Arbitration.MPW = NVME_CONST::AB_MPW;
    //cmd.u.SETFEATURES.CDW11.Arbitration.LPW = NVME_CONST::AB_LPW;

    ////submit command
    //bool ok = devext->AdminQueue->SubmitCmd(&cmd, NULL, true);
    //if (!ok)
    //    return STATUS_INTERNAL_ERROR;

    //if (!wait)
    //    return STATUS_SUCCESS;

    ////todo: refactor NVME_STATUS replying
    //NVME_COMMAND_STATUS nvme_status = { 0 };
    //NTSTATUS status = WaitAndPollCompletion(nvme_status, devext, devext->AdminQueue, &cmd);
    //return status;
}
NTSTATUS SetFeature_SyncHostTime(PSPCNVME_DEVEXT devext, bool wait)
{
    UNREFERENCED_PARAMETER(devext);
    UNREFERENCED_PARAMETER(wait);
    return STATUS_NOT_IMPLEMENTED;
    //KIRQL irql;
    //StorPortGetCurrentIrql((PVOID)devext, &irql);
    //ASSERT(irql == PASSIVE_LEVEL);

    ////KeQuerySystemTime() get system tick(100 ns) count since 1601/1/1 00:00:00
    //LARGE_INTEGER systime = { 0 };
    //KeQuerySystemTime(&systime);

    //LARGE_INTEGER elapsed = { 0 };
    //RtlTimeToSecondsSince1970(&systime, &elapsed.LowPart);
    //elapsed.QuadPart = elapsed.LowPart * 1000;

    //NVME_COMMAND cmd = { 0 };
    //cmd.CDW0.OPC = NVME_ADMIN_COMMAND_SET_FEATURES;
    //cmd.NSID = NVME_CONST::UNSPECIFIC_NSID;
    //cmd.u.SETFEATURES.CDW10.FID = NVME_FEATURE_TIMESTAMP;

    //PVOID timestamp = NULL;
    //ULONG size = PAGE_SIZE;
    //ULONG status = StorPortAllocatePool(devext, size, TAG_GENBUF, &timestamp);
    //if (STOR_STATUS_SUCCESS != status)
    //    return FALSE;
    //RtlZeroMemory(timestamp, size);
    //RtlCopyMemory(timestamp, &elapsed, sizeof(LARGE_INTEGER));

    //cmd.PRP1 = StorPortGetPhysicalAddress(devext, NULL, timestamp, &size).QuadPart;

    ////implement wait
    //return STATUS_INTERNAL_ERROR;
    ////submit command
    //bool ok = devext->AdminQueue->SubmitCmd(&cmd, NULL, CMD_CTX_TYPE::WAIT_EVENT);
    //if (!ok)
    //    return STATUS_INTERNAL_ERROR;

    //if (!wait)
    //    return STATUS_SUCCESS;

    ////todo: refactor NVME_STATUS replying
    //NVME_COMMAND_STATUS nvme_status = { 0 };
    //NTSTATUS status = WaitAndPollCompletion(nvme_status, devext, devext->AdminQueue, &cmd);
    //
    ////todo: maybe crash when STATUS_TIMEOUT?
    //StorPortFreePool(devext, timestamp);

    //return status;
}
NTSTATUS SetFeature_PowerManagement(PSPCNVME_DEVEXT devext, bool wait)
{
    UNREFERENCED_PARAMETER(wait);
    KIRQL irql;
    StorPortGetCurrentIrql((PVOID)devext, &irql);
    ASSERT(irql == PASSIVE_LEVEL);
    return STATUS_NOT_IMPLEMENTED;
}
NTSTATUS SetFeature_AsyncEvent(PSPCNVME_DEVEXT devext, bool wait)
{
    UNREFERENCED_PARAMETER(wait);
    KIRQL irql;
    StorPortGetCurrentIrql((PVOID)devext, &irql);
    ASSERT(irql == PASSIVE_LEVEL);
    return STATUS_NOT_IMPLEMENTED;
}
NTSTATUS NvmeIdentifyNamespace(PSPCNVME_DEVEXT devext, bool wait)
{
    UNREFERENCED_PARAMETER(devext);
    UNREFERENCED_PARAMETER(wait);
    return STATUS_NOT_IMPLEMENTED;
    //NVME_COMMAND cmd = { 0 };
    //PVOID buffer = NULL;
    //ULONG size = PAGE_SIZE;
    //KIRQL irql;
    //StorPortGetCurrentIrql((PVOID)devext, &irql);
    //ASSERT(irql == PASSIVE_LEVEL);

    //cmd.CDW0.OPC = NVME_ADMIN_COMMAND_IDENTIFY;
    ////each namespace map to one LUN. each namespace replied data should save to one LunExt.
    ////Now It only support 1 namespace on NVMe so I hardcode the LunExt array here.
    //cmd.NSID = 1;
    //cmd.u.IDENTIFY.CDW10.CNS = NVME_IDENTIFY_CNS_SPECIFIC_NAMESPACE;

    //StorPortAllocatePool(devext, size, TAG_GENBUF, &buffer);
    //RtlZeroMemory(buffer, size);

    //cmd.PRP1 = StorPortGetPhysicalAddress(devext, NULL, buffer, &size).QuadPart;

    ////submit command
    //bool ok = devext->AdminQueue->SubmitCmd(&cmd, NULL, true);
    //if (!ok)
    //    return STATUS_INTERNAL_ERROR;

    //if (!wait)
    //    return STATUS_SUCCESS;

    ////todo: refactor NVME_STATUS replying
    //NVME_COMMAND_STATUS nvme_status = { 0 };
    //NTSTATUS status = WaitAndPollCompletion(nvme_status, devext, devext->AdminQueue, &cmd);

    //if (NT_SUCCESS(status) && nvme_status.SC == 0 && nvme_status.SCT == 0)
    //    RtlCopyMemory(&devext->NsData[0], buffer, sizeof(NVME_IDENTIFY_NAMESPACE_DATA));

    ////todo: maybe crash when STATUS_TIMEOUT?
    //StorPortFreePool(devext, buffer);

    //return status;
}
#endif