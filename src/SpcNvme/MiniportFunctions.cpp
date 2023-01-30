#include "pch.h"

#if 0
static void CreateAdminQueue(PSPCNVME_DEVEXT devext)
{
    QUEUE_PAIR_CONFIG qcfg = {
        .DevExt = devext,
        .QID = 0,
        .Depth = NVME_CONST::ADMIN_QUEUE_DEPTH,
        .Type = QUEUE_TYPE::ADM_QUEUE,
    };
    devext->NvmeDev->GetAdmDoorbell(&qcfg.SubDbl, &qcfg.CplDbl);
    devext->AdminQueue = new CNvmeQueue(&qcfg);
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
static void CreateIoQueues(PSPCNVME_DEVEXT devext)
{
    devext->IoQueueCount = NVME_CONST::IO_QUEUE_COUNT;
    for(ULONG i=0; i<devext->IoQueueCount; i++)
    {
        QUEUE_PAIR_CONFIG qcfg = {
            .DevExt = devext,
            .QID = (USHORT) (i+1), //nvme qid is 0-based, but AdminQ is always queue[0]. so I/O queue qid can be treat as 1-based.
            .Depth = NVME_CONST::IO_QUEUE_DEPTH,
            .Type = QUEUE_TYPE::IO_QUEUE,
        };
        devext->IoQueue[i] = new CNvmeQueue(&qcfg);
    }
}
static void DeleteIoQueues(PSPCNVME_DEVEXT devext)
{
    for (int i = devext->IoQueueCount; i > 0; i--)
    {
        QUEUE_PAIR_CONFIG qcfg = {
            .DevExt = devext,
            .QID = (USHORT)i, //nvme qid is 0-based, but AdminQ is always queue[0]. so I/O queue qid can be treat as 1-based.
            .Depth = NVME_CONST::IO_QUEUE_DEPTH,
            .Type = QUEUE_TYPE::IO_QUEUE,
        };
        delete devext->IoQueue[i-1];
        devext->IoQueue[i - 1] = nullptr;
    }
    devext->IoQueueCount = 0;
}
static bool SetupDeviceExtension(PSPCNVME_DEVEXT devext, PSPCNVME_CONFIG cfg, PPORT_CONFIGURATION_INFORMATION portcfg)
{
//workaround
    RtlCopyMemory(devext->PciSpace, (*portcfg->AccessRanges), sizeof(ACCESS_RANGE)*portcfg->NumberOfAccessRanges);

    devext->NvmeDev = CNvmeDevice::Create(devext, cfg);
    if(NULL == devext->NvmeDev)
        return false;

    devext->CpuCount = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);

    CreateAdminQueue(devext);
    CreateIoQueues(devext);

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
#endif

static void FillPortConfiguration(PPORT_CONFIGURATION_INFORMATION portcfg, CNvmeDevice* nvme)
{
    portcfg->MaximumTransferLength = nvme->MaxTxSize;//MAX_TX_SIZE;
    portcfg->NumberOfPhysicalBreaks = nvme->MaxTxPages;//MAX_TX_PAGES;
    portcfg->AlignmentMask = FILE_LONG_ALIGNMENT;    //PRP 1 need align DWORD in some case. So set this align is better.
    portcfg->MiniportDumpData = NULL;
    portcfg->InitiatorBusId[0] = 1;
    portcfg->CachesData = FALSE;
    portcfg->MapBuffers = STOR_MAP_NON_READ_WRITE_BUFFERS; //specify bounce buffer type?
    portcfg->MaximumNumberOfTargets = NVME_CONST::MAX_TARGETS;
    portcfg->SrbType = SRB_TYPE_STORAGE_REQUEST_BLOCK;
    portcfg->DeviceExtensionSize = sizeof(CNvmeDevice);
    portcfg->SrbExtensionSize = sizeof(SPCNVME_SRBEXT);
    portcfg->MaximumNumberOfLogicalUnits = NVME_CONST::MAX_LU;
    portcfg->SynchronizationModel = StorSynchronizeFullDuplex;
    portcfg->HwMSInterruptRoutine = NvmeMsixISR;
    portcfg->InterruptSynchronizationMode = InterruptSynchronizePerMessage;
    portcfg->NumberOfBuses = 1;
    portcfg->ScatterGather = TRUE;
    portcfg->Master = TRUE;
    portcfg->AddressType = STORAGE_ADDRESS_TYPE_BTL8;
    portcfg->Dma64BitAddresses = SCSI_DMA64_MINIPORT_SUPPORTED;
    portcfg->MaxNumberOfIO = NVME_CONST::MAX_IO_PER_LU * NVME_CONST::MAX_LU;
    portcfg->MaxIOsPerLun = NVME_CONST::MAX_IO_PER_LU;

    //Dump is not supported now. Will be supported in future.
    portcfg->RequestedDumpBufferSize = 0;
    portcfg->DumpMode = 0;//DUMP_MODE_CRASH;
    portcfg->DumpRegion.VirtualBase = NULL;
    portcfg->DumpRegion.PhysicalBase.QuadPart = NULL;
    portcfg->DumpRegion.Length = 0;
}


_Use_decl_annotations_ ULONG HwFindAdapter(
    _In_ PVOID devext,
    _In_ PVOID ctx,
    _In_ PVOID businfo,
    _In_z_ PCHAR arg_str,
    _Inout_ PPORT_CONFIGURATION_INFORMATION port_cfg,
    _In_ PBOOLEAN Reserved3)
{
    //Running at PASSIVE_LEVEL!!!!!!!!!

    CDebugCallInOut inout(__FUNCTION__);
    UNREFERENCED_PARAMETER(ctx);
    UNREFERENCED_PARAMETER(businfo);
    UNREFERENCED_PARAMETER(arg_str);
    UNREFERENCED_PARAMETER(Reserved3);

    CNvmeDevice* nvme = (CNvmeDevice*)devext;
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    status = nvme->Setup(port_cfg);
    if (!NT_SUCCESS(status))
        goto error;

    status = nvme->InitController();
    if (!NT_SUCCESS(status))
        goto error;

    status = nvme->IdentifyController(NULL);
    if (!NT_SUCCESS(status))
        goto error;

    //PCI bus related initialize
    //this should be called AFTER identify controller , because 
    //we need identify controller to know MaxTxSize.
    FillPortConfiguration(port_cfg, nvme);

    //register Storport DPC
    StorPortInitializeDpc(devext, &nvme->QueueCplDpc, NvmeDpcRoutine);

    return SP_RETURN_FOUND;

error:
    nvme->Teardown();
    return SP_RETURN_NOT_FOUND;
}

_Use_decl_annotations_ BOOLEAN HwInitialize(PVOID devext)
{
//Running at DIRQL
    CDebugCallInOut inout(__FUNCTION__);
    //initialize perf options
    PERF_CONFIGURATION_DATA read_data = { 0 };
    PERF_CONFIGURATION_DATA write_data = { 0 };
    GROUP_AFFINITY affinity;
    ULONG stor_status;

    memset(&affinity, 0, sizeof(GROUP_AFFINITY));
    read_data.Version = STOR_PERF_VERSION;
    read_data.Size = sizeof(PERF_CONFIGURATION_DATA);
    read_data.MessageTargets = &affinity;

    stor_status = StorPortInitializePerfOpts(devext, TRUE, &read_data);

    if (stor_status == STOR_STATUS_SUCCESS) 
    {
        write_data.Version = STOR_PERF_VERSION;
        write_data.Size = sizeof(PERF_CONFIGURATION_DATA);

        if (read_data.Flags & STOR_PERF_CONCURRENT_CHANNELS)
        {
            write_data.Flags |= STOR_PERF_CONCURRENT_CHANNELS;
            write_data.ConcurrentChannels = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
        }
        if (read_data.Flags & STOR_PERF_NO_SGL)     //I don't use SGL...
            write_data.Flags |= STOR_PERF_NO_SGL;
        if (read_data.Flags & STOR_PERF_DPC_REDIRECTION)
            write_data.Flags |= STOR_PERF_DPC_REDIRECTION;
        if (read_data.Flags & STOR_PERF_OPTIMIZE_FOR_COMPLETION_DURING_STARTIO)
            write_data.Flags |= STOR_PERF_OPTIMIZE_FOR_COMPLETION_DURING_STARTIO;
        if (read_data.Flags & STOR_PERF_DPC_REDIRECTION_CURRENT_CPU)
            write_data.Flags |= STOR_PERF_DPC_REDIRECTION_CURRENT_CPU;

        stor_status = StorPortInitializePerfOpts(devext, FALSE, &write_data);
        ASSERT(STOR_STATUS_SUCCESS == stor_status);
    }

    StorPortEnablePassiveInitialization(devext, HwPassiveInitialize);
    return TRUE;
}

_Use_decl_annotations_
BOOLEAN HwPassiveInitialize(PVOID devext)
{
    //Running at PASSIVE_LEVEL
    CDebugCallInOut inout(__FUNCTION__);
    CNvmeDevice* nvme = (CNvmeDevice*)devext;
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    status = nvme->IdentifyNamespace(NULL);
    if (!NT_SUCCESS(status))
        return FALSE;
    status = nvme->SetInterruptCoalescing(NULL);
    if (!NT_SUCCESS(status))
        return FALSE;
    status = nvme->SetArbitration(NULL);
    if (!NT_SUCCESS(status))
        return FALSE;
    status = nvme->SetSyncHostTime(NULL);
    if (!NT_SUCCESS(status))
        return FALSE;
    status = nvme->SetPowerManagement(NULL);
    if (!NT_SUCCESS(status))
        return FALSE;
    status = nvme->SetAsyncEvent(NULL);
    if (!NT_SUCCESS(status))
        return FALSE;
    status = nvme->RegisterIoQ();
    if (!NT_SUCCESS(status))
        return FALSE;

    return TRUE;
}

_Use_decl_annotations_
BOOLEAN HwBuildIo(_In_ PVOID devext,_In_ PSCSI_REQUEST_BLOCK srb)
{
//BuildIo() callback is used to perform "resource preparing" and "jobs DON'T NEED lock".
//In this callback, also dispatch some behavior which need be handled very fast.
//some event (e.g. REMOVE_DEVICE and POWER_EVENTS) only fire once and need to be handled quickly.
//We can't dispatch such events to StartIo(), that could waste too much time.

    PSPCNVME_SRBEXT srbext = SPCNVME_SRBEXT::GetSrbExt(devext, (PSTORAGE_REQUEST_BLOCK)srb);
    BOOLEAN need_startio = FALSE;
    //UCHAR srb_status = 0;

    switch (srbext->FuncCode)
    {
    case SRB_FUNCTION_ABORT_COMMAND:
    case SRB_FUNCTION_RESET_LOGICAL_UNIT:
    case SRB_FUNCTION_RESET_DEVICE:
        //skip these request currently. I didn't get any idea yet to handle them.

    case SRB_FUNCTION_RESET_BUS:
    //MSDN said : it is possible for the HwScsiStartIo routine to be called 
    //              with an SRB in which the Function member is set to SRB_FUNCTION_RESET_BUS 
    //              if a NT-based operating system storage class driver requests this operation. 
    //              The HwScsiStartIo routine can simply call the HwScsiResetBus routine 
    //              to satisfy an incoming bus-reset request.
    //But, I don't understand the difference.... Current Windows family are all NT-based system :p
        SrbSetSrbStatus(srb, SRB_STATUS_INVALID_REQUEST);
        need_startio = FALSE;
        break;
    //case SRB_FUNCTION_WMI:

    case SRB_FUNCTION_POWER:
        BuildIo_SrbPowerHandler(srbext);
        need_startio = FALSE;
        break;
    case SRB_FUNCTION_EXECUTE_SCSI:
        need_startio = BuildIo_ScsiHandler(srbext);
        break;
    case SRB_FUNCTION_IO_CONTROL:
        //should check signature to determine incoming IOCTL
        break;
    case SRB_FUNCTION_PNP:
        //should handle PNP remove adapter
        break;
    default:
        need_startio = BuildIo_DefaultHandler(srbext);
        break;

    }
    return need_startio;
}

_Use_decl_annotations_
BOOLEAN HwStartIo(PVOID devext, PSCSI_REQUEST_BLOCK srb)
{
    PSPCNVME_SRBEXT srbext = SPCNVME_SRBEXT::GetSrbExt(devext, (PSTORAGE_REQUEST_BLOCK)srb);
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
    UNREFERENCED_PARAMETER(ControlType);
    UNREFERENCED_PARAMETER(Parameters);
    SCSI_ADAPTER_CONTROL_STATUS status = ScsiAdapterControlUnsuccessful;
    CNvmeDevice *nvme = (CNvmeDevice*)DeviceExtension;

    switch (ControlType)
    {
    case ScsiQuerySupportedControlTypes:
    {
        status = Handle_QuerySupportedControlTypes((PSCSI_SUPPORTED_CONTROL_TYPE_LIST)Parameters);
        break;
    }
    case ScsiStopAdapter:
    {
    //Device "entering" D1 / D2 / D3 from D0
    //**running at DIRQL
        //this is post event of DeviceRemove. 
        //In normal procedure of removing device, we don't have to do anything here.
        //BuildIo SRB_FUNCTION_PNP should handle DEVICE_REMOVE teardown.
        //ScsiStopAdapter is just a power state handler, we have no idea that
        //"is this call a device remove? or sleep? or hibernation? or power down?"
        status = ScsiAdapterControlSuccess;
        break;
    }
    case ScsiRestartAdapter:
    {
    //Device "entering" D0 state from D1 D2 D3 state
    //**running at DIRQL
        status = Handle_RestartAdapter(nvme);
        break;
    }
    case ScsiAdapterSurpriseRemoval:
    {
    //**running < DISPATCH_LEVEL
    //Device is surprise removed.
    //**Is still SRB_PNP_xxx fired if this control code supported/implemented?
    //***SurpriseRemove don't need unregister queues. Only need to delete queues.
        nvme->Teardown();
        break;
    }
    //Valid since Win8. If this control enabled, storport will notify 
    //miniport when power plan changed.
    case ScsiPowerSettingNotification:
    {
     //**running at PASSIVE_LEVEL
        STOR_POWER_SETTING_INFO* info = (STOR_POWER_SETTING_INFO*) Parameters;
        UNREFERENCED_PARAMETER(info);
        break;
    }

    //Valid since Win8. If this control enabled, miniport won't receive
    //SRB_FUNCTION_POWER in BuildIo and no ScsiStopAdapter in AdapterControl
    case ScsiAdapterPower:
    {
     //**running <= DISPATCH_LEVEL
      STOR_ADAPTER_CONTROL_POWER *power = (STOR_ADAPTER_CONTROL_POWER *)Parameters;
      UNREFERENCED_PARAMETER(power);
      break;
    }

#pragma region === Some explain of un-implemented control codes ===
    //If STOR_FEATURE_ADAPTER_CONTROL_PRE_FINDADAPTER is set in HW_INITIALIZATION_DATA of DriverEntry,
    // storport will fire this control code when handling IRP_MN_FILTER_RESOURCE_REQUIREMENTS.
    // **In this control code, DeviceExtension is STILL NOT initialized because HwFindAdapter not called yet.
    //case ScsiAdapterFilterResourceRequirements:
    //{
    //  //**running < DISPATCH_LEVEL
    //  STOR_FILTER_RESOURCE_REQUIREMENTS* filter = (STOR_FILTER_RESOURCE_REQUIREMENTS*)Parameters;
    //    break;
    //}

    //storport call this control code before  ScsiRestartAdapter.
    // interrupt is NOT connected yet here.
    // If HBA need restore config and resource via StorPortGetBusData() 
    // or StorPortSetBusDataByOffset(), we should implement this control code.
    // ** this means if you need re-touch PCIe resource and reconfig 
    //    runtime config(remap io space...etc), you'll need this control code.
    //case ScsiSetRunningConfig:
    //{
    //     //**running at PASSIVE_LEVEL
    //    //status = HandleScsiSetRunningConfig();
    //    break;
    //}
#pragma endregion

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
    //If there are DeviceIoControl use IOCTL_MINIPORT_PROCESS_SERVICE_IRP, 
    //we should implement this callback.
    
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
    //If HwProcessServiceRequest()is implemented, this HwCompleteServiceIrp 
    // wiill be called before stop to retrieve any new IOCTL_MINIPORT_PROCESS_SERVICE_IRP requests.
    //This callback give us a chance to cleanup requests.

    //example: if IOCTL_MINIPORT_PROCESS_SERVICE_IRP handler send IRP to 
    //          another device and waiting result, we should cancel waiting 
    //          and cleanup that IRP in this callback.

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

    //UnitControl is very similar as AdapterControl.
    //First call will query "ScsiQuerySupportedControlTypes", then 
    //miniport need fill corresponding element to report.

    //ScsiUnitStart => a unit is starting up (disk spin up?)
    //ScsiUnitPower => unit power on or off, [Parameters] arg is STOR_UNIT_CONTROL_POWER*
    //ScsiUnitRemove => DeviceRemove post event of Unit
    //ScsiUnitSurpriseRemoval => SurpriseRemoved event of Unit
    
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
