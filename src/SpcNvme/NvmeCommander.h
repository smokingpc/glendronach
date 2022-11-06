#pragma once
NTSTATUS SetInterruptCoalescing(PSPCNVME_DEVEXT devext);
NTSTATUS SetArbitration(PSPCNVME_DEVEXT devext);
NTSTATUS SyncHostTime(PSPCNVME_DEVEXT devext);
NTSTATUS SetPowerManagement(PSPCNVME_DEVEXT devext);
NTSTATUS SetAsyncEvent(PSPCNVME_DEVEXT devext);

NTSTATUS IdentifyController(PSPCNVME_DEVEXT devext);
NTSTATUS IdentifyNamespace(PSPCNVME_DEVEXT devext);
NTSTATUS RegisterIoQueues(PSPCNVME_DEVEXT devext);

