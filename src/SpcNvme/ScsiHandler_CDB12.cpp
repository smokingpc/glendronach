#include "pch.h"

UCHAR Scsi_ReportLuns12(PSPCNVME_SRBEXT srbext)
{
    UNREFERENCED_PARAMETER(srbext);
    return SRB_STATUS_INVALID_REQUEST;
    //struct _REPORT_LUNS {
    //    UCHAR OperationCode;    // 0xA0 - SCSIOP_REPORT_LUNS
    //    UCHAR Reserved1[5];
    //    UCHAR AllocationLength[4];
    //    UCHAR Reserved2[1];
    //    UCHAR Control;
    //} REPORT_LUNS;
    
    //UCHAR srb_status = SRB_STATUS_INVALID_REQUEST;
    ////ULONG ret_size = 0;
    //UNREFERENCED_PARAMETER(srbext);
    ////REPORT_LUNS + LUN_LIST structures

    //return srb_status;
}

UCHAR Scsi_Read12(PSPCNVME_SRBEXT srbext)
{
    UNREFERENCED_PARAMETER(srbext);
    return SRB_STATUS_INVALID_REQUEST;

//    return ReadWriteRamdisk(srbext, FALSE);
}

UCHAR Scsi_Write12(PSPCNVME_SRBEXT srbext)
{
    UNREFERENCED_PARAMETER(srbext);
    return SRB_STATUS_INVALID_REQUEST;

//    return ReadWriteRamdisk(srbext, TRUE);
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

