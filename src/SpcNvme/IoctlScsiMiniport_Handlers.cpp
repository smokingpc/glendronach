#include "pch.h"
#include "IoctlScsiMiniport_Handlers.h"

UCHAR IoctlScsiMiniport_Firmware(PSPCNVME_SRBEXT srbext, PSRB_IO_CONTROL ioctl)
{
    ULONG data_len = srbext->DataBufLen();
    PFIRMWARE_REQUEST_BLOCK request = (PFIRMWARE_REQUEST_BLOCK)(ioctl + 1);
    ULONG fw_status = FIRMWARE_STATUS_ERROR;
    if (data_len < (sizeof(SRB_IO_CONTROL) + sizeof(FIRMWARE_REQUEST_BLOCK)))
    {
        ioctl->ReturnCode = FIRMWARE_STATUS_OUTPUT_BUFFER_TOO_SMALL;
        return SRB_STATUS_BAD_SRB_BLOCK_LENGTH;
    }

    //refer to https://docs.microsoft.com/en-us/windows-hardware/drivers/storage/upgrading-firmware-for-an-nvme-device
    //the input buffer layout should be [SRB_IO_CONTROL][FIRMWARE_REQUEST_BLOCK]
    //output buffer layout should be [SRB_IO_CONTROL][FIRMWARE_REQUEST_BLOCK][CMD BUFFER]
    //all of them are located in srb data buffer.
    PVOID buffer = ((PUCHAR)ioctl) + request->DataBufferOffset;
    ULONG buf_len = request->DataBufferLength;
    switch (request->Function)
    {
    case FIRMWARE_FUNCTION_GET_INFO:
        fw_status = Firmware_ReplyInfo(srbext, buffer, buf_len);
        break;
    case FIRMWARE_FUNCTION_DOWNLOAD:
        fw_status = Firmware_DownloadToAdapter(srbext, buffer, buf_len);
        break;
    case FIRMWARE_FUNCTION_ACTIVATE:
        fw_status = Firmware_ActivateSlot(srbext);
        break;
    }

    //FIRMWARE_STATUS_ERROR
    ioctl->ReturnCode = fw_status;
    //In Firmware commands, SRBSTATUS only indicates 
    //"is this command sent to device successfully?"
    //The result of command stored in ioctl->ReturnCode.
    return SRB_STATUS_SUCCESS;
}
