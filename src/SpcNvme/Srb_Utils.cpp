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

//void SetScsiSenseBySrbStatus(PSTORAGE_REQUEST_BLOCK srb, USHORT nvme_sc)
//{
//    PSENSE_DATA sdata = (PSENSE_DATA)SrbGetSenseInfoBuffer(srb);
//    UCHAR sdata_size = SrbGetSenseInfoBufferLength(srb);
//
//    if (nvme_sc == 0)
//    {
//        SrbSetScsiStatus(srb, SCSISTAT_GOOD);
//        SrbSetSrbStatus(srb, SRB_STATUS_SUCCESS);
//        return;
//    }
//
//    //if nvme_sc is not zero but no SendData buffer?
//    if (NULL == sdata || sdata_size == 0)
//    {
//        SrbSetScsiStatus(srb, SCSISTAT_CONDITION_MET);
//        SrbSetSrbStatus(srb, SRB_STATUS_ERROR);
//        return;
//    }
//
//    //    if (NULL != sdata && sdata_size > 0)
//    RtlZeroMemory(sdata, sdata_size);
//    sdata->ErrorCode = SCSI_SENSE_ERRORCODE_FIXED_CURRENT;
//    sdata->Valid = 0;
//    sdata->AdditionalSenseLength = sizeof(SENSE_DATA) - 7;
//    sdata->AdditionalSenseCodeQualifier = 0;
//
//    switch (nvme_sc)
//    {
//    case 0x01://INVALID_COMMAND_OPCODE
//        sdata->SenseKey = SCSI_SENSE_ILLEGAL_REQUEST;
//        sdata->AdditionalSenseCode = SCSI_ADSENSE_ILLEGAL_COMMAND;
//        SrbSetScsiStatus(srb, SCSISTAT_CHECK_CONDITION);
//        break;
//    case 0x02://INVALID_FIELD_IN_COMMAND
//        sdata->SenseKey = SCSI_SENSE_ILLEGAL_REQUEST;
//        sdata->AdditionalSenseCode = SCSI_ADSENSE_INVALID_CDB;
//        SrbSetScsiStatus(srb, SCSISTAT_CHECK_CONDITION);
//        break;
//    case 0x03://COMMAND_ID_CONFLICT
//        sdata->SenseKey = SCSI_SENSE_ILLEGAL_REQUEST;
//        sdata->AdditionalSenseCode = SCSI_ADSENSE_ILLEGAL_COMMAND;
//        SrbSetScsiStatus(srb, SCSISTAT_CHECK_CONDITION);
//        break;
//    case 0x04://DATA_TRANSFER_ERROR
//        sdata->SenseKey = SCSI_SENSE_MEDIUM_ERROR;
//        sdata->AdditionalSenseCode = SCSI_ADSENSE_NO_SENSE;
//        SrbSetScsiStatus(srb, SCSISTAT_CHECK_CONDITION);
//        break;
//    case 0x05://COMMANDS_ABORTED_DUE_TO_POWER_LOSS_NOTIFICATION
//        sdata->SenseKey = SCSI_SENSE_ABORTED_COMMAND;
//        sdata->AdditionalSenseCode = SCSI_ADSENSE_WARNING;
//        sdata->AdditionalSenseCodeQualifier = SCSI_SENSEQ_POWER_LOSS_EXPECTED;
//        SrbSetScsiStatus(srb, SCSISTAT_CHECK_CONDITION);  //it is TASK ABORTED in nvme guide, maybe need fix in future
//        break;
//    case 0x06://INTERNAL_DEVICE_ERROR
//        sdata->SenseKey = SCSI_SENSE_HARDWARE_ERROR;
//        sdata->AdditionalSenseCode = SCSI_ADSENSE_INTERNAL_TARGET_FAILURE;
//        SrbSetScsiStatus(srb, SCSISTAT_CHECK_CONDITION);
//        break;
//    case 0x07://COMMAND_ABORT_REQUESTED
//        sdata->SenseKey = SCSI_SENSE_ABORTED_COMMAND;
//        sdata->AdditionalSenseCode = SCSI_ADSENSE_NO_SENSE;
//        SrbSetScsiStatus(srb, SCSISTAT_CHECK_CONDITION);  //it is TASK ABORTED in nvme guide, maybe need fix in future
//        break;
//    case 0x08://COMMAND_ABORTED_DUE_TO_SQ_DELETION
//        sdata->SenseKey = SCSI_SENSE_ABORTED_COMMAND;
//        sdata->AdditionalSenseCode = SCSI_ADSENSE_NO_SENSE;
//        SrbSetScsiStatus(srb, SCSISTAT_CHECK_CONDITION);  //it is TASK ABORTED in nvme guide, maybe need fix in future
//        break;
//    case 0x09://COMMAND_ABORTED_DUE_TO_FAILED_FUSED_COMMAND
//        sdata->SenseKey = SCSI_SENSE_ABORTED_COMMAND;
//        sdata->AdditionalSenseCode = SCSI_ADSENSE_NO_SENSE;
//        SrbSetScsiStatus(srb, SCSISTAT_CHECK_CONDITION);  //it is TASK ABORTED in nvme guide, maybe need fix in future
//        break;
//    case 0x0A://COMMAND_ABORTED_DUE_TO_MISSING_FUSED_COMMAND
//        sdata->SenseKey = SCSI_SENSE_ABORTED_COMMAND;
//        sdata->AdditionalSenseCode = SCSI_ADSENSE_NO_SENSE;
//        SrbSetScsiStatus(srb, SCSISTAT_CHECK_CONDITION);  //it is TASK ABORTED in nvme guide, maybe need fix in future
//        break;
//    case 0x0B://INVALID_NAMESPACE_OR_FORMAT
//        sdata->SenseKey = SCSI_SENSE_ILLEGAL_REQUEST;
//        sdata->AdditionalSenseCode = SCSI_ADSENSE_ACCESS_DENIED;
//        sdata->AdditionalSenseCodeQualifier = SCSI_SENSEQ_NO_ACCESS_RIGHTS;
//        SrbSetScsiStatus(srb, SCSISTAT_CHECK_CONDITION);  //it is TASK ABORTED in nvme guide, maybe need fix in future
//        break;
//    case 0x80://LBA_OUT_OF_RANGE
//        sdata->SenseKey = SCSI_SENSE_ILLEGAL_REQUEST;
//        sdata->AdditionalSenseCode = SCSI_ADSENSE_ILLEGAL_BLOCK;
//        sdata->AdditionalSenseCodeQualifier = SCSI_SENSEQ_LOGICAL_ADDRESS_OUT_OF_RANGE;
//        SrbSetScsiStatus(srb, SCSISTAT_CHECK_CONDITION);  //it is TASK ABORTED in nvme guide, maybe need fix in future
//        break;
//    case 0x81://CAPACITY_EXCEEDED
//        sdata->SenseKey = SCSI_SENSE_MEDIUM_ERROR;
//        sdata->AdditionalSenseCode = SCSI_ADSENSE_NO_SENSE;
//        SrbSetScsiStatus(srb, SCSISTAT_CHECK_CONDITION);
//        break;
//    case 0x82://NAMESPACE_NOT_READY
//        sdata->SenseKey = SCSI_SENSE_NOT_READY;
//        sdata->AdditionalSenseCode = SCSI_ADSENSE_LUN_NOT_READY;
//        sdata->AdditionalSenseCodeQualifier = SCSI_SENSEQ_CAUSE_NOT_REPORTABLE;
//        SrbSetScsiStatus(srb, SCSISTAT_CHECK_CONDITION);
//        break;
//    case 0x20://Namespace is Write Protected
//        sdata->SenseKey = SCSI_SENSE_MEDIUM_ERROR;
//        sdata->AdditionalSenseCode = SCSI_ADSENSE_WRITE_PROTECT;
//        sdata->AdditionalSenseCodeQualifier = SCSI_SENSEQ_SPACE_ALLOC_FAILED_WRITE_PROTECT;
//        SrbSetScsiStatus(srb, SCSISTAT_CHECK_CONDITION);
//        break;
//    default:
//        sdata->SenseKey = SCSI_SENSE_ILLEGAL_REQUEST;
//        sdata->AdditionalSenseCode = SCSI_ADSENSE_INTERNAL_TARGET_FAILURE;
//        SrbSetScsiStatus(srb, SCSISTAT_CHECK_CONDITION);
//        break;
//    }
//
//    SrbSetSrbStatus(srb, SRB_STATUS_ERROR | SRB_STATUS_AUTOSENSE_VALID);
//}
