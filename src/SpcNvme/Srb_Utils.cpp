#include "pch.h"

PSPCNVME_SRBEXT InitAndGetSrbExt(PVOID devext, PSTORAGE_REQUEST_BLOCK srb)
{
    PSPCNVME_SRBEXT srbext = GetSrbExt(srb);

    if(NULL == srbext)
        return NULL;

    srbext->DevExt = (PSPCNVME_DEVEXT)devext;
    srbext->Srb = srb;
    srbext->FuncCode = SrbGetSrbFunction((PVOID)srb);
    if (srbext->FuncCode == SRB_FUNCTION_STORAGE_REQUEST_BLOCK)
        srbext->FuncCode = ((PSTORAGE_REQUEST_BLOCK)srb)->SrbFunction;

    srbext->Cdb = (PCDB)SrbGetCdb((PVOID)srb);
    srbext->CdbLen = (SrbGetCdbLength((PVOID)srb));
    srbext->TargetID = (SrbGetTargetId((PVOID)srb));
    srbext->Lun = (SrbGetLun((PVOID)srb));
    srbext->DataBuf = (SrbGetDataBuffer((PVOID)srb));
    srbext->DataBufLen = (SrbGetDataTransferLength((PVOID)srb));
    srbext->TargetID = (SrbGetTargetId((PVOID)srb));
    srbext->Lun = (SrbGetLun((PVOID)srb));
    return srbext;
}
