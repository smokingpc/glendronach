#pragma once

UCHAR Scsi_ReportLuns12(SPCNVME_SRBEXT srbext);
UCHAR Scsi_Read12(SPCNVME_SRBEXT srbext);
UCHAR Scsi_Write12(SPCNVME_SRBEXT srbext);
UCHAR Scsi_Verify12(SPCNVME_SRBEXT srbext);
