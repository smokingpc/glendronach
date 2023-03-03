#pragma once

typedef enum class _QUEUE_TYPE
{
    UNKNOWN = 0,
    ADM_QUEUE = 1,
    IO_QUEUE = 2,
}QUEUE_TYPE;

_Enum_is_bitflag_
typedef enum class _NVME_CMD_TYPE : UINT32
{
    UNKNOWN = 0,
    ADM_CMD = 1,
    IO_CMD = 2,
//    SRB_CMD = 0x00001000,           //this command use SRB , no wait event. using regular SRB handling
//    WAIT_CMD = 0x00002000,          //this command is internal cmd and waiting for event signal
    SELF_ISSUED = 0x80000000,           //this command issued by SpcNvme.sys myself.
}NVME_CMD_TYPE;

typedef enum class _CMD_CTX_TYPE
{
    UNKNOWN = 0,
    WAIT_EVENT = 1,
    LOCAL_ADM_CMD = 2,
    SRBEXT = 3
}CMD_CTX_TYPE;

typedef enum class _IDENTIFY_CNS : UCHAR
{
    IDENT_NAMESPACE = 0,
    IDENT_CONTROLLER = 1,

}IDENTIFY_CNS;

//typedef enum _USE_STATE
//{
//    FREE = 0,
//    USED = 1,
//}USE_STATE;

typedef enum _NVME_STATE {
    STOP = 0,
    SETUP = 1,
    RUNNING = 2,
    TEARDOWN = 3,
    RESET = 4,
    SHUTDOWN = 5,   //CC.SHN==1
}NVME_STATE;

