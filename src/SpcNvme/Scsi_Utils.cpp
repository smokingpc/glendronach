#include "pch.h"
SPC_SRBEXT_COMPLETION Complete_ScsiReadWrite;
UCHAR NvmeStatus2SrbStatus(NVME_COMMAND_STATUS* status)
{
    if(status->SCT == NVME_STATUS_TYPE_GENERIC_COMMAND && 
        status->SC == NVME_STATUS_SUCCESS_COMPLETION)
        return SRB_STATUS_SUCCESS;

    return SRB_STATUS_ERROR;
}

void Complete_ScsiReadWrite(SPCNVME_SRBEXT *srbext)
{
    UCHAR srb_status = NvmeStatus2SrbStatus(&srbext->NvmeCpl.DW3.Status);
    srbext->CleanUp();
    srbext->CompleteSrbWithStatus(srb_status);
}

UCHAR Scsi_ReadWrite(PSPCNVME_SRBEXT srbext, ULONG64 offset, ULONG len, bool is_write)
{
    //the SCSI I/O are based for BLOCKs of device, not bytes....
    UCHAR srb_status = SRB_STATUS_PENDING;
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    ULONG nsid = srbext->Lun() + 1;

    if (!srbext->DevExt->IsInValidIoRange(nsid, offset, len))
        return SRB_STATUS_ERROR;

    BuiildCmd_ReadWrite(srbext, offset, len, is_write);
    srbext->CompletionCB = Complete_ScsiReadWrite;
    status = srbext->DevExt->SubmitIoCmd(srbext, &srbext->NvmeCmd);
    if (!NT_SUCCESS(status))
        srb_status = SRB_STATUS_ERROR;
    else
        srb_status = SRB_STATUS_PENDING;

    return srb_status;
}