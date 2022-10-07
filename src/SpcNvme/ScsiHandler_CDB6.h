#pragma once

UCHAR Scsi_RequestSense6(SPCNVME_SRBEXT srbext);
UCHAR Scsi_Read6(SPCNVME_SRBEXT srbext);
UCHAR Scsi_Write6(SPCNVME_SRBEXT srbext);
UCHAR Scsi_Inquiry6(SPCNVME_SRBEXT srbext);
UCHAR Scsi_Verify6(SPCNVME_SRBEXT srbext);
UCHAR Scsi_ModeSelect6(SPCNVME_SRBEXT srbext);
UCHAR Scsi_ModeSense6(SPCNVME_SRBEXT srbext);

