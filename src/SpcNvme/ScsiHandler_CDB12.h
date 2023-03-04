#pragma once

UCHAR Scsi_ReportLuns12(PSPCNVME_SRBEXT srbext);
UCHAR Scsi_Read12(PSPCNVME_SRBEXT srbext);
UCHAR Scsi_Write12(PSPCNVME_SRBEXT srbext);
UCHAR Scsi_Verify12(PSPCNVME_SRBEXT srbext);
