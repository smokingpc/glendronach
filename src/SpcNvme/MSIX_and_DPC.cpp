#include "pch.h"


BOOLEAN NvmeMsixISR(IN PVOID dev_ext, IN ULONG msgid)
{
    PSPCNVME_DEVEXT devext = (PSPCNVME_DEVEXT)dev_ext;
    ULONG irql = 0;
    ULONG status = STOR_STATUS_SUCCESS;
    BOOLEAN ok = FALSE;
    status = StorPortAcquireMSISpinLock(dev_ext, msgid, &irql);
    ok = StorPortIssueDpc(dev_ext, &devext->NvmeDPC, NULL, NULL);
    status = StorPortReleaseMSISpinLock(dev_ext, msgid, irql);

    return TRUE;
}

VOID NvmeDpcRoutine(
    _In_ PSTOR_DPC dpc,
    _In_ PVOID dev_ext,
    _In_opt_ PVOID sysarg1,
    _In_opt_ PVOID sysarg2
)
{
    UNREFERENCED_PARAMETER(dpc);
    UNREFERENCED_PARAMETER(dev_ext);
    UNREFERENCED_PARAMETER(sysarg1);
    UNREFERENCED_PARAMETER(sysarg2);
    //PSPCNVME_DEVEXT devext = (PSPCNVME_DEVEXT)dev_ext;

    //process completion of NVMe
}
