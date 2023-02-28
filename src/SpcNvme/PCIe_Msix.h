#pragma once

#pragma push(1)
#pragma warning(disable:4201)   // nameless struct/union

//MSIX Table data example: (only allocated 3 MSIX interrupt)
//0: kd > dd 0xffffe501eaaea000
//ffffe501`eaaea000  fee0400c 00000000 000049b3 00000000
//ffffe501`eaaea010  fee0800c 00000000 000049b3 00000000
//ffffe501`eaaea020  fee0100c 00000000 00004993 00000000
//ffffe501`eaaea030  fee0400c 00000000 000049b3 00000000
typedef union _MSIX_MSG_ADDR
{
    struct {
        ULONGLONG Aligment : 2;
        ULONGLONG DestinationMode : 1;
        ULONGLONG RedirHint : 1;
        ULONGLONG Reserved : 8;
        ULONGLONG DestinationID : 8;
        ULONGLONG BaseAddr : 12;        //LocalAPIC register phypage, which set in CPU MSR(0x1B)
        ULONGLONG Reserved2 : 32;
    };
    inline ULONGLONG GetApicBaseAddr() { return (BaseAddr << 20); }
    ULONGLONG AsUlonglong;
}MSIX_MSG_ADDR, * PMSIX_MSG_ADDR;

typedef union _MSIX_MSG_DATA{
    struct {
        ULONG Vector : 8;
        ULONG DeliveryMode : 3;
        ULONG Reserved : 3;
        ULONG Level : 1;
        ULONG TriggerMode : 1;
        ULONG Reserved2 : 16;
    };
    ULONG AsUlong;
}MSIX_MSG_DATA, *PMSIX_MSG_DATA;

typedef union _MSIX_VECTOR_CTRL
{
    struct {
        ULONG Mask : 1;
        ULONG Reserved : 31;
    };
    ULONG AsUlong;
}MSIX_VECTOR_CTRL, *PMSIX_VECTOR_CTRL;

typedef struct _MSIX_TABLE_ENTRY
{
    MSIX_MSG_ADDR MsgAddr;
    inline ULONGLONG GetApicBaseAddr() { return MsgAddr.GetApicBaseAddr(); }
    MSIX_MSG_DATA MsgData;
    MSIX_VECTOR_CTRL VectorCtrl;
}MSIX_TABLE_ENTRY, * PMSIX_TABLE_ENTRY;

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
