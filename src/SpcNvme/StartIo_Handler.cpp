#include "pch.h"
#include "ScsiHandler_CDB6.h"
#include "ScsiHandler_CDB10.h"
#include "ScsiHandler_CDB12.h"
#include "ScsiHandler_CDB16.h"
#include "ScsiHandler_InlineUtils.h"

BOOLEAN StartIo_DefaultHandler(PSPCNVME_SRBEXT srbext)
{
    //SetScsiSenseBySrbStatus(srbext->Srb, SRB_STATUS_INVALID_REQUEST);
    StorPortNotification(RequestComplete, srbext->DevExt, srbext->Srb);
    return FALSE;
}
BOOLEAN StartIo_ScsiHandler(PSPCNVME_SRBEXT srbext)
{
    //SetScsiSenseBySrbStatus(srbext->Srb, SRB_STATUS_INVALID_REQUEST);
    StorPortNotification(RequestComplete, srbext->DevExt, srbext->Srb);
    return FALSE;
}
