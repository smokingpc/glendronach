#pragma once

#include <wdm.h>
#include <nvme.h>

EXTERN_C_START
#include <storport.h>
#include <srbhelper.h>
EXTERN_C_END

#include "..\WdmUtils\WdmUtils.h"
#include "..\WdmUtils\UniquePtr.hpp"
#include "PoolTags.h"
#include "Constants.h"
#include "Inline_Utils.h"
#include "StateMachine.h"
#include "MiniportContext.h"
#include "MiniportFunctions.h"
#include "NvmeQueue.h"
#include "NvmeDevice.h"

#include "Srb_Utils.h"
#include "BuildIo_Handlers.h"