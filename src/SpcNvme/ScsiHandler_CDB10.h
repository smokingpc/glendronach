#pragma once

UCHAR Scsi_Read10(PSPCNVME_SRBEXT srbext);
UCHAR Scsi_Write10(PSPCNVME_SRBEXT srbext);
UCHAR Scsi_ReadCapacity10(PSPCNVME_SRBEXT srbext);
UCHAR Scsi_Verify10(PSPCNVME_SRBEXT srbext);
UCHAR Scsi_ModeSelect10(PSPCNVME_SRBEXT srbext);
UCHAR Scsi_ModeSense10(PSPCNVME_SRBEXT srbext);
