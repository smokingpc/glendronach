#pragma once

UCHAR BuildIo_DefaultHandler(PSPC_SRBEXT srbext);
UCHAR BuildIo_IoctlHandler(PSPC_SRBEXT srbext);
UCHAR BuildIo_ScsiHandler(PSPC_SRBEXT srbext);
UCHAR BuildIo_SrbPowerHandler(PSPC_SRBEXT srbext);
UCHAR BuildIo_SrbPnpHandler(PSPC_SRBEXT srbext);

