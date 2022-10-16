#include "pch.h"

BOOLEAN StartIo_DefaultHandler(PSPCNVME_SRBEXT srbext)
{
    SetScsiSenseBySrbStatus(srbext->Srb, SRB_STATUS_INVALID_REQUEST);
    StorPortNotification(RequestComplete, srbext->DevExt, srbext->Srb);
    return FALSE;
}
BOOLEAN StartIo_ScsiHandler(PSPCNVME_SRBEXT srbext)
{
    SetScsiSenseBySrbStatus(srbext->Srb, SRB_STATUS_INVALID_REQUEST);
    StorPortNotification(RequestComplete, srbext->DevExt, srbext->Srb);
    return FALSE;
}