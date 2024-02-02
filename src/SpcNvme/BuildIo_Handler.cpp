#include "pch.h"
#include "AdapterPnpHandler.h"

UCHAR BuildIo_DefaultHandler(PSPCNVME_SRBEXT srbext)
{
    //srbext->SetStatus(SRB_STATUS_INVALID_REQUEST);
    ////todo: set log 
    UNREFERENCED_PARAMETER(srbext);
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR BuildIo_IoctlHandler(PSPCNVME_SRBEXT srbext)
{
    //Handle IOCTL only in StartIo.
    //I don't like to handle IOCTL in DISPATCH_LEVEL...
    UNREFERENCED_PARAMETER(srbext);
    return SRB_STATUS_PENDING;
}

UCHAR BuildIo_ScsiHandler(PSPCNVME_SRBEXT srbext)
{
    //todo: set log 
    DebugScsiOpCode(srbext->Cdb()->CDB6GENERIC.OperationCode);
    
    //check path/target/lun here. Only accept request which has valid BTL address.
#if 0
    if(FALSE == (0==srbext->ScsiPath && 
                0==srbext->ScsiTarget && 
                srbext->NvmeDev->IsLunExist(srbext->ScsiLun))
        )
#endif
    if(NULL == srbext->NvmeDev)
        return SRB_STATUS_NO_DEVICE;

    return SRB_STATUS_PENDING;
}

UCHAR BuildIo_SrbPowerHandler(PSPCNVME_SRBEXT srbext)
{
//always return FALSE. This event only handled in BuildIo.
    UNREFERENCED_PARAMETER(srbext);
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR BuildIo_SrbPnpHandler(PSPCNVME_SRBEXT srbext)
{
    //NTSTATUS status = STATUS_UNSUCCESSFUL;
    ULONG flags = 0;
    UCHAR srb_status = SRB_STATUS_ERROR;

    STOR_PNP_ACTION action = STOR_PNP_ACTION::StorStartDevice;
    PSRBEX_DATA_PNP srb_pnp = srbext->SrbDataPnp();
    PVROC_DEVEXT devext = (PVROC_DEVEXT)srbext->DevExt;
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
            devext->ShutdownAllVrocNvmeControllers();
            devext->Teardown();
            srb_status = SRB_STATUS_SUCCESS;
            break;
        case StorSurpriseRemoval:
            //surprise remove doesn't need to shutdown controller.
            //controller is already gone , access controller registers will make BSoD or other problem.
            //srb_status = AdapterPnp_RemoveHandler(srbext);
            devext->Teardown();
            srb_status = SRB_STATUS_SUCCESS;
            break;
        case StorStopDevice:
        //StopDevice is used for some special case. e.g. PnpRebalanceResource event.
        //This is rare case that could happen when hotplug.
        //To handle rebalance resource event, just release all PNP resource then 
        //Storport will call HwFindAdapter again to re-assign resource to us.
            devext->DisableAllVrocNvmeControllers();
            devext->Teardown();
            srb_status = SRB_STATUS_SUCCESS;
            break;
    }

END:
    return srb_status;
}
