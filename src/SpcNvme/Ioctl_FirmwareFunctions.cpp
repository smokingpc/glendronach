#include "pch.h"
SPC_SRBEXT_COMPLETION Complete_FirmwareInfo;

static ULONG NvmeStatus2FirmwareStatus(NVME_COMMAND_STATUS *status)
{
    if(NVME_STATUS_TYPE_GENERIC_COMMAND == status->SCT &&
        NVME_STATUS_SUCCESS_COMPLETION == status->SC)
        return FIRMWARE_STATUS_SUCCESS;

    if(status->SCT == NVME_STATUS_TYPE_COMMAND_SPECIFIC)
    {
        switch(status->SC)
        {
        case NVME_STATUS_INVALID_LOG_PAGE:
            return FIRMWARE_STATUS_ILLEGAL_REQUEST;
        default:
            return FIRMWARE_STATUS_ERROR;
        }
    }

    return FIRMWARE_STATUS_CONTROLLER_ERROR;        
}

static void FillFirmwareInfoV2(
    CNvmeDevice* nvme,
    PNVME_FIRMWARE_SLOT_INFO_LOG logpage,
    PSTORAGE_FIRMWARE_INFO_V2 ret_info)
{
    PNVME_IDENTIFY_CONTROLLER_DATA ctrl = &nvme->CtrlIdent;

    ret_info->ActiveSlot = logpage->AFI.ActiveSlot;
    ret_info->PendingActivateSlot = logpage->AFI.PendingActivateSlot;
    ret_info->UpgradeSupport = (ctrl->FRMW.SlotCount > 1);
    ret_info->SlotCount = ctrl->FRMW.SlotCount;
    ret_info->Version = STORAGE_FIRMWARE_DOWNLOAD_STRUCTURE_VERSION_V2;
    ret_info->Size = sizeof(STORAGE_FIRMWARE_INFO_V2);
    ret_info->FirmwareShared = TRUE;

    if (ctrl->FWUG == 0xFF || 0 == ctrl->FWUG)
        ret_info->ImagePayloadAlignment = sizeof(ULONG);
    else
        ret_info->ImagePayloadAlignment = (ULONG)(PAGE_SIZE * ctrl->FWUG);
    //max size of payload in single piece of download image cmd...
    //refer to https://learn.microsoft.com/en-us/windows-hardware/drivers/storage/upgrading-firmware-for-an-nvme-device
    ret_info->ImagePayloadMaxSize = min(nvme->MaxTxSize, (128 * PAGE_SIZE));

    for (UCHAR i = 0; i < ctrl->FRMW.SlotCount; i++)
    {
        PSTORAGE_FIRMWARE_SLOT_INFO_V2 slot = &ret_info->Slot[i];
        slot->ReadOnly = FALSE;
        slot->SlotNumber = i + 1;   //slot id is 1-based.
        RtlZeroMemory(slot->Revision, STORAGE_FIRMWARE_SLOT_INFO_V2_REVISION_LENGTH);
        RtlCopyMemory(slot->Revision, &logpage->FRS[i], sizeof(ULONGLONG));
    }

    if (ctrl->FRMW.Slot1ReadOnly)
        ret_info->Slot[0].ReadOnly = TRUE;
}

