#include "pch.h"

BOOLEAN RaidMsixISR(IN PVOID hbaext, IN ULONG msgid)
{
    _SPC_DEVEXT *devext = (_SPC_DEVEXT*)hbaext;
    BOOLEAN ok = FALSE;
    CNvmeDevice* nvme = NULL;
    CNvmeQueue* queue = NULL;
    for (ULONG i = 0; i < MAX_CHILD_VROC_DEV; i++)
    {
        nvme = devext->NvmeDev[i];
        if (NULL == nvme)
            continue;

        queue = (msgid == 0) ? nvme->AdmQueue : nvme->IoQueue[msgid - 1];
        ok = StorPortIssueDpc(devext, &queue->QueueCplDpc, NULL, NULL);
    }

    return TRUE;
}
void GetPcieCapFromPciCfg(PPCI_COMMON_CONFIG pcicfg, PPCIE_CAP &ret)
{
    PPCI_CAPABILITIES_HEADER cap = NULL;
    ret = NULL;

    UCHAR type = pcicfg->HeaderType &(~PCI_MULTIFUNCTION);
    //highest bit of HeaderType is enable flag....
    switch(type)
    {
        case PCI_DEVICE_TYPE:
            cap = (PPCI_CAPABILITIES_HEADER)((PUCHAR)pcicfg + pcicfg->u.type0.CapabilitiesPtr);
            break;
        case PCI_BRIDGE_TYPE:
            cap = (PPCI_CAPABILITIES_HEADER)((PUCHAR)pcicfg + pcicfg->u.type1.CapabilitiesPtr);
            break;
        //case PCI_CARDBUS_BRIDGE_TYPE:
        //    cap = (PPCI_CAPABILITIES_HEADER)((PUCHAR)pcicfg + pcicfg->u.type2.CapabilitiesPtr);
        //    break;
        default:
            DbgBreakPoint();
            return;
    }

    while (IsValidPciCap(cap))
    {
        if (PCI_CAPABILITY_ID_PCI_EXPRESS == cap->CapabilityID)
        {
            ret = (PPCIE_CAP)cap;
            return;
        }

        if (0 == cap->Next)
        {
            cap = NULL;
        }
        else
        {
            //cap->Next means "offset from HeaderBegin, not "
            cap = (PPCI_CAPABILITIES_HEADER)((PUCHAR)pcicfg + cap->Next);
        }
    }
}
void GetVrocNvmeBar0(PPCI_COMMON_CONFIG pcicfg, PHYSICAL_ADDRESS &bar0)
{
    bar0.QuadPart = 0;
    ASSERT(IsPciDevice(pcicfg->HeaderType));
    if(!IsPciDevice(pcicfg->HeaderType))
        return;

    bar0.LowPart = pcicfg->u.type0.BaseAddresses[0];
    bar0.HighPart = pcicfg->u.type0.BaseAddresses[1];
}
PUCHAR GetBusCfgSpace(PUCHAR vmd_space, ULONG max_len, UCHAR bus_offset)
{
    if (bus_offset >= PCI_MAX_BRIDGE_NUMBER)
        return NULL;


    //"vmd_space" is "ptr mapped BAR0 of RaidController"
    ULONG bus_space_size = GetPciBusCfgSpaceSize(1);
    if ((bus_space_size*bus_offset) >= max_len)
        return NULL;

    return (vmd_space + bus_space_size * bus_offset);
}
PUCHAR GetDevCfgSpace(PUCHAR bus_space, UCHAR dev_id)
{
    if (dev_id >= PCI_MAX_DEVICES)
        return NULL;

    return (bus_space + GetPciDevCfgSpaceSize(1) * dev_id);
}

