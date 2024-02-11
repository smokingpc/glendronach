#include "pch.h"

static void FillPortConfiguration(
            PPORT_CONFIGURATION_INFORMATION portcfg, 
            PVROC_DEVEXT devext)
{
//Because MaxTxSize and MaxTxPages should be calculated by nvme->CtrlCap and nvme->CtrlIdent,
//So FillPortConfiguration() should be called AFTER nvme->IdentifyController()
    portcfg->MaximumTransferLength = devext->MaxTxSize;
    portcfg->NumberOfPhysicalBreaks = devext->MaxTxPages;
    portcfg->AlignmentMask = FILE_LONG_ALIGNMENT;    //PRP 1 need align DWORD in some case. So set this align is better.
    portcfg->MiniportDumpData = NULL;
    portcfg->InitiatorBusId[0] = 1;
    portcfg->CachesData = FALSE;
    portcfg->MapBuffers = STOR_MAP_ALL_BUFFERS_INCLUDING_READ_WRITE; //specify bounce buffer type?
    portcfg->MaximumNumberOfTargets = MAX_VROC_TARGETS;   //each NVMe treated as a SCSI Target
    portcfg->SrbType = SRB_TYPE_STORAGE_REQUEST_BLOCK;
    portcfg->DeviceExtensionSize = sizeof(VROC_DEVEXT);
    portcfg->SrbExtensionSize = sizeof(SPCNVME_SRBEXT);
    portcfg->MaximumNumberOfLogicalUnits = MAX_SCSI_LOGICAL_UNIT;
    portcfg->SynchronizationModel = StorSynchronizeFullDuplex;
    portcfg->HwMSInterruptRoutine = RaidMsixISR;
    portcfg->InterruptSynchronizationMode = InterruptSynchronizePerMessage;
    portcfg->NumberOfBuses = MAX_VROC_BUSES;    //each VROC NVMe are located at same SCSI bus
    portcfg->ScatterGather = TRUE;
    portcfg->Master = TRUE;
    portcfg->AddressType = STORAGE_ADDRESS_TYPE_BTL8;
    portcfg->Dma64BitAddresses = SCSI_DMA64_MINIPORT_FULL64BIT_SUPPORTED;   //should set this value if MaxNumberOfIO > 1000.
    portcfg->MaxNumberOfIO = MAX_ADAPTER_IO;
    portcfg->MaxIOsPerLun = MAX_IO_PER_LU;

    //this will limit LUN i/o queue and affect HBA Gateway OutstandingMax.
    //stornvme call StorPortSetDeviceQueueDepth() to adjust it dynamically.
    portcfg->InitialLunQueueDepth = MAX_IO_PER_LU;

    //Dump is not supported now. Will be supported in future.
    portcfg->RequestedDumpBufferSize = 0;
    portcfg->DumpMode = 0;//DUMP_MODE_CRASH;
    portcfg->DumpRegion.VirtualBase = NULL;
    portcfg->DumpRegion.PhysicalBase.QuadPart = NULL;
    portcfg->DumpRegion.Length = 0;
    portcfg->FeatureSupport = STOR_ADAPTER_DMA_V3_PREFERRED;
}

_Use_decl_annotations_ ULONG HwFindAdapter(
    _In_ PVOID hbaext,
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

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    PVROC_DEVEXT devext = (PVROC_DEVEXT)hbaext;
    status = devext->Setup(port_cfg);
    if (!NT_SUCCESS(status))
        goto ERROR;

    status = devext->InitAllVrocNvme();
    if (!NT_SUCCESS(status))
        goto ERROR;
    devext->UpdateVrocNvmeDevInfo();

    //[Workaround for AdapterTopologyTelemetry event]
    //before HwInitialize, should init DmaAdapter.
    //AdapterTopologyTelemetry could come in between HwFindAdapter and Hwinitialize.
    //But Storport!RaidpAdapterContiunueScatterGather has problem : it will return
    //error code if DmaAdapter not allocated. But upper callstacks didn't check error codes.
    //This situation cause SRB of AdapterTopologyTelemetry event be leaked.
    //If rapidly create/delete storage controller device, this SRB leaking will make system hang.
    devext->UncachedExt = StorPortGetUncachedExtension(
        hbaext, port_cfg, UNCACHED_EXT_SIZE);

    //PCI bus related initialize
    //this should be called AFTER InitController() , because 
    //we need identify controller to know MaxTxSize.
    FillPortConfiguration(port_cfg, devext);
    return SP_RETURN_FOUND;

ERROR:
    //return SP_RETURN_NOT_FOUND causes driver installation hanging?
    //I don't know why ....
    devext->Teardown();
    return SP_RETURN_ERROR;
}

