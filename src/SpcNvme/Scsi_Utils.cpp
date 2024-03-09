#include "pch.h"
SPC_SRBEXT_COMPLETION Complete_ScsiReadWrite;

void Complete_ScsiReadWrite(SPCNVME_SRBEXT *srbext)
{
    srbext->CleanUp();
}
UCHAR Scsi_ReadWrite(
    PSPCNVME_SRBEXT srbext, 
    ULONG64 offset, 
    ULONG len, 
    bool is_write)
{
    //the SCSI I/O are based for BLOCKs of device, not bytes....
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    ULONG nsid = LunToNsId(srbext->ScsiLun);

    if (!srbext->DevExt->IsFitValidIoRange(nsid, offset, len))
        return SRB_STATUS_ERROR;

    BuiildCmd_ReadWrite(srbext, offset, len, is_write);
    //srbext->CompletionCB = NULL;
    status = srbext->DevExt->SubmitIoCmd(srbext, &srbext->NvmeCmd);
    return NtStatusToSrbStatus(status);
}

bool ParseReadWriteOffsetAndLen(CDB& cdb, ULONG64& offset, ULONG& len)
{
    switch (cdb.CDB6GENERIC.OperationCode)
    {
    case SCSIOP_READ6:
    case SCSIOP_WRITE6:
        ParseReadWriteOffsetAndLen(cdb.CDB6READWRITE, offset, len);
        return true;
    case SCSIOP_READ:
    case SCSIOP_WRITE:
        ParseReadWriteOffsetAndLen(cdb.CDB10, offset, len);
        return true;
    case SCSIOP_READ12:
    case SCSIOP_WRITE12:
        ParseReadWriteOffsetAndLen(cdb.CDB12, offset, len);
        return true;
    case SCSIOP_READ16:
    case SCSIOP_WRITE16:
        ParseReadWriteOffsetAndLen(cdb.CDB16, offset, len);
        return true;
    }

    return false;
}
