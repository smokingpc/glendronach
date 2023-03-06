#include "pch.h"

UCHAR Scsi_ReportLuns12(PSPCNVME_SRBEXT srbext)
{
//according SEAGATE SCSI reference, SCSIOP_REPORT_LUNS
//is used to query SCSI Logical Unit class address.
//each address are 8 bytes. In current windows system 
//I can't find reference data to determine LU address.
//MSDN also said it's NOT recommend to translate this SCSI 
//command for NVMe device.
//So I skip this command....
    UNREFERENCED_PARAMETER(srbext);
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR Scsi_Read12(PSPCNVME_SRBEXT srbext)
{
    ULONG64 offset = 0; //in blocks
    ULONG len = 0;    //in blocks
    PCDB cdb = srbext->Cdb();

    ParseReadWriteOffsetAndLen(cdb->CDB12, offset, len);
    return Scsi_ReadWrite(srbext, offset, len, false);
}

UCHAR Scsi_Write12(PSPCNVME_SRBEXT srbext)
{
    ULONG64 offset = 0; //in blocks
    ULONG len = 0;    //in blocks
    PCDB cdb = srbext->Cdb();

    ParseReadWriteOffsetAndLen(cdb->CDB12, offset, len);
    return Scsi_ReadWrite(srbext, offset, len, true);
}

UCHAR Scsi_Verify12(PSPCNVME_SRBEXT srbext)
{
    UNREFERENCED_PARAMETER(srbext);
    return SRB_STATUS_INVALID_REQUEST;

    ////todo: complete this handler for FULL support of verify
    //UCHAR srb_status = SRB_STATUS_ERROR;
    //CRamdisk* disk = srbext->DevExt->RamDisk;
    //PCDB cdb = srbext->Cdb;
    //UINT32 lba_start = 0;    //in Blocks, not bytes
    //REVERSE_BYTES_4(&lba_start, cdb->CDB12.LogicalBlock);

    //UINT32 verify_len = 0;    //in Blocks, not bytes
    //REVERSE_BYTES_4(&verify_len, &cdb->CDB12.TransferLength);

    //if (FALSE == disk->IsExceedLbaRange(lba_start, verify_len))
    //    srb_status = SRB_STATUS_SUCCESS;

    //SrbSetDataTransferLength(srbext->Srb, 0);
    //return srb_status;
}