#pragma region ======== _VROC_BUS ========
void _VROC_BUS::Setup(UINT16 segment_idx, UCHAR primary_bus, UCHAR bus_idx, PPCI_COMMON_CONFIG bridge_cfg, PUCHAR bus_space)
{
    InitializeListHead(&List);
    InitializeListHead(&DevListHead);

    BridgeCfg = bridge_cfg;
    BusSpace = bus_space;
    PrimaryBus = primary_bus;
    MyBusIdx = bus_idx;
    MyBus = bus_idx + primary_bus;
    PciSegmentIdx = segment_idx;
}
void _VROC_BUS::Teardown()
{
    RemoveAllDevices();
}
void _VROC_BUS::AddDevice(struct _VROC_DEVICE* dev)
{
    InsertTailList(&DevListHead, &dev->List);
    dev->ParentBus = this;

}
void _VROC_BUS::RemoveAllDevices()
{
    while(!IsListEmpty(&DevListHead))
    {
        PLIST_ENTRY entry = RemoveHeadList(&DevListHead);
        PVROC_DEVICE dev = CONTAINING_RECORD(entry, VROC_DEVICE, List);
        dev->Teardown();
        delete dev;
    }
}
void _VROC_BUS::UpdateBridgeMemoryWindow(PHYSICAL_ADDRESS window_start, PHYSICAL_ADDRESS window_end)
{
//todo : update prefetch mem range and i/o range....
    //NonPrefetchable MemBase
    NpMemBase = window_start;
    NpMemLimit = window_end;
    UINT16 base = (UINT16)((window_start.QuadPart >> 16) & 0xFFF0ULL); //low 4 bits are readonly, ignore them. 
    UINT16 limit = (UINT16)((window_end.QuadPart >> 16) & 0xFFF0ULL); //low 4 bits are readonly, ignore them. 
    
    this->BridgeCfg->u.type1.MemoryBase = base;
    this->BridgeCfg->u.type1.MemoryLimit = limit;
}
#pragma endregion
#pragma region ======== _VROC_DEVICE ========
void _VROC_DEVICE::Setup(PVROC_BUS bus, UCHAR dev_id, PPCI_COMMON_CONFIG cfg, PHYSICAL_ADDRESS bar0)
{
    RtlZeroMemory(this, sizeof(VROC_DEVICE));
    InitializeListHead(&List);
    DevCfg = cfg;
    DevId = dev_id;
    IsBar0InIoSpace = ((cfg->u.type0.BaseAddresses[0] & 0x00000001) == TRUE);

//update BAR0 in VROC NVMe device's PCI Config Space.
    Bar0PA = bar0;
    Bar0Len = VROC_NVME_BAR0_SIZE;
    cfg->u.type0.BaseAddresses[0] = bar0.LowPart;
    cfg->u.type0.BaseAddresses[1] = bar0.HighPart;
    ParentBus = bus;
}
void _VROC_DEVICE::Teardown()
{
    DeleteNvmeDevice();
}
void _VROC_DEVICE::CreateNvmeDevice(PVOID devext)
{
    DbgBreakPoint();
    NvmeDev = new (NonPagedPool, TAG_VROC_NVME) CNvmeDevice[1];
    DbgBreakPoint();
    NvmeDev->Setup(devext, DevCfg, Bar0PA);
}
void _VROC_DEVICE::DeleteNvmeDevice()
{
    if(NULL != NvmeDev)
    {
        NvmeDev->Teardown();
        delete NvmeDev;
        NvmeDev = NULL;
    }
}
#pragma endregion
//#pragma region ======== SPC_DEVEXT ========
//#pragma endregion

