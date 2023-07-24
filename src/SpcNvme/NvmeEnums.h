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


typedef enum class _QUEUE_TYPE
{
    UNKNOWN = 0,
    ADM_QUEUE = 1,
    IO_QUEUE = 2,
}QUEUE_TYPE;

_Enum_is_bitflag_
typedef enum class _NVME_CMD_TYPE : UINT32
{
    UNKNOWN = 0,
    ADM_CMD = 1,
    IO_CMD = 2,
//    SRB_CMD = 0x00001000,           //this command use SRB , no wait event. using regular SRB handling
//    WAIT_CMD = 0x00002000,          //this command is internal cmd and waiting for event signal
    SELF_ISSUED = 0x80000000,           //this command issued by SpcNvme.sys myself.
}NVME_CMD_TYPE;

typedef enum class _CMD_CTX_TYPE
{
    UNKNOWN = 0,
    WAIT_EVENT = 1,
    LOCAL_ADM_CMD = 2,
    SRBEXT = 3
}CMD_CTX_TYPE;

typedef enum class _IDENTIFY_CNS : UCHAR
{
    IDENT_NAMESPACE = 0,
    IDENT_CONTROLLER = 1,

}IDENTIFY_CNS;

//typedef enum _USE_STATE
//{
//    FREE = 0,
//    USED = 1,
//}USE_STATE;

typedef enum _NVME_STATE {
    STOP = 0,
    RUNNING = 1,
    SETUP = 2,
    INIT = 3,       //In InitStage1 and InitStage2
    TEARDOWN = 4,
    RESETCTRL = 5,
    RESETBUS = 6,
    SHUTDOWN = 10,   //CC.SHN==1
}NVME_STATE;

