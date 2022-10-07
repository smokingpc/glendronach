#include "pch.h"

typedef struct _CDB6_REQUESTSENSE
{
    UCHAR OperationCode;
    UCHAR DescFormat : 1;
    UCHAR Reserved1 : 7;
    UCHAR Reserved2[2];
    UCHAR AllocSize;
    struct {
        UCHAR Link : 1;         //obsoleted by T10
        UCHAR Flag : 1;         //obsoleted by T10
        UCHAR NormalACA : 1;
        UCHAR Reserved : 3;
        UCHAR VenderSpecific : 2;
    }Control;
}CDB6_REQUESTSENSE, *PCDB6_REQUESTSENSE;


inline UCHAR HandleInquiryVPD(SPCNVME_SRBEXT srbext, ULONG& ret_size)
{
    return SRB_STATUS_INVALID_REQUEST;

    //PCDB cdb = srbext->Cdb;
    //PUCHAR buffer = (PUCHAR)srbext->DataBuffer;
    //ULONG buf_size = srbext->DataBufLen;
    //UCHAR srb_status = SRB_STATUS_INVALID_REQUEST;
    //ULONG stor_status = STOR_STATUS_SUCCESS;
    //CRamdisk* disk = srbext->DevExt->RamDisk;

    //switch (cdb->CDB6INQUIRY.PageCode)
    //{
    //case VPD_SUPPORTED_PAGES:
    //{
    //    UCHAR supported_pages = 5;
    //    PVPD_SUPPORTED_PAGES_PAGE page = NULL;
    //    buf_size = (FIELD_OFFSET(VPD_SUPPORTED_PAGES_PAGE, SupportedPageList) + supported_pages);
    //    stor_status = StorPortAllocatePool(srbext->DevExt, buf_size, VPDPAGE_TAG, (PVOID*)&page);

    //    if (stor_status != STOR_STATUS_SUCCESS)
    //    {
    //        srb_status = SRB_STATUS_INTERNAL_ERROR;
    //        break;
    //    }

    //    RtlZeroMemory(page, buf_size);
    //    page->DeviceType = DIRECT_ACCESS_DEVICE;
    //    page->DeviceTypeQualifier = DEVICE_CONNECTED;
    //    page->PageCode = VPD_SUPPORTED_PAGES;
    //    page->PageLength = supported_pages;
    //    page->SupportedPageList[0] = VPD_SUPPORTED_PAGES;
    //    page->SupportedPageList[1] = VPD_SERIAL_NUMBER;
    //    page->SupportedPageList[2] = VPD_DEVICE_IDENTIFIERS;
    //    page->SupportedPageList[3] = VPD_BLOCK_LIMITS;
    //    page->SupportedPageList[4] = VPD_BLOCK_DEVICE_CHARACTERISTICS;

    //    ULONG data_len = min(srbext->DataBufLen, buf_size);
    //    StorPortCopyMemory(srbext->DataBuffer, page, data_len);
    //    ret_size = data_len;
    //    StorPortFreePool(srbext->DevExt, page);
    //    stor_status = SRB_STATUS_SUCCESS;
    //}
    //break;
    //case VPD_SERIAL_NUMBER:
    //{
    //    size_t size = disk->SnStrLen;
    //    ret_size = (ULONG)(size + sizeof(VPD_SERIAL_NUMBER_PAGE) + 1);
    //    srb_status = SRB_STATUS_DATA_OVERRUN;
    //    if (ret_size <= buf_size)
    //    {
    //        FillInquiryPage_SerialNumber(buffer, buf_size, disk->SN, (UCHAR)disk->SnStrLen);
    //        srb_status = SRB_STATUS_SUCCESS;
    //    }
    //}
    //break;
    //case VPD_DEVICE_IDENTIFIERS:
    //{
    //    PVPD_IDENTIFICATION_PAGE page = NULL;
    //    ULONG nqn_size = (ULONG)disk->SnStrLen;
    //    ULONG vid_size = (ULONG)strlen(disk->VenderID);
    //    buf_size = (ULONG)sizeof(VPD_IDENTIFICATION_PAGE) + (ULONG)sizeof(VPD_IDENTIFICATION_DESCRIPTOR)
    //        + nqn_size + vid_size;

    //    stor_status = StorPortAllocatePool(srbext->DevExt, buf_size, VPDPAGE_TAG, (PVOID*)&page);
    //    if (stor_status != STOR_STATUS_SUCCESS)
    //    {
    //        srb_status = SRB_STATUS_INTERNAL_ERROR;
    //        break;
    //    }

    //    RtlZeroMemory(page, buf_size);
    //    FillInquiryPage_Identifier(page, (UCHAR*)disk->VenderID, (UCHAR)vid_size, disk->NQN, (UCHAR)disk->NqnStrLen);

    //    ULONG data_len = min(srbext->DataBufLen, buf_size);
    //    StorPortCopyMemory(srbext->DataBuffer, page, data_len);
    //    StorPortFreePool(srbext->DevExt, page);
    //    ret_size = data_len;

    //    srb_status = SRB_STATUS_SUCCESS;
    //}
    //break;
    //case VPD_BLOCK_LIMITS:
    //{
    //    //Max SCSI transfer block size
    //    //question : is it really used in modern windows system? Orz
    //    PVPD_BLOCK_LIMITS_PAGE page = NULL;
    //    buf_size = sizeof(VPD_BLOCK_LIMITS_PAGE);

    //    stor_status = StorPortAllocatePool(srbext->DevExt, buf_size, VPDPAGE_TAG, (PVOID*)&page);
    //    if (stor_status != STOR_STATUS_SUCCESS)
    //    {
    //        srb_status = SRB_STATUS_INTERNAL_ERROR;
    //        break;
    //    }

    //    RtlZeroMemory(page, buf_size);

    //    page->DeviceType = DIRECT_ACCESS_DEVICE;
    //    page->DeviceTypeQualifier = DEVICE_CONNECTED;
    //    page->PageCode = VPD_BLOCK_LIMITS;
    //    REVERSE_BYTES_2(page->PageLength, &buf_size);

    //    ULONG max_tx = disk->MaxTxSize;
    //    REVERSE_BYTES_4(page->MaximumTransferLength, &max_tx);
    //    REVERSE_BYTES_4(page->OptimalTransferLength, &max_tx);

    //    //todo: which value should I assign here?
    //    USHORT granularity = 0;
    //    REVERSE_BYTES_2(page->OptimalTransferLengthGranularity, &granularity);

    //    ULONG data_size = min(srbext->DataBufLen, buf_size);
    //    StorPortCopyMemory(srbext->DataBuffer, page, data_size);
    //    ret_size = data_size;
    //    StorPortFreePool(srbext->DevExt, page);

    //    srb_status = SRB_STATUS_SUCCESS;
    //}
    //break;
    //case VPD_BLOCK_DEVICE_CHARACTERISTICS:
    //{
    //    PVPD_BLOCK_DEVICE_CHARACTERISTICS_PAGE page = NULL;
    //    buf_size = sizeof(VPD_BLOCK_DEVICE_CHARACTERISTICS_PAGE);

    //    stor_status = StorPortAllocatePool(srbext->DevExt, buf_size, VPDPAGE_TAG, (PVOID*)&page);
    //    if (stor_status != STOR_STATUS_SUCCESS)
    //    {
    //        srb_status = SRB_STATUS_INTERNAL_ERROR;
    //        break;
    //    }

    //    RtlZeroMemory(page, buf_size);
    //    page->DeviceType = DIRECT_ACCESS_DEVICE;
    //    page->DeviceTypeQualifier = DEVICE_CONNECTED;
    //    page->PageCode = VPD_BLOCK_DEVICE_CHARACTERISTICS;
    //    page->PageLength = (UCHAR)buf_size;
    //    page->MediumRotationRateLsb = 1;
    //    page->NominalFormFactor = 0;

    //    ULONG data_size = min(srbext->DataBufLen, buf_size);
    //    StorPortCopyMemory(srbext->DataBuffer, page, data_size);
    //    ret_size = data_size;
    //    StorPortFreePool(srbext->DevExt, page);

    //    srb_status = SRB_STATUS_SUCCESS;
    //}
    //break;
    //default:
    //{
    //    srb_status = SRB_STATUS_ERROR;
    //}
    //break;
    //}

    //return srb_status;
}
inline void FillInquiry_InquiryData(PINQUIRYDATA data, char* vid, char* pid, char* rev)
{
    data->DeviceType = DIRECT_ACCESS_DEVICE;
    data->DeviceTypeQualifier = DEVICE_CONNECTED;
    data->RemovableMedia = 0;
    data->Versions = 0x06;
    data->NormACA = 0;
    data->HiSupport = 0;
    data->ResponseDataFormat = 2;
    data->AdditionalLength = INQUIRYDATABUFFERSIZE - 5;  // Amount of data we are returning
    data->EnclosureServices = 0;
    data->MediumChanger = 0;
    data->CommandQueue = 1;
    data->Wide16Bit = 0;
    data->Addr16 = 0;
    data->Synchronous = 0;
    data->Reserved3[0] = 0;

    data->Wide32Bit = TRUE;
    data->LinkedCommands = FALSE;   // No Linked Commands
    RtlCopyMemory((PUCHAR)&data->VendorId[0], vid, strlen(vid));
    RtlCopyMemory((PUCHAR)&data->ProductId[0], pid, strlen(pid));
    RtlCopyMemory((PUCHAR)&data->ProductRevisionLevel[0], rev, strlen(rev));
}

