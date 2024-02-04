#include "pch.h"

void _SPCNVME_SRBEXT::Init(PVOID devext, PSCSI_REQUEST_BLOCK srb)
{
    RtlZeroMemory(this, sizeof(_SPCNVME_SRBEXT));
    DevExt = (CNvmeDevice*)devext;
    Srb = srb;
    SrbStatus = SRB_STATUS_PENDING;
    InitOK = TRUE;
    
    if(NULL != srb)
    { 
        ScsiTag = SrbGetQueueTag(srb);
        Cdb = SrbGetCdb(srb);
        CdbLen = SrbGetCdbLength(srb);
        SrbFuncCode = SrbGetSrbFunction(srb);
        DataBuffer = SrbGetDataBuffer(srb);
        DataBufLen = SrbGetDataTransferLength(srb);

        if(SRB_FUNCTION_STORAGE_REQUEST_BLOCK == SrbFuncCode)
        {
            SrbEx = (PSTORAGE_REQUEST_BLOCK) srb;
            PSTOR_ADDR_BTL8 addr = (PSTOR_ADDR_BTL8)SrbGetAddress(SrbEx);
            ASSERT(STOR_ADDRESS_TYPE_BTL8 == addr->Type);
            ASSERT(STOR_ADDR_BTL8_ADDRESS_LENGTH == addr->AddressLength);
            ScsiPath = addr->Path;
            ScsiTarget = addr->Target;
            ScsiLun = addr->Lun;
            StoragePort = addr->Port;   //miniport RaidPortXX number. dispatched by storport.
        }
        else
        {
            ScsiPath = srb->PathId;
            ScsiTarget = srb->TargetId;
            ScsiLun = srb->Lun;
        }        
    }
    else
    {
        ScsiPath = INVALID_PATH_ID;
        ScsiTarget = INVALID_TARGET_ID;
        ScsiLun = INVALID_LUN_ID;
        ScsiTag = INVALID_SCSI_TAG;
        SrbFuncCode = SRB_FUNCTION_SPC_INTERNAL;
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
PSRBEX_DATA_PNP _SPCNVME_SRBEXT::GetSrbExPnpData()
{
    if (NULL != SrbEx)
        return (PSRBEX_DATA_PNP)SrbGetSrbExDataByType(
                    SrbEx, SrbExDataTypePnP);
    return NULL;
}