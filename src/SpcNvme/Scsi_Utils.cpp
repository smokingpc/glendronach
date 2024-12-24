#include "pch.h"
SPC_SRBEXT_COMPLETION Complete_ScsiReadWrite;

void Complete_ScsiReadWrite(PSPC_SRBEXT srbext)
{
    srbext->CleanUp();
}
UCHAR Scsi_ReadWrite(
    PSPC_SRBEXT srbext, 
    ULONG64 offset, 
    ULONG len, 
    bool is_write)
{
    //the SCSI I/O are based for BLOCKs of device, not bytes....
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    ULONG nsid = LunToNsId(srbext->ScsiLun);
    CNvmeDevice* devext = (CNvmeDevice*)srbext->DevExt;

    if (!devext->IsFitValidIoRange(nsid, offset, len))
        return SRB_STATUS_ERROR;

    BuiildCmd_ReadWrite(srbext, offset, len, is_write);
    status = devext->SubmitIoCmd(srbext, &srbext->NvmeCmd);
    return NtStatusToSrbStatus(status);
}

bool ParseReadWriteLBA(CDB& cdb, ULONG64& offset, ULONG& len)
{
    switch (cdb.CDB6GENERIC.OperationCode)
    {
    case SCSIOP_READ6:
    case SCSIOP_WRITE6:
        ParseReadWriteLBA(cdb.CDB6READWRITE, offset, len);
        return true;
    case SCSIOP_READ:
    case SCSIOP_WRITE:
        ParseReadWriteLBA(cdb.CDB10, offset, len);
        return true;
    case SCSIOP_READ12:
    case SCSIOP_WRITE12:
        ParseReadWriteLBA(cdb.CDB12, offset, len);
        return true;
    case SCSIOP_READ16:
    case SCSIOP_WRITE16:
        ParseReadWriteLBA(cdb.CDB16, offset, len);
        return true;
    }

    return false;
}
