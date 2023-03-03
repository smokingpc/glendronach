#include "pch.h"
#include "AdapterPnpHandler.h"

BOOLEAN BuildIo_DefaultHandler(PSPCNVME_SRBEXT srbext)
{
    srbext->Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
    //todo: set log 
    StorPortNotification(RequestComplete, srbext->DevExt, srbext->Srb);
    return FALSE;
}

BOOLEAN BuildIo_IoctlHandler(PSPCNVME_SRBEXT srbext)
{
    //Handle IOCTL only in StartIo.
    //I don't like to handle IOCTL in DISPATCH_LEVEL...
    
    return TRUE;
}

BOOLEAN BuildIo_ScsiHandler(PSPCNVME_SRBEXT srbext)
{
    //srbext->Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
    //todo: set log 
    //todo: build prp and dma
    
    return TRUE;
}

BOOLEAN BuildIo_SrbPowerHandler(PSPCNVME_SRBEXT srbext)
{
    srbext->Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
    //todo: set log 
    StorPortNotification(RequestComplete, srbext->DevExt, srbext->Srb);

//always return FALSE. This event only handled in BuildIo.
    return FALSE;
}

BOOLEAN BuildIo_SrbPnpHandler(PSPCNVME_SRBEXT srbext)
{
    ULONG flags = 0;
    ULONG action = 0;
    //All unit control migrated to HwUnitControl callback...
    if(SRB_PNP_FLAGS_ADAPTER_REQUEST != (flags & SRB_PNP_FLAGS_ADAPTER_REQUEST))
    {
        KdBreakPoint();
        goto END;
    }

    switch(action)
    {
        case StorQueryCapabilities:
            AdapterPnp_QueryCapHandler(srbext);
            break;
        case StorRemoveDevice:
        //regular RemoveDevice should shutdown controller first, then delete all queue memory.
            NTSTATUS status = srbext->DevExt->ShutdownController();
            if (!NT_SUCCESS(status))
            {
                KdBreakPoint();
                //todo: log
            }
            AdapterPnp_RemoveHandler(srbext);

            break;
        case StorSurpriseRemoval:
            //surprise remove doesn't need to shutdown controller.
            //controller is already gone , access controller registers will make BSoD or other problem.
            AdapterPnp_RemoveHandler(srbext);
            break;
    }

END:
    StorPortNotification(RequestComplete, srbext->DevExt, srbext->Srb);
    //always return FALSE. This event only handled in BuildIo.
    return FALSE;
}
