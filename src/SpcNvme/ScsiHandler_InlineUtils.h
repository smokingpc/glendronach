#pragma once
// ================================================================
// SpcNvme : OpenSource NVMe Driver for Windows 8+
// Author : Roy Wang(SmokingPC).
// Licensed by MIT License.
// 
// Copyright (C) 2022, Roy Wang (SmokingPC)
// https://github.com/smokingpc/
// 
// NVMe Spec: https://nvmexpress.org/specifications/
// Contact Me : smokingpc@gmail.com
// ================================================================
// Permission is hereby granted, free of charge, to any person obtaining a 
// copy of this softwareand associated documentation files(the "Software"), 
// to deal in the Software without restriction, including without limitation 
// the rights to use, copy, modify, merge, publish, distribute, sublicense, 
// and /or sell copies of the Software, and to permit persons to whom the 
// Software is furnished to do so, subject to the following conditions :
//
// The above copyright noticeand this permission notice shall be included in 
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE 
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS 
// IN THE SOFTWARE.
// ================================================================
// [Additional Statement]
// This Driver is implemented by NVMe Spec 1.3 and Windows Storport Miniport Driver.
// You can copy, modify, redistribute the source code. 
// 
// There is only one requirement to use this source code:
// PLEASE DO NOT remove or modify the "original author" of this codes.
// Keep "original author" declaration unmodified.
// 
// Enjoy it.
// ================================================================


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
inline void FillModePage_Caching(CNvmeDevice* devext, PMODE_CACHING_PAGE page)
{
    page->PageCode = MODE_PAGE_CACHING;
    page->PageLength = (UCHAR)(sizeof(MODE_CACHING_PAGE) - 2); //sizeof(MODE_CACHING_PAGE) - sizeof(page->PageCode) - sizeof(page->PageLength)
    page->ReadDisableCache = !devext->ReadCacheEnabled;
    page->WriteCacheEnable = devext->WriteCacheEnabled;
    page->PageSavable = TRUE;
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
inline ULONG ReplyModePageCaching(CNvmeDevice* devext, PUCHAR& buffer, ULONG& buf_size, ULONG& ret_size)
{
    MODE_CACHING_PAGE page = { 0 };
    ULONG page_size = sizeof(MODE_CACHING_PAGE);
    FillModePage_Caching(devext, &page);

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
inline bool ParseReadWriteOffsetAndLen(CDB& cdb, ULONG64& offset, ULONG& len)
{
    switch (cdb.CDB6GENERIC.OperationCode)
    {
    case SCSIOP_READ6:
    case SCSIOP_WRITE6:
        ParseReadWriteOffsetAndLen(cdb.CDB6READWRITE, offset, len);
        return true;
    case SCSIOP_READ:
    case SCSIOP_WRITE:
        ParseReadWriteOffsetAndLen(cdb.CDB10, offset, len);
        return true;
    case SCSIOP_READ12:
    case SCSIOP_WRITE12:
        ParseReadWriteOffsetAndLen(cdb.CDB12, offset, len);
        return true;
    case SCSIOP_READ16:
    case SCSIOP_WRITE16:
        ParseReadWriteOffsetAndLen(cdb.CDB16, offset, len);
        return true;
    }

    return false;
}
#pragma endregion 