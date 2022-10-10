#include "pch.h"


BOOLEAN MsixISR(IN PVOID dev_ext, IN ULONG msgid)
{
    PSPCNVME_DEVEXT devext = (PSPCNVME_DEVEXT)dev_ext;
    ULONG irql = 0;
    ULONG status = STOR_STATUS_SUCCESS;
    status = StorPortAcquireMSISpinLock(dev_ext, msgid, &irql);
    StorPortIssueDpc(dev_ext, &devext->NvmeDPC, NULL, NULL);
    status = StorPortReleaseMSISpinLock(dev_ext, msgid, irql);
}

VOID
NvmeDpcRoutine(
    _In_ PSTOR_DPC dpc,
    _In_ PVOID dev_ext,
    _In_opt_ PVOID sysarg1,
    _In_opt_ PVOID sysarg2
)
{
    UNREFERENCED_PARAMETER(dpc);
    UNREFERENCED_PARAMETER(sysarg1);
    UNREFERENCED_PARAMETER(sysarg2);
    PSPCNVME_DEVEXT devext = (PSPCNVME_DEVEXT)dev_ext;
}
