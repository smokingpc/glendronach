#pragma once

typedef enum _QUEUE_TYPE
{
    IO_QUEUE = 0,
    //INTIO_QUEUE = 1,
    ADM_QUEUE = 99,
}QUEUE_TYPE;

_Enum_is_bitflag_
typedef enum _NVME_CMD_TYPE
{
    UNKNOWN_CMD = 0,
    ADM_CMD = 1,
    IO_CMD = 2,
//    SRB_CMD = 0x00001000,           //this command use SRB , no wait event. using regular SRB handling
//    WAIT_CMD = 0x00002000,          //this command is internal cmd and waiting for event signal
    SELF_ISSUED = 0x80000000,           //this command issued by SpcNvme.sys myself.
}NVME_CMD_TYPE;

typedef enum _CMD_CTX_TYPE
{
    UNKNOWN = 0,
    WAIT_EVENT = 1,
    LOCAL_ADM_CMD = 2,
    SRBEXT = 3
}CMD_CTX_TYPE;

//typedef enum _USE_STATE
//{
//    FREE = 0,
//    USED = 1,
//}USE_STATE;
