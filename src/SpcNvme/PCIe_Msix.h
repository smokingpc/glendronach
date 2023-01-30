#pragma once

#pragma push(1)
#pragma warning(disable:4201)   // nameless struct/union
typedef struct
{
    PHYSICAL_ADDRESS MsgAddress;
    ULONG MsgData;
    struct {
        ULONG Mask : 1;
        ULONG Reserved : 31;
    }DUMMYSTRUCTNAME;
}MsixVector, * PMsixVector;
#pragma pop()
