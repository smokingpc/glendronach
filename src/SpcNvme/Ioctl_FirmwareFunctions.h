#pragma once
ULONG Firmware_ReplyInfo(PSPCNVME_SRBEXT srbext, PVOID out_buf, ULONG buf_size);
ULONG Firmware_DownloadToAdapter(PSPCNVME_SRBEXT srbext, PVOID in_buf, ULONG buf_size);
ULONG Firmware_ActivateSlot(PSPCNVME_SRBEXT srbext);
