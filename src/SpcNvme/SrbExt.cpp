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
//    SrbStatus = SRB_STATUS_SUCCESS;
    RtlZeroMemory(&NvmeCmd, sizeof(NVME_COMMAND));

    SrbStatus = SRB_STATUS_PENDING;
#if 0
    if(NULL != srb)
    {
        SrbStatus = SRB_STATUS_PENDING;
        //FuncCode = SrbGetSrbFunction(srb);
        //ScsiQTag = SrbGetQueueTag(srb);
        //Cdb = SrbGetCdb(srb);
        //CdbLen = SrbGetCdbLength(srb);
        //PathID = SrbGetPathId(srb); //SCSI Path (bus) ID
        //TargetID = SrbGetTargetId(srb); //SCSI Dvice ID
        //Lun = SrbGetLun(srb); //SCSI Logical UNit ID
        //DataBuf = SrbGetDataBuffer(srb);
        //DataBufLen = SrbGetDataTransferLength(srb);
    }
    else
    {
        SrbStatus = SRB_STATUS_PENDING;
        //FuncCode = SRB_FUNCTION_SPC_INTERNAL;
        //ScsiQTag = INVALID_SRB_QUEUETAG;
        //Cdb = NULL;
        //CdbLen = 0;
        //PathID = INVALID_PATH_ID;
        //TargetID = INVALID_TARGET_ID;
        //Lun = INVALID_LUN_ID;
        //DataBuf = NULL;
        //DataBufLen = 0;
    }
#endif
    InitOK = TRUE;
}

void _SPCNVME_SRBEXT::SetStatus(UCHAR status)
{
    if(NULL != Srb)
        SrbSetSrbStatus(Srb, status);
}
ULONG _SPCNVME_SRBEXT::FuncCode()
{
    if(NULL == Srb)
        return SRB_FUNCTION_SPC_INTERNAL;
    return SrbGetSrbFunction(Srb);
}
ULONG _SPCNVME_SRBEXT::ScsiQTag()
{
    if (NULL == Srb)
        return INVALID_SRB_QUEUETAG;
    return SrbGetQueueTag(Srb);
}
PCDB _SPCNVME_SRBEXT::Cdb()
{
    if (NULL == Srb)
        return NULL;
    return SrbGetCdb(Srb);
}
UCHAR _SPCNVME_SRBEXT::CdbLen() {
    if (NULL == Srb)
        return 0;
    return SrbGetCdbLength(Srb);
}
UCHAR _SPCNVME_SRBEXT::PathID() {
    if (NULL == Srb)
        return INVALID_PATH_ID;
    return SrbGetPathId(Srb);
}
UCHAR _SPCNVME_SRBEXT::TargetID() {
    if (NULL == Srb)
        return INVALID_TARGET_ID;
    return SrbGetTargetId(Srb);
}
UCHAR _SPCNVME_SRBEXT::Lun() {
    if (NULL == Srb)
        return INVALID_LUN_ID;
    return SrbGetLun(Srb);
}
PVOID _SPCNVME_SRBEXT::DataBuf() {
    if (NULL == Srb)
        return NULL;
    return SrbGetDataBuffer(Srb);
}
ULONG _SPCNVME_SRBEXT::DataBufLen() {
    if (NULL == Srb)
        return 0;
    return SrbGetDataTransferLength(Srb);
}

