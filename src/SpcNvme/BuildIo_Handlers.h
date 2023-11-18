#pragma once

UCHAR BuildIo_DefaultHandler(PSPCNVME_SRBEXT srbext);
UCHAR BuildIo_IoctlHandler(PSPCNVME_SRBEXT srbext);
UCHAR BuildIo_ScsiHandler(PSPCNVME_SRBEXT srbext);
UCHAR BuildIo_SrbPowerHandler(PSPCNVME_SRBEXT srbext);
UCHAR BuildIo_SrbPnpHandler(PSPCNVME_SRBEXT srbext);

