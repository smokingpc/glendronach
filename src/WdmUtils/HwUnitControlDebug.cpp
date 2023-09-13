#include "pch.h"
EXTERN_C_START
#include <storport.h>
#include <srbhelper.h>
EXTERN_C_END
#include <ntstrsafe.h>

void DebugUnitControlCode(SCSI_UNIT_CONTROL_TYPE code)
{
    char msg[MSG_BUFFER_SIZE] = { 0 };
    switch (code)
    {
    case ScsiQuerySupportedUnitControlTypes:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "QuerySupportedUnitControlTypes");
        break;
    case ScsiUnitUsage:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "UnitUsage");
        break;
    case ScsiUnitStart:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "UnitStart");
        break;
    case ScsiUnitPower:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "UnitPower");
        break;
    case ScsiUnitPoFxPowerInfo:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "UnitPoFxPowerInfo");
        break;
    case ScsiUnitPoFxPowerRequired:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "UnitPoFxPowerRequired");
        break;
    case ScsiUnitPoFxPowerActive:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "UnitPoFxPowerActive");
        break;
    case ScsiUnitPoFxPowerSetFState:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "UnitPoFxPowerSetFState");
        break;
    case ScsiUnitPoFxPowerControl:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "UnitPoFxPowerControl");
        break;
    case ScsiUnitRemove:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "UnitRemove");
        break;
    case ScsiUnitSurpriseRemoval:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "UnitSurpriseRemoval");
        break;
    case ScsiUnitRichDescription:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "UnitRichDescription");
        break;
    case ScsiUnitQueryBusType:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "UnitQueryBusType");
        break;
    case ScsiUnitQueryFruId:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "UnitQueryFruId");
        break;
    default:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "UNKNOWN");
        break;
    }

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DBG_FILTER, "%s Got UnitControl[%s]\n", DEBUG_PREFIX, msg);
}
