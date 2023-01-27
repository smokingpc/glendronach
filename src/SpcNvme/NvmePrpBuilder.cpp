#include "pch.h"

bool BuildPrp(PNVME_COMMAND cmd, PVOID buffer, size_t buf_size)
{
    //refer to NVMe 1.3 chapter 4.3
    //Physical Region Page Entry and List
    //The PBAO of PRP entry should align to DWORD.


}