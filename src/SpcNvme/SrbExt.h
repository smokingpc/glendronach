#pragma once

class CNvmeDevice;

typedef struct _SPCNVME_SRBEXT
{
    static _SPCNVME_SRBEXT *GetSrbExt(PVOID devext, PSTORAGE_REQUEST_BLOCK srb);

    CNvmeDevice *DevExt;
    PSTORAGE_REQUEST_BLOCK Srb;
    UCHAR SrbStatus;        //returned SrbStatus for SyncCall of Admin cmd (e.g. IndeitfyController) 
    BOOLEAN InitOK;
    BOOLEAN FreePrp2List;
    NVME_COMMAND NvmeCmd;
    NVME_COMPLETION_ENTRY NvmeCpl;
    PVOID Prp2VA;
    PHYSICAL_ADDRESS Prp2PA;

    void Init(PVOID devext, STORAGE_REQUEST_BLOCK *srb);
    void SetStatus(UCHAR status);
    void CompleteSrbWithStatus(UCHAR status);
    ULONG FuncCode();         //SRB Function Code
    ULONG ScsiQTag();
    PCDB Cdb();
    UCHAR CdbLen();
    UCHAR PathID();           //SCSI Path (bus) ID
    UCHAR TargetID();         //SCSI Device ID
    UCHAR Lun();              //SCSI Logical UNit ID
    PVOID DataBuf();
    ULONG DataBufLen();
    void SetTransferLength(ULONG length);
}SPCNVME_SRBEXT, * PSPCNVME_SRBEXT;

UCHAR ToSrbStatus(NVME_COMMAND_STATUS& status);
UCHAR NvmeGenericToSrbStatus(NVME_COMMAND_STATUS status);
UCHAR NvmeCmdSpecificToSrbStatus(NVME_COMMAND_STATUS status);
UCHAR NvmeMediaErrorToSrbStatus(NVME_COMMAND_STATUS status);

