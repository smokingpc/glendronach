#include "pch.h"

UCHAR NvmeToSrbStatus(NVME_COMMAND_STATUS& status)
{
//this is most frequently passed condition, so pull it up here.
//It make common route won't consume callstack too deep.
    if(0 == (status.SCT & status.SC))
        return SRB_STATUS_SUCCESS;

    switch(status.SCT)
    {
    case NVME_STATUS_TYPE_GENERIC_COMMAND:
        return NvmeGenericToSrbStatus(status);
    case NVME_STATUS_TYPE_COMMAND_SPECIFIC:
        return NvmeCmdSpecificToSrbStatus(status);
    case NVME_STATUS_TYPE_MEDIA_ERROR:
        return NvmeMediaErrorToSrbStatus(status);
    }
    return SRB_STATUS_INTERNAL_ERROR;
}
void SetScsiSenseBySrbStatus(PSTORAGE_REQUEST_BLOCK srb, UCHAR status)
{
//don't set ScsiStatus for other SRB_STATUS_xxx .
//Only SRB_STATUS_ERROR need it.
    switch (status)
    {
        case SRB_STATUS_SUCCESS:
            SrbSetScsiStatus(srb, SCSISTAT_GOOD);
            break;
        case SRB_STATUS_ERROR:
        {
            //If SRB_STATUS_ERROR go with wrong ScsiStatus, storport could treat it as SRB_STATUS_SUCCESS.

            PSENSE_DATA sdata = (PSENSE_DATA)SrbGetSenseInfoBuffer(srb);
            UCHAR sdata_size = SrbGetSenseInfoBufferLength(srb);
            if (NULL == sdata || sdata_size == 0)
            {
                SrbSetScsiStatus(srb, SCSISTAT_CONDITION_MET);
            }
            else
            {
                RtlZeroMemory(sdata, sdata_size);
                sdata->ErrorCode = SCSI_SENSE_ERRORCODE_FIXED_CURRENT;
                sdata->Valid = 0;
                sdata->AdditionalSenseLength = sizeof(SENSE_DATA) - 7;
                sdata->AdditionalSenseCodeQualifier = 0;
                sdata->SenseKey = SCSI_SENSE_ILLEGAL_REQUEST;
                sdata->AdditionalSenseCode = SCSI_ADSENSE_ILLEGAL_COMMAND;
                SrbSetScsiStatus(srb, SCSISTAT_CHECK_CONDITION);
            }
        }
    }
}

