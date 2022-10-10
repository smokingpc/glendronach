#include "pch.h"

#pragma region ======== CSpcNvmeDevice Factory Methods ======== 
CSpcNvmeDevice *CSpcNvmeDevice::Create(PVOID devext, PSPCNVME_CONFIG cfg)
{
    CSpcNvmeDevice *ret = new CSpcNvmeDevice(devext, cfg);

    return ret;
}
void CSpcNvmeDevice::Delete(CSpcNvmeDevice* ptr)
{
    ptr->Teardown();
    delete ptr;
}
#pragma endregion

#pragma region ======== class CSpcNvmeDevice ======== 

CSpcNvmeDevice::CSpcNvmeDevice(){}
CSpcNvmeDevice::CSpcNvmeDevice(PVOID devext, PSPCNVME_CONFIG cfg) 
    : CSpcNvmeDevice()
{
    Setup(devext, cfg);
}
CSpcNvmeDevice::~CSpcNvmeDevice()
{
    Teardown();
}

void CSpcNvmeDevice::Setup(PVOID devext, PSPCNVME_CONFIG cfg)
{
    this->DevExt = devext;
    RtlCopyMemory(&this->Cfg, cfg, sizeof(SPCNVME_CONFIG));

    PCI_COMMON_HEADER header = { 0 };
    if (GetPciBusData(header))
    {
        this->IsReady = MapControllerRegisters(header);
    }
}
void CSpcNvmeDevice::Teardown()
{
    if(!this->IsReady)
        return ;

    this->IsReady = false;
}

void CSpcNvmeDevice::UpdateIdentifyData(PNVME_IDENTIFY_CONTROLLER_DATA data)
{
    RtlCopyMemory(&this->IdentData, data, sizeof(NVME_IDENTIFY_CONTROLLER_DATA));
}

void CSpcNvmeDevice::SetMaxIoQueueCount(ULONG max)
{
    this->MaxIoQueueCount = max;
}

bool CSpcNvmeDevice::MapControllerRegisters(PCI_COMMON_HEADER &header)
{
    if(NULL == this->DevExt)
        return false;

    BOOLEAN in_iospace = FALSE;

    STOR_PHYSICAL_ADDRESS bar0 = { 0 };
    this->CtrlReg = NULL;
    //todo: use correct structure to parse and convert this physical address
    bar0.LowPart = (header.u.type0.BaseAddresses[0] & 0xFFFFC000);
    bar0.HighPart = header.u.type0.BaseAddresses[1];

    PACCESS_RANGE range = this->Cfg.AccessRanges;
    for (ULONG i = 0; i < this->Cfg.AccessRangeCount; i++)
    {
        if(true == PA_Utils::IsAddrEqual(range->RangeStart, bar0))
        {
            in_iospace = !range->RangeInMemory;
            this->CtrlReg = (PNVME_CONTROLLER_REGISTERS)StorPortGetDeviceBase(
                                    this->DevExt, PCIBus, 
                                    this->Cfg.BusNumber, bar0, 
                                    range->RangeLength, in_iospace);
            if (NULL != this->CtrlReg)
                return true;
        }
    }

    return false;
}
bool CSpcNvmeDevice::GetPciBusData(PCI_COMMON_HEADER& header)
{
    ULONG size = sizeof(PCI_COMMON_HEADER);
    ULONG status = StorPortGetBusData(this->DevExt, this->Cfg.InterfaceType,
        this->Cfg.BusNumber, this->Cfg.SlotNumber, &header, size);

    if (2 == status || status != size)
        return false;

    this->VendorID = header.VendorID;
    this->DeviceID = header.DeviceID;
    return true;
}

#pragma endregion

