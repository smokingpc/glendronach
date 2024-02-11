#pragma once

class CVrocDevice;
//Virtual Bus behind Virtual Bridge in in VROC RaidController's BAR0(child PciCfg Space).
class CVrocBus
{
public:
    LIST_ENTRY List;     //bus list behind same virtual bridge.
    LIST_ENTRY DevListHead;
    PPCI_COMMON_CONFIG BridgeCfg;       //PCI config of bridge
    
    PUCHAR BusSpace;        //PCI config space of entire Bus. it includes config space of all devices on this bus.
    UCHAR PrimaryBus;
    UCHAR MyBus;
    UCHAR MyBusIdx;       //MyBusIndex = MyBus - ParentBus. It represents bus offset in PciConfigSpace.
    PUCHAR Bar0SpaceForDevAndBridge;
    ULONG Guard;

    void Setup(UCHAR primary_bus, 
                UCHAR bus_idx, PPCI_COMMON_CONFIG bridge_cfg, 
                PUCHAR bus_space, PUCHAR dev_bar0_space);
    void Teardown();
    void AddDevice(class CVrocDevice* dev);
    void UpdateBridgeInfo(UINT64 msi_addr);
    void RemoveAllDevices();

    inline PUCHAR GetBar0SpaceForDevice(UCHAR dev_idx)
    {
        return (Bar0SpaceForDevAndBridge + (VROC_NVME_BAR0_SIZE * dev_idx));
    }
};

class CVrocDevice
{
public:
    LIST_ENTRY List;     //device list on same virtual bus.
    PPCI_COMMON_CONFIG DevCfg;
    PUCHAR Bar0VA;
    ULONG Bar0Len;
    CVrocBus *ParentBus;
    UCHAR DevId;        //Device ID in PCI Bus
    CNvmeDevice *NvmeDev;
    ULONG Guard;

    void Setup(PVOID devext, CVrocBus *bus, UCHAR dev_id, PPCI_COMMON_CONFIG cfg, PUCHAR bar0);
    void Teardown();
    void DeleteNvmeDevice();
};


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

    PCI_COMMON_CONFIG CopiedPciCfg;       //pci config space of RaidController
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

    //BusBar0Space is collection of all VROC NVMe device's 
    //BAR0 (Controller Register region) and Bridge's BAR0.
    //RaidController should calc and write back to VROC NVMe device's
    //BAR0 and update bridge's MemBase/MemLimit. Then you can see content of 
    //NVMe device's ControllerRegister region.
    PUCHAR BusBar0Space;    //BAR1 of RaidController
    PHYSICAL_ADDRESS BusBar0SpacePA;
    ULONG BusBar0SpaceSize;

    PUCHAR RaidMsixTable;    //BAR2 of RaidController
    PHYSICAL_ADDRESS RaidMsixTablePA;
    ULONG RaidMsixTableSize;

    //Each NVMe device are located BEHIND a pci bridge.
    //The bridge memory(resource) window should be setup correctly.
    //LED could be also controlled by these bridges.
    UCHAR PrimaryBus;       //for VROC segment, this RaidController's bus number...
    LIST_ENTRY BusListHead;

    CNvmeDevice *NvmeDev[MAX_CHILD_VROC_DEV];
    ULONG NvmeDevCount;

    PVOID UncachedExt;
    ULONG MaxTxSize;
    ULONG MaxTxPages;
    ULONG AccessRangeCount;
    ULONG MsixCount;
    UCHAR Pagging[PAGE_SIZE];   //make devext larger than 1 page...
    ULONG Guard;

    NTSTATUS Setup(PPORT_CONFIGURATION_INFORMATION portcfg);
    NTSTATUS GetRaidCtrlPciCfg();
    void EnableNvmeDevMsix();
    void MapRaidCtrlBar0(ACCESS_RANGE *ranges, ULONG count);
    void EnumVrocBuses();
    void EnumVrocDevsOnBus(CVrocBus *bus);
    void Teardown();
    void ShutdownAllVrocNvmeControllers();
    void DisableAllVrocNvmeControllers();
    NTSTATUS InitAllVrocNvme();
    NTSTATUS PassiveInitAllVrocNvme();
    void UpdateVrocNvmeDevInfo();
    inline PUCHAR GetNvmeBar0SpaceForBus(UCHAR bus_idx)
    {
    //Each Block has 2MB, 1st 1MB is for devices, 2nd 1MB is for bridge.
        return (BusBar0Space + (bus_idx * VROC_BUS_BAR0_REGION_SIZE));
        //(VROC_NVME_BAR0_SIZE* VROC_DEV_PER_BUS * bus_idx));
    }
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
