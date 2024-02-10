#pragma once
// ================================================================
// SpcNvme : OpenSource NVMe Driver for Windows 8+
// Author : Roy Wang(SmokingPC).
// Licensed by MIT License.
// 
// Copyright (C) 2022, Roy Wang (SmokingPC)
// https://github.com/smokingpc/
// 
// NVMe Spec: https://nvmexpress.org/specifications/
// Contact Me : smokingpc@gmail.com
// ================================================================
// Permission is hereby granted, free of charge, to any person obtaining a 
// copy of this softwareand associated documentation files(the "Software"), 
// to deal in the Software without restriction, including without limitation 
// the rights to use, copy, modify, merge, publish, distribute, sublicense, 
// and /or sell copies of the Software, and to permit persons to whom the 
// Software is furnished to do so, subject to the following conditions :
//
// The above copyright noticeand this permission notice shall be included in 
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE 
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS 
// IN THE SOFTWARE.
// ================================================================
// [Additional Statement]
// This Driver is implemented by NVMe Spec 1.3 and Windows Storport Miniport Driver.
// You can copy, modify, redistribute the source code. 
// 
// There is only one requirement to use this source code:
// PLEASE DO NOT remove or modify the "original author" of this codes.
// Keep "original author" declaration unmodified.
// 
// Enjoy it.
// ================================================================


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
        ULONG64 Aligment : 2;
        ULONG64 DestinationMode : 1;
        ULONG64 RedirHint : 1;
        ULONG64 Reserved : 8;
        ULONG64 DestinationID : 8;
        ULONG64 BaseAddr : 12;        //LocalAPIC register phypage, which set in CPU MSR(0x1B)
        ULONG64 Reserved2 : 32;
    };
    inline ULONG64 GetApicBaseAddr() { return (BaseAddr << 20); }
    ULONG64 AsULONG64;
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
    inline ULONG64 GetApicBaseAddr() { return MsgAddr.GetApicBaseAddr(); }
    MSIX_MSG_DATA MsgData;
    MSIX_VECTOR_CTRL VectorCtrl;
}MSIX_TABLE_ENTRY, * PMSIX_TABLE_ENTRY;

typedef struct _PCI_MSI_CAP
{
    struct {
        UINT16 CID : 8;         //Capability ID indicates this is MSI cap. (0x05)
        UINT16 NEXT : 8;        //indicates the next item in the list.
    }MID;           //Message Signaled Interrupt Identifiers

    struct {
        UINT16 MSIE : 1;        //MSI Enable. device will use INTx pins if set 0 here.
        UINT16 MMC : 3;         //Multiple Message Capable. Indicates the number of messages the controller "wants" to assert.
        UINT16 MME : 3;         //Multiple Message Enable. How many message the soft want controller assert. vendor will imp this field as ReadOnly if doesn't support multi-msi.
        UINT16 C64 : 1;         //64 Bit Address Capable. NVMe device should set 1 here.
        UINT16 PVM : 1;         //Per-Vector Masking Capable. indicates this device support PVM or not.
        UINT16 RESERVED : 7;
    }MC;            //Message Signaled Interrupt Message Control

    union
    {
        struct
        {
            struct {
                UINT32 RESERVED : 2;
                UINT32 ADDR : 30;       //Low 30 bits of system specificed message address. always DWORD aligned.
            }MLA;            //Message Signaled Interrupt Message Address
            UINT32 MUA;      //Message Signaled Interrupt Upper Address
        };
        UINT64 MA;      //Message Signaled Interrupt Address(64 bits)
    };

    UINT32 MD;            //Message Signaled Interrupt Message Data
    UINT32 MMASK;         //Message Signaled Interrupt Mask Bits (Optional). each bit can mask associated message.(by id)
    UINT32 MPEND;         //Message Signaled Interrupt Pending Bits (Optional) each bit indicates has pending msg(by id)
}PCI_MSI_CAP, * PPCI_MSI_CAP;

