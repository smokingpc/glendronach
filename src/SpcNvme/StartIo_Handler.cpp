#include "pch.h"

UCHAR StartIo_DefaultHandler(PSPCNVME_SRBEXT srbext)
{
    UNREFERENCED_PARAMETER(srbext);
    //SetScsiSenseBySrbStatus(srbext->Srb, SRB_STATUS_INVALID_REQUEST);
    //StorPortNotification(RequestComplete, srbext->DevExt, srbext->Srb);
    return SRB_STATUS_INVALID_REQUEST;
}
UCHAR StartIo_ScsiHandler(PSPCNVME_SRBEXT srbext)
{
    UCHAR opcode = srbext->Cdb()->CDB6GENERIC.OperationCode;
    UCHAR srb_status = SRB_STATUS_ERROR;
    DebugScsiOpCode(opcode);

    switch(opcode)
    {
#if 0
    // 6-byte commands:
    case SCSIOP_TEST_UNIT_READY:
    case SCSIOP_REZERO_UNIT:
        //case SCSIOP_REWIND:
    case SCSIOP_REQUEST_BLOCK_ADDR:
    case SCSIOP_FORMAT_UNIT:
    case SCSIOP_READ_BLOCK_LIMITS:
    case SCSIOP_REASSIGN_BLOCKS:
        //case SCSIOP_INIT_ELEMENT_STATUS:
    case SCSIOP_SEEK6:
        //case SCSIOP_TRACK_SELECT:
        //case SCSIOP_SLEW_PRINT:
        //case SCSIOP_SET_CAPACITY:
    case SCSIOP_SEEK_BLOCK:
    case SCSIOP_PARTITION:
    case SCSIOP_READ_REVERSE:
    case SCSIOP_WRITE_FILEMARKS:
        //case SCSIOP_FLUSH_BUFFER:
    case SCSIOP_SPACE:
    case SCSIOP_RECOVER_BUF_DATA:
    case SCSIOP_RESERVE_UNIT:
    case SCSIOP_RELEASE_UNIT:
    case SCSIOP_COPY:
    case SCSIOP_ERASE:
    case SCSIOP_START_STOP_UNIT:
        //case SCSIOP_STOP_PRINT:
        //case SCSIOP_LOAD_UNLOAD:
    case SCSIOP_RECEIVE_DIAGNOSTIC:
    case SCSIOP_SEND_DIAGNOSTIC:
    case SCSIOP_MEDIUM_REMOVAL:

    //refer to https://learn.microsoft.com/zh-tw/windows-hardware/test/hlk/testref/1f98eed5-478b-42bc-8c17-ee49a2c63202
    case SCSIOP_REQUEST_SENSE:
        srb_status = Scsi_RequestSense6(srbext);
        break;
#endif
    case SCSIOP_MODE_SELECT:        //cache and other feature options
        srb_status = Scsi_ModeSelect6(srbext);
        break;
    case SCSIOP_READ6:
        //case SCSIOP_RECEIVE:
        srb_status = Scsi_Read6(srbext);
        break;
    case SCSIOP_WRITE6:
        //case SCSIOP_PRINT:
        //case SCSIOP_SEND:
        srb_status = Scsi_Write6(srbext);
        break;
    case SCSIOP_INQUIRY:
        srb_status = Scsi_Inquiry6(srbext);
        break;
    case SCSIOP_MODE_SENSE:
        srb_status = Scsi_ModeSense6(srbext);
        break;
    case SCSIOP_VERIFY6:
        srb_status = Scsi_Verify6(srbext);
        break;

        // 10-byte commands
#if 0
    case SCSIOP_READ_FORMATTED_CAPACITY:
    case SCSIOP_SEEK:
        //case SCSIOP_LOCATE:
        //case SCSIOP_POSITION_TO_ELEMENT:
    case SCSIOP_WRITE_VERIFY:
    case SCSIOP_SEARCH_DATA_HIGH:
    case SCSIOP_SEARCH_DATA_EQUAL:
    case SCSIOP_SEARCH_DATA_LOW:
    case SCSIOP_SET_LIMITS:
    case SCSIOP_READ_POSITION:
    case SCSIOP_SYNCHRONIZE_CACHE:
    case SCSIOP_COMPARE:
    case SCSIOP_COPY_COMPARE:
    case SCSIOP_WRITE_DATA_BUFF:
    case SCSIOP_READ_DATA_BUFF:
    case SCSIOP_WRITE_LONG:
    case SCSIOP_CHANGE_DEFINITION:
    case SCSIOP_WRITE_SAME:
    case SCSIOP_READ_SUB_CHANNEL:
        //case SCSIOP_UNMAP:
    case SCSIOP_READ_TOC:
    case SCSIOP_READ_HEADER:
        //case SCSIOP_REPORT_DENSITY_SUPPORT:
    case SCSIOP_PLAY_AUDIO:
    case SCSIOP_GET_CONFIGURATION:
    case SCSIOP_PLAY_AUDIO_MSF:
    case SCSIOP_PLAY_TRACK_INDEX:
        //case SCSIOP_SANITIZE:
    case SCSIOP_PLAY_TRACK_RELATIVE:
    case SCSIOP_GET_EVENT_STATUS:
    case SCSIOP_PAUSE_RESUME:
    case SCSIOP_LOG_SELECT:
    case SCSIOP_LOG_SENSE:
    case SCSIOP_STOP_PLAY_SCAN:
    case SCSIOP_XDWRITE:
    case SCSIOP_XPWRITE:
        //case SCSIOP_READ_DISK_INFORMATION:
        //case SCSIOP_READ_DISC_INFORMATION:
    case SCSIOP_READ_TRACK_INFORMATION:
    case SCSIOP_XDWRITE_READ:
        //case SCSIOP_RESERVE_TRACK_RZONE:
    case SCSIOP_SEND_OPC_INFORMATION:
    case SCSIOP_RESERVE_UNIT10:
        //case SCSIOP_RESERVE_ELEMENT:
    case SCSIOP_RELEASE_UNIT10:
        //case SCSIOP_RELEASE_ELEMENT:
    case SCSIOP_REPAIR_TRACK:
    case SCSIOP_CLOSE_TRACK_SESSION:
    case SCSIOP_READ_BUFFER_CAPACITY:
    case SCSIOP_SEND_CUE_SHEET:
    case SCSIOP_PERSISTENT_RESERVE_IN:
    case SCSIOP_PERSISTENT_RESERVE_OUT:
#endif
    case SCSIOP_MODE_SELECT10:          //cache and other feature options
        srb_status = Scsi_ModeSelect10(srbext);
        break;
    case SCSIOP_READ_CAPACITY:
        srb_status = Scsi_ReadCapacity10(srbext);
        break;
    case SCSIOP_READ:
        srb_status = Scsi_Read10(srbext);
        break;
    case SCSIOP_WRITE:
        srb_status = Scsi_Write10(srbext);
        break;
    case SCSIOP_VERIFY:
        srb_status = Scsi_Verify10(srbext);
        break;
    case SCSIOP_MODE_SENSE10:
        srb_status = Scsi_ModeSense10(srbext);
        break;

        // 12-byte commands                  
#if 0
    case SCSIOP_BLANK:
        //case SCSIOP_ATA_PASSTHROUGH12:
    case SCSIOP_SEND_EVENT:
        //case SCSIOP_SECURITY_PROTOCOL_IN:
    case SCSIOP_SEND_KEY:
        //case SCSIOP_MAINTENANCE_IN:
    case SCSIOP_REPORT_KEY:
        //case SCSIOP_MAINTENANCE_OUT:
    case SCSIOP_MOVE_MEDIUM:
    case SCSIOP_LOAD_UNLOAD_SLOT:
        //case SCSIOP_EXCHANGE_MEDIUM:
    case SCSIOP_SET_READ_AHEAD:
        //case SCSIOP_MOVE_MEDIUM_ATTACHED:
    case SCSIOP_SERVICE_ACTION_OUT12:
    case SCSIOP_SEND_MESSAGE:
        //case SCSIOP_SERVICE_ACTION_IN12:
    case SCSIOP_GET_PERFORMANCE:
    case SCSIOP_READ_DVD_STRUCTURE:
    case SCSIOP_WRITE_VERIFY12:
    case SCSIOP_SEARCH_DATA_HIGH12:
    case SCSIOP_SEARCH_DATA_EQUAL12:
    case SCSIOP_SEARCH_DATA_LOW12:
    case SCSIOP_SET_LIMITS12:
    case SCSIOP_READ_ELEMENT_STATUS_ATTACHED:
    case SCSIOP_REQUEST_VOL_ELEMENT:
        //case SCSIOP_SECURITY_PROTOCOL_OUT:
    case SCSIOP_SEND_VOLUME_TAG:
        //case SCSIOP_SET_STREAMING:
    case SCSIOP_READ_DEFECT_DATA:
    case SCSIOP_READ_ELEMENT_STATUS:
    case SCSIOP_READ_CD_MSF:
    case SCSIOP_SCAN_CD:
        //case SCSIOP_REDUNDANCY_GROUP_IN:
    case SCSIOP_SET_CD_SPEED:
        //case SCSIOP_REDUNDANCY_GROUP_OUT:
    case SCSIOP_PLAY_CD:
        //case SCSIOP_SPARE_IN:
    case SCSIOP_MECHANISM_STATUS:
        //case SCSIOP_SPARE_OUT:
    case SCSIOP_READ_CD:
        //case SCSIOP_VOLUME_SET_IN:
    case SCSIOP_SEND_DVD_STRUCTURE:
        //case SCSIOP_VOLUME_SET_OUT:
    case SCSIOP_INIT_ELEMENT_RANGE:
#endif
    case SCSIOP_REPORT_LUNS:
        srb_status = Scsi_ReportLuns12(srbext);
        break;
    case SCSIOP_READ12:
        //case SCSIOP_GET_MESSAGE:
        srb_status = Scsi_Read12(srbext);
        break;
    case SCSIOP_WRITE12:
        srb_status = Scsi_Write12(srbext);
        break;
    case SCSIOP_VERIFY12:
        srb_status = Scsi_Verify12(srbext);
        break;

        // 16-byte commands
#if 0
    case SCSIOP_XDWRITE_EXTENDED16:
        //case SCSIOP_WRITE_FILEMARKS16:
    case SCSIOP_REBUILD16:
        //case SCSIOP_READ_REVERSE16:
    case SCSIOP_REGENERATE16:
    case SCSIOP_EXTENDED_COPY:
        //case SCSIOP_POPULATE_TOKEN:
        //case SCSIOP_WRITE_USING_TOKEN:
    case SCSIOP_RECEIVE_COPY_RESULTS:
        //case SCSIOP_RECEIVE_ROD_TOKEN_INFORMATION:
    case SCSIOP_ATA_PASSTHROUGH16:
    case SCSIOP_ACCESS_CONTROL_IN:
    case SCSIOP_ACCESS_CONTROL_OUT:
    case SCSIOP_COMPARE_AND_WRITE:
    case SCSIOP_READ_ATTRIBUTES:
    case SCSIOP_WRITE_ATTRIBUTES:
    case SCSIOP_WRITE_VERIFY16:
    case SCSIOP_PREFETCH16:
    case SCSIOP_SYNCHRONIZE_CACHE16:
        //case SCSIOP_SPACE16:
    case SCSIOP_LOCK_UNLOCK_CACHE16:
        //case SCSIOP_LOCATE16:
    case SCSIOP_WRITE_SAME16:
        //case SCSIOP_ERASE16:
    case SCSIOP_ZBC_OUT:
    case SCSIOP_ZBC_IN:
    case SCSIOP_READ_DATA_BUFF16:
    case SCSIOP_SERVICE_ACTION_OUT16:
#endif

    case SCSIOP_READ16:
        srb_status = Scsi_Read16(srbext);
        break;
    case SCSIOP_WRITE16:
        srb_status = Scsi_Write16(srbext);
        break;
    case SCSIOP_VERIFY16:
        srb_status = Scsi_Verify16(srbext);
        break;
    case SCSIOP_READ_CAPACITY16:
        //case SCSIOP_GET_LBA_STATUS:
        //case SCSIOP_GET_PHYSICAL_ELEMENT_STATUS:
        //case SCSIOP_REMOVE_ELEMENT_AND_TRUNCATE:
        //case SCSIOP_SERVICE_ACTION_IN16:
        srb_status = Scsi_ReadCapacity16(srbext);
        break;

        // 32-byte commands
#if 0
    case SCSIOP_OPERATION32:
        break;
#endif
    default:
        srb_status = SRB_STATUS_INVALID_REQUEST;
        break;
    }

    return srb_status;
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
    UCHAR srb_status = SRB_STATUS_INVALID_REQUEST;
    PSRB_IO_CONTROL ioctl = (PSRB_IO_CONTROL) srbext->DataBuf();
    size_t count = strlen(IOCTL_MINIPORT_SIGNATURE_SCSIDISK);

    switch (ioctl->ControlCode)
    {
        case IOCTL_SCSI_MINIPORT_FIRMWARE:
            if (0 == strncmp((const char*)ioctl->Signature, IOCTL_MINIPORT_SIGNATURE_FIRMWARE, count))
                srb_status = IoctlScsiMiniport_Firmware(srbext, ioctl);
            break;
        default:
            srb_status = SRB_STATUS_INVALID_REQUEST;
            break;
    }

    return srb_status;
}