#include "pch.h"

static ULONG ReplyFirmwareInfo(
            PNVME_IDENTIFY_CONTROLLER_DATA ctrl,
            PNVME_FIRMWARE_SLOT_INFO_LOG logpage, 
            ULONG max_tx_size,
            PSTORAGE_FIRMWARE_INFO buffer,
            ULONG buf_size)
{
    buffer->ActiveSlot = logpage->AFI.ActiveSlot;
    buffer->PendingActivateSlot = logpage->AFI.PendingActivateSlot;
    buffer->UpgradeSupport = (ctrl->FRMW.SlotCount > 1);
    buffer->SlotCount = ctrl->FRMW.SlotCount;
    if(buf_size >= (sizeof(STORAGE_FIRMWARE_INFO_V2) + 
                sizeof(STORAGE_FIRMWARE_SLOT_INFO_V2)* ctrl->FRMW.SlotCount))
    {
        PSTORAGE_FIRMWARE_INFO_V2 ret_info = (PSTORAGE_FIRMWARE_INFO_V2) buffer;
        ret_info->Version = STORAGE_FIRMWARE_DOWNLOAD_STRUCTURE_VERSION_V2;
        ret_info->Size = sizeof(STORAGE_FIRMWARE_INFO_V2);
        ret_info->FirmwareShared = TRUE;
        
        if(ctrl->FWUG == 0xFF)
            ret_info->ImagePayloadAlignment = sizeof(ULONG);
        else
            ret_info->ImagePayloadAlignment = (ULONG)(PAGE_SIZE * ctrl->FWUG);
        //max size of payload in single piece of download image cmd...
        ret_info->ImagePayloadMaxSize = max_tx_size;

        for(UCHAR i=0; i< ctrl->FRMW.SlotCount; i++)
        {
            PSTORAGE_FIRMWARE_SLOT_INFO_V2 slot = &ret_info->Slot[i];
            slot->ReadOnly = FALSE;
            slot->SlotNumber = i+1;
            RtlZeroMemory(slot->Revision, STORAGE_FIRMWARE_SLOT_INFO_V2_REVISION_LENGTH);
            RtlCopyMemory(slot->Revision, &logpage->FRS[i], sizeof(ULONGLONG));
        }

        if(ctrl->FRMW.Slot1ReadOnly)
            ret_info->Slot[0].ReadOnly = TRUE;
    }
    else 
    {
        PSTORAGE_FIRMWARE_INFO ret_info = (PSTORAGE_FIRMWARE_INFO)buffer;
        ret_info->Version = STORAGE_FIRMWARE_DOWNLOAD_STRUCTURE_VERSION;
        ret_info->Size = sizeof(STORAGE_FIRMWARE_INFO);

        for (UCHAR i = 0; i < ctrl->FRMW.SlotCount; i++)
        {
            PSTORAGE_FIRMWARE_SLOT_INFO slot = &ret_info->Slot[i];
            slot->ReadOnly = FALSE;
            slot->SlotNumber = i + 1;
            slot->Revision.AsUlonglong = logpage->FRS[i];
        }
        if (ctrl->FRMW.Slot1ReadOnly)
            ret_info->Slot[0].ReadOnly = TRUE;
    }

    return FIRMWARE_STATUS_SUCCESS;
}
ULONG Firmware_ReplyInfo(PSPCNVME_SRBEXT srbext, PVOID out_buf, ULONG buf_size)
{
    CAutoPtr<NVME_FIRMWARE_SLOT_INFO_LOG, NonPagedPool, TAG_LOG_PAGE> logpage;
    CNvmeDevice *nvme = srbext->DevExt;
    PSTORAGE_FIRMWARE_INFO reply = (PSTORAGE_FIRMWARE_INFO)out_buf;
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    ULONG ret = FIRMWARE_STATUS_SUCCESS;
    UCHAR total_slots = nvme->CtrlIdent.FRMW.SlotCount;

    if (NULL == out_buf)
        return FIRMWARE_STATUS_INVALID_PARAMETER;

    //Caller should provide V1 structure with enough slot space , at least....
    if (buf_size < (sizeof(STORAGE_FIRMWARE_INFO) +
        sizeof(STORAGE_FIRMWARE_SLOT_INFO)* total_slots))
        return FIRMWARE_STATUS_ILLEGAL_LENGTH;

    logpage.Reset(new NVME_FIRMWARE_SLOT_INFO_LOG());
    if(nvme->NvmeVer.MNR > 0)
        BuildCmd_GetFirmwareSlotsInfo(srbext, (PNVME_FIRMWARE_SLOT_INFO_LOG)logpage);
    else
        BuildCmd_GetFirmwareSlotsInfoV1(srbext, (PNVME_FIRMWARE_SLOT_INFO_LOG)logpage);

    status = nvme->SubmitAdmCmd(srbext, &srbext->NvmeCmd);
    if (!NT_SUCCESS(status))
        return FIRMWARE_STATUS_ERROR;

    do
    {
        StorPortStallExecution(nvme->StallDelay);
    } while (SRB_STATUS_PENDING == srbext->SrbStatus);

    if (SRB_STATUS_SUCCESS != srbext->SrbStatus)
        return FIRMWARE_STATUS_ERROR;

    ret = ReplyFirmwareInfo(&nvme->CtrlIdent, logpage, nvme->MaxTxSize, reply, buf_size);
    return ret;
}
ULONG Firmware_DownloadToAdapter(PSPCNVME_SRBEXT srbext, PVOID in_buf, ULONG buf_size) 
{
    UNREFERENCED_PARAMETER(srbext);
    UNREFERENCED_PARAMETER(in_buf);
    UNREFERENCED_PARAMETER(buf_size);
    return FIRMWARE_STATUS_ILLEGAL_REQUEST;
}
ULONG Firmware_ActivateSlot(PSPCNVME_SRBEXT srbext)
{
    UNREFERENCED_PARAMETER(srbext);
    return FIRMWARE_STATUS_ILLEGAL_REQUEST;
}
