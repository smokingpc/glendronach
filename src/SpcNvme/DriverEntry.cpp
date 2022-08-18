#include "pch.h"

EXTERN_C_START
sp_DRIVER_INITIALIZE DriverEntry;
ULONG DriverEntry(IN PVOID DrvObj, IN PVOID RegPath)
{
    CDebugCallInOut inout(__FUNCTION__);
    if (IsSupportedOS(10) == FALSE)
        return STOR_STATUS_UNSUPPORTED_VERSION;

    HW_INITIALIZATION_DATA init_data = { 0 };
    ULONG status = 0;

    // Set size of hardware initialization structure.
    init_data.HwInitializationDataSize = sizeof(HW_INITIALIZATION_DATA);

    // Identify required miniport entry point routines.
    init_data.HwInitialize = HwInitialize;
    init_data.HwBuildIo = HwBuildIo;
    init_data.HwStartIo = HwStartIo;
    init_data.HwFindAdapter = HwFindAdapter;        //AddDevice() + IRP_MJ_PNP +  IRP_MN_READ_CONFIG
    init_data.HwResetBus = HwResetBus;
    init_data.HwAdapterControl = HwAdapterControl;
    init_data.HwUnitControl = HwUnitControl;
    init_data.HwTracingEnabled = HwTracingEnabled;
    init_data.HwCleanupTracing = HwCleanupTracing;

    //IRP_MJ_DEVICE_CONTROL + IOCTL code == IOCTL_MINIPORT_PROCESS_SERVICE_IRP
    //to prevent IOCTL request in DPC_LEVEL, define another IOCTL to make sure it is processed in PASSIVE.
    //so define this IOCTL_MINIPORT_PROCESS_SERVICE_IRP
    init_data.HwProcessServiceRequest = HwProcessServiceRequest;
    //complete NOT FINISHED IRP received in HwProcessServiceRequest. it called when device removed
    init_data.HwCompleteServiceIrp = HwCompleteServiceIrp;

    // Specifiy adapter specific information.
    init_data.AutoRequestSense = TRUE;
    init_data.NeedPhysicalAddresses = TRUE;
    init_data.AdapterInterfaceType = PCIBus;
    init_data.MapBuffers = STOR_MAP_NON_READ_WRITE_BUFFERS;
    init_data.TaggedQueuing = TRUE;
    init_data.MultipleRequestPerLu = TRUE;

    // Specify support/use SRB Extension for Windows 8 and up
    init_data.SrbTypeFlags = SRB_TYPE_FLAG_STORAGE_REQUEST_BLOCK;
    init_data.FeatureSupport = STOR_FEATURE_FULL_PNP_DEVICE_CAPABILITIES;

    /* Set required extension sizes. */
    init_data.DeviceExtensionSize = sizeof(SPCNVME_DEVEXT);
    init_data.SrbExtensionSize = sizeof(SPCNVME_SRBEXT);

    // Call StorPortInitialize to register with HwInitData
    status = StorPortInitialize(DrvObj, RegPath, &init_data, NULL);

    return status;
}
EXTERN_C_END
