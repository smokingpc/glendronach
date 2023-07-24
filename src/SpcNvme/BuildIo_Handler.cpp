#include "pch.h"
#include "AdapterPnpHandler.h"

BOOLEAN BuildIo_DefaultHandler(PSPCNVME_SRBEXT srbext)
{
    //srbext->SetStatus(SRB_STATUS_INVALID_REQUEST);
    ////todo: set log 
    //StorPortNotification(RequestComplete, srbext->DevExt, srbext->Srb);
	srbext->CompleteSrb(SRB_STATUS_INVALID_REQUEST);
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
    //todo: set log 
    DebugScsiOpCode(srbext->Cdb()->CDB6GENERIC.OperationCode);
    
    //spcnvme only support 1 disk in current stage.
    //so check path/target/lun here.
    UCHAR path = 0, target = 0, lun = 0;
    SrbGetPathTargetLun(srbext->Srb, &path, &target, &lun);

    if(!(0==path && 0==target && 0==lun))
    {
        srbext->CompleteSrb(SRB_STATUS_INVALID_LUN);
        return FALSE;
    }
    return TRUE;
}

BOOLEAN BuildIo_SrbPowerHandler(PSPCNVME_SRBEXT srbext)
{
	srbext->CompleteSrb(SRB_STATUS_INVALID_REQUEST);
//always return FALSE. This event only handled in BuildIo.
    return FALSE;
}

BOOLEAN BuildIo_SrbPnpHandler(PSPCNVME_SRBEXT srbext)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    ULONG flags = 0;
    UCHAR srb_status = SRB_STATUS_ERROR;

    STOR_PNP_ACTION action = STOR_PNP_ACTION::StorStartDevice;
    PSRBEX_DATA_PNP srb_pnp = srbext->SrbDataPnp();

    ASSERT(NULL != srb_pnp);
    flags = srb_pnp->SrbPnPFlags;
    action = srb_pnp->PnPAction;

    //All unit control migrated to HwUnitControl callback...
    if(SRB_PNP_FLAGS_ADAPTER_REQUEST != (flags & SRB_PNP_FLAGS_ADAPTER_REQUEST))
    {
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
            srbext->DevExt->Teardown();
            srb_status = SRB_STATUS_SUCCESS;
            break;
        case StorSurpriseRemoval:
            //surprise remove doesn't need to shutdown controller.
            //controller is already gone , access controller registers will make BSoD or other problem.
            //srb_status = AdapterPnp_RemoveHandler(srbext);
            srbext->DevExt->Teardown();
            srb_status = SRB_STATUS_SUCCESS;
            break;
    }

END:
    srbext->CompleteSrb(srb_status);
    //always return FALSE. This event only handled in BuildIo.
    return FALSE;
}
