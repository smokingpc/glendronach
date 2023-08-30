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


namespace NVME_CONST{
    static const char* VENDOR_ID = "SPC     ";           //vendor name
    static const char* PRODUCT_ID = "SomkingPC NVMe  ";  //model name
    static const char* PRODUCT_REV = "0100";

    static const ULONG TX_PAGES = 512; //PRP1 + PRP2 MAX PAGES
    static const ULONG TX_SIZE = PAGE_SIZE * TX_PAGES;
    static const UCHAR SUPPORT_NAMESPACES = 4;
    static const ULONG UNSPECIFIC_NSID = 0;
    static const ULONG DEFAULT_CTRLID = 1;
    static const UCHAR MAX_TARGETS = 1;
    static const UCHAR MAX_LU = 1;
    static const ULONG MAX_IO_PER_LU = 512;
    static const UCHAR IOSQES = 6; //sizeof(NVME_COMMAND)==64 == 2^6, so IOSQES== 6.
    static const UCHAR IOCQES = 4; //sizeof(NVME_COMPLETION_ENTRY)==16 == 2^4, so IOCQES== 4.
    static const UCHAR ADMIN_QUEUE_DEPTH = 64;  //how many entries does the admin queue have?
    
    static const USHORT CPL_INIT_PHASETAG = 1;  //CompletionQueue phase tag init value;
    static const UCHAR IO_QUEUE_COUNT = 16;
    static const USHORT IO_QUEUE_DEPTH = MAX_IO_PER_LU / 2;
    static const ULONG DEFAULT_MAX_TXSIZE = 131072;
    static const USHORT MAX_CID = MAXUSHORT-1;  //0xFFFF is invalid 
    static const ULONG MAX_NS_COUNT = 1024;     //Max NameSpace count. defined in NVMe spec.
    //static const ULONG MAX_CONCURRENT_IO = 1024;
    //static const USHORT SQ_CMD_SIZE = sizeof(NVME_COMMAND);
    //static const USHORT SQ_CMD_SIZE_SHIFT = 6; //sizeof(NVME_COMMAND) is 64 bytes == 2^6
    static const ULONG CPL_ALL_ENTRY = MAXULONG;
    static const ULONG INIT_DBL_VALUE = 0;
    static const ULONG INVALID_DBL_VALUE = (ULONG)MAXUSHORT;
    static const ULONG ISR_HANDLED_CPL = 8;     //how many cpl entries will be handled in each ISR call?
    static const ULONG MAX_INT_COUNT = 64;
    static const ULONG SAFE_SUBMIT_THRESHOLD = 8;
    static const ULONG STALL_TIME_US = 100;  //in micro-seconds
    static const ULONG SLEEP_TIME_US = STALL_TIME_US*5;    //in micro-seconds

    #pragma region ======== SetFeature default values ========
    static const UCHAR INTCOAL_TIME = 2;   //Interrupt Coalescing time threshold in 100us unit.
    static const UCHAR INTCOAL_THRESHOLD = 8;   //Interrupt Coalescing trigger threshold.

    static const UCHAR AB_BURST = 7;        //Arbitration Burst. 111b(0n7) is Unlimit.
    static const UCHAR AB_HPW = 128 - 1;     //High Priority Weight. it is 0-based so need substract 1.
    static const UCHAR AB_MPW = 64 - 1;     //Medium Priority Weight. it is 0-based so need substract 1.
    static const UCHAR AB_LPW = 32 - 1;      //Low Priority Weight. it is 0-based so need substract 1.

    #pragma endregion
};
