#include "pch.h"

UCHAR Scsi_ReadWrite(PSPCNVME_SRBEXT srbext, ULONG64 offset, ULONG len, bool is_write)
{
    //the SCSI I/O are based for BLOCKs of device, not bytes....
    UCHAR srb_status = SRB_STATUS_ERROR;
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    srb_status = BuiildCmd_Read(srbext, offset, len);
    if (SRB_STATUS_SUCCESS == srb_status)
    {
        status = srbext->DevExt->SubmitCmd(srbext, &srbext->NvmeCmd);
        if (!NT_SUCCESS(status))
            srb_status = SRB_STATUS_ERROR;
    }

    return srb_status;
}