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

enum class SRBEXT_FLAG {
    NONE = 0,
    INIT_OK = 0x00000001,
    IS_COMPLETED = 0x00000002,
    FREE_PRP2_LIST = 0x00000004,
    //DEL_IN_COMPLETE = 0x00000008,
    //IS_READ_IO = 0x00000010,
    //IS_WRITE_IO = 0x00000020,
    //IS_PNP_SRB = 0x00000040,  //this SRB is PnpRequest.
    //IS_SRBEX = 0x80000000,  //indicates SRB in SRBEXT is STORAGE_REQUEST_BLOCK, not SCSI_REQUEST_BLOCK.
    MAX = 0x7FFFFFFF
};

//in VisualC++, there is macro "DEFINE_ENUM_FLAG_OPERATORS" can make enum type support bit-operation.
DEFINE_ENUM_FLAG_OPERATORS(SRBEXT_FLAG);






