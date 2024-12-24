#include "pch.h"

static __inline void GetStorportAddr(PSPC_SRBEXT srbext, PSCSI_REQUEST_BLOCK srb)
{
    srbext->StoragePort = INVALID_PORT_ID;
    srbext->ScsiPath = srb->PathId;
    srbext->ScsiTarget = srb->TargetId;
    srbext->ScsiLun = srb->Lun;
}
static __inline void GetStorportAddr(PSPC_SRBEXT srbext, PSTORAGE_REQUEST_BLOCK srb)
{
    PSTOR_ADDR_BTL8 addr = (PSTOR_ADDR_BTL8)SrbGetAddress(srb);
    ASSERT(STOR_ADDRESS_TYPE_BTL8 == addr->Type);
    ASSERT(STOR_ADDR_BTL8_ADDRESS_LENGTH == addr->AddressLength);
    srbext->ScsiPath = addr->Path;
    srbext->ScsiTarget = addr->Target;
    srbext->ScsiLun = addr->Lun;
    srbext->StoragePort = addr->Port;   //miniport RaidPortXX number. dispatched by storport.
}
static __inline bool IsStorageRequestBlock(PSCSI_REQUEST_BLOCK srb)
{
    return (SRB_FUNCTION_STORAGE_REQUEST_BLOCK == srb->Function);
}
static __inline bool IsScsiRead(UCHAR scsi_op)
{
    switch (scsi_op)
    {
    case SCSIOP_READ6:
    case SCSIOP_READ:
    case SCSIOP_READ12:
    case SCSIOP_READ16:
        return true;
    }

    return false;
}
static __inline bool IsScsiWrite(UCHAR scsi_op)
{
    switch (scsi_op)
    {
    case SCSIOP_WRITE6:
    case SCSIOP_WRITE:
    case SCSIOP_WRITE12:
    case SCSIOP_WRITE16:
        return true;
    }

    return false;
}

void _SPC_SRBEXT::Init(
    _In_ PVOID devext, 
    _In_ PSCSI_REQUEST_BLOCK srb, 
    _In_ PSPC_SRBEXT_COMPLETION callback)
{
    //DO NOT use virtual function in _SPC_SRBEXT, unless you remove RtlZeroMemory() here.
    //RtlZeroMemory() will destroy vtable of object.
    RtlZeroMemory(this, sizeof(_SPC_SRBEXT));

    //overrun guard should be init AFTER memset() immediately or at first line.
    OverrunGuard = OVERRUN_GUARD_TAG;
    DevExt = (CNvmeDevice*)devext;
    Srb = srb;
    SrbStatus = SRB_STATUS_PENDING;
    CompletionCB = callback;

    if(nullptr != srb)
    {
        ScsiTag = SrbGetQueueTag(srb);
        Cdb = SrbGetCdb(srb);
        CdbLen = SrbGetCdbLength(srb);
        FunctionCode = SrbGetSrbFunction(srb);
        DataBuffer = SrbGetDataBuffer(srb);
        DataBufLen = SrbGetDataTransferLength(srb);

        if(IsStorageRequestBlock(srb))
            GetStorportAddr(this, (PSTORAGE_REQUEST_BLOCK)srb);
        else
            GetStorportAddr(this, srb);

        if(nullptr != Cdb)
        {
            IsWrite = IsScsiWrite(Cdb->CDB6GENERIC.OperationCode);
        }
    }
    else
    {
        FunctionCode = SRB_FUNCTION_SPC_INTERNAL;
        ScsiTag = INVALID_SCSI_TAG;
        StoragePort = INVALID_PORT_ID;
        ScsiPath = INVALID_PATH_ID;
        ScsiTarget = INVALID_TARGET_ID;
        ScsiLun = INVALID_LUN_ID;
    }
    InitOK = TRUE;
}
void _SPC_SRBEXT::CleanUp()
{
    ResetExtBuf(nullptr);
    if(nullptr != Prp2List)
    { 
        ExFreePoolWithTag(Prp2List, TAG_PRP2);
        Prp2List = nullptr;
    }

    //DO NOT do anything after this line!!!
    if(DeleteInComplete)
        delete this;
}
void _SPC_SRBEXT::CompleteSrb(_In_ NVME_COMMAND_STATUS &nvme_status)
{
    UCHAR status = NvmeToSrbStatus(nvme_status);
    CompleteSrb(status);
}
void _SPC_SRBEXT::CompleteSrb(_In_ UCHAR status)
{
    ASSERT(!IsCompleted);
    if(nullptr != CompletionCB)
        CompletionCB(this);

    IsCompleted = TRUE;
    SrbStatus = status;
    if (nullptr != Srb)
    {
        SetScsiSenseBySrbStatus(Srb, status);
        SrbSetSrbStatus(Srb, status);
        StorPortNotification(RequestComplete, DevExt, Srb);
    }
}
void _SPC_SRBEXT::SetDataBufTxLength(_In_ ULONG length)
{
//some operation need set DataBufTxLength.
//When SRB complete, such operation will do same thing as buffered i/o.
//It copy data from DataBuffer back to usermode buffer.
//In this kind of operation, storport driver determines 
// "how many bytes copied back" by DataBufTxLength.
    if(nullptr != Srb)
        SrbSetDataTransferLength(Srb, length);
}
void _SPC_SRBEXT::ResetExtBuf(_In_ PVOID new_buffer)
{
    if(nullptr != ExtraBuf)
        delete[] ExtraBuf;
    ExtraBuf = new_buffer;
}
bool _SPC_SRBEXT::GetPnpRequest(
    _Out_ STOR_PNP_ACTION& action,
    _Out_ ULONG& flags)
{
    if(SRB_FUNCTION_PNP != FunctionCode)
        return false;

    PSRBEX_DATA_PNP srbex_pnp = (PSRBEX_DATA_PNP)SrbGetSrbExDataByType(
        (PSTORAGE_REQUEST_BLOCK)Srb, SrbExDataTypePnP);

    if (nullptr != srbex_pnp) {
        flags = srbex_pnp->SrbPnPFlags;
        action = srbex_pnp->PnPAction;
    }
    else {
        PSCSI_PNP_REQUEST_BLOCK scsi_pnp = (PSCSI_PNP_REQUEST_BLOCK)Srb;
        flags = scsi_pnp->SrbPnPFlags;
        action = scsi_pnp->PnPAction;
    }
    return true;
}
PVOID _SPC_SRBEXT::AllocExtraBuf(ULONG size)
{
    ResetExtBuf(nullptr);
    this->ExtraBufSize = size;
    this->ExtraBuf = new (NonPagedPool, TAG_GENBUF) UCHAR[size];
    if(nullptr != this->ExtraBuf)
        RtlZeroMemory(this->ExtraBuf, size);
    return this->ExtraBuf;
}

#if 0
bool _SPC_SRBEXT::BuildPrpByBuffer(PVOID buffer, UINT32 buf_size)
{
    if(!BuildPrp(this->Prp1VA, this->Prp2VA, this->PrpCount, this->Prp2List, buffer, buf_size))
        return false;

    this->Prp1PA.QuadPart = MmGetPhysicalAddress(this->Prp1VA).QuadPart;
    this->Prp2PA.QuadPart = MmGetPhysicalAddress(this->Prp2VA).QuadPart;
    return true;
}
#endif