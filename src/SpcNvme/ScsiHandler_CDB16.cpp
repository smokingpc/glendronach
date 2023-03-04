#include "pch.h"

UCHAR Scsi_Read16(PSPCNVME_SRBEXT srbext)
{
    UNREFERENCED_PARAMETER(srbext);
    return SRB_STATUS_INVALID_REQUEST;
//    return ReadWriteRamdisk(srbext, FALSE);
}

UCHAR Scsi_Write16(PSPCNVME_SRBEXT srbext)
{
    UNREFERENCED_PARAMETER(srbext);
    return SRB_STATUS_INVALID_REQUEST;
//    return ReadWriteRamdisk(srbext, TRUE);
}

UCHAR Scsi_Verify16(PSPCNVME_SRBEXT srbext)
{
    UNREFERENCED_PARAMETER(srbext);
    return SRB_STATUS_INVALID_REQUEST;
    //todo: complete this handler for FULL support of verify
    //UCHAR srb_status = SRB_STATUS_ERROR;
    //CRamdisk* disk = srbext->DevExt->RamDisk;
    //PCDB cdb = srbext->Cdb;
    //INT64 lba_start = 0;    //in Blocks, not bytes
    //
    //REVERSE_BYTES_8(&lba_start, cdb->CDB16.LogicalBlock);

    //UINT32 verify_len = 0;    //in Blocks, not bytes
    //REVERSE_BYTES_4(&verify_len, &cdb->CDB16.TransferLength);

    //if (FALSE == disk->IsExceedLbaRange(lba_start, verify_len))
    //    srb_status = SRB_STATUS_SUCCESS;

    //SrbSetDataTransferLength(srbext->Srb, 0);
    //return srb_status;
}

UCHAR Scsi_ReadCapacity16(PSPCNVME_SRBEXT srbext)
{
    UNREFERENCED_PARAMETER(srbext);
    return SRB_STATUS_INVALID_REQUEST;
    //UCHAR srb_status = SRB_STATUS_SUCCESS;
    //ULONG ret_size = sizeof(READ_CAPACITY16_DATA);
    //PREAD_CAPACITY16_DATA readcap = (PREAD_CAPACITY16_DATA)srbext->DataBuffer;
    //RtlZeroMemory(readcap, srbext->DataBufLen);

    //if(srbext->DataBufLen >= sizeof(READ_CAPACITY16_DATA))
    //{
    //    CRamdisk* disk = srbext->DevExt->RamDisk;
    //    ULONG block_size = disk->GetSizeOfBlock();
    //    INT64 last_lba = disk->GetMaxLBA();

    //    REVERSE_BYTES_QUAD(&readcap->LogicalBlockAddress, &last_lba);
    //    REVERSE_BYTES(&readcap->BytesPerBlock, &block_size);
    //    readcap->ProtectionEnable = FALSE;
    //    //todo: fill other fields in READ_CAPACITY16_DATA
    //    //e.g.      UCHAR ProtectionEnable : 1;
    //    //          UCHAR ProtectionType : 3;
    //}
    //else
    //{
    //    srb_status = SRB_STATUS_DATA_OVERRUN;
    //}

    //SrbSetDataTransferLength(srbext->Srb, ret_size);
    //return srb_status;
}
