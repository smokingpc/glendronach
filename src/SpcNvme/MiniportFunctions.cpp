#include "pch.h"

_Use_decl_annotations_ BOOLEAN HwInitialize(PVOID DeviceExtension)
{
    CDebugCallInOut inout(__FUNCTION__);
    StorPortEnablePassiveInitialization(DeviceExtension, HwPassiveInitialize);
    return FALSE;
}

_Use_decl_annotations_
BOOLEAN HwPassiveInitialize(PVOID DeviceExtension)
{
    CDebugCallInOut inout(__FUNCTION__);
    UNREFERENCED_PARAMETER(DeviceExtension);
    return FALSE;
}

_Use_decl_annotations_
BOOLEAN HwBuildIo(_In_ PVOID DevExt,_In_ PSCSI_REQUEST_BLOCK Srb)
{
    PSPCNVME_SRBEXT srbext = InitAndGetSrbExt(DevExt, (PSTORAGE_REQUEST_BLOCK)Srb);
    BOOLEAN need_startio = FALSE;
    UCHAR srb_status = 0;

    switch (srbext->FuncCode)
    {
        //case SRB_FUNCTION_ABORT_COMMAND:
    //case SRB_FUNCTION_RESET_LOGICAL_UNIT:
    //case SRB_FUNCTION_RESET_DEVICE:
    //case SRB_FUNCTION_RESET_BUS:
    //case SRB_FUNCTION_WMI:
    //case SRB_FUNCTION_POWER:
    //    srb_status = DefaultCmdHandler(srb, srbext);
    //    break;
    case SRB_FUNCTION_IO_CONTROL:
    case SRB_FUNCTION_PNP:
        //pnp handlers
    case SRB_FUNCTION_EXECUTE_SCSI:
        //scsi handlers
    default:
        need_startio = BuildIo_DefaultHandler(srbext);
        break;

    }

    if(FALSE != need_startio)
    {
        StorPortNotification(RequestComplete, DevExt, Srb);
    }

    return need_startio;
}

_Use_decl_annotations_
BOOLEAN HwStartIo(PVOID DeviceExtension, PSCSI_REQUEST_BLOCK Srb)
{
    UNREFERENCED_PARAMETER(DeviceExtension);
    UNREFERENCED_PARAMETER(Srb);
    return FALSE;
}

_Use_decl_annotations_ ULONG HwFindAdapter(
    _In_ PVOID DeviceExtension,
    _In_ PVOID HwContext,
    _In_ PVOID BusInformation,
    _In_z_ PCHAR ArgumentString,
    _Inout_ PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    _In_ PBOOLEAN Reserved3)
{
    CDebugCallInOut inout(__FUNCTION__);
    UNREFERENCED_PARAMETER(DeviceExtension);
    UNREFERENCED_PARAMETER(HwContext);
    UNREFERENCED_PARAMETER(BusInformation);
    UNREFERENCED_PARAMETER(ArgumentString);
    UNREFERENCED_PARAMETER(Reserved3);

    PSPCNVME_DEVEXT devext = (PSPCNVME_DEVEXT)DeviceExtension;


    //ConfigInfo->MaximumTransferLength = MAX_TX_SIZE;
    //ConfigInfo->NumberOfPhysicalBreaks = MAX_TX_PAGES;
    //ConfigInfo->AlignmentMask = FILE_LONG_ALIGNMENT;
    //ConfigInfo->MiniportDumpData = NULL;
    //ConfigInfo->InitiatorBusId[0] = 1;
    //ConfigInfo->CachesData = FALSE;
    //ConfigInfo->MapBuffers = STOR_MAP_ALL_BUFFERS_INCLUDING_READ_WRITE; //specify bounce buffer type?
    //ConfigInfo->MaximumNumberOfTargets = 1;
    //ConfigInfo->SrbType = SRB_TYPE_STORAGE_REQUEST_BLOCK;
    //ConfigInfo->DeviceExtensionSize = sizeof(SMOKY_EXT);
    //ConfigInfo->SrbExtensionSize = sizeof(SMOKY_SRBEXT);
    //ConfigInfo->MaximumNumberOfLogicalUnits = 1;
    //ConfigInfo->SynchronizationModel = StorSynchronizeFullDuplex;
    //ConfigInfo->HwMSInterruptRoutine = NULL;
    //ConfigInfo->InterruptSynchronizationMode = InterruptSupportNone;
    //ConfigInfo->VirtualDevice = TRUE;
    //ConfigInfo->MaxNumberOfIO = MAX_CONCURRENT_IO;
    //ConfigInfo->NumberOfBuses = 1;
    //ConfigInfo->ScatterGather = TRUE;
    //ConfigInfo->Master = TRUE;
    //ConfigInfo->AddressType = STORAGE_ADDRESS_TYPE_BTL8;
    //ConfigInfo->Dma64BitAddresses = SCSI_DMA64_MINIPORT_SUPPORTED;

    //{
    //    ConfigInfo->DumpRegion.VirtualBase = NULL;
    //    ConfigInfo->DumpRegion.PhysicalBase.QuadPart = NULL;
    //    ConfigInfo->DumpRegion.Length = 0;
    //}
    // If the buffer is not mapped, DataBuffer is the same as MDL's original virtual address, 
    // which could even be zero.
    return SP_RETURN_NOT_FOUND;
}

