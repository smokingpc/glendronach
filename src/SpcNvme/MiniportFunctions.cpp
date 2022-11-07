#include "pch.h"

static void CreateAdminQueue(PSPCNVME_DEVEXT devext)
{
    QUEUE_PAIR_CONFIG qcfg = {
        .DevExt = devext,
        .QID = 0,
        .Depth = NVME_CONST::ADMIN_QUEUE_DEPTH,
        .Type = QUEUE_TYPE::ADM_QUEUE,
    };
    devext->NvmeDev->GetAdmDoorbell(&qcfg.SubDbl, &qcfg.CplDbl);
    devext->AdminQueue = new CNvmeQueuePair(&qcfg);
}
static void DeleteAdminQueue(PSPCNVME_DEVEXT devext)
{
    if (nullptr != devext->AdminQueue)
    {
        //devext->AdminQueue->Teardown();
        delete devext->AdminQueue;
        devext->AdminQueue = nullptr;
    }
}


static bool SetupDeviceExtension(PSPCNVME_DEVEXT devext, PSPCNVME_CONFIG cfg, PPORT_CONFIGURATION_INFORMATION portcfg)
{
//workaround
    RtlCopyMemory(devext->PciSpace, (*portcfg->AccessRanges), sizeof(ACCESS_RANGE)*portcfg->NumberOfAccessRanges);

    devext->NvmeDev = CNvmeDevice::Create(devext, cfg);
    if(NULL == devext->NvmeDev)
        return false;

    CreateAdminQueue(devext);
    //prepare DPC for MSIX interrupt
    StorPortInitializeDpc(devext, &devext->NvmeDPC, NvmeDpcRoutine);
    
    return true;
}

static void TeardownDeviceExtension(PSPCNVME_DEVEXT devext)
{
    //stop msix
    //teardown io queues
    DeleteAdminQueue(devext);
}

static void FillPortConfiguration(PPORT_CONFIGURATION_INFORMATION portcfg, PSPCNVME_CONFIG nvmecfg)
{
    portcfg->MaximumTransferLength = nvmecfg->MaxTxSize;//MAX_TX_SIZE;
    portcfg->NumberOfPhysicalBreaks = nvmecfg->MaxTxPages;//MAX_TX_PAGES;
    portcfg->AlignmentMask = FILE_LONG_ALIGNMENT;    //PRP 1 need align DWORD in some case. So set this align is better.
    portcfg->MiniportDumpData = NULL;
    portcfg->InitiatorBusId[0] = 1;
    portcfg->CachesData = FALSE;
    portcfg->MapBuffers = STOR_MAP_NON_READ_WRITE_BUFFERS; //specify bounce buffer type?
    portcfg->MaximumNumberOfTargets = nvmecfg->MaxTargets;
    portcfg->SrbType = SRB_TYPE_STORAGE_REQUEST_BLOCK;
    portcfg->DeviceExtensionSize = sizeof(SPCNVME_DEVEXT);
    portcfg->SrbExtensionSize = sizeof(SPCNVME_SRBEXT);
    portcfg->MaximumNumberOfLogicalUnits = nvmecfg->MaxLu;
    portcfg->SynchronizationModel = StorSynchronizeFullDuplex;
    portcfg->HwMSInterruptRoutine = NvmeMsixISR;
    portcfg->InterruptSynchronizationMode = InterruptSynchronizePerMessage;
    portcfg->NumberOfBuses = 1;
    portcfg->ScatterGather = TRUE;
    portcfg->Master = TRUE;
    portcfg->AddressType = STORAGE_ADDRESS_TYPE_BTL8;
    portcfg->Dma64BitAddresses = SCSI_DMA64_MINIPORT_SUPPORTED;
    portcfg->MaxNumberOfIO = nvmecfg->MaxTotalIo;
    portcfg->MaxIOsPerLun = nvmecfg->MaxIoPerLU;

    //Dump is not supported now. Will be supported in future.
    portcfg->RequestedDumpBufferSize = 0;
    portcfg->DumpMode = DUMP_MODE_CRASH;
    portcfg->DumpRegion.VirtualBase = NULL;
    portcfg->DumpRegion.PhysicalBase.QuadPart = NULL;
    portcfg->DumpRegion.Length = 0;
}


