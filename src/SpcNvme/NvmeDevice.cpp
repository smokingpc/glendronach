#include "pch.h"

#pragma region ======== class CSpcNvmeDevice ======== 

CSpcNvmeDevice::CSpcNvmeDevice(){}
CSpcNvmeDevice::CSpcNvmeDevice(PVOID devext, PSPCNVME_CONFIG cfg) 
    : CSpcNvmeDevice()
{
    Setup(devext, cfg);
}
CSpcNvmeDevice::~CSpcNvmeDevice(){}

bool CSpcNvmeDevice::BindHBA(PPORT_CONFIGURATION_INFORMATION pci, HW_MESSAGE_SIGNALED_INTERRUPT_ROUTINE isr)
{
    if(NULL == this->DevExt)
        return false;

    this->BusNumber = pci->SystemIoBusNumber;

    //All untouched fields in PPORT_CONFIGURATION_INFORMATION are marked as 
    //"miniport driver should not modify it" in MSDN. So skip them...
    pci->MaximumTransferLength = this->MaxTxSize;
    pci->NumberOfPhysicalBreaks = this->MaxTxPages + 1;    //default value == 0x11
    pci->AlignmentMask = FILE_LONG_ALIGNMENT;   //Address Alignment of SRB request's buffer
    pci->MiniportDumpData = NULL;
    pci->NumberOfBuses = 1;
    pci->InitiatorBusId[0] = 1;
    pci->ScatterGather = TRUE;
    
    //todo: enable this and implement cache flush
    pci->CachesData = FALSE;
    pci->MapBuffers = STOR_MAP_ALL_BUFFERS_INCLUDING_READ_WRITE;
    pci->MaximumNumberOfTargets = this->MaxTargets;
    pci->SrbType = SRB_TYPE_STORAGE_REQUEST_BLOCK;
    pci->AddressType = STORAGE_ADDRESS_TYPE_BTL8;
    //pci->SpecificLuExtensionSize  ??
    //Caller should set SrbExtensionSize. this object only handle common fields.
    //pPCI->SrbExtensionSize = sizeof(NVME_SRB_EXTENSION);
    if (pci->Dma64BitAddresses == SCSI_DMA64_SYSTEM_SUPPORTED)
        pci->Dma64BitAddresses = SCSI_DMA64_MINIPORT_SUPPORTED;

    pci->MaximumNumberOfLogicalUnits = this->MaxNamespaces;
    pci->SynchronizationModel = StorSynchronizeFullDuplex;
    pci->InterruptSynchronizationMode = InterruptSynchronizePerMessage;
    pci->HwMSInterruptRoutine = isr;

//TODO: SUPPORT dump     
    pci->DumpRegion.Length = 0;
    pci->DumpRegion.PhysicalBase.QuadPart = 0;
    pci->DumpRegion.VirtualBase = NULL;
    pci->RequestedDumpBufferSize = 0;
    pci->DumpMode = DUMP_MODE_CRASH;
    pci->MaxNumberOfIO = this->MaxIoPerLU * this->MaxNamespaces;

    if(0==pci->NumberOfAccessRanges)
        return false;
    
    PACCESS_RANGE range = &(*(pci->AccessRanges))[0];
    

    return true;
}

void CSpcNvmeDevice::Setup(PVOID devext, PSPCNVME_CONFIG cfg)
{
    this->DevExt = devext;

    this->MaxTxSize = cfg->MaxTxSize;
    this->MaxTxPages = cfg->MaxTxPages;
    this->MaxNamespaces = cfg->MaxNamespaces;
    this->MaxTargets = cfg->MaxTargets;
    this->MaxIoPerLU = cfg->MaxIoPerLU;
    this->InterfaceType = cfg->InterfaceType;
}
void CSpcNvmeDevice::Teardown(){}

//void CSpcNvmeDevice::Init()
//{
//}


bool CSpcNvmeDevice::MapControllerRegisters(PACCESS_RANGE range)
{
    this->CtrlReg = (PNVME_CONTROLLER_REGISTERS)StorPortGetDeviceBase(
        this->DevExt,
        this->InterfaceType,
        this->BusNumber,
        range->RangeStart,
        range->RangeLength,
        FALSE);

    if (NULL == this->CtrlReg)
        return false;


    return true;

}
#pragma endregion

