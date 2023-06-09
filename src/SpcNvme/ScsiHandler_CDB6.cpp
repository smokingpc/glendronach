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

static UCHAR Reply_VpdSupportPages(PSPCNVME_SRBEXT srbext, ULONG& ret_size)
{
    ULONG buf_size = srbext->DataBufLen();
    ret_size = 0;

    UCHAR valid_pages = 5;
    buf_size = (FIELD_OFFSET(VPD_SUPPORTED_PAGES_PAGE, SupportedPageList) +
        valid_pages * sizeof(UCHAR));

    SPC::CAutoPtr<UCHAR, PagedPool, TAG_VPDPAGE>
            page_ptr(new(PagedPool, TAG_VPDPAGE) UCHAR[buf_size]);
    if(page_ptr.IsNull())
        return SRB_STATUS_INSUFFICIENT_RESOURCES;
    PVPD_SUPPORTED_PAGES_PAGE page = (PVPD_SUPPORTED_PAGES_PAGE)page_ptr.Get();
    page->DeviceType = DIRECT_ACCESS_DEVICE;
    page->DeviceTypeQualifier = DEVICE_CONNECTED;
    page->PageCode = VPD_SUPPORTED_PAGES;
    page->PageLength = valid_pages;
    page->SupportedPageList[0] = VPD_SUPPORTED_PAGES;
    page->SupportedPageList[1] = VPD_SERIAL_NUMBER;
    page->SupportedPageList[2] = VPD_DEVICE_IDENTIFIERS;
    page->SupportedPageList[3] = VPD_BLOCK_LIMITS;
    page->SupportedPageList[4] = VPD_BLOCK_DEVICE_CHARACTERISTICS;

    ret_size = min(srbext->DataBufLen(), buf_size);
    RtlCopyMemory(srbext->DataBuf(), page, ret_size);
    return SRB_STATUS_SUCCESS;
}
static UCHAR Reply_VpdSerialNumber(PSPCNVME_SRBEXT srbext, ULONG& ret_size)
{
    PUCHAR buffer = (PUCHAR)srbext->DataBuf();
    ULONG buf_size = srbext->DataBufLen();
    size_t sn_len = strlen((char*)srbext->DevExt->CtrlIdent.SN);
    sn_len = (sn_len, 255);
    ULONG size = (ULONG)(sn_len + sizeof(VPD_SERIAL_NUMBER_PAGE) + 1);
    ret_size = size;

    if(size < srbext->DataBufLen())
        return SRB_STATUS_INSUFFICIENT_RESOURCES;

    PVPD_SERIAL_NUMBER_PAGE page = (PVPD_SERIAL_NUMBER_PAGE)srbext->DataBuf();
    RtlZeroMemory(buffer, buf_size);
    page->DeviceType = DIRECT_ACCESS_DEVICE;
    page->DeviceTypeQualifier = DEVICE_CONNECTED;
    page->PageCode = VPD_SERIAL_NUMBER;
    page->PageLength = (UCHAR) sn_len;
    page++;
    memcpy(page, srbext->DevExt->CtrlIdent.SN, sn_len);

    return SRB_STATUS_SUCCESS;
}
static UCHAR Reply_VpdIdentifier(PSPCNVME_SRBEXT srbext, ULONG& ret_size)
{
    char *subnqn = (char*)srbext->DevExt->CtrlIdent.SUBNQN;
    ULONG nqn_size = (ULONG)strlen((char*)subnqn);
    ULONG vid_size = (ULONG)strlen((char*)NVME_CONST::VENDOR_ID);
    size_t buf_size = (ULONG)sizeof(VPD_IDENTIFICATION_PAGE) +
                        (ULONG)sizeof(VPD_IDENTIFICATION_DESCRIPTOR)
                        + nqn_size + vid_size + 1;      //1 more byte for "_"
    SPC::CAutoPtr<VPD_IDENTIFICATION_PAGE, PagedPool, TAG_VPDPAGE>
        page(new(PagedPool, TAG_VPDPAGE) UCHAR[buf_size]);

    //NQN is too long. So only use VID + SN as Identifier.
    PVPD_IDENTIFICATION_DESCRIPTOR desc = NULL;
    ULONG size = (ULONG)buf_size - sizeof(VPD_IDENTIFICATION_PAGE);
    page->DeviceType = DIRECT_ACCESS_DEVICE;
    page->DeviceTypeQualifier = DEVICE_CONNECTED;
    page->PageCode = VPD_DEVICE_IDENTIFIERS;
    page->PageLength = (UCHAR) min(size, MAXUCHAR);
    desc = (PVPD_IDENTIFICATION_DESCRIPTOR)page->Descriptors;

    desc->CodeSet = VpdCodeSetAscii;
    desc->IdentifierType = VpdIdentifierTypeVendorId;
    desc->Association = VpdAssocDevice;
    size = size - sizeof(VPD_IDENTIFICATION_DESCRIPTOR);
    desc->IdentifierLength = (UCHAR)min(size, 255);
    RtlCopyMemory(desc->Identifier, NVME_CONST::VENDOR_ID, vid_size);
    RtlCopyMemory(&desc->Identifier[vid_size], "_", 1);
    RtlCopyMemory(&desc->Identifier[vid_size + 1], subnqn, nqn_size);

    ret_size = (ULONG)min(srbext->DataBufLen(), buf_size);
    RtlCopyMemory(srbext->DataBuf(), page, ret_size);

    return SRB_STATUS_SUCCESS;
}
static UCHAR Reply_VpdBlockLimits(PSPCNVME_SRBEXT srbext, ULONG& ret_size)
{
    //Max SCSI transfer block size
    //question : is it really used in modern windows system? Orz
    ULONG buf_size = sizeof(VPD_BLOCK_LIMITS_PAGE);
    SPC::CAutoPtr<VPD_BLOCK_LIMITS_PAGE, PagedPool, TAG_VPDPAGE>
        page(new(PagedPool, TAG_VPDPAGE) UCHAR[buf_size]);

    page->DeviceType = DIRECT_ACCESS_DEVICE;
    page->DeviceTypeQualifier = DEVICE_CONNECTED;
    page->PageCode = VPD_BLOCK_LIMITS;
    REVERSE_BYTES_2(page->PageLength, &buf_size);

    ULONG max_tx = srbext->DevExt->MaxTxSize;
    //tell I/O system: max tx size and optimal tx size of this adapter.
    REVERSE_BYTES_4(page->MaximumTransferLength, &max_tx);
    REVERSE_BYTES_4(page->OptimalTransferLength, &max_tx);

    //Refer to SCSI SBC3 doc or SCSI reference Block Limits VPD page_ptr.
    //http://www.13thmonkey.org/documentation/SCSI/sbc3r25.pdf
    USHORT granularity = 4;
    REVERSE_BYTES_2(page->OptimalTransferLengthGranularity, &granularity);

    ret_size = min(srbext->DataBufLen(), buf_size);
    RtlCopyMemory(srbext->DataBuf(), page, ret_size);

    return SRB_STATUS_SUCCESS;
}
static UCHAR Reply_VpdBlockDeviceCharacteristics(PSPCNVME_SRBEXT srbext, ULONG& ret_size)
{
    ULONG buf_size = sizeof(VPD_BLOCK_DEVICE_CHARACTERISTICS_PAGE);
    SPC::CAutoPtr<VPD_BLOCK_DEVICE_CHARACTERISTICS_PAGE, PagedPool, TAG_VPDPAGE>
        page(new(PagedPool, TAG_VPDPAGE) UCHAR[buf_size]);

    page->DeviceType = DIRECT_ACCESS_DEVICE;
    page->DeviceTypeQualifier = DEVICE_CONNECTED;
    page->PageCode = VPD_BLOCK_DEVICE_CHARACTERISTICS;
    page->PageLength = (UCHAR)buf_size;
    page->MediumRotationRateLsb = 1;        //todo: what is this?
    page->NominalFormFactor = 0;

    ret_size = min(srbext->DataBufLen(), buf_size);
    RtlCopyMemory(srbext->DataBuf(), page, ret_size);

    return SRB_STATUS_SUCCESS;
}
static UCHAR HandleInquiryVPD(PSPCNVME_SRBEXT srbext, ULONG& ret_size)
{
    PCDB cdb = srbext->Cdb();
    UCHAR srb_status = SRB_STATUS_INVALID_REQUEST;

    switch (cdb->CDB6INQUIRY.PageCode)
    {
        case VPD_SUPPORTED_PAGES:
            srb_status = Reply_VpdSupportPages(srbext, ret_size);
            break;
        case VPD_SERIAL_NUMBER:
            srb_status = Reply_VpdSerialNumber(srbext, ret_size);
            break;
        case VPD_DEVICE_IDENTIFIERS:
            srb_status = Reply_VpdIdentifier(srbext, ret_size);
            break;
        case VPD_BLOCK_LIMITS:
            srb_status = Reply_VpdBlockLimits(srbext, ret_size);
            break;
        case VPD_BLOCK_DEVICE_CHARACTERISTICS:
            srb_status = Reply_VpdBlockDeviceCharacteristics(srbext, ret_size);
            break;
        default:
            srb_status = SRB_STATUS_ERROR;
            break;
    }

    return srb_status;
}
static void BuildInquiryData(PINQUIRYDATA data, char* vid, char* pid, char* rev)
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
    RtlCopyMemory((PUCHAR)&data->VendorId[0], vid, sizeof(data->VendorId));
    RtlCopyMemory((PUCHAR)&data->ProductId[0], pid, sizeof(data->ProductId));
    RtlCopyMemory((PUCHAR)&data->ProductRevisionLevel[0], rev, sizeof(data->ProductRevisionLevel));
}

