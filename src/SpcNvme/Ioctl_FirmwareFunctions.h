#pragma once

UCHAR Firmware_GetInfo(PSPCNVME_SRBEXT srbext);
UCHAR Firmware_DownloadToAdapter(PSPCNVME_SRBEXT srbext, PSRB_IO_CONTROL ioctl, PFIRMWARE_REQUEST_BLOCK request);
UCHAR Firmware_ActivateSlot(PSPCNVME_SRBEXT srbext, PSRB_IO_CONTROL ioctl, PFIRMWARE_REQUEST_BLOCK request);
