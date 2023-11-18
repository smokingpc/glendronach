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


#define NVME_INVALID_ID     (MAXULONG)
#define NVME_INVALID_CID    (MAXUSHORT)        //should align to NVME CID size
#define NVME_INVALID_QID    (MAXUSHORT)

#define MAX_LOGIC_UNIT      1
#define MAX_IO_QUEUE_COUNT  64
#define ACCESS_RANGE_COUNT  2
//const char* SpcVendorID = "SPC     ";           //vendor name
//const char* SpcProductID = "SomkingPC NVMe  ";  //model name
//const char* SpcProductRev = "0100";

#pragma region  ======== SCSI and SRB ========
#define SRB_FUNCTION_SPC_INTERNAL   0xFF
#define INVALID_PORT_ID      (MAXUSHORT)
#define INVALID_PATH_ID      (MAXUCHAR)
#define INVALID_TARGET_ID    (MAXUCHAR)
#define INVALID_LUN_ID       (MAXUCHAR)
#define INVALID_SCSI_TAG     (MAXULONG)
#define INVALID_SRB_QUEUETAG (MAXULONG)
#define SCSI_TAG_SHIFT       10
#define MAX_SCSI_TARGETS        1
#define MAX_SCSI_LOGICAL_UNIT   1
#define MAX_SCSI_LU_SHIFT       5
static_assert(((1<< MAX_SCSI_LU_SHIFT) >= MAX_SCSI_LOGICAL_UNIT), 
                    "Incorrect definition of MAX_SCSI_LU_SHIFT");
#pragma endregion
#pragma region  ======== NVME ========
#define DEFAULT_INT_COALESCE_COUNT  0
#define DEFAULT_INT_COALESCE_TIME   0    //in 100us unit
#define MAX_ADM_CMD_COUNT           256
//#define ADM_CMD_CID_FLAG        (USHORT)0x8000
#pragma endregion

#pragma region  ======== STORPORT MINIPORT ========
#define MAX_IO_PER_LU           (1<<SCSI_TAG_SHIFT)-1//1024-1 = 1023 
#pragma endregion

#pragma region  ======== REGISTRY ========
#define REGNAME_ADMQ_DEPTH      (UCHAR*)"AdmQDepth"
#define REGNAME_IOQ_DEPTH       (UCHAR*)"IoQDepth"
#define REGNAME_IOQ_COUNT       (UCHAR*)"IoQCount"
#define REGNAME_COALESCE_TIME   (UCHAR*)"IntCoalescingTime"
#define REGNAME_COALESCE_COUNT  (UCHAR*)"IntCoalescingEntries"


#pragma endregion
