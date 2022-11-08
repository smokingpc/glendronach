#pragma once


//if true==wait, this command is sync call and poll AdminQueue to get completion result.
NTSTATUS SetFeature_InterruptCoalescing(PSPCNVME_DEVEXT devext, bool wait = true);
NTSTATUS SetFeature_Arbitration(PSPCNVME_DEVEXT devext, bool wait = true);
NTSTATUS SetFeature_SyncHostTime(PSPCNVME_DEVEXT devext, bool wait = true);
NTSTATUS SetFeature_PowerManagement(PSPCNVME_DEVEXT devext, bool wait = true);
NTSTATUS SetFeature_AsyncEvent(PSPCNVME_DEVEXT devext, bool wait = true);

NTSTATUS NvmeRegisterIoQueues(PSPCNVME_DEVEXT devext, bool wait = true);
NTSTATUS NvmeUnregisterIoQueues(PSPCNVME_DEVEXT devext, bool wait = true);

NTSTATUS NvmeSetFeatures(PSPCNVME_DEVEXT devext, bool wait = true);
NTSTATUS NvmeGetFeatures(PSPCNVME_DEVEXT devext, bool wait = true);
NTSTATUS NvmeIdentifyController(PSPCNVME_DEVEXT devext, bool wait = true);
NTSTATUS NvmeIdentifyNamespace(PSPCNVME_DEVEXT devext, bool wait = true);