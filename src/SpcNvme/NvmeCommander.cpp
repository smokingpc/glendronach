#include "pch.h"

NTSTATUS WaitAndPollCompletion(OUT NVME_COMMAND_STATUS &nvme_status, PSPCNVME_DEVEXT devext, CNvmeQueuePair *qp, PNVME_COMMAND cmd)
{
    KIRQL irql;
    StorPortGetCurrentIrql((PVOID)devext, &irql);
    ASSERT(irql == PASSIVE_LEVEL);

    //wait loop to poll completion
    NVME_COMPLETION_ENTRY cqe = { 0 };        //cqe => completion queue entry
    int wait_loop = 5;
    PVOID ctx = NULL;
    CMD_CTX_TYPE ctx_type;

    bool ok = qp->CompleteCmd(&cqe, ctx, ctx_type);
    while (!ok && wait_loop > 0)
    {
        wait_loop--;
        StorPortStallExecution(NVME_CONST::SLEEP_TIME_US);
        PVOID ctx = NULL;
        CMD_CTX_TYPE ctx_type;
        ok = qp->CompleteCmd(&cqe, ctx, ctx_type);
    }

    if (!ok)
        return STATUS_TIMEOUT;

    //after submit cmd, NVME_COMMAND::CDW0::CID is updated in CNvmeQueuePair.
    //so use it to identify completion entry.
    if(cmd->CDW0.CID == cqe.DW3.CID)
    {
        nvme_status = cqe.DW3.Status;
        return STATUS_SUCCESS;
    }
    else
    {
        nvme_status.SC = NVME_STATUS_RESERVED;
        nvme_status.SCT = NVME_STATUS_TYPE_GENERIC_COMMAND;
        return STATUS_UNSUCCESSFUL;
    }
}

NTSTATUS SetFeature_InterruptCoalescing(PSPCNVME_DEVEXT devext, bool wait)
{
    KIRQL irql;
    StorPortGetCurrentIrql((PVOID)devext, &irql);
    ASSERT(irql == PASSIVE_LEVEL);

    NVME_COMMAND cmd = {0};
    cmd.CDW0.OPC = NVME_ADMIN_COMMAND_SET_FEATURES;
    cmd.NSID = NVME_CONST::DEFAULT_NSID;
    cmd.u.SETFEATURES.CDW10.FID = NVME_FEATURE_INTERRUPT_COALESCING;
    cmd.u.SETFEATURES.CDW11.InterruptCoalescing.THR = NVME_CONST::INTCOAL_THRESHOLD;
    cmd.u.SETFEATURES.CDW11.InterruptCoalescing.TIME = NVME_CONST::INTCOAL_TIME;

    //submit command
    bool ok = devext->AdminQueue->SubmitCmd(&cmd, NULL, true);
    if(!ok)
        return STATUS_INTERNAL_ERROR;

    if(!wait)
        return STATUS_SUCCESS;

//todo: refactor NVME_STATUS replying
    NVME_COMMAND_STATUS nvme_status = {0};
    NTSTATUS status = WaitAndPollCompletion(nvme_status, devext, devext->AdminQueue, &cmd);
    return status;
}

NTSTATUS SetFeature_Arbitration(PSPCNVME_DEVEXT devext, bool wait)
{
    KIRQL irql;
    StorPortGetCurrentIrql((PVOID)devext, &irql);
    ASSERT(irql == PASSIVE_LEVEL);

    NVME_COMMAND cmd = { 0 };
    cmd.CDW0.OPC = NVME_ADMIN_COMMAND_SET_FEATURES;
    cmd.NSID = NVME_CONST::DEFAULT_NSID;
    cmd.u.SETFEATURES.CDW10.FID = NVME_FEATURE_ARBITRATION;
    cmd.u.SETFEATURES.CDW11.Arbitration.AB = NVME_CONST::AB_BURST;
    cmd.u.SETFEATURES.CDW11.Arbitration.HPW = NVME_CONST::AB_HPW;
    cmd.u.SETFEATURES.CDW11.Arbitration.MPW = NVME_CONST::AB_MPW;
    cmd.u.SETFEATURES.CDW11.Arbitration.LPW = NVME_CONST::AB_LPW;

    //submit command
    bool ok = devext->AdminQueue->SubmitCmd(&cmd, NULL, true);
    if (!ok)
        return STATUS_INTERNAL_ERROR;

    if (!wait)
        return STATUS_SUCCESS;

    //todo: refactor NVME_STATUS replying
    NVME_COMMAND_STATUS nvme_status = { 0 };
    NTSTATUS status = WaitAndPollCompletion(nvme_status, devext, devext->AdminQueue, &cmd);
    return status;
}

