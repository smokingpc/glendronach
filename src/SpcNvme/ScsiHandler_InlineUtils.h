#pragma once
inline void CopyToCdbBuffer(PUCHAR& buffer, ULONG& buf_size, PVOID page, ULONG page_size, ULONG& ret_size)
{
    ULONG copy_size = 0;
    copy_size = page_size;
    if (copy_size > buf_size)
        copy_size = buf_size;

    StorPortCopyMemory(buffer, page, copy_size);
    buf_size -= copy_size;  //calculate "how many bytes left for my pending copy"?
    ret_size += copy_size;
    buffer += copy_size;
}
inline void FillParamHeader(PMODE_PARAMETER_HEADER header)
{
    header->ModeDataLength = sizeof(MODE_PARAMETER_HEADER) - sizeof(header->ModeDataLength);
    header->MediumType = DIRECT_ACCESS_DEVICE;
    header->DeviceSpecificParameter = 0;
    header->BlockDescriptorLength = 0;  //I don't want reply BlockDescriptor  :p
}
inline void FillParamHeader10(PMODE_PARAMETER_HEADER10 header)
{
    //MODE_PARAMETER_HEADER10 header = { 0 };
    USHORT data_size = sizeof(MODE_PARAMETER_HEADER10);
    USHORT mode_data_size = data_size - sizeof(header->ModeDataLength);
    REVERSE_BYTES_2(header->ModeDataLength, &mode_data_size);
    header->MediumType = DIRECT_ACCESS_DEVICE;
    header->DeviceSpecificParameter = 0;
    //I don't want reply BlockDescriptor, so dont set BlockDescriptorLength field  :p
}
inline void FillModePage_Caching(PMODE_CACHING_PAGE page)
{
    page->PageCode = MODE_PAGE_CACHING;
    page->PageLength = (UCHAR)(sizeof(MODE_CACHING_PAGE) - 2); //sizeof(MODE_CACHING_PAGE) - sizeof(page->PageCode) - sizeof(page->PageLength)
    page->ReadDisableCache = 1;
}
inline void FillModePage_InfoException(PMODE_INFO_EXCEPTIONS page)
{
    page->PageCode = MODE_PAGE_FAULT_REPORTING;
    page->PageLength = (UCHAR)(sizeof(MODE_INFO_EXCEPTIONS) - 2); //sizeof(MODE_INFO_EXCEPTIONS) - sizeof(page->PageCode) - sizeof(page->PageLength)
    page->ReportMethod = 5;  //Generate no sense
}
inline void FillModePage_Control(PMODE_CONTROL_PAGE page)
{
    //all fields of MODE_CONTROL_PAGE refer to Seagate SCSI reference "Control Mode page (table 302)" 
    page->PageCode = MODE_PAGE_CONTROL;
    page->PageLength = (UCHAR)(sizeof(MODE_CONTROL_PAGE) - 2); //sizeof(MODE_CONTROL_PAGE) - sizeof(page->PageCode) - sizeof(page->PageLength)
    page->QueueAlgorithmModifier = 0;
}
inline void ReplyModePageCaching(PUCHAR& buffer, ULONG& buf_size, ULONG& ret_size)
{
    MODE_CACHING_PAGE page = { 0 };
    ULONG page_size = sizeof(MODE_CACHING_PAGE);
    FillModePage_Caching(&page);
    CopyToCdbBuffer(buffer, buf_size, &page, page_size, ret_size);
}
inline void ReplyModePageControl(PUCHAR& buffer, ULONG& buf_size, ULONG& ret_size)
{
    MODE_CONTROL_PAGE page = { 0 };
    ULONG page_size = sizeof(MODE_CONTROL_PAGE);
    FillModePage_Control(&page);
    CopyToCdbBuffer(buffer, buf_size, &page, page_size, ret_size);
}
inline void ReplyModePageInfoExceptionCtrl(PUCHAR& buffer, ULONG& buf_size, ULONG& ret_size)
{
    MODE_INFO_EXCEPTIONS page = { 0 };
    ULONG page_size = sizeof(MODE_INFO_EXCEPTIONS);
    FillModePage_InfoException(&page);
    CopyToCdbBuffer(buffer, buf_size, &page, page_size, ret_size);
}
inline void FillInquiryPage_SerialNumber(PUCHAR buffer, ULONG buf_size, UCHAR* sn, UCHAR sn_len)
{
    PVPD_SERIAL_NUMBER_PAGE page = (PVPD_SERIAL_NUMBER_PAGE)buffer;
    RtlZeroMemory(buffer, buf_size);
    page->DeviceType = DIRECT_ACCESS_DEVICE;
    page->DeviceTypeQualifier = DEVICE_CONNECTED;
    page->PageCode = VPD_SERIAL_NUMBER;
    page->PageLength = sn_len;

    memcpy((++page), sn, sn_len);
}
inline void FillInquiryPage_Identifier(PVPD_IDENTIFICATION_PAGE page, UCHAR *vid, UCHAR vid_size, UCHAR *nqn, UCHAR nqn_size)
{
    PVPD_IDENTIFICATION_DESCRIPTOR desc = NULL;
    ULONG size = sizeof(VPD_IDENTIFICATION_DESCRIPTOR) + vid_size + nqn_size;
    page->DeviceType = DIRECT_ACCESS_DEVICE;
    page->DeviceTypeQualifier = DEVICE_CONNECTED;
    page->PageCode = VPD_DEVICE_IDENTIFIERS;
    page->PageLength = (UCHAR)size;
    desc = (PVPD_IDENTIFICATION_DESCRIPTOR)page->Descriptors;

    desc->CodeSet = VpdCodeSetAscii;
    desc->IdentifierType = VpdIdentifierTypeVendorId;
    desc->Association = VpdAssocDevice;
    desc->IdentifierLength = nqn_size + vid_size;
    StorPortCopyMemory(desc->Identifier, vid, vid_size);
    StorPortCopyMemory(&desc->Identifier[vid_size], nqn, nqn_size);
}

