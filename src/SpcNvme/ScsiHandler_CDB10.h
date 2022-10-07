#pragma once

UCHAR Scsi_Read10(SPCNVME_SRBEXT srbext);
UCHAR Scsi_Write10(SPCNVME_SRBEXT srbext);
UCHAR Scsi_ReadCapacity10(SPCNVME_SRBEXT srbext);
UCHAR Scsi_Verify10(SPCNVME_SRBEXT srbext);
UCHAR Scsi_ModeSelect10(SPCNVME_SRBEXT srbext);
UCHAR Scsi_ModeSense10(SPCNVME_SRBEXT srbext);
