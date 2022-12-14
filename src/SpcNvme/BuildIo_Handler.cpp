#include "pch.h"

BOOLEAN BuildIo_DefaultHandler(PSPCNVME_SRBEXT srbext)
{
    SetScsiSenseBySrbStatus(srbext->Srb, SRB_STATUS_INVALID_REQUEST);
    StorPortNotification(RequestComplete, srbext->DevExt, srbext->Srb);
    return FALSE;
}

BOOLEAN BuildIo_ScsiHandler(PSPCNVME_SRBEXT srbext)
{
    SetScsiSenseBySrbStatus(srbext->Srb, SRB_STATUS_INVALID_REQUEST);
    StorPortNotification(RequestComplete, srbext->DevExt, srbext->Srb);
    return FALSE;
}