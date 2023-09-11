#include "pch.h"
SCSI_UNIT_CONTROL_STATUS Handle_QuerySupportedUnitControl(
    PSCSI_SUPPORTED_CONTROL_TYPE_LIST list)
{
    ULONG max_support = list->MaxControlType;

    if (ScsiUnitStart <= max_support)
        list->SupportedTypeList[ScsiUnitStart] = TRUE;
    //ScsiUnitUsage,
    //ScsiUnitStart,
    //ScsiUnitPower,
    //ScsiUnitPoFxPowerInfo,
    //ScsiUnitPoFxPowerRequired,
    //ScsiUnitPoFxPowerActive,
    //ScsiUnitPoFxPowerSetFState,
    //ScsiUnitPoFxPowerControl,
    //ScsiUnitRemove,
    //ScsiUnitSurpriseRemoval,
    //ScsiUnitRichDescription,
    //ScsiUnitQueryBusType,
    //ScsiUnitQueryFruId,

    return ScsiUnitControlSuccess;
}


