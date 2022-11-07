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
    SELF_ISSUED = 0x80000000,           //this command issued by SpcNvme.sys myself.
}NVME_CMD_TYPE;

typedef enum _USE_STATE
{
    FREE = 0,
    USED = 1,
}USE_STATE;