_Use_decl_annotations_ BOOLEAN HwInitialize(PVOID hbaext)
{
//Running at DIRQL
    //in stornvme, it checks PPORT_CONFIGURATION_INFORMATION::DumpMode.
    //If (DumpMode != 0) , stornvme will do all Initialize here....
    //Todo: crack stornvme to know why it can do init here. This is called in DIRQL.
    CDebugCallInOut inout(__FUNCTION__);
    PVROC_DEVEXT devext = (PVROC_DEVEXT)hbaext;

    //initialize perf options
    PERF_CONFIGURATION_DATA set_perf = { 0 };
    PERF_CONFIGURATION_DATA supported = { 0 };
    ULONG stor_status = STOR_STATUS_SUCCESS;
    //Just using STOR_PERF_VERSION_5, STOR_PERF_VERSION_6 is for Win2019 and above...
    supported.Version = STOR_PERF_VERSION_5;
    supported.Size = sizeof(PERF_CONFIGURATION_DATA);
    stor_status = StorPortInitializePerfOpts(hbaext, TRUE, &supported);
    if (STOR_STATUS_SUCCESS != stor_status)
        return FALSE;

    set_perf.Version = STOR_PERF_VERSION_5;
    set_perf.Size = sizeof(PERF_CONFIGURATION_DATA);

    //Allow multiple I/O incoming concurrently. 
    if (0 != (supported.Flags & STOR_PERF_CONCURRENT_CHANNELS))
    {
        set_perf.Flags |= STOR_PERF_CONCURRENT_CHANNELS;
        set_perf.ConcurrentChannels = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
    }
    //I don't use SGL... but Win10 don't support this flag
    if (0 != (supported.Flags & STOR_PERF_NO_SGL))
        set_perf.Flags |= STOR_PERF_NO_SGL;

    //spread DPC to all cpu. don't make single cpu too busy.
    if (0 != (supported.Flags & STOR_PERF_DPC_REDIRECTION))
        set_perf.Flags |= STOR_PERF_DPC_REDIRECTION;

    //IF not set this flag, storport will attempt to fire completion DPC on
    //original cpu which accept this I/O request.
    if (0 != (supported.Flags & STOR_PERF_DPC_REDIRECTION_CURRENT_CPU))
        set_perf.Flags |= STOR_PERF_DPC_REDIRECTION_CURRENT_CPU;

    if (0 != (supported.Flags & STOR_PERF_OPTIMIZE_FOR_COMPLETION_DURING_STARTIO))
        set_perf.Flags |= STOR_PERF_OPTIMIZE_FOR_COMPLETION_DURING_STARTIO;

    stor_status = StorPortInitializePerfOpts(hbaext, FALSE, &set_perf);
    if (STOR_STATUS_SUCCESS != stor_status)
        return FALSE;

    StorPortEnablePassiveInitialization(hbaext, HwPassiveInitialize);
    return TRUE;
}

_Use_decl_annotations_
BOOLEAN HwPassiveInitialize(PVOID hbaext)
{
    //Running at PASSIVE_LEVEL
    CDebugCallInOut inout(__FUNCTION__);
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    PVROC_DEVEXT devext = (PVROC_DEVEXT)hbaext;
    StorPortPause(hbaext, MAXULONG);
    status = devext->PassiveInitAllVrocNvme();
    StorPortResume(hbaext);
    if (!NT_SUCCESS(status))
        devext->Teardown();

    return (STATUS_SUCCESS == status);
}

