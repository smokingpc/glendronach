#include "pch.h"
SCSI_UNIT_CONTROL_STATUS Handle_QuerySupportedUnitControl(
    PSCSI_SUPPORTED_CONTROL_TYPE_LIST list)
{
//called at PASSIVE_LEVEL
    ULONG max_support = list->MaxControlType;

    if (ScsiUnitStart <= max_support)
        list->SupportedTypeList[ScsiUnitStart] = TRUE;
    //ScsiUnitUsage, <= query "is miniport support BOOT/DUMP/PAGE FILE/HIBERNATION" ?
    if (ScsiUnitStart <= max_support)
        list->SupportedTypeList[ScsiUnitStart] = TRUE;
    if (ScsiUnitPower <= max_support)
        list->SupportedTypeList[ScsiUnitPower] = TRUE;
    //ScsiUnitPoFxPowerInfo,
    //ScsiUnitPoFxPowerRequired,
    //ScsiUnitPoFxPowerActive,
    //ScsiUnitPoFxPowerSetFState,
    //ScsiUnitPoFxPowerControl,

    if (ScsiUnitRemove <= max_support)
        list->SupportedTypeList[ScsiUnitRemove] = TRUE;
    if (ScsiUnitSurpriseRemoval <= max_support)
        list->SupportedTypeList[ScsiUnitSurpriseRemoval] = TRUE;

    //ScsiUnitRichDescription,
    //ScsiUnitQueryBusType,
    //ScsiUnitQueryFruId,

    return ScsiUnitControlSuccess;
}

SCSI_UNIT_CONTROL_STATUS Handle_UnitStart(PSTOR_ADDR_BTL8 addr)
{
    UNREFERENCED_PARAMETER(addr);
    DbgBreakPoint();
    return ScsiUnitControlSuccess;
}
SCSI_UNIT_CONTROL_STATUS Handle_UnitPower(PSTOR_UNIT_CONTROL_POWER power)
{
    UNREFERENCED_PARAMETER(power);
    //if support this UnitControl event, storport will NOT
    //send SRB_FUNCTION_POWER.
    DbgBreakPoint();
    return ScsiUnitControlSuccess;
}

SCSI_UNIT_CONTROL_STATUS Handle_UnitRemove(PSTOR_ADDR_BTL8 addr)
{
    UNREFERENCED_PARAMETER(addr);
    DbgBreakPoint();
    return ScsiUnitControlSuccess;
}

SCSI_UNIT_CONTROL_STATUS Handle_UnitSurpriseRemove(PSTOR_ADDR_BTL8 addr)
{
    UNREFERENCED_PARAMETER(addr);
    DbgBreakPoint();
    return ScsiUnitControlSuccess;
}
