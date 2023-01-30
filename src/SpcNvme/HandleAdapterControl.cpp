#include "pch.h"

static void Worker_RestartAdapter(
    _In_ PVOID DevExt,
    _In_ PVOID Context,
    _In_ PVOID Worker)
{
    UNREFERENCED_PARAMETER(Context);
    CNvmeDevice* nvme = (CNvmeDevice*)DevExt;

    //TODO: error handling and log
    nvme->InitController();
    nvme->RegisterIoQ();

    StorPortFreeWorker(DevExt, Worker);
}

SCSI_ADAPTER_CONTROL_STATUS Handle_QuerySupportedControlTypes(
    PSCSI_SUPPORTED_CONTROL_TYPE_LIST list)
{
    ULONG max_support = list->MaxControlType;

    if (ScsiStopAdapter <= max_support)
        list->SupportedTypeList[ScsiStopAdapter] = TRUE;
    if (ScsiRestartAdapter <= max_support)
        list->SupportedTypeList[ScsiRestartAdapter] = TRUE;
    if (ScsiAdapterSurpriseRemoval <= max_support)
        list->SupportedTypeList[ScsiAdapterSurpriseRemoval] = TRUE;
    if (ScsiAdapterPower <= max_support)
        list->SupportedTypeList[ScsiAdapterPower] = TRUE;
    if (ScsiPowerSettingNotification <= max_support)
        list->SupportedTypeList[ScsiPowerSettingNotification] = TRUE;
    return ScsiAdapterControlSuccess;
}

SCSI_ADAPTER_CONTROL_STATUS Handle_RestartAdapter(CNvmeDevice* devext)
{
    //Device "entering" D0 state from D1 D2 D3 state
    //**ATTENTION: this function run at DIRQL!
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    PVOID worker = NULL;
    status = devext->EnableController();
    if(!NT_SUCCESS(status))
        return ScsiAdapterControlUnsuccessful;

    //they are limited IRQL<=DISPATCH_LEVEL, try another way...
    //StorPortInitializeWorker(devext, &worker);
    //StorPortQueueWorkItem(devext, Worker_RestartAdapter, worker, NULL);
}