UCHAR Scsi_RequestSense6(PSPCNVME_SRBEXT srbext)
{
    UNREFERENCED_PARAMETER(srbext);
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
UCHAR Scsi_Read6(PSPCNVME_SRBEXT srbext)
{
//the SCSI I/O are based for BLOCKs of device, not bytes....
    ULONG64 offset = 0; //in blocks
    ULONG len = 0;    //in blocks
    PCDB cdb = srbext->Cdb();

    ParseReadWriteOffsetAndLen(cdb->CDB6READWRITE, offset, len);
    return Scsi_ReadWrite(srbext, offset, len, false);
}
UCHAR Scsi_Write6(PSPCNVME_SRBEXT srbext)
{
    //the SCSI I/O are based for BLOCKs of device, not bytes....
    ULONG64 offset = 0; //in blocks
    ULONG len = 0;    //in blocks
    PCDB cdb = srbext->Cdb();

    ParseReadWriteOffsetAndLen(cdb->CDB6READWRITE, offset, len);
    return Scsi_ReadWrite(srbext, offset, len, true);
}
UCHAR Scsi_Inquiry6(PSPCNVME_SRBEXT srbext) 
{
    ULONG ret_size = 0;
    UCHAR srb_status = SRB_STATUS_ERROR;
    PCDB cdb = srbext->Cdb();

    if (cdb->CDB6INQUIRY3.EnableVitalProductData)
    {
        srb_status = HandleInquiryVPD(srbext, ret_size);
    }
    else
    {
        if (cdb->CDB6INQUIRY3.PageCode > 0) 
        {
            srb_status = SRB_STATUS_ERROR;
        }
        else
        {
            PINQUIRYDATA data = (PINQUIRYDATA)srbext->DataBuf();
            ULONG size = srbext->DataBufLen();
            ret_size = 0;
            srb_status = SRB_STATUS_DATA_OVERRUN;
            //in Win2000 and older version, NT SCSI system only query 
            //INQUIRYDATABUFFERSIZE bytes.
            //Since WinXP, it should return sizeof(INQUIRYDATA) bytes data. 
            if(size >= INQUIRYDATABUFFERSIZE)
            {
                RtlZeroMemory(srbext->DataBuf(), srbext->DataBufLen());
                BuildInquiryData(data, (char*)NVME_CONST::VENDOR_ID,
                                (char*)NVME_CONST::PRODUCT_ID, 
                                (char*)NVME_CONST::PRODUCT_REV);
                srb_status = SRB_STATUS_SUCCESS;
                ret_size = size;
            }
        }
    }
    
    SrbSetDataTransferLength(srbext->Srb, ret_size);
    return srb_status;
}
UCHAR Scsi_Verify6(PSPCNVME_SRBEXT srbext)
{
////VERIFY(6) seems obsoleted? I didn't see description in Seagate SCSI reference.
    UNREFERENCED_PARAMETER(srbext);
    return SRB_STATUS_INVALID_REQUEST;
}
UCHAR Scsi_ModeSelect6(PSPCNVME_SRBEXT srbext)
{
    CDB::_MODE_SELECT *select = &srbext->Cdb()->MODE_SELECT;
    PUCHAR buffer = (PUCHAR)srbext->DataBuf();
    ULONG mode_data_size = 0;
    ULONG offset = 0;
    PMODE_PARAMETER_HEADER header = (PMODE_PARAMETER_HEADER)buffer;
    PMODE_PARAMETER_BLOCK param_block = (PMODE_PARAMETER_BLOCK)(header+1);
    PUCHAR cursor = ((PUCHAR)param_block + header->BlockDescriptorLength);
    //ParameterList Layout of ModeSelect is:
    //[MODE_PARAMETER_HEADER][MODE_PARAMETER_BLOCK][PAGE1][PAGE2][.....]
    //There are two problem here:
    //1.PMODE_PARAMETER_BLOCK is optional. Sometimes it is not exist 
    //  so header->BlockDescriptorLength == 0.
    //2.header->ModeDataLength IS ALWAYS 0. Don't use it to check following data.
    //  (refet to Seagate SCSI Command reference. This field is reserved in MODE_SELECT cmd)
    //  You should use total buffer length to check following data(mode page) blocks.

    //currently don't support VendorSpecific MODE_SELECT op.
    if(0 == select->PFBit)
        return SRB_STATUS_INVALID_REQUEST;

    DbgBreakPoint();
    if(0 == header->BlockDescriptorLength)
        param_block = NULL;

    //windows set header->ModeDataLength to 0. No idea if it is bug or lazy....
    mode_data_size = srbext->DataBufLen() - sizeof(MODE_PARAMETER_HEADER) - header->BlockDescriptorLength;
    offset = 0;
    while(mode_data_size > 0)
    {
        PMODE_CACHING_PAGE page = (PMODE_CACHING_PAGE)(cursor + offset);
        mode_data_size -= (page->PageLength+2);
        offset += (page->PageLength + 2);

        if (0 == page->PageLength)
            break;

        if(page->PageCode != MODE_PAGE_CACHING || page->PageLength != (sizeof(MODE_CACHING_PAGE)-2))
            continue;

        DbgBreakPoint();
        srbext->DevExt->ReadCacheEnabled = !page->ReadDisableCache;
        srbext->DevExt->WriteCacheEnabled = page->WriteCacheEnable;
    }

    SrbSetDataTransferLength(srbext->Srb, 0);
    return SRB_STATUS_SUCCESS;
}

UCHAR Scsi_ModeSense6(PSPCNVME_SRBEXT srbext)
{
    UCHAR srb_status = SRB_STATUS_ERROR;
    PCDB cdb = srbext->Cdb();
    PUCHAR buffer = (PUCHAR)srbext->DataBuf();
    PMODE_PARAMETER_HEADER header = (PMODE_PARAMETER_HEADER)buffer;
    ULONG buf_size = srbext->DataBufLen();
    ULONG ret_size = 0;
    ULONG page_size = 0;    //this is "copied ModePage size", not OS PAGE_SIZE...

    if (buf_size < sizeof(MODE_PARAMETER_HEADER) || NULL == buffer)
    {
        srb_status = SRB_STATUS_DATA_OVERRUN;
        ret_size = sizeof(MODE_PARAMETER_HEADER);
        goto end;
    }

    FillParamHeader(header);
    buffer += sizeof(MODE_PARAMETER_HEADER);
    buf_size -= sizeof(MODE_PARAMETER_HEADER);
    ret_size += sizeof(MODE_PARAMETER_HEADER);

    // Todo: reply real mode sense data
    switch (cdb->MODE_SENSE.PageCode)
    {
    case MODE_PAGE_CACHING:
    {
        page_size = ReplyModePageCaching(srbext->DevExt, buffer, buf_size, ret_size);
        header->ModeDataLength += (UCHAR)page_size;
        srb_status = SRB_STATUS_SUCCESS;
        break;
    }
    case MODE_PAGE_CONTROL:
    {
        page_size = ReplyModePageControl(buffer, buf_size, ret_size);
        header->ModeDataLength += (UCHAR)page_size;
        srb_status = SRB_STATUS_SUCCESS;
        break;
    }
    case MODE_PAGE_FAULT_REPORTING:
    {
        //in HLK, it required "Information Exception Control Page".
        //But it is renamed to MODE_PAGE_FAULT_REPORTING in Windows Storport ....
        //refet to https://www.t10.org/ftp/t10/document.94/94-190r3.pdf
        page_size = ReplyModePageInfoExceptionCtrl(buffer, buf_size, ret_size);
        header->ModeDataLength += (UCHAR)page_size;
        srb_status = SRB_STATUS_SUCCESS;
        break;
    }
    case MODE_SENSE_RETURN_ALL:
    {
        if (buf_size > 0)
        {
        //buffer size and buffer will be updated in function.
            page_size = ReplyModePageCaching(srbext->DevExt, buffer, buf_size, ret_size);
            header->ModeDataLength += (UCHAR)page_size;
        }
        if (buf_size > 0)
        {
            page_size = ReplyModePageControl(buffer, buf_size, ret_size);
            header->ModeDataLength += (UCHAR)page_size;
        }
        if (buf_size > 0)
        {
            page_size = ReplyModePageInfoExceptionCtrl(buffer, buf_size, ret_size);
            header->ModeDataLength += (UCHAR)page_size;
        }

        srb_status = SRB_STATUS_SUCCESS;
        break;
    }
    default:
    {
        srb_status = SRB_STATUS_INVALID_REQUEST;
        break;
    }
    }

end:
    SrbSetDataTransferLength(srbext->Srb, ret_size);
    return srb_status;
}
UCHAR Scsi_TestUnitReady(PSPCNVME_SRBEXT srbext)
{
    UNREFERENCED_PARAMETER(srbext);
    return SRB_STATUS_INVALID_REQUEST;
}
