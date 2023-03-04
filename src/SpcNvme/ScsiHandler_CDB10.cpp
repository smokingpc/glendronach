#include "pch.h"

inline void FillReadCapacityEx(PSPCNVME_SRBEXT srbext)
{
    UNREFERENCED_PARAMETER(srbext);
    //PREAD_CAPACITY_DATA_EX readcap = (PREAD_CAPACITY_DATA_EX)srbext->DataBuffer;

    //CRamdisk* disk = srbext->DevExt->RamDisk;
    //ULONG block_size = disk->GetSizeOfBlock();
    //INT64 last_lba = disk->GetMaxLBA();

    //REVERSE_BYTES_QUAD(&readcap->LogicalBlockAddress.QuadPart, &last_lba);
    //REVERSE_BYTES(&readcap->BytesPerBlock, &block_size);

    //SrbSetDataTransferLength((PVOID)(srbext->Srb), sizeof(READ_CAPACITY_DATA_EX));
}
inline void FillReadCapacity(PSPCNVME_SRBEXT srbext)
{
    UNREFERENCED_PARAMETER(srbext);
    //PREAD_CAPACITY_DATA readcap = (PREAD_CAPACITY_DATA)srbext->DataBuffer;

    //CRamdisk* disk = srbext->DevExt->RamDisk;
    //ULONG block_size = disk->GetSizeOfBlock();
    //INT64 last_lba = disk->GetMaxLBA();

    //if (last_lba > 0xFFFFFFFF)
    //    last_lba = 0xFFFFFFFF;

    //REVERSE_BYTES(&readcap->LogicalBlockAddress, &last_lba);
    //REVERSE_BYTES(&readcap->BytesPerBlock, &block_size);

    //SrbSetDataTransferLength((PVOID)(srbext->Srb), sizeof(READ_CAPACITY_DATA));
}

UCHAR Scsi_Read10(PSPCNVME_SRBEXT srbext)
{
    UNREFERENCED_PARAMETER(srbext);
    return SRB_STATUS_INVALID_REQUEST;
//    return ReadWriteRamdisk(srbext, FALSE);
}
UCHAR Scsi_Write10(PSPCNVME_SRBEXT srbext)
{
    UNREFERENCED_PARAMETER(srbext);
    return SRB_STATUS_INVALID_REQUEST;
//    return ReadWriteRamdisk(srbext, TRUE);
}

UCHAR Scsi_ReadCapacity10(PSPCNVME_SRBEXT srbext)
{
    UNREFERENCED_PARAMETER(srbext);
    return SRB_STATUS_INVALID_REQUEST;

    //UCHAR srb_status = SRB_STATUS_SUCCESS;
    //ULONG ret_size = 0;
    //RtlZeroMemory(srbext->DataBuffer, srbext->DataBufLen);
    //if (srbext->DataBufLen >= sizeof(READ_CAPACITY_DATA_EX))
    //{
    //    FillReadCapacityEx(srbext);
    //    ret_size = sizeof(READ_CAPACITY_DATA_EX);
    //}
    //else if (srbext->DataBufLen >= sizeof(READ_CAPACITY_DATA)) 
    //{
    //    FillReadCapacity(srbext);
    //    ret_size = sizeof(READ_CAPACITY_DATA);
    //}
    //else 
    //{
    //    srb_status = SRB_STATUS_DATA_OVERRUN;
    //    ret_size = sizeof(READ_CAPACITY_DATA_EX);
    //}
    //SrbSetDataTransferLength(srbext->Srb, ret_size);
    ////HandleCheckCondition(srbext->Srb, srb_status, ret_size);
    //return srb_status;
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

