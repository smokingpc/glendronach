#pragma once
void BuiildCmd_ReadWrite(PSPCNVME_SRBEXT srbext, ULONG64 offset, ULONG blocks, bool is_write);
void BuildCmd_IdentCtrler(PSPCNVME_SRBEXT srbext, PNVME_IDENTIFY_CONTROLLER_DATA data);
void BuildCmd_IdentActiveNsidList(PSPCNVME_SRBEXT srbext, PVOID nsid_list, size_t list_size);
void BuildCmd_IdentSpecifiedNS(PSPCNVME_SRBEXT srbext, PNVME_IDENTIFY_NAMESPACE_DATA data, ULONG nsid);
void BuildCmd_IdentAllNSList(PSPCNVME_SRBEXT srbext, PVOID ns_buf, size_t buf_size);
void BuildCmd_SetIoQueueCount(PSPCNVME_SRBEXT srbext, USHORT count);

void BuildCmd_RegIoSubQ(PSPCNVME_SRBEXT srbext, CNvmeQueue* queue);
void BuildCmd_RegIoCplQ(PSPCNVME_SRBEXT srbext, CNvmeQueue* queue);
void BuildCmd_UnRegIoSubQ(PSPCNVME_SRBEXT srbext, CNvmeQueue* queue);
void BuildCmd_UnRegIoCplQ(PSPCNVME_SRBEXT srbext, CNvmeQueue* queue);

void BuildCmd_InterruptCoalescing(PSPCNVME_SRBEXT srbext, UCHAR threshold, UCHAR interval);
void BuildCmd_SetArbitration(PSPCNVME_SRBEXT srbext);

void BuildCmd_GetFirmwareSlotsInfo(PSPCNVME_SRBEXT srbext, PNVME_FIRMWARE_SLOT_INFO_LOG info);
void BuildCmd_GetFirmwareSlotsInfoV1(PSPCNVME_SRBEXT srbext, PNVME_FIRMWARE_SLOT_INFO_LOG info);