UCHAR Scsi_RequestSense6(SPCNVME_SRBEXT srbext)
{
    return SRB_STATUS_INVALID_REQUEST;

    ////request sense is used to query error information from device.
    ////device should reply SENSE_DATA structure.
    ////Todo: return real sense data back....
    //UCHAR srb_status = SRB_STATUS_ERROR;
    //PCDB cdb = srbext->Cdb;
    //PCDB6_REQUESTSENSE request = (PCDB6_REQUESTSENSE) cdb->AsByte;
    //UINT32 alloc_size = request->AllocSize;     //cdb->CDB6GENERIC.CommandUniqueBytes[2];
    //UINT8 format = request->DescFormat;   //cdb->CDB6GENERIC.Immediate;

    //ULONG copy_size = 0;

    ////DescFormat field indicates "which format of sense data should be returned?"
    ////1 == descriptor format sense data shall be returned. (desc header + multiple SENSE_DATA)
    ////0 == return fixed format data (just return one SENSE_DATA structure)
    //SENSE_DATA_EX data = {0};
    //if (1 == format)
    //{
    //    data.DescriptorData.ErrorCode = SCSI_SENSE_ERRORCODE_DESCRIPTOR_CURRENT;
    //    data.DescriptorData.SenseKey = SCSI_SENSE_NO_SENSE;
    //    data.DescriptorData.AdditionalSenseCode = SCSI_ADSENSE_NO_SENSE;
    //    data.DescriptorData.AdditionalSenseCodeQualifier = 0;
    //    data.DescriptorData.AdditionalSenseLength = 0;
    //    srb_status = SRB_STATUS_SUCCESS;
    //    copy_size = sizeof(DESCRIPTOR_SENSE_DATA);
    //}
    //else
    //{
    //    /* Fixed Format Sense Data */
    //    data.FixedData.ErrorCode = SCSI_SENSE_ERRORCODE_FIXED_CURRENT;
    //    data.FixedData.SenseKey = SCSI_SENSE_NO_SENSE;
    //    data.FixedData.AdditionalSenseCode = SCSI_ADSENSE_NO_SENSE;
    //    data.FixedData.AdditionalSenseCodeQualifier = 0;
    //    data.FixedData.AdditionalSenseLength = 0;

    //    srb_status = SRB_STATUS_SUCCESS;
    //    copy_size = sizeof(SENSE_DATA);
    //}

    //if (copy_size > alloc_size)
    //    copy_size = alloc_size;

    //StorPortCopyMemory(srbext->DataBuffer, &data, copy_size);
    //SrbSetDataTransferLength(srbext->Srb, copy_size);
    //return srb_status;
}
UCHAR Scsi_Read6(SPCNVME_SRBEXT srbext)
{
    return SRB_STATUS_INVALID_REQUEST;
//    return ReadWriteRamdisk(srbext, FALSE);
}
UCHAR Scsi_Write6(SPCNVME_SRBEXT srbext)
{
    return SRB_STATUS_INVALID_REQUEST;
//    return ReadWriteRamdisk(srbext, TRUE);
}
UCHAR Scsi_Inquiry6(SPCNVME_SRBEXT srbext) 
{
    return SRB_STATUS_INVALID_REQUEST;

    //UCHAR srb_status = SRB_STATUS_INVALID_REQUEST;
    //ULONG ret_size = 0;
    //PCDB cdb = srbext->Cdb;

    //if (cdb->CDB6INQUIRY3.EnableVitalProductData)
    //{
    //    srb_status = HandleInquiryVPD(srbext, ret_size);
    //}
    //else
    //{
    //    if (cdb->CDB6INQUIRY3.PageCode > 0) 
    //    {
    //        srb_status = SRB_STATUS_ERROR;
    //    }
    //    else
    //    {
    //        PINQUIRYDATA data = (PINQUIRYDATA)srbext->DataBuffer;
    //        ULONG size = srbext->DataBufLen;
    //        ret_size = INQUIRYDATABUFFERSIZE;
    //        srb_status = SRB_STATUS_DATA_OVERRUN;
    //        if(size >= INQUIRYDATABUFFERSIZE)
    //        {
    //            RtlZeroMemory(srbext->DataBuffer, srbext->DataBufLen);
    //            CRamdisk* disk = srbext->DevExt->RamDisk;
    //            FillInquiry_InquiryData(data, (char*)disk->VenderID, 
    //                            (char*)disk->ProductID, (char*)disk->RevID);
    //            srb_status = SRB_STATUS_SUCCESS;
    //        }
    //    }
    //}
    //
    //SrbSetDataTransferLength(srbext->Srb, ret_size);
    //return srb_status;
}
UCHAR Scsi_Verify6(SPCNVME_SRBEXT srbext)
{
    return SRB_STATUS_INVALID_REQUEST;

////VERIFY(6) seems obsoleted? I didn't see description in Seagate SCSI reference.
//    UCHAR srb_status = SRB_STATUS_INVALID_REQUEST;
//    UNREFERENCED_PARAMETER(srbext);
//    //ULONG ret_size = 0;
//
//    //HandleCheckCondition(srbext->Srb, srb_status, ret_size);
//    return srb_status;
}
UCHAR Scsi_ModeSelect6(SPCNVME_SRBEXT srbext)
{
    return SRB_STATUS_INVALID_REQUEST;
    //UCHAR srb_status = SRB_STATUS_INVALID_REQUEST;
    ////ULONG ret_size = 0;
    //UNREFERENCED_PARAMETER(srbext);
    ////HandleCheckCondition(srbext->Srb, srb_status, ret_size);
    //return srb_status;
}
UCHAR Scsi_ModeSense6(SPCNVME_SRBEXT srbext)
{
    return SRB_STATUS_INVALID_REQUEST;
//    UCHAR srb_status = SRB_STATUS_ERROR;
//    PCDB cdb = srbext->Cdb;
//    PUCHAR buffer = (PUCHAR) srbext->DataBuffer;
//    PMODE_PARAMETER_HEADER header = (PMODE_PARAMETER_HEADER)buffer;
//    ULONG buf_size = srbext->DataBufLen;
//    ULONG ret_size = 0;
//    UCHAR page_size = 0;
//
//    if (NULL == buffer || 0 == buf_size)
//        return SRB_STATUS_ERROR;
//
//    if (buf_size < sizeof(MODE_PARAMETER_HEADER))
//    {
//        srb_status = SRB_STATUS_DATA_OVERRUN;
//        ret_size = sizeof(MODE_PARAMETER_HEADER);
//        goto end;
//    }
//
//    FillParamHeader(header);
//    buffer += sizeof(MODE_PARAMETER_HEADER);
//    buf_size -= sizeof(MODE_PARAMETER_HEADER);
//    ret_size += sizeof(MODE_PARAMETER_HEADER);
//
//    // Todo: reply real mode sense data
//    switch (cdb->MODE_SENSE.PageCode)
//    {
//        case MODE_PAGE_CACHING:
//        {
//            ReplyModePageCaching(buffer, buf_size, ret_size);
//            header->ModeDataLength += page_size;
//            srb_status = SRB_STATUS_SUCCESS;
//            break;
//        }
//        case MODE_PAGE_CONTROL:
//        {
//            ReplyModePageControl(buffer, buf_size, ret_size);
//            header->ModeDataLength += page_size;
//            srb_status = SRB_STATUS_SUCCESS;
//            break;
//        }
//        case MODE_PAGE_FAULT_REPORTING:
//        {
//            //in HLK, it required "Information Exception Control Page".
//            //But it is renamed to MODE_PAGE_FAULT_REPORTING in Windows Storport ....
//            //refet to https://www.t10.org/ftp/t10/document.94/94-190r3.pdf
//            ReplyModePageInfoExceptionCtrl(buffer, buf_size, ret_size);
//            header->ModeDataLength += page_size;
//            srb_status = SRB_STATUS_SUCCESS;
//            break;
//        }
//        case MODE_SENSE_RETURN_ALL:
//        {
//            if(buf_size > 0)
//            {
//                ReplyModePageCaching(buffer, buf_size, ret_size);
//                header->ModeDataLength += page_size;
//            }
//            if (buf_size > 0)
//            {
//                ReplyModePageControl(buffer, buf_size, ret_size);
//                header->ModeDataLength += page_size;
//            }
//            if (buf_size > 0)
//            {
//                ReplyModePageInfoExceptionCtrl(buffer, buf_size, ret_size);
//                header->ModeDataLength += page_size;
//            }
//
//            srb_status = SRB_STATUS_SUCCESS;
//            break;
//        }
//        default:
//        {
//            srb_status = SRB_STATUS_SUCCESS;
//            break;
//        }
//    }
//
//end:
//    SrbSetDataTransferLength(srbext->Srb, ret_size);
//    return srb_status;
}


