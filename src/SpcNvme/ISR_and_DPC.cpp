#include "pch.h"


BOOLEAN NvmeMsixISR(IN PVOID devext, IN ULONG msgid)
{
    CNvmeDevice* nvme = (CNvmeDevice*)devext;
    BOOLEAN ok = FALSE;
    ok = StorPortIssueDpc(devext, &nvme->QueueCplDpc, (PVOID) msgid, NULL);
    if (!ok)
    {
        //Todo: add debug print or log if IssueDPC failed.
    }


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
