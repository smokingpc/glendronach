#pragma once
inline ULONG CopyToCdbBuffer(PUCHAR& buffer, ULONG& buf_size, PVOID page, ULONG page_size, ULONG &ret_size)
{
    ULONG copied_size = 0;
    copied_size = page_size;
    if (copied_size > buf_size)
        copied_size = buf_size;

    RtlCopyMemory(buffer, page, copied_size);
    buf_size -= copied_size;  //calculate "how many bytes left for my pending copy"?
    buffer += copied_size;
    ret_size += copied_size;
    return copied_size;
}
inline void FillParamHeader(PMODE_PARAMETER_HEADER header)
{
//In a complete page return buffer, the header->ModeDataLength should be
// => sizeof(followed page data) + sizeof(header) - sizeof(header->ModeDataLength).
//Here are set header->ModeDataLength to sizeof(MODE_PARAMETER_HEADER) - sizeof(header->ModeDataLength).
//In following procedure we will set it to header->ModeDataLength += page_size;
    header->ModeDataLength = sizeof(MODE_PARAMETER_HEADER) - sizeof(header->ModeDataLength);
    header->MediumType = DIRECT_ACCESS_DEVICE;
    header->DeviceSpecificParameter = 0;
    header->BlockDescriptorLength = 0;  //I don't want reply BlockDescriptor  :p
}
inline void FillParamHeader10(PMODE_PARAMETER_HEADER10 header)
{
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
inline ULONG ReplyModePageCaching(PUCHAR& buffer, ULONG& buf_size, ULONG& ret_size)
{
    MODE_CACHING_PAGE page = { 0 };
    ULONG page_size = sizeof(MODE_CACHING_PAGE);
    FillModePage_Caching(&page);

    return CopyToCdbBuffer(buffer, buf_size, &page, page_size, ret_size);
}
inline ULONG ReplyModePageControl(PUCHAR& buffer, ULONG& buf_size, ULONG& ret_size)
{
    MODE_CONTROL_PAGE page = { 0 };
    ULONG page_size = sizeof(MODE_CONTROL_PAGE);
    FillModePage_Control(&page);
    return CopyToCdbBuffer(buffer, buf_size, &page, page_size, ret_size);
}
inline ULONG ReplyModePageInfoExceptionCtrl(PUCHAR& buffer, ULONG& buf_size, ULONG& ret_size)
{
    MODE_INFO_EXCEPTIONS page = { 0 };
    ULONG page_size = sizeof(MODE_INFO_EXCEPTIONS);
    FillModePage_InfoException(&page);
    return CopyToCdbBuffer(buffer, buf_size, &page, page_size, ret_size);
}

#pragma region ======== Parse SCSI ReadWrite length and offset ======== 
//Note: In SCSI, all read/write request are in "BLOCKS", not in bytes.
inline void ParseReadWriteOffsetAndLen(CDB::_CDB6READWRITE &rw, ULONG64 &offset, ULONG &len)
{
    offset = (rw.LogicalBlockMsb1 << 16) | (rw.LogicalBlockMsb0 << 8) | rw.LogicalBlockLsb;
    len = rw.TransferBlocks;
    if(0 == len)
        len = 256;
}
inline void ParseReadWriteOffsetAndLen(CDB::_CDB10& rw, ULONG64& offset, ULONG &len)
{
    offset = 0;
    len = 0;
    REVERSE_BYTES_4(&offset, &rw.LogicalBlockByte0);
    REVERSE_BYTES_2(&len, &rw.TransferBlocksMsb);
}
inline void ParseReadWriteOffsetAndLen(CDB::_CDB12& rw, ULONG64& offset, ULONG &len)
{
    offset = 0;
    len = 0;
    REVERSE_BYTES_4(&offset, &rw.LogicalBlock);
    REVERSE_BYTES_4(&len, &rw.TransferLength);
}
inline void ParseReadWriteOffsetAndLen(CDB::_CDB16& rw, ULONG64& offset, ULONG &len)
{
    REVERSE_BYTES_8(&offset, &rw.LogicalBlock);
    REVERSE_BYTES_4(&len, &rw.TransferLength);
}
#pragma endregion 