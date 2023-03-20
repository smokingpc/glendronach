#include "pch.h"
#include "AdapterPnpHandler.h"

BOOLEAN BuildIo_DefaultHandler(PSPCNVME_SRBEXT srbext)
{
//    srbext->Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
    srbext->SetStatus(SRB_STATUS_INVALID_REQUEST);
    //todo: set log 
    StorPortNotification(RequestComplete, srbext->DevExt, srbext->Srb);
    return FALSE;
}

BOOLEAN BuildIo_IoctlHandler(PSPCNVME_SRBEXT srbext)
{
    //Handle IOCTL only in StartIo.
    //I don't like to handle IOCTL in DISPATCH_LEVEL...
    UNREFERENCED_PARAMETER(srbext);
    return TRUE;
}

BOOLEAN BuildIo_ScsiHandler(PSPCNVME_SRBEXT srbext)
{
    UNREFERENCED_PARAMETER(srbext);
    //srbext->Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
    //todo: set log 
    //todo: build prp and dma
    DebugScsiOpCode(srbext->Cdb()->CDB6GENERIC.OperationCode);
    
    return TRUE;
}

BOOLEAN BuildIo_SrbPowerHandler(PSPCNVME_SRBEXT srbext)
{
//    srbext->Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
    srbext->SetStatus(SRB_STATUS_INVALID_REQUEST);
    //todo: set log 
    StorPortNotification(RequestComplete, srbext->DevExt, srbext->Srb);

//always return FALSE. This event only handled in BuildIo.
    return FALSE;
}

BOOLEAN BuildIo_SrbPnpHandler(PSPCNVME_SRBEXT srbext)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    ULONG flags = 0;
    ULONG action = 0;
    UCHAR srb_status = SRB_STATUS_ERROR;

    STOR_PNP_ACTION PnPAction;
    PSRBEX_DATA_PNP srb_pnp = srbext->SrbDataPnp();

    ASSERT (NULL != srb_pnp);
    flags = srb_pnp->SrbPnPFlags;
    action = srb_pnp->PnPAction;

    //All unit control migrated to HwUnitControl callback...
    if(SRB_PNP_FLAGS_ADAPTER_REQUEST != (flags & SRB_PNP_FLAGS_ADAPTER_REQUEST))
    {
        //KdBreakPoint();
        goto END;
    }

    switch(action)
    {
        case StorQueryCapabilities:
            srb_status = AdapterPnp_QueryCapHandler(srbext);
            break;
        case StorRemoveDevice:
        //regular RemoveDevice should shutdown controller first, then delete all queue memory.
            status = srbext->DevExt->ShutdownController();
            if (!NT_SUCCESS(status))
            {
                KdBreakPoint();
                //todo: log
            }
            srb_status = AdapterPnp_RemoveHandler(srbext);

            break;
        case StorSurpriseRemoval:
            //surprise remove doesn't need to shutdown controller.
            //controller is already gone , access controller registers will make BSoD or other problem.
            srb_status = AdapterPnp_RemoveHandler(srbext);
            break;
    }

END:
    srbext->SetStatus(srb_status);
    StorPortNotification(RequestComplete, srbext->DevExt, srbext->Srb);
    //always return FALSE. This event only handled in BuildIo.
    return FALSE;
}
