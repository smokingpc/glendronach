#include "pch.h"

static inline size_t GetDistanceToNextPage(PUCHAR ptr)
{
    return (((PUCHAR)PAGE_ALIGN(ptr) + PAGE_SIZE) - ptr);
}
//static inline void BuildPrp1PhyAddr(PHYSICAL_ADDRESS& prp1, PVOID ptr)
//{
//    PHYSICAL_ADDRESS pa = MmGetPhysicalAddress(ptr);
//    prp1.QuadPart = pa.QuadPart;
//}
//static inline void BuildPrp2PhyAddr(PHYSICAL_ADDRESS& prp2, PVOID ptr)
//{
//    PHYSICAL_ADDRESS pa = MmGetPhysicalAddress(ptr);
//    prp2.QuadPart = pa.QuadPart;
//}
static UINT32 BuildPrp2List(PVOID prp2, PVOID ptr, size_t size)
{
    PHYSICAL_ADDRESS pa = {0};
    PUINT64 prp2list = (PUINT64)prp2;
    PUCHAR cursor = (PUCHAR)ptr;
    size_t size_left = size;
    ULONG prp2_index = 0;
    UINT32 entries = 0;

    while(size_left > 0)
    {
        pa = MmGetPhysicalAddress(cursor);
        prp2list[prp2_index] = pa.QuadPart;
        if(size_left <= PAGE_SIZE)
        {
            size_left = 0;
        }
        else
        {
            size_left -= PAGE_SIZE;
            cursor += PAGE_SIZE;
        }
        entries++;
        prp2_index++;
    }

    return entries;
}

bool BuildPrp(
    _Out_ PVOID& prp1va,
    _Out_ PVOID& prp2va,
    _Out_ ULONG &prp_count,
    _Out_opt_ PVOID& prp2list,
    _In_ PVOID buffer,
    _In_ size_t buf_size)
{
    //refer to NVMe 1.3 chapter 4.3
    //Physical Region Page Entry and List
    //The PBAO of PRP entry should align to DWORD.
    PUCHAR cursor = (PUCHAR) buffer;
    size_t size_left = buf_size;
    size_t distance = 0;
    prp_count = 0;

    prp1va = cursor;
    prp_count++;
    distance = GetDistanceToNextPage(cursor);

    //this buffer is smaller than PAGE_SIZE and not cross page boundary. 
    //Using PRP1 is enough...
    if(distance > size_left)
        return true;

    size_left -= distance;
    cursor += distance;
    if(size_left <= PAGE_SIZE)
    {
        prp2va = cursor;
        prp_count++;
    }
    else
    {
        //PRP2 need list
        prp2list = ExAllocatePoolWithTag(NonPagedPool, PAGE_SIZE, TAG_PRP2);
        if(nullptr == prp2list)
            return false;
        RtlZeroMemory(prp2list, PAGE_SIZE);
        prp2va = prp2list;
        prp_count += BuildPrp2List(prp2list, cursor, size_left);
    }
    return true;
}

bool BuildPrp(
    _Inout_ PSPC_SRBEXT srbext,
    _Inout_ PNVME_COMMAND cmd,
    _In_ PVOID buffer,
    _In_ size_t buf_size)
{
    bool ok = BuildPrp(srbext->Prp1VA, 
                        srbext->Prp2VA, 
                        srbext->PrpCount, 
                        srbext->Prp2List, 
                        buffer, 
                        buf_size);
    if(ok)
    {
        srbext->Prp1PA.QuadPart = MmGetPhysicalAddress(srbext->Prp1VA).QuadPart;
        srbext->Prp2PA.QuadPart = MmGetPhysicalAddress(srbext->Prp2VA).QuadPart;
        cmd->PRP1 = srbext->Prp1PA.QuadPart;
        cmd->PRP2 = srbext->Prp2PA.QuadPart;
    }
    return ok;
}

