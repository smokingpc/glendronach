#include "pch.h"

NTSTATUS SetInterruptCoalescing(PSPCNVME_DEVEXT devext)
{
    return STATUS_NOT_IMPLEMENTED;
}
NTSTATUS SetArbitration(PSPCNVME_DEVEXT devext)
{
    return STATUS_NOT_IMPLEMENTED;
}
NTSTATUS SyncHostTime(PSPCNVME_DEVEXT devext)
{
    return STATUS_NOT_IMPLEMENTED;
}
NTSTATUS SetPowerManagement(PSPCNVME_DEVEXT devext) 
{
    return STATUS_NOT_IMPLEMENTED;
}
NTSTATUS SetAsyncEvent(PSPCNVME_DEVEXT devext)
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS IdentifyController(PSPCNVME_DEVEXT devext)
{
    //return STATUS_NOT_IMPLEMENTED;
    //THIS function is called in FindAdapter, which the ISR not registered yet.
    //so I wait and poll the Admin CplQ.

}
NTSTATUS IdentifyNamespace(PSPCNVME_DEVEXT devext)
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS RegisterIoQueues(PSPCNVME_DEVEXT devext)
{
    return STATUS_NOT_IMPLEMENTED;
}
