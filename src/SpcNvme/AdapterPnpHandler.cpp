#include "pch.h"

UCHAR AdapterPnp_QueryCapHandler(PSPCNVME_SRBEXT srbext)
{
    PSTOR_DEVICE_CAPABILITIES_EX cap = (PSTOR_DEVICE_CAPABILITIES_EX)srbext->DataBuf();
    cap->Version = STOR_DEVICE_CAPABILITIES_EX_VERSION_1;
    cap->Size = sizeof(STOR_DEVICE_CAPABILITIES_EX);
    cap->DeviceD1 = 0;
    cap->DeviceD2 = 0;
    cap->LockSupported = 0;
    cap->EjectSupported = 0;
    cap->Removable = 1;         //support RemoveAdapter
    cap->DockDevice = 0;
    cap->UniqueID = 0;
    cap->SilentInstall = 1;     //support silent install on DeviceManager UI
    cap->SurpriseRemovalOK = 1; //support Adapter SurpriseRemove (hot plug)
    cap->NoDisplayInUI = 0;     //should this adapter be shown on DeviceManager UI?
    cap->Address = 0;
    cap->UINumber = 0xFFFFFFFF;
//    srbext->SetStatus(SRB_STATUS_SUCCESS);
    return SRB_STATUS_SUCCESS;
}

UCHAR AdapterPnp_RemoveHandler(PSPCNVME_SRBEXT srbext)
{
    //NTSTATUS status = STATUS_UNSUCCESSFUL;
    srbext->DevExt->Teardown();
//    srbext->Srb->SrbStatus = SRB_STATUS_SUCCESS;
//    srbext->SetStatus(SRB_STATUS_SUCCESS);
    return SRB_STATUS_SUCCESS;
}
