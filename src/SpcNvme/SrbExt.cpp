#include "pch.h"

_SPCNVME_SRBEXT* _SPCNVME_SRBEXT::GetSrbExt(PVOID devext, PSTORAGE_REQUEST_BLOCK srb)
{
    PSPCNVME_SRBEXT srbext = (PSPCNVME_SRBEXT)SrbGetMiniportContext(srb);
    srbext->Init(devext, srb);
    return srbext;
}

void _SPCNVME_SRBEXT::Init(PVOID devext, STORAGE_REQUEST_BLOCK* srb)
{
    DevExt = (CNvmeDevice*)devext;
    Srb = srb;
    SrbStatus = SRB_STATUS_SUCCESS;
    RtlZeroMemory(&SrcCmd, sizeof(NVME_COMMAND));

    if(NULL != srb)
    {
        FuncCode = SrbGetSrbFunction(srb);
        SrbStatus = SRB_STATUS_PENDING;
        ScsiQTag = SrbGetQueueTag(srb);
        Cdb = SrbGetCdb(srb);
        CdbLen = SrbGetCdbLength(srb);
        PathID = SrbGetPathId(srb); //SCSI Path (bus) ID
        TargetID = SrbGetTargetId(srb); //SCSI Dvice ID
        Lun = SrbGetLun(srb); //SCSI Logical UNit ID
        DataBuf = SrbGetDataBuffer(srb);
        DataBufLen = SrbGetDataTransferLength(srb);
    }
    else
    {
        FuncCode = SRB_FUNCTION_SPC_INTERNAL;
        SrbStatus = SRB_STATUS_PENDING;
        ScsiQTag = INVALID_SRB_QUEUETAG;
        Cdb = NULL;
        CdbLen = 0;
        PathID = INVALID_PATH_ID;
        TargetID = INVALID_TARGET_ID;
        Lun = INVALID_LUN_ID;
        DataBuf = NULL;
        DataBufLen = 0;
    }

    InitOK = TRUE;
}
