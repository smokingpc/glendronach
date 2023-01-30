#include "pch.h"

bool BuildPrp(PNVME_COMMAND cmd, PVOID buffer, size_t buf_size)
{
    //refer to NVMe 1.3 chapter 4.3
    //Physical Region Page Entry and List
    //The PBAO of PRP entry should align to DWORD.
    
    //DWORD_
    //PAGE_ALIGN

    //1.CHECK total size
    //  if buffer is small and not cross PAGE boundary, then
    //      get physical addr and fill PRP1 directly.
    //  else if buffer cross 1 PAGE boundary, then
    //      fill PRP1 with 1st part, and fill PRP2 with 2nd part which align to next page.
    //  else if buffer cross 2 PAGE boundary and above, then
    //      fill PRP1, fill PRP2 as PRPList(2 addr)

}