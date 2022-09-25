#pragma once

typedef struct _SPCNVME_DEVEXT
{
    NVME_STATE State;
    CSpcNvmeDevice *NvmeDev[MAX_LOGIC_UNIT];

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