_Use_decl_annotations_ ULONG HwFindAdapter(
    _In_ PVOID dev_ext,
    _In_ PVOID ctx,
    _In_ PVOID businfo,
    _In_z_ PCHAR arg_str,
    _Inout_ PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    _In_ PBOOLEAN Reserved3)
{
    //Running at PASSIVE_LEVEL!!!!!!!!!

    CDebugCallInOut inout(__FUNCTION__);
    UNREFERENCED_PARAMETER(ctx);
    UNREFERENCED_PARAMETER(businfo);
    UNREFERENCED_PARAMETER(arg_str);
    UNREFERENCED_PARAMETER(Reserved3);

    SPCNVME_CONFIG nvmecfg;
    nvmecfg.BusNumber = ConfigInfo->SystemIoBusNumber;
    nvmecfg.SlotNumber = ConfigInfo->SlotNumber;

    //PCI bus related initialize
    FillPortConfiguration(ConfigInfo, &nvmecfg);

    PSPCNVME_DEVEXT devext = (PSPCNVME_DEVEXT)dev_ext;
    CNvmeDevice* nvme = devext->NvmeDev;
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    nvmecfg.SetAccessRanges(*(ConfigInfo->AccessRanges), ConfigInfo->NumberOfAccessRanges);
    if (false == SetupDeviceExtension(devext, &nvmecfg, ConfigInfo))
        goto error;

    if (!nvme->DisableController())
        goto error;

    if(!nvme->RegisterAdminQueuePair(devext->AdminQueue))
        goto error;

    if (!nvme->EnableController())
        goto error;

    //TODO: query controller identify, query namespace identify,
    //      set features, 
    //because this function is not returned, MSIX won't fire.
    //so we do some admin commands here...
    status = NvmeIdentifyController(devext);
    if (!NT_SUCCESS(status))
        goto error;

    status = NvmeSetFeatures(devext);
    if(!NT_SUCCESS(status))
        goto error;

    return SP_RETURN_FOUND;

error:
    TeardownDeviceExtension(devext);
    return SP_RETURN_NOT_FOUND;
}

_Use_decl_annotations_ BOOLEAN HwInitialize(PVOID DeviceExtension)
{
//Running at DIRQL
    CDebugCallInOut inout(__FUNCTION__);
    //initialize perf options
    StorPortEnablePassiveInitialization(DeviceExtension, HwPassiveInitialize);
    return FALSE;
}

_Use_decl_annotations_
BOOLEAN HwPassiveInitialize(PVOID DeviceExtension)
{
    //Running at PASSIVE_LEVEL
    CDebugCallInOut inout(__FUNCTION__);
    UNREFERENCED_PARAMETER(DeviceExtension);
    //INITIALIZE driver contexts
    //TODO: register IoQueues

    return FALSE;
}

_Use_decl_annotations_
BOOLEAN HwBuildIo(_In_ PVOID DevExt,_In_ PSCSI_REQUEST_BLOCK Srb)
{
    PSPCNVME_SRBEXT srbext = InitAndGetSrbExt(DevExt, (PSTORAGE_REQUEST_BLOCK)Srb);
    BOOLEAN need_startio = FALSE;
    //UCHAR srb_status = 0;

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
    case SRB_FUNCTION_EXECUTE_SCSI:
        need_startio = BuildIo_ScsiHandler(srbext);
        break;
    case SRB_FUNCTION_IO_CONTROL:
    case SRB_FUNCTION_PNP:
        //pnp handlers
        //scsi handlers
    default:
        need_startio = BuildIo_DefaultHandler(srbext);
        break;

    }
    return need_startio;
}

_Use_decl_annotations_
BOOLEAN HwStartIo(PVOID DevExt, PSCSI_REQUEST_BLOCK Srb)
{
    PSPCNVME_SRBEXT srbext = InitAndGetSrbExt(DevExt, (PSTORAGE_REQUEST_BLOCK)Srb);
    BOOLEAN ok = FALSE;

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
    case SRB_FUNCTION_EXECUTE_SCSI:
        ok = StartIo_ScsiHandler(srbext);
        break;
    case SRB_FUNCTION_IO_CONTROL:
    case SRB_FUNCTION_PNP:
        //pnp handlers
        //scsi handlers
    default:
        ok = StartIo_DefaultHandler(srbext);
        break;

    }
    //each handler should complete the requests.
    return ok;
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
