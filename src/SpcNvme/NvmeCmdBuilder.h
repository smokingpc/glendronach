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
// Please keep my name in "author" field.
// 
// Enjoy it.
// ================================================================


void BuiildCmd_ReadWrite(PSPC_SRBEXT srbext, ULONG64 offset, ULONG blocks, bool is_write);
void BuildCmd_IdentCtrler(PSPC_SRBEXT srbext, PNVME_IDENTIFY_CONTROLLER_DATA data);
void BuildCmd_IdentActiveNsidList(PSPC_SRBEXT srbext, PVOID nsid_list, size_t list_size);
void BuildCmd_IdentSpecifiedNS(PSPC_SRBEXT srbext, PNVME_IDENTIFY_NAMESPACE_DATA data, ULONG nsid);
void BuildCmd_IdentAllNSList(PSPC_SRBEXT srbext, PVOID ns_buf, size_t buf_size);
void BuildCmd_SetIoQueueCount(PSPC_SRBEXT srbext, USHORT count);

void BuildCmd_RegIoSubQ(PSPC_SRBEXT srbext, CNvmeQueue* queue);
void BuildCmd_RegIoCplQ(PSPC_SRBEXT srbext, CNvmeQueue* queue);
void BuildCmd_UnRegIoSubQ(PSPC_SRBEXT srbext, CNvmeQueue* queue);
void BuildCmd_UnRegIoCplQ(PSPC_SRBEXT srbext, CNvmeQueue* queue);

void BuildCmd_InterruptCoalescing(PSPC_SRBEXT srbext, UCHAR threshold, UCHAR interval);
void BuildCmd_SetArbitration(PSPC_SRBEXT srbext);
void BuildCmd_SyncHostTime(PSPC_SRBEXT srbext, LARGE_INTEGER& timestamp);
void BuildCmd_SetAsyncEvent(PSPC_SRBEXT srbext);
void BuildCmd_GetFirmwareSlotsInfo(PSPC_SRBEXT srbext, PNVME_FIRMWARE_SLOT_INFO_LOG info);
void BuildCmd_GetFirmwareSlotsInfoV1(PSPC_SRBEXT srbext, PNVME_FIRMWARE_SLOT_INFO_LOG info);

void BuildCmd_AdminSecuritySend(PSPC_SRBEXT srbext, ULONG nsid, PCDB cdb);
void BuildCmd_AdminSecurityRecv(PSPC_SRBEXT srbext, ULONG nsid, PCDB cdb);

void BuildCmd_RequestAsyncEvent(PSPC_SRBEXT srbext);
void BuildCmd_GetLogPage(PSPC_SRBEXT srbext, UCHAR log_id, PVOID log_buf, UINT32 buf_size);
void BuildCmd_GetLogPageV13(PSPC_SRBEXT srbext, UCHAR log_id, PVOID log_buf, UINT32 buf_size);
void BuildCmd_SetVolatileWriteCache(PSPC_SRBEXT srbext);
void BuildCmd_Flush(PSPC_SRBEXT srbext, ULONG nsid);
