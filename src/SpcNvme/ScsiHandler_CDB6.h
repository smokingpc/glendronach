#pragma once

UCHAR Scsi_RequestSense6(PSPCNVME_SRBEXT srbext);
UCHAR Scsi_Read6(PSPCNVME_SRBEXT srbext);
UCHAR Scsi_Write6(PSPCNVME_SRBEXT srbext);
UCHAR Scsi_Inquiry6(PSPCNVME_SRBEXT srbext);
UCHAR Scsi_Verify6(PSPCNVME_SRBEXT srbext);
UCHAR Scsi_ModeSelect6(PSPCNVME_SRBEXT srbext);
UCHAR Scsi_ModeSense6(PSPCNVME_SRBEXT srbext);
UCHAR Scsi_TestUnitReady(PSPCNVME_SRBEXT srbext);