_Use_decl_annotations_
BOOLEAN HwBuildIo(_In_ PVOID hbaext,_In_ PSCSI_REQUEST_BLOCK srb)
{
//BuildIo() callback is used to perform "resource preparing" and "jobs DON'T NEED lock".
//In this callback, also dispatch some behavior which need be handled very fast.
//some event (e.g. REMOVE_DEVICE and POWER_EVENTS) only fire once and need to be handled quickly.
//We can't dispatch such events to StartIo(), that could waste too much time.
    UCHAR srb_status = SRB_STATUS_INVALID_REQUEST;
    PVROC_DEVEXT devext = (PVROC_DEVEXT)hbaext;
    PSPCNVME_SRBEXT srbext = InitSrbExt(hbaext, (PSTORAGE_REQUEST_BLOCK)srb);
    CNvmeDevice* nvme = devext->FindVrocNvmeDev(srbext->ScsiPath);

    if(NULL == nvme)
    {
        srb_status = SRB_STATUS_NO_DEVICE;
        goto END;
    }
    SetNvmeDeviceToSrbExt(srbext, nvme);

    switch (srbext->FuncCode())
    {
    case SRB_FUNCTION_EXECUTE_SCSI:
        srb_status = BuildIo_ScsiHandler(srbext);
        break;
    case SRB_FUNCTION_IO_CONTROL:
        //should check signature to determine incoming IOCTL
        srb_status = BuildIo_IoctlHandler(srbext);
        break;
    case SRB_FUNCTION_PNP:
        //should handle PNP remove adapter
        srb_status = BuildIo_SrbPnpHandler(srbext);
        break;
    #if 0
    case SRB_FUNCTION_POWER:
        srb_status = BuildIo_SrbPowerHandler(srbext);
        break;
    case SRB_FUNCTION_RESET_LOGICAL_UNIT:
    case SRB_FUNCTION_ABORT_COMMAND:
    case SRB_FUNCTION_RESET_DEVICE:
        //skip these request currently. I didn't get any idea yet to handle them.
    case SRB_FUNCTION_RESET_BUS:
        //MSDN said : 
        //  it is possible for the HwScsiStartIo routine to be called 
        //  with an SRB in which the Function member is set to SRB_FUNCTION_RESET_BUS 
        //  if a NT-based operating system storage class driver requests this operation. 
        //  The HwScsiStartIo routine can simply call the HwScsiResetBus routine 
        //  to satisfy an incoming bus-reset request.
        //  I don't understand the difference.... 
        //  Current Windows family are already all NT-based system :p
    #endif
    default:
        srb_status = BuildIo_DefaultHandler(srbext);
        break;

    }

END:
    if(SRB_STATUS_PENDING != srb_status)
        srbext->CompleteSrb(srb_status);

    //if srbstatus is pending, it need StartIo to following process.
    return (SRB_STATUS_PENDING == srb_status);
}