#pragma region ======== SPC_DEVEXT ========
NTSTATUS _SPC_DEVEXT::Setup(PPORT_CONFIGURATION_INFORMATION portcfg)
{
    RtlZeroMemory(this, sizeof(_SPC_DEVEXT));
    PortCfg = portcfg;

    InitializeListHead(&BusListHead);
    GetRaidCtrlPciCfg();
    MapRaidCtrlBar0(*portcfg->AccessRanges, portcfg->NumberOfAccessRanges);
    EnumVrocBuses();

    return STATUS_UNSUCCESSFUL;
}
NTSTATUS _SPC_DEVEXT::GetRaidCtrlPciCfg()
{
    ULONG size = sizeof(PciCfg);
    ULONG status = StorPortGetBusData(this, 
                        PortCfg->AdapterInterfaceType, 
                        PortCfg->SystemIoBusNumber, 
                        PortCfg->SlotNumber, 
                        &PciCfg, size);

    if(2 == status || status != size)
        return STATUS_UNSUCCESSFUL;

    //Virtual PCI-bus CAP is following PCI_COMMON_HEADER.
    //It is first CAP in PCI_COMMON_HEADER::DeviceSpecific.
    //All regular CAPs will be after this CAP.
    PVROC_RAID_VIRTUAL_BUS_CAP cap = 
            (PVROC_RAID_VIRTUAL_BUS_CAP)PciCfg.DeviceSpecific;
    if(cap->UseMask)
    {
        switch(cap->BusMask)
        {
            case 0:
                PrimaryBus = 0;
                break;
            case 1:
                PrimaryBus = 0x80;
                break;
            case 2:
                PrimaryBus = 0xE0;  //E0 or C0?? no idea...
                break;
            default:
                return STATUS_UNSUCCESSFUL;
        }
    }

    return STATUS_SUCCESS;
}
void _SPC_DEVEXT::MapRaidCtrlBar0(ACCESS_RANGE* ranges, ULONG count)
{
    AccessRangeCount = min(ACCESS_RANGE_COUNT, count);
    RtlCopyMemory(AccessRanges, ranges, AccessRangeCount * sizeof(ACCESS_RANGE));

    INTERFACE_TYPE type = PortCfg->AdapterInterfaceType;
    PACCESS_RANGE range = NULL;
    PUCHAR addr = NULL;
    BOOLEAN in_iospace = FALSE;
    
    range = &AccessRanges[0];
    in_iospace = !range->RangeInMemory;
    addr = (PUCHAR)StorPortGetDeviceBase(
        this, type,
        PortCfg->SystemIoBusNumber, range->RangeStart,
        range->RangeLength, in_iospace);
    if (NULL != addr)
    {
        RaidPcieCfgSpace = addr;
        RaidPcieCfgSpaceSize = range->RangeLength;
        RaidPcieCfgSpacePA = range->RangeStart;
    }

    range = &AccessRanges[1];
    in_iospace = !range->RangeInMemory;
    addr = (PUCHAR)StorPortGetDeviceBase(
        this, type,
        PortCfg->SystemIoBusNumber, range->RangeStart,
        range->RangeLength, in_iospace);
    if (NULL != addr)
    {
        RaidNvmeCfgSpace = addr;
        RaidNvmeCfgSpaceSize = range->RangeLength;
        RaidNvmeCfgSpacePA = range->RangeStart;
    }

    range = &AccessRanges[2];
    in_iospace = !range->RangeInMemory;
    addr = (PUCHAR)StorPortGetDeviceBase(
        this, type,
        PortCfg->SystemIoBusNumber, range->RangeStart,
        range->RangeLength, in_iospace);
    if (NULL != addr)
    {
        RaidMsixCfgSpace = addr;
        RaidMsixCfgSpaceSize = range->RangeLength;
        RaidMsixCfgSpacePA = range->RangeStart;
    }
}
void _SPC_DEVEXT::EnumVrocBuses()
{
    //VROC Controller CfgSpace in Bar0 has 32MB.
    //It indicates 32 buses :
    //Bus0 has only PCI Bridges, and all NVMe are located on Bus1~Bus31.
    //Each Bus has NVMe devices and connected to VMD RaidController via 1 bridge.
    //So we should treat as a real PCI bridged bus => It is similar as extra PCI segment.

    //PCI bus has max 32 devices on 1 bus....
    //Enum bridges first.
    //Bus 0 stores Pci Bridges, other Bus stores VROC NVMe devices.
    PUCHAR bridge_bus_space = GetBusCfgSpace(RaidPcieCfgSpace, RaidPcieCfgSpaceSize, 0);
    UCHAR bus_idx = 1;  //bus 0 is for root bus which has bridges
    UINT16 segment_idx = VROC_PCI_SEGMENT_START_ID;      //PCI segment index.
    for (UCHAR bridge_idx = 0; bridge_idx < PCI_MAX_DEVICES; bridge_idx++)
    {
        PPCI_COMMON_CONFIG bridge = (PPCI_COMMON_CONFIG)GetDevCfgSpace(bridge_bus_space, bridge_idx);
        if (!IsValidVendorID(bridge))
            continue;

        if (!IsPciBridge(bridge->HeaderType))
            continue;

        //found a bridge device, set it up and enum its child bus

        //[Don't forget to set MemBase/MemLimit of this bridge.]
        //System can't access resources of all devices behind this bridge, 
        // if you didn't setup MemBase/MemLimit.
        PVROC_BUS bus = new (NonPagedPool, TAG_VROC_BUS) VROC_BUS();
        //PVROC_BUS bus = (PVROC_BUS) ExAllocatePoolWithTag(NonPagedPool, sizeof(VROC_BUS), TAG_VROC_BUS);
        //RtlZeroMemory(bus, sizeof(VROC_BUS));
        DbgBreakPoint();
        PUCHAR dev_bus_space = GetBusCfgSpace(RaidPcieCfgSpace, RaidPcieCfgSpaceSize, bus_idx);
        bus->Setup(segment_idx, PrimaryBus, bus_idx, bridge, dev_bus_space);
        
        PHYSICAL_ADDRESS start = {0}, end = {0};
        start.QuadPart = RaidNvmeCfgSpacePA.QuadPart + (bus_idx * VROC_BRIDGE_WINDOW_SIZE);
        end.QuadPart = start.QuadPart + VROC_BRIDGE_WINDOW_SIZE - 1;
        bus->UpdateBridgeMemoryWindow(start, end);

        EnumVrocDevsOnBus(bus);
        InsertTailList(&BusListHead, &bus->List);
        bus_idx++;
        segment_idx++;
    }
}
void _SPC_DEVEXT::EnumVrocDevsOnBus(PVROC_BUS bus)
{
    //enumerate all devices on this child bus.
    for (UCHAR dev_id = 0; dev_id < PCI_MAX_DEVICES; dev_id++)
    {
        // currently only enumerate 1 device on bus.
        PPCI_COMMON_CONFIG dev_space = (PPCI_COMMON_CONFIG)GetDevCfgSpace(bus->BusSpace, dev_id);
        if (!IsValidVendorID(dev_space))
            continue;
        
        PVROC_DEVICE dev = new (NonPagedPool, TAG_VROC_DEVICE) VROC_DEVICE[1];
        //PVROC_DEVICE dev = (PVROC_DEVICE)ExAllocatePoolWithTag(NonPagedPool, sizeof(VROC_DEVICE), TAG_VROC_DEVICE);
        //RtlZeroMemory(dev, sizeof(VROC_DEVICE));

        DbgBreakPoint();
        //calculate Bar0 PhyAddr for this device.
        PHYSICAL_ADDRESS nvme_bar0 = {0};
        nvme_bar0.QuadPart = bus->NpMemBase.QuadPart +
                                (dev_id * VROC_NVME_BAR0_SIZE);
        dev->Setup(bus, dev_id, dev_space, nvme_bar0);
        DbgBreakPoint();
        dev->CreateNvmeDevice(this);
        //Debug
        DbgBreakPoint();

        bus->AddDevice(dev);
        NvmeDev[NvmeDevCount++] = dev->NvmeDev;
        //Todo: currently only enum 1 device on bus.
        //      should do it to enum all 32 devices...
        break;
    }
}
void _SPC_DEVEXT::Teardown()
{
    while(!IsListEmpty(&BusListHead))
    {
        PLIST_ENTRY entry = RemoveHeadList(&BusListHead);
        PVROC_BUS bus = CONTAINING_RECORD(entry, VROC_BUS, List);
        bus->Teardown();
        delete bus;
    }
    UncachedExt = NULL;
}
void _SPC_DEVEXT::ShutdownAllVrocNvmeControllers()
{
    for (ULONG i = 0; i < MAX_CHILD_VROC_DEV; i++)
    {
        if (NULL == NvmeDev[i])
            continue;

        NvmeDev[i]->ShutdownController();
    }
}
void _SPC_DEVEXT::DisableAllVrocNvmeControllers()
{
    for (ULONG i = 0; i < MAX_CHILD_VROC_DEV; i++)
    {
        if (NULL == NvmeDev[i])
            continue;

        NvmeDev[i]->DisableController();
    }
}
NTSTATUS _SPC_DEVEXT::InitAllVrocNvme()
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    for (ULONG i = 0; i < MAX_CHILD_VROC_DEV; i++)
    {
        if (NULL == NvmeDev[i])
            continue;

        CNvmeDevice* ptr = NvmeDev[i];

        status = ptr->InitController();
        if (!NT_SUCCESS(status))
        {
            DbgBreakPoint();
            return status;
        }
        status = ptr->IdentifyController(NULL, &ptr->CtrlIdent, true);
        if (!NT_SUCCESS(status))
        {
            DbgBreakPoint();
            return status;
        }
    }

    return STATUS_SUCCESS;
}
NTSTATUS _SPC_DEVEXT::PassiveInitAllVrocNvme()
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    for (ULONG i = 0; i < MAX_CHILD_VROC_DEV; i++)
    {
        CNvmeDevice* ptr = NvmeDev[i];
        if (NULL == ptr)
            continue;
        status = ptr->InitNvmeStage1();
        if (!NT_SUCCESS(status))
        {
            DbgBreakPoint();
            return status;
        }
        status = ptr->InitNvmeStage2();
        if (!NT_SUCCESS(status))
        {
            DbgBreakPoint();
            return status;
        }
        status = ptr->CreateIoQueues();
        if (!NT_SUCCESS(status))
        {
            DbgBreakPoint();
            return status;
        }
        status = ptr->RegisterIoQueues(NULL);
        if (!NT_SUCCESS(status))
        {
            DbgBreakPoint();
            return status;
        }
    }

    return STATUS_SUCCESS;
}
void _SPC_DEVEXT::UpdateVrocNvmeDevInfo()
{
    MaxTxSize = MaxTxPages = 0;
    for (ULONG i = 0; i < MAX_CHILD_VROC_DEV; i++)
    {
        CNvmeDevice* ptr = NvmeDev[i];
        if (NULL == ptr)
            continue;

        //VMD RaidController has multiple NVMe device.
        //My MaxTxSize / MaxTxPages should get minimum value of them.
        if(0 == MaxTxSize || MaxTxSize > ptr->MaxTxSize)
        {
            if(ptr->MaxTxSize > 0)
                MaxTxSize = ptr->MaxTxSize;
            else
                MaxTxSize = DEFAULT_MAX_TXSIZE;
            MaxTxPages = (ULONG)(MaxTxSize / PAGE_SIZE);
        }
    }
}
#pragma endregion
