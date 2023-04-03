#pragma once

class CNvmeDevice;
struct _SPCNVME_SRBEXT;

typedef VOID SPC_SRBEXT_COMPLETION(struct _SPCNVME_SRBEXT *srbext);
typedef SPC_SRBEXT_COMPLETION* PSPC_SRBEXT_COMPLETION;

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
    PSPC_SRBEXT_COMPLETION CompletionCB;
    //ExtBuf is used to retrieve data by cmd. e.g. LogPage Buffer in GetLogPage().
    //It should be freed in CompletionCB.
    PVOID ExtBuf;        

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
    void ResetExtBuf(PVOID new_buffer = NULL);
    PSRBEX_DATA_PNP SrbDataPnp();
}SPCNVME_SRBEXT, * PSPCNVME_SRBEXT;

UCHAR ToSrbStatus(NVME_COMMAND_STATUS& status);
UCHAR NvmeGenericToSrbStatus(NVME_COMMAND_STATUS status);
UCHAR NvmeCmdSpecificToSrbStatus(NVME_COMMAND_STATUS status);
UCHAR NvmeMediaErrorToSrbStatus(NVME_COMMAND_STATUS status);

