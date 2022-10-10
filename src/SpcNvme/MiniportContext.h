#pragma once

typedef struct _SPCNVME_DEVEXT
{
    PNVME_CONTROLLER_REGISTERS CtrlReg;
    NVME_VERSION CurrentVer;

    NVME_STATE State;
    CSpcNvmeDevice *NvmeDev[MAX_LOGIC_UNIT];

    STOR_DPC NvmeDPC;      //DPC for MSIX interrupt which triggerred from NVMe Device

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
