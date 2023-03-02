#include "pch.h"
#include "AdapterPnpHandler.h"

BOOLEAN BuildIo_DefaultHandler(PSPCNVME_SRBEXT srbext)
{
    srbext->Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
    //todo: set log 
    StorPortNotification(RequestComplete, srbext->DevExt, srbext->Srb);
    return FALSE;
}

BOOLEAN BuildIo_ScsiHandler(PSPCNVME_SRBEXT srbext)
{
    //srbext->Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
    //todo: set log 
    //todo: build prp and dma
    StorPortNotification(RequestComplete, srbext->DevExt, srbext->Srb);
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
            AdapterPnp_RemoveHandler(srbext);
            NTSTATUS status = srbext->DevExt->ShutdownController();
            if (!NT_SUCCESS(status))
            {
                KdBreakPoint();
                //todo: log
            }

            break;
        case StorSurpriseRemoval:
            AdapterPnp_RemoveHandler(srbext);
            //surprise remove doesn't need to shutdown controller.
            break;
    }

END:
    StorPortNotification(RequestComplete, srbext->DevExt, srbext->Srb);
    //always return FALSE. This event only handled in BuildIo.
    return FALSE;
}
