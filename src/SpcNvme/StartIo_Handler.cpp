#include "pch.h"
#include "ScsiHandler_CDB6.h"
#include "ScsiHandler_CDB10.h"
#include "ScsiHandler_CDB12.h"
#include "ScsiHandler_CDB16.h"
#include "ScsiHandler_InlineUtils.h"
#include "IoctlScsiMiniport_Handlers.h"
#include "IoctlStorageQuery_Handlers.h"

UCHAR StartIo_DefaultHandler(PSPCNVME_SRBEXT srbext)
{
    //SetScsiSenseBySrbStatus(srbext->Srb, SRB_STATUS_INVALID_REQUEST);
    //StorPortNotification(RequestComplete, srbext->DevExt, srbext->Srb);
    return SRB_STATUS_ERROR;
}
UCHAR StartIo_ScsiHandler(PSPCNVME_SRBEXT srbext)
{
    //SetScsiSenseBySrbStatus(srbext->Srb, SRB_STATUS_INVALID_REQUEST);
    //StorPortNotification(RequestComplete, srbext->DevExt, srbext->Srb);




    return SRB_STATUS_ERROR;
}
UCHAR StartIo_IoctlHandler(PSPCNVME_SRBEXT srbext)
{
    //SRB_FUNCTION_IO_CONTROL handles serveral kinds (groups) of IOCTL:
    //1. IOCTL_SCSI_MINIPORT_* ioctl codes.
    //2. IOCTL_STORAGE_* ioctl codes
    //3. custom made ioctl codes.
    //4. .....so on.....
    //All of them use SRB_IO_CONTROL as input buffer data. 
    //it is passed-in via Srb->DataBuffer. Because there are many "groups" of 
    //ioctl codes in this handler, so we should identify them by 
    //SrbIoCtrl->Signature field. possible values are:
    //  IOCTL_MINIPORT_SIGNATURE_SCSIDISK           "SCSIDISK"
    //  IOCTL_MINIPORT_SIGNATURE_HYBRDISK           "HYBRDISK"
    //  IOCTL_MINIPORT_SIGNATURE_DSM_NOTIFICATION   "MPDSM   "
    //  IOCTL_MINIPORT_SIGNATURE_DSM_GENERAL        "MPDSMGEN"
    //  IOCTL_MINIPORT_SIGNATURE_FIRMWARE           "FIRMWARE"
    //  IOCTL_MINIPORT_SIGNATURE_QUERY_PROTOCOL     "PROTOCOL"
    //  IOCTL_MINIPORT_SIGNATURE_SET_PROTOCOL       "SETPROTO"
    //  IOCTL_MINIPORT_SIGNATURE_QUERY_TEMPERATURE  "TEMPERAT"
    //  IOCTL_MINIPORT_SIGNATURE_SET_TEMPERATURE_THRESHOLD  "SETTEMPT"
    //  IOCTL_MINIPORT_SIGNATURE_QUERY_PHYSICAL_TOPOLOGY    "TOPOLOGY"
    //  IOCTL_MINIPORT_SIGNATURE_ENDURANCE_INFO     "ENDURINF"
    //** to use custom made ioctl codes, you should also define your own signature for SrbIoCtrl->Signature.
    UCHAR srb_status = SRB_STATUS_ERROR;
    PSRB_IO_CONTROL ioctl = (PSRB_IO_CONTROL) srbext->DataBuf;
    //all signature only has max 8 bytes length. so only need to calc this size one time.
    size_t count = strlen(IOCTL_MINIPORT_SIGNATURE_SCSIDISK);

    if (0 == strnicmp((const char*)ioctl->Signature, IOCTL_MINIPORT_SIGNATURE_SCSIDISK, count))
    {
        //handles IOCTL_SCSI_MINIPORT_* forwarded from DISK class driver device.
        //(e.g. disk SMART query request: "IOCTL_SCSI_MINIPORT_READ_SMART_LOG" from \\.\PhysicalDrive3)

    }
    else if (0 == strnicmp((const char*)ioctl->Signature, IOCTL_MINIPORT_SIGNATURE_QUERY_PROTOCOL, count))
    {
        //handles IOCTL_STORAGE_QUERY_PROPERTY
    }
    else
    {}

    return srb_status;
}