#pragma once

typedef struct _SPCNVME_CONFIG {
    const ULONG DEFAULT_TX_SIZE = 1048576 * 2;
    const ULONG DEFAULT_TX_PAGES = DEFAULT_TX_SIZE / PAGE_SIZE; //PRP1 + PRP2 MAX PAGES
    const UCHAR SUPPORT_NAMESPACES = 1;
    const UCHAR MAX_TARGETS = 1;
    const ULONG MAX_IO_PER_LU = 256;

    ULONG MaxTxSize = DEFAULT_TX_SIZE;
    ULONG MaxTxPages = DEFAULT_TX_PAGES;
    UCHAR MaxNamespaces = SUPPORT_NAMESPACES;
    UCHAR MaxTargets = MAX_TARGETS;
    ULONG MaxIoPerLU = MAX_IO_PER_LU;
    INTERFACE_TYPE InterfaceType = PCIBus;
    
    //ISR and DPC for Queues Completion
    PHW_MESSAGE_SIGNALED_INTERRUPT_ROUTINE Isr = NULL;
    PHW_DPC_ROUTINE Dpc = NULL;
}SPCNVME_CONFIG, *PSPCNVME_CONFIG;

class CSpcNvmeDevice {
public:
    static CSpcNvmeDevice *Create(PVOID devext, PSPCNVME_CONFIG cfg);
    static void Delete(CSpcNvmeDevice *ptr);
public:
    CSpcNvmeDevice();
    CSpcNvmeDevice(PVOID devext, PSPCNVME_CONFIG cfg);
    ~CSpcNvmeDevice();

    void Setup(PVOID devext, PSPCNVME_CONFIG cfg);
    bool BindHBA(PPORT_CONFIGURATION_INFORMATION pci, HW_MESSAGE_SIGNALED_INTERRUPT_ROUTINE isr);
    void Teardown();

    void CompleteNVMeCmd();
    //void ReloadCtrlRegs();

private:
    PNVME_CONTROLLER_REGISTERS          CtrlReg = NULL;
    NVME_IDENTIFY_CONTROLLER_DATA       IdentData = { 0 };

    ULONG BusNumber = NVME_INVALID_ID;
    ULONG SlotNumber = NVME_INVALID_ID;
    
    STOR_DPC    CplDpc;
    PVOID       DevExt = NULL;      //StorPort Device Extension 
    ULONG IoQueueCount = 0;
    ULONG MaxIoQueueCount = 0;      //calculate by device returned config
    
    CNvmeQueuePair  *AdminQueue = NULL;
    CNvmeQueuePair  *IoQueue[MAX_IO_QUEUE_COUNT] = {NULL};

    ULONG MaxTxSize = 0;
    ULONG MaxTxPages = 0;
    UCHAR MaxNamespaces = 0;
    UCHAR MaxTargets = 0;
    ULONG MaxIoPerLU = 0;
    INTERFACE_TYPE InterfaceType = PCIBus;

    bool IsReady = false;
    
    bool MapControllerRegisters(PACCESS_RANGE range);

    bool CreateAdminQueuePair();
    bool RegisterAdminQueuePair();
    
    bool CreateIoQueuePairs();
    bool RegisterIoQueuePairs();
    bool RegisterIoQueuePair(ULONG index);
};

