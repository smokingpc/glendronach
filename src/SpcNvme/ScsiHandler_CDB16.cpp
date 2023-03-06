#include "pch.h"
inline void FillReadCapacityEx(UCHAR lun, PSPCNVME_SRBEXT srbext)
{
    PREAD_CAPACITY_DATA_EX cap = (PREAD_CAPACITY_DATA_EX)srbext->DataBuf();
    ULONG block_size = 0;
    ULONG64 blocks = 0;
    srbext->DevExt->GetNamespaceBlockSize(lun+1, block_size);

    //LogicalBlockAddress is MAX LBA index, it's zero-based id.
    //**this field is (total LBA count)-1.
    srbext->DevExt->GetNamespaceTotalBlocks(lun+1, blocks);
    blocks -= 1;
    REVERSE_BYTES_4(cap->BytesPerBlock, &block_size);
    REVERSE_BYTES_8(cap->LogicalBlockAddress.QuadPart, &blocks);
}

UCHAR Scsi_Read16(PSPCNVME_SRBEXT srbext)
{
    ULONG64 offset = 0; //in blocks
    ULONG len = 0;    //in blocks
    PCDB cdb = srbext->Cdb();

    ParseReadWriteOffsetAndLen(cdb->CDB16, offset, len);
    return Scsi_ReadWrite(srbext, offset, len, false);
}

UCHAR Scsi_Write16(PSPCNVME_SRBEXT srbext)
{
    ULONG64 offset = 0; //in blocks
    ULONG len = 0;    //in blocks
    PCDB cdb = srbext->Cdb();

    ParseReadWriteOffsetAndLen(cdb->CDB16, offset, len);
    return Scsi_ReadWrite(srbext, offset, len, true);
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
    UCHAR srb_status = SRB_STATUS_SUCCESS;
    ULONG ret_size = 0;
    UCHAR lun = srbext->Lun();

    //LUN is zero based...
    if (lun >= srbext->DevExt->NamespaceCount)
    {
        srb_status = SRB_STATUS_INVALID_LUN;
        goto END;
    }
    if (!srbext->DevExt->IsWorking())
    {
        srb_status = SRB_STATUS_NO_DEVICE;
        goto END;
    }

    if (srbext->DataBufLen() >= sizeof(READ_CAPACITY_DATA_EX))
    {
        FillReadCapacityEx(lun, srbext);
        ret_size = sizeof(READ_CAPACITY_DATA_EX);
    }
    else
    {
        srb_status = SRB_STATUS_DATA_OVERRUN;
        ret_size = sizeof(READ_CAPACITY_DATA_EX);
    }
END:
    srbext->SetTransferLength(ret_size);
    return SRB_STATUS_INVALID_REQUEST;
}