_Use_decl_annotations_
BOOLEAN HwStartIo(PVOID hbaext, PSCSI_REQUEST_BLOCK srb)
{
    UNREFERENCED_PARAMETER(hbaext);
    PSPCNVME_SRBEXT srbext = GetSrbExt((PSTORAGE_REQUEST_BLOCK)srb);
    UCHAR srb_status = SRB_STATUS_ERROR;

    switch (srbext->FuncCode())
    {
    //case SRB_FUNCTION_RESET_LOGICAL_UNIT:     //dispatched in HwUnitControl
    //case SRB_FUNCTION_RESET_DEVICE:           //dispatched in HwAdapterControl
    //case SRB_FUNCTION_RESET_BUS:              //dispatched in HwResetBus
    //case SRB_FUNCTION_POWER:                  //dispatched in HwBuildIo
    //case SRB_FUNCTION_ABORT_COMMAND:          //should I support abort of async I/O?
    //case SRB_FUNCTION_WMI:                    //TODO: do it later....
    //    srb_status = DefaultCmdHandler(srb, srbext);
    //    break;
    case SRB_FUNCTION_EXECUTE_SCSI:
        srb_status = StartIo_ScsiHandler(srbext);
        break;
    case SRB_FUNCTION_IO_CONTROL:
        srb_status = StartIo_IoctlHandler(srbext);
        break;
    //case SRB_FUNCTION_PNP:
        //pnp handlers
        //scsi handlers
    default:
        srb_status = StartIo_DefaultHandler(srbext);
        break;
    }

    //todo: handle SCSI status for SRB
    if (srb_status != SRB_STATUS_PENDING)
        srbext->CompleteSrb(srb_status);

    //return TRUE indicates that "this driver handled this request, 
    //no matter succeed or fail..."
    return TRUE;
}

_Use_decl_annotations_
BOOLEAN HwResetBus(
    PVOID DeviceExtension,
    ULONG PathId
)
{
    CDebugCallInOut inout(__FUNCTION__);
    //miniport driver is responsible for completing SRBs received by HwStorStartIo for 
    //PathId during this routine and setting their status to SRB_STATUS_BUS_RESET if necessary.

    PVROC_DEVEXT devext = (PVROC_DEVEXT)DeviceExtension;

    STOR_ADDR_BTL8 addr = {0};
    CNvmeDevice* nvme = devext->FindVrocNvmeDev((UCHAR)PathId);
    nvme->ReleaseOutstandingSrbs();
    return TRUE;
}