static void FillFirmwareInfoV1(
    CNvmeDevice* nvme,
    PNVME_FIRMWARE_SLOT_INFO_LOG logpage,
    PSTORAGE_FIRMWARE_INFO ret_info)
{
    PNVME_IDENTIFY_CONTROLLER_DATA ctrl = &nvme->CtrlIdent;

    ret_info->Version = STORAGE_FIRMWARE_DOWNLOAD_STRUCTURE_VERSION;
    ret_info->Size = sizeof(STORAGE_FIRMWARE_INFO);
    ret_info->ActiveSlot = logpage->AFI.ActiveSlot;
    ret_info->PendingActivateSlot = logpage->AFI.PendingActivateSlot;
    ret_info->UpgradeSupport = (ctrl->FRMW.SlotCount > 1);
    ret_info->SlotCount = ctrl->FRMW.SlotCount;

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

VOID Complete_FirmwareInfo(SPCNVME_SRBEXT *srbext)
{
    CNvmeDevice* nvme = srbext->DevExt;
    PSRB_IO_CONTROL ioctl = (PSRB_IO_CONTROL)srbext->DataBuf();
    PFIRMWARE_REQUEST_BLOCK request = (PFIRMWARE_REQUEST_BLOCK)(ioctl + 1);
    PSTORAGE_FIRMWARE_INFO buffer = (PSTORAGE_FIRMWARE_INFO)((PUCHAR)ioctl + request->DataBufferOffset);
    PNVME_FIRMWARE_SLOT_INFO_LOG logpage = (PNVME_FIRMWARE_SLOT_INFO_LOG)srbext->ExtBuf;
    PNVME_IDENTIFY_CONTROLLER_DATA ctrl = &nvme->CtrlIdent;
    ULONG buf_len = request->DataBufferLength;
    UCHAR total_slots = ctrl->FRMW.SlotCount;
    UCHAR srb_status = SRB_STATUS_INTERNAL_ERROR;
    ULONG fw_status = FIRMWARE_STATUS_ERROR;

    if (0 == request->DataBufferOffset)
    {
        fw_status = FIRMWARE_STATUS_OUTPUT_BUFFER_TOO_SMALL;
        srb_status = SRB_STATUS_INVALID_PARAMETER;
        goto END;
    }

    //Caller should provide V1 structure with enough slot space
    if (buf_len < (sizeof(STORAGE_FIRMWARE_INFO) + (sizeof(STORAGE_FIRMWARE_SLOT_INFO) * total_slots)))
    {
        fw_status = FIRMWARE_STATUS_OUTPUT_BUFFER_TOO_SMALL;
        srb_status = SRB_STATUS_INVALID_PARAMETER;
        goto END;
    }
    //translate NVME status code to firmware status code
    fw_status = NvmeStatus2FirmwareStatus(&srbext->NvmeCpl.DW3.Status);
    if(FIRMWARE_STATUS_SUCCESS != fw_status)
        goto END;

    if (buf_len >= (sizeof(STORAGE_FIRMWARE_INFO_V2) +
        sizeof(STORAGE_FIRMWARE_SLOT_INFO_V2) * ctrl->FRMW.SlotCount))
    {
        FillFirmwareInfoV2(nvme, logpage, (PSTORAGE_FIRMWARE_INFO_V2)buffer);
    }
    else
    {
        FillFirmwareInfoV1(nvme, logpage, buffer);
    }
    fw_status = FIRMWARE_STATUS_SUCCESS;
    srb_status = SRB_STATUS_SUCCESS;

END:
    ioctl->ReturnCode = fw_status;
    srbext->CleanUp();
    srbext->CompleteSrbWithStatus(srb_status);
}

UCHAR Firmware_GetInfo(PSPCNVME_SRBEXT srbext)
{
    PNVME_FIRMWARE_SLOT_INFO_LOG logpage = NULL;
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    logpage = new NVME_FIRMWARE_SLOT_INFO_LOG();
    srbext->ResetExtBuf(logpage);
    srbext->CompletionCB = Complete_FirmwareInfo;

    if(srbext->DevExt->NvmeVer.MNR > 0)
        BuildCmd_GetFirmwareSlotsInfo(srbext, logpage);
    else
        BuildCmd_GetFirmwareSlotsInfoV1(srbext, logpage);

    status = srbext->DevExt->SubmitAdmCmd(srbext, &srbext->NvmeCmd);
    if (!NT_SUCCESS(status))
    {
        delete logpage;
        return SRB_STATUS_INTERNAL_ERROR;
    }
    
    return SRB_STATUS_PENDING;
}
UCHAR Firmware_DownloadToAdapter(PSPCNVME_SRBEXT srbext, PSRB_IO_CONTROL ioctl, PFIRMWARE_REQUEST_BLOCK request)
{
    UNREFERENCED_PARAMETER(srbext);
    UNREFERENCED_PARAMETER(ioctl);
    UNREFERENCED_PARAMETER(request);
    return SRB_STATUS_INVALID_REQUEST;
}
UCHAR Firmware_ActivateSlot(PSPCNVME_SRBEXT srbext, PSRB_IO_CONTROL ioctl, PFIRMWARE_REQUEST_BLOCK request)
{
    UNREFERENCED_PARAMETER(srbext);
    UNREFERENCED_PARAMETER(ioctl);
    UNREFERENCED_PARAMETER(request);
    return SRB_STATUS_INVALID_REQUEST;
}
