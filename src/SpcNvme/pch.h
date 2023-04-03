#pragma once

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
#include "Inline_Utils.h"
#include "NvmeConstants.h"
#include "NvmeEnums.h"
#include "PCIe_Msix.h"
#include "SrbExt.h"
#include "NvmeQueue.h"
#include "NvmeDevice.h"
#include "MiniportFunctions.h"
#include "NvmePrpBuilder.h"
#include "NvmeCmdBuilder.h"
#include "BuildIo_Handlers.h"
#include "StartIo_Handler.h"
#include "ISR_and_DPC.h"

#include "Scsi_Utils.h"
#include "ScsiHandler_CDB6.h"
#include "ScsiHandler_CDB10.h"
#include "ScsiHandler_CDB12.h"
#include "ScsiHandler_CDB16.h"
#include "ScsiHandler_InlineUtils.h"

using SPC::CAutoPtr;