_Use_decl_annotations_
SCSI_ADAPTER_CONTROL_STATUS HwAdapterControl(
    PVOID hbaext,
    SCSI_ADAPTER_CONTROL_TYPE ctrlcode,
    PVOID param
)
{
    CDebugCallInOut inout(__FUNCTION__);
    UNREFERENCED_PARAMETER(param);
    SCSI_ADAPTER_CONTROL_STATUS status = ScsiAdapterControlUnsuccessful;
    PVROC_DEVEXT devext = (PVROC_DEVEXT)hbaext;
    DebugAdapterControlCode(ctrlcode);

    switch (ctrlcode)
    {
    case ScsiQuerySupportedControlTypes:
    {
        status = Handle_QuerySupportedControlTypes((PSCSI_SUPPORTED_CONTROL_TYPE_LIST)param);
        break;
    }
    case ScsiStopAdapter:
    {
    //Device "entering" D1 / D2 / D3 from D0
    //**running at DIRQL
        //this is post event of DeviceRemove and PreEvent of PCI rebalance.
        //In PCI rebalance, we should disable controller and free all resources
        //here and stoport will call HwFindAdapter again.
        
        //Before adapter remove, suspend/hiberation, and rebalance , 
        //it will enter D3 state.
        //Here should disable controller so storport can disconnect interrupts.
        devext->DisableAllVrocNvmeControllers();
        break;
    }
    case ScsiRestartAdapter:
    {
    //Device "entering" D0 state from D1 D2 D3 state
    //This control code do similar things(exclude PerfConfig) as HwInitialize().
    //**running at DIRQL
    //this event handles "Wakeup" from suspend/hiberation.
    //StopAdapter disabled controller so should restart it here.
        status = ScsiAdapterControlUnsuccessful;
        break;
    }
    case ScsiAdapterSurpriseRemoval:
    {
    //**running < DISPATCH_LEVEL
    //Device is surprise removed.
    //**Will SRB_PNP_xxx still be fired if this control code supported/implemented?
    //***SurpriseRemove don't need unregister queues. Only need to delete queues.
        devext->Teardown();
        status = ScsiAdapterControlSuccess;
        break;
    }
    //Valid since Win8. If this control enabled, storport will notify 
    //miniport when power plan changed.
    case ScsiPowerSettingNotification:
    {
     //**running at PASSIVE_LEVEL
        STOR_POWER_SETTING_INFO* info = (STOR_POWER_SETTING_INFO*) param;
        UNREFERENCED_PARAMETER(info);
        status = ScsiAdapterControlSuccess;
        break;
    }

    //Valid since Win8. If this control enabled, miniport won't receive
    //SRB_FUNCTION_POWER in BuildIo and no ScsiStopAdapter in AdapterControl
    case ScsiAdapterPower:
    {
     //**running <= DISPATCH_LEVEL
        STOR_ADAPTER_CONTROL_POWER *power = (STOR_ADAPTER_CONTROL_POWER *)param;
        UNREFERENCED_PARAMETER(power);
        status = ScsiAdapterControlSuccess;
        break;
    }

    case ScsiSetBootConfig: //**running at PASSIVE_LEVEL, called AFTER ScsiStopAdapter ??
    case ScsiSetRunningConfig://**running at PASSIVE_LEVEL, called BEFORE ScsiRestartAdapter ??
    case ScsiAdapterPoFxPowerRequired://**running <= DISPATCH_LEVEL
    case ScsiAdapterPoFxPowerActive://**running <= DISPATCH_LEVEL
    case ScsiAdapterPoFxPowerSetFState://**running <= DISPATCH_LEVEL
    case ScsiAdapterPoFxPowerControl://**running <= DISPATCH_LEVEL
    case ScsiAdapterPrepareForBusReScan://**running at PASSIVE_LEVEL
    case ScsiAdapterSystemPowerHints://**running at PASSIVE_LEVEL
    case ScsiAdapterFilterResourceRequirements://**running < DISPATCH_LEVEL
    case ScsiAdapterPoFxMaxOperationalPower://**running at PASSIVE_LEVEL
    case ScsiAdapterPoFxSetPerfState:       //**running <= DISPATCH_LEVEL
    case ScsiAdapterSerialNumber:       //**running < DISPATCH_LEVEL
    case ScsiAdapterCryptoOperation:    //**running at PASSIVE_LEVEL
    case ScsiAdapterQueryFruId: //**running at PASSIVE_LEVEL
    case ScsiAdapterSetEventLogging:    //**running at PASSIVE_LEVEL
        status = ScsiAdapterControlSuccess;
        break;


#pragma region === Some explain of un-implemented control codes ===
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
    PVOID hbaext,
    PVOID request_irp
)
{
    //If there are DeviceIoControl use IOCTL_MINIPORT_PROCESS_SERVICE_IRP, 
    //we should implement this callback.
    
    CDebugCallInOut inout(__FUNCTION__);
    PIRP irp = (PIRP)request_irp;
    //UNREFERENCED_PARAMETER(Irp);
    ////ioctl interface for miniport
    //PSMOKY_EXT devext = (PSMOKY_EXT)DeviceExtension;
    //PIRP irp = (PIRP)Irp;
    //irp->IoStatus.Information = 0;
    //irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
    //StorPortCompleteServiceIrp(devext, irp);

    irp->IoStatus.Information = 0;
    irp->IoStatus.Status = STATUS_SUCCESS;
    StorPortCompleteServiceIrp(hbaext, irp);
}

_Use_decl_annotations_
void HwCompleteServiceIrp(PVOID hbaext)
{
    //If HwProcessServiceRequest()is implemented, this HwCompleteServiceIrp 
    // wiill be called before stop to retrieve any new IOCTL_MINIPORT_PROCESS_SERVICE_IRP requests.
    //This callback give us a chance to cleanup requests.

    //example: if IOCTL_MINIPORT_PROCESS_SERVICE_IRP handler send IRP to 
    //          another device and waiting result, we should cancel waiting 
    //          and cleanup that IRP in this callback.

    CDebugCallInOut inout(__FUNCTION__);
    UNREFERENCED_PARAMETER(hbaext);
    //if any async request in HwProcessServiceRequest, 
    //we should complete them here and let them go back asap.
}
