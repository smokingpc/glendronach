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
void CopyPcieCapFromPciCfg(PPCI_COMMON_CONFIG pcicfg, PPCIE_CAP ret)
{
    PPCI_CAPABILITIES_HEADER cap = NULL;
    switch(pcicfg->HeaderType)
    {
        case 0:
            cap = (PPCI_CAPABILITIES_HEADER)((PUCHAR)pcicfg + pcicfg->u.type0.CapabilitiesPtr);
            break;
        case 1:
            cap = (PPCI_CAPABILITIES_HEADER)((PUCHAR)pcicfg + pcicfg->u.type1.CapabilitiesPtr);
            break;
        case 2:
            cap = (PPCI_CAPABILITIES_HEADER)((PUCHAR)pcicfg + pcicfg->u.type2.CapabilitiesPtr);
            break;
        default:
            DbgBreakPoint();
            return;
    }
    
    while (IsValidPciCap(cap))
    {
        if (PCI_CAPABILITY_ID_PCI_EXPRESS == cap->CapabilityID)
        {
            RtlCopyMemory(ret, cap, sizeof(PCIE_CAP));
            break;
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
NTSTATUS _SPC_DEVEXT::Setup(PPORT_CONFIGURATION_INFORMATION portcfg)
{
    RtlZeroMemory(this, sizeof(_SPC_DEVEXT));
    PortCfg = portcfg;

    ULONG size = sizeof(PciCfg);
    ULONG status = StorPortGetBusData(this, portcfg->AdapterInterfaceType,
                                            portcfg->SystemIoBusNumber, 
                                            portcfg->SlotNumber, &PciCfg, 
                                            size);
    //refer to MSDN StorPortGetBusData() to check why 2==status is error.
    if (2 == status || status != size)
        return STATUS_UNSUCCESSFUL;

    MapVrocBar0(*portcfg->AccessRanges, portcfg->NumberOfAccessRanges);
    EnumNvmeDevs();
    return STATUS_SUCCESS;
}
void _SPC_DEVEXT::MapVrocBar0(ACCESS_RANGE* ranges, ULONG count)
{
    DbgBreakPoint();
    RtlCopyMemory(AccessRanges, ranges, sizeof(AccessRanges));
    AccessRangeCount = min(ACCESS_RANGE_COUNT, count);

    BOOLEAN in_iospace = FALSE;
    STOR_PHYSICAL_ADDRESS bar0 = { 0 };
    INTERFACE_TYPE type = PortCfg->AdapterInterfaceType;
    PACCESS_RANGE range = NULL;
    PUCHAR addr = NULL;

    range = &AccessRanges[0];
    bar0.LowPart = (PciCfg.u.type0.BaseAddresses[0] & BAR0_LOWPART_MASK);
    bar0.HighPart = PciCfg.u.type0.BaseAddresses[1];
    in_iospace = !range->RangeInMemory;
    addr = (PUCHAR)StorPortGetDeviceBase(
        this, type,
        PortCfg->SystemIoBusNumber, bar0,
        range->RangeLength, in_iospace);
    if (NULL != addr)
    {
        RaidPciePage = addr;
        RaidPciePageSize = range->RangeLength;
    }

    range = &AccessRanges[1];
    bar0.LowPart = (PciCfg.u.type0.BaseAddresses[2] & BAR0_LOWPART_MASK);
    bar0.HighPart = PciCfg.u.type0.BaseAddresses[3];
    in_iospace = !range->RangeInMemory;
    addr = (PUCHAR)StorPortGetDeviceBase(
        this, type,
        PortCfg->SystemIoBusNumber, bar0,
        range->RangeLength, in_iospace);
    if (NULL != addr)
    {
        RaidCtrlRegPage = addr;
        RaidCtrlRegPageSize = range->RangeLength;
    }

    range = &AccessRanges[2];
    bar0.LowPart = (PciCfg.u.type0.BaseAddresses[4] & BAR0_LOWPART_MASK);
    bar0.HighPart = PciCfg.u.type0.BaseAddresses[5];
    in_iospace = !range->RangeInMemory;
    addr = (PUCHAR)StorPortGetDeviceBase(
        this, type,
        PortCfg->SystemIoBusNumber, bar0,
        range->RangeLength, in_iospace);
    if (NULL != addr)
    {
        RaidMsixPage = addr;
        RaidMsixPageSize = range->RangeLength;
    }
}
void _SPC_DEVEXT::EnumNvmeDevs()
{
    UINT32 pci_offset = MB_SIZE;
    UINT32 ctrlreg_offset = 2* MB_SIZE;

    //each bus occupy 1 MB space in PcieInfo Page.
    //each NVME device is treated as 1 device on 1 PCI bus.
    for(ULONG i=0; i< MAX_CHILD_VROC_DEV; i++)
    {
        PPCI_COMMON_CONFIG pcidev = (PPCI_COMMON_CONFIG)(RaidPciePage + (pci_offset * i));
        if(!IsValidVendorID(pcidev))
            continue;

        PNVME_CONTROLLER_REGISTERS ctrlreg = 
            (PNVME_CONTROLLER_REGISTERS)(RaidPciePage + (ctrlreg_offset * i));
        NvmePciCfg[i] = pcidev;
        NvmeCtrlReg[i] = ctrlreg;
        CopyPcieCapFromPciCfg(pcidev, &NvmePcieCap[i]);
    }
}
void _SPC_DEVEXT::Teardown()
{
    for(ULONG i=0; i< MAX_CHILD_VROC_DEV; i++)
    {
        if(NULL == NvmeDev[i])
            continue;

        NvmeDev[i]->Teardown();
        delete NvmeDev[i];
        NvmeDev[i] = NULL;
        NvmePciCfg[i] = NULL;
        NvmeCtrlReg[i] = NULL;
    }

    UncachedExt = NULL;
}
void _SPC_DEVEXT::ShutdownAllVmdController()
{
    for (ULONG i = 0; i < MAX_CHILD_VROC_DEV; i++)
    {
        if (NULL == NvmeDev[i])
            continue;

        NvmeDev[i]->ShutdownController();
    }
}
void _SPC_DEVEXT::DisableAllVmdController()
{
    for (ULONG i = 0; i < MAX_CHILD_VROC_DEV; i++)
    {
        if (NULL == NvmeDev[i])
            continue;

        NvmeDev[i]->DisableController();
    }
}
NTSTATUS _SPC_DEVEXT::InitVmd()
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    for (ULONG i = 0; i < MAX_CHILD_VROC_DEV; i++)
    {
        if (NULL == NvmeDev[i])
            continue;

        CNvmeDevice *ptr = new CNvmeDevice();
        NvmeDev[i] = ptr;
        status = ptr->Setup(NvmePciCfg[i], NvmeCtrlReg[i]);
        if (!NT_SUCCESS(status))
        {
            DbgBreakPoint();
            return status;
        }
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
NTSTATUS _SPC_DEVEXT::PassiveInitVmd()
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
void _SPC_DEVEXT::UpdateNvmeInfoByVmd()
{
//VMD RaidController has multiple NVMe device.
//My MaxTxSize / MaxTxPages should get minimum value of them.
    for (ULONG i = 0; i < MAX_CHILD_VROC_DEV; i++)
    {
        CNvmeDevice* ptr = NvmeDev[i];
        if (NULL == ptr)
            continue;

        if(0 == MaxTxSize || MaxTxSize < ptr->MaxTxSize)
        {
            MaxTxSize = ptr->MaxTxSize;
            if (0 == MaxTxSize)
                MaxTxSize = DEFAULT_MAX_TXSIZE;
            MaxTxPages = (ULONG)(MaxTxSize / PAGE_SIZE);
        }
    }
}
