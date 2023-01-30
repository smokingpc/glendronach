#include "pch.h"


BOOLEAN NvmeMsixISR(IN PVOID devext, IN ULONG msgid)
{
    CNvmeDevice* nvme = (CNvmeDevice*)devext;
    ULONG irql = 0;
    ULONG status = STOR_STATUS_SUCCESS;
    BOOLEAN ok = FALSE;
    status = StorPortAcquireMSISpinLock(devext, msgid, &irql);
    //todo: log if acquire MSI lock failed.

    ok = StorPortIssueDpc(devext, &nvme->QueueCplDpc, (PVOID) msgid, NULL);

    status = StorPortReleaseMSISpinLock(devext, msgid, irql);
    //todo: log if release MSI lock failed.

    if (!ok)
    {
        //todo: log
    }

    //Todo: add debug print or log if IssueDPC failed.

    return TRUE;
}

VOID NvmeDpcRoutine(
    _In_ PSTOR_DPC dpc,
    _In_ PVOID devext,
    _In_opt_ PVOID sysarg1,
    _In_opt_ PVOID sysarg2
)
{
    UNREFERENCED_PARAMETER(dpc);
    UNREFERENCED_PARAMETER(sysarg2);

    CNvmeDevice* nvme = (CNvmeDevice*)devext;
    ULONG msgid = PtrToUlong(sysarg1);

    nvme->DoQueueCplByDPC(msgid);
}
