#include "pch.h"
#include "IoctlScsiMiniport_Handlers.h"

UCHAR IoctlScsiMiniport_Firmware(PSPCNVME_SRBEXT srbext, PSRB_IO_CONTROL ioctl)
{
    ULONG data_len = srbext->DataBufLen();
    PFIRMWARE_REQUEST_BLOCK request = (PFIRMWARE_REQUEST_BLOCK)(ioctl + 1);
    UCHAR srb_status = SRB_STATUS_INVALID_REQUEST;
    if (data_len < (sizeof(SRB_IO_CONTROL) + sizeof(FIRMWARE_REQUEST_BLOCK)))
    {
        ioctl->ReturnCode = FIRMWARE_STATUS_OUTPUT_BUFFER_TOO_SMALL;
        return SRB_STATUS_INVALID_PARAMETER;
    }

    //refer to https://docs.microsoft.com/en-us/windows-hardware/drivers/storage/upgrading-firmware-for-an-nvme-device
    //the input buffer layout should be [SRB_IO_CONTROL][FIRMWARE_REQUEST_BLOCK]
    //output buffer layout should be [SRB_IO_CONTROL][FIRMWARE_REQUEST_BLOCK][RET BUFFER]
    //all of them are located in srb data buffer.
    switch (request->Function)
    {
    case FIRMWARE_FUNCTION_GET_INFO:
        srb_status = Firmware_GetInfo(srbext);
        break;
    case FIRMWARE_FUNCTION_DOWNLOAD:
        //srb_status = Firmware_DownloadToAdapter(srbext, ioctl, request);
        srb_status = SRB_STATUS_INVALID_REQUEST;
        break;
    case FIRMWARE_FUNCTION_ACTIVATE:
        //srb_status = Firmware_ActivateSlot(srbext, ioctl, request);
        srb_status = SRB_STATUS_INVALID_REQUEST;
        break;
    }

    //In Firmware commands, SRBSTATUS only indicates 
    //"is this command sent to device successfully?"
    //The result of command stored in ioctl->ReturnCode.
    return srb_status;
}
