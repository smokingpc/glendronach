#pragma once

BOOLEAN BuildIo_DefaultHandler(PSPCNVME_SRBEXT srbext);
BOOLEAN BuildIo_IoctlHandler(PSPCNVME_SRBEXT srbext);
BOOLEAN BuildIo_ScsiHandler(PSPCNVME_SRBEXT srbext);
BOOLEAN BuildIo_SrbPowerHandler(PSPCNVME_SRBEXT srbext);
BOOLEAN BuildIo_SrbPnpHandler(PSPCNVME_SRBEXT srbext);

