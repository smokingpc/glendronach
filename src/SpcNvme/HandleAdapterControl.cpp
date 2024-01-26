#include "pch.h"

SCSI_ADAPTER_CONTROL_STATUS Handle_QuerySupportedControlTypes(
    PSCSI_SUPPORTED_CONTROL_TYPE_LIST list)
{
    ULONG max_support = list->MaxControlType;

    if (ScsiStopAdapter <= max_support)
        list->SupportedTypeList[ScsiStopAdapter] = TRUE;
    if (ScsiAdapterSurpriseRemoval <= max_support)
        list->SupportedTypeList[ScsiAdapterSurpriseRemoval] = TRUE;

#if 0
    if (ScsiRestartAdapter <= max_support)
        list->SupportedTypeList[ScsiRestartAdapter] = TRUE;
    if (ScsiAdapterPower <= max_support)
        list->SupportedTypeList[ScsiAdapterPower] = TRUE;
    if (ScsiPowerSettingNotification <= max_support)
        list->SupportedTypeList[ScsiPowerSettingNotification] = TRUE;
    //following are not implemented AdapterControl. 
    //Just keep the callback for tracing.
    if (ScsiSetBootConfig <= max_support)
        list->SupportedTypeList[ScsiSetBootConfig] = TRUE;
    if (ScsiSetRunningConfig <= max_support)
        list->SupportedTypeList[ScsiSetRunningConfig] = TRUE;
    if (ScsiAdapterPoFxPowerRequired <= max_support)
        list->SupportedTypeList[ScsiAdapterPoFxPowerRequired] = TRUE;
    if (ScsiAdapterPoFxPowerActive <= max_support)
        list->SupportedTypeList[ScsiAdapterPoFxPowerActive] = TRUE;
    if (ScsiAdapterPoFxPowerSetFState <= max_support)
        list->SupportedTypeList[ScsiAdapterPoFxPowerSetFState] = TRUE;
    if (ScsiAdapterPoFxPowerControl <= max_support)
        list->SupportedTypeList[ScsiAdapterPoFxPowerControl] = TRUE;
    if (ScsiAdapterPrepareForBusReScan <= max_support)
        list->SupportedTypeList[ScsiAdapterPrepareForBusReScan] = TRUE;
    if (ScsiAdapterSystemPowerHints <= max_support)
        list->SupportedTypeList[ScsiAdapterSystemPowerHints] = TRUE;
    if (ScsiAdapterFilterResourceRequirements <= max_support)
        list->SupportedTypeList[ScsiAdapterFilterResourceRequirements] = TRUE;
    if (ScsiAdapterPoFxMaxOperationalPower <= max_support)
        list->SupportedTypeList[ScsiAdapterPoFxMaxOperationalPower] = TRUE;
    if (ScsiAdapterPoFxSetPerfState <= max_support)
        list->SupportedTypeList[ScsiAdapterPoFxSetPerfState] = TRUE;
    if (ScsiAdapterSerialNumber <= max_support)
        list->SupportedTypeList[ScsiAdapterSerialNumber] = TRUE;
    if (ScsiAdapterCryptoOperation <= max_support)
        list->SupportedTypeList[ScsiAdapterCryptoOperation] = TRUE;
    if (ScsiAdapterQueryFruId <= max_support)
        list->SupportedTypeList[ScsiAdapterQueryFruId] = TRUE;
    if (ScsiAdapterSetEventLogging <= max_support)
        list->SupportedTypeList[ScsiAdapterSetEventLogging] = TRUE;
#endif
    return ScsiAdapterControlSuccess;
}

SCSI_ADAPTER_CONTROL_STATUS Handle_StopAdapter(CNvmeDevice* devext)
{
    devext->DisableController();
    return ScsiAdapterControlSuccess;
}

SCSI_ADAPTER_CONTROL_STATUS Handle_RestartAdapter(CNvmeDevice* devext)
{
    devext->RestartController();
    return ScsiAdapterControlSuccess;
}
