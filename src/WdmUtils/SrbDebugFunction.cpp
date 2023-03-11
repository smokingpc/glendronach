#include "pch.h"
EXTERN_C_START
#include <storport.h>
#include <srbhelper.h>
EXTERN_C_END
#include <ntstrsafe.h>

void DebugSrbFunctionCode(ULONG code)
{
    char msg[MSG_BUFFER_SIZE] = { 0 };

    switch (code)
    {
    case SRB_FUNCTION_EXECUTE_SCSI:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SRB_FUNCTION_EXECUTE_SCSI");
        break;
    case SRB_FUNCTION_CLAIM_DEVICE:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SRB_FUNCTION_CLAIM_DEVICE");
        break;
    case SRB_FUNCTION_IO_CONTROL:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SRB_FUNCTION_IO_CONTROL");
        break;
    case SRB_FUNCTION_RECEIVE_EVENT:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SRB_FUNCTION_RECEIVE_EVENT");
        break;
    case SRB_FUNCTION_RELEASE_QUEUE:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SRB_FUNCTION_RELEASE_QUEUE");
        break;
    case SRB_FUNCTION_ATTACH_DEVICE:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SRB_FUNCTION_ATTACH_DEVICE");
        break;
    case SRB_FUNCTION_RELEASE_DEVICE:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SRB_FUNCTION_RELEASE_DEVICE");
        break;
    case SRB_FUNCTION_SHUTDOWN:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SRB_FUNCTION_SHUTDOWN");
        break;
    case SRB_FUNCTION_FLUSH:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SRB_FUNCTION_FLUSH");
        break;
    case SRB_FUNCTION_PROTOCOL_COMMAND:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SRB_FUNCTION_PROTOCOL_COMMAND");
        break;
    case SRB_FUNCTION_ABORT_COMMAND:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SRB_FUNCTION_ABORT_COMMAND");
        break;
    case SRB_FUNCTION_RELEASE_RECOVERY:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SRB_FUNCTION_RELEASE_RECOVERY");
        break;
    case SRB_FUNCTION_RESET_BUS:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SRB_FUNCTION_RESET_BUS");
        break;
    case SRB_FUNCTION_RESET_DEVICE:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SRB_FUNCTION_RESET_DEVICE");
        break;
    case SRB_FUNCTION_TERMINATE_IO:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SRB_FUNCTION_TERMINATE_IO");
        break;
    case SRB_FUNCTION_FLUSH_QUEUE:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SRB_FUNCTION_FLUSH_QUEUE");
        break;
    case SRB_FUNCTION_REMOVE_DEVICE:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SRB_FUNCTION_REMOVE_DEVICE");
        break;
    case SRB_FUNCTION_WMI:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SRB_FUNCTION_WMI");
        break;
    case SRB_FUNCTION_LOCK_QUEUE:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SRB_FUNCTION_LOCK_QUEUE");
        break;
    case SRB_FUNCTION_UNLOCK_QUEUE:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SRB_FUNCTION_UNLOCK_QUEUE");
        break;
    case SRB_FUNCTION_QUIESCE_DEVICE:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SRB_FUNCTION_QUIESCE_DEVICE");
        break;
    case SRB_FUNCTION_RESET_LOGICAL_UNIT:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SRB_FUNCTION_RESET_LOGICAL_UNIT");
        break;
    case SRB_FUNCTION_SET_LINK_TIMEOUT:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SRB_FUNCTION_SET_LINK_TIMEOUT");
        break;
    case SRB_FUNCTION_LINK_TIMEOUT_OCCURRED:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SRB_FUNCTION_LINK_TIMEOUT_OCCURRED");
        break;
    case SRB_FUNCTION_LINK_TIMEOUT_COMPLETE:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SRB_FUNCTION_LINK_TIMEOUT_COMPLETE");
        break;
    case SRB_FUNCTION_POWER:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SRB_FUNCTION_POWER");
        break;
    case SRB_FUNCTION_PNP:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SRB_FUNCTION_PNP");
        break;
    case SRB_FUNCTION_DUMP_POINTERS:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SRB_FUNCTION_DUMP_POINTERS");
        break;
    case SRB_FUNCTION_FREE_DUMP_POINTERS:
        RtlStringCbCatA(msg, MSG_BUFFER_SIZE, "SRB_FUNCTION_FREE_DUMP_POINTERS");
        break;
    }

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DBG_FILTER, "%s Got SRB cmd, code (%s)\n", DEBUG_PREFIX, msg);
}
