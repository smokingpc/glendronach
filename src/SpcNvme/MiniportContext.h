#pragma once

class CNvmeDevice;
class CNvmeQueue;
typedef struct _SPCNVME_DEVEXT
{
    //todo: runtime allocate a memory here... no hardcode....
    ACCESS_RANGE PciSpace[4];
    ULONG CpuCount;
    NVME_STATE State;
    CNvmeDevice *NvmeDev;
    CNvmeQueue *AdminQueue;
    CNvmeQueue *IoQueue[MAX_IO_QUEUE_COUNT];
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
    UCHAR SrbStatus;
    BOOLEAN InitOK;
    NVME_COMMAND SrcCmd;
    ULONG FuncCode;         //SRB Function Code
    ULONG ScsiQTag;
    PCDB Cdb;
    UCHAR CdbLen;
    UCHAR PathID;           //SCSI Path (bus) ID
    UCHAR TargetID;         //SCSI Dvice ID
    UCHAR Lun;              //SCSI Logical UNit ID
    PVOID DataBuf;
    ULONG DataBufLen;
}SPCNVME_SRBEXT, * PSPCNVME_SRBEXT;
