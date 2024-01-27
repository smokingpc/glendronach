#pragma once
#define MAX_CHILD_VROC_DEV      16

typedef struct _SPC_DEVEXT {

    PCI_COMMON_CONFIG PciCfg;
    PPORT_CONFIGURATION_INFORMATION PortCfg;
    ACCESS_RANGE AccessRanges[ACCESS_RANGE_COUNT];

    PUCHAR RaidPciePage;
    ULONG RaidPciePageSize;
    PUCHAR RaidCtrlRegPage;
    ULONG RaidCtrlRegPageSize;
    PUCHAR RaidMsixPage;
    ULONG RaidMsixPageSize;

    PPCI_COMMON_CONFIG NvmePciCfg[MAX_CHILD_VROC_DEV];
    PNVME_CONTROLLER_REGISTERS NvmeCtrlReg[MAX_CHILD_VROC_DEV];
    PCIE_CAP NvmePcieCap[MAX_CHILD_VROC_DEV];

    CNvmeDevice *NvmeDev[MAX_CHILD_VROC_DEV];
    PVOID UncachedExt;
    ULONG MaxTxSize;
    ULONG MaxTxPages;
    ULONG AccessRangeCount;

    NTSTATUS Setup(PPORT_CONFIGURATION_INFORMATION portcfg);
    void MapVrocBar0(ACCESS_RANGE *ranges, ULONG count);
    void EnumNvmeDevs();
    void Teardown();
    void ShutdownAllVmdController();
    void DisableAllVmdController();
    NTSTATUS InitVmd();
    NTSTATUS PassiveInitVmd();
    void UpdateNvmeInfoByVmd();
    inline CNvmeDevice* _SPC_DEVEXT::FindVmdDev(UCHAR target_id)
    {
        //treat VMD index as scsi target id.
        //there is only one bus of this storage.
        //each path has multiple targets.
        //each target only have 1 logical unit.
        if (target_id >= MAX_CHILD_VROC_DEV)
            return NULL;

        return NvmeDev[target_id];
    }
}SPC_DEVEXT, * PSPC_DEVEXT;

BOOLEAN RaidMsixISR(IN PVOID hbaext, IN ULONG msgid);
