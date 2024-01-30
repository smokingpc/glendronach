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
#pragma endregion
#pragma region  ======== NVME ========
#define DEFAULT_MAX_TXSIZE          (256 * PAGE_SIZE)
#define DEFAULT_INT_COALESCE_COUNT  0
#define DEFAULT_INT_COALESCE_TIME   0    //in 100us unit
#define MAX_ADM_CMD_COUNT           256
#define SUBQ_ENTRY_SIZE             6   //sizeof(NVME_COMMAND)==64 == 2^6, so IOSQES== 6.
#define CPLQ_ENTRY_SIZE             4   //sizeof(NVME_COMPLETION_ENTRY)==16 == 2^4, so IOCQES== 4.
#define INIT_DBL_VALUE              0
#define INVALID_DBL_VALUE           ((ULONG)MAXUSHORT)
#define CPL_INIT_PHASETAG           1
#define MAX_INT_COUNT               64
#define SUPPORT_NAMESPACES          1
#define SAFE_SUBMIT_THRESHOLD       8
#define MAX_IO_QUEUE_COUNT          32
#define STALL_TIME_US               100     //in micro-seconds
#define SLEEP_TIME_US               (STALL_TIME_US*10)
#define UNSPECIFIC_NSID             0
#define DEFAULT_CTRLID              1
#define ADMIN_QUEUE_DEPTH           64
#define IO_QUEUE_DEPTH              256
#define MAX_ASYNC_EVENT_LOG         (1<<4)  //max 16 logs.
#define MAX_ASYNC_EVENT_LOGPAGES    MAX_ASYNC_EVENT_LOG
#define CELSIUS_ZERO_TO_KELVIN      (273)
#define KELVIN_ZERO_TO_CELSIUS      (-273)
#define BAR0_LOWPART_MASK           (0xFFFFC000)
#define MAX_NS_COUNT                1024    //Max NameSpace count. defined in NVMe spec.
#define INTCOAL_TIME                2       //Interrupt Coalescing time threshold in 100us unit.
#define INTCOAL_THRESHOLD           8       //Interrupt Coalescing trigger threshold.
#define AB_BURST                    7       //Arbitration Burst. 111b(0n7) is Unlimit.
#define AB_HPW                      (128-1) //High Priority Weight. it is 0-based so need substract 1.
#define AB_MPW                      (64-1)  //Medium Priority Weight. it is 0-based so need substract 1.
#define AB_LPW                      (32-1)  //Low Priority Weight. it is 0-based so need substract 1.
#pragma endregion

#pragma region  ======== STORPORT MINIPORT ========
#define MAX_IO_PER_LU           (1<<SCSI_TAG_SHIFT)-1//1024-1 = 1023 
#define ABOVE_4G_ADDR           ((LONGLONG)1<<32)
#define VENDOR_ID               "SPC     "          //vendor name
#define PRODUCT_ID              "SomkingPC NVMe  "  //model name
#define PRODUCT_REV             "0100"
#define UNCACHED_EXT_SIZE       (21*PAGE_SIZE)
#define ACCESS_RANGE_COUNT      2
#pragma endregion

#pragma region  ======== REGISTRY ========
#define REGNAME_ADMQ_DEPTH      (UCHAR*)"AdmQDepth"
#define REGNAME_IOQ_DEPTH       (UCHAR*)"IoQDepth"
#define REGNAME_IOQ_COUNT       (UCHAR*)"IoQCount"
#define REGNAME_COALESCE_TIME   (UCHAR*)"IntCoalescingTime"
#define REGNAME_COALESCE_COUNT  (UCHAR*)"IntCoalescingEntries"
#pragma endregion