typedef struct _PCI_MSIX_CAP {
    struct {
        UINT16 CID : 8;         //Capability ID indicates this is MSIX cap. (0x11)
        UINT16 NEXT : 8;        //indicates the next item in the list.
    }MXID;           //MSI-X Identifiers

    struct {
        UINT16 TS : 11;         //Table Size. how many MSI-X message this device support?
        UINT16 RESERVED : 3;
        UINT16 FM : 1;          //Function Mask. Block MSI-X function if set 1.
        UINT16 MXE : 1;         //MSI-X Enable. If MXE==1 and MSIE==0, this device use MSIX and block INTx pin
    }MXC;           //MSI-X Message Control

    struct {                    //MSIX table
        UINT32 TBIR : 3;        //Table BIR. refer to NVMe spec 1.3 , section 2.4.3
        UINT32 TO : 29;         //Table Offset since BusData begin address.  refer to NVMe spec 1.3 , section 2.4.3
    }MTAB;          //MSI-X Table Offset / Table BIR

    struct {                     //PBA is Pending Bit of interrupt array
        UINT32 PBIR : 3;        //PBA BIR.  refer to NVMe spec 1.3 , section 2.4.4
        UINT32 PBAO : 29;       //PBA Offset since BusData begin address.  refer to NVMe spec 1.3 , section 2.4.4
    }MPBA;          //MSI-X PBA Offset / PBA BIR
}PCI_MSIX_CAP, * PPCI_MSIX_CAP;

typedef struct
{
    PHYSICAL_ADDRESS MsgAddress;
    ULONG MsgData;
    struct {
        ULONG Mask : 1;
        ULONG Reserved : 31;
    }DUMMYSTRUCTNAME;
}MSIX_VECTOR, * PMSIX_VECTOR;

typedef union _ECAM_OFFSET {
    struct {
        UINT32 Reserved : 2;
        UINT32 Reg : 6;
        UINT32 ExtReg : 4;
        UINT32 Func : 3;
        UINT32 Dev : 5;
        UINT32 Bus : 8;
        UINT32 NotUsed : 4;
    } u;
    UINT32 AsAddr;
} ECAM_OFFSET, * PECAM_OFFSET;


#pragma region======== copied from coalila ========
typedef struct {
    UINT16 Version : 4;         //PCIe cap structure version (0x02)
    UINT16 DevicePortType : 4;  //Device/Port Type, 0==PCIe endpoint, 1==Legacy PCIe endpoint, ...etc.
    UINT16 SlotImplement : 1;          //Slot Implemented (not applicable for PCIe endpoint device.
    UINT16 InterruptMsgNumber : 5;         //Interrupt Message Number ??
    UINT16 Undefined : 1;
    UINT16 Reserved : 1;
}PCIE_CAPABILITY_REG;

typedef struct {
    UINT32 MaxPayloadSupport : 3;
    UINT32 PhantomFunctionSupport : 2;
    UINT32 ExtendTagFieldSupport : 1;
    UINT32 EndpointL0AcceptLatency : 3;
    UINT32 EndpointL1AcceptLatency : 3;
    UINT32 Undefined : 3;
    UINT32 RoleBasedErrorReport : 1;
    UINT32 ErrCorSubclassCap : 1;
    UINT32 Reserved : 1;
    UINT32 SlotPowerLimitValue : 8;
    UINT32 SlotPowerLimitScale : 2;
    UINT32 FuncLevelResetCap : 1;
    UINT32 Reserved2 : 3;
} PCIE_DEVICE_CAPABILITY;

typedef struct {
    USHORT CorrectableErrorReport : 1;
    USHORT NonFatalErrorReport : 1;
    USHORT FatalErrorReport : 1;
    USHORT UnsupportRequestReport : 1;
    USHORT RelaxedOrdering : 1;
    USHORT MaxPayloadSize : 3;
    USHORT ExtendTagField : 1;
    USHORT PhantomFunction : 1;
    USHORT AuxPowerPM : 1;
    USHORT NoSnoop : 1;
    USHORT MaxReadRequestSize : 3;
    USHORT BCR_IFLR : 1;        //this field controls Bridge Config Retry or Initiate Function Level Reset.
}PCIE_DEVICE_CONTROL;

