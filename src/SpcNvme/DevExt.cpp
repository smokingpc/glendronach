#include "pch.h"

BOOLEAN RaidMsixISR(IN PVOID hbaext, IN ULONG msgid)
{
    _VROC_DEVEXT *devext = (_VROC_DEVEXT*)hbaext;
    BOOLEAN ok = FALSE;
    CNvmeDevice* nvme = NULL;
    CNvmeQueue* queue = NULL;
    //DbgBreakPoint();

    for (ULONG i = 0; i < devext->NvmeDevCount; i++)
    {
        nvme = devext->NvmeDev[i];
        if (NULL == nvme)
            continue;
        if(0 == msgid)
            DbgBreakPoint();
        else
        {
            queue = ((msgid-1) == 0) ? nvme->AdmQueue : nvme->IoQueue[msgid - 1 - 1];
            ok = StorPortIssueDpc(devext, &queue->QueueCplDpc, NULL, NULL);
        }
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
            return;
    }

    if(NULL == cap)
        return;

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


    //"vmd_space" is "nvme mapped BAR0 of RaidController"
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

#pragma region ======== CVrocBus ========
void CVrocBus::Setup(
                UCHAR primary_bus, 
                UCHAR bus_idx, 
                PPCI_COMMON_CONFIG bridge_cfg, 
                PUCHAR bus_space,
                PUCHAR dev_bar0_space)
{
    InitializeListHead(&this->List);
    InitializeListHead(&this->DevListHead);

    BridgeCfg = bridge_cfg;
    BusSpace = bus_space;
    PrimaryBus = primary_bus;
    MyBusIdx = bus_idx;
    MyBus = bus_idx + primary_bus;
    Bar0SpaceForDevAndBridge = dev_bar0_space;
    Guard = 0x23939889;
}
void CVrocBus::Teardown()
{
    RemoveAllDevices();
}
void CVrocBus::AddDevice(class CVrocDevice* dev)
{
    InsertTailList(&this->DevListHead, &dev->List);
//    dev->ParentBus = this;
}
void CVrocBus::RemoveAllDevices()
{
    while(!IsListEmpty(&DevListHead))
    {
        PLIST_ENTRY entry = RemoveHeadList(&DevListHead);
        CVrocDevice *dev = CONTAINING_RECORD(entry, CVrocDevice, List);
        dev->Teardown();
        MemDelete(dev, TAG_VROC_DEVICE);
    }
}
void CVrocBus::UpdateBridgeInfo(UINT64 msi_addr)
{
//todo : update prefetch mem range and i/o range....
    //NonPrefetchable MemBase
    PHYSICAL_ADDRESS addr = { 0 };
    PPCI_MSI_CAP msi = NULL;
    PPCI_MSIX_CAP msix = NULL;
    //Each bus has 2 MB region in Bar0SpaceForDevAndBridge.
    //It should be split into 2 parts:
    //  1st 1MB is used for all child NVMe devices.
    //  2nd 1MB is used for Bridge's BAR0.
    addr = MmGetPhysicalAddress(this->Bar0SpaceForDevAndBridge);

    //np stands for "NonPrefetchable"
    //pf stands for "Prefetchable"
    UINT16 base = (UINT16)((addr.LowPart >> 16) & 0xFFF0ULL); //low 4 bits are readonly, ignore them. 
    UINT16 limit = (UINT16)((addr.LowPart >> 16) & 0xFFF0ULL); //low 4 bits are readonly, ignore them. 
    
    this->BridgeCfg->u.type1.MemoryBase = base;
    this->BridgeCfg->u.type1.MemoryLimit = limit;
    //this->BridgeCfg->u.type1.PrefetchBase = base;
    //this->BridgeCfg->u.type1.PrefetchLimit = limit;
    //this->BridgeCfg->u.type1.PrefetchBaseUpper32 = window_start.HighPart;
    //this->BridgeCfg->u.type1.PrefetchLimitUpper32 = window_end.HighPart;

    addr = MmGetPhysicalAddress(this->Bar0SpaceForDevAndBridge + MB_SIZE);
    this->BridgeCfg->u.type1.BaseAddresses[0] = 
                        (UINT32) (addr.QuadPart & 0xFFFFFFFFULL);
    this->BridgeCfg->u.type1.BaseAddresses[1] = 
                        (UINT32) (addr.QuadPart >> 32);

    ParseMsiCaps(BridgeCfg, msi, msix);
    //enable MSI of bridge to make NVMe device's MSIX can passthru.
    if(NULL != msi)
    {
        PHYSICAL_ADDRESS temp;
        temp.QuadPart = msi_addr;
        msi->MA_LOW = temp.LowPart;
        msi->MA_UP = temp.HighPart;
        msi->MC.MSIE = TRUE;
    }
}
#pragma endregion
#pragma region ======== CVrocDevice ========
void CVrocDevice::Setup(PVOID devext, CVrocBus *bus, UCHAR dev_id, PPCI_COMMON_CONFIG cfg, PUCHAR bar0)
{
    Guard = 0x23939889;
    InitializeListHead(&this->List);
    DevCfg = cfg;
    DevId = dev_id;
    Bar0VA = bar0;
    Bar0Len = VROC_NVME_BAR0_SIZE;
    ParentBus = bus;

    if(NULL != bar0)
    {
        //update BAR0 in VROC NVMe device's PCI Config Space.
        PHYSICAL_ADDRESS addr = {0};
        addr.QuadPart = ~(0ULL);
        cfg->u.type0.BaseAddresses[0] = addr.LowPart;
        cfg->u.type0.BaseAddresses[1] = addr.HighPart;

        addr = MmGetPhysicalAddress(bar0); 
        cfg->u.type0.BaseAddresses[0] = addr.LowPart;
        cfg->u.type0.BaseAddresses[1] = addr.HighPart;
    }

//    NvmeDev = (CNvmeDevice*) MemAlloc(NonPagedPool, sizeof(CNvmeDevice), TAG_VROC_NVME);
    NvmeDev = MemAllocEx<CNvmeDevice>(NonPagedPool, TAG_VROC_NVME);
    NvmeDev->Setup(devext, DevCfg, Bar0VA);
}
void CVrocDevice::Teardown()
{
    DeleteNvmeDevice();
}
void CVrocDevice::DeleteNvmeDevice()
{
    if(NULL != NvmeDev)
    {
        NvmeDev->Teardown();
        MemDelete(NvmeDev, TAG_VROC_NVME);
        NvmeDev = NULL;
    }
}
#pragma endregion

#pragma region ======== VROC_DEVEXT ========
NTSTATUS _VROC_DEVEXT::Setup(PPORT_CONFIGURATION_INFORMATION portcfg)
{
    PortCfg = portcfg;
    Guard = 0x23939889;
    InitializeListHead(&this->BusListHead);
    GetRaidCtrlPciCfg();
    MapRaidCtrlBar0(*portcfg->AccessRanges, portcfg->NumberOfAccessRanges);
    EnumVrocBuses();
    return STATUS_SUCCESS;
}
NTSTATUS _VROC_DEVEXT::GetRaidCtrlPciCfg()
{
    ULONG size = sizeof(CopiedPciCfg);
    ULONG status = StorPortGetBusData(this, 
                        PortCfg->AdapterInterfaceType, 
                        PortCfg->SystemIoBusNumber, 
                        PortCfg->SlotNumber, 
                        &CopiedPciCfg, size);

    if(2 == status || status != size)
        return STATUS_UNSUCCESSFUL;

    //Virtual PCI-bus CAP is following PCI_COMMON_HEADER.
    //It is first CAP in PCI_COMMON_HEADER::DeviceSpecific.
    //All regular CAPs will be after this CAP.
    PVROC_RAID_VIRTUAL_BUS_CAP cap = 
            (PVROC_RAID_VIRTUAL_BUS_CAP)CopiedPciCfg.DeviceSpecific;
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

    MsixCount = GetMsixTableSize(&CopiedPciCfg);
    return STATUS_SUCCESS;
}
void _VROC_DEVEXT::EnableNvmeDevMsix()
{
    for (ULONG i = 0; i < MAX_CHILD_VROC_DEV; i++)
    {
        CNvmeDevice* nvme = NvmeDev[i];
        if (NULL == nvme)
            continue;
        nvme->EnableMsix();
    }
}
void _VROC_DEVEXT::MapRaidCtrlBar0(ACCESS_RANGE* ranges, ULONG count)
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
        BusBar0Space = addr;
        BusBar0SpaceSize = range->RangeLength;
        BusBar0SpacePA = range->RangeStart;
    }

    range = &AccessRanges[2];
    in_iospace = !range->RangeInMemory;
    addr = (PUCHAR)StorPortGetDeviceBase(
        this, type,
        PortCfg->SystemIoBusNumber, range->RangeStart,
        range->RangeLength, in_iospace);
    if (NULL != addr)
    {
        RaidMsixTable = addr;
        RaidMsixTableSize = range->RangeLength;
        RaidMsixTablePA = range->RangeStart;
    }
}
void _VROC_DEVEXT::EnumVrocBuses()
{
    //VROC Controller CfgSpace in Bar0 has 32MB.
    //It indicates 32 buses :
    //Bus0 has only PCI Bridges, and all NVMe are located on Bus1~Bus31.
    //Each Bus has NVMe devices and connected to VMD RaidController via 1 bridge.
    //So we should treat as a real PCI bridged bus => It is similar as extra PCI segment.

    //PCI bus has max 32 devices on 1 bus....
    //Enum bridges first.
    //Bus 0 stores Pci Bridges, other Bus stores VROC NVMe devices.
    PUCHAR bus0_space = GetBusCfgSpace(RaidPcieCfgSpace, RaidPcieCfgSpaceSize, 0);
    for (UCHAR bridge_idx = 1; bridge_idx < MAX_VROC_VIRTUALBUS; bridge_idx++)
    {
    //Each bridge map to one bus. bridge id == bus id.
    //there is no bridge 0.
        PPCI_COMMON_CONFIG bridge = (PPCI_COMMON_CONFIG)GetDevCfgSpace(bus0_space, bridge_idx);
        if (!IsValidVendorID(bridge) || !IsPciBridge(bridge->HeaderType))
            continue;

        //found a bridge device, set it up and enum its child bus

        //[Don't forget to set MemBase/MemLimit of this bridge.]
        //System can't access resources of all devices behind this bridge, 
        // if you didn't setup MemBase/MemLimit.
        UCHAR bus_idx = bridge->u.type1.SecondaryBus - bridge->u.type1.PrimaryBus;
        CVrocBus* bus = MemAllocEx<CVrocBus>(NonPagedPool, TAG_VROC_BUS);
        PUCHAR bus_space = GetBusCfgSpace(RaidPcieCfgSpace, RaidPcieCfgSpaceSize, bus_idx);
        bus->Setup(PrimaryBus, bus_idx, bridge, 
                    bus_space, GetNvmeBar0SpaceForBus(bus_idx));

        //Bridge Memory Window SHOULD BE SET BETORE Bar0 been set for All Device on this bus.
        //The Bridge Memory Windows is 1MB at least. Bar0 of 32 NVMe devices are 512K.
        //It's smaller than 1MB, just set start and end addr to same value.

        //tricky way:
        // using 1st entry of RaidController's MSIX to get MsgAddr fields.
        MSIX_TABLE_ENTRY entry = {0};
        entry.MsgAddr.AsULONG64 = ((PMSIX_TABLE_ENTRY)this->RaidMsixTable)->MsgAddr.AsULONG64;
        entry.MsgAddr.DestinationID = 0;
        bus->UpdateBridgeInfo(entry.MsgAddr.AsULONG64);
        EnumVrocDevsOnBus(bus);
        InsertTailList(&BusListHead, &bus->List);
    }
}
void _VROC_DEVEXT::EnumVrocDevsOnBus(CVrocBus *bus)
{
    //enumerate all devices on this child bus.
    for (UCHAR dev_id = 0; dev_id < PCI_MAX_DEVICES; dev_id++)
    {
        // currently only enumerate 1 device on bus.
        PPCI_COMMON_CONFIG dev_space = (PPCI_COMMON_CONFIG)GetDevCfgSpace(bus->BusSpace, dev_id);
        if (!IsValidVendorID(dev_space))
            continue;
        
//        CVrocDevice *dev = new (NonPagedPool, TAG_VROC_DEVICE) CVrocDevice();
        CVrocDevice* dev = MemAllocEx<CVrocDevice>(NonPagedPool, TAG_VROC_DEVICE);
        dev->Setup(this, bus, dev_id, dev_space, bus->GetBar0SpaceForDevice(dev_id));
        bus->AddDevice(dev);
        NvmeDev[NvmeDevCount++] = dev->NvmeDev;
        //Todo: currently only enum 1 device on bus.
        //      should do it to enum all 32 devices...
        break;
    }
}
void _VROC_DEVEXT::Teardown()
{
    RtlZeroMemory(NvmeDev, sizeof(NvmeDev));
    NvmeDevCount = 0;

    while(!IsListEmpty(&BusListHead))
    {
        PLIST_ENTRY entry = RemoveHeadList(&BusListHead);
        CVrocBus *bus = CONTAINING_RECORD(entry, CVrocBus, List);
        bus->Teardown();
        MemDelete(bus, TAG_VROC_BUS);
    }
    UncachedExt = NULL;
}
void _VROC_DEVEXT::ShutdownAllVrocNvmeControllers()
{
    for (ULONG i = 0; i < MAX_CHILD_VROC_DEV; i++)
    {
        if (NULL == NvmeDev[i])
            continue;

        NvmeDev[i]->ShutdownController();
    }
}
void _VROC_DEVEXT::DisableAllVrocNvmeControllers()
{
    for (ULONG i = 0; i < MAX_CHILD_VROC_DEV; i++)
    {
        if (NULL == NvmeDev[i])
            continue;

        NvmeDev[i]->DisableController();
    }
}
NTSTATUS _VROC_DEVEXT::InitAllVrocNvme()
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    for (ULONG i = 0; i < MAX_CHILD_VROC_DEV; i++)
    {
        if (NULL == NvmeDev[i])
            continue;

        CNvmeDevice* ptr = NvmeDev[i];

        status = ptr->InitController();
        if (!NT_SUCCESS(status))
            return status;

        status = ptr->IdentifyController(NULL, &ptr->CtrlIdent, true);
        if (!NT_SUCCESS(status))
            return status;
    }

    return STATUS_SUCCESS;
}
NTSTATUS _VROC_DEVEXT::PassiveInitAllVrocNvme()
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    //bridge should be setup before load msix table.
    //all child device's msix will be calculate?
    for (ULONG i = 0; i < MAX_CHILD_VROC_DEV; i++)
    {
        CNvmeDevice* nvme = NvmeDev[i];
        if (NULL == nvme)
            continue;

        status = nvme->InitNvmeStage1();
        if (!NT_SUCCESS(status))
            return status;

        status = nvme->InitNvmeStage2();
        if (!NT_SUCCESS(status))
            return status;

        status = nvme->CreateIoQueues();
        if (!NT_SUCCESS(status))
            return status;

        status = nvme->RegisterIoQueues(NULL);
        if (!NT_SUCCESS(status))
            return status;
    }

    return STATUS_SUCCESS;
}
void _VROC_DEVEXT::UpdateVrocNvmeDevInfo()
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
