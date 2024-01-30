#pragma once

struct _VROC_DEVICE;

//Virtual Bus behind Virtual Bridge in in VROC RaidController's BAR0(child PciCfg Space).
typedef struct _VROC_BUS
{
    LIST_ENTRY List;     //bus list behind same virtual bridge.
    LIST_ENTRY DevListHead;
    PPCI_COMMON_CONFIG BridgeCfg;       //PCI config of bridge
    
    PUCHAR BusSpace;        //PCI config space of entire Bus. it includes config space of all devices on this bus.
    UINT16 PciSegmentIdx;      //PCI segment index. Each VROC bus is a different PCI segment.
    UCHAR PrimaryBus;
    UCHAR MyBus;
    UCHAR MyBusIdx;       //MyBusIndex = MyBus - ParentBus. It represents bus offset in PciConfigSpace.
    
    //for bridge memory windows setup.
    //np stands for "NonPrefetchable"
    //pf stands for "Prefetchable"
    PHYSICAL_ADDRESS NpMemBase;
    PHYSICAL_ADDRESS NpMemLimit;

    void Setup(UINT16 segment_idx, UCHAR primary_bus, UCHAR bus_idx, PPCI_COMMON_CONFIG bridge_cfg, PUCHAR bus_space);
    void Teardown();
    void AddDevice(struct _VROC_DEVICE* dev);
    void UpdateBridgeMemoryWindow(PHYSICAL_ADDRESS window_start, PHYSICAL_ADDRESS window_end);
    void RemoveAllDevices();
}VROC_BUS, *PVROC_BUS;

typedef struct _VROC_DEVICE
{
    LIST_ENTRY List;     //device list on same virtual bus.
    PPCI_COMMON_CONFIG DevCfg;
    PHYSICAL_ADDRESS Bar0PA;
    ULONG Bar0Len;
    BOOLEAN IsBar0InIoSpace;
    PVROC_BUS ParentBus;
    UCHAR DevId;        //Device ID in PCI Bus
    CNvmeDevice *NvmeDev;

    void Setup(PVROC_BUS bus, UCHAR dev_id, PPCI_COMMON_CONFIG cfg, PHYSICAL_ADDRESS bar0);
    void Teardown();
    void CreateNvmeDevice(PVOID devext);
    void DeleteNvmeDevice();
}VROC_DEVICE, * PVROC_DEVICE;


#pragma push(1)
typedef struct _VROC_RAID_VIRTUAL_BUS_CAP {
    ULONG UseMask : 1;
    ULONG Reserved1 : 31;
    ULONG Reserved2 : 8;
    ULONG BusMask : 3;
    ULONG Reserved : 21;
}VROC_RAID_VIRTUAL_BUS_CAP, *PVROC_RAID_VIRTUAL_BUS_CAP;
#pragma pop()

typedef struct _VROC_DEVEXT {

    PCI_COMMON_CONFIG PciCfg;       //pci config space of RaidController
    PPORT_CONFIGURATION_INFORMATION PortCfg;
    ACCESS_RANGE AccessRanges[ACCESS_RANGE_COUNT];

    //In VROC RaidController, there are 3 BARs
    //BAR0 == PCI conofig space of child NVMe devices and bridges.
    //BAR1 == NVMe Controller Register region of all child devices.
    //BAR2 == MSIX table (RAID Controller or all VROC NVMe devices??)
    //I only need BAR0 and the NVMe CtrlReg region can be mapped from child device's BAR0.
    PUCHAR RaidPcieCfgSpace;    //BAR0 of RaidController
    PHYSICAL_ADDRESS RaidPcieCfgSpacePA;
    ULONG RaidPcieCfgSpaceSize;

    //RaidNvmeCfgSpace is collection of all VROC NVMe device's 
    //BAR0 (Controller Register region).
    //RaidController should calc and write back to VROC NVMe device's
    //BAR0 and update bridge's MemBase/MemLimit. Then you can see content of 
    //NVMe device's ControllerRegister region.
    PUCHAR RaidNvmeCfgSpace;    //BAR1 of RaidController
    PHYSICAL_ADDRESS RaidNvmeCfgSpacePA;
    ULONG RaidNvmeCfgSpaceSize;

    PUCHAR RaidMsixCfgSpace;    //BAR2 of RaidController
    PHYSICAL_ADDRESS RaidMsixCfgSpacePA;
    ULONG RaidMsixCfgSpaceSize;

    //Each NVMe device are located BEHIND a pci bridge.
    //The bridge memory(resource) window should be setup correctly.
    //LED could be also controlled by these bridges.
    UCHAR PrimaryBus;       //for VROC segment, this RaidController's bus number...
    LIST_ENTRY BusListHead;

    CNvmeDevice *NvmeDev[MAX_CHILD_VROC_DEV];
    ULONG NvmeDevCount;
    //ULONG NvmeBar0IndexInRaidCtrlBAR1;
    PVOID UncachedExt;
    ULONG MaxTxSize;
    ULONG MaxTxPages;
    ULONG AccessRangeCount;

    NTSTATUS Setup(PPORT_CONFIGURATION_INFORMATION portcfg);
    NTSTATUS GetRaidCtrlPciCfg();
    void MapRaidCtrlBar0(ACCESS_RANGE *ranges, ULONG count);
    void EnumVrocBuses();
    void EnumVrocDevsOnBus(PVROC_BUS bus);

    void Teardown();
    void ShutdownAllVrocNvmeControllers();
    void DisableAllVrocNvmeControllers();
    NTSTATUS InitAllVrocNvme();
    NTSTATUS PassiveInitAllVrocNvme();
    void UpdateVrocNvmeDevInfo();
    inline CNvmeDevice* _VROC_DEVEXT::FindVrocNvmeDev(UCHAR target_id)
    {
        //treat vroc virtual bus index as scsi target id.
        //there is only one bus of this storage.
        //each path has multiple targets.
        //each target only have 1 logical unit.
        if (target_id >= MAX_CHILD_VROC_DEV)
            return NULL;

        return NvmeDev[target_id];
    }
}VROC_DEVEXT, * PVROC_DEVEXT;

BOOLEAN RaidMsixISR(IN PVOID hbaext, IN ULONG msgid);
