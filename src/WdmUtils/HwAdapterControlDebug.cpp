#include "pch.h"
EXTERN_C_START
#include <storport.h>
#include <srbhelper.h>
EXTERN_C_END
#include <ntstrsafe.h>

void DebugAdapterControlCode(SCSI_ADAPTER_CONTROL_TYPE code)
{
    char msg[MSG_BUFFER_SIZE] = { 0 };

    switch (code)
    {
    case ScsiQuerySupportedControlTypes:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "QuerySupportedControlTypes");
        break;
    case ScsiStopAdapter:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "StopAdapter");
        break;
    case ScsiRestartAdapter:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "RestartAdapter");
        break;
    case ScsiSetBootConfig:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SetBootConfig");
        break;
    case ScsiSetRunningConfig:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SetRunningConfig");
        break;
    case ScsiPowerSettingNotification:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "PowerSettingNotification");
        break;
    case ScsiAdapterPower:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "AdapterPower");
        break;
    case ScsiAdapterPoFxPowerRequired:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "AdapterPoFxPowerRequired");
        break;
    case ScsiAdapterPoFxPowerActive:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "AdapterPoFxPowerActive");
        break;
    case ScsiAdapterPoFxPowerSetFState:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "AdapterPoFxPowerSetFState");
        break;
    case ScsiAdapterPoFxPowerControl:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "AdapterPoFxPowerControl");
        break;
    case ScsiAdapterPrepareForBusReScan:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "AdapterPrepareForBusReScan");
        break;
    case ScsiAdapterSystemPowerHints:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "AdapterSystemPowerHints");
        break;
    case ScsiAdapterFilterResourceRequirements:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "AdapterFilterResourceRequirements");
        break;
    case ScsiAdapterPoFxMaxOperationalPower:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "AdapterPoFxMaxOperationalPower");
        break;
    case ScsiAdapterPoFxSetPerfState:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "AdapterPoFxSetPerfState");
        break;
    case ScsiAdapterSurpriseRemoval:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "AdapterSurpriseRemoval");
        break;
    case ScsiAdapterSerialNumber:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "AdapterSerialNumber");
        break;
    case ScsiAdapterCryptoOperation:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "AdapterCryptoOperation");
        break;
    case ScsiAdapterQueryFruId:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "AdapterQueryFruId");
        break;
    case ScsiAdapterSetEventLogging:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "AdapterSetEventLogging");
        break;
    default:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "UNKNOWN");
        break;
    }

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DBG_FILTER, "%s Got AdapterControl[%s]\n", DEBUG_PREFIX, msg);
}
