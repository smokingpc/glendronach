#include "pch.h"
#include "AdapterPnpHandler.h"

UCHAR BuildIo_DefaultHandler(PSPC_SRBEXT srbext)
{
    //srbext->SetStatus(SRB_STATUS_INVALID_REQUEST);
    ////todo: set log 
    UNREFERENCED_PARAMETER(srbext);
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR BuildIo_IoctlHandler(PSPC_SRBEXT srbext)
{
    //Handle IOCTL only in StartIo.
    //I don't like to handle IOCTL in DISPATCH_LEVEL...
    UNREFERENCED_PARAMETER(srbext);
    return SRB_STATUS_PENDING;
}

UCHAR BuildIo_ScsiHandler(PSPC_SRBEXT srbext)
{
    //todo: set log 
    DebugScsiOpCode(srbext->Cdb->CDB6GENERIC.OperationCode);
    CNvmeDevice* devext = (CNvmeDevice*)srbext->DevExt;
    //check path/target/lun here. Only accept request which has valid BTL address.
    if(FALSE == (0==srbext->ScsiPath && 
                0==srbext->ScsiTarget && 
                devext->IsLunExist(srbext->ScsiLun))
        )
    {
        return SRB_STATUS_INVALID_LUN;
    }

    return SRB_STATUS_PENDING;
}

UCHAR BuildIo_SrbPowerHandler(PSPC_SRBEXT srbext)
{
//always return FALSE. This event only handled in BuildIo.
    UNREFERENCED_PARAMETER(srbext);
    return SRB_STATUS_INVALID_REQUEST;
}

UCHAR BuildIo_SrbPnpHandler(PSPC_SRBEXT srbext)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    ULONG flags = 0;
    UCHAR srb_status = SRB_STATUS_ERROR;
    CNvmeDevice* devext = (CNvmeDevice*)srbext->DevExt;

    STOR_PNP_ACTION action = STOR_PNP_ACTION::StorStartDevice;
    if(!srbext->GetPnpRequest(action, flags))
        return SRB_STATUS_ERROR;

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
            status = devext->ShutdownController();
            if (!NT_SUCCESS(status))
            {
                KdBreakPoint();
                //todo: log
            }
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
            status = devext->DisableController();
            if (!NT_SUCCESS(status))
            {
                KdBreakPoint();
                //todo: log
            }
            devext->Teardown();
            devext->RebalancingPnp = TRUE;
            srb_status = SRB_STATUS_SUCCESS;
            break;
    }

END:
    return srb_status;
}
