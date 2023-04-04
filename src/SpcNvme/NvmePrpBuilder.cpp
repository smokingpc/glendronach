#include "pch.h"

static inline size_t GetDistanceToNextPage(PUCHAR ptr)
{
    return (((PUCHAR)PAGE_ALIGN(ptr) + PAGE_SIZE) - ptr);
}

static inline void BuildPrp1(ULONG64 &prp1, PVOID ptr)
{
    PHYSICAL_ADDRESS pa = MmGetPhysicalAddress(ptr);
    prp1 = pa.QuadPart;
}
static inline void BuildPrp2(ULONG64& prp2, PVOID ptr)
{
    PHYSICAL_ADDRESS pa = MmGetPhysicalAddress(ptr);
    prp2 = pa.QuadPart;
}

static void BuildPrp2List(PVOID prp2, PVOID ptr, size_t size)
{
    PHYSICAL_ADDRESS pa = {0};
    PULONG64 prp2list = (PULONG64)prp2;
    PUCHAR cursor = (PUCHAR)ptr;
    size_t size_left = size;
    ULONG prp2_index = 0;

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
        prp2_index++;
    }
}

bool BuildPrp(PSPCNVME_SRBEXT srbext, PNVME_COMMAND cmd, PVOID buffer, size_t buf_size)
{
    //refer to NVMe 1.3 chapter 4.3
    //Physical Region Page Entry and List
    //The PBAO of PRP entry should align to DWORD.
    PUCHAR cursor = (PUCHAR) buffer;
    size_t size_left = buf_size;
    size_t distance = 0;

    BuildPrp1(cmd->PRP1, cursor);
    distance = GetDistanceToNextPage(cursor);

    //this buffer is smaller than PAGE_SIZE and not cross page boundary. 
    //Using PRP1 is enough...
    if(distance > size_left)
        return true;

    size_left -= distance;
    cursor += distance;
    if(size_left <= PAGE_SIZE)
    {
        BuildPrp2(cmd->PRP2, cursor);
        return true;
    }
    else
    {
        //PRP2 need list
        srbext->Prp2VA = ExAllocatePoolWithTag(NonPagedPool, PAGE_SIZE, TAG_PRP2);
        if(NULL == srbext->Prp2VA)
            return false;

        RtlZeroMemory(srbext->Prp2VA, PAGE_SIZE);
        srbext->FreePrp2List = TRUE;
        srbext->Prp2PA = MmGetPhysicalAddress(srbext->Prp2VA);
        BuildPrp2List(srbext->Prp2VA, cursor, size_left);
        cmd->PRP2 = srbext->Prp2PA.QuadPart;
    }
    return true;
}