_Use_decl_annotations_
BOOLEAN HwResetBus(
    PVOID DeviceExtension,
    ULONG PathId
)
{
    CDebugCallInOut inout(__FUNCTION__);
    UNREFERENCED_PARAMETER(DeviceExtension);
    UNREFERENCED_PARAMETER(PathId);

    //miniport driver is responsible for completing SRBs received by HwStorStartIo for 
    //PathId during this routine and setting their status to SRB_STATUS_BUS_RESET if necessary.

    return TRUE;
}

_Use_decl_annotations_
SCSI_ADAPTER_CONTROL_STATUS HwAdapterControl(
    PVOID DeviceExtension,
    SCSI_ADAPTER_CONTROL_TYPE ControlType,
    PVOID Parameters
)
{
    CDebugCallInOut inout(__FUNCTION__);
    UNREFERENCED_PARAMETER(DeviceExtension);
    UNREFERENCED_PARAMETER(ControlType);
    UNREFERENCED_PARAMETER(Parameters);
    SCSI_ADAPTER_CONTROL_STATUS status = ScsiAdapterControlUnsuccessful;

    switch (ControlType)
    {
    case ScsiQuerySupportedControlTypes:
    {
        //PSCSI_SUPPORTED_CONTROL_TYPE_LIST ctl_list = (PSCSI_SUPPORTED_CONTROL_TYPE_LIST)Parameters;
        //status = HandleQueryControlTypeList(ctl_list);
        break;
    }
    case ScsiStopAdapter:
    {
        //shutdown HBA, this is post event of DeviceRemove 
        //status = HandleScsiStopAdapter(DeviceExtension);
        break;
    }
    case ScsiRestartAdapter:
    {
        //status = HandleScsiRestartAdapter();
        break;
    }
    case ScsiSetRunningConfig:
    {
        //status = HandleScsiSetRunningConfig();
        break;
    }
    default:
        status = ScsiAdapterControlUnsuccessful;
    }
    return status;
}

_Use_decl_annotations_
void HwProcessServiceRequest(
    PVOID DeviceExtension,
    PVOID Irp
)
{
    CDebugCallInOut inout(__FUNCTION__);
    UNREFERENCED_PARAMETER(DeviceExtension);
    UNREFERENCED_PARAMETER(Irp);
    ////ioctl interface for miniport
    //PSMOKY_EXT devext = (PSMOKY_EXT)DeviceExtension;
    //PIRP irp = (PIRP)Irp;
    //irp->IoStatus.Information = 0;
    //irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
    //StorPortCompleteServiceIrp(devext, irp);
}

_Use_decl_annotations_
void HwCompleteServiceIrp(PVOID DeviceExtension)
{
    CDebugCallInOut inout(__FUNCTION__);
    UNREFERENCED_PARAMETER(DeviceExtension);
    //if any async request in HwProcessServiceRequest, 
    //we should complete them here and let them go back asap.
//    StorPortPause(DeviceExtension, PAUSE_ADAPTER_TIMEOUT);

    //for(int i=0;i<6;i++)
    //    StorPortStallExecution(STALL_TIMEOUT);
}

_Use_decl_annotations_
SCSI_UNIT_CONTROL_STATUS HwUnitControl(
    _In_ PVOID DeviceExtension,
    _In_ SCSI_UNIT_CONTROL_TYPE ControlType,
    _In_ PVOID Parameters
)
{
    UNREFERENCED_PARAMETER(DeviceExtension);
    UNREFERENCED_PARAMETER(ControlType);
    UNREFERENCED_PARAMETER(Parameters);

    //UnitControl should handle events of LU:
    //1.Power States
    //2.Device Start
    //3.Device Remove and Surprise Remove
    return ScsiUnitControlSuccess;
}

_Use_decl_annotations_
VOID HwTracingEnabled(
    _In_ PVOID HwDeviceExtension,
    _In_ BOOLEAN Enabled
)
{
    UNREFERENCED_PARAMETER(HwDeviceExtension);
    UNREFERENCED_PARAMETER(Enabled);

    //miniport should write its own ETW log via StorPortEtwEventXXX API (refer to storport.h)
    // So HwTracingEnabled and HwCleanupTracing are used to "turn on" and "turn off" its own ETW logging mechanism.
}

_Use_decl_annotations_
VOID HwCleanupTracing(
    _In_ PVOID  Arg1
)
{
    UNREFERENCED_PARAMETER(Arg1);
}
