#pragma once

class CNvmeDevice;
typedef struct _SPCNVME_DEVEXT
{
    //todo: runtime allocate a memory here... no hardcode....
    ACCESS_RANGE PciSpace[4];
    ULONG CpuCount;
    NVME_STATE State;
    CNvmeDevice *NvmeDev;
    CNvmeQueuePair *AdminQueue;
    CNvmeQueuePair *IoQueue[MAX_IO_QUEUE_COUNT];
    ULONG IoQueueCount = 0;
    BOOLEAN IsReady;
    STOR_DPC NvmeDPC;      //DPC for MSIX interrupt which triggerred from NVMe Device
    NVME_IDENTIFY_CONTROLLER_DATA IdentData;
    NVME_IDENTIFY_NAMESPACE_DATA NsData[NVME_CONST::SUPPORT_NAMESPACES];
}SPCNVME_DEVEXT, * PSPCNVME_DEVEXT;

typedef struct _SPCNVME_SRBEXT
{
    PSPCNVME_DEVEXT DevExt;
    PSTORAGE_REQUEST_BLOCK Srb;
    ULONG FuncCode;         //SRB Function Code
    PCDB Cdb;
    UCHAR CdbLen;
    ULONG Lun;
    ULONG TargetID;
    PVOID DataBuf;
    ULONG DataBufLen;
}SPCNVME_SRBEXT, * PSPCNVME_SRBEXT;
