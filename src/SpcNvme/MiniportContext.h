#pragma once

class CNvmeDevice;
typedef struct _SPCNVME_DEVEXT
{
    //todo: runtime allocate a memory here... no hardcode....
    ACCESS_RANGE PciSpace[4];

    NVME_STATE State;
    CNvmeDevice *NvmeDev;
    CNvmeQueuePair *AdminQueue;
    CNvmeQueuePair *IoQueue[MAX_IO_QUEUE_COUNT];

    BOOLEAN IsReady;
    STOR_DPC NvmeDPC;      //DPC for MSIX interrupt which triggerred from NVMe Device
    NVME_IDENTIFY_CONTROLLER_DATA IdentData;

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
