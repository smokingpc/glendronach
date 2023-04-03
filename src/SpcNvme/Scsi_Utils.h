#pragma once
UCHAR Scsi_ReadWrite(PSPCNVME_SRBEXT srbext, ULONG64 offset, ULONG len, bool is_write);
UCHAR NvmeStatus2SrbStatus(NVME_COMMAND_STATUS* status);