//inline void HandleCheckCondition(PSTORAGE_REQUEST_BLOCK srb, UCHAR srb_status, ULONG ret_size)
//{
//    PSENSE_DATA sense = (PSENSE_DATA)SrbGetSenseInfoBuffer(srb);
//    UCHAR sense_size = SrbGetSenseInfoBufferLength(srb);
//    UCHAR sense_key = SCSI_SENSE_NO_SENSE;
//    UCHAR adsense_key = SCSI_ADSENSE_NO_SENSE;
//
//    if (SRB_STATUS_SUCCESS != srb_status)
//    {
//        sense_key = SCSI_SENSE_ILLEGAL_REQUEST;
//        adsense_key = SCSI_ADSENSE_ILLEGAL_COMMAND;
//    }
//
//    if (sense && sense_size)
//    {
//        RtlZeroMemory(sense, sense_size);
//        sense->ErrorCode = SCSI_SENSE_ERRORCODE_FIXED_CURRENT;
//        sense->Valid = 0;
//        sense->SenseKey = sense_key;
//        sense->AdditionalSenseLength = 0;
//        sense->AdditionalSenseCode = adsense_key;  //problem root cause
//        sense->AdditionalSenseCodeQualifier = 0;
//        SrbSetScsiStatus(srb, SCSISTAT_CHECK_CONDITION);
//        srb_status |= SRB_STATUS_AUTOSENSE_VALID;
//    }
//    //
//    // Set this to 0 to indicate that no data was transfered because
//    // of the error.
//    //
//    if (srb_status != SRB_STATUS_DATA_OVERRUN && srb_status != SRB_STATUS_SUCCESS)
//        ret_size = 0;
//    SrbSetDataTransferLength(srb, ret_size);
//}
