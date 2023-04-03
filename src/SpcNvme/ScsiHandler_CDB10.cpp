#include "pch.h"

UCHAR Scsi_Read10(PSPCNVME_SRBEXT srbext)
{
    ULONG64 offset = 0; //in blocks
    ULONG len = 0;    //in blocks
    PCDB cdb = srbext->Cdb();

    ParseReadWriteOffsetAndLen(cdb->CDB10, offset, len);
    return Scsi_ReadWrite(srbext, offset, len, false);
}
UCHAR Scsi_Write10(PSPCNVME_SRBEXT srbext)
{
    ULONG64 offset = 0; //in blocks
    ULONG len = 0;    //in blocks
    PCDB cdb = srbext->Cdb();

    ParseReadWriteOffsetAndLen(cdb->CDB10, offset, len);
    return Scsi_ReadWrite(srbext, offset, len, true);
}

UCHAR Scsi_ReadCapacity10(PSPCNVME_SRBEXT srbext)
{
    UCHAR srb_status = SRB_STATUS_SUCCESS;
    ULONG ret_size = 0;
    PREAD_CAPACITY_DATA cap = (PREAD_CAPACITY_DATA)srbext->DataBuf();
    ULONG block_size = 0;
    ULONG64 blocks = 0;
    UCHAR lun = srbext->Lun();

    //LUN is zero based...
    if(lun >= srbext->DevExt->NamespaceCount)
    { 
        srb_status = SRB_STATUS_INVALID_LUN;
        goto END;
    }
    if(!srbext->DevExt->IsWorking())
    {
        srb_status = SRB_STATUS_NO_DEVICE;
        goto END;
    }
    
    if (srbext->DataBufLen() < sizeof(READ_CAPACITY_DATA))
    {
        srb_status = SRB_STATUS_DATA_OVERRUN;
        ret_size = sizeof(READ_CAPACITY_DATA);
        goto END;
    }
    
    //LogicalBlockAddress is MAX LBA index, it's zero-based id.
    //**this field is (total LBA count)-1.
    srbext->DevExt->GetNamespaceTotalBlocks(lun + 1, blocks);
    srbext->DevExt->GetNamespaceBlockSize(lun + 1, block_size);
    if (blocks > MAXULONG32)
    {
        srb_status = SRB_STATUS_INVALID_REQUEST;
        ret_size = 0;
        goto END;
    }
    //NO support thin-provisioning in current stage....
    //*From SBC - 3 r27:
    //    *If the RETURNED LOGICAL BLOCK ADDRESS field is set to FFFF_FFFFh,
    //    * then the application client should issue a READ CAPACITY(16)
    //    * command(see 5.16) to request that the device server transfer the
    //    * READ CAPACITY(16) parameter data to the data - in buffer.
    blocks -= 1;

    cap->LogicalBlockAddress = cap->BytesPerBlock = 0;
    REVERSE_BYTES_4(&cap->BytesPerBlock, &block_size);
    REVERSE_BYTES_4(&cap->LogicalBlockAddress, &blocks); //only reverse lower 4 bytes

    ret_size = sizeof(READ_CAPACITY_DATA);
    srb_status = SRB_STATUS_SUCCESS;

END:
    srbext->SetTransferLength(ret_size);
    return srb_status;
}
UCHAR Scsi_Verify10(PSPCNVME_SRBEXT srbext)
{
    UNREFERENCED_PARAMETER(srbext);
    return SRB_STATUS_INVALID_REQUEST;

    ////todo: complete this handler for FULL support of verify
    //UCHAR srb_status = SRB_STATUS_ERROR;
    //CRamdisk* disk = srbext->DevExt->RamDisk;
    //PCDB cdb = srbext->Cdb;
    //UINT32 lba_start = 0;    //in Blocks, not bytes
    //REVERSE_BYTES_4(&lba_start, &cdb->CDB10.LogicalBlockByte0);
    //
    //UINT16 verify_len = 0;    //in Blocks, not bytes
    //REVERSE_BYTES_2(&verify_len, &cdb->CDB10.TransferBlocksMsb);

    //if(FALSE == disk->IsExceedLbaRange(lba_start, verify_len))
    //    srb_status = SRB_STATUS_SUCCESS;

    //SrbSetDataTransferLength(srbext->Srb, 0);
    //return srb_status;
}
UCHAR Scsi_ModeSelect10(PSPCNVME_SRBEXT srbext)
{
    UNREFERENCED_PARAMETER(srbext);
    return SRB_STATUS_INVALID_REQUEST;

    //UCHAR srb_status = SRB_STATUS_INVALID_REQUEST;
    //UNREFERENCED_PARAMETER(srbext);
    //return srb_status;
}
UCHAR Scsi_ModeSense10(PSPCNVME_SRBEXT srbext)
{
    UNREFERENCED_PARAMETER(srbext);
    return SRB_STATUS_INVALID_REQUEST;

//    UCHAR srb_status = SRB_STATUS_ERROR;
//    PCDB cdb = srbext->Cdb;
//    PUCHAR buffer = (PUCHAR)srbext->DataBuffer;
//    PMODE_PARAMETER_HEADER10 header = (PMODE_PARAMETER_HEADER10)buffer;
//    ULONG buf_size = srbext->DataBufLen;
//    ULONG ret_size = 0;
//    ULONG page_size = 0;
//    ULONG mode_data_size = 0;
//
//    if (NULL == buffer || 0 == buf_size)
//        return SRB_STATUS_ERROR;
//
//    if (buf_size < sizeof(MODE_PARAMETER_HEADER10))
//    {
//        srb_status = SRB_STATUS_DATA_OVERRUN;
//        ret_size = sizeof(MODE_PARAMETER_HEADER10);
//        goto end;
//    }
//
//    FillParamHeader10(header);
//    buffer += sizeof(MODE_PARAMETER_HEADER10);
//    buf_size -= sizeof(MODE_PARAMETER_HEADER10);
//    ret_size += sizeof(MODE_PARAMETER_HEADER10);
//    REVERSE_BYTES_2(&mode_data_size, header->ModeDataLength);
//
//    // Todo: reply real mode sense data
//    switch (cdb->MODE_SENSE.PageCode)
//    {
//        case MODE_PAGE_CACHING:
//        {
//            ReplyModePageCaching(buffer, buf_size, ret_size);
//            mode_data_size += page_size;
//            srb_status = SRB_STATUS_SUCCESS;
//            break;
//        }
//        case MODE_PAGE_CONTROL:
//        {
//            ReplyModePageControl(buffer, buf_size, ret_size);
//            mode_data_size += page_size;
//            srb_status = SRB_STATUS_SUCCESS;
//            break;
//        }
//        case MODE_PAGE_FAULT_REPORTING:
//        {
//            //in HLK, it required "Information Exception Control Page".
//            //But it is renamed to MODE_PAGE_FAULT_REPORTING in Windows Storport ....
//            //refet to https://www.t10.org/ftp/t10/document.94/94-190r3.pdf
//            ReplyModePageInfoExceptionCtrl(buffer, buf_size, ret_size);
//            mode_data_size += page_size;
//            srb_status = SRB_STATUS_SUCCESS;
//            break;
//        }
//        case MODE_SENSE_RETURN_ALL:
//        {
//            if (buf_size > 0)
//            {
//                ReplyModePageCaching(buffer, buf_size, ret_size);
//                mode_data_size += page_size;
//            }
//            if (buf_size > 0)
//            {
//                ReplyModePageControl(buffer, buf_size, ret_size);
//                mode_data_size += page_size;
//            }
//            if (buf_size > 0)
//            {
//                ReplyModePageInfoExceptionCtrl(buffer, buf_size, ret_size);
//                mode_data_size += page_size;
//            }
//
//            srb_status = SRB_STATUS_SUCCESS;
//            break;
//        }
//        default:
//        {
//            page_size = 0;
//            srb_status = SRB_STATUS_SUCCESS;
//            break;
//        }
//    }
//    REVERSE_BYTES_2(header->ModeDataLength, &mode_data_size);
//
//end:
//    SrbSetDataTransferLength(srbext->Srb, ret_size);
//    return srb_status;
}