NTSTATUS SetFeature_SyncHostTime(PSPCNVME_DEVEXT devext, bool wait)
{
    KIRQL irql;
    StorPortGetCurrentIrql((PVOID)devext, &irql);
    ASSERT(irql == PASSIVE_LEVEL);

    //KeQuerySystemTime() get system tick(100 ns) count since 1601/1/1 00:00:00
    LARGE_INTEGER systime = { 0 };
    KeQuerySystemTime(&systime);

    LARGE_INTEGER elapsed = { 0 };
    RtlTimeToSecondsSince1970(&systime, &elapsed.LowPart);
    elapsed.QuadPart = elapsed.LowPart * 1000;

    NVME_COMMAND cmd = { 0 };
    cmd.CDW0.OPC = NVME_ADMIN_COMMAND_SET_FEATURES;
    cmd.NSID = NVME_CONST::DEFAULT_NSID;
    cmd.u.SETFEATURES.CDW10.FID = NVME_FEATURE_TIMESTAMP;

    PVOID timestamp = NULL;
    ULONG size = PAGE_SIZE;
    ULONG status = StorPortAllocatePool(devext, size, TAG_GENERIC_BUFFER, &timestamp);
    if (STOR_STATUS_SUCCESS != status)
        return FALSE;
    RtlZeroMemory(timestamp, size);
    RtlCopyMemory(timestamp, &elapsed, sizeof(LARGE_INTEGER));

    cmd.PRP1 = StorPortGetPhysicalAddress(devext, NULL, timestamp, &size).QuadPart;

    //implement wait
    return STATUS_INTERNAL_ERROR;
    //submit command
    bool ok = devext->AdminQueue->SubmitCmd(&cmd, NULL, CMD_CTX_TYPE::WAIT_EVENT);
    if (!ok)
        return STATUS_INTERNAL_ERROR;

    if (!wait)
        return STATUS_SUCCESS;

    //todo: refactor NVME_STATUS replying
    NVME_COMMAND_STATUS nvme_status = { 0 };
    NTSTATUS status = WaitAndPollCompletion(nvme_status, devext, devext->AdminQueue, &cmd);
    
    //todo: maybe crash when STATUS_TIMEOUT?
    StorPortFreePool(devext, timestamp);

    return status;
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

NTSTATUS NvmeRegisterIoQueues(PSPCNVME_DEVEXT devext, bool wait)
{
    KIRQL irql;
    StorPortGetCurrentIrql((PVOID)devext, &irql);
    ASSERT(irql == PASSIVE_LEVEL);

    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NvmeUnregisterIoQueues(PSPCNVME_DEVEXT devext, bool wait)
{
    UNREFERENCED_PARAMETER(wait);
    KIRQL irql;
    StorPortGetCurrentIrql((PVOID)devext, &irql);
    ASSERT(irql == PASSIVE_LEVEL);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NvmeSetFeatures(PSPCNVME_DEVEXT devext, bool wait)
{
    UNREFERENCED_PARAMETER(wait);
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    KIRQL irql;
    StorPortGetCurrentIrql((PVOID)devext, &irql);
    ASSERT(irql == PASSIVE_LEVEL);

    //status = SetFeature_InterruptCoalescing(devext, wait);
    //status = SetFeature_Arbitration(devext, wait);
    if (devext->IdentData.ONCS.Timestamp)
        status = SetFeature_SyncHostTime(devext, wait);

    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NvmeGetFeatures(PSPCNVME_DEVEXT devext, bool wait)
{
    UNREFERENCED_PARAMETER(wait);
    KIRQL irql;
    StorPortGetCurrentIrql((PVOID)devext, &irql);
    ASSERT(irql == PASSIVE_LEVEL);
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NvmeIdentifyController(PSPCNVME_DEVEXT devext, bool wait)
{
    NVME_COMMAND cmd = { 0 };
    PVOID buffer = NULL;
    ULONG size = PAGE_SIZE;
    KIRQL irql;
    StorPortGetCurrentIrql((PVOID)devext, &irql);
    ASSERT(irql == PASSIVE_LEVEL);

    cmd.CDW0.OPC = NVME_ADMIN_COMMAND_IDENTIFY;
    cmd.NSID = 0;
    cmd.u.IDENTIFY.CDW10.CNS = NVME_IDENTIFY_CNS_CONTROLLER;
    //todo: how to identify multiple controller?
    cmd.u.IDENTIFY.CDW10.CNTID = 0;

    StorPortAllocatePool(devext, size, TAG_GENERIC_BUFFER, &buffer);
    RtlZeroMemory(buffer, size);

    cmd.PRP1 = StorPortGetPhysicalAddress(devext, NULL, buffer, &size).QuadPart;

    //submit command
    bool ok = devext->AdminQueue->SubmitCmd(&cmd, NULL, true);
    if (!ok)
        return STATUS_INTERNAL_ERROR;

    if (!wait)
        return STATUS_SUCCESS;

    //todo: refactor NVME_STATUS replying
    NVME_COMMAND_STATUS nvme_status = { 0 };
    NTSTATUS status = WaitAndPollCompletion(nvme_status, devext, devext->AdminQueue, &cmd);

    if(NT_SUCCESS(status) && nvme_status.SC==0 && nvme_status.SCT == 0)
        RtlCopyMemory(&devext->IdentData, buffer, sizeof(NVME_IDENTIFY_CONTROLLER_DATA));

    //todo: maybe crash when STATUS_TIMEOUT?
    StorPortFreePool(devext, buffer);

    return status;
}

NTSTATUS NvmeIdentifyNamespace(PSPCNVME_DEVEXT devext, bool wait)
{
    NVME_COMMAND cmd = { 0 };
    PVOID buffer = NULL;
    ULONG size = PAGE_SIZE;
    KIRQL irql;
    StorPortGetCurrentIrql((PVOID)devext, &irql);
    ASSERT(irql == PASSIVE_LEVEL);

    cmd.CDW0.OPC = NVME_ADMIN_COMMAND_IDENTIFY;
    //each namespace map to one LUN. each namespace replied data should save to one LunExt.
    //Now It only support 1 namespace on NVMe so I hardcode the LunExt array here.
    cmd.NSID = 1;
    cmd.u.IDENTIFY.CDW10.CNS = NVME_IDENTIFY_CNS_SPECIFIC_NAMESPACE;

    StorPortAllocatePool(devext, size, TAG_GENERIC_BUFFER, &buffer);
    RtlZeroMemory(buffer, size);

    cmd.PRP1 = StorPortGetPhysicalAddress(devext, NULL, buffer, &size).QuadPart;

    //submit command
    bool ok = devext->AdminQueue->SubmitCmd(&cmd, NULL, true);
    if (!ok)
        return STATUS_INTERNAL_ERROR;

    if (!wait)
        return STATUS_SUCCESS;

    //todo: refactor NVME_STATUS replying
    NVME_COMMAND_STATUS nvme_status = { 0 };
    NTSTATUS status = WaitAndPollCompletion(nvme_status, devext, devext->AdminQueue, &cmd);

    if (NT_SUCCESS(status) && nvme_status.SC == 0 && nvme_status.SCT == 0)
        RtlCopyMemory(&devext->NsData[0], buffer, sizeof(NVME_IDENTIFY_NAMESPACE_DATA));

    //todo: maybe crash when STATUS_TIMEOUT?
    StorPortFreePool(devext, buffer);

    return status;
}
