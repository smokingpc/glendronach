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
    RtlZeroMemory(&NvmeCmd, sizeof(NVME_COMMAND));
    RtlZeroMemory(&NvmeCpl, sizeof(NVME_COMPLETION_ENTRY));
    SetStatus(SRB_STATUS_PENDING);
    FreePrp2List = FALSE;
    Prp2VA = NULL;
    Prp2PA.QuadPart = 0;
    InitOK = TRUE;
}

void _SPCNVME_SRBEXT::SetStatus(UCHAR status)
{
    if(NULL != Srb)
        SrbSetSrbStatus(Srb, status);
    this->SrbStatus = status;
}
void _SPCNVME_SRBEXT::CompleteSrbWithStatus(UCHAR status)
{
    this->SrbStatus = status;
    if (NULL != Srb)
    {
        SrbSetSrbStatus(Srb, status);
        StorPortNotification(RequestComplete, DevExt, Srb);
    }
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

void _SPCNVME_SRBEXT::SetTransferLength(ULONG length)
{
    if(NULL != Srb)
        SrbSetDataTransferLength(Srb, length);
}

PSRBEX_DATA_PNP _SPCNVME_SRBEXT::SrbDataPnp()
{
    if (NULL != Srb)
        return (PSRBEX_DATA_PNP)SrbGetSrbExDataByType(Srb, SrbExDataTypePnP);
    return NULL;
}