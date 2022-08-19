#pragma once
class CSpcNvmeDevice {
public:
    CSpcNvmeDevice();
    //CSpcNvmeDevice();
    ~CSpcNvmeDevice();

    void Setup();
    void Teardown();

private:
    PPORT_CONFIGURATION_INFORMATION     PciCfg = NULL;
    PNVME_CONTROLLER_REGISTERS          CtrlReg = NULL;
    //NVME_IDENTIFY_CONTROLLER_DATA       DevIdent = {0};
    NVME_CONTROLLER_CAPABILITIES        DevCap = { 0 };

    ULONG   BusNumber = NVME_INVALID_ID;
    ULONG   SlotNumber = NVME_INVALID_ID;
    
    STOR_DPC    AdmQCplDpc;
    NVME_VERSION    NvmeVer = {0};

    CNvmeQueuePair  AdminQueue;
    ULONG IoQueueCount = 0;
    ULONG MaxIoQueueCount = 0;      //calculate by device returned config
    CNvmeQueuePair  *IoQueue = NULL;

    void Init();
};