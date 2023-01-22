#pragma once

__inline PSPCNVME_SRBEXT GetSrbExt(PSTORAGE_REQUEST_BLOCK srb)
{
    return (PSPCNVME_SRBEXT)SrbGetMiniportContext(srb);
}

void InitInternalSrbExt(PVOID devext, PSPCNVME_SRBEXT srbext);
void InitSrbExt(PVOID devext, PSPCNVME_SRBEXT srbext, PSTORAGE_REQUEST_BLOCK srb);
PSPCNVME_SRBEXT InitAndGetSrbExt(PVOID devext, PSTORAGE_REQUEST_BLOCK srb);
void SetScsiSenseBySrbStatus(PSTORAGE_REQUEST_BLOCK srb, UCHAR srb_status);

UCHAR ToSrbStatus(NVME_COMMAND_STATUS status);
UCHAR NvmeGenericToSrbStatus(NVME_COMMAND_STATUS status);
UCHAR NvmeCmdSpecificToSrbStatus(NVME_COMMAND_STATUS status);
UCHAR NvmeMediaErrorToSrbStatus(NVME_COMMAND_STATUS status);
