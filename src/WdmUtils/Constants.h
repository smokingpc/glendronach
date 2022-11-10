#pragma once
#define MARKER_TAG          0x23939889      //pizza-hut!  :p
#define BSOD_DELETE_ERROR   0x28825252      //dominos pizza!  :p

#define MSG_BUFFER_SIZE     128

#define DEBUG_PREFIX        "SPC ==>"
#define DBG_FILTER          0x00000888

#define CALLINOUT_TAG       (ULONG)'TUOC'

#define DBGMSG_LEVEL       0x00001000

#define KPRINTF(x)  DbgPrintEx(DPFLTR_IHVDRIVER_ID,DBGMSG_LEVEL,x);
