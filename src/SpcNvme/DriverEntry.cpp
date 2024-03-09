#include "pch.h"

EXTERN_C_START
sp_DRIVER_INITIALIZE DriverEntry;
ULONG DriverEntry(IN PVOID DrvObj, IN PVOID RegPath)
{
    VROC_BUS* bus = NULL;
    VROC_DEVICE* dev = NULL;
    PVOID test = ExAllocatePoolWithTag(NonPagedPool, 72, TAG_GENBUF);
    DbgBreakPoint();
    ExFreePool(test, TAG_GENBUF);
    for (ULONG i = 0; i < 3; i++)
    {
        bus = (VROC_BUS*)ExAllocatePoolWithTag(NonPagedPool, sizeof(VROC_BUS), TAG_VROC_BUS);
        DbgBreakPoint();
        dev = (VROC_DEVICE*)ExAllocatePoolWithTag(NonPagedPool, sizeof(VROC_DEVICE), TAG_VROC_DEVICE);
        ExFreePool(bus, TAG_VROC_BUS);
        ExFreePool(dev, TAG_VROC_DEVICE);

        //bus = MemAllocEx<_VROC_BUS>(NonPagedPool, TAG_VROC_BUS);
        //dev = MemAllocEx<_VROC_DEVICE>(NonPagedPool, TAG_VROC_DEVICE);
        //DbgBreakPoint();
        //MemDelete(bus, TAG_VROC_BUS);
        //MemDelete(dev, TAG_VROC_DEVICE);
        //DbgBreakPoint();
    }

    //CDebugCallInOut inout(__FUNCTION__);
    if (IsSupportedOS(10) == FALSE)
        return STOR_STATUS_UNSUPPORTED_VERSION;

    HW_INITIALIZATION_DATA init_data = { 0 };
    ULONG status = 0;

    // Set size of hardware initialization structure.
    init_data.HwInitializationDataSize = HW_INIT_DATA_SIZE_PHYSICAL;

    // Identify required miniport entry point routines.
    init_data.HwInitialize = HwInitialize;
    init_data.HwBuildIo = HwBuildIo;
    init_data.HwStartIo = HwStartIo;
    init_data.HwFindAdapter = HwFindAdapter;        //AddDevice() + IRP_MJ_PNP +  IRP_MN_READ_CONFIG...blahblah
    init_data.HwResetBus = HwResetBus;
    init_data.HwAdapterControl = HwAdapterControl;
#if 0
    init_data.HwUnitControl = HwUnitControl;
    init_data.HwTracingEnabled = HwTracingEnabled;
    init_data.HwCleanupTracing = HwCleanupTracing;
    //IRP_MJ_DEVICE_CONTROL + IOCTL code IOCTL_MINIPORT_PROCESS_SERVICE_IRP == IOCTL_MINIPORT_PROCESS_SERVICE_IRP
    //to prevent IOCTL request in DPC_LEVEL, define another IOCTL to make sure it is processed in PASSIVE.
    //so define this IOCTL_MINIPORT_PROCESS_SERVICE_IRP
    init_data.HwProcessServiceRequest = HwProcessServiceRequest;
    //complete NOT FINISHED IRP received in HwProcessServiceRequest. it called when device removing
    init_data.HwCompleteServiceIrp = HwCompleteServiceIrp;
#endif

    // Specifiy adapter specific information.
    init_data.AutoRequestSense = TRUE;
    init_data.NeedPhysicalAddresses = TRUE;
    init_data.AdapterInterfaceType = PCIBus;
    init_data.MapBuffers = STOR_MAP_ALL_BUFFERS_INCLUDING_READ_WRITE;
    init_data.TaggedQueuing = TRUE;
    init_data.MultipleRequestPerLu = TRUE;
    init_data.NumberOfAccessRanges = ACCESS_RANGE_COUNT;
    // Specify support/use SRB Extension for Windows 8 and up
    init_data.SrbTypeFlags = SRB_TYPE_FLAG_STORAGE_REQUEST_BLOCK;
    
    //stornvme uses these features:
    //  STOR_FEATURE_DUMP_INFO
    //  STOR_FEATURE_ADAPTER_CONTROL_PRE_FINDADAPTER
    //  STOR_FEATURE_EXTRA_IO_INFORMATION
    //  STOR_FEATURE_DUMP_RESUME_CAPABLE
    //  STOR_FEATURE_DEVICE_NAME_NO_SUFFIX
    //  STOR_FEATURE_DUMP_POINTERS
    init_data.FeatureSupport = STOR_FEATURE_FULL_PNP_DEVICE_CAPABILITIES;
//            STOR_FEATURE_ADAPTER_CONTROL_PRE_FINDADAPTER |
//            STOR_FEATURE_EXTRA_IO_INFORMATION |         //what is this?
//            STOR_FEATURE_FULL_PNP_DEVICE_CAPABILITIES | 
//            STOR_FEATURE_NVME;

    /* Set required extension sizes. */
    init_data.DeviceExtensionSize = sizeof(VROC_DEVEXT);
    init_data.SrbExtensionSize = sizeof(SPCNVME_SRBEXT);

    // Call StorPortInitialize to register with HwInitData
    status = StorPortInitialize(DrvObj, RegPath, &init_data, NULL);

    return STOR_STATUS_UNSUCCESSFUL;
}
EXTERN_C_END