typedef struct {
    USHORT CorrectableError : 1;
    USHORT NonFatalError : 1;
    USHORT FatalError : 1;
    USHORT UnsupportedRequest : 1;
    USHORT AuxPower : 1;
    USHORT TransactionPending : 1;
    USHORT EmergencyPowerReduction : 1;
    USHORT Reserved : 9;
}PCIE_DEVICE_STATUS;

typedef struct {
    UINT32 MaxLinkSpeed : 4;
    UINT32 MaxLinkWidth : 6;
    UINT32 ASPMSupport : 2;
    UINT32 L0ExitLatency : 3;
    UINT32 L1ExitLatency : 3;
    UINT32 ClockPowerManagement : 1;
    UINT32 SurpriseDownErrorReport : 1;
    UINT32 DLL_LinkActiveReport : 1;        //Data Link Layer Link Active Reporting
    UINT32 LinkBandwidthNotification : 1;
    UINT32 ASPMOptionalCompliance : 1;
    UINT32 Reserved : 1;
    UINT32 PortNumber : 8;
} PCIE_LINK_CAPABILITY;

typedef struct {
    USHORT ASPMControl : 2;
    USHORT Reserved : 1;
    USHORT ReadCompletionBoundary : 1;
    USHORT LinkDisable : 1;
    USHORT RetrainLink : 1;
    USHORT CommonClockConfig : 1;
    USHORT ExtendedSync : 1;
    USHORT ClockPowerManagement : 1;
    USHORT HwAutonomousWidthDisable : 1;
    USHORT LinkBandwidthMgmtInterrupt : 1;
    USHORT LinkAutoBandwidthInterrupt : 1;  //Link Autonomous Bandwidth Interrupt
    USHORT Reserved2 : 2;
    USHORT DRSSignalControl : 2;
} PCIE_LINK_CONTROL;

typedef struct {
    USHORT LinkSpeed : 4;       //current link speed
    USHORT LinkWidth : 6;       //negotiated link width
    USHORT Undefined : 1;
    USHORT LinkTraining : 1;
    USHORT SlotClockConfig : 1;
    USHORT DataLinkLayerLinkActive : 1;
    USHORT LinkBandwidthMgmt : 1;       //Link Bandwidth Management status
    USHORT LinkAutoBandwidth : 1;       //Link Autonomous Bandwidth status
} PCIE_LINK_STATUS;

typedef struct {
    UINT32 AttentionButtom : 1;
    UINT32 PowerConotroller : 1;
    UINT32 MRL_Sensor : 1;
    UINT32 AttentionIndicator : 1;
    UINT32 PowerIndicator : 1;
    UINT32 HotPlugSurprise : 1;
    UINT32 HotPlugCapable : 1;
    UINT32 SlotPowerLimitValue : 8;
    UINT32 SlotPowerLimitScale : 2;
    UINT32 EletromechanicalInterLock : 1;
    UINT32 NoCommandCompleted : 1;
    UINT32 PhysicalSlotNumber : 13;
}PCIE_SLOT_CAPABILITY;

typedef struct {
    UINT16 AttentionButtonPressed : 1;
    UINT16 PowerFaultDetected : 1;
    UINT16 MRL_SensorChanged : 1;
    UINT16 PresenceDetectChanged : 1;
    UINT16 CommandCompletedInterrupt : 1;
    UINT16 HotPlugInterrupt : 1;
    UINT16 AttentionIndicator : 2;
    UINT16 PowerIndicator : 2;
    UINT16 PowerControl : 1;
    UINT16 EletromechanicalInterLock : 1;
    UINT16 DataLinkLayerStateChanged : 1;
    UINT16 AutoSlotPowerLimitDisable : 1;
    UINT16 InBandPDDisable : 1;
    UINT16 Reserved : 1;
}PCIE_SLOT_CONTROL;

