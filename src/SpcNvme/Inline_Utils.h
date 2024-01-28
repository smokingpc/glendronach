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


FORCEINLINE size_t DivRoundUp(size_t value, size_t align_size)
{
    return (size_t)((value + (align_size-1))/align_size);
}

FORCEINLINE size_t RoundUp(size_t value, size_t align_size)
{
    return (DivRoundUp(value, align_size) * align_size);
}

FORCEINLINE bool IsAddrEqual(PPHYSICAL_ADDRESS a, PPHYSICAL_ADDRESS b)
{
    if (a->QuadPart == b->QuadPart)
        return true;
    return false;
}
FORCEINLINE bool IsAddrEqual(PHYSICAL_ADDRESS& a, PHYSICAL_ADDRESS& b)
{
    return IsAddrEqual(&a, &b);
}

FORCEINLINE ULONG LunToNsId(UCHAR lun)
{
//LUN is zero-based, Namespace ID is 1-based.
    return (ULONG)(lun+1);
}
FORCEINLINE UCHAR NsIdToLun(ULONG nsid)
{
    //LUN is zero-based, Namespace ID is 1-based.
    return (UCHAR)(nsid-1);
}
FORCEINLINE bool IsPciMultiFuncDevice(UCHAR type)
{
    return (PCI_MULTIFUNCTION == type);
}
FORCEINLINE bool IsPciDevice(UCHAR type)
{
    return (PCI_DEVICE_TYPE == (type & (~PCI_MULTIFUNCTION)));
}
FORCEINLINE bool IsPciBridge(UCHAR type)
{
    return (PCI_BRIDGE_TYPE == (type & (~PCI_MULTIFUNCTION)));
}
FORCEINLINE bool IsValidVendorID(PPCI_COMMON_HEADER header)
{
    //if not mapped PCI HEADER, reading it always return 0xFFFF or 0(invalid value)
    return (PCI_INVALID_VENDORID != header->VendorID || 0 != header->VendorID);
}
FORCEINLINE bool IsValidVendorID(PPCI_COMMON_CONFIG cfg)
{
    return IsValidVendorID((PPCI_COMMON_HEADER)cfg);
}
FORCEINLINE bool IsValidPciCap(PPCI_CAPABILITIES_HEADER cap)
{
    if (NULL == cap || 0 == cap->CapabilityID || 0xFF == cap->CapabilityID)
        return false;
    return true;
}
//Calculate the size of PCI Device(with its child functions) config space block.
FORCEINLINE ULONG GetPciDevCfgSpaceSize(ULONG dev_count)
{
//if dev_count == 3, return "total pci space length of 3 devices".
    ECAM_OFFSET ecam = { 0 };
    ecam.u.Dev = dev_count;
    return ecam.AsAddr;
}
//Calculate the size of PCI Bus(with its child devices and functions) config space block.
FORCEINLINE ULONG GetPciBusCfgSpaceSize(ULONG bus_count)
{
    //if dev_count == 3, return "total pci config space length of 3 buses".
    ECAM_OFFSET ecam = { 0 };
    ecam.u.Bus = bus_count;
    return ecam.AsAddr;
}
FORCEINLINE ULONG GetDeviceBarLength(ULONGLONG* bar_addr)
{
    //https://stackoverflow.com/questions/19006632/how-is-a-pci-pcie-bar-size-determined
        //step1: backup old BAR value.
        //step2: fill 0xFFFFFFFFFFFFFFFF to BAR
        //step3: read new value and clear low 4 bits (they are readonly, refer to PCI spec 3.0 6.2.5.1.)
        //step4: invert all bits
        //step5: add 1 to this value, you now get the length of BAR.
        //step6: restore the backuped old value back to BAR
    ULONGLONG old_value = *bar_addr;
    ULONGLONG new_value = 0;
    ULONG length = 0;
    *bar_addr = MAXULONGLONG;
    new_value = (*bar_addr & PCI_BAR_ADDR_MASK);
    new_value = ~new_value;
    ASSERT((new_value + 1) < 0x100000000ULL);
    length = (ULONG)(new_value + 1);
    *bar_addr = old_value;
    return length;
}
FORCEINLINE bool IsBarAddrPrefetchable(ULONG value)
{
    return ((value & 0x00000008) > 0);
}
//PLease refer to NVMe Spec IdentifyController data for 
//meaning of mpsmin / mpsmax / mdts. 
FORCEINLINE ULONG CalcMinPageSize(ULONGLONG mpsmin)
{
    return (ULONG)(1 << (12 + mpsmin));
}
FORCEINLINE ULONG CalcMaxPageSize(ULONGLONG mpsmax)
{
    return (ULONG)(1 << (12 + mpsmax));
}
FORCEINLINE ULONG CalcMaxTxSize(UCHAR mdts, ULONGLONG mpsmin)
{
    ULONG pagesize = CalcMinPageSize(mpsmin);
    return (ULONG)((1 << mdts) * pagesize);
}
