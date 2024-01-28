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


#include <string.h>
#include <ntifs.h>
#include <wdm.h>
#include <ntddscsi.h>
#include <ntdddisk.h>
#include <nvme.h>

EXTERN_C_START
#include <storport.h>
#include <srbhelper.h>
EXTERN_C_END

#include "..\WdmUtils\WdmUtils.h"
#include "..\WdmUtils\AutoPtr.hpp"
#include "PoolTags.h"
#include "Constants.h"
#include "PCIe_Msix.h"
#include "Inline_Utils.h"
#include "NvmeEnums.h"
#include "SrbExt.h"
#include "NvmeAsyncEventLog.h"
#include "NvmeQueue.h"
#include "NvmeDevice.h"
#include "MiniportFunctions.h"
#include "NvmePrpBuilder.h"
#include "NvmeCmdBuilder.h"
#include "BuildIo_Handlers.h"
#include "StartIo_Handler.h"

#include "Scsi_Utils.h"
#include "ScsiHandler_CDB6.h"
#include "ScsiHandler_CDB10.h"
#include "ScsiHandler_CDB12.h"
#include "ScsiHandler_CDB16.h"
#include "ScsiHandler_InlineUtils.h"

#include "IoctlScsiMiniport_Handlers.h"
#include "IoctlStorageQuery_Handlers.h"
#include "Ioctl_FirmwareFunctions.h"
#include "DevExt.h"

using SPC::CAutoPtr;
