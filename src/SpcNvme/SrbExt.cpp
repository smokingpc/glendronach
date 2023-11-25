#include "pch.h"

void _SPCNVME_SRBEXT::Init(PVOID devext, STORAGE_REQUEST_BLOCK* srb)
{
    RtlZeroMemory(this, sizeof(_SPCNVME_SRBEXT));
    DevExt = (CNvmeDevice*)devext;
    Srb = srb;
    SrbStatus = SRB_STATUS_PENDING;
    InitOK = TRUE;
    
    if(NULL != srb)
    { 
        PSTOR_ADDR_BTL8 addr = (PSTOR_ADDR_BTL8)SrbGetAddress(Srb);
        ASSERT(STOR_ADDRESS_TYPE_BTL8 == addr->Type);
        ASSERT(STOR_ADDR_BTL8_ADDRESS_LENGTH == addr->AddressLength);
        ScsiPath = addr->Path;
        ScsiTarget = addr->Target;
        ScsiLun = addr->Lun;
        StoragePort = addr->Port;   //miniport RaidPortXX number. dispatched by storport.
        ScsiTag = SrbGetQueueTag(srb);
    }
    else
    {
        ScsiPath = INVALID_PATH_ID;
        ScsiTarget = INVALID_TARGET_ID;
        ScsiLun = INVALID_LUN_ID;
        StoragePort = INVALID_PORT_ID;
        ScsiTag = INVALID_SCSI_TAG;
    }
}
void _SPCNVME_SRBEXT::CleanUp()
{
    ResetExtBuf(NULL);
    if(NULL != Prp2VA)
    { 
        ExFreePoolWithTag(Prp2VA, TAG_PRP2);
        Prp2VA = NULL;
    }
}
void _SPCNVME_SRBEXT::CompleteSrb(NVME_COMMAND_STATUS &nvme_status)
{
    UCHAR status = NvmeToSrbStatus(nvme_status);
    CompleteSrb(status);
}
void _SPCNVME_SRBEXT::CompleteSrb(UCHAR status)
{
    ASSERT(!this->IsCompleted);
    this->IsCompleted = TRUE;
    this->SrbStatus = status;
    if (NULL != Srb)
    {
        SetScsiSenseBySrbStatus(Srb, status);
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
//ULONG _SPCNVME_SRBEXT::ScsiQTag()
//{
//    if (NULL == Srb)
//        return INVALID_SCSI_TAG;
//    return SrbGetQueueTag(Srb);
//}
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
//UCHAR _SPCNVME_SRBEXT::PathID() {
//    if (NULL == Srb)
//        return INVALID_PATH_ID;
//    return SrbGetPathId(Srb);
//}
//UCHAR _SPCNVME_SRBEXT::TargetID() {
//    if (NULL == Srb)
//        return INVALID_TARGET_ID;
//    return SrbGetTargetId(Srb);
//}
//UCHAR _SPCNVME_SRBEXT::Lun() {
//    if (NULL == Srb)
//        return INVALID_LUN_ID;
//    return SrbGetLun(Srb);
//}
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
void _SPCNVME_SRBEXT::ResetExtBuf(PVOID new_buffer)
{
    if(NULL != ExtBuf)
        delete[] ExtBuf;
    ExtBuf = new_buffer;
}
PSRBEX_DATA_PNP _SPCNVME_SRBEXT::SrbDataPnp()
{
    if (NULL != Srb)
        return (PSRBEX_DATA_PNP)SrbGetSrbExDataByType(Srb, SrbExDataTypePnP);
    return NULL;
}