#include "pch.h"
SPC_SRBEXT_COMPLETION Complete_ScsiReadWrite;

void Complete_ScsiReadWrite(SPCNVME_SRBEXT *srbext)
{
    srbext->CleanUp();
}

UCHAR Scsi_ReadWrite(PSPCNVME_SRBEXT srbext, ULONG64 offset, ULONG len, bool is_write)
{
    //the SCSI I/O are based for BLOCKs of device, not bytes....
    UCHAR srb_status = SRB_STATUS_PENDING;
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    ULONG nsid = LunToNsId(srbext->ScsiLun);

    if (!srbext->NvmeDev->IsFitValidIoRange(nsid, offset, len))
        return SRB_STATUS_ERROR;

    BuiildCmd_ReadWrite(srbext, offset, len, is_write);
    //srbext->CompletionCB = NULL;
    status = srbext->NvmeDev->SubmitIoCmd(srbext, &srbext->NvmeCmd);
    if (!NT_SUCCESS(status))
    {
        if(STATUS_DEVICE_BUSY == status)
            srb_status = SRB_STATUS_BUSY;
        else
            srb_status = SRB_STATUS_ERROR;
    
    }
    else
        srb_status = SRB_STATUS_PENDING;

    return srb_status;
}