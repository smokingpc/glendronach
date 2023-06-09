#include "pch.h"

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
    devext->RestartController();
    return ScsiAdapterControlSuccess;
}