typedef struct {
    UINT16 AttentionButton : 1;
    UINT16 PowerFault : 1;
    UINT16 MRL_SensorChanged : 1;
    UINT16 PresenceDetectChanged : 1;
    UINT16 CommandCompleted : 1;
    UINT16 MRL_Sensor : 1;
    UINT16 PresenceDetect : 1;
    UINT16 EletromechanicalInterLock : 1;
    UINT16 DataLinkLayerStateChanged : 1;
    UINT16 Reserved : 7;
}PCIE_SLOT_STATUS;

typedef struct {
    UINT16 SystemErrorOnCorrectableError : 1;
    UINT16 SystemErrorOnNonFatalError : 1;
    UINT16 SystemErrorOnFatalError : 1;
    UINT16 PME_Interrupt : 1;
    UINT16 CRS_SoftwareVisibility : 1;
    UINT16 Reserved : 11;
}PCIE_ROOT_CONTROL;

typedef struct {
    UINT16 CRS_SoftwareVisibility : 1;
    UINT16 Reserved : 15;
}PCIE_ROOT_CAPABILITY;

typedef struct {
    UINT32 PME_RequesterId : 16;
    UINT32 PME_Status : 1;
    UINT32 PME_Pending : 1;
    UINT32 Reserved : 14;
}PCIE_ROOT_STATUS;

typedef struct {
    UINT32 CompletionTimeoutRanges : 4;
    UINT32 CompletionTimeoutDisable : 1;
    UINT32 ARI_Forward : 1;
    UINT32 AtomicOpRouting : 1;
    UINT32 AtomicOp32BitCompleter : 1;
    UINT32 AtomicOp64BitCompleter : 1;
    UINT32 CAS128BitCompleter : 1;
    UINT32 NoROEnabled_PR_PR_Parsing : 1;
    UINT32 LTR_Mechanism : 1;
    UINT32 TPH_Completer : 2;
    UINT32 LNSystemCLS : 2;
    UINT32 TagCompleter10Bit : 1;
    UINT32 TagRequester10Bit : 1;
    UINT32 OBFF : 2;
    UINT32 ExtendedFmtField : 1;
    UINT32 End2End_TLP_Prefix : 1;
    UINT32 MaxEnd2EndTLPPrefixes : 2;
    UINT32 EmergencyPowerReduction : 2;
    UINT32 EmergPowerReductInitRequired : 1;    //Emergency Power Reduction Initialization Required
    UINT32 Reserved : 4;
    UINT32 FRS : 1;
}PCIE_DEVICE_CAPABILITY2;

typedef struct _PCIE_CAP
{
    PCI_CAPABILITIES_HEADER Header;
    PCIE_CAPABILITY_REG PCIeCap;
    PCIE_DEVICE_CAPABILITY DevCap;
    PCIE_DEVICE_CONTROL DevCtrl;
    PCIE_DEVICE_STATUS DevStatus;
    PCIE_LINK_CAPABILITY LinkCap;
    PCIE_LINK_CONTROL LinkCtrl;
    PCIE_LINK_STATUS LinkStatus;
    PCIE_SLOT_CAPABILITY SlotCap;
    PCIE_SLOT_CONTROL SlotCtrl;
    PCIE_SLOT_STATUS SlotStatus;
    PCIE_ROOT_CONTROL RootCtrl;
    UINT16 RootCap;
    UINT32 RootStatus;
    UINT32 DevCap2;
    UINT16 DevCtrl2;
    UINT16 DevStatus2;
}PCIE_CAP, * PPCIE_CAP;
#pragma endregion
#pragma pop()

void ParseMsiCaps(
    PCI_COMMON_CONFIG* cfg,
    PPCI_MSI_CAP &msi,
    PPCI_MSIX_CAP &msix);

ULONG GetMsixTableSize(PCI_COMMON_CONFIG* cfg);
