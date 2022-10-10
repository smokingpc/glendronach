#pragma once

typedef struct _SPCNVME_CONFIG {
    const ULONG DEFAULT_TX_PAGES = 512; //PRP1 + PRP2 MAX PAGES
    const ULONG DEFAULT_TX_SIZE = PAGE_SIZE * DEFAULT_TX_PAGES;
    const UCHAR SUPPORT_NAMESPACES = 1;
    const UCHAR MAX_TARGETS = 1;
    const UCHAR MAX_LU = 1;
    const ULONG MAX_IO_PER_LU = 1024;

    ULONG MaxTxSize = DEFAULT_TX_SIZE;
    ULONG MaxTxPages = DEFAULT_TX_PAGES;
    UCHAR MaxNamespaces = SUPPORT_NAMESPACES;
    UCHAR MaxTargets = MAX_TARGETS;
    UCHAR MaxLu = MAX_LU;
    ULONG MaxIoPerLU = MAX_IO_PER_LU;
    ULONG MaxTotalIo = MAX_IO_PER_LU * MAX_LU;
    INTERFACE_TYPE InterfaceType = PCIBus;
    
    ULONG BusNumber = NVME_INVALID_ID;
    ULONG SlotNumber = NVME_INVALID_ID;
    ACCESS_RANGE AccessRanges[ACCESS_RANGE_COUNT] = {0};         //AccessRange from miniport HwFindAdapter.
    ULONG AccessRangeCount = 0;
    bool SetAccessRanges(ACCESS_RANGE buffer[], ULONG count)
    {
        if(count > ACCESS_RANGE_COUNT)
            return false;

        AccessRangeCount = count;
        RtlCopyMemory(AccessRanges, buffer, sizeof(ACCESS_RANGE) * count);

        return true;
    }
}SPCNVME_CONFIG, *PSPCNVME_CONFIG;

class CSpcNvmeDevice {
public:
    static CSpcNvmeDevice *Create(PVOID devext, PSPCNVME_CONFIG cfg);
    static void Delete(CSpcNvmeDevice *ptr);
    //static PVOID MapRegisterAddress(PVOID devext, ULONG slot, PACCESS_RANGE range);

public:
    CSpcNvmeDevice();
    CSpcNvmeDevice(PVOID devext, PSPCNVME_CONFIG cfg);
    ~CSpcNvmeDevice();

    void Setup(PVOID devext, PSPCNVME_CONFIG cfg);
    void Teardown();
    //inline void BindCtrlReg(PNVME_CONTROLLER_REGISTERS ctrlreg)
    //{this->CtrlReg = ctrlreg;}

    void UpdateIdentifyData(PNVME_IDENTIFY_CONTROLLER_DATA data);
    void SetMaxIoQueueCount(ULONG max);

private:
    PNVME_CONTROLLER_REGISTERS          CtrlReg = NULL;
    NVME_IDENTIFY_CONTROLLER_DATA       IdentData = { 0 };

    ULONG BusNumber = NVME_INVALID_ID;
    ULONG SlotNumber = NVME_INVALID_ID;
    
    PVOID DevExt = NULL;      //StorPort Device Extension 
    ULONG IoQueueCount = 0;
    ULONG MaxIoQueueCount = 0;      //calculate by device returned config
    
    CNvmeQueuePair  *AdminQueue = NULL;
    CNvmeQueuePair  *IoQueue[MAX_IO_QUEUE_COUNT] = {NULL};

    SPCNVME_CONFIG  Cfg;
    USHORT  VendorID;
    USHORT  DeviceID;
    bool IsReady = false;
    
    //bool MapControllerRegisters(PACCESS_RANGE range);
    bool MapControllerRegisters(PCI_COMMON_HEADER& header);
    bool GetPciBusData(PCI_COMMON_HEADER& header);

    bool CreateAdminQueuePair();
    bool RegisterAdminQueuePair();
    
    bool CreateIoQueuePairs();
    bool RegisterIoQueuePairs();
    bool RegisterIoQueuePair(ULONG index);
};

